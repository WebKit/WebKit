//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TranslatorVulkan:
//   A GLSL-based translator that outputs shaders that fit GL_KHR_vulkan_glsl and feeds them into
//   glslang to spit out SPIR-V.
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

class TranslatorVulkan : public TCompiler
{
  public:
    TranslatorVulkan(sh::GLenum type, ShShaderSpec spec);

  protected:
    ANGLE_NO_DISCARD bool translate(TIntermBlock *root,
                                    ShCompileOptions compileOptions,
                                    PerformanceDiagnostics *perfDiagnostics) override;
    bool shouldFlattenPragmaStdglInvariantAll() override;

    // Subclass can call this method to transform the AST before writing the final output.
    // See TranslatorMetal.cpp.
    ANGLE_NO_DISCARD bool translateImpl(TInfoSinkBase &sink,
                                        TIntermBlock *root,
                                        ShCompileOptions compileOptions,
                                        PerformanceDiagnostics *perfDiagnostics,
                                        SpecConst *specConst,
                                        DriverUniform *driverUniforms);

    void writeExtensionBehavior(ShCompileOptions compileOptions, TInfoSinkBase &sink);

    // Give subclass such as TranslatorMetal a chance to do depth transform before
    // TranslatorVulkan apply its own transform.
    ANGLE_NO_DISCARD virtual bool transformDepthBeforeCorrection(
        TIntermBlock *root,
        const DriverUniform *driverUniforms)
    {
        return true;
    }

    // Generate SPIR-V out of intermediate GLSL through glslang.
    ANGLE_NO_DISCARD bool compileToSpirv(const TInfoSinkBase &glsl);
};

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TRANSLATORVULKAN_H_
