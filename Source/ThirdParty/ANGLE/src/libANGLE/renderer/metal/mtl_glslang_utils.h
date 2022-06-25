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

// spirvBlobsOut is the SPIR-V code per shader stage.
void GlslangGetShaderSpirvCode(const gl::ProgramState &programState,
                               const gl::ProgramLinkedResources &resources,
                               gl::ShaderMap<const angle::spirv::Blob *> *spirvBlobsOut,
                               ShaderInterfaceVariableInfoMap *variableInfoMapOut,
                               ShaderInterfaceVariableInfoMap *xfbOnlyVSVariableInfoMapOut);

angle::Result GlslangTransformSpirvCode(const gl::ShaderBitSet &linkedShaderStages,
                                        const gl::ShaderMap<const angle::spirv::Blob *> &spirvBlobs,
                                        bool isTransformFeedbackEnabled,
                                        const ShaderInterfaceVariableInfoMap &variableInfoMap,
                                        gl::ShaderMap<angle::spirv::Blob> *shaderCodeOut);

// Translate from SPIR-V code to Metal shader source code.
// - spirvShaderCode is SPIRV code per shader stage when XFB emulation is turned off.
// - xfbOnlySpirvCode is  vertex shader's SPIRV code when XFB emulation is turned on.
// - mslShaderInfoOut is result MSL info per shader stage when XFB emulation is turned off.
// - mslXfbOnlyShaderInfoOut is result vertex shader's MSL info when XFB emulation is turned on.
angle::Result SpirvCodeToMsl(Context *context,
                             const gl::ProgramState &programState,
                             const ShaderInterfaceVariableInfoMap &xfbVSVariableInfoMap,
                             gl::ShaderMap<angle::spirv::Blob> *spirvShaderCode,
                             angle::spirv::Blob *xfbOnlySpirvCode /** nullable */,
                             gl::ShaderMap<TranslatedShaderInfo> *mslShaderInfoOut,
                             TranslatedShaderInfo *mslXfbOnlyShaderInfoOut /** nullable */);

}  // namespace mtl
}  // namespace rx

#endif /* LIBANGLE_RENDERER_METAL_GLSLANGWRAPPER_H_ */
