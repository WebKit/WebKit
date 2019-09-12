//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DynamicImage2DHLSL.h: Interface for link and run-time HLSL generation
//

#ifndef LIBANGLE_RENDERER_D3D_DYNAMICIMAGE2DHLSL_H_
#define LIBANGLE_RENDERER_D3D_DYNAMICIMAGE2DHLSL_H_

#include "common/angleutils.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"

namespace rx
{
std::string generateShaderForImage2DBindSignature(
    const d3d::Context *context,
    ProgramD3D &programD3D,
    const gl::ProgramState &programData,
    gl::ShaderType shaderType,
    std::vector<sh::Uniform> &image2DUniforms,
    const gl::ImageUnitTextureTypeMap &image2DBindLayout);

inline std::string GenerateComputeShaderForImage2DBindSignature(
    const d3d::Context *context,
    ProgramD3D &programD3D,
    const gl::ProgramState &programData,
    std::vector<sh::Uniform> &image2DUniforms,
    const gl::ImageUnitTextureTypeMap &image2DBindLayout)
{
    return generateShaderForImage2DBindSignature(context, programD3D, programData,
                                                 gl::ShaderType::Compute, image2DUniforms,
                                                 image2DBindLayout);
}

}  // namespace rx

#endif  // LIBANGLE_RENDERER_D3D_DYNAMICHLSL_H_
