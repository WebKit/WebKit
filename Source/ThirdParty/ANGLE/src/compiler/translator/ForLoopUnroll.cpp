//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/ForLoopUnroll.h"

#include "compiler/translator/ValidateLimitations.h"
#include "angle_gl.h"

bool ForLoopUnrollMarker::visitBinary(Visit, TIntermBinary *node)
{
    if (mUnrollCondition != kSamplerArrayIndex)
        return true;

    // If a sampler array index is also the loop index,
    //   1) if the index type is integer, mark the loop for unrolling;
    //   2) if the index type if float, set a flag to later fail compile.
    switch (node->getOp())
    {
      case EOpIndexIndirect:
        if (node->getLeft() != NULL && node->getRight() != NULL && node->getLeft()->getAsSymbolNode())
        {
            TIntermSymbol *symbol = node->getLeft()->getAsSymbolNode();
            if (IsSampler(symbol->getBasicType()) && symbol->isArray() && !mLoopStack.empty())
            {
                mVisitSamplerArrayIndexNodeInsideLoop = true;
                node->getRight()->traverse(this);
                mVisitSamplerArrayIndexNodeInsideLoop = false;
                // We have already visited all the children.
                return false;
            }
        }
        break;
      default:
        break;
    }
    return true;
}

bool ForLoopUnrollMarker::visitLoop(Visit, TIntermLoop *node)
{
    bool canBeUnrolled = mHasRunLoopValidation;
    if (!mHasRunLoopValidation)
    {
        canBeUnrolled = ValidateLimitations::IsLimitedForLoop(node);
    }
    if (mUnrollCondition == kIntegerIndex && canBeUnrolled)
    {
        // Check if loop index type is integer.
        // This is called after ValidateLimitations pass, so the loop has the limited form specified
        // in ESSL 1.00 appendix A.
        TIntermSequence *declSeq = node->getInit()->getAsAggregate()->getSequence();
        TIntermSymbol *symbol = (*declSeq)[0]->getAsBinaryNode()->getLeft()->getAsSymbolNode();
        if (symbol->getBasicType() == EbtInt)
            node->setUnrollFlag(true);
    }

    TIntermNode *body = node->getBody();
    if (body != nullptr)
    {
        if (canBeUnrolled)
        {
            mLoopStack.push(node);
            body->traverse(this);
            mLoopStack.pop();
        }
        else
        {
            body->traverse(this);
        }
    }
    // The loop is fully processed - no need to visit children.
    return false;
}

void ForLoopUnrollMarker::visitSymbol(TIntermSymbol* symbol)
{
    if (!mVisitSamplerArrayIndexNodeInsideLoop)
        return;
    TIntermLoop *loop = mLoopStack.findLoop(symbol);
    if (loop)
    {
        switch (symbol->getBasicType())
        {
          case EbtFloat:
            mSamplerArrayIndexIsFloatLoopIndex = true;
            break;
          case EbtInt:
            loop->setUnrollFlag(true);
            break;
          default:
            UNREACHABLE();
        }
    }
}
