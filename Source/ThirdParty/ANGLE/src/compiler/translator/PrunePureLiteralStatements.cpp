//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// PrunePureLiteralStatements.cpp: Removes statements that are literals and nothing else.

#include "compiler/translator/PrunePureLiteralStatements.h"

#include "compiler/translator/IntermNode.h"

namespace sh
{

namespace
{

class PrunePureLiteralStatementsTraverser : public TIntermTraverser
{
  public:
    PrunePureLiteralStatementsTraverser() : TIntermTraverser(true, false, false) {}

    bool visitBlock(Visit visit, TIntermBlock *node) override
    {
        TIntermSequence *statements = node->getSequence();
        if (statements == nullptr)
        {
            return false;
        }

        // Empty case statements at the end of a switch are invalid: if the last statements
        // of a block was a pure literal, also delete all the case statements directly preceding it.
        bool deleteCaseStatements = false;
        for (int i = static_cast<int>(statements->size()); i-- > 0;)
        {
            TIntermNode *statement = (*statements)[i];

            if (statement->getAsConstantUnion() != nullptr)
            {
                TIntermSequence emptyReplacement;
                mMultiReplacements.push_back(
                    NodeReplaceWithMultipleEntry(node, statement, emptyReplacement));

                if (i == static_cast<int>(statements->size()) - 1)
                {
                    deleteCaseStatements = true;
                }

                continue;
            }

            if (deleteCaseStatements)
            {
                if (statement->getAsCaseNode() != nullptr)
                {
                    TIntermSequence emptyReplacement;
                    mMultiReplacements.push_back(
                        NodeReplaceWithMultipleEntry(node, statement, emptyReplacement));
                }
                else
                {
                    deleteCaseStatements = false;
                }
            }
        }

        return true;
    }

    bool visitLoop(Visit visit, TIntermLoop *loop) override
    {
        TIntermTyped *expr = loop->getExpression();
        if (expr != nullptr && expr->getAsConstantUnion() != nullptr)
        {
            loop->setExpression(nullptr);
        }

        return true;
    }
};

}  // namespace

void PrunePureLiteralStatements(TIntermNode *root)
{
    PrunePureLiteralStatementsTraverser prune;
    root->traverse(&prune);
    prune.updateTree();
}

}  // namespace sh
