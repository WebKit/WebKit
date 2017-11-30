//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RewriteElseBlocks.cpp: Implementation for tree transform to change
//   all if-else blocks to if-if blocks.
//

#include "compiler/translator/RewriteElseBlocks.h"

#include "compiler/translator/IntermNode.h"
#include "compiler/translator/IntermNode_util.h"
#include "compiler/translator/NodeSearch.h"
#include "compiler/translator/SymbolTable.h"

namespace sh
{

namespace
{

class ElseBlockRewriter : public TIntermTraverser
{
  public:
    ElseBlockRewriter(TSymbolTable *symbolTable);

  protected:
    bool visitFunctionDefinition(Visit visit, TIntermFunctionDefinition *aggregate) override;
    bool visitBlock(Visit visit, TIntermBlock *block) override;

  private:
    TIntermNode *rewriteIfElse(TIntermIfElse *ifElse);

    const TType *mFunctionType;
};

ElseBlockRewriter::ElseBlockRewriter(TSymbolTable *symbolTable)
    : TIntermTraverser(true, false, true, symbolTable), mFunctionType(nullptr)
{
}

bool ElseBlockRewriter::visitFunctionDefinition(Visit visit, TIntermFunctionDefinition *node)
{
    // Store the current function context (see comment below)
    mFunctionType = ((visit == PreVisit) ? &node->getFunctionPrototype()->getType() : nullptr);
    return true;
}

bool ElseBlockRewriter::visitBlock(Visit visit, TIntermBlock *node)
{
    if (visit == PostVisit)
    {
        for (size_t statementIndex = 0; statementIndex != node->getSequence()->size();
             statementIndex++)
        {
            TIntermNode *statement = (*node->getSequence())[statementIndex];
            TIntermIfElse *ifElse  = statement->getAsIfElseNode();
            if (ifElse && ifElse->getFalseBlock() != nullptr)
            {
                (*node->getSequence())[statementIndex] = rewriteIfElse(ifElse);
            }
        }
    }
    return true;
}

TIntermNode *ElseBlockRewriter::rewriteIfElse(TIntermIfElse *ifElse)
{
    ASSERT(ifElse != nullptr);

    nextTemporaryId();

    TIntermDeclaration *storeCondition = createTempInitDeclaration(ifElse->getCondition());

    TIntermBlock *falseBlock = nullptr;

    TType boolType(EbtBool, EbpUndefined, EvqTemporary);

    if (ifElse->getFalseBlock())
    {
        TIntermBlock *negatedElse = nullptr;
        // crbug.com/346463
        // D3D generates error messages claiming a function has no return value, when rewriting
        // an if-else clause that returns something non-void in a function. By appending dummy
        // returns (that are unreachable) we can silence this compile error.
        if (mFunctionType && mFunctionType->getBasicType() != EbtVoid)
        {
            TIntermNode *returnNode = new TIntermBranch(EOpReturn, CreateZeroNode(*mFunctionType));
            negatedElse = new TIntermBlock();
            negatedElse->appendStatement(returnNode);
        }

        TIntermSymbol *conditionSymbolElse = createTempSymbol(boolType);
        TIntermUnary *negatedCondition     = new TIntermUnary(EOpLogicalNot, conditionSymbolElse);
        TIntermIfElse *falseIfElse =
            new TIntermIfElse(negatedCondition, ifElse->getFalseBlock(), negatedElse);
        falseBlock = EnsureBlock(falseIfElse);
    }

    TIntermSymbol *conditionSymbolSel = createTempSymbol(boolType);
    TIntermIfElse *newIfElse =
        new TIntermIfElse(conditionSymbolSel, ifElse->getTrueBlock(), falseBlock);

    TIntermBlock *block = new TIntermBlock();
    block->getSequence()->push_back(storeCondition);
    block->getSequence()->push_back(newIfElse);

    return block;
}

}  // anonymous namespace

void RewriteElseBlocks(TIntermNode *node, TSymbolTable *symbolTable)
{
    ElseBlockRewriter rewriter(symbolTable);
    node->traverse(&rewriter);
}

}  // namespace sh
