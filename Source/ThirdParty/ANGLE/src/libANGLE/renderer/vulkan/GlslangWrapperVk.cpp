//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// GlslangWrapperVk: Wrapper for Vulkan's glslang compiler.
//

#include "libANGLE/renderer/vulkan/GlslangWrapperVk.h"

#include "libANGLE/renderer/glslang_wrapper_utils.h"
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

GlslangSourceOptions CreateSourceOptions()
{
    GlslangSourceOptions options;
    options.uniformsAndXfbDescriptorSetIndex = kUniformsAndXfbDescriptorSetIndex;
    options.textureDescriptorSetIndex        = kTextureDescriptorSetIndex;
    options.shaderResourceDescriptorSetIndex = kShaderResourceDescriptorSetIndex;
    options.driverUniformsDescriptorSetIndex = kDriverUniformsDescriptorSetIndex;
    options.xfbBindingIndexStart             = kXfbBindingIndexStart;
    return options;
}
}  // namespace

// static
void GlslangWrapperVk::GetShaderSource(bool useOldRewriteStructSamplers,
                                       const gl::ProgramState &programState,
                                       const gl::ProgramLinkedResources &resources,
                                       gl::ShaderMap<std::string> *shaderSourcesOut)
{
    GlslangGetShaderSource(CreateSourceOptions(), useOldRewriteStructSamplers, programState,
                           resources, shaderSourcesOut);
}

// static
angle::Result GlslangWrapperVk::GetShaderCode(vk::Context *context,
                                              const gl::Caps &glCaps,
                                              bool enableLineRasterEmulation,
                                              const gl::ShaderMap<std::string> &shaderSources,
                                              gl::ShaderMap<std::vector<uint32_t>> *shaderCodeOut)
{
    return GlslangGetShaderSpirvCode(
        [context](GlslangError error) { return ErrorHandler(context, error); }, glCaps,
        enableLineRasterEmulation, shaderSources, shaderCodeOut);
}
}  // namespace rx
