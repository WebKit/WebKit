//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATORHLSL_H_
#define COMPILER_TRANSLATORHLSL_H_

#include "compiler/ShHandle.h"

class TranslatorHLSL : public TCompiler {
public:
    TranslatorHLSL(ShShaderType type, ShShaderSpec spec);

protected:
    virtual void translate(TIntermNode* root);
};

#endif  // COMPILER_TRANSLATORHLSL_H_
