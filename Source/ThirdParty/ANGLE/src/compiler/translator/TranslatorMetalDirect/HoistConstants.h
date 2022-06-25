//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_TRANSLATORMETALDIRECT_HOISTCONSTANTS_H_
#define COMPILER_TRANSLATOR_TRANSLATORMETALDIRECT_HOISTCONSTANTS_H_

#include "common/angleutils.h"
#include "compiler/translator/Compiler.h"
#include "compiler/translator/TranslatorMetalDirect/IdGen.h"

namespace sh
{

// Hoists function-local constants to the global scope if their Metal sizeof meets
// `minRequiredSize`.
ANGLE_NO_DISCARD bool HoistConstants(TCompiler &compiler,
                                     TIntermBlock &root,
                                     IdGen &idGen,
                                     size_t minRequiredSize);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TRANSLATORMETALDIRECT_HOISTCONSTANTS_H_
