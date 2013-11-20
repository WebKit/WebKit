//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/InitializeGLPosition.h"
#include "compiler/debug.h"

bool InitializeGLPosition::visitAggregate(Visit visit, TIntermAggregate* node)
{
    bool visitChildren = !mCodeInserted;
    switch (node->getOp())
    {
      case EOpSequence: break;
      case EOpFunction:
      {
        // Function definition.
        ASSERT(visit == PreVisit);
        if (node->getName() == "main(")
        {
            TIntermSequence &sequence = node->getSequence();
            ASSERT((sequence.size() == 1) || (sequence.size() == 2));
            TIntermAggregate *body = NULL;
            if (sequence.size() == 1)
            {
                body = new TIntermAggregate(EOpSequence);
                sequence.push_back(body);
            }
            else
            {
                body = sequence[1]->getAsAggregate();
            }
            ASSERT(body);
            insertCode(body->getSequence());
            mCodeInserted = true;
        }
        break;
      }
      default: visitChildren = false; break;
    }
    return visitChildren;
}

void InitializeGLPosition::insertCode(TIntermSequence& sequence)
{
    TIntermBinary *binary = new TIntermBinary(EOpAssign);
    sequence.insert(sequence.begin(), binary);

    TIntermSymbol *left = new TIntermSymbol(
        0, "gl_Position", TType(EbtFloat, EbpUndefined, EvqPosition, 4));
    binary->setLeft(left);

    ConstantUnion *u = new ConstantUnion[4];
    for (int ii = 0; ii < 3; ++ii)
        u[ii].setFConst(0.0f);
    u[3].setFConst(1.0f);
    TIntermConstantUnion *right = new TIntermConstantUnion(
        u, TType(EbtFloat, EbpUndefined, EvqConst, 4));
    binary->setRight(right);
}
