//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TranslatorVulkan:
//   A set of transformations that prepare the AST to be compatible with GL_KHR_vulkan_glsl followed
//   by a pass that generates SPIR-V.
//   See: https://www.khronos.org/registry/vulkan/specs/misc/GL_KHR_vulkan_glsl.txt
//

#ifndef COMPILER_TRANSLATOR_TRANSLATORVULKAN_H_
#define COMPILER_TRANSLATOR_TRANSLATORVULKAN_H_

#include "compiler/translator/Compiler.h"

namespace sh
{

class TOutputVulkanGLSL;
class SpecConst;
class DriverUniform;

class TranslatorVulkan final : public TCompiler
{
  public:
    TranslatorVulkan(sh::GLenum type, ShShaderSpec spec);

  protected:
    [[nodiscard]] bool translate(TIntermBlock *root,
                                 const ShCompileOptions &compileOptions,
                                 PerformanceDiagnostics *perfDiagnostics) override;
    bool shouldFlattenPragmaStdglInvariantAll() override;

    [[nodiscard]] bool translateImpl(TIntermBlock *root,
                                     const ShCompileOptions &compileOptions,
                                     PerformanceDiagnostics *perfDiagnostics,
                                     SpecConst *specConst,
                                     DriverUniform *driverUniforms);
};

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TRANSLATORVULKAN_H_
