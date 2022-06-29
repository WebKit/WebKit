//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// translator_fuzzer.cpp: A libfuzzer fuzzer for the shader translator.

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <unordered_map>

#include "angle_gl.h"
#include "anglebase/no_destructor.h"
#include "compiler/translator/Compiler.h"
#include "compiler/translator/util.h"

using namespace sh;

namespace
{

// Options supported by any output
constexpr ShCompileOptions kCommonOptions =
    SH_VALIDATE_LOOP_INDEXING | SH_INTERMEDIATE_TREE | SH_OBJECT_CODE | SH_VARIABLES |
    SH_LINE_DIRECTIVES | SH_SOURCE_PATH | SH_REMOVE_INVARIANT_AND_CENTROID_FOR_ESSL3 |
    SH_EMULATE_ABS_INT_FUNCTION | SH_ENFORCE_PACKING_RESTRICTIONS | SH_CLAMP_INDIRECT_ARRAY_BOUNDS |
    SH_LIMIT_EXPRESSION_COMPLEXITY | SH_LIMIT_CALL_STACK_DEPTH | SH_INIT_GL_POSITION |
    SH_INIT_OUTPUT_VARIABLES | SH_SCALARIZE_VEC_AND_MAT_CONSTRUCTOR_ARGS |
    SH_FLATTEN_PRAGMA_STDGL_INVARIANT_ALL | SH_HLSL_GET_DIMENSIONS_IGNORES_BASE_LEVEL |
    SH_REWRITE_TEXELFETCHOFFSET_TO_TEXELFETCH | SH_EMULATE_ISNAN_FLOAT_FUNCTION |
    SH_INITIALIZE_UNINITIALIZED_LOCALS | SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW |
    SH_CLAMP_POINT_SIZE | SH_DONT_USE_LOOPS_TO_INITIALIZE_VARIABLES |
    SH_SKIP_D3D_CONSTANT_REGISTER_ZERO | SH_EMULATE_GL_DRAW_ID | SH_INIT_SHARED_VARIABLES |
    SH_FORCE_ATOMIC_VALUE_RESOLUTION | SH_EMULATE_GL_BASE_VERTEX_BASE_INSTANCE |
    SH_TAKE_VIDEO_TEXTURE_AS_EXTERNAL_OES | SH_VALIDATE_AST | SH_ADD_BASE_VERTEX_TO_VERTEX_ID |
    SH_REMOVE_DYNAMIC_INDEXING_OF_SWIZZLED_VECTOR | SH_DISABLE_ARB_TEXTURE_RECTANGLE |
    SH_IGNORE_PRECISION_QUALIFIERS | SH_FORCE_SHADER_PRECISION_HIGHP_TO_MEDIUMP;

// Options supported by GLSL or ESSL only
constexpr ShCompileOptions kGLSLOrESSLOnlyOptions =
    SH_EMULATE_ATAN2_FLOAT_FUNCTION | SH_CLAMP_FRAG_DEPTH | SH_REGENERATE_STRUCT_NAMES |
    SH_REWRITE_REPEATED_ASSIGN_TO_SWIZZLED | SH_USE_UNUSED_STANDARD_SHARED_BLOCKS |
    SH_SELECT_VIEW_IN_NV_GLSL_VERTEX_SHADER;

#if defined(ANGLE_PLATFORM_APPLE)
// Options supported by GLSL only on mac
constexpr ShCompileOptions kGLSLMacOnlyOptions =
    SH_REWRITE_FLOAT_UNARY_MINUS_OPERATOR | SH_ADD_AND_TRUE_TO_LOOP_CONDITION |
    SH_REWRITE_DO_WHILE_LOOPS | SH_UNFOLD_SHORT_CIRCUIT | SH_REWRITE_ROW_MAJOR_MATRICES;
#endif

// Options supported by Vulkan SPIR-V output only
constexpr ShCompileOptions kVulkanOnlyOptions =
    SH_EMULATE_SEAMFUL_CUBE_MAP_SAMPLING | SH_USE_SPECIALIZATION_CONSTANT |
    SH_ADD_VULKAN_XFB_EMULATION_SUPPORT_CODE | SH_ROUND_OUTPUT_AFTER_DITHERING |
    SH_ADD_ADVANCED_BLEND_EQUATIONS_EMULATION;

// Options supported by HLSL output only
constexpr ShCompileOptions kHLSLOnlyOptions = SH_EXPAND_SELECT_HLSL_INTEGER_POW_EXPRESSIONS |
                                              SH_ALLOW_TRANSLATE_UNIFORM_BLOCK_TO_STRUCTUREDBUFFER |
                                              SH_REWRITE_INTEGER_UNARY_MINUS_OPERATOR;

struct TranslatorCacheKey
{
    bool operator==(const TranslatorCacheKey &other) const
    {
        return type == other.type && spec == other.spec && output == other.output;
    }

    uint32_t type   = 0;
    uint32_t spec   = 0;
    uint32_t output = 0;
};
}  // anonymous namespace

namespace std
{

template <>
struct hash<TranslatorCacheKey>
{
    std::size_t operator()(const TranslatorCacheKey &k) const
    {
        return (hash<uint32_t>()(k.type) << 1) ^ (hash<uint32_t>()(k.spec) >> 1) ^
               hash<uint32_t>()(k.output);
    }
};
}  // namespace std

struct TCompilerDeleter
{
    void operator()(TCompiler *compiler) const { DeleteCompiler(compiler); }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    // Reserve some size for future compile options
    const size_t kHeaderSize = 128;

    if (size <= kHeaderSize)
    {
        return 0;
    }

    // Make sure the rest of data will be a valid C string so that we don't have to copy it.
    if (data[size - 1] != 0)
    {
        return 0;
    }

    uint32_t type    = *reinterpret_cast<const uint32_t *>(data);
    uint32_t spec    = *reinterpret_cast<const uint32_t *>(data + 4);
    uint32_t output  = *reinterpret_cast<const uint32_t *>(data + 8);
    uint64_t options = *reinterpret_cast<const uint64_t *>(data + 12);

    if (type != GL_FRAGMENT_SHADER && type != GL_VERTEX_SHADER)
    {
        return 0;
    }

    if (spec != SH_GLES2_SPEC && type != SH_WEBGL_SPEC && spec != SH_GLES3_SPEC &&
        spec != SH_WEBGL2_SPEC)
    {
        return 0;
    }

    ShShaderOutput shaderOutput = static_cast<ShShaderOutput>(output);

    ShCompileOptions supportedOptions = kCommonOptions;

    if (IsOutputGLSL(shaderOutput) || IsOutputESSL(shaderOutput))
    {
        supportedOptions |= kGLSLOrESSLOnlyOptions;
#if defined(ANGLE_PLATFORM_APPLE)
        supportedOptions |= kGLSLMacOnlyOptions;
#endif
    }
    else if (IsOutputVulkan(shaderOutput))
    {
        supportedOptions |= kVulkanOnlyOptions;
    }
    else if (IsOutputHLSL(shaderOutput))
    {
        supportedOptions |= kHLSLOnlyOptions;
    }

    // If there are any options not supported with this output, don't attempt to run the translator.
    if ((options & ~supportedOptions) != 0)
    {
        return 0;
    }

    std::vector<uint32_t> validOutputs;
    validOutputs.push_back(SH_ESSL_OUTPUT);
    validOutputs.push_back(SH_GLSL_COMPATIBILITY_OUTPUT);
    validOutputs.push_back(SH_GLSL_130_OUTPUT);
    validOutputs.push_back(SH_GLSL_140_OUTPUT);
    validOutputs.push_back(SH_GLSL_150_CORE_OUTPUT);
    validOutputs.push_back(SH_GLSL_330_CORE_OUTPUT);
    validOutputs.push_back(SH_GLSL_400_CORE_OUTPUT);
    validOutputs.push_back(SH_GLSL_410_CORE_OUTPUT);
    validOutputs.push_back(SH_GLSL_420_CORE_OUTPUT);
    validOutputs.push_back(SH_GLSL_430_CORE_OUTPUT);
    validOutputs.push_back(SH_GLSL_440_CORE_OUTPUT);
    validOutputs.push_back(SH_GLSL_450_CORE_OUTPUT);
    validOutputs.push_back(SH_HLSL_3_0_OUTPUT);
    validOutputs.push_back(SH_HLSL_4_1_OUTPUT);
    validOutputs.push_back(SH_HLSL_4_0_FL9_3_OUTPUT);
    bool found = false;
    for (auto valid : validOutputs)
    {
        found = found || (valid == output);
    }
    if (!found)
    {
        return 0;
    }

    size -= kHeaderSize;
    data += kHeaderSize;

    sh::InitializeGlslang();
    if (!sh::Initialize())
    {
        return 0;
    }

    TranslatorCacheKey key;
    key.type   = type;
    key.spec   = spec;
    key.output = output;

    using UniqueTCompiler = std::unique_ptr<TCompiler, TCompilerDeleter>;
    static angle::base::NoDestructor<angle::HashMap<TranslatorCacheKey, UniqueTCompiler>>
        translators;

    if (translators->find(key) == translators->end())
    {
        UniqueTCompiler translator(
            ConstructCompiler(type, static_cast<ShShaderSpec>(spec), shaderOutput));

        if (translator == nullptr)
        {
            return 0;
        }

        ShBuiltInResources resources;
        sh::InitBuiltInResources(&resources);

        // Enable all the extensions to have more coverage
        resources.OES_standard_derivatives        = 1;
        resources.OES_EGL_image_external          = 1;
        resources.OES_EGL_image_external_essl3    = 1;
        resources.NV_EGL_stream_consumer_external = 1;
        resources.ARB_texture_rectangle           = 1;
        resources.EXT_blend_func_extended         = 1;
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
        resources.EXT_primitive_bounding_box      = 1;
        resources.OES_primitive_bounding_box      = 1;

        if (!translator->Init(resources))
        {
            return 0;
        }

        (*translators)[key] = std::move(translator);
    }

    auto &translator = (*translators)[key];

    const char *shaderStrings[] = {reinterpret_cast<const char *>(data)};
    translator->compile(shaderStrings, 1, options);

    return 0;
}
