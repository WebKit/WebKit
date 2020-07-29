//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// GlslangWrapperVk: Wrapper for Vulkan's glslang compiler.
//

#include "libANGLE/renderer/vulkan/GlslangWrapperVk.h"

#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/vk_cache_utils.h"

namespace rx
{
namespace
{
angle::Result ErrorHandler(vk::Context *context, GlslangError)
{
    ANGLE_VK_CHECK(context, false, VK_ERROR_INVALID_SHADER_NV);
    return angle::Result::Stop;
}

}  // namespace

// static
GlslangSourceOptions GlslangWrapperVk::CreateSourceOptions(const angle::FeaturesVk &features)
{
    GlslangSourceOptions options;

    options.useOldRewriteStructSamplers = features.forceOldRewriteStructSamplers.enabled;
    options.supportsTransformFeedbackExtension =
        features.supportsTransformFeedbackExtension.enabled;
    options.emulateTransformFeedback = features.emulateTransformFeedback.enabled;
    options.emulateBresenhamLines    = features.basicGLLineRasterization.enabled;

    return options;
}

// static
void GlslangWrapperVk::ResetGlslangProgramInterfaceInfo(
    GlslangProgramInterfaceInfo *glslangProgramInterfaceInfo)
{
    glslangProgramInterfaceInfo->uniformsAndXfbDescriptorSetIndex =
        kUniformsAndXfbDescriptorSetIndex;
    glslangProgramInterfaceInfo->currentUniformBindingIndex = 0;
    glslangProgramInterfaceInfo->textureDescriptorSetIndex  = kTextureDescriptorSetIndex;
    glslangProgramInterfaceInfo->currentTextureBindingIndex = 0;
    glslangProgramInterfaceInfo->shaderResourceDescriptorSetIndex =
        kShaderResourceDescriptorSetIndex;
    glslangProgramInterfaceInfo->currentShaderResourceBindingIndex = 0;
    glslangProgramInterfaceInfo->driverUniformsDescriptorSetIndex =
        kDriverUniformsDescriptorSetIndex;

    glslangProgramInterfaceInfo->locationsUsedForXfbExtension = 0;
}

// static
void GlslangWrapperVk::GetShaderSource(const angle::FeaturesVk &features,
                                       const gl::ProgramState &programState,
                                       const gl::ProgramLinkedResources &resources,
                                       GlslangProgramInterfaceInfo *programInterfaceInfo,
                                       gl::ShaderMap<std::string> *shaderSourcesOut,
                                       ShaderMapInterfaceVariableInfoMap *variableInfoMapOut)
{
    GlslangSourceOptions options = CreateSourceOptions(features);
    GlslangGetShaderSource(options, programState, resources, programInterfaceInfo, shaderSourcesOut,
                           variableInfoMapOut);
}

// static
angle::Result GlslangWrapperVk::GetShaderCode(
    vk::Context *context,
    const gl::ShaderBitSet &linkedShaderStages,
    const gl::Caps &glCaps,
    const gl::ShaderMap<std::string> &shaderSources,
    const ShaderMapInterfaceVariableInfoMap &variableInfoMap,
    gl::ShaderMap<std::vector<uint32_t>> *shaderCodeOut)
{
    return GlslangGetShaderSpirvCode(
        [context](GlslangError error) { return ErrorHandler(context, error); }, linkedShaderStages,
        glCaps, shaderSources, variableInfoMap, shaderCodeOut);
}

// static
angle::Result GlslangWrapperVk::TransformSpirV(
    vk::Context *context,
    const gl::ShaderType shaderType,
    bool removeEarlyFragmentTestsOptimization,
    const ShaderInterfaceVariableInfoMap &variableInfoMap,
    const SpirvBlob &initialSpirvBlob,
    SpirvBlob *shaderCodeOut)
{
    const bool removeDebugInfo = !context->getRenderer()->getEnableValidationLayers();

    return GlslangTransformSpirvCode(
        [context](GlslangError error) { return ErrorHandler(context, error); }, shaderType,
        removeEarlyFragmentTestsOptimization, removeDebugInfo, variableInfoMap, initialSpirvBlob,
        shaderCodeOut);
}
}  // namespace rx
