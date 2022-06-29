//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TranslatorMetal:
//   A GLSL-based translator that outputs shaders that fit GL_KHR_vulkan_glsl.
//   It takes into account some considerations for Metal backend also.
//   The shaders are then fed into glslang to spit out SPIR-V.
//   See: https://www.khronos.org/registry/vulkan/specs/misc/GL_KHR_vulkan_glsl.txt
//
//   The SPIR-V will then be translated to Metal Shading Language later in Metal backend.
//

#ifndef LIBANGLE_RENDERER_METAL_TRANSLATORMETAL_H_
#define LIBANGLE_RENDERER_METAL_TRANSLATORMETAL_H_

#include "compiler/translator/DriverUniformMetal.h"
#include "compiler/translator/TranslatorVulkan.h"
#include "compiler/translator/tree_util/DriverUniform.h"
#include "compiler/translator/tree_util/SpecializationConstant.h"

namespace sh
{

// TODO: http://anglebug.com/5339 Implement it using actual specialization constant. For now we are
// redirecting to driver uniforms
class SpecConstMetal : public SpecConst
{
  public:
    SpecConstMetal(TSymbolTable *symbolTable, ShCompileOptions compileOptions, GLenum shaderType)
        : SpecConst(symbolTable, compileOptions, shaderType)
    {}
    ~SpecConstMetal() override {}

  private:
};

class TranslatorMetal : public TranslatorVulkan
{
  public:
    TranslatorMetal(sh::GLenum type, ShShaderSpec spec);

  protected:
    [[nodiscard]] bool translate(TIntermBlock *root,
                                 ShCompileOptions compileOptions,
                                 PerformanceDiagnostics *perfDiagnostics) override;

    [[nodiscard]] bool transformDepthBeforeCorrection(TIntermBlock *root,
                                                      const DriverUniform *driverUniforms) override;

    [[nodiscard]] bool insertSampleMaskWritingLogic(TInfoSinkBase &sink,
                                                    TIntermBlock *root,
                                                    const DriverUniformMetal *driverUniforms);
    [[nodiscard]] bool insertRasterizerDiscardLogic(TInfoSinkBase &sink, TIntermBlock *root);
};

}  // namespace sh

#endif /* LIBANGLE_RENDERER_METAL_TRANSLATORMETAL_H_ */
