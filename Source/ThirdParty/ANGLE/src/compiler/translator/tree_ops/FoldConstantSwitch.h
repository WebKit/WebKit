//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FoldConstantSwitch.h: Run after FoldExpressions, folds switch statements with constant
// expression.

#ifndef COMPILER_TRANSLATOR_TREEOPS_FOLDCONSTANTSWITCH_H_
#define COMPILER_TRANSLATOR_TREEOPS_FOLDCONSTANTSWITCH_H_

#include "common/angleutils.h"

namespace sh
{

class TCompiler;
class TIntermBlock;
class TSymbolTable;

[[nodiscard]] bool FoldConstantSwitch(TCompiler *compiler,
                                      TIntermBlock *root,
                                      TSymbolTable *symbolTable);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_FOLDCONSTANTSWITCH_H_
