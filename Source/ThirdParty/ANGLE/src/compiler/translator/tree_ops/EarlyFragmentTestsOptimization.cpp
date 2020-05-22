//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CheckEarlyFragmentTestOptimization is an AST traverser to check if early fragment
// test as an optimization is feasible.
//

#include "compiler/translator/tree_ops/EarlyFragmentTestsOptimization.h"

#include "compiler/translator/InfoSink.h"
#include "compiler/translator/Symbol.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"

namespace sh
{

namespace
{

// Traverser that check conditions that would prevent early fragment tests optmization.
class CheckEFTOptimizationTraverser : public TIntermTraverser
{
  public:
    CheckEFTOptimizationTraverser();

    void visitSymbol(TIntermSymbol *node) override;
    bool visitBranch(Visit visit, TIntermBranch *node) override;

    bool isFragDepthUsed() { return mFragDepthUsed; }
    bool isDiscardOpUsed() { return mDiscardOpUsed; }

  protected:
    bool mFragDepthUsed;
    bool mDiscardOpUsed;
};

CheckEFTOptimizationTraverser::CheckEFTOptimizationTraverser()
    : TIntermTraverser(true, false, false), mFragDepthUsed(false), mDiscardOpUsed(false)
{}

void CheckEFTOptimizationTraverser::visitSymbol(TIntermSymbol *node)
{
    // Check the qualifier from the variable, not from the symbol node. The node may have a
    // different qualifier if it's the result of a folded ternary node.
    TQualifier qualifier = node->variable().getType().getQualifier();

    if (qualifier == EvqFragDepth || qualifier == EvqFragDepthEXT)
    {
        mFragDepthUsed = true;
    }
}

bool CheckEFTOptimizationTraverser::visitBranch(Visit visit, TIntermBranch *node)
{
    if (node->getFlowOp() == EOpKill)
    {
        mDiscardOpUsed = true;
    }
    return true;
}

}  // namespace

bool CheckEarlyFragmentTestsFeasible(TCompiler *compiler, TIntermNode *root)
{
    CheckEFTOptimizationTraverser traverser;
    root->traverse(&traverser);

    if (traverser.isFragDepthUsed() || traverser.isDiscardOpUsed())
    {
        return false;
    }
    return true;
}

}  // namespace sh
