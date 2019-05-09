//
// Copyright (c) 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RewriteRepeatedAssignToSwizzled.cpp: Rewrite expressions that assign an assignment to a swizzled
// vector, like:
//     v.x = z = expression;
// to:
//     z = expression;
//     v.x = z;
//
// Note that this doesn't handle some corner cases: expressions nested inside other expressions,
// inside loop headers, or inside if conditions.

#include "compiler/translator/tree_ops/RewriteRepeatedAssignToSwizzled.h"

#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"

namespace sh
{

namespace
{

class RewriteAssignToSwizzledTraverser : public TIntermTraverser
{
  public:
    static void rewrite(TIntermBlock *root);

  private:
    RewriteAssignToSwizzledTraverser();

    bool visitBinary(Visit, TIntermBinary *node) override;

    void nextIteration();

    bool didRewrite() { return mDidRewrite; }

    bool mDidRewrite;
};

// static
void RewriteAssignToSwizzledTraverser::rewrite(TIntermBlock *root)
{
    RewriteAssignToSwizzledTraverser rewrite;
    do
    {
        rewrite.nextIteration();
        root->traverse(&rewrite);
        rewrite.updateTree();
    } while (rewrite.didRewrite());
}

RewriteAssignToSwizzledTraverser::RewriteAssignToSwizzledTraverser()
    : TIntermTraverser(true, false, false), mDidRewrite(false)
{}

void RewriteAssignToSwizzledTraverser::nextIteration()
{
    mDidRewrite = false;
}

bool RewriteAssignToSwizzledTraverser::visitBinary(Visit, TIntermBinary *node)
{
    TIntermBinary *rightBinary = node->getRight()->getAsBinaryNode();
    TIntermBlock *parentBlock  = getParentNode()->getAsBlock();
    if (parentBlock && node->isAssignment() && node->getLeft()->getAsSwizzleNode() && rightBinary &&
        rightBinary->isAssignment())
    {
        TIntermSequence replacements;
        replacements.push_back(rightBinary);
        TIntermTyped *rightAssignmentTargetCopy = rightBinary->getLeft()->deepCopy();
        TIntermBinary *lastAssign =
            new TIntermBinary(EOpAssign, node->getLeft(), rightAssignmentTargetCopy);
        replacements.push_back(lastAssign);
        mMultiReplacements.push_back(NodeReplaceWithMultipleEntry(parentBlock, node, replacements));
        mDidRewrite = true;
        return false;
    }
    return true;
}

}  // anonymous namespace

void RewriteRepeatedAssignToSwizzled(TIntermBlock *root)
{
    RewriteAssignToSwizzledTraverser::rewrite(root);
}

}  // namespace sh
