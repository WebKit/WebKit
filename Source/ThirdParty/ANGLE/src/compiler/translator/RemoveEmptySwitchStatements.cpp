//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RemoveEmptySwitchStatements.cpp: Remove switch statements that have an empty statement list.

#include "compiler/translator/RemoveEmptySwitchStatements.h"

#include "compiler/translator/IntermTraverse.h"

namespace sh
{

namespace
{

class RemoveEmptySwitchStatementsTraverser : public TIntermTraverser
{
  public:
    RemoveEmptySwitchStatementsTraverser() : TIntermTraverser(true, false, false) {}

    bool visitSwitch(Visit visit, TIntermSwitch *node);
};

bool RemoveEmptySwitchStatementsTraverser::visitSwitch(Visit visit, TIntermSwitch *node)
{
    if (node->getStatementList()->getSequence()->empty())
    {
        // Just output the init statement.
        if (node->getInit()->hasSideEffects())
        {
            queueReplacement(node->getInit(), OriginalNode::IS_DROPPED);
        }
        else
        {
            TIntermSequence emptyReplacement;
            ASSERT(getParentNode()->getAsBlock());
            mMultiReplacements.push_back(NodeReplaceWithMultipleEntry(getParentNode()->getAsBlock(),
                                                                      node, emptyReplacement));
        }
        return false;  // Nothing inside the child nodes to traverse.
    }
    return true;
}

}  // anonymous namespace

void RemoveEmptySwitchStatements(TIntermBlock *root)
{
    RemoveEmptySwitchStatementsTraverser traverser;
    root->traverse(&traverser);
    traverser.updateTree();
}

}  // namespace sh
