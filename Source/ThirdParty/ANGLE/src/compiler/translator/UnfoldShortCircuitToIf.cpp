//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// UnfoldShortCircuitToIf is an AST traverser to convert short-circuiting operators to if-else statements.
// The results are assigned to s# temporaries, which are used by the main translator instead of
// the original expression.
//

#include "compiler/translator/UnfoldShortCircuitToIf.h"

#include "compiler/translator/IntermNode.h"

namespace
{

// Traverser that unfolds one short-circuiting operation at a time.
class UnfoldShortCircuitTraverser : public TIntermTraverser
{
  public:
    UnfoldShortCircuitTraverser();

    bool visitBinary(Visit visit, TIntermBinary *node) override;
    bool visitAggregate(Visit visit, TIntermAggregate *node) override;
    bool visitSelection(Visit visit, TIntermSelection *node) override;
    bool visitLoop(Visit visit, TIntermLoop *node) override;

    void nextIteration();
    bool foundShortCircuit() const { return mFoundShortCircuit; }

  protected:
    // Check if the traversal is inside a loop condition or expression, in which case the unfolded
    // expression needs to be copied inside the loop. Returns true if the copying is done, in which
    // case no further unfolding should be done on the same traversal.
    // The parameters are the node that will be unfolded to multiple statements and so can't remain
    // inside a loop condition, and its parent.
    bool copyLoopConditionOrExpression(TIntermNode *parent, TIntermTyped *node);

    // Marked to true once an operation that needs to be unfolded has been found.
    // After that, no more unfolding is performed on that traversal.
    bool mFoundShortCircuit;

    // Set to the loop node while a loop condition or expression is being traversed.
    TIntermLoop *mParentLoop;
    // Parent of the loop node while a loop condition or expression is being traversed.
    TIntermNode *mLoopParent;

    bool mInLoopCondition;
    bool mInLoopExpression;
};

UnfoldShortCircuitTraverser::UnfoldShortCircuitTraverser()
    : TIntermTraverser(true, false, true),
      mFoundShortCircuit(false),
      mParentLoop(nullptr),
      mLoopParent(nullptr),
      mInLoopCondition(false),
      mInLoopExpression(false)
{
}

bool UnfoldShortCircuitTraverser::visitBinary(Visit visit, TIntermBinary *node)
{
    if (mFoundShortCircuit)
        return false;
    // If our right node doesn't have side effects, we know we don't need to unfold this
    // expression: there will be no short-circuiting side effects to avoid
    // (note: unfolding doesn't depend on the left node -- it will always be evaluated)
    if (!node->getRight()->hasSideEffects())
    {
        return true;
    }

    switch (node->getOp())
    {
      case EOpLogicalOr:
        mFoundShortCircuit = true;
        if (!copyLoopConditionOrExpression(getParentNode(), node))
        {
            // "x || y" is equivalent to "x ? true : y", which unfolds to "bool s; if(x) s = true;
            // else s = y;",
            // and then further simplifies down to "bool s = x; if(!s) s = y;".

            TIntermSequence insertions;
            TType boolType(EbtBool, EbpUndefined, EvqTemporary);

            ASSERT(node->getLeft()->getType() == boolType);
            insertions.push_back(createTempInitDeclaration(node->getLeft()));

            TIntermAggregate *assignRightBlock = new TIntermAggregate(EOpSequence);
            ASSERT(node->getRight()->getType() == boolType);
            assignRightBlock->getSequence()->push_back(createTempAssignment(node->getRight()));

            TIntermUnary *notTempSymbol = new TIntermUnary(EOpLogicalNot, boolType);
            notTempSymbol->setOperand(createTempSymbol(boolType));
            TIntermSelection *ifNode = new TIntermSelection(notTempSymbol, assignRightBlock, nullptr);
            insertions.push_back(ifNode);

            insertStatementsInParentBlock(insertions);

            NodeUpdateEntry replaceVariable(getParentNode(), node, createTempSymbol(boolType), false);
            mReplacements.push_back(replaceVariable);
        }
        return false;
      case EOpLogicalAnd:
        mFoundShortCircuit = true;
        if (!copyLoopConditionOrExpression(getParentNode(), node))
        {
            // "x && y" is equivalent to "x ? y : false", which unfolds to "bool s; if(x) s = y;
            // else s = false;",
            // and then further simplifies down to "bool s = x; if(s) s = y;".
            TIntermSequence insertions;
            TType boolType(EbtBool, EbpUndefined, EvqTemporary);

            ASSERT(node->getLeft()->getType() == boolType);
            insertions.push_back(createTempInitDeclaration(node->getLeft()));

            TIntermAggregate *assignRightBlock = new TIntermAggregate(EOpSequence);
            ASSERT(node->getRight()->getType() == boolType);
            assignRightBlock->getSequence()->push_back(createTempAssignment(node->getRight()));

            TIntermSelection *ifNode = new TIntermSelection(createTempSymbol(boolType), assignRightBlock, nullptr);
            insertions.push_back(ifNode);

            insertStatementsInParentBlock(insertions);

            NodeUpdateEntry replaceVariable(getParentNode(), node, createTempSymbol(boolType), false);
            mReplacements.push_back(replaceVariable);
        }
        return false;
      default:
        return true;
    }
}

bool UnfoldShortCircuitTraverser::visitSelection(Visit visit, TIntermSelection *node)
{
    if (mFoundShortCircuit)
        return false;

    // Unfold "b ? x : y" into "type s; if(b) s = x; else s = y;"
    if (visit == PreVisit && node->usesTernaryOperator())
    {
        mFoundShortCircuit = true;
        if (!copyLoopConditionOrExpression(getParentNode(), node))
        {
            TIntermSequence insertions;

            TIntermSymbol *tempSymbol         = createTempSymbol(node->getType());
            TIntermAggregate *tempDeclaration = new TIntermAggregate(EOpDeclaration);
            tempDeclaration->getSequence()->push_back(tempSymbol);
            insertions.push_back(tempDeclaration);

            TIntermAggregate *trueBlock = new TIntermAggregate(EOpSequence);
            TIntermBinary *trueAssignment =
                createTempAssignment(node->getTrueBlock()->getAsTyped());
            trueBlock->getSequence()->push_back(trueAssignment);

            TIntermAggregate *falseBlock = new TIntermAggregate(EOpSequence);
            TIntermBinary *falseAssignment =
                createTempAssignment(node->getFalseBlock()->getAsTyped());
            falseBlock->getSequence()->push_back(falseAssignment);

            TIntermSelection *ifNode =
                new TIntermSelection(node->getCondition()->getAsTyped(), trueBlock, falseBlock);
            insertions.push_back(ifNode);

            insertStatementsInParentBlock(insertions);

            TIntermSymbol *ternaryResult = createTempSymbol(node->getType());
            NodeUpdateEntry replaceVariable(getParentNode(), node, ternaryResult, false);
            mReplacements.push_back(replaceVariable);
        }
        return false;
    }

    return true;
}

bool UnfoldShortCircuitTraverser::visitAggregate(Visit visit, TIntermAggregate *node)
{
    if (visit == PreVisit && mFoundShortCircuit)
        return false; // No need to traverse further

    if (node->getOp() == EOpComma)
    {
        ASSERT(visit != PreVisit || !mFoundShortCircuit);

        if (visit == PostVisit && mFoundShortCircuit)
        {
            // We can be sure that we arrived here because there was a short-circuiting operator
            // inside the sequence operator since we only start traversing the sequence operator in
            // case a short-circuiting operator has not been found so far.
            // We need to unfold the sequence (comma) operator, otherwise the evaluation order of
            // statements would be messed up by unfolded operations inside.
            // Don't do any other unfolding on this round of traversal.
            mReplacements.clear();
            mMultiReplacements.clear();
            mInsertions.clear();

            if (!copyLoopConditionOrExpression(getParentNode(), node))
            {
                TIntermSequence insertions;
                TIntermSequence *seq = node->getSequence();

                TIntermSequence::size_type i = 0;
                ASSERT(!seq->empty());
                while (i < seq->size() - 1)
                {
                    TIntermTyped *child = (*seq)[i]->getAsTyped();
                    insertions.push_back(child);
                    ++i;
                }

                insertStatementsInParentBlock(insertions);

                NodeUpdateEntry replaceVariable(getParentNode(), node, (*seq)[i], false);
                mReplacements.push_back(replaceVariable);
            }
        }
    }
    return true;
}

bool UnfoldShortCircuitTraverser::visitLoop(Visit visit, TIntermLoop *node)
{
    if (visit == PreVisit)
    {
        if (mFoundShortCircuit)
            return false;  // No need to traverse further

        mLoopParent = getParentNode();
        mParentLoop = node;
        incrementDepth(node);

        if (node->getInit())
        {
            node->getInit()->traverse(this);
            if (mFoundShortCircuit)
            {
                decrementDepth();
                return false;
            }
        }

        if (node->getCondition())
        {
            mInLoopCondition = true;
            node->getCondition()->traverse(this);
            mInLoopCondition = false;

            if (mFoundShortCircuit)
            {
                decrementDepth();
                return false;
            }
        }

        if (node->getExpression())
        {
            mInLoopExpression = true;
            node->getExpression()->traverse(this);
            mInLoopExpression = false;

            if (mFoundShortCircuit)
            {
                decrementDepth();
                return false;
            }
        }

        if (node->getBody())
            node->getBody()->traverse(this);

        decrementDepth();
    }
    return false;
}

bool UnfoldShortCircuitTraverser::copyLoopConditionOrExpression(TIntermNode *parent,
                                                                TIntermTyped *node)
{
    if (mInLoopCondition)
    {
        mReplacements.push_back(
            NodeUpdateEntry(parent, node, createTempSymbol(node->getType()), false));
        TIntermAggregate *body = mParentLoop->getBody();
        TIntermSequence empty;
        if (mParentLoop->getType() == ELoopDoWhile)
        {
            // Declare the temporary variable before the loop.
            TIntermSequence insertionsBeforeLoop;
            insertionsBeforeLoop.push_back(createTempDeclaration(node->getType()));
            insertStatementsInParentBlock(insertionsBeforeLoop);

            // Move a part of do-while loop condition to inside the loop.
            TIntermSequence insertionsInLoop;
            insertionsInLoop.push_back(createTempAssignment(node));
            mInsertions.push_back(NodeInsertMultipleEntry(body, body->getSequence()->size() - 1,
                                                          empty, insertionsInLoop));
        }
        else
        {
            // The loop initializer expression and one copy of the part of the loop condition are
            // executed before the loop. They need to be in a new scope.
            TIntermAggregate *loopScope = new TIntermAggregate(EOpSequence);

            TIntermNode *initializer = mParentLoop->getInit();
            if (initializer != nullptr)
            {
                // Move the initializer to the newly created outer scope, so that condition can
                // depend on it.
                mReplacements.push_back(NodeUpdateEntry(mParentLoop, initializer, nullptr, false));
                loopScope->getSequence()->push_back(initializer);
            }

            loopScope->getSequence()->push_back(createTempInitDeclaration(node));
            loopScope->getSequence()->push_back(mParentLoop);
            mReplacements.push_back(NodeUpdateEntry(mLoopParent, mParentLoop, loopScope, true));

            // The second copy of the part of the loop condition is executed inside the loop.
            TIntermSequence insertionsInLoop;
            insertionsInLoop.push_back(createTempAssignment(node->deepCopy()));
            mInsertions.push_back(NodeInsertMultipleEntry(body, body->getSequence()->size() - 1,
                                                          empty, insertionsInLoop));
        }
        return true;
    }

    if (mInLoopExpression)
    {
        TIntermTyped *movedExpression = mParentLoop->getExpression();
        mReplacements.push_back(NodeUpdateEntry(mParentLoop, movedExpression, nullptr, false));
        TIntermAggregate *body = mParentLoop->getBody();
        TIntermSequence empty;
        TIntermSequence insertions;
        insertions.push_back(movedExpression);
        mInsertions.push_back(
            NodeInsertMultipleEntry(body, body->getSequence()->size() - 1, empty, insertions));
        return true;
    }
    return false;
}

void UnfoldShortCircuitTraverser::nextIteration()
{
    mFoundShortCircuit = false;
    nextTemporaryIndex();
}

} // namespace

void UnfoldShortCircuitToIf(TIntermNode *root, unsigned int *temporaryIndex)
{
    UnfoldShortCircuitTraverser traverser;
    ASSERT(temporaryIndex != nullptr);
    traverser.useTemporaryIndex(temporaryIndex);
    // Unfold one operator at a time, and reset the traverser between iterations.
    do
    {
        traverser.nextIteration();
        root->traverse(&traverser);
        if (traverser.foundShortCircuit())
            traverser.updateTree();
    }
    while (traverser.foundShortCircuit());
}
