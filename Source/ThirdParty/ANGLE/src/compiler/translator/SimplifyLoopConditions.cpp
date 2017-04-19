//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SimplifyLoopConditions is an AST traverser that converts loop conditions and loop expressions
// to regular statements inside the loop. This way further transformations that generate statements
// from loop conditions and loop expressions work correctly.
//

#include "compiler/translator/SimplifyLoopConditions.h"

#include "compiler/translator/IntermNode.h"
#include "compiler/translator/IntermNodePatternMatcher.h"

namespace sh
{

namespace
{

TIntermConstantUnion *CreateBoolConstantNode(bool value)
{
    TConstantUnion *u = new TConstantUnion;
    u->setBConst(value);
    TIntermConstantUnion *node =
        new TIntermConstantUnion(u, TType(EbtBool, EbpUndefined, EvqConst, 1));
    return node;
}

class SimplifyLoopConditionsTraverser : public TLValueTrackingTraverser
{
  public:
    SimplifyLoopConditionsTraverser(unsigned int conditionsToSimplifyMask,
                                    const TSymbolTable &symbolTable,
                                    int shaderVersion);

    void traverseLoop(TIntermLoop *node) override;

    bool visitBinary(Visit visit, TIntermBinary *node) override;
    bool visitAggregate(Visit visit, TIntermAggregate *node) override;
    bool visitTernary(Visit visit, TIntermTernary *node) override;
    bool visitDeclaration(Visit visit, TIntermDeclaration *node) override;

    void nextIteration();
    bool foundLoopToChange() const { return mFoundLoopToChange; }

  protected:
    // Marked to true once an operation that needs to be hoisted out of the expression has been
    // found. After that, no more AST updates are performed on that traversal.
    bool mFoundLoopToChange;
    bool mInsideLoopInitConditionOrExpression;
    IntermNodePatternMatcher mConditionsToSimplify;
};

SimplifyLoopConditionsTraverser::SimplifyLoopConditionsTraverser(
    unsigned int conditionsToSimplifyMask,
    const TSymbolTable &symbolTable,
    int shaderVersion)
    : TLValueTrackingTraverser(true, false, false, symbolTable, shaderVersion),
      mFoundLoopToChange(false),
      mInsideLoopInitConditionOrExpression(false),
      mConditionsToSimplify(conditionsToSimplifyMask)
{
}

void SimplifyLoopConditionsTraverser::nextIteration()
{
    mFoundLoopToChange                   = false;
    mInsideLoopInitConditionOrExpression = false;
    nextTemporaryIndex();
}

// The visit functions operate in three modes:
// 1. If a matching expression has already been found, we return early since only one loop can
//    be transformed on one traversal.
// 2. We try to find loops. In case a node is not inside a loop and can not contain loops, we
//    stop traversing the subtree.
// 3. If we're inside a loop initialization, condition or expression, we check for expressions
//    that should be moved out of the loop condition or expression. If one is found, the loop
//    is processed.
bool SimplifyLoopConditionsTraverser::visitBinary(Visit visit, TIntermBinary *node)
{

    if (mFoundLoopToChange)
        return false;

    if (!mInsideLoopInitConditionOrExpression)
        return false;

    mFoundLoopToChange = mConditionsToSimplify.match(node, getParentNode(), isLValueRequiredHere());
    return !mFoundLoopToChange;
}

bool SimplifyLoopConditionsTraverser::visitAggregate(Visit visit, TIntermAggregate *node)
{
    if (mFoundLoopToChange)
        return false;

    if (!mInsideLoopInitConditionOrExpression)
        return false;

    mFoundLoopToChange = mConditionsToSimplify.match(node, getParentNode());
    return !mFoundLoopToChange;
}

bool SimplifyLoopConditionsTraverser::visitTernary(Visit visit, TIntermTernary *node)
{
    if (mFoundLoopToChange)
        return false;

    if (!mInsideLoopInitConditionOrExpression)
        return false;

    mFoundLoopToChange = mConditionsToSimplify.match(node);
    return !mFoundLoopToChange;
}

bool SimplifyLoopConditionsTraverser::visitDeclaration(Visit visit, TIntermDeclaration *node)
{
    if (mFoundLoopToChange)
        return false;

    if (!mInsideLoopInitConditionOrExpression)
        return false;

    mFoundLoopToChange = mConditionsToSimplify.match(node);
    return !mFoundLoopToChange;
}

void SimplifyLoopConditionsTraverser::traverseLoop(TIntermLoop *node)
{
    if (mFoundLoopToChange)
        return;

    // Mark that we're inside a loop condition or expression, and transform the loop if needed.

    ScopedNodeInTraversalPath addToPath(this, node);

    mInsideLoopInitConditionOrExpression = true;
    TLoopType loopType                   = node->getType();

    if (!mFoundLoopToChange && node->getInit())
    {
        node->getInit()->traverse(this);
    }

    if (!mFoundLoopToChange && node->getCondition())
    {
        node->getCondition()->traverse(this);
    }

    if (!mFoundLoopToChange && node->getExpression())
    {
        node->getExpression()->traverse(this);
    }

    if (mFoundLoopToChange)
    {
        // Replace the loop condition with a boolean variable that's updated on each iteration.
        if (loopType == ELoopWhile)
        {
            // Transform:
            //   while (expr) { body; }
            // into
            //   bool s0 = expr;
            //   while (s0) { { body; } s0 = expr; }
            TIntermSequence tempInitSeq;
            tempInitSeq.push_back(createTempInitDeclaration(node->getCondition()->deepCopy()));
            insertStatementsInParentBlock(tempInitSeq);

            TIntermBlock *newBody = new TIntermBlock();
            if (node->getBody())
            {
                newBody->getSequence()->push_back(node->getBody());
            }
            newBody->getSequence()->push_back(
                createTempAssignment(node->getCondition()->deepCopy()));

            // Can't use queueReplacement to replace old body, since it may have been nullptr.
            // It's safe to do the replacements in place here - this node won't be traversed
            // further.
            node->setBody(newBody);
            node->setCondition(createTempSymbol(node->getCondition()->getType()));
        }
        else if (loopType == ELoopDoWhile)
        {
            // Transform:
            //   do {
            //     body;
            //   } while (expr);
            // into
            //   bool s0 = true;
            //   do {
            //     { body; }
            //     s0 = expr;
            //   } while (s0);
            TIntermSequence tempInitSeq;
            tempInitSeq.push_back(createTempInitDeclaration(CreateBoolConstantNode(true)));
            insertStatementsInParentBlock(tempInitSeq);

            TIntermBlock *newBody = new TIntermBlock();
            if (node->getBody())
            {
                newBody->getSequence()->push_back(node->getBody());
            }
            newBody->getSequence()->push_back(
                createTempAssignment(node->getCondition()->deepCopy()));

            // Can't use queueReplacement to replace old body, since it may have been nullptr.
            // It's safe to do the replacements in place here - this node won't be traversed
            // further.
            node->setBody(newBody);
            node->setCondition(createTempSymbol(node->getCondition()->getType()));
        }
        else if (loopType == ELoopFor)
        {
            // Move the loop condition inside the loop.
            // Transform:
            //   for (init; expr; exprB) { body; }
            // into
            //   {
            //     init;
            //     bool s0 = expr;
            //     while (s0) {
            //       { body; }
            //       exprB;
            //       s0 = expr;
            //     }
            //   }
            TIntermBlock *loopScope            = new TIntermBlock();
            TIntermSequence *loopScopeSequence = loopScope->getSequence();

            // Insert "init;"
            if (node->getInit())
            {
                loopScopeSequence->push_back(node->getInit());
            }

            // Insert "bool s0 = expr;" if applicable, "bool s0 = true;" otherwise
            TIntermTyped *conditionInitializer = nullptr;
            if (node->getCondition())
            {
                conditionInitializer = node->getCondition()->deepCopy();
            }
            else
            {
                conditionInitializer = TIntermTyped::CreateBool(true);
            }
            loopScopeSequence->push_back(createTempInitDeclaration(conditionInitializer));

            // Insert "{ body; }" in the while loop
            TIntermBlock *whileLoopBody = new TIntermBlock();
            if (node->getBody())
            {
                whileLoopBody->getSequence()->push_back(node->getBody());
            }
            // Insert "exprB;" in the while loop
            if (node->getExpression())
            {
                whileLoopBody->getSequence()->push_back(node->getExpression());
            }
            // Insert "s0 = expr;" in the while loop
            if (node->getCondition())
            {
                whileLoopBody->getSequence()->push_back(
                    createTempAssignment(node->getCondition()->deepCopy()));
            }

            // Create "while(s0) { whileLoopBody }"
            TIntermLoop *whileLoop = new TIntermLoop(
                ELoopWhile, nullptr, createTempSymbol(conditionInitializer->getType()), nullptr,
                whileLoopBody);
            loopScope->getSequence()->push_back(whileLoop);
            queueReplacement(node, loopScope, OriginalNode::IS_DROPPED);
        }
    }

    mInsideLoopInitConditionOrExpression = false;

    if (!mFoundLoopToChange && node->getBody())
        node->getBody()->traverse(this);
}

}  // namespace

void SimplifyLoopConditions(TIntermNode *root,
                            unsigned int conditionsToSimplifyMask,
                            unsigned int *temporaryIndex,
                            const TSymbolTable &symbolTable,
                            int shaderVersion)
{
    SimplifyLoopConditionsTraverser traverser(conditionsToSimplifyMask, symbolTable, shaderVersion);
    ASSERT(temporaryIndex != nullptr);
    traverser.useTemporaryIndex(temporaryIndex);
    // Process one loop at a time, and reset the traverser between iterations.
    do
    {
        traverser.nextIteration();
        root->traverse(&traverser);
        if (traverser.foundLoopToChange())
            traverser.updateTree();
    } while (traverser.foundLoopToChange());
}

}  // namespace sh
