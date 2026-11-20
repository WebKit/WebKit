//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EnsureLoopForwardProgress is an AST traverser that ensures that loops are either finite or
// invoke side-effects.
//

#include "compiler/translator/tree_ops/EnsureLoopForwardProgress.h"

#include <limits>

#include "compiler/translator/Compiler.h"
#include "compiler/translator/StaticType.h"
#include "compiler/translator/tree_util/IntermNodePatternMatcher.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"

namespace sh
{

namespace
{

bool IsReadOnlyVariable(TIntermSymbol *variable)
{
    switch (variable->getQualifier())
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
    // Currently, only match loops in the following form:
    //
    //     for (...; variable op constant; expression)
    //
    // Where:
    //
    // * `variable` can only be a scalar integer
    // * `op` can only be one of > >= < <= == or !=
    // * `constant` can be a constant, or a read-only value such as shader inputs and
    // uniforms.
    // * `expression` can only be one of:
    //   * variable++
    //   * variable--
    //   * variable += constant
    //   * variable -= constant
    TIntermTyped *cond = loop->getCondition();
    TIntermTyped *expr = loop->getExpression();
    if (cond == nullptr || expr == nullptr)
    {
        return nullptr;
    }

    TIntermBinary *condBinary = cond->getAsBinaryNode();
    if (condBinary == nullptr)
    {
        return nullptr;
    }

    TIntermSymbol *loopVariable              = condBinary->getLeft()->getAsSymbolNode();
    TIntermSymbol *comparatorSymbol          = condBinary->getRight()->getAsSymbolNode();
    TIntermConstantUnion *comparatorConstant = condBinary->getRight()->getAsConstantUnion();
    if (loopVariable == nullptr || (comparatorSymbol == nullptr && comparatorConstant == nullptr))
    {
        return nullptr;
    }

    // Limit to local scalar integer types
    if (!IsInteger(loopVariable->getType().getBasicType()) || !loopVariable->getType().isScalar() ||
        loopVariable->getType().getQualifier() != EvqTemporary)
    {
        return nullptr;
    }

    // If the loop comparator is a symbol, it must be read-only.
    if (comparatorSymbol != nullptr && !IsReadOnlyVariable(comparatorSymbol))
    {
        return nullptr;
    }

    switch (condBinary->getOp())
    {
        case EOpEqual:
        case EOpNotEqual:
        case EOpLessThan:
        case EOpGreaterThan:
            break;
        case EOpLessThanEqual:
        {
            // If the condition is <=, it might be |<= UINT_MAX| or |<= INT_MAX| for signed ints.
            // However, |for| loops with <= are likely common, so we try to avoid considering them
            // as infinite loop unnecessarily.  For simplicity, if a variable is used or any
            // unrealistically large constant, consider it a potential infinite loop.
            constexpr int32_t kLoopLimit     = 1'000'000;
            const TConstantUnion *comparator = condBinary->getRight()->getConstantValue();
            if (comparator == nullptr)
            {
                return nullptr;
            }
            switch (comparator->getType())
            {
                case EbtInt:
                    if (comparator->getIConst() < 0 || comparator->getIConst() >= kLoopLimit)
                    {
                        return nullptr;
                    }
                    break;
                case EbtUInt:
                    if (comparator->getUConst() >= kLoopLimit)
                    {
                        return nullptr;
                    }
                    break;
                default:
                    return nullptr;
            }
            break;
        }
        case EOpGreaterThanEqual:
            // If the type is uint and the condition is >=, it might be |>= 0| which is always true;
            // don't consider that a finite loop.  Only |>= 0| and |>= variable_that_could_be_zero|
            // may lead to infinite loop, but all uint with >= is considered infinite loop for
            // simplicity.
            //
            // While signed int overflow is undefined behavior, |>= INT_MIN| would also be
            // unconditionally true even if the compiler makes signed int overflow well-defined.
            // Using >= in loops is uncommon, so any >= use is considered a potential infinite loop
            // for simplicity.
            return nullptr;
        default:
            return nullptr;
    }

    TIntermUnary *unary   = expr->getAsUnaryNode();
    TIntermBinary *binary = expr->getAsBinaryNode();
    if (unary == nullptr && binary == nullptr)
    {
        return nullptr;
    }

    if (unary != nullptr)
    {
        switch (unary->getOp())
        {
            case EOpPostIncrement:
            case EOpPostDecrement:
            case EOpPreIncrement:
            case EOpPreDecrement:
                break;
            default:
                return nullptr;
        }
        TIntermSymbol *exprVariable = unary->getOperand()->getAsSymbolNode();
        if (exprVariable == nullptr || loopVariable->uniqueId() != exprVariable->uniqueId())
        {
            return nullptr;
        }
    }
    else
    {
        switch (binary->getOp())
        {
            case EOpAddAssign:
            case EOpSubAssign:
                break;
            default:
                return nullptr;
        }
        TIntermSymbol *exprVariable = binary->getLeft()->getAsSymbolNode();
        const TConstantUnion *value = binary->getRight()->getConstantValue();
        if (exprVariable == nullptr || value == nullptr ||
            loopVariable->uniqueId() != exprVariable->uniqueId())
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
    return &loopVariable->variable();
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
    EnsureLoopForwardProgressTraverser(TSymbolTable *symbolTable,
                                       EnsureLoopForwardProgressMode mode);
    void visitSymbol(TIntermSymbol *node) override;
    void traverseLoop(TIntermLoop *node) override;

  private:
    void ensureProgressWithVolatile(TIntermLoop *node);
    void ensureProgressWithCounter(TIntermLoop *node);

    LoopInfoStack *mLoopInfoStack = nullptr;
    EnsureLoopForwardProgressMode mMode;
};

EnsureLoopForwardProgressTraverser::EnsureLoopForwardProgressTraverser(
    TSymbolTable *symbolTable,
    EnsureLoopForwardProgressMode mode)
    : TLValueTrackingTraverser(true, false, false, symbolTable), mMode(mode)
{}

void EnsureLoopForwardProgressTraverser::traverseLoop(TIntermLoop *node)
{
    ScopedNodeInTraversalPath addToPath(this, node);
    if (node->getInit())
    {
        node->getInit()->traverse(this);
    }
    if (node->getCondition())
    {
        node->getCondition()->traverse(this);
    }
    if (node->getExpression())
    {
        node->getExpression()->traverse(this);
    }

    LoopInfoStack loopInfo{node, mLoopInfoStack};
    mLoopInfoStack = &loopInfo;

    node->getBody()->traverse(this);

    if (!loopInfo.isFinite())
    {
        switch (mMode)
        {
            case EnsureLoopForwardProgressMode::Volatile:
                ensureProgressWithVolatile(node);
                break;
            case EnsureLoopForwardProgressMode::Counter:
                ensureProgressWithCounter(node);
                break;
        }
    }
    mLoopInfoStack = mLoopInfoStack->getParent();
}

void EnsureLoopForwardProgressTraverser::ensureProgressWithVolatile(TIntermLoop *node)
{
    // Turn the loop into the following:
    //
    //     <loop> {
    //         loopForwardProgress();
    //         {
    //             <original body>
    //         }
    //     }
    TIntermBlock *newBody = new TIntermBlock();
    newBody->appendStatement(CreateBuiltInFunctionCallNode("loopForwardProgress", {}, *mSymbolTable,
                                                           kESSLInternalBackendBuiltIns));
    newBody->appendStatement(node->getBody());
    node->setBody(newBody);
}

void EnsureLoopForwardProgressTraverser::ensureProgressWithCounter(TIntermLoop *node)
{
    // Turn the loop into the following:
    //
    //     highp int progressCounter = INT_MIN;
    //     <loop> {
    //         ++progressCounter;
    //         if (progressCounter == INT_MAX)
    //         {
    //             break;
    //         }
    //         {
    //             <original body>
    //         }
    //     }
    //
    // A 32-bit counter is sufficient, as 4 billions iterations would most likely times out on the
    // GPU before it's reached.
    TIntermDeclaration *declaration = nullptr;
    const TVariable *progressCounter =
        DeclareTempVariable(mSymbolTable, CreateIndexNode(std::numeric_limits<int32_t>::min()),
                            EvqTemporary, &declaration);
    TIntermSymbol *progressCounterSymbol = new TIntermSymbol(progressCounter);

    insertStatementInParentBlock(declaration);

    TIntermBlock *ifBody = new TIntermBlock();
    ifBody->appendStatement(new TIntermBranch(EOpBreak, nullptr));
    TIntermBinary *ifCondition =
        new TIntermBinary(EOpEqual, progressCounterSymbol->deepCopy(),
                          CreateIndexNode(std::numeric_limits<int32_t>::max()));

    TIntermBlock *newBody = new TIntermBlock();
    newBody->appendStatement(new TIntermUnary(EOpPreIncrement, progressCounterSymbol, nullptr));
    newBody->appendStatement(new TIntermIfElse(ifCondition, ifBody, nullptr));
    newBody->appendStatement(node->getBody());
    node->setBody(newBody);
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

bool EnsureLoopForwardProgress(TCompiler *compiler,
                               TIntermNode *root,
                               EnsureLoopForwardProgressMode mode)
{
    EnsureLoopForwardProgressTraverser traverser(&compiler->getSymbolTable(), mode);
    root->traverse(&traverser);
    return traverser.updateTree(compiler, root);
}

}  // namespace sh
