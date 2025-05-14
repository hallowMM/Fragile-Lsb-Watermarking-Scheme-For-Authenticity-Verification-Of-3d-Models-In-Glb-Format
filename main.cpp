/**
 * @file     main.cpp
 * @brief    Fragile LSB watermarking implementation for GLB format 3D models
 *
 * @author   Marcin Matczuk
 * @date     2025-05-14
 * @version  1.0
 *
 * @copyright Copyright (c) 2025 Martin Marcin Matczuk,
 *            Department of Computer Science,
 *            Lublin University of Technology
 *            Nadbystrzycka 38D, 20-618 Lublin, Poland
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * This software was developed as part of research described in:
 * "[Full Article Title]", Applied Sciences, MDPI, [Year].
 * DOI: [If available]
 */



#include <iostream>
#include <numeric>
#include <string>
#include <fstream>
#include <chrono>
#include <random>
#include <ranges>
#include <string_view>
#include <filesystem>

#include <openssl/hmac.h>
#include <openssl/sha.h>

#include "gltf_model.h"
using namespace tinygltf;
using namespace std;

constexpr uint32_t HMAC_BYTE_SIZE = SHA512_DIGEST_LENGTH;
constexpr uint32_t MAX_VERTICES_FOR_HMAC = 512;

using BYTE = unsigned char;
using HMAC_CODE = array<BYTE, HMAC_BYTE_SIZE>;

struct SteganographyKey
{
	int numBits;
	int seed;
	string attr;
	string keyHMAC;
	
	SteganographyKey(string_view key)
	{
		numBits = stoi(string{ key.substr(0, 2) });
		key.remove_prefix(2);

		size_t pos = key.find('*');
		if (pos == string_view::npos) {
			println("Invalid key format");
			return;
		}
		seed = stoi(string{ key.substr(0, pos) });
		key.remove_prefix(pos + 1);

		pos = key.find('*');
		if (pos == string_view::npos) {
			println("Invalid key format");
			return;
		}
		attr = key.substr(0, pos);
		key.remove_prefix(pos + 1);

		if (pos == string_view::npos) {
			println("Invalid key format");
			return;
		}
		keyHMAC = key;
	}

};

auto GetMarkedFilename(const string& filename) -> string;
auto LoadFileData(const string& path) -> vector<BYTE>;
auto ZeroingBits(gltf_model& gltfModel, const SteganographyKey& key) -> void;
auto CalculateHMAC(const vector<BYTE>& data, string_view key) -> HMAC_CODE;
auto EmbedHMAC(gltf_model& gltfModel, const SteganographyKey& key, const HMAC_CODE& hmac) -> void;
auto ExtractHMAC(gltf_model& gltfModel, const SteganographyKey& key, HMAC_CODE& hmac) -> void;

inline
auto GenerateVertexIndices(int seed, int maxIndex) -> array<size_t, 512>
{
	vector<size_t> allIndices( maxIndex, 0);
	ranges::iota(allIndices, 0);

	mt19937 rng(seed);
	ranges::shuffle(allIndices, rng); 

	array<size_t, 512> indices{};
	copy_n(allIndices.begin(), 512, indices.begin()); 

	return indices;
}

inline
auto FindMaxVerticesMeshId(const gltf_model& gltfModel) -> pair<int, int>
{
	const Model& model = gltfModel.m_model;

	int maxNumElements = 0;
	int maxNumElMeshId = 0;

	for (auto [i, mesh] : views::enumerate(model.meshes))
	{
		int numElements = 0;
		for (auto& primitive : mesh.primitives)
		{
			int accessorIndex = primitive.attributes.at("POSITION");
			const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
			int numComponents = tinygltf::GetNumComponentsInType(accessor.type);
			numElements += accessor.count * numComponents;
		}
		if (numElements > maxNumElements)
		{
			maxNumElements = numElements;
			maxNumElMeshId = i;
		}
	}
	return { maxNumElMeshId, maxNumElements };
}

int main()
{
	ios::sync_with_stdio(false);
	
	string keyString;
	string modelFilename;

	bool isMarkingProcess = 0;
	char param;

	while (true)
	{
		cout << "***Watermarking 3D Models.glb***\n";
		cout << "Entry:\n	1 if you want mark model\n"
			"	0 if you want check model\n"
			"	e if you want to exit\n";

		cin >> param;
		if (tolower(param) == 'e')
			break;

		if (param == '1' || param == '0'){
			isMarkingProcess = int(param - '0');
		}
		else{
			println("Undefined input");
			continue;
		}

		cin.get();
		println("Entry path to file (with extension): ");
		getline(cin, modelFilename);

		// Remove white chars before the string.
		modelFilename.erase(
			modelFilename.begin(),
			ranges::find_if(modelFilename, [](unsigned char ch) {
				return !isspace(ch); }));


		println("Entry steganography key: ");
		getline(cin, keyString);
		const SteganographyKey keyStego(keyString);
		const string markedFilename = GetMarkedFilename(modelFilename);


		if (isMarkingProcess)
		{
			// // Marking process
			// Loading
			gltf_model model{ modelFilename };
			// Zeroing bits
			ZeroingBits(model, keyStego);
			// Saving modified model
			model.save(markedFilename);

			// HMAC
			auto fileData = LoadFileData(markedFilename);
			auto hmac = CalculateHMAC(fileData, keyStego.keyHMAC);

			// Loading modified model
			gltf_model markedModel{ markedFilename };
			// Embedding HMAC
			EmbedHMAC(markedModel, keyStego, hmac);
			// Saving marked model
			markedModel.save(markedFilename);

			println("Model marked!");
		}
		else
		{
			// // Authorization process
			// Loading marked model
			gltf_model markedModel{ modelFilename };
			// Reading HMAC
			HMAC_CODE hmacLoaded {};
			ExtractHMAC(markedModel, keyStego, hmacLoaded);
			// Zeroing bits
			ZeroingBits(markedModel, keyStego);
			string path = "xaw32.zuz";
			// Saving modified model (to new file)
			markedModel.save(path);

			// HMAC
			auto fileData = LoadFileData(path);
			auto hmacRecalculated = CalculateHMAC(fileData, keyStego.keyHMAC);
			filesystem::remove(path);

			// Comparing HMACs
			if (hmacRecalculated == hmacLoaded)
				println("Verification Confirmed");
			else
				println("Watermark violate!");
		}
		println("\n");
	}
	println("Program terminated");
	system("PAUSE");
	return 0;
}


void ZeroingBits(gltf_model& gltfModel, const SteganographyKey& key)
{
	Model& model = gltfModel.m_model;
	const auto [maxNumElMeshId, maxNumElements] = FindMaxVerticesMeshId(gltfModel);

	uint32_t numOfVerts = (MAX_VERTICES_FOR_HMAC / (key.numBits * 3)) + 1;
	if (maxNumElements < numOfVerts)
		return;

	for (auto& primitive : model.meshes[maxNumElMeshId].primitives)
	{
		if (primitive.attributes.contains(key.attr))
		{
			// Boilerplate code
			int accessor_index = primitive.attributes[key.attr];
			const Accessor& accessor = model.accessors[accessor_index];
			const BufferView& buffer_view = model.bufferViews[accessor.bufferView];
			const Buffer& buffer = model.buffers[buffer_view.buffer];

			const BYTE* data = buffer.data.data() +
				buffer_view.byteOffset + accessor.byteOffset;
			//

			int numOfComps = GetNumComponentsInType(accessor.type);
			int bitCounter = 0;
			auto indices = GenerateVertexIndices(key.seed, accessor.count - 1);

			for (int i = 0; ;i++){
				float* vertex = (float*)(data + indices[i] * accessor.ByteStride(buffer_view));

				for (int j = 0; j < numOfComps; j++){
					for (int k = 0; k < key.numBits; k++){
						*reinterpret_cast<unsigned int*>(&vertex[j]) &= ~(1u << k);

						if (bitCounter++; bitCounter == MAX_VERTICES_FOR_HMAC) 
							return;
					}
				}
			}
		}
		else
		{
			println("Invalid attribute of vertex!");
			exit(1);
		}
	}
}

HMAC_CODE CalculateHMAC(const vector<BYTE>& data, string_view key)
{
	unsigned int len = HMAC_BYTE_SIZE;
	array<BYTE, HMAC_BYTE_SIZE> result;

	HMAC(EVP_sha512(),
		key.data(),
		key.size(),
		data.data(),
		data.size(),
		result.data(),
		&len);

	return result;
}

void EmbedHMAC(gltf_model& gltfModel, const SteganographyKey& key, const HMAC_CODE& hmac)
{
	Model& model = gltfModel.m_model;
	const auto [maxNumElMeshId, maxNumElements] = FindMaxVerticesMeshId(gltfModel);

	uint32_t numOfVerts = (MAX_VERTICES_FOR_HMAC / (key.numBits * 3)) + 1;
	if (maxNumElements < numOfVerts)
		return;

	for (auto& primitive : model.meshes[maxNumElMeshId].primitives)
	{
		if (primitive.attributes.contains(key.attr))
		{
			// Boilerplate code
			int accessor_index = primitive.attributes[key.attr];
			const Accessor& accessor = model.accessors[accessor_index];
			const BufferView& buffer_view = model.bufferViews[accessor.bufferView];
			const Buffer& buffer = model.buffers[buffer_view.buffer];

			const BYTE* data = buffer.data.data() +
				buffer_view.byteOffset + accessor.byteOffset;
			//

			int numOfComps = GetNumComponentsInType(accessor.type);
			auto indices = GenerateVertexIndices(key.seed, accessor.count - 1);

			int byteCounter = 0;
			int bitCounter = 7;

			for (int i = 0; ; i++)
			{
				float* vertex = (float*)(data + indices[i] * accessor.ByteStride(buffer_view));

				for (int j = 0; j < numOfComps; j++) {
					for (int k = 0; k < key.numBits; k++) {
						BYTE byte = hmac[byteCounter];

						*reinterpret_cast<unsigned int*>(&vertex[j])
							^= (((byte >> bitCounter) & 1u) << k);


						if (bitCounter--; bitCounter == -1)
						{
							byteCounter++;
							bitCounter = 7;
						}
						if (byteCounter == HMAC_BYTE_SIZE)
							return;
					}
				}
			}
		}
	}
}

void ExtractHMAC(gltf_model& gltfModel, const SteganographyKey& key, HMAC_CODE& hmac)
{
	Model& model = gltfModel.m_model;
	const auto [maxNumElMeshId, maxNumElements] = FindMaxVerticesMeshId(gltfModel);

	uint32_t numOfVerts = (MAX_VERTICES_FOR_HMAC / (key.numBits * 3)) + 1;
	if (maxNumElements < numOfVerts)
		return;

	for (auto& primitive : model.meshes[maxNumElMeshId].primitives)
	{
		if (primitive.attributes.contains(key.attr))
		{
			// Boilerplate code
			int accessor_index = primitive.attributes[key.attr];
			const Accessor& accessor = model.accessors[accessor_index];
			const BufferView& buffer_view = model.bufferViews[accessor.bufferView];
			const Buffer& buffer = model.buffers[buffer_view.buffer];

			const BYTE* data = buffer.data.data() +
				buffer_view.byteOffset + accessor.byteOffset;
			//

			int numOfComps = GetNumComponentsInType(accessor.type);
			auto indices = GenerateVertexIndices(key.seed, accessor.count - 1);

			int byteCounter = 0;
			int bitCounter = 7;
			BYTE byte = 0;
			for (int i = 0; ; i++)
			{
				float* vertex = (float*)(data + indices[i] * accessor.ByteStride(buffer_view));
				for (int j = 0; j < numOfComps; j++) {

					for (int k = 0; k < key.numBits; k++) {

						hmac[byteCounter] |= ((*reinterpret_cast<unsigned int*>(&vertex[j]) >> k)
							& 1u) << bitCounter;

						if (bitCounter--; bitCounter == -1)
						{
							byteCounter++;
							bitCounter = 7;
						}
						if (byteCounter == HMAC_BYTE_SIZE)
							return;
					}
				}
			}
		}
		else
		{
			println("Invalid attribute of vertex!");
			exit(1);
		}
	}
}

vector<BYTE> LoadFileData(const string& path)
{
	ifstream file(path, ios::binary | ios::ate);
	if (!file) {
		cout << "File not found!\n";
		return {};
	}
	auto size = file.tellg();
	file.seekg(0);

	vector<BYTE> data(size);
	file.read(reinterpret_cast<char*>(data.data()), size);
	return data;
}

string GetMarkedFilename(const string& filename) {
	size_t dotPos = filename.find_last_of('.');
	return (dotPos == string::npos)
		? filename + "_marked.glb"
		: filename.substr(0, dotPos) + "_marked" + filename.substr(dotPos);
}
