/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Dumps the fuzzer sample into an ANGLE test case.
#include <iostream>
#include <fstream>
#include <vector>

#include "TranslatorFuzzerSupport.h"

// In order to link with the fuzzer that uses LLVMFuzzerMutate, provide a dummy symbol for the function.
extern "C" size_t LLVMFuzzerMutate(uint8_t*, size_t, size_t)
{
    exit(1);
    return 0;
}

static const char testHeader[] = R"cpp(
//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CompilerWorks_test.cpp:
//   Some tests for shader compilation.
//

#include "GLSLANG/ShaderLang.h"
#include "angle_gl.h"
#include "compiler/translator/Compiler.h"
#include "gtest/gtest.h"

namespace sh
{
)cpp";

static const char testContent1[] = R"cpp(
{
    ShBuiltInResources resources;
    sh::InitBuiltInResources(&resources);
    ShCompileOptions options = { };
)cpp";

static const char testContent2[] = R"cpp(
    ShHandle compiler = sh::ConstructCompiler(type, spec, output, &resources);
    EXPECT_NE(static_cast<ShHandle>(0), compiler);

    const char program[] = R"TEST()cpp";

static const char testContent3[] = R"cpp()TEST";
    sh::Compile(compiler, program, 1, options);
    sh::Destruct(compiler);
}
)cpp";

static const char testFooter[] = R"cpp(
}
)cpp";

#define RETURN_STRING_IF_EQUAL(var, name) if (var == name) return #name

static std::string typeToString(uint32_t type)
{
    RETURN_STRING_IF_EQUAL(type, GL_FRAGMENT_SHADER);
    RETURN_STRING_IF_EQUAL(type, GL_VERTEX_SHADER);
    return std::string{ "static_cast<uint32_t>(" } + std::to_string(type) + ")";
}

static std::string shaderSpecToString(ShShaderSpec spec)
{
    RETURN_STRING_IF_EQUAL(spec, SH_GLES2_SPEC);
    RETURN_STRING_IF_EQUAL(spec, SH_WEBGL_SPEC);
    RETURN_STRING_IF_EQUAL(spec, SH_WEBGL2_SPEC);
    RETURN_STRING_IF_EQUAL(spec, SH_GLES3_SPEC);
    return std::string{"static_cast<ShShaderSpec>("} + std::to_string(spec) + ")";
}

static std::string shaderOutputToString(ShShaderOutput output)
{
 
    RETURN_STRING_IF_EQUAL(output, SH_MSL_METAL_OUTPUT);
    RETURN_STRING_IF_EQUAL(output, SH_ESSL_OUTPUT);
    RETURN_STRING_IF_EQUAL(output, SH_GLSL_COMPATIBILITY_OUTPUT);
    RETURN_STRING_IF_EQUAL(output, SH_GLSL_130_OUTPUT);
    RETURN_STRING_IF_EQUAL(output, SH_GLSL_140_OUTPUT);
    RETURN_STRING_IF_EQUAL(output, SH_GLSL_150_CORE_OUTPUT);
    RETURN_STRING_IF_EQUAL(output, SH_GLSL_330_CORE_OUTPUT);
    RETURN_STRING_IF_EQUAL(output, SH_GLSL_400_CORE_OUTPUT);
    RETURN_STRING_IF_EQUAL(output, SH_GLSL_410_CORE_OUTPUT);
    RETURN_STRING_IF_EQUAL(output, SH_GLSL_420_CORE_OUTPUT);
    RETURN_STRING_IF_EQUAL(output, SH_GLSL_430_CORE_OUTPUT);
    RETURN_STRING_IF_EQUAL(output, SH_GLSL_440_CORE_OUTPUT);
    RETURN_STRING_IF_EQUAL(output, SH_GLSL_450_CORE_OUTPUT);
    return std::string{"static_cast<ShShaderOutput>("} + std::to_string(output) + ")";
}

int main(int argc, const char * argv[])
{
    if (argc < 2) {
        std::cout << "usage: " << argv[0] << "testcase [testcase...]" << std::endl;
        exit(1);
    }

    std::cout << testHeader;
    for (int i = 1; i < argc; ++i) {
        std::vector<uint8_t> fileData;
        {
            std::streampos fileSize;
            {
                std::ifstream file { argv[i], std::ios::binary };
                file.seekg(0, std::ios::end);
                fileSize = file.tellg();
                file.seekg(0, std::ios::beg);
                if (fileData.size() < static_cast<size_t>(fileSize))
                    fileData.resize(static_cast<size_t>(fileSize));
                file.read(reinterpret_cast<char*>(&fileData[0]), fileSize);
            }
        }
        GLSLDumpHeader header { &fileData[0] };
        header.output = resolveShaderOutput(header.output);
        filterOptions(header.output, header.options);
        std::cout << "TEST(CompilerWorksTest, Test" << i << ")";
        std::cout << testContent1;
#define COUT_OPTION(NAME, I, ALLOW, FORCE) if (header.options.NAME) std::cout << "    options." #NAME " = true;" << std::endl;
        FOR_EACH_SH_COMPILE_OPTIONS_BOOL_OPTION(COUT_OPTION);
#undef COUT_OPTION
        std::cout << "    uint32_t type = " << typeToString(header.type) << ";" << std::endl;
        std::cout << "    ShShaderSpec spec = " << shaderSpecToString(header.spec) << ";" << std::endl;
        std::cout << "    ShShaderOutput output = " << shaderOutputToString(header.output) << ";" << std::endl;
        std::cout << testContent2;
        std::cout << &fileData[GLSLDumpHeader::kHeaderSize];
        std::cout << testContent3;
    }
    std::cout << testFooter;
    return 0;
}
