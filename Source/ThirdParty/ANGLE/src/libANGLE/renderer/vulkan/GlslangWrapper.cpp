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

namespace rx
{

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

LinkResult GlslangWrapper::linkProgram(const std::string &vertexSource,
                                       const std::string &fragmentSource,
                                       std::vector<uint32_t> *vertexCodeOut,
                                       std::vector<uint32_t> *fragmentCodeOut)
{
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
