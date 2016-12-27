//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/UnfoldShortCircuitAST.h"

namespace
{

// "x || y" is equivalent to "x ? true : y".
TIntermTernary *UnfoldOR(TIntermTyped *x, TIntermTyped *y)
{
    TConstantUnion *u = new TConstantUnion;
    u->setBConst(true);
    TIntermConstantUnion *trueNode = new TIntermConstantUnion(
        u, TType(EbtBool, EbpUndefined, EvqConst, 1));
    return new TIntermTernary(x, trueNode, y);
}

// "x && y" is equivalent to "x ? y : false".
TIntermTernary *UnfoldAND(TIntermTyped *x, TIntermTyped *y)
{
    TConstantUnion *u = new TConstantUnion;
    u->setBConst(false);
    TIntermConstantUnion *falseNode = new TIntermConstantUnion(
        u, TType(EbtBool, EbpUndefined, EvqConst, 1));
    return new TIntermTernary(x, y, falseNode);
}

}  // namespace anonymous

bool UnfoldShortCircuitAST::visitBinary(Visit visit, TIntermBinary *node)
{
    TIntermTernary *replacement = nullptr;

    switch (node->getOp())
    {
      case EOpLogicalOr:
        replacement = UnfoldOR(node->getLeft(), node->getRight());
        break;
      case EOpLogicalAnd:
        replacement = UnfoldAND(node->getLeft(), node->getRight());
        break;
      default:
        break;
    }
    if (replacement)
    {
        queueReplacement(node, replacement, OriginalNode::IS_DROPPED);
    }
    return true;
}
