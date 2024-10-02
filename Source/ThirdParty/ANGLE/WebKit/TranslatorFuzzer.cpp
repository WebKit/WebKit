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

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <random>
#include <string.h>

#include "TranslatorFuzzerSupport.h"
#include "angle_gl.h"
#include "common/string_utils.h"
#include "common/system_utils.h"
#include "compiler/translator/Compiler.h"
#include "compiler/translator/util.h"

extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize);

using namespace sh;

namespace
{

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

static uint32_t allTypes[] = {
    GL_FRAGMENT_SHADER,
    GL_VERTEX_SHADER
};
static constexpr int allTypesCount = ARRAY_COUNT(allTypes);

static const struct {
    ShShaderSpec spec;
    const char* flag;
} allSpecs[] = {
    { SH_GLES2_SPEC, "i2"},
    { SH_GLES3_SPEC, "i3"},
    { SH_WEBGL_SPEC, "w" },
    { SH_WEBGL2_SPEC, "w2" },
};
static constexpr int allSpecsCount = ARRAY_COUNT(allSpecs);
static ShShaderSpec validSpecs[allSpecsCount] = {
    SH_WEBGL_SPEC,
    SH_WEBGL2_SPEC,
};
static int validSpecsCount = 2;

static const struct {
    ShShaderOutput output;
    const char* flag;
} allOutputs[] = {
    { SH_MSL_METAL_OUTPUT, "m"},
    { SH_ESSL_OUTPUT, "e" },
    { SH_GLSL_COMPATIBILITY_OUTPUT, "g" },
    { SH_GLSL_130_OUTPUT, "g130" },
    { SH_GLSL_140_OUTPUT, "g140" },
    { SH_GLSL_150_CORE_OUTPUT, "g150" },
    { SH_GLSL_330_CORE_OUTPUT, "g330" },
    { SH_GLSL_400_CORE_OUTPUT, "g400" },
    { SH_GLSL_410_CORE_OUTPUT, "g410" },
    { SH_GLSL_420_CORE_OUTPUT, "g420" },
    { SH_GLSL_430_CORE_OUTPUT, "g430" },
    { SH_GLSL_440_CORE_OUTPUT, "g440" },
    { SH_GLSL_450_CORE_OUTPUT, "g450" },
};
static constexpr int allOutputsCount = ARRAY_COUNT(allOutputs);
static ShShaderOutput validOutputs[allOutputsCount] = {
    SH_MSL_METAL_OUTPUT
};
static int validOutputsCount = 1;

static struct {
    TCompiler* translator;
    uint32_t type;
    ShShaderSpec spec;
    ShShaderOutput output;
} translators[allTypesCount * allSpecsCount * allOutputsCount];
static int translatorsCount;

void mutateOptions(ShShaderOutput output, ShCompileOptions& options)
{
    size_t mutateLength = offsetof(ShCompileOptions, metal);
    LLVMFuzzerMutate(reinterpret_cast<uint8_t*>(&options), mutateLength, mutateLength);
}

static bool initializeValidFuzzerOptions()
{
    const std::string optionsSpec = angle::GetEnvironmentVar("ANGLE_TRANSLATOR_FUZZER_OPTIONS");
    const std::vector<std::string> optionsSpecParts = angle::SplitString(optionsSpec, ";,: ", angle::KEEP_WHITESPACE, angle::SPLIT_WANT_NONEMPTY);
    const std::set<std::string> options { optionsSpecParts.begin(), optionsSpecParts.end() };
    if (options.empty())
        return true;
    int specs = 0;
    int outputs = 0;
    for (const std::string& option : options) {
        for (auto& spec : allSpecs) {
            if (option == spec.flag) {
                validSpecs[specs++] = spec.spec;
                break;
            }
        }
        for (auto& output : allOutputs) {
            if (option == output.flag) {
                validOutputs[specs++] = output.output;
                break;
            }
        }
    }
    if (specs)
        validSpecsCount = specs;
    if (outputs)
        validOutputsCount = outputs;
    return true;
}

void mutate(GLSLDumpHeader& header, unsigned seed)
{
    std::minstd_rand rnd { seed };
    switch (rnd() % 4) {
        case 0:
            header.type = allTypes[(header.type + rnd()) % allTypesCount];
            break;
        case 1:
            header.spec = validSpecs[(header.spec + rnd()) % validSpecsCount];
            break;
        case 2:
            header.output = validOutputs[(header.output + rnd()) % validOutputsCount];
            break;
        case 3:
            mutateOptions(header.output, header.options);
            break;
    }
}

bool initializeTranslators()
{
    if (!sh::Initialize())
        return false;
    for (int typeIndex = 0; typeIndex < allTypesCount; ++typeIndex) {
        auto type = allTypes[typeIndex];
        for (int specIndex = 0; specIndex < validSpecsCount; ++specIndex) {
            auto spec = validSpecs[specIndex];
            for (int outputIndex = 0; outputIndex < validOutputsCount; ++outputIndex) {
                auto output = validOutputs[outputIndex];
                TCompiler* translator = ConstructCompiler(type, spec, output);
                ShBuiltInResources resources;
                sh::InitBuiltInResources(&resources);
                // Enable all the extensions to have more coverage
                resources.OES_standard_derivatives        = 1;
                resources.OES_EGL_image_external          = 1;
                resources.OES_EGL_image_external_essl3    = 1;
                resources.NV_EGL_stream_consumer_external = 1;
                resources.ARB_texture_rectangle           = 1;
                resources.EXT_blend_func_extended         = 1;
                resources.EXT_conservative_depth          = 1;
                resources.EXT_draw_buffers                = 1;
                resources.EXT_frag_depth                  = 1;
                resources.EXT_shader_texture_lod          = 1;
                resources.EXT_shader_framebuffer_fetch    = 1;
                resources.NV_shader_framebuffer_fetch     = 1;
                resources.ARM_shader_framebuffer_fetch    = 1;
                resources.EXT_YUV_target                  = 1;
                resources.APPLE_clip_distance             = 1;
                resources.MaxDualSourceDrawBuffers        = 1;
                resources.EXT_gpu_shader5                 = 1;
                resources.MaxClipDistances                = 1;
                resources.EXT_shadow_samplers             = 1;
                resources.EXT_clip_cull_distance          = 1;
                resources.ANGLE_clip_cull_distance        = 1;
                resources.EXT_primitive_bounding_box      = 1;
                resources.OES_primitive_bounding_box      = 1;
                if (!translator->Init(resources))
                    return false;
                translators[translatorsCount++] = { translator, type, spec, output };
            }
        }
    }
    return true;
}

TCompiler* getTranslator(uint32_t type, ShShaderSpec spec, ShShaderOutput output)
{
    for (int i = 0; i < translatorsCount; ++i) {
        if (translators[i].type == type && translators[i].spec == spec && translators[i].output == output)
            return translators[i].translator;
    }
    return nullptr;
}

void initializeFuzzer()
{
    static int i = [] {
        if (!initializeValidFuzzerOptions() || !initializeTranslators())
            exit(1);
        return 0;
    }();
    ANGLE_UNUSED_VARIABLE(i);
}

}

void filterOptions(ShShaderOutput output, ShCompileOptions& options)
{
    const bool any = true;
    const bool none = false;
    const bool msl = output == SH_MSL_METAL_OUTPUT;
    const bool glsl = IsOutputGLSL(output) || IsOutputESSL(output);
#if defined(ANGLE_PLATFORM_APPLE)
    const bool appleGLSL = glsl;
#else
    const bool appleGLSL = false;
#endif
    const bool hlsl = IsOutputHLSL(output);
    const bool spirvVk = output == SH_SPIRV_VULKAN_OUTPUT;
#define CHECK_VALID_OPTION(name, i, allowed, forced) options.name = (allowed) ? options.name : (forced);
    FOR_EACH_SH_COMPILE_OPTIONS_BOOL_OPTION(CHECK_VALID_OPTION)
#undef CHECK_VALID_OPTION
}

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size, size_t maxSize, unsigned int seed)
{
    initializeFuzzer();
    if (maxSize < GLSLDumpHeader::kHeaderSize + 2)
        return size;
    if (size < GLSLDumpHeader::kHeaderSize + 2)
        size = GLSLDumpHeader::kHeaderSize + 2;
    GLSLDumpHeader header { data };
    if (std::minstd_rand { seed }() % 100 == 0) {
        mutate(header, seed);
        header.write(data);
    }
    data += GLSLDumpHeader::kHeaderSize;
    size -= GLSLDumpHeader::kHeaderSize;
    maxSize -= GLSLDumpHeader::kHeaderSize;
    size_t newSize = LLVMFuzzerMutate(data, size - 1, maxSize - 1);
    if (data[newSize] != '\0')
        data[newSize++] = '\0';
    return GLSLDumpHeader::kHeaderSize + newSize;
}

extern "C" int LLVMFuzzerTestOneInput (const uint8_t* data, size_t size)
{
    initializeFuzzer();
    if (size < GLSLDumpHeader::kHeaderSize + 2)
        return 0;
    // Make sure the rest of data will be a valid C string so that we don't have to copy it.
    if (data[size - 1] != 0)
        return 0;

    GLSLDumpHeader header { data };
    filterOptions(header.output, header.options);
    auto* translator = getTranslator(header.type, header.spec, header.output);
    if (!translator)
        return 0;
    size -= GLSLDumpHeader::kHeaderSize;
    data += GLSLDumpHeader::kHeaderSize;
    const char *shaderStrings[] = { reinterpret_cast<const char *>(data) };
    translator->compile(shaderStrings, 1, header.options);
    return 0;
}
