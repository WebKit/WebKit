//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// GlslangWrapper: Wrapper for Vulkan's glslang compiler.
//

#include "libANGLE/renderer/vulkan/GlslangWrapper.h"

// glslang's version of ShaderLang.h, not to be confused with ANGLE's.
// Our function defs conflict with theirs, but we carefully manage our includes to prevent this.
#include <ShaderLang.h>

// Other glslang includes.
#include <StandAlone/ResourceLimits.h>
#include <SPIRV/GlslangToSpv.h>

#include <array>

#include "common/string_utils.h"
#include "common/utilities.h"
#include "libANGLE/ProgramLinkedResources.h"

namespace rx
{

namespace
{

void InsertLayoutSpecifierString(std::string *shaderString,
                                 const std::string &variableName,
                                 const std::string &layoutString)
{
    std::stringstream searchStringBuilder;
    searchStringBuilder << "@@ LAYOUT-" << variableName << " @@";
    std::string searchString = searchStringBuilder.str();

    angle::ReplaceSubstring(shaderString, searchString, layoutString);
}

}  // anonymous namespace

// static
GlslangWrapper *GlslangWrapper::mInstance = nullptr;

// static
GlslangWrapper *GlslangWrapper::GetReference()
{
    if (!mInstance)
    {
        mInstance = new GlslangWrapper();
    }

    mInstance->addRef();

    return mInstance;
}

// static
void GlslangWrapper::ReleaseReference()
{
    if (mInstance->getRefCount() == 1)
    {
        mInstance->release();
        mInstance = nullptr;
    }
    else
    {
        mInstance->release();
    }
}

GlslangWrapper::GlslangWrapper()
{
    int result = ShInitialize();
    ASSERT(result != 0);
}

GlslangWrapper::~GlslangWrapper()
{
    int result = ShFinalize();
    ASSERT(result != 0);
}

gl::LinkResult GlslangWrapper::linkProgram(const gl::Context *glContext,
                                           const gl::ProgramState &programState,
                                           const gl::ProgramLinkedResources &resources,
                                           std::vector<uint32_t> *vertexCodeOut,
                                           std::vector<uint32_t> *fragmentCodeOut)
{
    gl::Shader *glVertexShader   = programState.getAttachedVertexShader();
    gl::Shader *glFragmentShader = programState.getAttachedFragmentShader();

    std::string vertexSource   = glVertexShader->getTranslatedSource(glContext);
    std::string fragmentSource = glFragmentShader->getTranslatedSource(glContext);

    // Parse attribute locations and replace them in the vertex shader.
    // See corresponding code in OutputVulkanGLSL.cpp.
    // TODO(jmadill): Also do the same for ESSL 3 fragment outputs.
    for (const auto &attribute : programState.getAttributes())
    {
        if (!attribute.staticUse)
            continue;

        std::string locationString = "location = " + Str(attribute.location);
        InsertLayoutSpecifierString(&vertexSource, attribute.name, locationString);
    }

    // Assign varying locations.
    // TODO(jmadill): This might need to be redone.
    for (const auto &varyingReg : resources.varyingPacking.getRegisterList())
    {
        const auto &varying        = *varyingReg.packedVarying;
        std::string locationString = "location = " + Str(varyingReg.registerRow);
        InsertLayoutSpecifierString(&vertexSource, varying.varying->name, locationString);
        InsertLayoutSpecifierString(&fragmentSource, varying.varying->name, locationString);
    }

    // Bind the default uniforms for vertex and fragment shaders.
    // See corresponding code in OutputVulkanGLSL.cpp.
    std::stringstream searchStringBuilder;
    searchStringBuilder << "@@ DEFAULT-UNIFORMS-SET-BINDING @@";
    std::string searchString = searchStringBuilder.str();

    std::string vertexDefaultUniformsBinding   = "set = 0, binding = 0";
    std::string fragmentDefaultUniformsBinding = "set = 0, binding = 1";

    angle::ReplaceSubstring(&vertexSource, searchString, vertexDefaultUniformsBinding);
    angle::ReplaceSubstring(&fragmentSource, searchString, fragmentDefaultUniformsBinding);

    // Assign textures to a descriptor set and binding.
    int textureCount     = 0;
    const auto &uniforms = programState.getUniforms();
    for (unsigned int uniformIndex : programState.getSamplerUniformRange())
    {
        const gl::LinkedUniform &samplerUniform = uniforms[uniformIndex];

        std::string setBindingString = "set = 1, binding = " + Str(textureCount);

        ASSERT(samplerUniform.vertexStaticUse || samplerUniform.fragmentStaticUse);
        if (samplerUniform.vertexStaticUse)
        {
            InsertLayoutSpecifierString(&vertexSource, samplerUniform.name, setBindingString);
        }

        if (samplerUniform.fragmentStaticUse)
        {
            InsertLayoutSpecifierString(&fragmentSource, samplerUniform.name, setBindingString);
        }

        textureCount += samplerUniform.getBasicTypeElementCount();
    }

    std::array<const char *, 2> strings = {{vertexSource.c_str(), fragmentSource.c_str()}};

    std::array<int, 2> lengths = {
        {static_cast<int>(vertexSource.length()), static_cast<int>(fragmentSource.length())}};

    // Enable SPIR-V and Vulkan rules when parsing GLSL
    EShMessages messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);

    glslang::TShader vertexShader(EShLangVertex);
    vertexShader.setStringsWithLengths(&strings[0], &lengths[0], 1);
    vertexShader.setEntryPoint("main");
    bool vertexResult = vertexShader.parse(&glslang::DefaultTBuiltInResource, 450, ECoreProfile,
                                           false, false, messages);
    if (!vertexResult)
    {
        return gl::InternalError() << "Internal error parsing Vulkan vertex shader:\n"
                                   << vertexShader.getInfoLog() << "\n"
                                   << vertexShader.getInfoDebugLog() << "\n";
    }

    glslang::TShader fragmentShader(EShLangFragment);
    fragmentShader.setStringsWithLengths(&strings[1], &lengths[1], 1);
    fragmentShader.setEntryPoint("main");
    bool fragmentResult = fragmentShader.parse(&glslang::DefaultTBuiltInResource, 450, ECoreProfile,
                                               false, false, messages);
    if (!fragmentResult)
    {
        return gl::InternalError() << "Internal error parsing Vulkan fragment shader:\n"
                                   << fragmentShader.getInfoLog() << "\n"
                                   << fragmentShader.getInfoDebugLog() << "\n";
    }

    glslang::TProgram program;
    program.addShader(&vertexShader);
    program.addShader(&fragmentShader);
    bool linkResult = program.link(messages);
    if (!linkResult)
    {
        return gl::InternalError() << "Internal error linking Vulkan shaders:\n"
                                   << program.getInfoLog() << "\n";
    }

    glslang::TIntermediate *vertexStage   = program.getIntermediate(EShLangVertex);
    glslang::TIntermediate *fragmentStage = program.getIntermediate(EShLangFragment);
    glslang::GlslangToSpv(*vertexStage, *vertexCodeOut);
    glslang::GlslangToSpv(*fragmentStage, *fragmentCodeOut);

    return true;
}

}  // namespace rx
