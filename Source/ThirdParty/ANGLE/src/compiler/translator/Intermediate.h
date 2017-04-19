//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_INTERMEDIATE_H_
#define COMPILER_TRANSLATOR_INTERMEDIATE_H_

#include "compiler/translator/IntermNode.h"

namespace sh
{

struct TVectorFields
{
    int offsets[4];
    int num;
};

//
// Set of helper functions to help build the tree.
//
class TIntermediate
{
  public:
    POOL_ALLOCATOR_NEW_DELETE();
    TIntermediate() {}

    TIntermSymbol *addSymbol(int id, const TString &, const TType &, const TSourceLoc &);
    TIntermTyped *addIndex(TOperator op,
                           TIntermTyped *base,
                           TIntermTyped *index,
                           const TSourceLoc &line,
                           TDiagnostics *diagnostics);
    static TIntermBlock *EnsureBlock(TIntermNode *node);
    TIntermNode *addIfElse(TIntermTyped *cond, TIntermNodePair code, const TSourceLoc &line);
    static TIntermTyped *AddTernarySelection(TIntermTyped *cond,
                                             TIntermTyped *trueExpression,
                                             TIntermTyped *falseExpression,
                                             const TSourceLoc &line);
    TIntermSwitch *addSwitch(TIntermTyped *init,
                             TIntermBlock *statementList,
                             const TSourceLoc &line);
    TIntermCase *addCase(TIntermTyped *condition, const TSourceLoc &line);
    static TIntermTyped *AddComma(TIntermTyped *left,
                                  TIntermTyped *right,
                                  const TSourceLoc &line,
                                  int shaderVersion);
    TIntermConstantUnion *addConstantUnion(const TConstantUnion *constantUnion,
                                           const TType &type,
                                           const TSourceLoc &line);
    TIntermNode *addLoop(TLoopType,
                         TIntermNode *,
                         TIntermTyped *,
                         TIntermTyped *,
                         TIntermNode *,
                         const TSourceLoc &);
    TIntermBranch *addBranch(TOperator, const TSourceLoc &);
    TIntermBranch *addBranch(TOperator, TIntermTyped *, const TSourceLoc &);
    static TIntermTyped *AddSwizzle(TIntermTyped *baseExpression,
                                    const TVectorFields &fields,
                                    const TSourceLoc &dotLocation);

    static void outputTree(TIntermNode *, TInfoSinkBase &);

    TIntermTyped *foldAggregateBuiltIn(TIntermAggregate *aggregate, TDiagnostics *diagnostics);

  private:
    void operator=(TIntermediate &);  // prevent assignments
};

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_INTERMEDIATE_H_
