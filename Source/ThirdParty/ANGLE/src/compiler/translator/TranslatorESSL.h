//
// Copyright (c) 2002-2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_TRANSLATORESSL_H_
#define COMPILER_TRANSLATOR_TRANSLATORESSL_H_

#include "compiler/translator/Compiler.h"

namespace sh
{

class TranslatorESSL : public TCompiler
{
  public:
    TranslatorESSL(sh::GLenum type, ShShaderSpec spec);

  protected:
    void initBuiltInFunctionEmulator(BuiltInFunctionEmulator *emu,
                                     ShCompileOptions compileOptions) override;

    void translate(TIntermBlock *root,
                   ShCompileOptions compileOptions,
                   PerformanceDiagnostics *perfDiagnostics) override;
    bool shouldFlattenPragmaStdglInvariantAll() override;

  private:
    void writeExtensionBehavior(ShCompileOptions compileOptions);
};

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TRANSLATORESSL_H_
