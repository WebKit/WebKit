//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_TRANSLATORHLSL_H_
#define COMPILER_TRANSLATOR_TRANSLATORHLSL_H_

#include "compiler/translator/Compiler.h"

namespace sh
{

class TranslatorHLSL : public TCompiler
{
  public:
    TranslatorHLSL(sh::GLenum type, ShShaderSpec spec, ShShaderOutput output);
    TranslatorHLSL *getAsTranslatorHLSL() { return this; }

    bool hasInterfaceBlock(const std::string &interfaceBlockName) const;
    unsigned int getInterfaceBlockRegister(const std::string &interfaceBlockName) const;

    const std::map<std::string, unsigned int> *getUniformRegisterMap() const;

  protected:
    void translate(TIntermNode *root, ShCompileOptions compileOptions) override;
    bool shouldFlattenPragmaStdglInvariantAll() override;

    // collectVariables needs to be run always so registers can be assigned.
    bool shouldCollectVariables(ShCompileOptions compileOptions) override { return true; }

    std::map<std::string, unsigned int> mInterfaceBlockRegisterMap;
    std::map<std::string, unsigned int> mUniformRegisterMap;
};

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TRANSLATORHLSL_H_
