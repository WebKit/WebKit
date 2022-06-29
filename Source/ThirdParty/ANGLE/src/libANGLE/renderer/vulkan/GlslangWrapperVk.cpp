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
// static
GlslangSourceOptions GlslangWrapperVk::CreateSourceOptions(const angle::FeaturesVk &features)
{
    GlslangSourceOptions options;

    options.supportsTransformFeedbackExtension =
        features.supportsTransformFeedbackExtension.enabled;
    options.supportsTransformFeedbackEmulation = features.emulateTransformFeedback.enabled;
    options.enableTransformFeedbackEmulation   = options.supportsTransformFeedbackEmulation;

    return options;
}

// static
void GlslangWrapperVk::ResetGlslangProgramInterfaceInfo(
    GlslangProgramInterfaceInfo *glslangProgramInterfaceInfo)
{
    glslangProgramInterfaceInfo->uniformsAndXfbDescriptorSetIndex =
        ToUnderlying(DescriptorSetIndex::UniformsAndXfb);
    glslangProgramInterfaceInfo->currentUniformBindingIndex = 0;
    glslangProgramInterfaceInfo->textureDescriptorSetIndex =
        ToUnderlying(DescriptorSetIndex::Texture);
    glslangProgramInterfaceInfo->currentTextureBindingIndex = 0;
    glslangProgramInterfaceInfo->shaderResourceDescriptorSetIndex =
        ToUnderlying(DescriptorSetIndex::ShaderResource);
    glslangProgramInterfaceInfo->currentShaderResourceBindingIndex = 0;
    glslangProgramInterfaceInfo->driverUniformsDescriptorSetIndex =
        ToUnderlying(DescriptorSetIndex::Internal);

    glslangProgramInterfaceInfo->locationsUsedForXfbExtension = 0;
}

// static
void GlslangWrapperVk::GetShaderCode(const angle::FeaturesVk &features,
                                     const gl::ProgramState &programState,
                                     const gl::ProgramLinkedResources &resources,
                                     GlslangProgramInterfaceInfo *programInterfaceInfo,
                                     gl::ShaderMap<const angle::spirv::Blob *> *spirvBlobsOut,
                                     ShaderInterfaceVariableInfoMap *variableInfoMapOut)
{
    GlslangSourceOptions options = CreateSourceOptions(features);
    GlslangGetShaderSpirvCode(options, programState, resources, programInterfaceInfo, spirvBlobsOut,
                              variableInfoMapOut);
}

// static
angle::Result GlslangWrapperVk::TransformSpirV(
    const GlslangSpirvOptions &options,
    const ShaderInterfaceVariableInfoMap &variableInfoMap,
    const angle::spirv::Blob &initialSpirvBlob,
    angle::spirv::Blob *shaderCodeOut)
{
    return GlslangTransformSpirvCode(options, variableInfoMap, initialSpirvBlob, shaderCodeOut);
}
}  // namespace rx
