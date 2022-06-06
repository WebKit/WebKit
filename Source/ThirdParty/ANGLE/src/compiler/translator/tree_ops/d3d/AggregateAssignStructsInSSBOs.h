//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_TREEOPS_D3D_AGGREGATEASSIGNSTRUCTSINSSBOS_H_
#define COMPILER_TRANSLATOR_TREEOPS_D3D_AGGREGATEASSIGNSTRUCTSINSSBOS_H_

#include "common/angleutils.h"

namespace sh
{
class TCompiler;
class TIntermBlock;
class TSymbolTable;

ANGLE_NO_DISCARD bool AggregateAssignStructsInSSBOs(TCompiler *compiler,
                                                    TIntermBlock *root,
                                                    TSymbolTable *symbolTable);
}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_D3D_AGGREGATEASSIGNSTRUCTSINSSBOS_H_
