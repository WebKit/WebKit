//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
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
#include "libANGLE/renderer/metal/mtl_glslang_mtl_utils.h"
namespace rx
{
namespace mtl
{
// - shaderSourcesOut is result GLSL code per shader stage when XFB emulation is turned off.
// - xfbOnlyShaderSourceOut will contain vertex shader's GLSL code when XFB emulation is turned on.
void GlslangGetShaderSource(const gl::ProgramState &programState,
                            const gl::ProgramLinkedResources &resources,
                            gl::ShaderMap<std::string> *shaderSourcesOut,
                            std::string *xfbOnlyShaderSourceOut,
                            ShaderMapInterfaceVariableInfoMap *variableInfoMapOut,
                            ShaderInterfaceVariableInfoMap *xfbOnlyVSVariableInfoMapOut);

angle::Result GlslangGetShaderSpirvCode(ErrorHandler *context,
                                        const gl::ShaderBitSet &linkedShaderStages,
                                        const gl::Caps &glCaps,
                                        const gl::ShaderMap<std::string> &shaderSources,
                                        const ShaderMapInterfaceVariableInfoMap &variableInfoMap,
                                        gl::ShaderMap<std::vector<uint32_t>> *shaderCodeOut);

angle::Result MSLGetShaderSpirvCode(ErrorHandler *context,
                                    const gl::ShaderBitSet &linkedShaderStages,
                                    const gl::Caps &glCaps,
                                    const gl::ShaderMap<std::string> &shaderSources,
                                    const ShaderMapInterfaceVariableInfoMap &variableInfoMap,
                                    gl::ShaderMap<std::vector<uint32_t>> *shaderCodeOut);

// Translate from SPIR-V code to Metal shader source code.
// - spirvShaderCode is SPIRV code per shader stage when XFB emulation is turned off.
// - xfbOnlySpirvCode is  vertex shader's SPIRV code when XFB emulation is turned on.
// - mslShaderInfoOut is result MSL info per shader stage when XFB emulation is turned off.
// - mslXfbOnlyShaderInfoOut is result vertex shader's MSL info when XFB emulation is turned on.
angle::Result SpirvCodeToMsl(Context *context,
                             const gl::ProgramState &programState,
                             const ShaderInterfaceVariableInfoMap &xfbVSVariableInfoMap,
                             gl::ShaderMap<std::vector<uint32_t>> *spirvShaderCode,
                             std::vector<uint32_t> *xfbOnlySpirvCode /** nullable */,
                             gl::ShaderMap<TranslatedShaderInfo> *mslShaderInfoOut,
                             TranslatedShaderInfo *mslXfbOnlyShaderInfoOut /** nullable */);

void MSLGetShaderSource(const gl::ProgramState &programState,
                        const gl::ProgramLinkedResources &resources,
                        gl::ShaderMap<std::string> *shaderSourcesOut,
                        ShaderMapInterfaceVariableInfoMap *variableInfoMapOut);

angle::Result GlslangGetMSL(Context *context,
                            const gl::ShaderBitSet &linkedShaderStages,
                            const gl::Caps &glCaps,
                            const gl::ShaderMap<std::string> &shaderSources,
                            const ShaderMapInterfaceVariableInfoMap &variableInfoMap,
                            gl::ShaderMap<TranslatedShaderInfo> *mslShaderInfoOut,
                            gl::ShaderMap<std::string> *mslCodeOut,
                            size_t xfbBufferCount);

// Get equivalent shadow compare mode that is used in translated msl shader.
uint MslGetShaderShadowCompareMode(GLenum mode, GLenum func);

}  // namespace mtl
}  // namespace rx

#endif /* LIBANGLE_RENDERER_METAL_GLSLANGWRAPPER_H_ */
