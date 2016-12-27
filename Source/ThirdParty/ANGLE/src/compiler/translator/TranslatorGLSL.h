//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_TRANSLATORGLSL_H_
#define COMPILER_TRANSLATOR_TRANSLATORGLSL_H_

#include "compiler/translator/Compiler.h"

class TranslatorGLSL : public TCompiler
{
  public:
    TranslatorGLSL(sh::GLenum type, ShShaderSpec spec, ShShaderOutput output);

  protected:
    void initBuiltInFunctionEmulator(BuiltInFunctionEmulator *emu,
                                     ShCompileOptions compileOptions) override;

    void translate(TIntermNode *root, ShCompileOptions compileOptions) override;
    bool shouldFlattenPragmaStdglInvariantAll() override;

  private:
    void writeVersion(TIntermNode *root);
    void writeExtensionBehavior(TIntermNode *root);
    void conditionallyOutputInvariantDeclaration(const char *builtinVaryingName);
};

#endif  // COMPILER_TRANSLATOR_TRANSLATORGLSL_H_
