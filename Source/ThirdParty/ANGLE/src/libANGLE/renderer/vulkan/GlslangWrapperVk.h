//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// GlslangWrapperVk: Wrapper for Vulkan's glslang compiler.
//

#ifndef LIBANGLE_RENDERER_VULKAN_GLSLANG_WRAPPER_H_
#define LIBANGLE_RENDERER_VULKAN_GLSLANG_WRAPPER_H_

#include "libANGLE/renderer/ProgramImpl.h"
#include "libANGLE/renderer/glslang_wrapper_utils.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"

namespace angle
{
struct FeaturesVk;
}  // namespace angle

namespace rx
{
// This class currently holds no state. If we want to hold state we would need to solve the
// potential race conditions with multiple threads.
class GlslangWrapperVk
{
  public:
    static void GetShaderSource(const angle::FeaturesVk &features,
                                const gl::ProgramState &programState,
                                const gl::ProgramLinkedResources &resources,
                                gl::ShaderMap<std::string> *shaderSourcesOut,
                                ShaderInterfaceVariableInfoMap *variableInfoMapOut);

    static angle::Result GetShaderCode(vk::Context *context,
                                       const gl::Caps &glCaps,
                                       const gl::ShaderMap<std::string> &shaderSources,
                                       const ShaderInterfaceVariableInfoMap &variableInfoMap,
                                       gl::ShaderMap<std::vector<uint32_t>> *shaderCodesOut);
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_GLSLANG_WRAPPER_H_
