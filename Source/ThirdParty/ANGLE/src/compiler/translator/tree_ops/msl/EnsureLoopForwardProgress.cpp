//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EnsureLoopForwardProgress is an AST traverser that inserts volatile variable
// access inside loops which it cannot prove to be finite.
//

#include "compiler/translator/tree_ops/msl/EnsureLoopForwardProgress.h"

#include "compiler/translator/Compiler.h"
#include "compiler/translator/StaticType.h"
#include "compiler/translator/tree_util/IntermNodePatternMatcher.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"

namespace sh
{

namespace
{

const TVariable *ViewSymbolVariable(TIntermTyped &node)
{
    TIntermSymbol *symbol = node.getAsSymbolNode();
    if (symbol == nullptr)
    {
        return nullptr;
    }
    return &symbol->variable();
}

bool IsReadOnlyExpr(TIntermTyped &node)
{
    switch (node.getQualifier())
    {
        case EvqConst:
        case EvqAttribute:
        case EvqUniform:
        case EvqVaryingIn:
        case EvqSmoothIn:
        case EvqFlatIn:
        case EvqNoPerspectiveIn:
        case EvqCentroidIn:
        case EvqSampleIn:
        case EvqNoPerspectiveCentroidIn:
        case EvqNoPerspectiveSampleIn:
            return true;
        default:
            break;
    }
    return false;
}

const TVariable *computeFiniteLoopVariable(TIntermLoop *loop)
{
    // Currently matches only to loop of form:
    // for (**; cond ; expr)
    // where
    //   cond is `variable` `relation` `readonly symbol` and `variable`is of type int or uint
    //   expr increments or decrements the variable by one.
    // Assumes ints wrap around in a defined way.
    TIntermTyped *cond = loop->getCondition();
    if (cond == nullptr)
    {
        return nullptr;
    }
    TIntermTyped *expr = loop->getExpression();
    if (expr == nullptr)
    {
        return nullptr;
    }
    TIntermBinary *binCond = cond->getAsBinaryNode();
    if (binCond == nullptr)
    {
        return nullptr;
    }
    const TVariable *variable = ViewSymbolVariable(*binCond->getLeft());
    if (variable == nullptr)
    {
        return nullptr;
    }
    if (!IsInteger(variable->getType().getBasicType()))
    {
        return nullptr;
    }
    switch (binCond->getOp())
    {
        case EOpEqual:
        case EOpNotEqual:
        case EOpLessThan:
        case EOpGreaterThan:
        case EOpLessThanEqual:
        case EOpGreaterThanEqual:
            break;
        default:
            return nullptr;
    }
    // Loop index must be compared with a constant or uniform or similar read-only variable.
    if (!IsReadOnlyExpr(*binCond->getRight()))
    {
        return nullptr;
    }
    if (TIntermUnary *unary = expr->getAsUnaryNode())
    {
        switch (unary->getOp())
        {
            case EOpPostIncrement:
            case EOpPreIncrement:
            case EOpPreDecrement:
            case EOpPostDecrement:
                break;
            default:
                return nullptr;
        }
        if (variable != ViewSymbolVariable(*unary->getOperand()))
        {
            return nullptr;
        }
    }
    else if (TIntermBinary *binExpr = expr->getAsBinaryNode())
    {
        switch (binExpr->getOp())
        {
            case EOpAddAssign:
            case EOpSubAssign:
                break;
            default:
                return nullptr;
        }
        if (variable != ViewSymbolVariable(*binExpr->getLeft()))
        {
            return nullptr;
        }
        const TConstantUnion *value = binExpr->getRight()->getConstantValue();
        if (value == nullptr)
        {
            return nullptr;
        }
        switch (value->getType())
        {
            case EbtInt:
                if (value->getIConst() == -1 || value->getIConst() == 1)
                {
                    break;
                }
                return nullptr;
            case EbtUInt:
                if (value->getUConst() == 1)
                {
                    break;
                }
                return nullptr;
            default:
                UNREACHABLE();
                return nullptr;
        }
    }
    return variable;
}

class LoopInfoStack
{
  public:
    LoopInfoStack(TIntermLoop *node, LoopInfoStack *parent);
    bool isFinite() const { return mVariable != nullptr; }
    LoopInfoStack *getParent() const { return mParent; }
    void setNotFinite() { mVariable = nullptr; }
    LoopInfoStack *findLoopForVariable(const TVariable *variable);

  private:
    LoopInfoStack *mParent     = nullptr;
    const TVariable *mVariable = nullptr;
};

LoopInfoStack::LoopInfoStack(TIntermLoop *node, LoopInfoStack *parent)
    : mParent(parent), mVariable(computeFiniteLoopVariable(node))
{}

LoopInfoStack *LoopInfoStack::findLoopForVariable(const TVariable *variable)
{
    LoopInfoStack *info = this;
    do
    {
        if (info->mVariable == variable)
        {
            return info;
        }
        info = info->mParent;
    } while (info != nullptr);
    return nullptr;
}

class EnsureLoopForwardProgressTraverser final : public TLValueTrackingTraverser
{
  public:
    EnsureLoopForwardProgressTraverser(TSymbolTable *symbolTable);
    void visitSymbol(TIntermSymbol *node) override;
    void traverseLoop(TIntermLoop *node) override;

  private:
    LoopInfoStack *mLoopInfoStack = nullptr;
};

EnsureLoopForwardProgressTraverser::EnsureLoopForwardProgressTraverser(TSymbolTable *symbolTable)
    : TLValueTrackingTraverser(true, false, false, symbolTable)
{}

void EnsureLoopForwardProgressTraverser::traverseLoop(TIntermLoop *node)
{
    LoopInfoStack loopInfo{node, mLoopInfoStack};
    mLoopInfoStack = &loopInfo;

    ScopedNodeInTraversalPath addToPath(this, node);
    node->getBody()->traverse(this);

    if (!loopInfo.isFinite())
    {
        TIntermBlock *newBody     = new TIntermBlock();
        TIntermSequence *sequence = newBody->getSequence();
        sequence->push_back(CreateBuiltInFunctionCallNode("loopForwardProgress", {}, *mSymbolTable,
                                                          kESSLInternalBackendBuiltIns));
        sequence->push_back(node->getBody());
        node->setBody(newBody);
    }
    mLoopInfoStack = mLoopInfoStack->getParent();
}

void EnsureLoopForwardProgressTraverser::visitSymbol(TIntermSymbol *node)
{
    if (!mLoopInfoStack)
    {
        return;
    }
    LoopInfoStack *loop = mLoopInfoStack->findLoopForVariable(&node->variable());
    if (loop != nullptr && isLValueRequiredHere())
    {
        loop->setNotFinite();
    }
}

}  // namespace

bool EnsureLoopForwardProgress(TCompiler *compiler, TIntermNode *root)
{
    EnsureLoopForwardProgressTraverser traverser(&compiler->getSymbolTable());
    root->traverse(&traverser);
    return traverser.updateTree(compiler, root);
}

}  // namespace sh
