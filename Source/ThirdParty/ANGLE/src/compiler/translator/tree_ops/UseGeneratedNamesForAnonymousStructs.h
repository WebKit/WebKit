//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// UseGeneratedNamesForAnonymousStructs.h: Force all anonymous structs to use generated names.
//

#ifndef COMPILER_TRANSLATOR_TREEOPS_USEGENERATEDNAMESFORANONYMOUSSTRUCTS_H_
#define COMPILER_TRANSLATOR_TREEOPS_USEGENERATEDNAMESFORANONYMOUSSTRUCTS_H_

#include "common/angleutils.h"

namespace sh
{
class TCompiler;
class TIntermBlock;

[[nodiscard]] bool UseGeneratedNamesForAnonymousStructs(TCompiler *compiler, TIntermBlock *root);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_USEGENERATEDNAMESFORANONYMOUSSTRUCTS_H_
