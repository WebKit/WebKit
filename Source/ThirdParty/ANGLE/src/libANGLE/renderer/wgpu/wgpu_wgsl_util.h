//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// wgpu_wgsl_util.h: Utilities to manipulate previously translated WGSL.
//

#ifndef LIBANGLE_RENDERER_WGPU_WGPU_WGSL_UTIL_H_
#define LIBANGLE_RENDERER_WGPU_WGPU_WGSL_UTIL_H_

#include "common/PackedGLEnums_autogen.h"
#include "libANGLE/Program.h"

namespace rx
{
namespace webgpu
{

// Replaces location markers in the WGSL source with actual locations, for
// `shaderVars` which is a vector of either gl::ProgramInputs or gl::ProgramOutputs, and for
// `mergedVaryings` which get assigned sequentially increasing locations. There should be at most
// vertex and fragment shader stages or this function will not assign locations correctly.
//
// Also assigns sampler bindings, which are split into two separate sampler/texture
// variables in WGSL and are assigned binding numbers as such:
// @binding(n*2) for the WGSL sampler variable corresponding to the n-th GLSL sampler
// @binding(n*2+1) for the WGSL texture variable corresponding to the n-th GLSL sampler.
template <typename T>
std::string WgslAssignLocationsAndSamplerBindings(const gl::ProgramExecutable &executable,
                                                  const std::string &shaderSource,
                                                  const std::vector<T> shaderVars,
                                                  const gl::ProgramMergedVaryings &mergedVaryings,
                                                  gl::ShaderType shaderType);

}  // namespace webgpu
}  // namespace rx

#endif /* LIBANGLE_RENDERER_WGPU_WGPU_WGSL_UTIL_H_ */
