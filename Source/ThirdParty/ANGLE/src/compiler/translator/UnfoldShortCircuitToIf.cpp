//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// UnfoldShortCircuitToIf is an AST traverser to convert short-circuiting operators to if-else
// statements.
// The results are assigned to s# temporaries, which are used by the main translator instead of
// the original expression.
//

#include "compiler/translator/UnfoldShortCircuitToIf.h"

#include "compiler/translator/IntermNodePatternMatcher.h"
#include "compiler/translator/IntermTraverse.h"

namespace sh
{

namespace
{

// Traverser that unfolds one short-circuiting operation at a time.
class UnfoldShortCircuitTraverser : public TIntermTraverser
{
  public:
    UnfoldShortCircuitTraverser(TSymbolTable *symbolTable);

    bool visitBinary(Visit visit, TIntermBinary *node) override;
    bool visitTernary(Visit visit, TIntermTernary *node) override;

    void nextIteration();
    bool foundShortCircuit() const { return mFoundShortCircuit; }

  protected:
    // Marked to true once an operation that needs to be unfolded has been found.
    // After that, no more unfolding is performed on that traversal.
    bool mFoundShortCircuit;

    IntermNodePatternMatcher mPatternToUnfoldMatcher;
};

UnfoldShortCircuitTraverser::UnfoldShortCircuitTraverser(TSymbolTable *symbolTable)
    : TIntermTraverser(true, false, true, symbolTable),
      mFoundShortCircuit(false),
      mPatternToUnfoldMatcher(IntermNodePatternMatcher::kUnfoldedShortCircuitExpression)
{
}

bool UnfoldShortCircuitTraverser::visitBinary(Visit visit, TIntermBinary *node)
{
    if (mFoundShortCircuit)
        return false;

    if (visit != PreVisit)
        return true;

    if (!mPatternToUnfoldMatcher.match(node, getParentNode()))
        return true;

    // If our right node doesn't have side effects, we know we don't need to unfold this
    // expression: there will be no short-circuiting side effects to avoid
    // (note: unfolding doesn't depend on the left node -- it will always be evaluated)
    ASSERT(node->getRight()->hasSideEffects());

    mFoundShortCircuit = true;

    switch (node->getOp())
    {
        case EOpLogicalOr:
        {
            // "x || y" is equivalent to "x ? true : y", which unfolds to "bool s; if(x) s = true;
            // else s = y;",
            // and then further simplifies down to "bool s = x; if(!s) s = y;".

            TIntermSequence insertions;
            TType boolType(EbtBool, EbpUndefined, EvqTemporary);

            ASSERT(node->getLeft()->getType() == boolType);
            insertions.push_back(createTempInitDeclaration(node->getLeft()));

            TIntermBlock *assignRightBlock = new TIntermBlock();
            ASSERT(node->getRight()->getType() == boolType);
            assignRightBlock->getSequence()->push_back(createTempAssignment(node->getRight()));

            TIntermUnary *notTempSymbol =
                new TIntermUnary(EOpLogicalNot, createTempSymbol(boolType));
            TIntermIfElse *ifNode = new TIntermIfElse(notTempSymbol, assignRightBlock, nullptr);
            insertions.push_back(ifNode);

            insertStatementsInParentBlock(insertions);

            queueReplacement(createTempSymbol(boolType), OriginalNode::IS_DROPPED);
            return false;
        }
        case EOpLogicalAnd:
        {
            // "x && y" is equivalent to "x ? y : false", which unfolds to "bool s; if(x) s = y;
            // else s = false;",
            // and then further simplifies down to "bool s = x; if(s) s = y;".
            TIntermSequence insertions;
            TType boolType(EbtBool, EbpUndefined, EvqTemporary);

            ASSERT(node->getLeft()->getType() == boolType);
            insertions.push_back(createTempInitDeclaration(node->getLeft()));

            TIntermBlock *assignRightBlock = new TIntermBlock();
            ASSERT(node->getRight()->getType() == boolType);
            assignRightBlock->getSequence()->push_back(createTempAssignment(node->getRight()));

            TIntermIfElse *ifNode =
                new TIntermIfElse(createTempSymbol(boolType), assignRightBlock, nullptr);
            insertions.push_back(ifNode);

            insertStatementsInParentBlock(insertions);

            queueReplacement(createTempSymbol(boolType), OriginalNode::IS_DROPPED);
            return false;
        }
        default:
            UNREACHABLE();
            return true;
    }
}

bool UnfoldShortCircuitTraverser::visitTernary(Visit visit, TIntermTernary *node)
{
    if (mFoundShortCircuit)
        return false;

    if (visit != PreVisit)
        return true;

    if (!mPatternToUnfoldMatcher.match(node))
        return true;

    mFoundShortCircuit = true;

    // Unfold "b ? x : y" into "type s; if(b) s = x; else s = y;"
    TIntermSequence insertions;

    TIntermDeclaration *tempDeclaration = createTempDeclaration(node->getType());
    insertions.push_back(tempDeclaration);

    TIntermBlock *trueBlock       = new TIntermBlock();
    TIntermBinary *trueAssignment = createTempAssignment(node->getTrueExpression());
    trueBlock->getSequence()->push_back(trueAssignment);

    TIntermBlock *falseBlock       = new TIntermBlock();
    TIntermBinary *falseAssignment = createTempAssignment(node->getFalseExpression());
    falseBlock->getSequence()->push_back(falseAssignment);

    TIntermIfElse *ifNode =
        new TIntermIfElse(node->getCondition()->getAsTyped(), trueBlock, falseBlock);
    insertions.push_back(ifNode);

    insertStatementsInParentBlock(insertions);

    TIntermSymbol *ternaryResult = createTempSymbol(node->getType());
    queueReplacement(ternaryResult, OriginalNode::IS_DROPPED);

    return false;
}

void UnfoldShortCircuitTraverser::nextIteration()
{
    mFoundShortCircuit = false;
    nextTemporaryId();
}

}  // namespace

void UnfoldShortCircuitToIf(TIntermNode *root, TSymbolTable *symbolTable)
{
    UnfoldShortCircuitTraverser traverser(symbolTable);
    // Unfold one operator at a time, and reset the traverser between iterations.
    do
    {
        traverser.nextIteration();
        root->traverse(&traverser);
        if (traverser.foundShortCircuit())
            traverser.updateTree();
    } while (traverser.foundShortCircuit());
}

}  // namespace sh
