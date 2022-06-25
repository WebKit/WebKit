//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_OUTPUTGLSL_H_
#define COMPILER_TRANSLATOR_OUTPUTGLSL_H_

#include "compiler/translator/OutputGLSLBase.h"

namespace sh
{

class TOutputGLSL : public TOutputGLSLBase
{
  public:
    TOutputGLSL(TCompiler *compiler, TInfoSinkBase &objSink, ShCompileOptions compileOptions);

  protected:
    bool writeVariablePrecision(TPrecision) override;
    void visitSymbol(TIntermSymbol *node) override;
    ImmutableString translateTextureFunction(const ImmutableString &name,
                                             const ShCompileOptions &option) override;
};

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_OUTPUTGLSL_H_
