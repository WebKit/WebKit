//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RemoveNoOpCasesFromEndOfSwitchStatements.cpp: Clean up cases from the end of a switch statement
// that only contain no-ops.

#include "compiler/translator/RemoveNoOpCasesFromEndOfSwitchStatements.h"

#include "compiler/translator/IntermNode.h"
#include "compiler/translator/IntermNode_util.h"
#include "compiler/translator/IntermTraverse.h"
#include "compiler/translator/SymbolTable.h"

namespace sh
{

namespace
{

bool AreEmptyBlocks(TIntermSequence *statements, size_t i);

bool IsEmptyBlock(TIntermNode *node)
{
    TIntermBlock *asBlock = node->getAsBlock();
    if (asBlock)
    {
        if (asBlock->getSequence()->empty())
        {
            return true;
        }
        return AreEmptyBlocks(asBlock->getSequence(), 0u);
    }
    // Empty declarations should have already been pruned, otherwise they would need to be handled
    // here. Note that declarations for struct types do contain a nameless child node.
    ASSERT(node->getAsDeclarationNode() == nullptr ||
           !node->getAsDeclarationNode()->getSequence()->empty());
    // Pure literal statements should also already be pruned.
    ASSERT(node->getAsConstantUnion() == nullptr);
    return false;
}

// Return true if all statements in "statements" starting from index i consist only of empty blocks
// and no-op statements. Returns true also if there are no statements.
bool AreEmptyBlocks(TIntermSequence *statements, size_t i)
{
    for (; i < statements->size(); ++i)
    {
        if (!IsEmptyBlock(statements->at(i)))
        {
            return false;
        }
    }
    return true;
}

void RemoveNoOpCasesFromEndOfStatementList(TIntermBlock *statementList, TSymbolTable *symbolTable)
{
    TIntermSequence *statements = statementList->getSequence();

    bool foundDeadCase = false;
    do
    {
        if (statements->empty())
        {
            return;
        }

        // Find the last case label.
        size_t i = statements->size();
        while (i > 0u && !(*statements)[i - 1]->getAsCaseNode())
        {
            --i;
        }
        // Now i is the index of the first statement following the last label inside the switch
        // statement.
        ASSERT(i > 0u);

        foundDeadCase = AreEmptyBlocks(statements, i);
        if (foundDeadCase)
        {
            statements->erase(statements->begin() + (i - 1u), statements->end());
        }
    } while (foundDeadCase);
}

class RemoveNoOpCasesFromEndOfSwitchTraverser : public TIntermTraverser
{
  public:
    RemoveNoOpCasesFromEndOfSwitchTraverser(TSymbolTable *symbolTable)
        : TIntermTraverser(true, false, false, symbolTable)
    {
    }

    bool visitSwitch(Visit visit, TIntermSwitch *node) override;
};

bool RemoveNoOpCasesFromEndOfSwitchTraverser::visitSwitch(Visit visit, TIntermSwitch *node)
{
    // Here we may mutate the statement list, but it's safe since traversal has not yet reached
    // there.
    RemoveNoOpCasesFromEndOfStatementList(node->getStatementList(), mSymbolTable);
    // Handle also nested switch statements.
    return true;
}

}  // anonymous namespace

void RemoveNoOpCasesFromEndOfSwitchStatements(TIntermBlock *root, TSymbolTable *symbolTable)
{
    RemoveNoOpCasesFromEndOfSwitchTraverser traverser(symbolTable);
    root->traverse(&traverser);
}

}  // namespace sh
