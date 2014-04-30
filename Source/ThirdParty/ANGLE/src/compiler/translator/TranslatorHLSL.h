//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATORHLSL_H_
#define COMPILER_TRANSLATORHLSL_H_

#include "compiler/translator/ShHandle.h"
#include "common/shadervars.h"

class TranslatorHLSL : public TCompiler {
public:
    TranslatorHLSL(ShShaderType type, ShShaderSpec spec, ShShaderOutput output);

    virtual TranslatorHLSL *getAsTranslatorHLSL() { return this; }
    const std::vector<gl::Uniform> &getUniforms() { return mActiveUniforms; }
    const std::vector<gl::InterfaceBlock> &getInterfaceBlocks() const { return mActiveInterfaceBlocks; }
    const std::vector<gl::Attribute> &getOutputVariables() { return mActiveOutputVariables; }
    const std::vector<gl::Attribute> &getAttributes() { return mActiveAttributes; }
    const std::vector<gl::Varying> &getVaryings() { return mActiveVaryings; }

protected:
    virtual void translate(TIntermNode* root);

    std::vector<gl::Uniform> mActiveUniforms;
    std::vector<gl::InterfaceBlock> mActiveInterfaceBlocks;
    std::vector<gl::Attribute> mActiveOutputVariables;
    std::vector<gl::Attribute> mActiveAttributes;
    std::vector<gl::Varying> mActiveVaryings;
    ShShaderOutput mOutputType;
};

#endif  // COMPILER_TRANSLATORHLSL_H_
