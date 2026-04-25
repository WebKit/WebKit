//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RewriteMixedTypeMathExprs: Some mixed-type arithmetic is legal in GLSL but not
// WGSL. Generate code to perform the arithmetic as specified in GLSL.
//

#include "compiler/translator/tree_ops/wgsl/RewriteMixedTypeMathExprs.h"

#include "common/log_utils.h"
#include "compiler/translator/BaseTypes.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/Operator_autogen.h"
#include "compiler/translator/tree_util/DriverUniform.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"
#include "compiler/translator/util.h"

namespace sh
{
namespace
{

class MixedTypeMathExprTraverser : public TIntermTraverser
{
  public:
    MixedTypeMathExprTraverser(TSymbolTable *symbolTable)
        : TIntermTraverser(true, false, false, symbolTable)
    {}

    bool visitBinary(Visit visit, TIntermBinary *node) override;

    bool update(TCompiler *compiler, TIntermBlock *root);

  private:
    // Create a helper function that will just construct a matrix with every element set to the
    // scalar.
    const TFunction *createMatrixConstructorHelper(const TType &matrixType)
    {
        ASSERT(matrixType.isMatrix());

        TType *retType = new TType(matrixType);
        retType->setQualifier(EvqTemporary);

        TFunction *helper = new TFunction(mSymbolTable, kEmptyImmutableString,
                                          SymbolType::AngleInternal, retType, true);

        TType *argType = new TType(matrixType);
        argType->toComponentType();
        argType->setQualifier(EvqParamIn);

        TVariable *argVar =
            new TVariable(mSymbolTable, kEmptyImmutableString, argType, SymbolType::AngleInternal);
        helper->addParameter(argVar);

        TIntermSequence constructorArgs;
        TIntermTyped *scalar = new TIntermSymbol(argVar);

        // Create a matrix with every element set to the scalar.
        for (uint8_t i = 0; i < matrixType.getCols() * matrixType.getRows(); ++i)
        {
            constructorArgs.push_back(CastScalar(matrixType, scalar->deepCopy()));
        }

        TIntermBlock *body = new TIntermBlock;
        body->appendStatement(new TIntermBranch(
            EOpReturn, TIntermAggregate::CreateConstructor(matrixType, &constructorArgs)));

        mFunctionsToAdd.push_back(
            new TIntermFunctionDefinition(new TIntermFunctionPrototype(helper), body));

        return helper;
    }

    // Converts a scalar into a nonscalar type that has every element set to be the scalar.
    // This guarantees that `scalar` is added as a child of the returned node, so
    // queueReplacementWithParent() can use BECOMES_CHILD and it will automatically be traversed by
    // this traverser.
    TIntermNode *convertScalarToNonScalar(const TType &nonScalarType, TIntermTyped *scalar)
    {
        ASSERT(!nonScalarType.isScalar());
        ASSERT(scalar->isScalar());

        if (nonScalarType.isVector())
        {
            // In WGSL, vectors have constructors that take a single scalar and fill the vector with
            // that scalar.
            return TIntermAggregate::CreateConstructor(nonScalarType, {scalar});
        }
        else if (nonScalarType.isMatrix())
        {
            // In WGSL, matrices do not have constructors that take a single scalar at all, and in
            // the future if they did, they would probably only initialize the diagonal. So, create
            // a helper function that does this.
            const TFunction *helper = createMatrixConstructorHelper(nonScalarType);
            return TIntermAggregate::CreateFunctionCall(*helper, new TIntermSequence{scalar});
        }
        else
        {
            UNREACHABLE();
            return nullptr;
        }
    }

    TIntermSequence mFunctionsToAdd;
};

bool IsMixedTypeOkayInWgsl(const TType &nonScalarType, TOperator op)
{
    // https://www.w3.org/TR/WGSL/#arithmetic-expr:~:text=arithmetic%20expressions%20with%20mixed
    if (nonScalarType.isVector())
    {
        switch (op)
        {
            case EOpAdd:
            case EOpAddAssign:
            case EOpSub:
            case EOpSubAssign:
            case EOpMul:
            case EOpMulAssign:
            case EOpDiv:
            case EOpDivAssign:
            case EOpIMod:
            case EOpIModAssign:
            case EOpVectorTimesScalar:
            case EOpVectorTimesScalarAssign:
                return true;
            default:
                return false;
        }
    }

    if (nonScalarType.isMatrix() && op == EOpMatrixTimesScalar)
    {
        return true;
    }

    return false;
}

bool MixedTypeMathExprTraverser::visitBinary(Visit visit, TIntermBinary *binNode)
{
    switch (binNode->getOp())
    {
        // All of the following can operate on mixed types.
        case EOpAdd:
        case EOpSub:
        case EOpMul:
        case EOpDiv:
        case EOpIMod:
        case EOpVectorTimesScalar:
        case EOpBitShiftLeft:
        case EOpBitShiftRight:
        case EOpBitwiseAnd:
        case EOpBitwiseXor:
        case EOpBitwiseOr:
        // Assignments work the same.
        case EOpAddAssign:
        case EOpSubAssign:
        case EOpMulAssign:
        case EOpDivAssign:
        case EOpIModAssign:
        case EOpVectorTimesScalarAssign:
        case EOpBitShiftLeftAssign:
        case EOpBitShiftRightAssign:
        case EOpBitwiseAndAssign:
        case EOpBitwiseXorAssign:
        case EOpBitwiseOrAssign:
        {
            TIntermTyped *scalarNode    = nullptr;
            TIntermTyped *nonScalarNode = nullptr;
            if (binNode->getLeft()->isScalar() && !binNode->getRight()->isScalar())
            {
                scalarNode    = binNode->getLeft();
                nonScalarNode = binNode->getRight();
            }
            else if (!binNode->getLeft()->isScalar() && binNode->getRight()->isScalar())
            {
                nonScalarNode = binNode->getLeft();
                scalarNode    = binNode->getRight();
            }
            else
            {
                break;
            }

            if (IsMixedTypeOkayInWgsl(nonScalarNode->getType(), binNode->getOp()))
            {
                break;
            }

            // TODO(anglebug.com/42267100): WGSL does not support component-wise matrix division.
            if (nonScalarNode->isMatrix() &&
                (binNode->getOp() == EOpDiv || binNode->getOp() == EOpDivAssign))
            {
                UNIMPLEMENTED();
            }

            TIntermNode *nonScalarConstructor =
                convertScalarToNonScalar(nonScalarNode->getType(), scalarNode);
            queueReplacementWithParent(binNode, scalarNode, nonScalarConstructor,
                                       OriginalNode::BECOMES_CHILD);

            break;
        }

        // All legal in WGSL:
        case EOpVectorTimesMatrix:
        case EOpMatrixTimesVector:
        case EOpMatrixTimesScalar:
        case EOpMatrixTimesMatrix:
        case EOpVectorTimesMatrixAssign:
        case EOpMatrixTimesScalarAssign:
        case EOpMatrixTimesMatrixAssign:
            break;

        // The types must always match for both operands in GLSL comparisons.
        case EOpEqual:
        case EOpNotEqual:
        case EOpLessThan:
        case EOpGreaterThan:
        case EOpLessThanEqual:
        case EOpGreaterThanEqual:
            ASSERT(binNode->getLeft()->getType() == binNode->getRight()->getType());
            break;
        // Only operate on booleans.
        case EOpLogicalOr:
        case EOpLogicalXor:
        case EOpLogicalAnd:
        case EOpLogicalNot:
            ASSERT(binNode->getLeft()->getBasicType() == EbtBool &&
                   binNode->getRight()->getBasicType() == EbtBool);
            break;
        default:
            return true;
    }

    return true;
}

bool MixedTypeMathExprTraverser::update(TCompiler *compiler, TIntermBlock *root)
{
    // Insert any added function definitions at the tope of the block
    root->insertChildNodes(0, mFunctionsToAdd);

    // Apply updates and validate
    return updateTree(compiler, root);
}
}  // anonymous namespace

bool RewriteMixedTypeMathExprs(TCompiler *compiler, TIntermBlock *root)
{
    MixedTypeMathExprTraverser traverser(&compiler->getSymbolTable());
    root->traverse(&traverser);
    return traverser.update(compiler, root);
}
}  // namespace sh
