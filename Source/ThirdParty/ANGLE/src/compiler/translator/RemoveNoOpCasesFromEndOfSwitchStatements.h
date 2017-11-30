//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RemoveNoOpCasesFromEndOfSwitchStatements.h: Clean up cases from the end of a switch statement
// that only contain no-ops.

#ifndef COMPILER_TRANSLATOR_REMOVENOOPCASESFROMENDOFSWITCHSTATEMENTS_H_
#define COMPILER_TRANSLATOR_REMOVENOOPCASESFROMENDOFSWITCHSTATEMENTS_H_

namespace sh
{
class TIntermBlock;
class TSymbolTable;

void RemoveNoOpCasesFromEndOfSwitchStatements(TIntermBlock *root, TSymbolTable *symbolTable);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_REMOVENOOPCASESFROMENDOFSWITCHSTATEMENTS_H_
