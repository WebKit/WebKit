//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// GlslangWrapper: Wrapper for Vulkan's glslang compiler.
//

#include "libANGLE/renderer/vulkan/GlslangWrapper.h"

// glslang has issues with some specific warnings.
ANGLE_DISABLE_EXTRA_SEMI_WARNING

// glslang's version of ShaderLang.h, not to be confused with ANGLE's.
#include <glslang/Public/ShaderLang.h>

// Other glslang includes.
#include <SPIRV/GlslangToSpv.h>
#include <StandAlone/ResourceLimits.h>

ANGLE_REENABLE_EXTRA_SEMI_WARNING

#include <array>

#include "common/FixedVector.h"
#include "common/string_utils.h"
#include "common/utilities.h"
#include "libANGLE/Caps.h"
#include "libANGLE/ProgramLinkedResources.h"

namespace rx
{
namespace
{
constexpr char kQualifierMarkerBegin[] = "@@ QUALIFIER-";
constexpr char kLayoutMarkerBegin[]    = "@@ LAYOUT-";
constexpr char kMarkerEnd[]            = " @@";
constexpr char kUniformQualifier[]     = "uniform";
constexpr char kVersionDefine[]        = "#version 450 core\n";
constexpr char kLineRasterDefine[]     = R"(#version 450 core

#define ANGLE_ENABLE_LINE_SEGMENT_RASTERIZATION
)";

void GetBuiltInResourcesFromCaps(const gl::Caps &caps, TBuiltInResource *outBuiltInResources)
{
    outBuiltInResources->maxDrawBuffers                   = caps.maxDrawBuffers;
    outBuiltInResources->maxAtomicCounterBindings         = caps.maxAtomicCounterBufferBindings;
    outBuiltInResources->maxAtomicCounterBufferSize       = caps.maxAtomicCounterBufferSize;
    outBuiltInResources->maxClipPlanes                    = caps.maxClipPlanes;
    outBuiltInResources->maxCombinedAtomicCounterBuffers  = caps.maxCombinedAtomicCounterBuffers;
    outBuiltInResources->maxCombinedAtomicCounters        = caps.maxCombinedAtomicCounters;
    outBuiltInResources->maxCombinedImageUniforms         = caps.maxCombinedImageUniforms;
    outBuiltInResources->maxCombinedTextureImageUnits     = caps.maxCombinedTextureImageUnits;
    outBuiltInResources->maxCombinedShaderOutputResources = caps.maxCombinedShaderOutputResources;
    outBuiltInResources->maxComputeWorkGroupCountX        = caps.maxComputeWorkGroupCount[0];
    outBuiltInResources->maxComputeWorkGroupCountY        = caps.maxComputeWorkGroupCount[1];
    outBuiltInResources->maxComputeWorkGroupCountZ        = caps.maxComputeWorkGroupCount[2];
    outBuiltInResources->maxComputeWorkGroupSizeX         = caps.maxComputeWorkGroupSize[0];
    outBuiltInResources->maxComputeWorkGroupSizeY         = caps.maxComputeWorkGroupSize[1];
    outBuiltInResources->maxComputeWorkGroupSizeZ         = caps.maxComputeWorkGroupSize[2];
    outBuiltInResources->minProgramTexelOffset            = caps.minProgramTexelOffset;
    outBuiltInResources->maxFragmentUniformVectors        = caps.maxFragmentUniformVectors;
    outBuiltInResources->maxFragmentInputComponents       = caps.maxFragmentInputComponents;
    outBuiltInResources->maxGeometryInputComponents       = caps.maxGeometryInputComponents;
    outBuiltInResources->maxGeometryOutputComponents      = caps.maxGeometryOutputComponents;
    outBuiltInResources->maxGeometryOutputVertices        = caps.maxGeometryOutputVertices;
    outBuiltInResources->maxGeometryTotalOutputComponents = caps.maxGeometryTotalOutputComponents;
    outBuiltInResources->maxLights                        = caps.maxLights;
    outBuiltInResources->maxProgramTexelOffset            = caps.maxProgramTexelOffset;
    outBuiltInResources->maxVaryingComponents             = caps.maxVaryingComponents;
    outBuiltInResources->maxVaryingVectors                = caps.maxVaryingVectors;
    outBuiltInResources->maxVertexAttribs                 = caps.maxVertexAttributes;
    outBuiltInResources->maxVertexOutputComponents        = caps.maxVertexOutputComponents;
    outBuiltInResources->maxVertexUniformVectors          = caps.maxVertexUniformVectors;
}

void InsertLayoutSpecifierString(std::string *shaderString,
                                 const std::string &variableName,
                                 const std::string &layoutString)
{
    std::stringstream searchStringBuilder;
    searchStringBuilder << kLayoutMarkerBegin << variableName << kMarkerEnd;
    std::string searchString = searchStringBuilder.str();

    if (!layoutString.empty())
    {
        angle::ReplaceSubstring(shaderString, searchString, "layout(" + layoutString + ")");
    }
    else
    {
        angle::ReplaceSubstring(shaderString, searchString, layoutString);
    }
}

void InsertQualifierSpecifierString(std::string *shaderString,
                                    const std::string &variableName,
                                    const std::string &replacementString)
{
    std::stringstream searchStringBuilder;
    searchStringBuilder << kQualifierMarkerBegin << variableName << kMarkerEnd;
    std::string searchString = searchStringBuilder.str();
    angle::ReplaceSubstring(shaderString, searchString, replacementString);
}

void EraseLayoutAndQualifierStrings(std::string *vertexSource,
                                    std::string *fragmentSource,
                                    const std::string &uniformName)
{
    InsertLayoutSpecifierString(vertexSource, uniformName, "");
    InsertLayoutSpecifierString(fragmentSource, uniformName, "");

    InsertQualifierSpecifierString(vertexSource, uniformName, "");
    InsertQualifierSpecifierString(fragmentSource, uniformName, "");
}

std::string GetMappedSamplerName(const std::string &originalName)
{
    std::string samplerName = gl::ParseResourceName(originalName, nullptr);

    // Samplers in structs are extracted.
    std::replace(samplerName.begin(), samplerName.end(), '.', '_');

    // Samplers in arrays of structs are also extracted.
    std::replace(samplerName.begin(), samplerName.end(), '[', '_');
    samplerName.erase(std::remove(samplerName.begin(), samplerName.end(), ']'), samplerName.end());
    return samplerName;
}
}  // anonymous namespace

// static
void GlslangWrapper::Initialize()
{
    int result = ShInitialize();
    ASSERT(result != 0);
}

// static
void GlslangWrapper::Release()
{
    int result = ShFinalize();
    ASSERT(result != 0);
}

// static
void GlslangWrapper::GetShaderSource(const gl::ProgramState &programState,
                                     const gl::ProgramLinkedResources &resources,
                                     std::string *vertexSourceOut,
                                     std::string *fragmentSourceOut)
{
    gl::Shader *glVertexShader   = programState.getAttachedShader(gl::ShaderType::Vertex);
    gl::Shader *glFragmentShader = programState.getAttachedShader(gl::ShaderType::Fragment);

    std::string vertexSource   = glVertexShader->getTranslatedSource();
    std::string fragmentSource = glFragmentShader->getTranslatedSource();

    // Parse attribute locations and replace them in the vertex shader.
    // See corresponding code in OutputVulkanGLSL.cpp.
    for (const sh::Attribute &attribute : programState.getAttributes())
    {
        // Warning: If we endup supporting ES 3.0 shaders and up, Program::linkAttributes is going
        // to bring us all attributes in this list instead of only the active ones.
        ASSERT(attribute.active);

        std::string locationString = "location = " + Str(attribute.location);
        InsertLayoutSpecifierString(&vertexSource, attribute.name, locationString);
        InsertQualifierSpecifierString(&vertexSource, attribute.name, "in");
    }

    // The attributes in the programState could have been filled with active attributes only
    // depending on the shader version. If there is inactive attributes left, we have to remove
    // their @@ QUALIFIER and @@ LAYOUT markers.
    for (const sh::Attribute &attribute : glVertexShader->getAllAttributes())
    {
        if (attribute.active)
        {
            continue;
        }

        InsertLayoutSpecifierString(&vertexSource, attribute.name, "");
        InsertQualifierSpecifierString(&vertexSource, attribute.name, "");
    }

    // Parse output locations and replace them in the fragment shader.
    // See corresponding code in OutputVulkanGLSL.cpp.
    // TODO(syoussefi): Add support for EXT_blend_func_extended.  http://anglebug.com/3385
    const auto &outputLocations = programState.getOutputLocations();
    const auto &outputVariables = programState.getOutputVariables();
    for (const gl::VariableLocation &outputLocation : outputLocations)
    {
        if (outputLocation.arrayIndex == 0 && outputLocation.used() && !outputLocation.ignored)
        {
            const sh::OutputVariable &outputVar = outputVariables[outputLocation.index];

            std::string locationString;
            if (outputVar.location != -1)
            {
                locationString = "location = " + Str(outputVar.location);
            }
            else
            {
                // If there is only one output, it is allowed not to have a location qualifier, in
                // which case it defaults to 0.  GLSL ES 3.00 spec, section 4.3.8.2.
                ASSERT(outputVariables.size() == 1);
                locationString = "location = 0";
            }

            InsertLayoutSpecifierString(&fragmentSource, outputVar.name, locationString);
        }
    }

    // Assign varying locations.
    for (const gl::PackedVaryingRegister &varyingReg : resources.varyingPacking.getRegisterList())
    {
        const auto &varying = *varyingReg.packedVarying;

        std::string locationString = "location = " + Str(varyingReg.registerRow);
        if (varyingReg.registerColumn > 0)
        {
            ASSERT(!varying.varying->isStruct());
            ASSERT(!gl::IsMatrixType(varying.varying->type));
            locationString += ", component = " + Str(varyingReg.registerColumn);
        }

        InsertLayoutSpecifierString(&vertexSource, varying.varying->name, locationString);
        InsertLayoutSpecifierString(&fragmentSource, varying.varying->name, locationString);

        ASSERT(varying.interpolation == sh::INTERPOLATION_SMOOTH);
        InsertQualifierSpecifierString(&vertexSource, varying.varying->name, "out");
        InsertQualifierSpecifierString(&fragmentSource, varying.varying->name, "in");
    }

    // Remove all the markers for unused varyings.
    for (const std::string &varyingName : resources.varyingPacking.getInactiveVaryingNames())
    {
        EraseLayoutAndQualifierStrings(&vertexSource, &fragmentSource, varyingName);
    }

    // Bind the default uniforms for vertex and fragment shaders.
    // See corresponding code in OutputVulkanGLSL.cpp.
    std::string uniformsSearchString("@@ DEFAULT-UNIFORMS-SET-BINDING @@");

    std::string vertexDefaultUniformsBinding   = "set = 0, binding = 0";
    std::string fragmentDefaultUniformsBinding = "set = 0, binding = 1";

    angle::ReplaceSubstring(&vertexSource, uniformsSearchString, vertexDefaultUniformsBinding);
    angle::ReplaceSubstring(&fragmentSource, uniformsSearchString, fragmentDefaultUniformsBinding);

    // Assign textures to a descriptor set and binding.
    int textureCount     = 0;
    const auto &uniforms = programState.getUniforms();
    for (unsigned int uniformIndex : programState.getSamplerUniformRange())
    {
        const gl::LinkedUniform &samplerUniform = uniforms[uniformIndex];
        std::string setBindingString            = "set = 1, binding = " + Str(textureCount);

        // Samplers in structs are extracted and renamed.
        const std::string samplerName = GetMappedSamplerName(samplerUniform.name);

        ASSERT(samplerUniform.isActive(gl::ShaderType::Vertex) ||
               samplerUniform.isActive(gl::ShaderType::Fragment));
        if (samplerUniform.isActive(gl::ShaderType::Vertex))
        {
            InsertLayoutSpecifierString(&vertexSource, samplerName, setBindingString);
        }
        InsertQualifierSpecifierString(&vertexSource, samplerName, kUniformQualifier);

        if (samplerUniform.isActive(gl::ShaderType::Fragment))
        {
            InsertLayoutSpecifierString(&fragmentSource, samplerName, setBindingString);
        }
        InsertQualifierSpecifierString(&fragmentSource, samplerName, kUniformQualifier);

        textureCount++;
    }

    // Start the unused sampler bindings at something ridiculously high.
    constexpr int kBaseUnusedSamplerBinding = 100;
    int unusedSamplerBinding                = kBaseUnusedSamplerBinding;

    for (const gl::UnusedUniform &unusedUniform : resources.unusedUniforms)
    {
        if (unusedUniform.isSampler)
        {
            // Samplers in structs are extracted and renamed.
            std::string uniformName = GetMappedSamplerName(unusedUniform.name);

            std::stringstream layoutStringStream;

            layoutStringStream << "set = 0, binding = " << unusedSamplerBinding++;

            std::string layoutString = layoutStringStream.str();

            InsertLayoutSpecifierString(&vertexSource, uniformName, layoutString);
            InsertLayoutSpecifierString(&fragmentSource, uniformName, layoutString);

            InsertQualifierSpecifierString(&vertexSource, uniformName, kUniformQualifier);
            InsertQualifierSpecifierString(&fragmentSource, uniformName, kUniformQualifier);
        }
        else
        {
            EraseLayoutAndQualifierStrings(&vertexSource, &fragmentSource, unusedUniform.name);
        }
    }

    // Substitute layout and qualifier strings for the driver uniforms block.
    constexpr char kDriverBlockLayoutString[] = "set = 2, binding = 0";
    constexpr char kDriverBlockName[]         = "ANGLEUniforms";
    InsertLayoutSpecifierString(&vertexSource, kDriverBlockName, kDriverBlockLayoutString);
    InsertLayoutSpecifierString(&fragmentSource, kDriverBlockName, kDriverBlockLayoutString);

    InsertQualifierSpecifierString(&vertexSource, kDriverBlockName, kUniformQualifier);
    InsertQualifierSpecifierString(&fragmentSource, kDriverBlockName, kUniformQualifier);

    // Substitute layout and qualifier strings for the position varying. Use the first free
    // varying register after the packed varyings.
    constexpr char kVaryingName[] = "ANGLEPosition";
    std::stringstream layoutStream;
    layoutStream << "location = " << (resources.varyingPacking.getMaxSemanticIndex() + 1);
    const std::string layout = layoutStream.str();
    InsertLayoutSpecifierString(&vertexSource, kVaryingName, layout);
    InsertLayoutSpecifierString(&fragmentSource, kVaryingName, layout);

    InsertQualifierSpecifierString(&vertexSource, kVaryingName, "out");
    InsertQualifierSpecifierString(&fragmentSource, kVaryingName, "in");

    *vertexSourceOut   = vertexSource;
    *fragmentSourceOut = fragmentSource;
}

// static
angle::Result GlslangWrapper::GetShaderCode(vk::Context *context,
                                            const gl::Caps &glCaps,
                                            bool enableLineRasterEmulation,
                                            const std::string &vertexSource,
                                            const std::string &fragmentSource,
                                            std::vector<uint32_t> *vertexCodeOut,
                                            std::vector<uint32_t> *fragmentCodeOut)
{
    if (enableLineRasterEmulation)
    {
        std::string patchedVertexSource   = vertexSource;
        std::string patchedFragmentSource = fragmentSource;

        // #defines must come after the #version directive.
        ANGLE_VK_CHECK(
            context,
            angle::ReplaceSubstring(&patchedVertexSource, kVersionDefine, kLineRasterDefine),
            VK_ERROR_INVALID_SHADER_NV);
        ANGLE_VK_CHECK(
            context,
            angle::ReplaceSubstring(&patchedFragmentSource, kVersionDefine, kLineRasterDefine),
            VK_ERROR_INVALID_SHADER_NV);

        return GetShaderCodeImpl(context, glCaps, patchedVertexSource, patchedFragmentSource,
                                 vertexCodeOut, fragmentCodeOut);
    }
    else
    {
        return GetShaderCodeImpl(context, glCaps, vertexSource, fragmentSource, vertexCodeOut,
                                 fragmentCodeOut);
    }
}

// static
angle::Result GlslangWrapper::GetShaderCodeImpl(vk::Context *context,
                                                const gl::Caps &glCaps,
                                                const std::string &vertexSource,
                                                const std::string &fragmentSource,
                                                std::vector<uint32_t> *vertexCodeOut,
                                                std::vector<uint32_t> *fragmentCodeOut)
{
    std::array<const char *, 2> strings = {{vertexSource.c_str(), fragmentSource.c_str()}};
    std::array<int, 2> lengths          = {
        {static_cast<int>(vertexSource.length()), static_cast<int>(fragmentSource.length())}};

    // Enable SPIR-V and Vulkan rules when parsing GLSL
    EShMessages messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);

    glslang::TShader vertexShader(EShLangVertex);
    vertexShader.setStringsWithLengths(&strings[0], &lengths[0], 1);
    vertexShader.setEntryPoint("main");

    TBuiltInResource builtInResources(glslang::DefaultTBuiltInResource);
    GetBuiltInResourcesFromCaps(glCaps, &builtInResources);

    bool vertexResult =
        vertexShader.parse(&builtInResources, 450, ECoreProfile, false, false, messages);
    if (!vertexResult)
    {
        ERR() << "Internal error parsing Vulkan vertex shader:\n"
              << vertexShader.getInfoLog() << "\n"
              << vertexShader.getInfoDebugLog() << "\n";
        ANGLE_VK_CHECK(context, false, VK_ERROR_INVALID_SHADER_NV);
    }

    glslang::TShader fragmentShader(EShLangFragment);
    fragmentShader.setStringsWithLengths(&strings[1], &lengths[1], 1);
    fragmentShader.setEntryPoint("main");
    bool fragmentResult =
        fragmentShader.parse(&builtInResources, 450, ECoreProfile, false, false, messages);
    if (!fragmentResult)
    {
        ERR() << "Internal error parsing Vulkan fragment shader:\n"
              << fragmentShader.getInfoLog() << "\n"
              << fragmentShader.getInfoDebugLog() << "\n";
        ANGLE_VK_CHECK(context, false, VK_ERROR_INVALID_SHADER_NV);
    }

    glslang::TProgram program;
    program.addShader(&vertexShader);
    program.addShader(&fragmentShader);
    bool linkResult = program.link(messages);
    if (!linkResult)
    {
        ERR() << "Internal error linking Vulkan shaders:\n" << program.getInfoLog() << "\n";
        ANGLE_VK_CHECK(context, false, VK_ERROR_INVALID_SHADER_NV);
    }

    glslang::TIntermediate *vertexStage   = program.getIntermediate(EShLangVertex);
    glslang::TIntermediate *fragmentStage = program.getIntermediate(EShLangFragment);
    glslang::GlslangToSpv(*vertexStage, *vertexCodeOut);
    glslang::GlslangToSpv(*fragmentStage, *fragmentCodeOut);

    return angle::Result::Continue;
}
}  // namespace rx
