//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the integer pow expressions HLSL bug workaround.
// See header for more info.

#include "compiler/translator/ExpandIntegerPowExpressions.h"

#include <cmath>
#include <cstdlib>

#include "compiler/translator/IntermNode.h"

namespace sh
{

namespace
{

class Traverser : public TIntermTraverser
{
  public:
    static void Apply(TIntermNode *root, unsigned int *tempIndex);

  private:
    Traverser();
    bool visitAggregate(Visit visit, TIntermAggregate *node) override;
    void nextIteration();

    bool mFound = false;
};

// static
void Traverser::Apply(TIntermNode *root, unsigned int *tempIndex)
{
    Traverser traverser;
    traverser.useTemporaryIndex(tempIndex);
    do
    {
        traverser.nextIteration();
        root->traverse(&traverser);
        if (traverser.mFound)
        {
            traverser.updateTree();
        }
    } while (traverser.mFound);
}

Traverser::Traverser() : TIntermTraverser(true, false, false)
{
}

void Traverser::nextIteration()
{
    mFound = false;
    nextTemporaryIndex();
}

bool Traverser::visitAggregate(Visit visit, TIntermAggregate *node)
{
    if (mFound)
    {
        return false;
    }

    // Test 0: skip non-pow operators.
    if (node->getOp() != EOpPow)
    {
        return true;
    }

    const TIntermSequence *sequence = node->getSequence();
    ASSERT(sequence->size() == 2u);
    const TIntermConstantUnion *constantNode = sequence->at(1)->getAsConstantUnion();

    // Test 1: check for a single constant.
    if (!constantNode || constantNode->getNominalSize() != 1)
    {
        return true;
    }

    const TConstantUnion *constant = constantNode->getUnionArrayPointer();

    TConstantUnion asFloat;
    asFloat.cast(EbtFloat, *constant);

    float value = asFloat.getFConst();

    // Test 2: value is in the problematic range.
    if (value < -5.0f || value > 9.0f)
    {
        return true;
    }

    // Test 3: value is integer or pretty close to an integer.
    float absval = std::abs(value);
    float frac   = absval - std::round(absval);
    if (frac > 0.0001f)
    {
        return true;
    }

    // Test 4: skip -1, 0, and 1
    int exponent = static_cast<int>(value);
    int n        = std::abs(exponent);
    if (n < 2)
    {
        return true;
    }

    // Potential problem case detected, apply workaround.
    nextTemporaryIndex();

    TIntermTyped *lhs = sequence->at(0)->getAsTyped();
    ASSERT(lhs);

    TIntermAggregate *init = createTempInitDeclaration(lhs);
    TIntermTyped *current  = createTempSymbol(lhs->getType());

    insertStatementInParentBlock(init);

    // Create a chain of n-1 multiples.
    for (int i = 1; i < n; ++i)
    {
        TIntermBinary *mul = new TIntermBinary(EOpMul, current, createTempSymbol(lhs->getType()));
        mul->setLine(node->getLine());
        current = mul;
    }

    // For negative pow, compute the reciprocal of the positive pow.
    if (exponent < 0)
    {
        TConstantUnion *oneVal = new TConstantUnion();
        oneVal->setFConst(1.0f);
        TIntermConstantUnion *oneNode = new TIntermConstantUnion(oneVal, node->getType());
        TIntermBinary *div            = new TIntermBinary(EOpDiv, oneNode, current);
        current = div;
    }

    queueReplacement(node, current, OriginalNode::IS_DROPPED);
    mFound = true;
    return false;
}

}  // anonymous namespace

void ExpandIntegerPowExpressions(TIntermNode *root, unsigned int *tempIndex)
{
    Traverser::Apply(root, tempIndex);
}

}  // namespace sh
