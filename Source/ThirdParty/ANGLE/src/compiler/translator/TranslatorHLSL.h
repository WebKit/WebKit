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
#ifdef ANGLE_ENABLE_HLSL
    TranslatorHLSL *getAsTranslatorHLSL() override { return this; }
#endif

    bool hasShaderStorageBlock(const std::string &interfaceBlockName) const;
    unsigned int getShaderStorageBlockRegister(const std::string &interfaceBlockName) const;

    bool hasUniformBlock(const std::string &interfaceBlockName) const;
    unsigned int getUniformBlockRegister(const std::string &interfaceBlockName) const;

    const std::map<std::string, unsigned int> *getUniformRegisterMap() const;
    unsigned int getReadonlyImage2DRegisterIndex() const;
    unsigned int getImage2DRegisterIndex() const;
    const std::set<std::string> *getUsedImage2DFunctionNames() const;

  protected:
    void translate(TIntermBlock *root,
                   ShCompileOptions compileOptions,
                   PerformanceDiagnostics *perfDiagnostics) override;
    bool shouldFlattenPragmaStdglInvariantAll() override;

    // collectVariables needs to be run always so registers can be assigned.
    bool shouldCollectVariables(ShCompileOptions compileOptions) override { return true; }

    std::map<std::string, unsigned int> mShaderStorageBlockRegisterMap;
    std::map<std::string, unsigned int> mUniformBlockRegisterMap;
    std::map<std::string, unsigned int> mUniformRegisterMap;
    unsigned int mReadonlyImage2DRegisterIndex;
    unsigned int mImage2DRegisterIndex;
    std::set<std::string> mUsedImage2DFunctionNames;
};

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TRANSLATORHLSL_H_
