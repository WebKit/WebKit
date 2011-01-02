//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/TranslatorGLSL.h"

#include "compiler/OutputGLSL.h"

TranslatorGLSL::TranslatorGLSL(EShLanguage lang, EShSpec spec)
    : TCompiler(lang, spec) {
}

bool TranslatorGLSL::compile(TIntermNode* root) {
    TOutputGLSL outputGLSL(infoSink.obj);
    root->traverse(&outputGLSL);

    return true;
}
