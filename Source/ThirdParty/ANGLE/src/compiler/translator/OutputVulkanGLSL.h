//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// OutputVulkanGLSL:
//   Code that outputs shaders that fit GL_KHR_vulkan_glsl.
//   The shaders are then fed into glslang to spit out SPIR-V (libANGLE-side).
//   See: https://www.khronos.org/registry/vulkan/specs/misc/GL_KHR_vulkan_glsl.txt
//

#include "compiler/translator/OutputGLSL.h"

namespace sh
{

class TOutputVulkanGLSL : public TOutputGLSLBase
{
  public:
    TOutputVulkanGLSL(TInfoSinkBase &objSink,
                      ShArrayIndexClampingStrategy clampingStrategy,
                      ShHashFunction64 hashFunction,
                      NameMap &nameMap,
                      TSymbolTable &symbolTable,
                      sh::GLenum shaderType,
                      int shaderVersion,
                      ShShaderOutput output,
                      ShCompileOptions compileOptions);

  protected:
    void writeLayoutQualifier(const TType &type) override;
    bool writeVariablePrecision(TPrecision precision) override;
    void visitSymbol(TIntermSymbol *node) override;
};

}  // namespace sh
