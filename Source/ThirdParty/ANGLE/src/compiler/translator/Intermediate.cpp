//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

//
// Build the intermediate representation.
//

#include <float.h>
#include <limits.h>
#include <algorithm>

#include "compiler/translator/Intermediate.h"
#include "compiler/translator/SymbolTable.h"

namespace sh
{

////////////////////////////////////////////////////////////////////////////
//
// First set of functions are to help build the intermediate representation.
// These functions are not member functions of the nodes.
// They are called from parser productions.
//
/////////////////////////////////////////////////////////////////////////////

//
// Add a terminal node for an identifier in an expression.
//
// Returns the added node.
//
TIntermSymbol *TIntermediate::addSymbol(int id,
                                        const TString &name,
                                        const TType &type,
                                        const TSourceLoc &line)
{
    TIntermSymbol *node = new TIntermSymbol(id, name, type);
    node->setLine(line);

    return node;
}

//
// Connect two nodes through an index operator, where the left node is the base
// of an array or struct, and the right node is a direct or indirect offset.
//
// Returns the added node.
// The caller should set the type of the returned node.
//
TIntermTyped *TIntermediate::addIndex(TOperator op,
                                      TIntermTyped *base,
                                      TIntermTyped *index,
                                      const TSourceLoc &line,
                                      TDiagnostics *diagnostics)
{
    TIntermBinary *node = new TIntermBinary(op, base, index);
    node->setLine(line);

    TIntermTyped *folded = node->fold(diagnostics);
    if (folded)
    {
        return folded;
    }

    return node;
}

// If the input node is nullptr, return nullptr.
// If the input node is a block node, return it.
// If the input node is not a block node, put it inside a block node and return that.
TIntermBlock *TIntermediate::EnsureBlock(TIntermNode *node)
{
    if (node == nullptr)
        return nullptr;
    TIntermBlock *blockNode = node->getAsBlock();
    if (blockNode != nullptr)
        return blockNode;

    blockNode = new TIntermBlock();
    blockNode->setLine(node->getLine());
    blockNode->getSequence()->push_back(node);
    return blockNode;
}

// For "if" test nodes.  There are three children; a condition,
// a true path, and a false path.  The two paths are in the
// nodePair.
//
// Returns the node created.
TIntermNode *TIntermediate::addIfElse(TIntermTyped *cond,
                                      TIntermNodePair nodePair,
                                      const TSourceLoc &line)
{
    // For compile time constant conditions, prune the code now.

    if (cond->getAsConstantUnion())
    {
        if (cond->getAsConstantUnion()->getBConst(0) == true)
        {
            return EnsureBlock(nodePair.node1);
        }
        else
        {
            return EnsureBlock(nodePair.node2);
        }
    }

    TIntermIfElse *node =
        new TIntermIfElse(cond, EnsureBlock(nodePair.node1), EnsureBlock(nodePair.node2));
    node->setLine(line);

    return node;
}

TIntermTyped *TIntermediate::AddComma(TIntermTyped *left,
                                      TIntermTyped *right,
                                      const TSourceLoc &line,
                                      int shaderVersion)
{
    TIntermTyped *commaNode = nullptr;
    if (!left->hasSideEffects())
    {
        commaNode = right;
    }
    else
    {
        commaNode = new TIntermBinary(EOpComma, left, right);
        commaNode->setLine(line);
    }
    TQualifier resultQualifier = TIntermBinary::GetCommaQualifier(shaderVersion, left, right);
    commaNode->getTypePointer()->setQualifier(resultQualifier);
    return commaNode;
}

// For "?:" test nodes.  There are three children; a condition,
// a true path, and a false path.  The two paths are specified
// as separate parameters.
//
// Returns the ternary node created, or one of trueExpression and falseExpression if the expression
// could be folded.
TIntermTyped *TIntermediate::AddTernarySelection(TIntermTyped *cond,
                                                 TIntermTyped *trueExpression,
                                                 TIntermTyped *falseExpression,
                                                 const TSourceLoc &line)
{
    // Note that the node resulting from here can be a constant union without being qualified as
    // constant.
    if (cond->getAsConstantUnion())
    {
        TQualifier resultQualifier =
            TIntermTernary::DetermineQualifier(cond, trueExpression, falseExpression);
        if (cond->getAsConstantUnion()->getBConst(0))
        {
            trueExpression->getTypePointer()->setQualifier(resultQualifier);
            return trueExpression;
        }
        else
        {
            falseExpression->getTypePointer()->setQualifier(resultQualifier);
            return falseExpression;
        }
    }

    // Make a ternary node.
    TIntermTernary *node = new TIntermTernary(cond, trueExpression, falseExpression);
    node->setLine(line);

    return node;
}

TIntermSwitch *TIntermediate::addSwitch(TIntermTyped *init,
                                        TIntermBlock *statementList,
                                        const TSourceLoc &line)
{
    TIntermSwitch *node = new TIntermSwitch(init, statementList);
    node->setLine(line);

    return node;
}

TIntermCase *TIntermediate::addCase(TIntermTyped *condition, const TSourceLoc &line)
{
    TIntermCase *node = new TIntermCase(condition);
    node->setLine(line);

    return node;
}

//
// Constant terminal nodes.  Has a union that contains bool, float or int constants
//
// Returns the constant union node created.
//

TIntermConstantUnion *TIntermediate::addConstantUnion(const TConstantUnion *constantUnion,
                                                      const TType &type,
                                                      const TSourceLoc &line)
{
    TIntermConstantUnion *node = new TIntermConstantUnion(constantUnion, type);
    node->setLine(line);

    return node;
}

TIntermTyped *TIntermediate::AddSwizzle(TIntermTyped *baseExpression,
                                        const TVectorFields &fields,
                                        const TSourceLoc &dotLocation)
{
    TVector<int> fieldsVector;
    for (int i = 0; i < fields.num; ++i)
    {
        fieldsVector.push_back(fields.offsets[i]);
    }
    TIntermSwizzle *node = new TIntermSwizzle(baseExpression, fieldsVector);
    node->setLine(dotLocation);

    TIntermTyped *folded = node->fold();
    if (folded)
    {
        return folded;
    }

    return node;
}

//
// Create loop nodes.
//
TIntermNode *TIntermediate::addLoop(TLoopType type,
                                    TIntermNode *init,
                                    TIntermTyped *cond,
                                    TIntermTyped *expr,
                                    TIntermNode *body,
                                    const TSourceLoc &line)
{
    TIntermNode *node = new TIntermLoop(type, init, cond, expr, EnsureBlock(body));
    node->setLine(line);

    return node;
}

//
// Add branches.
//
TIntermBranch *TIntermediate::addBranch(TOperator branchOp, const TSourceLoc &line)
{
    return addBranch(branchOp, 0, line);
}

TIntermBranch *TIntermediate::addBranch(TOperator branchOp,
                                        TIntermTyped *expression,
                                        const TSourceLoc &line)
{
    TIntermBranch *node = new TIntermBranch(branchOp, expression);
    node->setLine(line);

    return node;
}

TIntermTyped *TIntermediate::foldAggregateBuiltIn(TIntermAggregate *aggregate,
                                                  TDiagnostics *diagnostics)
{
    switch (aggregate->getOp())
    {
        case EOpAtan:
        case EOpPow:
        case EOpMod:
        case EOpMin:
        case EOpMax:
        case EOpClamp:
        case EOpMix:
        case EOpStep:
        case EOpSmoothStep:
        case EOpLdexp:
        case EOpMulMatrixComponentWise:
        case EOpOuterProduct:
        case EOpEqualComponentWise:
        case EOpNotEqualComponentWise:
        case EOpLessThanComponentWise:
        case EOpLessThanEqualComponentWise:
        case EOpGreaterThanComponentWise:
        case EOpGreaterThanEqualComponentWise:
        case EOpDistance:
        case EOpDot:
        case EOpCross:
        case EOpFaceForward:
        case EOpReflect:
        case EOpRefract:
        case EOpBitfieldExtract:
        case EOpBitfieldInsert:
            return aggregate->fold(diagnostics);
        default:
            // TODO: Add support for folding array constructors
            if (aggregate->isConstructor() && !aggregate->isArray())
            {
                return aggregate->fold(diagnostics);
            }
            // Constant folding not supported for the built-in.
            return nullptr;
    }
}

}  // namespace sh
