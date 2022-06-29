//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// glslang_wrapper:
//   A wrapper to compile GLSL strings to SPIR-V blobs.  glslang here refers to the Khronos
//   compiler.
//

#ifndef COMPILER_TRANSLATOR_GLSLANG_WRAPPER_H_
#define COMPILER_TRANSLATOR_GLSLANG_WRAPPER_H_

#include "GLSLANG/ShaderLang.h"
#include "common/PackedEnums.h"
#include "common/spirv/spirv_types.h"

#include <string>
#include <vector>

namespace sh
{
#if defined(ANGLE_ENABLE_SPIRV_GENERATION_THROUGH_GLSLANG)
void GlslangInitialize();
void GlslangFinalize();

// Generate SPIR-V out of intermediate GLSL through glslang.
[[nodiscard]] bool GlslangCompileToSpirv(const ShBuiltInResources &resources,
                                         sh::GLenum shaderType,
                                         const std::string &shaderSource,
                                         angle::spirv::Blob *spirvBlobOut);
#else
ANGLE_INLINE void GlslangInitialize() {}
ANGLE_INLINE void GlslangFinalize() {}
ANGLE_INLINE bool GlslangCompileToSpirv(const ShBuiltInResources &resources,
                                        sh::GLenum shaderType,
                                        const std::string &shaderSource,
                                        angle::spirv::Blob *spirvBlobOut)
{
    UNREACHABLE();
    return false;
}
#endif  // defined(ANGLE_ENABLE_VULKAN) || defined(ANGLE_ENABLE_METAL)
}  // namespace sh

#endif  // COMPILER_TRANSLATOR_GLSLANG_WRAPPER_H_
