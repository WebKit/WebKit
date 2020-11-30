//
// Copyright (c) 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// GlslangUtils: Wrapper for Khronos's glslang compiler.
//

#ifndef LIBANGLE_RENDERER_METAL_GLSLANGWRAPPER_H_
#define LIBANGLE_RENDERER_METAL_GLSLANGWRAPPER_H_

#include "libANGLE/Caps.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/ProgramImpl.h"
#include "libANGLE/renderer/glslang_wrapper_utils.h"
#include "libANGLE/renderer/metal/mtl_common.h"

namespace rx
{
namespace mtl
{

struct SamplerBinding
{
    uint32_t textureBinding = 0;
    uint32_t samplerBinding = 0;
};

struct TranslatedShaderInfo
{
    std::array<SamplerBinding, kMaxGLSamplerBindings> actualSamplerBindings;
    // NOTE(hqle): UBO, XFB bindings.
};

void GlslangGetShaderSource(const gl::ProgramState &programState,
                            const gl::ProgramLinkedResources &resources,
                            gl::ShaderMap<std::string> *shaderSourcesOut,
                            ShaderMapInterfaceVariableInfoMap *variableInfoMapOut);

angle::Result GlslangGetShaderSpirvCode(ErrorHandler *context,
                                        const gl::ShaderBitSet &linkedShaderStages,
                                        const gl::Caps &glCaps,
                                        const gl::ShaderMap<std::string> &shaderSources,
                                        const ShaderMapInterfaceVariableInfoMap &variableInfoMap,
                                        gl::ShaderMap<std::vector<uint32_t>> *shaderCodeOut);

// Translate from SPIR-V code to Metal shader source code.
angle::Result SpirvCodeToMsl(Context *context,
                             const gl::ProgramState &programState,
                             gl::ShaderMap<std::vector<uint32_t>> *sprivShaderCode,
                             gl::ShaderMap<TranslatedShaderInfo> *mslShaderInfoOut,
                             gl::ShaderMap<std::string> *mslCodeOut);

}  // namespace mtl
}  // namespace rx

#endif /* LIBANGLE_RENDERER_METAL_GLSLANGWRAPPER_H_ */
