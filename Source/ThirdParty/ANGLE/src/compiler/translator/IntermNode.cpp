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
#include <math.h>
#include <stdlib.h>
#include <algorithm>
#include <vector>

#include "common/mathutil.h"
#include "common/matrix_utils.h"
#include "compiler/translator/Diagnostics.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/util.h"

namespace sh
{

namespace
{

const float kPi                         = 3.14159265358979323846f;
const float kDegreesToRadiansMultiplier = kPi / 180.0f;
const float kRadiansToDegreesMultiplier = 180.0f / kPi;

TPrecision GetHigherPrecision(TPrecision left, TPrecision right)
{
    return left > right ? left : right;
}

TConstantUnion *Vectorize(const TConstantUnion &constant, size_t size)
{
    TConstantUnion *constUnion = new TConstantUnion[size];
    for (unsigned int i = 0; i < size; ++i)
        constUnion[i]   = constant;

    return constUnion;
}

void UndefinedConstantFoldingError(const TSourceLoc &loc,
                                   TOperator op,
                                   TBasicType basicType,
                                   TDiagnostics *diagnostics,
                                   TConstantUnion *result)
{
    diagnostics->warning(loc, "operation result is undefined for the values passed in",
                         GetOperatorString(op));

    switch (basicType)
    {
        case EbtFloat:
            result->setFConst(0.0f);
            break;
        case EbtInt:
            result->setIConst(0);
            break;
        case EbtUInt:
            result->setUConst(0u);
            break;
        case EbtBool:
            result->setBConst(false);
            break;
        default:
            break;
    }
}

float VectorLength(const TConstantUnion *paramArray, size_t paramArraySize)
{
    float result = 0.0f;
    for (size_t i = 0; i < paramArraySize; i++)
    {
        float f = paramArray[i].getFConst();
        result += f * f;
    }
    return sqrtf(result);
}

float VectorDotProduct(const TConstantUnion *paramArray1,
                       const TConstantUnion *paramArray2,
                       size_t paramArraySize)
{
    float result = 0.0f;
    for (size_t i = 0; i < paramArraySize; i++)
        result += paramArray1[i].getFConst() * paramArray2[i].getFConst();
    return result;
}

TIntermTyped *CreateFoldedNode(const TConstantUnion *constArray,
                               const TIntermTyped *originalNode,
                               TQualifier qualifier)
{
    if (constArray == nullptr)
    {
        return nullptr;
    }
    TIntermTyped *folded = new TIntermConstantUnion(constArray, originalNode->getType());
    folded->getTypePointer()->setQualifier(qualifier);
    folded->setLine(originalNode->getLine());
    return folded;
}

angle::Matrix<float> GetMatrix(const TConstantUnion *paramArray,
                               const unsigned int &rows,
                               const unsigned int &cols)
{
    std::vector<float> elements;
    for (size_t i = 0; i < rows * cols; i++)
        elements.push_back(paramArray[i].getFConst());
    // Transpose is used since the Matrix constructor expects arguments in row-major order,
    // whereas the paramArray is in column-major order. Rows/cols parameters are also flipped below
    // so that the created matrix will have the expected dimensions after the transpose.
    return angle::Matrix<float>(elements, cols, rows).transpose();
}

angle::Matrix<float> GetMatrix(const TConstantUnion *paramArray, const unsigned int &size)
{
    std::vector<float> elements;
    for (size_t i = 0; i < size * size; i++)
        elements.push_back(paramArray[i].getFConst());
    // Transpose is used since the Matrix constructor expects arguments in row-major order,
    // whereas the paramArray is in column-major order.
    return angle::Matrix<float>(elements, size).transpose();
}

void SetUnionArrayFromMatrix(const angle::Matrix<float> &m, TConstantUnion *resultArray)
{
    // Transpose is used since the input Matrix is in row-major order,
    // whereas the actual result should be in column-major order.
    angle::Matrix<float> result       = m.transpose();
    std::vector<float> resultElements = result.elements();
    for (size_t i = 0; i < resultElements.size(); i++)
        resultArray[i].setFConst(resultElements[i]);
}

}  // namespace anonymous

////////////////////////////////////////////////////////////////
//
// Member functions of the nodes used for building the tree.
//
////////////////////////////////////////////////////////////////

void TIntermTyped::setTypePreservePrecision(const TType &t)
{
    TPrecision precision = getPrecision();
    mType                = t;
    ASSERT(mType.getBasicType() != EbtBool || precision == EbpUndefined);
    mType.setPrecision(precision);
}

#define REPLACE_IF_IS(node, type, original, replacement) \
    if (node == original)                                \
    {                                                    \
        node = static_cast<type *>(replacement);         \
        return true;                                     \
    }

bool TIntermLoop::replaceChildNode(TIntermNode *original, TIntermNode *replacement)
{
    ASSERT(original != nullptr);  // This risks replacing multiple children.
    REPLACE_IF_IS(mInit, TIntermNode, original, replacement);
    REPLACE_IF_IS(mCond, TIntermTyped, original, replacement);
    REPLACE_IF_IS(mExpr, TIntermTyped, original, replacement);
    REPLACE_IF_IS(mBody, TIntermBlock, original, replacement);
    return false;
}

bool TIntermBranch::replaceChildNode(TIntermNode *original, TIntermNode *replacement)
{
    REPLACE_IF_IS(mExpression, TIntermTyped, original, replacement);
    return false;
}

bool TIntermSwizzle::replaceChildNode(TIntermNode *original, TIntermNode *replacement)
{
    ASSERT(original->getAsTyped()->getType() == replacement->getAsTyped()->getType());
    REPLACE_IF_IS(mOperand, TIntermTyped, original, replacement);
    return false;
}

bool TIntermBinary::replaceChildNode(TIntermNode *original, TIntermNode *replacement)
{
    REPLACE_IF_IS(mLeft, TIntermTyped, original, replacement);
    REPLACE_IF_IS(mRight, TIntermTyped, original, replacement);
    return false;
}

bool TIntermUnary::replaceChildNode(TIntermNode *original, TIntermNode *replacement)
{
    ASSERT(original->getAsTyped()->getType() == replacement->getAsTyped()->getType());
    REPLACE_IF_IS(mOperand, TIntermTyped, original, replacement);
    return false;
}

bool TIntermInvariantDeclaration::replaceChildNode(TIntermNode *original, TIntermNode *replacement)
{
    REPLACE_IF_IS(mSymbol, TIntermSymbol, original, replacement);
    return false;
}

bool TIntermFunctionDefinition::replaceChildNode(TIntermNode *original, TIntermNode *replacement)
{
    REPLACE_IF_IS(mPrototype, TIntermFunctionPrototype, original, replacement);
    REPLACE_IF_IS(mBody, TIntermBlock, original, replacement);
    return false;
}

bool TIntermAggregate::replaceChildNode(TIntermNode *original, TIntermNode *replacement)
{
    return replaceChildNodeInternal(original, replacement);
}

bool TIntermBlock::replaceChildNode(TIntermNode *original, TIntermNode *replacement)
{
    return replaceChildNodeInternal(original, replacement);
}

bool TIntermFunctionPrototype::replaceChildNode(TIntermNode *original, TIntermNode *replacement)
{
    return replaceChildNodeInternal(original, replacement);
}

bool TIntermDeclaration::replaceChildNode(TIntermNode *original, TIntermNode *replacement)
{
    return replaceChildNodeInternal(original, replacement);
}

bool TIntermAggregateBase::replaceChildNodeInternal(TIntermNode *original, TIntermNode *replacement)
{
    for (size_t ii = 0; ii < getSequence()->size(); ++ii)
    {
        REPLACE_IF_IS((*getSequence())[ii], TIntermNode, original, replacement);
    }
    return false;
}

bool TIntermAggregateBase::replaceChildNodeWithMultiple(TIntermNode *original,
                                                        const TIntermSequence &replacements)
{
    for (auto it = getSequence()->begin(); it < getSequence()->end(); ++it)
    {
        if (*it == original)
        {
            it = getSequence()->erase(it);
            getSequence()->insert(it, replacements.begin(), replacements.end());
            return true;
        }
    }
    return false;
}

bool TIntermAggregateBase::insertChildNodes(TIntermSequence::size_type position,
                                            const TIntermSequence &insertions)
{
    if (position > getSequence()->size())
    {
        return false;
    }
    auto it = getSequence()->begin() + position;
    getSequence()->insert(it, insertions.begin(), insertions.end());
    return true;
}

TIntermAggregate *TIntermAggregate::CreateFunctionCall(const TFunction &func,
                                                       TIntermSequence *arguments)
{
    TIntermAggregate *callNode =
        new TIntermAggregate(func.getReturnType(), EOpCallFunctionInAST, arguments);
    callNode->getFunctionSymbolInfo()->setFromFunction(func);
    return callNode;
}

TIntermAggregate *TIntermAggregate::CreateFunctionCall(const TType &type,
                                                       const TSymbolUniqueId &id,
                                                       const TName &name,
                                                       TIntermSequence *arguments)
{
    TIntermAggregate *callNode = new TIntermAggregate(type, EOpCallFunctionInAST, arguments);
    callNode->getFunctionSymbolInfo()->setId(id);
    callNode->getFunctionSymbolInfo()->setNameObj(name);
    return callNode;
}

TIntermAggregate *TIntermAggregate::CreateBuiltInFunctionCall(const TFunction &func,
                                                              TIntermSequence *arguments)
{
    TIntermAggregate *callNode =
        new TIntermAggregate(func.getReturnType(), EOpCallBuiltInFunction, arguments);
    callNode->getFunctionSymbolInfo()->setFromFunction(func);
    // Note that name needs to be set before texture function type is determined.
    callNode->setBuiltInFunctionPrecision();
    return callNode;
}

TIntermAggregate *TIntermAggregate::CreateConstructor(const TType &type,
                                                      TIntermSequence *arguments)
{
    return new TIntermAggregate(type, EOpConstruct, arguments);
}

TIntermAggregate *TIntermAggregate::Create(const TType &type,
                                           TOperator op,
                                           TIntermSequence *arguments)
{
    TIntermAggregate *node = new TIntermAggregate(type, op, arguments);
    ASSERT(op != EOpCallFunctionInAST);    // Should use CreateFunctionCall
    ASSERT(op != EOpCallBuiltInFunction);  // Should use CreateBuiltInFunctionCall
    ASSERT(!node->isConstructor());        // Should use CreateConstructor
    return node;
}

TIntermAggregate::TIntermAggregate(const TType &type, TOperator op, TIntermSequence *arguments)
    : TIntermOperator(op), mUseEmulatedFunction(false), mGotPrecisionFromChildren(false)
{
    if (arguments != nullptr)
    {
        mArguments.swap(*arguments);
    }
    setTypePrecisionAndQualifier(type);
}

void TIntermAggregate::setTypePrecisionAndQualifier(const TType &type)
{
    setType(type);
    mType.setQualifier(EvqTemporary);
    if (!isFunctionCall())
    {
        if (isConstructor())
        {
            // Structs should not be precision qualified, the individual members may be.
            // Built-in types on the other hand should be precision qualified.
            if (getBasicType() != EbtStruct)
            {
                setPrecisionFromChildren();
            }
        }
        else
        {
            setPrecisionForBuiltInOp();
        }
        if (areChildrenConstQualified())
        {
            mType.setQualifier(EvqConst);
        }
    }
}

bool TIntermAggregate::areChildrenConstQualified()
{
    for (TIntermNode *&arg : mArguments)
    {
        TIntermTyped *typedArg = arg->getAsTyped();
        if (typedArg && typedArg->getQualifier() != EvqConst)
        {
            return false;
        }
    }
    return true;
}

void TIntermAggregate::setPrecisionFromChildren()
{
    mGotPrecisionFromChildren = true;
    if (getBasicType() == EbtBool)
    {
        mType.setPrecision(EbpUndefined);
        return;
    }

    TPrecision precision                = EbpUndefined;
    TIntermSequence::iterator childIter = mArguments.begin();
    while (childIter != mArguments.end())
    {
        TIntermTyped *typed = (*childIter)->getAsTyped();
        if (typed)
            precision = GetHigherPrecision(typed->getPrecision(), precision);
        ++childIter;
    }
    mType.setPrecision(precision);
}

void TIntermAggregate::setPrecisionForBuiltInOp()
{
    ASSERT(!isConstructor());
    ASSERT(!isFunctionCall());
    if (!setPrecisionForSpecialBuiltInOp())
    {
        setPrecisionFromChildren();
    }
}

bool TIntermAggregate::setPrecisionForSpecialBuiltInOp()
{
    switch (mOp)
    {
        case EOpBitfieldExtract:
            mType.setPrecision(mArguments[0]->getAsTyped()->getPrecision());
            mGotPrecisionFromChildren = true;
            return true;
        case EOpBitfieldInsert:
            mType.setPrecision(GetHigherPrecision(mArguments[0]->getAsTyped()->getPrecision(),
                                                  mArguments[1]->getAsTyped()->getPrecision()));
            mGotPrecisionFromChildren = true;
            return true;
        case EOpUaddCarry:
        case EOpUsubBorrow:
            mType.setPrecision(EbpHigh);
            return true;
        default:
            return false;
    }
}

void TIntermAggregate::setBuiltInFunctionPrecision()
{
    // All built-ins returning bool should be handled as ops, not functions.
    ASSERT(getBasicType() != EbtBool);
    ASSERT(mOp == EOpCallBuiltInFunction);

    TPrecision precision = EbpUndefined;
    for (TIntermNode *arg : mArguments)
    {
        TIntermTyped *typed = arg->getAsTyped();
        // ESSL spec section 8: texture functions get their precision from the sampler.
        if (typed && IsSampler(typed->getBasicType()))
        {
            precision = typed->getPrecision();
            break;
        }
    }
    // ESSL 3.0 spec section 8: textureSize always gets highp precision.
    // All other functions that take a sampler are assumed to be texture functions.
    if (mFunctionInfo.getName().find("textureSize") == 0)
        mType.setPrecision(EbpHigh);
    else
        mType.setPrecision(precision);
}

TString TIntermAggregate::getSymbolTableMangledName() const
{
    ASSERT(!isConstructor());
    switch (mOp)
    {
        case EOpCallInternalRawFunction:
        case EOpCallBuiltInFunction:
        case EOpCallFunctionInAST:
            return TFunction::GetMangledNameFromCall(mFunctionInfo.getName(), mArguments);
        default:
            TString opString = GetOperatorString(mOp);
            return TFunction::GetMangledNameFromCall(opString, mArguments);
    }
}

bool TIntermAggregate::hasSideEffects() const
{
    if (isFunctionCall() && mFunctionInfo.isKnownToNotHaveSideEffects())
    {
        for (TIntermNode *arg : mArguments)
        {
            if (arg->getAsTyped()->hasSideEffects())
            {
                return true;
            }
        }
        return false;
    }
    // Conservatively assume most aggregate operators have side-effects
    return true;
}

void TIntermBlock::appendStatement(TIntermNode *statement)
{
    // Declaration nodes with no children can appear if it was an empty declaration or if all the
    // declarators just added constants to the symbol table instead of generating code. We still
    // need to add the declaration to the AST in that case because it might be relevant to the
    // validity of switch/case.
    if (statement != nullptr)
    {
        mStatements.push_back(statement);
    }
}

void TIntermFunctionPrototype::appendParameter(TIntermSymbol *parameter)
{
    ASSERT(parameter != nullptr);
    mParameters.push_back(parameter);
}

void TIntermDeclaration::appendDeclarator(TIntermTyped *declarator)
{
    ASSERT(declarator != nullptr);
    ASSERT(declarator->getAsSymbolNode() != nullptr ||
           (declarator->getAsBinaryNode() != nullptr &&
            declarator->getAsBinaryNode()->getOp() == EOpInitialize));
    ASSERT(mDeclarators.empty() ||
           declarator->getType().sameNonArrayType(mDeclarators.back()->getAsTyped()->getType()));
    mDeclarators.push_back(declarator);
}

bool TIntermTernary::replaceChildNode(TIntermNode *original, TIntermNode *replacement)
{
    REPLACE_IF_IS(mCondition, TIntermTyped, original, replacement);
    REPLACE_IF_IS(mTrueExpression, TIntermTyped, original, replacement);
    REPLACE_IF_IS(mFalseExpression, TIntermTyped, original, replacement);
    return false;
}

bool TIntermIfElse::replaceChildNode(TIntermNode *original, TIntermNode *replacement)
{
    REPLACE_IF_IS(mCondition, TIntermTyped, original, replacement);
    REPLACE_IF_IS(mTrueBlock, TIntermBlock, original, replacement);
    REPLACE_IF_IS(mFalseBlock, TIntermBlock, original, replacement);
    return false;
}

bool TIntermSwitch::replaceChildNode(TIntermNode *original, TIntermNode *replacement)
{
    REPLACE_IF_IS(mInit, TIntermTyped, original, replacement);
    REPLACE_IF_IS(mStatementList, TIntermBlock, original, replacement);
    ASSERT(mStatementList);
    return false;
}

bool TIntermCase::replaceChildNode(TIntermNode *original, TIntermNode *replacement)
{
    REPLACE_IF_IS(mCondition, TIntermTyped, original, replacement);
    return false;
}

TIntermTyped::TIntermTyped(const TIntermTyped &node) : TIntermNode(), mType(node.mType)
{
    // Copy constructor is disallowed for TIntermNode in order to disallow it for subclasses that
    // don't explicitly allow it, so normal TIntermNode constructor is used to construct the copy.
    // We need to manually copy any fields of TIntermNode besides handling fields in TIntermTyped.
    mLine = node.mLine;
}

bool TIntermTyped::isConstructorWithOnlyConstantUnionParameters()
{
    TIntermAggregate *constructor = getAsAggregate();
    if (!constructor || !constructor->isConstructor())
    {
        return false;
    }
    for (TIntermNode *&node : *constructor->getSequence())
    {
        if (!node->getAsConstantUnion())
            return false;
    }
    return true;
}

TIntermConstantUnion::TIntermConstantUnion(const TIntermConstantUnion &node) : TIntermTyped(node)
{
    mUnionArrayPointer = node.mUnionArrayPointer;
}

void TFunctionSymbolInfo::setFromFunction(const TFunction &function)
{
    setName(function.getName());
    setId(TSymbolUniqueId(function));
}

TFunctionSymbolInfo::TFunctionSymbolInfo(const TSymbolUniqueId &id)
    : mId(new TSymbolUniqueId(id)), mKnownToNotHaveSideEffects(false)
{
}

TFunctionSymbolInfo::TFunctionSymbolInfo(const TFunctionSymbolInfo &info)
    : mName(info.mName), mId(nullptr), mKnownToNotHaveSideEffects(info.mKnownToNotHaveSideEffects)
{
    if (info.mId)
    {
        mId = new TSymbolUniqueId(*info.mId);
    }
}

TFunctionSymbolInfo &TFunctionSymbolInfo::operator=(const TFunctionSymbolInfo &info)
{
    mName = info.mName;
    if (info.mId)
    {
        mId = new TSymbolUniqueId(*info.mId);
    }
    else
    {
        mId = nullptr;
    }
    return *this;
}

void TFunctionSymbolInfo::setId(const TSymbolUniqueId &id)
{
    mId = new TSymbolUniqueId(id);
}

const TSymbolUniqueId &TFunctionSymbolInfo::getId() const
{
    ASSERT(mId);
    return *mId;
}

TIntermAggregate::TIntermAggregate(const TIntermAggregate &node)
    : TIntermOperator(node),
      mUseEmulatedFunction(node.mUseEmulatedFunction),
      mGotPrecisionFromChildren(node.mGotPrecisionFromChildren),
      mFunctionInfo(node.mFunctionInfo)
{
    for (TIntermNode *arg : node.mArguments)
    {
        TIntermTyped *typedArg = arg->getAsTyped();
        ASSERT(typedArg != nullptr);
        TIntermTyped *argCopy = typedArg->deepCopy();
        mArguments.push_back(argCopy);
    }
}

TIntermAggregate *TIntermAggregate::shallowCopy() const
{
    TIntermSequence *copySeq = new TIntermSequence();
    copySeq->insert(copySeq->begin(), getSequence()->begin(), getSequence()->end());
    TIntermAggregate *copyNode         = new TIntermAggregate(mType, mOp, copySeq);
    *copyNode->getFunctionSymbolInfo() = mFunctionInfo;
    copyNode->setLine(mLine);
    return copyNode;
}

TIntermSwizzle::TIntermSwizzle(const TIntermSwizzle &node) : TIntermTyped(node)
{
    TIntermTyped *operandCopy = node.mOperand->deepCopy();
    ASSERT(operandCopy != nullptr);
    mOperand = operandCopy;
    mSwizzleOffsets = node.mSwizzleOffsets;
}

TIntermBinary::TIntermBinary(const TIntermBinary &node)
    : TIntermOperator(node), mAddIndexClamp(node.mAddIndexClamp)
{
    TIntermTyped *leftCopy  = node.mLeft->deepCopy();
    TIntermTyped *rightCopy = node.mRight->deepCopy();
    ASSERT(leftCopy != nullptr && rightCopy != nullptr);
    mLeft  = leftCopy;
    mRight = rightCopy;
}

TIntermUnary::TIntermUnary(const TIntermUnary &node)
    : TIntermOperator(node), mUseEmulatedFunction(node.mUseEmulatedFunction)
{
    TIntermTyped *operandCopy = node.mOperand->deepCopy();
    ASSERT(operandCopy != nullptr);
    mOperand = operandCopy;
}

TIntermTernary::TIntermTernary(const TIntermTernary &node) : TIntermTyped(node)
{
    TIntermTyped *conditionCopy = node.mCondition->deepCopy();
    TIntermTyped *trueCopy      = node.mTrueExpression->deepCopy();
    TIntermTyped *falseCopy     = node.mFalseExpression->deepCopy();
    ASSERT(conditionCopy != nullptr && trueCopy != nullptr && falseCopy != nullptr);
    mCondition       = conditionCopy;
    mTrueExpression  = trueCopy;
    mFalseExpression = falseCopy;
}

bool TIntermOperator::isAssignment() const
{
    return IsAssignment(mOp);
}

bool TIntermOperator::isMultiplication() const
{
    switch (mOp)
    {
        case EOpMul:
        case EOpMatrixTimesMatrix:
        case EOpMatrixTimesVector:
        case EOpMatrixTimesScalar:
        case EOpVectorTimesMatrix:
        case EOpVectorTimesScalar:
            return true;
        default:
            return false;
    }
}

bool TIntermOperator::isConstructor() const
{
    return (mOp == EOpConstruct);
}

bool TIntermOperator::isFunctionCall() const
{
    switch (mOp)
    {
        case EOpCallFunctionInAST:
        case EOpCallBuiltInFunction:
        case EOpCallInternalRawFunction:
            return true;
        default:
            return false;
    }
}

TOperator TIntermBinary::GetMulOpBasedOnOperands(const TType &left, const TType &right)
{
    if (left.isMatrix())
    {
        if (right.isMatrix())
        {
            return EOpMatrixTimesMatrix;
        }
        else
        {
            if (right.isVector())
            {
                return EOpMatrixTimesVector;
            }
            else
            {
                return EOpMatrixTimesScalar;
            }
        }
    }
    else
    {
        if (right.isMatrix())
        {
            if (left.isVector())
            {
                return EOpVectorTimesMatrix;
            }
            else
            {
                return EOpMatrixTimesScalar;
            }
        }
        else
        {
            // Neither operand is a matrix.
            if (left.isVector() == right.isVector())
            {
                // Leave as component product.
                return EOpMul;
            }
            else
            {
                return EOpVectorTimesScalar;
            }
        }
    }
}

TOperator TIntermBinary::GetMulAssignOpBasedOnOperands(const TType &left, const TType &right)
{
    if (left.isMatrix())
    {
        if (right.isMatrix())
        {
            return EOpMatrixTimesMatrixAssign;
        }
        else
        {
            // right should be scalar, but this may not be validated yet.
            return EOpMatrixTimesScalarAssign;
        }
    }
    else
    {
        if (right.isMatrix())
        {
            // Left should be a vector, but this may not be validated yet.
            return EOpVectorTimesMatrixAssign;
        }
        else
        {
            // Neither operand is a matrix.
            if (left.isVector() == right.isVector())
            {
                // Leave as component product.
                return EOpMulAssign;
            }
            else
            {
                // left should be vector and right should be scalar, but this may not be validated
                // yet.
                return EOpVectorTimesScalarAssign;
            }
        }
    }
}

//
// Make sure the type of a unary operator is appropriate for its
// combination of operation and operand type.
//
void TIntermUnary::promote()
{
    if (mOp == EOpArrayLength)
    {
        // Special case: the qualifier of .length() doesn't depend on the operand qualifier.
        setType(TType(EbtInt, EbpUndefined, EvqConst));
        return;
    }

    TQualifier resultQualifier = EvqTemporary;
    if (mOperand->getQualifier() == EvqConst)
        resultQualifier = EvqConst;

    unsigned char operandPrimarySize =
        static_cast<unsigned char>(mOperand->getType().getNominalSize());
    switch (mOp)
    {
        case EOpFloatBitsToInt:
            setType(TType(EbtInt, EbpHigh, resultQualifier, operandPrimarySize));
            break;
        case EOpFloatBitsToUint:
            setType(TType(EbtUInt, EbpHigh, resultQualifier, operandPrimarySize));
            break;
        case EOpIntBitsToFloat:
        case EOpUintBitsToFloat:
            setType(TType(EbtFloat, EbpHigh, resultQualifier, operandPrimarySize));
            break;
        case EOpPackSnorm2x16:
        case EOpPackUnorm2x16:
        case EOpPackHalf2x16:
        case EOpPackUnorm4x8:
        case EOpPackSnorm4x8:
            setType(TType(EbtUInt, EbpHigh, resultQualifier));
            break;
        case EOpUnpackSnorm2x16:
        case EOpUnpackUnorm2x16:
            setType(TType(EbtFloat, EbpHigh, resultQualifier, 2));
            break;
        case EOpUnpackHalf2x16:
            setType(TType(EbtFloat, EbpMedium, resultQualifier, 2));
            break;
        case EOpUnpackUnorm4x8:
        case EOpUnpackSnorm4x8:
            setType(TType(EbtFloat, EbpMedium, resultQualifier, 4));
            break;
        case EOpAny:
        case EOpAll:
            setType(TType(EbtBool, EbpUndefined, resultQualifier));
            break;
        case EOpLength:
        case EOpDeterminant:
            setType(TType(EbtFloat, mOperand->getType().getPrecision(), resultQualifier));
            break;
        case EOpTranspose:
            setType(TType(EbtFloat, mOperand->getType().getPrecision(), resultQualifier,
                          static_cast<unsigned char>(mOperand->getType().getRows()),
                          static_cast<unsigned char>(mOperand->getType().getCols())));
            break;
        case EOpIsInf:
        case EOpIsNan:
            setType(TType(EbtBool, EbpUndefined, resultQualifier, operandPrimarySize));
            break;
        case EOpBitfieldReverse:
            setType(TType(mOperand->getBasicType(), EbpHigh, resultQualifier, operandPrimarySize));
            break;
        case EOpBitCount:
            setType(TType(EbtInt, EbpLow, resultQualifier, operandPrimarySize));
            break;
        case EOpFindLSB:
            setType(TType(EbtInt, EbpLow, resultQualifier, operandPrimarySize));
            break;
        case EOpFindMSB:
            setType(TType(EbtInt, EbpLow, resultQualifier, operandPrimarySize));
            break;
        default:
            setType(mOperand->getType());
            mType.setQualifier(resultQualifier);
            break;
    }
}

TIntermSwizzle::TIntermSwizzle(TIntermTyped *operand, const TVector<int> &swizzleOffsets)
    : TIntermTyped(TType(EbtFloat, EbpUndefined)),
      mOperand(operand),
      mSwizzleOffsets(swizzleOffsets)
{
    ASSERT(mSwizzleOffsets.size() <= 4);
    promote();
}

TIntermUnary::TIntermUnary(TOperator op, TIntermTyped *operand)
    : TIntermOperator(op), mOperand(operand), mUseEmulatedFunction(false)
{
    promote();
}

TIntermBinary::TIntermBinary(TOperator op, TIntermTyped *left, TIntermTyped *right)
    : TIntermOperator(op), mLeft(left), mRight(right), mAddIndexClamp(false)
{
    promote();
}

TIntermInvariantDeclaration::TIntermInvariantDeclaration(TIntermSymbol *symbol, const TSourceLoc &line)
    : TIntermNode(), mSymbol(symbol)
{
    ASSERT(symbol);
    setLine(line);
}

TIntermTernary::TIntermTernary(TIntermTyped *cond,
                               TIntermTyped *trueExpression,
                               TIntermTyped *falseExpression)
    : TIntermTyped(trueExpression->getType()),
      mCondition(cond),
      mTrueExpression(trueExpression),
      mFalseExpression(falseExpression)
{
    getTypePointer()->setQualifier(
        TIntermTernary::DetermineQualifier(cond, trueExpression, falseExpression));
}

TIntermLoop::TIntermLoop(TLoopType type,
                         TIntermNode *init,
                         TIntermTyped *cond,
                         TIntermTyped *expr,
                         TIntermBlock *body)
    : mType(type), mInit(init), mCond(cond), mExpr(expr), mBody(body)
{
    // Declaration nodes with no children can appear if all the declarators just added constants to
    // the symbol table instead of generating code. They're no-ops so don't add them to the tree.
    if (mInit && mInit->getAsDeclarationNode() &&
        mInit->getAsDeclarationNode()->getSequence()->empty())
    {
        mInit = nullptr;
    }
}

TIntermIfElse::TIntermIfElse(TIntermTyped *cond, TIntermBlock *trueB, TIntermBlock *falseB)
    : TIntermNode(), mCondition(cond), mTrueBlock(trueB), mFalseBlock(falseB)
{
    // Prune empty false blocks so that there won't be unnecessary operations done on it.
    if (mFalseBlock && mFalseBlock->getSequence()->empty())
    {
        mFalseBlock = nullptr;
    }
}

TIntermSwitch::TIntermSwitch(TIntermTyped *init, TIntermBlock *statementList)
    : TIntermNode(), mInit(init), mStatementList(statementList)
{
    ASSERT(mStatementList);
}

void TIntermSwitch::setStatementList(TIntermBlock *statementList)
{
    ASSERT(statementList);
    mStatementList = statementList;
}

// static
TQualifier TIntermTernary::DetermineQualifier(TIntermTyped *cond,
                                              TIntermTyped *trueExpression,
                                              TIntermTyped *falseExpression)
{
    if (cond->getQualifier() == EvqConst && trueExpression->getQualifier() == EvqConst &&
        falseExpression->getQualifier() == EvqConst)
    {
        return EvqConst;
    }
    return EvqTemporary;
}

TIntermTyped *TIntermTernary::fold()
{
    if (mCondition->getAsConstantUnion())
    {
        if (mCondition->getAsConstantUnion()->getBConst(0))
        {
            mTrueExpression->getTypePointer()->setQualifier(mType.getQualifier());
            return mTrueExpression;
        }
        else
        {
            mFalseExpression->getTypePointer()->setQualifier(mType.getQualifier());
            return mFalseExpression;
        }
    }
    return this;
}

void TIntermSwizzle::promote()
{
    TQualifier resultQualifier = EvqTemporary;
    if (mOperand->getQualifier() == EvqConst)
        resultQualifier = EvqConst;

    auto numFields = mSwizzleOffsets.size();
    setType(TType(mOperand->getBasicType(), mOperand->getPrecision(), resultQualifier,
                  static_cast<unsigned char>(numFields)));
}

bool TIntermSwizzle::hasDuplicateOffsets() const
{
    int offsetCount[4] = {0u, 0u, 0u, 0u};
    for (const auto offset : mSwizzleOffsets)
    {
        offsetCount[offset]++;
        if (offsetCount[offset] > 1)
        {
            return true;
        }
    }
    return false;
}

bool TIntermSwizzle::offsetsMatch(int offset) const
{
    return mSwizzleOffsets.size() == 1 && mSwizzleOffsets[0] == offset;
}

void TIntermSwizzle::writeOffsetsAsXYZW(TInfoSinkBase *out) const
{
    for (const int offset : mSwizzleOffsets)
    {
        switch (offset)
        {
            case 0:
                *out << "x";
                break;
            case 1:
                *out << "y";
                break;
            case 2:
                *out << "z";
                break;
            case 3:
                *out << "w";
                break;
            default:
                UNREACHABLE();
        }
    }
}

TQualifier TIntermBinary::GetCommaQualifier(int shaderVersion,
                                            const TIntermTyped *left,
                                            const TIntermTyped *right)
{
    // ESSL3.00 section 12.43: The result of a sequence operator is not a constant-expression.
    if (shaderVersion >= 300 || left->getQualifier() != EvqConst ||
        right->getQualifier() != EvqConst)
    {
        return EvqTemporary;
    }
    return EvqConst;
}

// Establishes the type of the result of the binary operation.
void TIntermBinary::promote()
{
    ASSERT(!isMultiplication() ||
           mOp == GetMulOpBasedOnOperands(mLeft->getType(), mRight->getType()));

    // Comma is handled as a special case. Note that the comma node qualifier depends on the shader
    // version and so is not being set here.
    if (mOp == EOpComma)
    {
        setType(mRight->getType());
        return;
    }

    // Base assumption:  just make the type the same as the left
    // operand.  Then only deviations from this need be coded.
    setType(mLeft->getType());

    TQualifier resultQualifier = EvqConst;
    // Binary operations results in temporary variables unless both
    // operands are const.
    if (mLeft->getQualifier() != EvqConst || mRight->getQualifier() != EvqConst)
    {
        resultQualifier = EvqTemporary;
        getTypePointer()->setQualifier(EvqTemporary);
    }

    // Handle indexing ops.
    switch (mOp)
    {
        case EOpIndexDirect:
        case EOpIndexIndirect:
            if (mLeft->isArray())
            {
                mType.toArrayElementType();
            }
            else if (mLeft->isMatrix())
            {
                setType(TType(mLeft->getBasicType(), mLeft->getPrecision(), resultQualifier,
                              static_cast<unsigned char>(mLeft->getRows())));
            }
            else if (mLeft->isVector())
            {
                setType(TType(mLeft->getBasicType(), mLeft->getPrecision(), resultQualifier));
            }
            else
            {
                UNREACHABLE();
            }
            return;
        case EOpIndexDirectStruct:
        {
            const TFieldList &fields = mLeft->getType().getStruct()->fields();
            const int i              = mRight->getAsConstantUnion()->getIConst(0);
            setType(*fields[i]->type());
            getTypePointer()->setQualifier(resultQualifier);
            return;
        }
        case EOpIndexDirectInterfaceBlock:
        {
            const TFieldList &fields = mLeft->getType().getInterfaceBlock()->fields();
            const int i              = mRight->getAsConstantUnion()->getIConst(0);
            setType(*fields[i]->type());
            getTypePointer()->setQualifier(resultQualifier);
            return;
        }
        default:
            break;
    }

    ASSERT(mLeft->isArray() == mRight->isArray());

    // The result gets promoted to the highest precision.
    TPrecision higherPrecision = GetHigherPrecision(mLeft->getPrecision(), mRight->getPrecision());
    getTypePointer()->setPrecision(higherPrecision);

    const int nominalSize = std::max(mLeft->getNominalSize(), mRight->getNominalSize());

    //
    // All scalars or structs. Code after this test assumes this case is removed!
    //
    if (nominalSize == 1)
    {
        switch (mOp)
        {
            //
            // Promote to conditional
            //
            case EOpEqual:
            case EOpNotEqual:
            case EOpLessThan:
            case EOpGreaterThan:
            case EOpLessThanEqual:
            case EOpGreaterThanEqual:
                setType(TType(EbtBool, EbpUndefined, resultQualifier));
                break;

            //
            // And and Or operate on conditionals
            //
            case EOpLogicalAnd:
            case EOpLogicalXor:
            case EOpLogicalOr:
                ASSERT(mLeft->getBasicType() == EbtBool && mRight->getBasicType() == EbtBool);
                setType(TType(EbtBool, EbpUndefined, resultQualifier));
                break;

            default:
                break;
        }
        return;
    }

    // If we reach here, at least one of the operands is vector or matrix.
    // The other operand could be a scalar, vector, or matrix.
    TBasicType basicType = mLeft->getBasicType();

    switch (mOp)
    {
        case EOpMul:
            break;
        case EOpMatrixTimesScalar:
            if (mRight->isMatrix())
            {
                setType(TType(basicType, higherPrecision, resultQualifier,
                              static_cast<unsigned char>(mRight->getCols()),
                              static_cast<unsigned char>(mRight->getRows())));
            }
            break;
        case EOpMatrixTimesVector:
            setType(TType(basicType, higherPrecision, resultQualifier,
                          static_cast<unsigned char>(mLeft->getRows()), 1));
            break;
        case EOpMatrixTimesMatrix:
            setType(TType(basicType, higherPrecision, resultQualifier,
                          static_cast<unsigned char>(mRight->getCols()),
                          static_cast<unsigned char>(mLeft->getRows())));
            break;
        case EOpVectorTimesScalar:
            setType(TType(basicType, higherPrecision, resultQualifier,
                          static_cast<unsigned char>(nominalSize), 1));
            break;
        case EOpVectorTimesMatrix:
            setType(TType(basicType, higherPrecision, resultQualifier,
                          static_cast<unsigned char>(mRight->getCols()), 1));
            break;
        case EOpMulAssign:
        case EOpVectorTimesScalarAssign:
        case EOpVectorTimesMatrixAssign:
        case EOpMatrixTimesScalarAssign:
        case EOpMatrixTimesMatrixAssign:
            ASSERT(mOp == GetMulAssignOpBasedOnOperands(mLeft->getType(), mRight->getType()));
            break;
        case EOpAssign:
        case EOpInitialize:
            ASSERT((mLeft->getNominalSize() == mRight->getNominalSize()) &&
                   (mLeft->getSecondarySize() == mRight->getSecondarySize()));
            break;
        case EOpAdd:
        case EOpSub:
        case EOpDiv:
        case EOpIMod:
        case EOpBitShiftLeft:
        case EOpBitShiftRight:
        case EOpBitwiseAnd:
        case EOpBitwiseXor:
        case EOpBitwiseOr:
        case EOpAddAssign:
        case EOpSubAssign:
        case EOpDivAssign:
        case EOpIModAssign:
        case EOpBitShiftLeftAssign:
        case EOpBitShiftRightAssign:
        case EOpBitwiseAndAssign:
        case EOpBitwiseXorAssign:
        case EOpBitwiseOrAssign:
        {
            const int secondarySize =
                std::max(mLeft->getSecondarySize(), mRight->getSecondarySize());
            setType(TType(basicType, higherPrecision, resultQualifier,
                          static_cast<unsigned char>(nominalSize),
                          static_cast<unsigned char>(secondarySize)));
            ASSERT(!mLeft->isArray() && !mRight->isArray());
            break;
        }
        case EOpEqual:
        case EOpNotEqual:
        case EOpLessThan:
        case EOpGreaterThan:
        case EOpLessThanEqual:
        case EOpGreaterThanEqual:
            ASSERT((mLeft->getNominalSize() == mRight->getNominalSize()) &&
                   (mLeft->getSecondarySize() == mRight->getSecondarySize()));
            setType(TType(EbtBool, EbpUndefined, resultQualifier));
            break;

        case EOpIndexDirect:
        case EOpIndexIndirect:
        case EOpIndexDirectInterfaceBlock:
        case EOpIndexDirectStruct:
            // These ops should be already fully handled.
            UNREACHABLE();
            break;
        default:
            UNREACHABLE();
            break;
    }
}

const TConstantUnion *TIntermConstantUnion::foldIndexing(int index)
{
    if (isArray())
    {
        ASSERT(index < static_cast<int>(getType().getOutermostArraySize()));
        TType arrayElementType = getType();
        arrayElementType.toArrayElementType();
        size_t arrayElementSize = arrayElementType.getObjectSize();
        return &mUnionArrayPointer[arrayElementSize * index];
    }
    else if (isMatrix())
    {
        ASSERT(index < getType().getCols());
        int size = getType().getRows();
        return &mUnionArrayPointer[size * index];
    }
    else if (isVector())
    {
        ASSERT(index < getType().getNominalSize());
        return &mUnionArrayPointer[index];
    }
    else
    {
        UNREACHABLE();
        return nullptr;
    }
}

TIntermTyped *TIntermSwizzle::fold()
{
    TIntermConstantUnion *operandConstant = mOperand->getAsConstantUnion();
    if (operandConstant == nullptr)
    {
        return this;
    }

    TConstantUnion *constArray = new TConstantUnion[mSwizzleOffsets.size()];
    for (size_t i = 0; i < mSwizzleOffsets.size(); ++i)
    {
        constArray[i] = *operandConstant->foldIndexing(mSwizzleOffsets.at(i));
    }
    return CreateFoldedNode(constArray, this, mType.getQualifier());
}

TIntermTyped *TIntermBinary::fold(TDiagnostics *diagnostics)
{
    TIntermConstantUnion *leftConstant  = mLeft->getAsConstantUnion();
    TIntermConstantUnion *rightConstant = mRight->getAsConstantUnion();
    switch (mOp)
    {
        case EOpComma:
        {
            if (mLeft->hasSideEffects())
            {
                return this;
            }
            mRight->getTypePointer()->setQualifier(mType.getQualifier());
            return mRight;
        }
        case EOpIndexDirect:
        {
            if (leftConstant == nullptr || rightConstant == nullptr)
            {
                return this;
            }
            int index = rightConstant->getIConst(0);

            const TConstantUnion *constArray = leftConstant->foldIndexing(index);
            if (!constArray)
            {
                return this;
            }
            return CreateFoldedNode(constArray, this, mType.getQualifier());
        }
        case EOpIndexDirectStruct:
        {
            if (leftConstant == nullptr || rightConstant == nullptr)
            {
                return this;
            }
            const TFieldList &fields = mLeft->getType().getStruct()->fields();
            size_t index             = static_cast<size_t>(rightConstant->getIConst(0));

            size_t previousFieldsSize = 0;
            for (size_t i = 0; i < index; ++i)
            {
                previousFieldsSize += fields[i]->type()->getObjectSize();
            }

            const TConstantUnion *constArray = leftConstant->getUnionArrayPointer();
            return CreateFoldedNode(constArray + previousFieldsSize, this, mType.getQualifier());
        }
        case EOpIndexIndirect:
        case EOpIndexDirectInterfaceBlock:
            // Can never be constant folded.
            return this;
        default:
        {
            if (leftConstant == nullptr || rightConstant == nullptr)
            {
                return this;
            }
            TConstantUnion *constArray =
                leftConstant->foldBinary(mOp, rightConstant, diagnostics, mLeft->getLine());
            if (!constArray)
            {
                return this;
            }

            // Nodes may be constant folded without being qualified as constant.
            return CreateFoldedNode(constArray, this, mType.getQualifier());
        }
    }
}

TIntermTyped *TIntermUnary::fold(TDiagnostics *diagnostics)
{
    TConstantUnion *constArray = nullptr;

    if (mOp == EOpArrayLength)
    {
        // The size of runtime-sized arrays may only be determined at runtime.
        if (mOperand->hasSideEffects() || mOperand->getType().isUnsizedArray())
        {
            return this;
        }
        constArray = new TConstantUnion[1];
        constArray->setIConst(mOperand->getOutermostArraySize());
    }
    else
    {
        TIntermConstantUnion *operandConstant = mOperand->getAsConstantUnion();
        if (operandConstant == nullptr)
        {
            return this;
        }

        switch (mOp)
        {
            case EOpAny:
            case EOpAll:
            case EOpLength:
            case EOpTranspose:
            case EOpDeterminant:
            case EOpInverse:
            case EOpPackSnorm2x16:
            case EOpUnpackSnorm2x16:
            case EOpPackUnorm2x16:
            case EOpUnpackUnorm2x16:
            case EOpPackHalf2x16:
            case EOpUnpackHalf2x16:
            case EOpPackUnorm4x8:
            case EOpPackSnorm4x8:
            case EOpUnpackUnorm4x8:
            case EOpUnpackSnorm4x8:
                constArray = operandConstant->foldUnaryNonComponentWise(mOp);
                break;
            default:
                constArray = operandConstant->foldUnaryComponentWise(mOp, diagnostics);
                break;
        }
    }
    if (constArray == nullptr)
    {
        return this;
    }

    // Nodes may be constant folded without being qualified as constant.
    return CreateFoldedNode(constArray, this, mType.getQualifier());
}

TIntermTyped *TIntermAggregate::fold(TDiagnostics *diagnostics)
{
    // Make sure that all params are constant before actual constant folding.
    for (auto *param : *getSequence())
    {
        if (param->getAsConstantUnion() == nullptr)
        {
            return this;
        }
    }
    TConstantUnion *constArray = nullptr;
    if (isConstructor())
        constArray = TIntermConstantUnion::FoldAggregateConstructor(this);
    else
        constArray = TIntermConstantUnion::FoldAggregateBuiltIn(this, diagnostics);

    // Nodes may be constant folded without being qualified as constant.
    return CreateFoldedNode(constArray, this, getQualifier());
}

//
// The fold functions see if an operation on a constant can be done in place,
// without generating run-time code.
//
// Returns the constant value to keep using or nullptr.
//
TConstantUnion *TIntermConstantUnion::foldBinary(TOperator op,
                                                 TIntermConstantUnion *rightNode,
                                                 TDiagnostics *diagnostics,
                                                 const TSourceLoc &line)
{
    const TConstantUnion *leftArray  = getUnionArrayPointer();
    const TConstantUnion *rightArray = rightNode->getUnionArrayPointer();

    ASSERT(leftArray && rightArray);

    size_t objectSize = getType().getObjectSize();

    // for a case like float f = vec4(2, 3, 4, 5) + 1.2;
    if (rightNode->getType().getObjectSize() == 1 && objectSize > 1)
    {
        rightArray = Vectorize(*rightNode->getUnionArrayPointer(), objectSize);
    }
    else if (rightNode->getType().getObjectSize() > 1 && objectSize == 1)
    {
        // for a case like float f = 1.2 + vec4(2, 3, 4, 5);
        leftArray  = Vectorize(*getUnionArrayPointer(), rightNode->getType().getObjectSize());
        objectSize = rightNode->getType().getObjectSize();
    }

    TConstantUnion *resultArray = nullptr;

    switch (op)
    {
        case EOpAdd:
            resultArray = new TConstantUnion[objectSize];
            for (size_t i = 0; i < objectSize; i++)
                resultArray[i] =
                    TConstantUnion::add(leftArray[i], rightArray[i], diagnostics, line);
            break;
        case EOpSub:
            resultArray = new TConstantUnion[objectSize];
            for (size_t i = 0; i < objectSize; i++)
                resultArray[i] =
                    TConstantUnion::sub(leftArray[i], rightArray[i], diagnostics, line);
            break;

        case EOpMul:
        case EOpVectorTimesScalar:
        case EOpMatrixTimesScalar:
            resultArray = new TConstantUnion[objectSize];
            for (size_t i = 0; i < objectSize; i++)
                resultArray[i] =
                    TConstantUnion::mul(leftArray[i], rightArray[i], diagnostics, line);
            break;

        case EOpMatrixTimesMatrix:
        {
            // TODO(jmadll): This code should check for overflows.
            ASSERT(getType().getBasicType() == EbtFloat && rightNode->getBasicType() == EbtFloat);

            const int leftCols   = getCols();
            const int leftRows   = getRows();
            const int rightCols  = rightNode->getType().getCols();
            const int rightRows  = rightNode->getType().getRows();
            const int resultCols = rightCols;
            const int resultRows = leftRows;

            resultArray = new TConstantUnion[resultCols * resultRows];
            for (int row = 0; row < resultRows; row++)
            {
                for (int column = 0; column < resultCols; column++)
                {
                    resultArray[resultRows * column + row].setFConst(0.0f);
                    for (int i = 0; i < leftCols; i++)
                    {
                        resultArray[resultRows * column + row].setFConst(
                            resultArray[resultRows * column + row].getFConst() +
                            leftArray[i * leftRows + row].getFConst() *
                                rightArray[column * rightRows + i].getFConst());
                    }
                }
            }
        }
        break;

        case EOpDiv:
        case EOpIMod:
        {
            resultArray = new TConstantUnion[objectSize];
            for (size_t i = 0; i < objectSize; i++)
            {
                switch (getType().getBasicType())
                {
                    case EbtFloat:
                    {
                        ASSERT(op == EOpDiv);
                        float dividend = leftArray[i].getFConst();
                        float divisor  = rightArray[i].getFConst();
                        if (divisor == 0.0f)
                        {
                            if (dividend == 0.0f)
                            {
                                diagnostics->warning(
                                    getLine(),
                                    "Zero divided by zero during constant folding generated NaN",
                                    "/");
                                resultArray[i].setFConst(std::numeric_limits<float>::quiet_NaN());
                            }
                            else
                            {
                                diagnostics->warning(getLine(),
                                                     "Divide by zero during constant folding", "/");
                                bool negativeResult =
                                    std::signbit(dividend) != std::signbit(divisor);
                                resultArray[i].setFConst(
                                    negativeResult ? -std::numeric_limits<float>::infinity()
                                                   : std::numeric_limits<float>::infinity());
                            }
                        }
                        else if (gl::isInf(dividend) && gl::isInf(divisor))
                        {
                            diagnostics->warning(getLine(),
                                                 "Infinity divided by infinity during constant "
                                                 "folding generated NaN",
                                                 "/");
                            resultArray[i].setFConst(std::numeric_limits<float>::quiet_NaN());
                        }
                        else
                        {
                            float result = dividend / divisor;
                            if (!gl::isInf(dividend) && gl::isInf(result))
                            {
                                diagnostics->warning(
                                    getLine(), "Constant folded division overflowed to infinity",
                                    "/");
                            }
                            resultArray[i].setFConst(result);
                        }
                        break;
                    }
                    case EbtInt:
                        if (rightArray[i] == 0)
                        {
                            diagnostics->warning(
                                getLine(), "Divide by zero error during constant folding", "/");
                            resultArray[i].setIConst(INT_MAX);
                        }
                        else
                        {
                            int lhs     = leftArray[i].getIConst();
                            int divisor = rightArray[i].getIConst();
                            if (op == EOpDiv)
                            {
                                // Check for the special case where the minimum representable number
                                // is
                                // divided by -1. If left alone this leads to integer overflow in
                                // C++.
                                // ESSL 3.00.6 section 4.1.3 Integers:
                                // "However, for the case where the minimum representable value is
                                // divided by -1, it is allowed to return either the minimum
                                // representable value or the maximum representable value."
                                if (lhs == -0x7fffffff - 1 && divisor == -1)
                                {
                                    resultArray[i].setIConst(0x7fffffff);
                                }
                                else
                                {
                                    resultArray[i].setIConst(lhs / divisor);
                                }
                            }
                            else
                            {
                                ASSERT(op == EOpIMod);
                                if (lhs < 0 || divisor < 0)
                                {
                                    // ESSL 3.00.6 section 5.9: Results of modulus are undefined
                                    // when
                                    // either one of the operands is negative.
                                    diagnostics->warning(getLine(),
                                                         "Negative modulus operator operand "
                                                         "encountered during constant folding",
                                                         "%");
                                    resultArray[i].setIConst(0);
                                }
                                else
                                {
                                    resultArray[i].setIConst(lhs % divisor);
                                }
                            }
                        }
                        break;

                    case EbtUInt:
                        if (rightArray[i] == 0)
                        {
                            diagnostics->warning(
                                getLine(), "Divide by zero error during constant folding", "/");
                            resultArray[i].setUConst(UINT_MAX);
                        }
                        else
                        {
                            if (op == EOpDiv)
                            {
                                resultArray[i].setUConst(leftArray[i].getUConst() /
                                                         rightArray[i].getUConst());
                            }
                            else
                            {
                                ASSERT(op == EOpIMod);
                                resultArray[i].setUConst(leftArray[i].getUConst() %
                                                         rightArray[i].getUConst());
                            }
                        }
                        break;

                    default:
                        UNREACHABLE();
                        return nullptr;
                }
            }
        }
        break;

        case EOpMatrixTimesVector:
        {
            // TODO(jmadll): This code should check for overflows.
            ASSERT(rightNode->getBasicType() == EbtFloat);

            const int matrixCols = getCols();
            const int matrixRows = getRows();

            resultArray = new TConstantUnion[matrixRows];

            for (int matrixRow = 0; matrixRow < matrixRows; matrixRow++)
            {
                resultArray[matrixRow].setFConst(0.0f);
                for (int col = 0; col < matrixCols; col++)
                {
                    resultArray[matrixRow].setFConst(
                        resultArray[matrixRow].getFConst() +
                        leftArray[col * matrixRows + matrixRow].getFConst() *
                            rightArray[col].getFConst());
                }
            }
        }
        break;

        case EOpVectorTimesMatrix:
        {
            // TODO(jmadll): This code should check for overflows.
            ASSERT(getType().getBasicType() == EbtFloat);

            const int matrixCols = rightNode->getType().getCols();
            const int matrixRows = rightNode->getType().getRows();

            resultArray = new TConstantUnion[matrixCols];

            for (int matrixCol = 0; matrixCol < matrixCols; matrixCol++)
            {
                resultArray[matrixCol].setFConst(0.0f);
                for (int matrixRow = 0; matrixRow < matrixRows; matrixRow++)
                {
                    resultArray[matrixCol].setFConst(
                        resultArray[matrixCol].getFConst() +
                        leftArray[matrixRow].getFConst() *
                            rightArray[matrixCol * matrixRows + matrixRow].getFConst());
                }
            }
        }
        break;

        case EOpLogicalAnd:
        {
            resultArray = new TConstantUnion[objectSize];
            for (size_t i = 0; i < objectSize; i++)
            {
                resultArray[i] = leftArray[i] && rightArray[i];
            }
        }
        break;

        case EOpLogicalOr:
        {
            resultArray = new TConstantUnion[objectSize];
            for (size_t i = 0; i < objectSize; i++)
            {
                resultArray[i] = leftArray[i] || rightArray[i];
            }
        }
        break;

        case EOpLogicalXor:
        {
            ASSERT(getType().getBasicType() == EbtBool);
            resultArray = new TConstantUnion[objectSize];
            for (size_t i = 0; i < objectSize; i++)
            {
                resultArray[i].setBConst(leftArray[i] != rightArray[i]);
            }
        }
        break;

        case EOpBitwiseAnd:
            resultArray = new TConstantUnion[objectSize];
            for (size_t i      = 0; i < objectSize; i++)
                resultArray[i] = leftArray[i] & rightArray[i];
            break;
        case EOpBitwiseXor:
            resultArray = new TConstantUnion[objectSize];
            for (size_t i      = 0; i < objectSize; i++)
                resultArray[i] = leftArray[i] ^ rightArray[i];
            break;
        case EOpBitwiseOr:
            resultArray = new TConstantUnion[objectSize];
            for (size_t i      = 0; i < objectSize; i++)
                resultArray[i] = leftArray[i] | rightArray[i];
            break;
        case EOpBitShiftLeft:
            resultArray = new TConstantUnion[objectSize];
            for (size_t i = 0; i < objectSize; i++)
                resultArray[i] =
                    TConstantUnion::lshift(leftArray[i], rightArray[i], diagnostics, line);
            break;
        case EOpBitShiftRight:
            resultArray = new TConstantUnion[objectSize];
            for (size_t i = 0; i < objectSize; i++)
                resultArray[i] =
                    TConstantUnion::rshift(leftArray[i], rightArray[i], diagnostics, line);
            break;

        case EOpLessThan:
            ASSERT(objectSize == 1);
            resultArray = new TConstantUnion[1];
            resultArray->setBConst(*leftArray < *rightArray);
            break;

        case EOpGreaterThan:
            ASSERT(objectSize == 1);
            resultArray = new TConstantUnion[1];
            resultArray->setBConst(*leftArray > *rightArray);
            break;

        case EOpLessThanEqual:
            ASSERT(objectSize == 1);
            resultArray = new TConstantUnion[1];
            resultArray->setBConst(!(*leftArray > *rightArray));
            break;

        case EOpGreaterThanEqual:
            ASSERT(objectSize == 1);
            resultArray = new TConstantUnion[1];
            resultArray->setBConst(!(*leftArray < *rightArray));
            break;

        case EOpEqual:
        case EOpNotEqual:
        {
            resultArray = new TConstantUnion[1];
            bool equal  = true;
            for (size_t i = 0; i < objectSize; i++)
            {
                if (leftArray[i] != rightArray[i])
                {
                    equal = false;
                    break;  // break out of for loop
                }
            }
            if (op == EOpEqual)
            {
                resultArray->setBConst(equal);
            }
            else
            {
                resultArray->setBConst(!equal);
            }
        }
        break;

        default:
            UNREACHABLE();
            return nullptr;
    }
    return resultArray;
}

// The fold functions do operations on a constant at GLSL compile time, without generating run-time
// code. Returns the constant value to keep using. Nullptr should not be returned.
TConstantUnion *TIntermConstantUnion::foldUnaryNonComponentWise(TOperator op)
{
    // Do operations where the return type may have a different number of components compared to the
    // operand type.

    const TConstantUnion *operandArray = getUnionArrayPointer();
    ASSERT(operandArray);

    size_t objectSize           = getType().getObjectSize();
    TConstantUnion *resultArray = nullptr;
    switch (op)
    {
        case EOpAny:
            ASSERT(getType().getBasicType() == EbtBool);
            resultArray = new TConstantUnion();
            resultArray->setBConst(false);
            for (size_t i = 0; i < objectSize; i++)
            {
                if (operandArray[i].getBConst())
                {
                    resultArray->setBConst(true);
                    break;
                }
            }
            break;

        case EOpAll:
            ASSERT(getType().getBasicType() == EbtBool);
            resultArray = new TConstantUnion();
            resultArray->setBConst(true);
            for (size_t i = 0; i < objectSize; i++)
            {
                if (!operandArray[i].getBConst())
                {
                    resultArray->setBConst(false);
                    break;
                }
            }
            break;

        case EOpLength:
            ASSERT(getType().getBasicType() == EbtFloat);
            resultArray = new TConstantUnion();
            resultArray->setFConst(VectorLength(operandArray, objectSize));
            break;

        case EOpTranspose:
        {
            ASSERT(getType().getBasicType() == EbtFloat);
            resultArray = new TConstantUnion[objectSize];
            angle::Matrix<float> result =
                GetMatrix(operandArray, getType().getRows(), getType().getCols()).transpose();
            SetUnionArrayFromMatrix(result, resultArray);
            break;
        }

        case EOpDeterminant:
        {
            ASSERT(getType().getBasicType() == EbtFloat);
            unsigned int size = getType().getNominalSize();
            ASSERT(size >= 2 && size <= 4);
            resultArray = new TConstantUnion();
            resultArray->setFConst(GetMatrix(operandArray, size).determinant());
            break;
        }

        case EOpInverse:
        {
            ASSERT(getType().getBasicType() == EbtFloat);
            unsigned int size = getType().getNominalSize();
            ASSERT(size >= 2 && size <= 4);
            resultArray                 = new TConstantUnion[objectSize];
            angle::Matrix<float> result = GetMatrix(operandArray, size).inverse();
            SetUnionArrayFromMatrix(result, resultArray);
            break;
        }

        case EOpPackSnorm2x16:
            ASSERT(getType().getBasicType() == EbtFloat);
            ASSERT(getType().getNominalSize() == 2);
            resultArray = new TConstantUnion();
            resultArray->setUConst(
                gl::packSnorm2x16(operandArray[0].getFConst(), operandArray[1].getFConst()));
            break;

        case EOpUnpackSnorm2x16:
        {
            ASSERT(getType().getBasicType() == EbtUInt);
            resultArray = new TConstantUnion[2];
            float f1, f2;
            gl::unpackSnorm2x16(operandArray[0].getUConst(), &f1, &f2);
            resultArray[0].setFConst(f1);
            resultArray[1].setFConst(f2);
            break;
        }

        case EOpPackUnorm2x16:
            ASSERT(getType().getBasicType() == EbtFloat);
            ASSERT(getType().getNominalSize() == 2);
            resultArray = new TConstantUnion();
            resultArray->setUConst(
                gl::packUnorm2x16(operandArray[0].getFConst(), operandArray[1].getFConst()));
            break;

        case EOpUnpackUnorm2x16:
        {
            ASSERT(getType().getBasicType() == EbtUInt);
            resultArray = new TConstantUnion[2];
            float f1, f2;
            gl::unpackUnorm2x16(operandArray[0].getUConst(), &f1, &f2);
            resultArray[0].setFConst(f1);
            resultArray[1].setFConst(f2);
            break;
        }

        case EOpPackHalf2x16:
            ASSERT(getType().getBasicType() == EbtFloat);
            ASSERT(getType().getNominalSize() == 2);
            resultArray = new TConstantUnion();
            resultArray->setUConst(
                gl::packHalf2x16(operandArray[0].getFConst(), operandArray[1].getFConst()));
            break;

        case EOpUnpackHalf2x16:
        {
            ASSERT(getType().getBasicType() == EbtUInt);
            resultArray = new TConstantUnion[2];
            float f1, f2;
            gl::unpackHalf2x16(operandArray[0].getUConst(), &f1, &f2);
            resultArray[0].setFConst(f1);
            resultArray[1].setFConst(f2);
            break;
        }

        case EOpPackUnorm4x8:
        {
            ASSERT(getType().getBasicType() == EbtFloat);
            resultArray = new TConstantUnion();
            resultArray->setUConst(
                gl::PackUnorm4x8(operandArray[0].getFConst(), operandArray[1].getFConst(),
                                 operandArray[2].getFConst(), operandArray[3].getFConst()));
            break;
        }
        case EOpPackSnorm4x8:
        {
            ASSERT(getType().getBasicType() == EbtFloat);
            resultArray = new TConstantUnion();
            resultArray->setUConst(
                gl::PackSnorm4x8(operandArray[0].getFConst(), operandArray[1].getFConst(),
                                 operandArray[2].getFConst(), operandArray[3].getFConst()));
            break;
        }
        case EOpUnpackUnorm4x8:
        {
            ASSERT(getType().getBasicType() == EbtUInt);
            resultArray = new TConstantUnion[4];
            float f[4];
            gl::UnpackUnorm4x8(operandArray[0].getUConst(), f);
            for (size_t i = 0; i < 4; ++i)
            {
                resultArray[i].setFConst(f[i]);
            }
            break;
        }
        case EOpUnpackSnorm4x8:
        {
            ASSERT(getType().getBasicType() == EbtUInt);
            resultArray = new TConstantUnion[4];
            float f[4];
            gl::UnpackSnorm4x8(operandArray[0].getUConst(), f);
            for (size_t i = 0; i < 4; ++i)
            {
                resultArray[i].setFConst(f[i]);
            }
            break;
        }

        default:
            UNREACHABLE();
            break;
    }

    return resultArray;
}

TConstantUnion *TIntermConstantUnion::foldUnaryComponentWise(TOperator op,
                                                             TDiagnostics *diagnostics)
{
    // Do unary operations where each component of the result is computed based on the corresponding
    // component of the operand. Also folds normalize, though the divisor in that case takes all
    // components into account.

    const TConstantUnion *operandArray = getUnionArrayPointer();
    ASSERT(operandArray);

    size_t objectSize = getType().getObjectSize();

    TConstantUnion *resultArray = new TConstantUnion[objectSize];
    for (size_t i = 0; i < objectSize; i++)
    {
        switch (op)
        {
            case EOpNegative:
                switch (getType().getBasicType())
                {
                    case EbtFloat:
                        resultArray[i].setFConst(-operandArray[i].getFConst());
                        break;
                    case EbtInt:
                        if (operandArray[i] == std::numeric_limits<int>::min())
                        {
                            // The minimum representable integer doesn't have a positive
                            // counterpart, rather the negation overflows and in ESSL is supposed to
                            // wrap back to the minimum representable integer. Make sure that we
                            // don't actually let the negation overflow, which has undefined
                            // behavior in C++.
                            resultArray[i].setIConst(std::numeric_limits<int>::min());
                        }
                        else
                        {
                            resultArray[i].setIConst(-operandArray[i].getIConst());
                        }
                        break;
                    case EbtUInt:
                        if (operandArray[i] == 0x80000000u)
                        {
                            resultArray[i].setUConst(0x80000000u);
                        }
                        else
                        {
                            resultArray[i].setUConst(static_cast<unsigned int>(
                                -static_cast<int>(operandArray[i].getUConst())));
                        }
                        break;
                    default:
                        UNREACHABLE();
                        return nullptr;
                }
                break;

            case EOpPositive:
                switch (getType().getBasicType())
                {
                    case EbtFloat:
                        resultArray[i].setFConst(operandArray[i].getFConst());
                        break;
                    case EbtInt:
                        resultArray[i].setIConst(operandArray[i].getIConst());
                        break;
                    case EbtUInt:
                        resultArray[i].setUConst(static_cast<unsigned int>(
                            static_cast<int>(operandArray[i].getUConst())));
                        break;
                    default:
                        UNREACHABLE();
                        return nullptr;
                }
                break;

            case EOpLogicalNot:
                switch (getType().getBasicType())
                {
                    case EbtBool:
                        resultArray[i].setBConst(!operandArray[i].getBConst());
                        break;
                    default:
                        UNREACHABLE();
                        return nullptr;
                }
                break;

            case EOpBitwiseNot:
                switch (getType().getBasicType())
                {
                    case EbtInt:
                        resultArray[i].setIConst(~operandArray[i].getIConst());
                        break;
                    case EbtUInt:
                        resultArray[i].setUConst(~operandArray[i].getUConst());
                        break;
                    default:
                        UNREACHABLE();
                        return nullptr;
                }
                break;

            case EOpRadians:
                ASSERT(getType().getBasicType() == EbtFloat);
                resultArray[i].setFConst(kDegreesToRadiansMultiplier * operandArray[i].getFConst());
                break;

            case EOpDegrees:
                ASSERT(getType().getBasicType() == EbtFloat);
                resultArray[i].setFConst(kRadiansToDegreesMultiplier * operandArray[i].getFConst());
                break;

            case EOpSin:
                foldFloatTypeUnary(operandArray[i], &sinf, &resultArray[i]);
                break;

            case EOpCos:
                foldFloatTypeUnary(operandArray[i], &cosf, &resultArray[i]);
                break;

            case EOpTan:
                foldFloatTypeUnary(operandArray[i], &tanf, &resultArray[i]);
                break;

            case EOpAsin:
                // For asin(x), results are undefined if |x| > 1, we are choosing to set result to
                // 0.
                if (fabsf(operandArray[i].getFConst()) > 1.0f)
                    UndefinedConstantFoldingError(getLine(), op, getType().getBasicType(),
                                                  diagnostics, &resultArray[i]);
                else
                    foldFloatTypeUnary(operandArray[i], &asinf, &resultArray[i]);
                break;

            case EOpAcos:
                // For acos(x), results are undefined if |x| > 1, we are choosing to set result to
                // 0.
                if (fabsf(operandArray[i].getFConst()) > 1.0f)
                    UndefinedConstantFoldingError(getLine(), op, getType().getBasicType(),
                                                  diagnostics, &resultArray[i]);
                else
                    foldFloatTypeUnary(operandArray[i], &acosf, &resultArray[i]);
                break;

            case EOpAtan:
                foldFloatTypeUnary(operandArray[i], &atanf, &resultArray[i]);
                break;

            case EOpSinh:
                foldFloatTypeUnary(operandArray[i], &sinhf, &resultArray[i]);
                break;

            case EOpCosh:
                foldFloatTypeUnary(operandArray[i], &coshf, &resultArray[i]);
                break;

            case EOpTanh:
                foldFloatTypeUnary(operandArray[i], &tanhf, &resultArray[i]);
                break;

            case EOpAsinh:
                foldFloatTypeUnary(operandArray[i], &asinhf, &resultArray[i]);
                break;

            case EOpAcosh:
                // For acosh(x), results are undefined if x < 1, we are choosing to set result to 0.
                if (operandArray[i].getFConst() < 1.0f)
                    UndefinedConstantFoldingError(getLine(), op, getType().getBasicType(),
                                                  diagnostics, &resultArray[i]);
                else
                    foldFloatTypeUnary(operandArray[i], &acoshf, &resultArray[i]);
                break;

            case EOpAtanh:
                // For atanh(x), results are undefined if |x| >= 1, we are choosing to set result to
                // 0.
                if (fabsf(operandArray[i].getFConst()) >= 1.0f)
                    UndefinedConstantFoldingError(getLine(), op, getType().getBasicType(),
                                                  diagnostics, &resultArray[i]);
                else
                    foldFloatTypeUnary(operandArray[i], &atanhf, &resultArray[i]);
                break;

            case EOpAbs:
                switch (getType().getBasicType())
                {
                    case EbtFloat:
                        resultArray[i].setFConst(fabsf(operandArray[i].getFConst()));
                        break;
                    case EbtInt:
                        resultArray[i].setIConst(abs(operandArray[i].getIConst()));
                        break;
                    default:
                        UNREACHABLE();
                        return nullptr;
                }
                break;

            case EOpSign:
                switch (getType().getBasicType())
                {
                    case EbtFloat:
                    {
                        float fConst  = operandArray[i].getFConst();
                        float fResult = 0.0f;
                        if (fConst > 0.0f)
                            fResult = 1.0f;
                        else if (fConst < 0.0f)
                            fResult = -1.0f;
                        resultArray[i].setFConst(fResult);
                        break;
                    }
                    case EbtInt:
                    {
                        int iConst  = operandArray[i].getIConst();
                        int iResult = 0;
                        if (iConst > 0)
                            iResult = 1;
                        else if (iConst < 0)
                            iResult = -1;
                        resultArray[i].setIConst(iResult);
                        break;
                    }
                    default:
                        UNREACHABLE();
                        return nullptr;
                }
                break;

            case EOpFloor:
                foldFloatTypeUnary(operandArray[i], &floorf, &resultArray[i]);
                break;

            case EOpTrunc:
                foldFloatTypeUnary(operandArray[i], &truncf, &resultArray[i]);
                break;

            case EOpRound:
                foldFloatTypeUnary(operandArray[i], &roundf, &resultArray[i]);
                break;

            case EOpRoundEven:
            {
                ASSERT(getType().getBasicType() == EbtFloat);
                float x = operandArray[i].getFConst();
                float result;
                float fractPart = modff(x, &result);
                if (fabsf(fractPart) == 0.5f)
                    result = 2.0f * roundf(x / 2.0f);
                else
                    result = roundf(x);
                resultArray[i].setFConst(result);
                break;
            }

            case EOpCeil:
                foldFloatTypeUnary(operandArray[i], &ceilf, &resultArray[i]);
                break;

            case EOpFract:
            {
                ASSERT(getType().getBasicType() == EbtFloat);
                float x = operandArray[i].getFConst();
                resultArray[i].setFConst(x - floorf(x));
                break;
            }

            case EOpIsNan:
                ASSERT(getType().getBasicType() == EbtFloat);
                resultArray[i].setBConst(gl::isNaN(operandArray[0].getFConst()));
                break;

            case EOpIsInf:
                ASSERT(getType().getBasicType() == EbtFloat);
                resultArray[i].setBConst(gl::isInf(operandArray[0].getFConst()));
                break;

            case EOpFloatBitsToInt:
                ASSERT(getType().getBasicType() == EbtFloat);
                resultArray[i].setIConst(gl::bitCast<int32_t>(operandArray[0].getFConst()));
                break;

            case EOpFloatBitsToUint:
                ASSERT(getType().getBasicType() == EbtFloat);
                resultArray[i].setUConst(gl::bitCast<uint32_t>(operandArray[0].getFConst()));
                break;

            case EOpIntBitsToFloat:
                ASSERT(getType().getBasicType() == EbtInt);
                resultArray[i].setFConst(gl::bitCast<float>(operandArray[0].getIConst()));
                break;

            case EOpUintBitsToFloat:
                ASSERT(getType().getBasicType() == EbtUInt);
                resultArray[i].setFConst(gl::bitCast<float>(operandArray[0].getUConst()));
                break;

            case EOpExp:
                foldFloatTypeUnary(operandArray[i], &expf, &resultArray[i]);
                break;

            case EOpLog:
                // For log(x), results are undefined if x <= 0, we are choosing to set result to 0.
                if (operandArray[i].getFConst() <= 0.0f)
                    UndefinedConstantFoldingError(getLine(), op, getType().getBasicType(),
                                                  diagnostics, &resultArray[i]);
                else
                    foldFloatTypeUnary(operandArray[i], &logf, &resultArray[i]);
                break;

            case EOpExp2:
                foldFloatTypeUnary(operandArray[i], &exp2f, &resultArray[i]);
                break;

            case EOpLog2:
                // For log2(x), results are undefined if x <= 0, we are choosing to set result to 0.
                // And log2f is not available on some plarforms like old android, so just using
                // log(x)/log(2) here.
                if (operandArray[i].getFConst() <= 0.0f)
                    UndefinedConstantFoldingError(getLine(), op, getType().getBasicType(),
                                                  diagnostics, &resultArray[i]);
                else
                {
                    foldFloatTypeUnary(operandArray[i], &logf, &resultArray[i]);
                    resultArray[i].setFConst(resultArray[i].getFConst() / logf(2.0f));
                }
                break;

            case EOpSqrt:
                // For sqrt(x), results are undefined if x < 0, we are choosing to set result to 0.
                if (operandArray[i].getFConst() < 0.0f)
                    UndefinedConstantFoldingError(getLine(), op, getType().getBasicType(),
                                                  diagnostics, &resultArray[i]);
                else
                    foldFloatTypeUnary(operandArray[i], &sqrtf, &resultArray[i]);
                break;

            case EOpInverseSqrt:
                // There is no stdlib built-in function equavalent for GLES built-in inversesqrt(),
                // so getting the square root first using builtin function sqrt() and then taking
                // its inverse.
                // Also, for inversesqrt(x), results are undefined if x <= 0, we are choosing to set
                // result to 0.
                if (operandArray[i].getFConst() <= 0.0f)
                    UndefinedConstantFoldingError(getLine(), op, getType().getBasicType(),
                                                  diagnostics, &resultArray[i]);
                else
                {
                    foldFloatTypeUnary(operandArray[i], &sqrtf, &resultArray[i]);
                    resultArray[i].setFConst(1.0f / resultArray[i].getFConst());
                }
                break;

            case EOpLogicalNotComponentWise:
                ASSERT(getType().getBasicType() == EbtBool);
                resultArray[i].setBConst(!operandArray[i].getBConst());
                break;

            case EOpNormalize:
            {
                ASSERT(getType().getBasicType() == EbtFloat);
                float x      = operandArray[i].getFConst();
                float length = VectorLength(operandArray, objectSize);
                if (length)
                    resultArray[i].setFConst(x / length);
                else
                    UndefinedConstantFoldingError(getLine(), op, getType().getBasicType(),
                                                  diagnostics, &resultArray[i]);
                break;
            }
            case EOpBitfieldReverse:
            {
                uint32_t value;
                if (getType().getBasicType() == EbtInt)
                {
                    value = static_cast<uint32_t>(operandArray[i].getIConst());
                }
                else
                {
                    ASSERT(getType().getBasicType() == EbtUInt);
                    value = operandArray[i].getUConst();
                }
                uint32_t result = gl::BitfieldReverse(value);
                if (getType().getBasicType() == EbtInt)
                {
                    resultArray[i].setIConst(static_cast<int32_t>(result));
                }
                else
                {
                    resultArray[i].setUConst(result);
                }
                break;
            }
            case EOpBitCount:
            {
                uint32_t value;
                if (getType().getBasicType() == EbtInt)
                {
                    value = static_cast<uint32_t>(operandArray[i].getIConst());
                }
                else
                {
                    ASSERT(getType().getBasicType() == EbtUInt);
                    value = operandArray[i].getUConst();
                }
                int result = gl::BitCount(value);
                resultArray[i].setIConst(result);
                break;
            }
            case EOpFindLSB:
            {
                uint32_t value;
                if (getType().getBasicType() == EbtInt)
                {
                    value = static_cast<uint32_t>(operandArray[i].getIConst());
                }
                else
                {
                    ASSERT(getType().getBasicType() == EbtUInt);
                    value = operandArray[i].getUConst();
                }
                resultArray[i].setIConst(gl::FindLSB(value));
                break;
            }
            case EOpFindMSB:
            {
                uint32_t value;
                if (getType().getBasicType() == EbtInt)
                {
                    int intValue = operandArray[i].getIConst();
                    value        = static_cast<uint32_t>(intValue);
                    if (intValue < 0)
                    {
                        // Look for zero instead of one in value. This also handles the intValue ==
                        // -1 special case, where the return value needs to be -1.
                        value = ~value;
                    }
                }
                else
                {
                    ASSERT(getType().getBasicType() == EbtUInt);
                    value = operandArray[i].getUConst();
                }
                resultArray[i].setIConst(gl::FindMSB(value));
                break;
            }
            case EOpDFdx:
            case EOpDFdy:
            case EOpFwidth:
                ASSERT(getType().getBasicType() == EbtFloat);
                // Derivatives of constant arguments should be 0.
                resultArray[i].setFConst(0.0f);
                break;

            default:
                return nullptr;
        }
    }

    return resultArray;
}

void TIntermConstantUnion::foldFloatTypeUnary(const TConstantUnion &parameter,
                                              FloatTypeUnaryFunc builtinFunc,
                                              TConstantUnion *result) const
{
    ASSERT(builtinFunc);

    ASSERT(getType().getBasicType() == EbtFloat);
    result->setFConst(builtinFunc(parameter.getFConst()));
}

// static
TConstantUnion *TIntermConstantUnion::FoldAggregateConstructor(TIntermAggregate *aggregate)
{
    ASSERT(aggregate->getSequence()->size() > 0u);
    size_t resultSize           = aggregate->getType().getObjectSize();
    TConstantUnion *resultArray = new TConstantUnion[resultSize];
    TBasicType basicType        = aggregate->getBasicType();

    size_t resultIndex = 0u;

    if (aggregate->getSequence()->size() == 1u)
    {
        TIntermNode *argument                    = aggregate->getSequence()->front();
        TIntermConstantUnion *argumentConstant   = argument->getAsConstantUnion();
        const TConstantUnion *argumentUnionArray = argumentConstant->getUnionArrayPointer();
        // Check the special case of constructing a matrix diagonal from a single scalar,
        // or a vector from a single scalar.
        if (argumentConstant->getType().getObjectSize() == 1u)
        {
            if (aggregate->isMatrix())
            {
                int resultCols = aggregate->getType().getCols();
                int resultRows = aggregate->getType().getRows();
                for (int col = 0; col < resultCols; ++col)
                {
                    for (int row = 0; row < resultRows; ++row)
                    {
                        if (col == row)
                        {
                            resultArray[resultIndex].cast(basicType, argumentUnionArray[0]);
                        }
                        else
                        {
                            resultArray[resultIndex].setFConst(0.0f);
                        }
                        ++resultIndex;
                    }
                }
            }
            else
            {
                while (resultIndex < resultSize)
                {
                    resultArray[resultIndex].cast(basicType, argumentUnionArray[0]);
                    ++resultIndex;
                }
            }
            ASSERT(resultIndex == resultSize);
            return resultArray;
        }
        else if (aggregate->isMatrix() && argumentConstant->isMatrix())
        {
            // The special case of constructing a matrix from a matrix.
            int argumentCols = argumentConstant->getType().getCols();
            int argumentRows = argumentConstant->getType().getRows();
            int resultCols   = aggregate->getType().getCols();
            int resultRows   = aggregate->getType().getRows();
            for (int col = 0; col < resultCols; ++col)
            {
                for (int row = 0; row < resultRows; ++row)
                {
                    if (col < argumentCols && row < argumentRows)
                    {
                        resultArray[resultIndex].cast(basicType,
                                                      argumentUnionArray[col * argumentRows + row]);
                    }
                    else if (col == row)
                    {
                        resultArray[resultIndex].setFConst(1.0f);
                    }
                    else
                    {
                        resultArray[resultIndex].setFConst(0.0f);
                    }
                    ++resultIndex;
                }
            }
            ASSERT(resultIndex == resultSize);
            return resultArray;
        }
    }

    for (TIntermNode *&argument : *aggregate->getSequence())
    {
        TIntermConstantUnion *argumentConstant   = argument->getAsConstantUnion();
        size_t argumentSize                      = argumentConstant->getType().getObjectSize();
        const TConstantUnion *argumentUnionArray = argumentConstant->getUnionArrayPointer();
        for (size_t i = 0u; i < argumentSize; ++i)
        {
            if (resultIndex >= resultSize)
                break;
            resultArray[resultIndex].cast(basicType, argumentUnionArray[i]);
            ++resultIndex;
        }
    }
    ASSERT(resultIndex == resultSize);
    return resultArray;
}

bool TIntermAggregate::CanFoldAggregateBuiltInOp(TOperator op)
{
    switch (op)
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
        case EOpFaceforward:
        case EOpReflect:
        case EOpRefract:
        case EOpBitfieldExtract:
        case EOpBitfieldInsert:
            return true;
        default:
            return false;
    }
}

// static
TConstantUnion *TIntermConstantUnion::FoldAggregateBuiltIn(TIntermAggregate *aggregate,
                                                           TDiagnostics *diagnostics)
{
    TOperator op              = aggregate->getOp();
    TIntermSequence *arguments = aggregate->getSequence();
    unsigned int argsCount     = static_cast<unsigned int>(arguments->size());
    std::vector<const TConstantUnion *> unionArrays(argsCount);
    std::vector<size_t> objectSizes(argsCount);
    size_t maxObjectSize = 0;
    TBasicType basicType = EbtVoid;
    TSourceLoc loc;
    for (unsigned int i = 0; i < argsCount; i++)
    {
        TIntermConstantUnion *argConstant = (*arguments)[i]->getAsConstantUnion();
        ASSERT(argConstant != nullptr);  // Should be checked already.

        if (i == 0)
        {
            basicType = argConstant->getType().getBasicType();
            loc       = argConstant->getLine();
        }
        unionArrays[i] = argConstant->getUnionArrayPointer();
        objectSizes[i] = argConstant->getType().getObjectSize();
        if (objectSizes[i] > maxObjectSize)
            maxObjectSize = objectSizes[i];
    }

    if (!(*arguments)[0]->getAsTyped()->isMatrix() && aggregate->getOp() != EOpOuterProduct)
    {
        for (unsigned int i = 0; i < argsCount; i++)
            if (objectSizes[i] != maxObjectSize)
                unionArrays[i] = Vectorize(*unionArrays[i], maxObjectSize);
    }

    TConstantUnion *resultArray = nullptr;

    switch (op)
    {
        case EOpAtan:
        {
            ASSERT(basicType == EbtFloat);
            resultArray = new TConstantUnion[maxObjectSize];
            for (size_t i = 0; i < maxObjectSize; i++)
            {
                float y = unionArrays[0][i].getFConst();
                float x = unionArrays[1][i].getFConst();
                // Results are undefined if x and y are both 0.
                if (x == 0.0f && y == 0.0f)
                    UndefinedConstantFoldingError(loc, op, basicType, diagnostics, &resultArray[i]);
                else
                    resultArray[i].setFConst(atan2f(y, x));
            }
            break;
        }

        case EOpPow:
        {
            ASSERT(basicType == EbtFloat);
            resultArray = new TConstantUnion[maxObjectSize];
            for (size_t i = 0; i < maxObjectSize; i++)
            {
                float x = unionArrays[0][i].getFConst();
                float y = unionArrays[1][i].getFConst();
                // Results are undefined if x < 0.
                // Results are undefined if x = 0 and y <= 0.
                if (x < 0.0f)
                    UndefinedConstantFoldingError(loc, op, basicType, diagnostics, &resultArray[i]);
                else if (x == 0.0f && y <= 0.0f)
                    UndefinedConstantFoldingError(loc, op, basicType, diagnostics, &resultArray[i]);
                else
                    resultArray[i].setFConst(powf(x, y));
            }
            break;
        }

        case EOpMod:
        {
            ASSERT(basicType == EbtFloat);
            resultArray = new TConstantUnion[maxObjectSize];
            for (size_t i = 0; i < maxObjectSize; i++)
            {
                float x = unionArrays[0][i].getFConst();
                float y = unionArrays[1][i].getFConst();
                resultArray[i].setFConst(x - y * floorf(x / y));
            }
            break;
        }

        case EOpMin:
        {
            resultArray = new TConstantUnion[maxObjectSize];
            for (size_t i = 0; i < maxObjectSize; i++)
            {
                switch (basicType)
                {
                    case EbtFloat:
                        resultArray[i].setFConst(
                            std::min(unionArrays[0][i].getFConst(), unionArrays[1][i].getFConst()));
                        break;
                    case EbtInt:
                        resultArray[i].setIConst(
                            std::min(unionArrays[0][i].getIConst(), unionArrays[1][i].getIConst()));
                        break;
                    case EbtUInt:
                        resultArray[i].setUConst(
                            std::min(unionArrays[0][i].getUConst(), unionArrays[1][i].getUConst()));
                        break;
                    default:
                        UNREACHABLE();
                        break;
                }
            }
            break;
        }

        case EOpMax:
        {
            resultArray = new TConstantUnion[maxObjectSize];
            for (size_t i = 0; i < maxObjectSize; i++)
            {
                switch (basicType)
                {
                    case EbtFloat:
                        resultArray[i].setFConst(
                            std::max(unionArrays[0][i].getFConst(), unionArrays[1][i].getFConst()));
                        break;
                    case EbtInt:
                        resultArray[i].setIConst(
                            std::max(unionArrays[0][i].getIConst(), unionArrays[1][i].getIConst()));
                        break;
                    case EbtUInt:
                        resultArray[i].setUConst(
                            std::max(unionArrays[0][i].getUConst(), unionArrays[1][i].getUConst()));
                        break;
                    default:
                        UNREACHABLE();
                        break;
                }
            }
            break;
        }

        case EOpStep:
        {
            ASSERT(basicType == EbtFloat);
            resultArray = new TConstantUnion[maxObjectSize];
            for (size_t i = 0; i < maxObjectSize; i++)
                resultArray[i].setFConst(
                    unionArrays[1][i].getFConst() < unionArrays[0][i].getFConst() ? 0.0f : 1.0f);
            break;
        }

        case EOpLessThanComponentWise:
        {
            resultArray = new TConstantUnion[maxObjectSize];
            for (size_t i = 0; i < maxObjectSize; i++)
            {
                switch (basicType)
                {
                    case EbtFloat:
                        resultArray[i].setBConst(unionArrays[0][i].getFConst() <
                                                 unionArrays[1][i].getFConst());
                        break;
                    case EbtInt:
                        resultArray[i].setBConst(unionArrays[0][i].getIConst() <
                                                 unionArrays[1][i].getIConst());
                        break;
                    case EbtUInt:
                        resultArray[i].setBConst(unionArrays[0][i].getUConst() <
                                                 unionArrays[1][i].getUConst());
                        break;
                    default:
                        UNREACHABLE();
                        break;
                }
            }
            break;
        }

        case EOpLessThanEqualComponentWise:
        {
            resultArray = new TConstantUnion[maxObjectSize];
            for (size_t i = 0; i < maxObjectSize; i++)
            {
                switch (basicType)
                {
                    case EbtFloat:
                        resultArray[i].setBConst(unionArrays[0][i].getFConst() <=
                                                 unionArrays[1][i].getFConst());
                        break;
                    case EbtInt:
                        resultArray[i].setBConst(unionArrays[0][i].getIConst() <=
                                                 unionArrays[1][i].getIConst());
                        break;
                    case EbtUInt:
                        resultArray[i].setBConst(unionArrays[0][i].getUConst() <=
                                                 unionArrays[1][i].getUConst());
                        break;
                    default:
                        UNREACHABLE();
                        break;
                }
            }
            break;
        }

        case EOpGreaterThanComponentWise:
        {
            resultArray = new TConstantUnion[maxObjectSize];
            for (size_t i = 0; i < maxObjectSize; i++)
            {
                switch (basicType)
                {
                    case EbtFloat:
                        resultArray[i].setBConst(unionArrays[0][i].getFConst() >
                                                 unionArrays[1][i].getFConst());
                        break;
                    case EbtInt:
                        resultArray[i].setBConst(unionArrays[0][i].getIConst() >
                                                 unionArrays[1][i].getIConst());
                        break;
                    case EbtUInt:
                        resultArray[i].setBConst(unionArrays[0][i].getUConst() >
                                                 unionArrays[1][i].getUConst());
                        break;
                    default:
                        UNREACHABLE();
                        break;
                }
            }
            break;
        }
        case EOpGreaterThanEqualComponentWise:
        {
            resultArray = new TConstantUnion[maxObjectSize];
            for (size_t i = 0; i < maxObjectSize; i++)
            {
                switch (basicType)
                {
                    case EbtFloat:
                        resultArray[i].setBConst(unionArrays[0][i].getFConst() >=
                                                 unionArrays[1][i].getFConst());
                        break;
                    case EbtInt:
                        resultArray[i].setBConst(unionArrays[0][i].getIConst() >=
                                                 unionArrays[1][i].getIConst());
                        break;
                    case EbtUInt:
                        resultArray[i].setBConst(unionArrays[0][i].getUConst() >=
                                                 unionArrays[1][i].getUConst());
                        break;
                    default:
                        UNREACHABLE();
                        break;
                }
            }
        }
        break;

        case EOpEqualComponentWise:
        {
            resultArray = new TConstantUnion[maxObjectSize];
            for (size_t i = 0; i < maxObjectSize; i++)
            {
                switch (basicType)
                {
                    case EbtFloat:
                        resultArray[i].setBConst(unionArrays[0][i].getFConst() ==
                                                 unionArrays[1][i].getFConst());
                        break;
                    case EbtInt:
                        resultArray[i].setBConst(unionArrays[0][i].getIConst() ==
                                                 unionArrays[1][i].getIConst());
                        break;
                    case EbtUInt:
                        resultArray[i].setBConst(unionArrays[0][i].getUConst() ==
                                                 unionArrays[1][i].getUConst());
                        break;
                    case EbtBool:
                        resultArray[i].setBConst(unionArrays[0][i].getBConst() ==
                                                 unionArrays[1][i].getBConst());
                        break;
                    default:
                        UNREACHABLE();
                        break;
                }
            }
            break;
        }

        case EOpNotEqualComponentWise:
        {
            resultArray = new TConstantUnion[maxObjectSize];
            for (size_t i = 0; i < maxObjectSize; i++)
            {
                switch (basicType)
                {
                    case EbtFloat:
                        resultArray[i].setBConst(unionArrays[0][i].getFConst() !=
                                                 unionArrays[1][i].getFConst());
                        break;
                    case EbtInt:
                        resultArray[i].setBConst(unionArrays[0][i].getIConst() !=
                                                 unionArrays[1][i].getIConst());
                        break;
                    case EbtUInt:
                        resultArray[i].setBConst(unionArrays[0][i].getUConst() !=
                                                 unionArrays[1][i].getUConst());
                        break;
                    case EbtBool:
                        resultArray[i].setBConst(unionArrays[0][i].getBConst() !=
                                                 unionArrays[1][i].getBConst());
                        break;
                    default:
                        UNREACHABLE();
                        break;
                }
            }
            break;
        }

        case EOpDistance:
        {
            ASSERT(basicType == EbtFloat);
            TConstantUnion *distanceArray = new TConstantUnion[maxObjectSize];
            resultArray                   = new TConstantUnion();
            for (size_t i = 0; i < maxObjectSize; i++)
            {
                float x = unionArrays[0][i].getFConst();
                float y = unionArrays[1][i].getFConst();
                distanceArray[i].setFConst(x - y);
            }
            resultArray->setFConst(VectorLength(distanceArray, maxObjectSize));
            break;
        }

        case EOpDot:
            ASSERT(basicType == EbtFloat);
            resultArray = new TConstantUnion();
            resultArray->setFConst(VectorDotProduct(unionArrays[0], unionArrays[1], maxObjectSize));
            break;

        case EOpCross:
        {
            ASSERT(basicType == EbtFloat && maxObjectSize == 3);
            resultArray = new TConstantUnion[maxObjectSize];
            float x0    = unionArrays[0][0].getFConst();
            float x1    = unionArrays[0][1].getFConst();
            float x2    = unionArrays[0][2].getFConst();
            float y0    = unionArrays[1][0].getFConst();
            float y1    = unionArrays[1][1].getFConst();
            float y2    = unionArrays[1][2].getFConst();
            resultArray[0].setFConst(x1 * y2 - y1 * x2);
            resultArray[1].setFConst(x2 * y0 - y2 * x0);
            resultArray[2].setFConst(x0 * y1 - y0 * x1);
            break;
        }

        case EOpReflect:
        {
            ASSERT(basicType == EbtFloat);
            // genType reflect (genType I, genType N) :
            //     For the incident vector I and surface orientation N, returns the reflection
            //     direction:
            //     I - 2 * dot(N, I) * N.
            resultArray      = new TConstantUnion[maxObjectSize];
            float dotProduct = VectorDotProduct(unionArrays[1], unionArrays[0], maxObjectSize);
            for (size_t i = 0; i < maxObjectSize; i++)
            {
                float result = unionArrays[0][i].getFConst() -
                               2.0f * dotProduct * unionArrays[1][i].getFConst();
                resultArray[i].setFConst(result);
            }
            break;
        }

        case EOpMulMatrixComponentWise:
        {
            ASSERT(basicType == EbtFloat && (*arguments)[0]->getAsTyped()->isMatrix() &&
                   (*arguments)[1]->getAsTyped()->isMatrix());
            // Perform component-wise matrix multiplication.
            resultArray = new TConstantUnion[maxObjectSize];
            int size    = (*arguments)[0]->getAsTyped()->getNominalSize();
            angle::Matrix<float> result =
                GetMatrix(unionArrays[0], size).compMult(GetMatrix(unionArrays[1], size));
            SetUnionArrayFromMatrix(result, resultArray);
            break;
        }

        case EOpOuterProduct:
        {
            ASSERT(basicType == EbtFloat);
            size_t numRows = (*arguments)[0]->getAsTyped()->getType().getObjectSize();
            size_t numCols = (*arguments)[1]->getAsTyped()->getType().getObjectSize();
            resultArray    = new TConstantUnion[numRows * numCols];
            angle::Matrix<float> result =
                GetMatrix(unionArrays[0], static_cast<int>(numRows), 1)
                    .outerProduct(GetMatrix(unionArrays[1], 1, static_cast<int>(numCols)));
            SetUnionArrayFromMatrix(result, resultArray);
            break;
        }

        case EOpClamp:
        {
            resultArray = new TConstantUnion[maxObjectSize];
            for (size_t i = 0; i < maxObjectSize; i++)
            {
                switch (basicType)
                {
                    case EbtFloat:
                    {
                        float x   = unionArrays[0][i].getFConst();
                        float min = unionArrays[1][i].getFConst();
                        float max = unionArrays[2][i].getFConst();
                        // Results are undefined if min > max.
                        if (min > max)
                            UndefinedConstantFoldingError(loc, op, basicType, diagnostics,
                                                          &resultArray[i]);
                        else
                            resultArray[i].setFConst(gl::clamp(x, min, max));
                        break;
                    }

                    case EbtInt:
                    {
                        int x   = unionArrays[0][i].getIConst();
                        int min = unionArrays[1][i].getIConst();
                        int max = unionArrays[2][i].getIConst();
                        // Results are undefined if min > max.
                        if (min > max)
                            UndefinedConstantFoldingError(loc, op, basicType, diagnostics,
                                                          &resultArray[i]);
                        else
                            resultArray[i].setIConst(gl::clamp(x, min, max));
                        break;
                    }
                    case EbtUInt:
                    {
                        unsigned int x   = unionArrays[0][i].getUConst();
                        unsigned int min = unionArrays[1][i].getUConst();
                        unsigned int max = unionArrays[2][i].getUConst();
                        // Results are undefined if min > max.
                        if (min > max)
                            UndefinedConstantFoldingError(loc, op, basicType, diagnostics,
                                                          &resultArray[i]);
                        else
                            resultArray[i].setUConst(gl::clamp(x, min, max));
                        break;
                    }
                    default:
                        UNREACHABLE();
                        break;
                }
            }
            break;
        }

        case EOpMix:
        {
            ASSERT(basicType == EbtFloat);
            resultArray = new TConstantUnion[maxObjectSize];
            for (size_t i = 0; i < maxObjectSize; i++)
            {
                float x         = unionArrays[0][i].getFConst();
                float y         = unionArrays[1][i].getFConst();
                TBasicType type = (*arguments)[2]->getAsTyped()->getType().getBasicType();
                if (type == EbtFloat)
                {
                    // Returns the linear blend of x and y, i.e., x * (1 - a) + y * a.
                    float a = unionArrays[2][i].getFConst();
                    resultArray[i].setFConst(x * (1.0f - a) + y * a);
                }
                else  // 3rd parameter is EbtBool
                {
                    ASSERT(type == EbtBool);
                    // Selects which vector each returned component comes from.
                    // For a component of a that is false, the corresponding component of x is
                    // returned.
                    // For a component of a that is true, the corresponding component of y is
                    // returned.
                    bool a = unionArrays[2][i].getBConst();
                    resultArray[i].setFConst(a ? y : x);
                }
            }
            break;
        }

        case EOpSmoothStep:
        {
            ASSERT(basicType == EbtFloat);
            resultArray = new TConstantUnion[maxObjectSize];
            for (size_t i = 0; i < maxObjectSize; i++)
            {
                float edge0 = unionArrays[0][i].getFConst();
                float edge1 = unionArrays[1][i].getFConst();
                float x     = unionArrays[2][i].getFConst();
                // Results are undefined if edge0 >= edge1.
                if (edge0 >= edge1)
                {
                    UndefinedConstantFoldingError(loc, op, basicType, diagnostics, &resultArray[i]);
                }
                else
                {
                    // Returns 0.0 if x <= edge0 and 1.0 if x >= edge1 and performs smooth
                    // Hermite interpolation between 0 and 1 when edge0 < x < edge1.
                    float t = gl::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
                    resultArray[i].setFConst(t * t * (3.0f - 2.0f * t));
                }
            }
            break;
        }

        case EOpLdexp:
        {
            resultArray = new TConstantUnion[maxObjectSize];
            for (size_t i = 0; i < maxObjectSize; i++)
            {
                float x = unionArrays[0][i].getFConst();
                int exp = unionArrays[1][i].getIConst();
                if (exp > 128)
                {
                    UndefinedConstantFoldingError(loc, op, basicType, diagnostics, &resultArray[i]);
                }
                else
                {
                    resultArray[i].setFConst(gl::Ldexp(x, exp));
                }
            }
            break;
        }

        case EOpFaceforward:
        {
            ASSERT(basicType == EbtFloat);
            // genType faceforward(genType N, genType I, genType Nref) :
            //     If dot(Nref, I) < 0 return N, otherwise return -N.
            resultArray      = new TConstantUnion[maxObjectSize];
            float dotProduct = VectorDotProduct(unionArrays[2], unionArrays[1], maxObjectSize);
            for (size_t i = 0; i < maxObjectSize; i++)
            {
                if (dotProduct < 0)
                    resultArray[i].setFConst(unionArrays[0][i].getFConst());
                else
                    resultArray[i].setFConst(-unionArrays[0][i].getFConst());
            }
            break;
        }

        case EOpRefract:
        {
            ASSERT(basicType == EbtFloat);
            // genType refract(genType I, genType N, float eta) :
            //     For the incident vector I and surface normal N, and the ratio of indices of
            //     refraction eta,
            //     return the refraction vector. The result is computed by
            //         k = 1.0 - eta * eta * (1.0 - dot(N, I) * dot(N, I))
            //         if (k < 0.0)
            //             return genType(0.0)
            //         else
            //             return eta * I - (eta * dot(N, I) + sqrt(k)) * N
            resultArray      = new TConstantUnion[maxObjectSize];
            float dotProduct = VectorDotProduct(unionArrays[1], unionArrays[0], maxObjectSize);
            for (size_t i = 0; i < maxObjectSize; i++)
            {
                float eta = unionArrays[2][i].getFConst();
                float k   = 1.0f - eta * eta * (1.0f - dotProduct * dotProduct);
                if (k < 0.0f)
                    resultArray[i].setFConst(0.0f);
                else
                    resultArray[i].setFConst(eta * unionArrays[0][i].getFConst() -
                                             (eta * dotProduct + sqrtf(k)) *
                                                 unionArrays[1][i].getFConst());
            }
            break;
        }
        case EOpBitfieldExtract:
        {
            resultArray = new TConstantUnion[maxObjectSize];
            for (size_t i = 0; i < maxObjectSize; ++i)
            {
                int offset = unionArrays[1][0].getIConst();
                int bits   = unionArrays[2][0].getIConst();
                if (bits == 0)
                {
                    if (aggregate->getBasicType() == EbtInt)
                    {
                        resultArray[i].setIConst(0);
                    }
                    else
                    {
                        ASSERT(aggregate->getBasicType() == EbtUInt);
                        resultArray[i].setUConst(0);
                    }
                }
                else if (offset < 0 || bits < 0 || offset >= 32 || bits > 32 || offset + bits > 32)
                {
                    UndefinedConstantFoldingError(loc, op, aggregate->getBasicType(), diagnostics,
                                                  &resultArray[i]);
                }
                else
                {
                    // bits can be 32 here, so we need to avoid bit shift overflow.
                    uint32_t maskMsb = 1u << (bits - 1);
                    uint32_t mask    = ((maskMsb - 1u) | maskMsb) << offset;
                    if (aggregate->getBasicType() == EbtInt)
                    {
                        uint32_t value = static_cast<uint32_t>(unionArrays[0][i].getIConst());
                        uint32_t resultUnsigned = (value & mask) >> offset;
                        if ((resultUnsigned & maskMsb) != 0)
                        {
                            // The most significant bits (from bits+1 to the most significant bit)
                            // should be set to 1.
                            uint32_t higherBitsMask = ((1u << (32 - bits)) - 1u) << bits;
                            resultUnsigned |= higherBitsMask;
                        }
                        resultArray[i].setIConst(static_cast<int32_t>(resultUnsigned));
                    }
                    else
                    {
                        ASSERT(aggregate->getBasicType() == EbtUInt);
                        uint32_t value = unionArrays[0][i].getUConst();
                        resultArray[i].setUConst((value & mask) >> offset);
                    }
                }
            }
            break;
        }
        case EOpBitfieldInsert:
        {
            resultArray = new TConstantUnion[maxObjectSize];
            for (size_t i = 0; i < maxObjectSize; ++i)
            {
                int offset = unionArrays[2][0].getIConst();
                int bits   = unionArrays[3][0].getIConst();
                if (bits == 0)
                {
                    if (aggregate->getBasicType() == EbtInt)
                    {
                        int32_t base = unionArrays[0][i].getIConst();
                        resultArray[i].setIConst(base);
                    }
                    else
                    {
                        ASSERT(aggregate->getBasicType() == EbtUInt);
                        uint32_t base = unionArrays[0][i].getUConst();
                        resultArray[i].setUConst(base);
                    }
                }
                else if (offset < 0 || bits < 0 || offset >= 32 || bits > 32 || offset + bits > 32)
                {
                    UndefinedConstantFoldingError(loc, op, aggregate->getBasicType(), diagnostics,
                                                  &resultArray[i]);
                }
                else
                {
                    // bits can be 32 here, so we need to avoid bit shift overflow.
                    uint32_t maskMsb    = 1u << (bits - 1);
                    uint32_t insertMask = ((maskMsb - 1u) | maskMsb) << offset;
                    uint32_t baseMask   = ~insertMask;
                    if (aggregate->getBasicType() == EbtInt)
                    {
                        uint32_t base   = static_cast<uint32_t>(unionArrays[0][i].getIConst());
                        uint32_t insert = static_cast<uint32_t>(unionArrays[1][i].getIConst());
                        uint32_t resultUnsigned =
                            (base & baseMask) | ((insert << offset) & insertMask);
                        resultArray[i].setIConst(static_cast<int32_t>(resultUnsigned));
                    }
                    else
                    {
                        ASSERT(aggregate->getBasicType() == EbtUInt);
                        uint32_t base   = unionArrays[0][i].getUConst();
                        uint32_t insert = unionArrays[1][i].getUConst();
                        resultArray[i].setUConst((base & baseMask) |
                                                 ((insert << offset) & insertMask));
                    }
                }
            }
            break;
        }

        default:
            UNREACHABLE();
            return nullptr;
    }
    return resultArray;
}

}  // namespace sh
