/**
 * @file     gltf_model.h
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



#pragma once

#include <string_view>
#include <print>
#include "tiny_gltf.h"


struct gltf_model
{
public:
	gltf_model(std::string_view filename);

	auto save(const std::string& path) -> void
	{
		tinygltf::TinyGLTF writer;
		bool ret = writer.WriteGltfSceneToFile(&m_model, path, true, true, true, true);
		if (!ret) {
			std::println("Failed to save model: {}", path);
		}
	}

public:
	tinygltf::Model m_model;
};


