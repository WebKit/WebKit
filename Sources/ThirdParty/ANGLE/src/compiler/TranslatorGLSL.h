//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATORGLSL_H_
#define COMPILER_TRANSLATORGLSL_H_

#include "compiler/ShHandle.h"

class TranslatorGLSL : public TCompiler {
public:
    TranslatorGLSL(EShLanguage lang, EShSpec spec);
    virtual bool compile(TIntermNode* root);
};

#endif  // COMPILER_TRANSLATORGLSL_H_
