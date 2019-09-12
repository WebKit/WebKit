//
// Copyright (c) 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ReplaceVariable.cpp: Replace all references to a specific variable in the AST with references to
// another variable.

#include "compiler/translator/tree_util/ReplaceVariable.h"

#include "compiler/translator/IntermNode.h"
#include "compiler/translator/tree_util/IntermTraverse.h"

namespace sh
{

namespace
{

class ReplaceVariableTraverser : public TIntermTraverser
{
  public:
    ReplaceVariableTraverser(const TVariable *toBeReplaced, const TIntermTyped *replacement)
        : TIntermTraverser(true, false, false),
          mToBeReplaced(toBeReplaced),
          mReplacement(replacement)
    {}

    void visitSymbol(TIntermSymbol *node) override
    {
        if (&node->variable() == mToBeReplaced)
        {
            queueReplacement(mReplacement->deepCopy(), OriginalNode::IS_DROPPED);
        }
    }

  private:
    const TVariable *const mToBeReplaced;
    const TIntermTyped *const mReplacement;
};

}  // anonymous namespace

// Replaces every occurrence of a variable with another variable.
void ReplaceVariable(TIntermBlock *root,
                     const TVariable *toBeReplaced,
                     const TVariable *replacement)
{
    ReplaceVariableTraverser traverser(toBeReplaced, new TIntermSymbol(replacement));
    root->traverse(&traverser);
    traverser.updateTree();
}

// Replaces every occurrence of a variable with a TIntermNode.
void ReplaceVariableWithTyped(TIntermBlock *root,
                              const TVariable *toBeReplaced,
                              const TIntermTyped *replacement)
{
    ReplaceVariableTraverser traverser(toBeReplaced, replacement);
    root->traverse(&traverser);
    traverser.updateTree();
}

}  // namespace sh
