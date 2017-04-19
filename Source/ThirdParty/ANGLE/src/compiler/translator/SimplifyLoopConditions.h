//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SimplifyLoopConditions is an AST traverser that converts loop conditions and loop expressions
// to regular statements inside the loop. This way further transformations that generate statements
// from loop conditions and loop expressions work correctly.
//

#ifndef COMPILER_TRANSLATOR_SIMPLIFYLOOPCONDITIONS_H_
#define COMPILER_TRANSLATOR_SIMPLIFYLOOPCONDITIONS_H_

namespace sh
{
class TIntermNode;
class TSymbolTable;

void SimplifyLoopConditions(TIntermNode *root,
                            unsigned int conditionsToSimplify,
                            unsigned int *temporaryIndex,
                            const TSymbolTable &symbolTable,
                            int shaderVersion);
}  // namespace sh

#endif  // COMPILER_TRANSLATOR_SIMPLIFYLOOPCONDITIONS_H_
