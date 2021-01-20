//
// Copyright (c) 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TOutputVulkanGLSLForMetal:
//    This is a special version Vulkan GLSL output that will make some special
//    considerations for Metal backend limitations.
//

#include "compiler/translator/OutputVulkanGLSL.h"

namespace sh
{

class TOutputVulkanGLSLForMetal : public TOutputVulkanGLSL
{
  public:
    TOutputVulkanGLSLForMetal(TInfoSinkBase &objSink,
                              ShArrayIndexClampingStrategy clampingStrategy,
                              ShHashFunction64 hashFunction,
                              NameMap &nameMap,
                              TSymbolTable *symbolTable,
                              sh::GLenum shaderType,
                              int shaderVersion,
                              ShShaderOutput output,
                              ShCompileOptions compileOptions);

    static void RemoveInvariantForTest(bool remove);

  protected:
    bool visitGlobalQualifierDeclaration(Visit visit,
                                         TIntermGlobalQualifierDeclaration *node) override;
    void writeVariableType(const TType &type,
                           const TSymbol *symbol,
                           bool isFunctionArgument) override;
};

}  // namespace sh
