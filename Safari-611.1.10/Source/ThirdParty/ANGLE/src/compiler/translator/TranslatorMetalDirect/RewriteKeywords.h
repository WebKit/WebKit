//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_TRANSLATORMETALDIRECT_REWRITEKEYWORDS_H_
#define COMPILER_TRANSLATOR_TRANSLATORMETALDIRECT_REWRITEKEYWORDS_H_

#include <set>

#include "common/angleutils.h"
#include "compiler/translator/Compiler.h"
#include "compiler/translator/TranslatorMetalDirect/IdGen.h"

namespace sh
{

// This walks the tree and renames all names that conflict with the input `keywords`.
ANGLE_NO_DISCARD bool RewriteKeywords(TCompiler &compiler,
                                      TIntermBlock &root,
                                      IdGen &idGen,
                                      const std::set<ImmutableString> &keywords);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TRANSLATORMETALDIRECT_REWRITEKEYWORDS_H_
