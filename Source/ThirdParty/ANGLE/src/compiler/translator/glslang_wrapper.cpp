//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// glslang_wrapper:
//   A wrapper to compile GLSL strings to SPIR-V blobs.  glslang here refers to the Khronos
//   compiler.
//
//   This file is separated as glslang's header contains conflicting macro definitions with ANGLE's.
//

#include "compiler/translator/glslang_wrapper.h"

// glslang has issues with some specific warnings.
ANGLE_DISABLE_SHADOWING_WARNING
ANGLE_DISABLE_SUGGEST_OVERRIDE_WARNINGS

// glslang's version of ShaderLang.h, not to be confused with ANGLE's.
#include <glslang/Public/ShaderLang.h>

// Other glslang includes.
#include <SPIRV/GlslangToSpv.h>
#include <StandAlone/ResourceLimits.h>

// SPIR-V tools include for disassembly
#include <spirv-tools/libspirv.hpp>

// Enable this for debug logging of pre-transform SPIR-V:
#if !defined(ANGLE_DEBUG_SPIRV_GENERATION)
#    define ANGLE_DEBUG_SPIRV_GENERATION 0
#endif  // !defined(ANGLE_DEBUG_SPIRV_GENERATION)

ANGLE_REENABLE_SUGGEST_OVERRIDE_WARNINGS
ANGLE_REENABLE_SHADOWING_WARNING

namespace sh
{
namespace
{
// Run at startup to warm up glslang's internals to avoid hitches on first shader compile.
void Warmup()
{
    EShMessages messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);

    const TBuiltInResource builtInResources(glslang::DefaultTBuiltInResource);
    glslang::TShader warmUpShader(EShLangVertex);

    const char *kShaderString = R"(#version 450 core
        void main(){}
    )";
    const int kShaderLength   = static_cast<int>(strlen(kShaderString));

    warmUpShader.setStringsWithLengths(&kShaderString, &kShaderLength, 1);
    warmUpShader.setEntryPoint("main");

    bool result = warmUpShader.parse(&builtInResources, 450, ECoreProfile, false, false, messages);
    ASSERT(result);
}

// Generate glslang resources from ANGLE translator resources.
void GetBuiltInResources(const ShBuiltInResources &resources, TBuiltInResource *outResources)
{
    outResources->maxDrawBuffers                   = resources.MaxDrawBuffers;
    outResources->maxAtomicCounterBindings         = resources.MaxAtomicCounterBindings;
    outResources->maxAtomicCounterBufferSize       = resources.MaxAtomicCounterBufferSize;
    outResources->maxCombinedAtomicCounterBuffers  = resources.MaxCombinedAtomicCounterBuffers;
    outResources->maxCombinedAtomicCounters        = resources.MaxCombinedAtomicCounters;
    outResources->maxCombinedImageUniforms         = resources.MaxCombinedImageUniforms;
    outResources->maxCombinedTextureImageUnits     = resources.MaxCombinedTextureImageUnits;
    outResources->maxCombinedShaderOutputResources = resources.MaxCombinedShaderOutputResources;
    outResources->maxComputeWorkGroupCountX        = resources.MaxComputeWorkGroupCount[0];
    outResources->maxComputeWorkGroupCountY        = resources.MaxComputeWorkGroupCount[1];
    outResources->maxComputeWorkGroupCountZ        = resources.MaxComputeWorkGroupCount[2];
    outResources->maxComputeWorkGroupSizeX         = resources.MaxComputeWorkGroupSize[0];
    outResources->maxComputeWorkGroupSizeY         = resources.MaxComputeWorkGroupSize[1];
    outResources->maxComputeWorkGroupSizeZ         = resources.MaxComputeWorkGroupSize[2];
    outResources->minProgramTexelOffset            = resources.MinProgramTexelOffset;
    outResources->maxFragmentUniformVectors        = resources.MaxFragmentUniformVectors;
    outResources->maxGeometryInputComponents       = resources.MaxGeometryInputComponents;
    outResources->maxGeometryOutputComponents      = resources.MaxGeometryOutputComponents;
    outResources->maxGeometryOutputVertices        = resources.MaxGeometryOutputVertices;
    outResources->maxGeometryTotalOutputComponents = resources.MaxGeometryTotalOutputComponents;
    outResources->maxPatchVertices                 = resources.MaxPatchVertices;
    outResources->maxProgramTexelOffset            = resources.MaxProgramTexelOffset;
    outResources->maxVaryingVectors                = resources.MaxVaryingVectors;
    outResources->maxVertexUniformVectors          = resources.MaxVertexUniformVectors;
    outResources->maxClipDistances                 = resources.MaxClipDistances;
    outResources->maxSamples                       = resources.MaxSamples;
    outResources->maxCullDistances                 = resources.MaxCullDistances;
    outResources->maxCombinedClipAndCullDistances  = resources.MaxCombinedClipAndCullDistances;
}
}  // anonymous namespace

void GlslangInitialize()
{
    int result = ShInitialize();
    ASSERT(result != 0);
    Warmup();
}

void GlslangFinalize()
{
    int result = ShFinalize();
    ASSERT(result != 0);
}

ANGLE_NO_DISCARD bool GlslangCompileToSpirv(const ShBuiltInResources &resources,
                                            sh::GLenum shaderType,
                                            const std::string &shaderSource,
                                            angle::spirv::Blob *spirvBlobOut)
{
    TBuiltInResource builtInResources(glslang::DefaultTBuiltInResource);
    GetBuiltInResources(resources, &builtInResources);

    EShLanguage language = EShLangVertex;

    switch (shaderType)
    {
        case GL_VERTEX_SHADER:
            language = EShLangVertex;
            break;
        case GL_TESS_CONTROL_SHADER:
            language = EShLangTessControl;
            break;
        case GL_TESS_EVALUATION_SHADER_EXT:
            language = EShLangTessEvaluation;
            break;
        case GL_GEOMETRY_SHADER:
            language = EShLangGeometry;
            break;
        case GL_FRAGMENT_SHADER:
            language = EShLangFragment;
            break;
        case GL_COMPUTE_SHADER:
            language = EShLangCompute;
            break;
        default:
            UNREACHABLE();
    }

    glslang::TShader shader(language);
    glslang::TProgram program;

    // Enable SPIR-V and Vulkan rules when parsing GLSL
    constexpr EShMessages messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);

    const char *shaderString = shaderSource.c_str();
    int shaderLength         = static_cast<int>(shaderSource.size());

    shader.setStringsWithLengths(&shaderString, &shaderLength, 1);
    shader.setEntryPoint("main");

#if ANGLE_DEBUG_SPIRV_GENERATION
    fprintf(stderr, "%s\n", shaderString);
#endif  // ANGLE_DEBUG_SPIRV_GENERATION

    bool result = shader.parse(&builtInResources, 450, ECoreProfile, false, false, messages);
    if (!result)
    {
        WARN() << "Internal error parsing Vulkan shader corresponding to " << shaderType << ":\n"
               << shader.getInfoLog() << "\n"
               << shader.getInfoDebugLog() << "\n";
        return false;
    }

    program.addShader(&shader);

    bool linkResult = program.link(messages);
    if (!linkResult)
    {
        WARN() << "Internal error linking Vulkan shader:\n" << program.getInfoLog() << "\n";
    }

    glslang::TIntermediate *intermediate = program.getIntermediate(language);
    glslang::GlslangToSpv(*intermediate, *spirvBlobOut);

#if ANGLE_DEBUG_SPIRV_GENERATION
    spvtools::SpirvTools spirvTools(SPV_ENV_VULKAN_1_1);
    std::string readableSpirv;
    spirvTools.Disassemble(*spirvBlobOut, &readableSpirv, 0);
    fprintf(stderr, "%s\n", readableSpirv.c_str());
#endif  // ANGLE_DEBUG_SPIRV_GENERATION

    return true;
}
}  // namespace sh
