/*-------------------------------------------------------------------------
 * drawElements Quality Program Random Shader Generator
 * ----------------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Binary ops.
 *//*--------------------------------------------------------------------*/

#include "rsgBinaryOps.hpp"
#include "rsgVariableManager.hpp"
#include "rsgUtils.hpp"
#include "deMath.h"

using std::vector;

namespace rsg
{

// CustomAbsOp and CustomBinaryOp are used to resolve float comparision corner case.
// This error happened when two floats with the same value were compared
// without using epsilon. If result of this comparisment influenced the
// output color then result and reference images could differ.
class CustomAbsOp : public Expression
{
public:
    CustomAbsOp(void);
    virtual ~CustomAbsOp(void);

    void setChild(Expression *expression);
    Expression *createNextChild(GeneratorState &state);
    void tokenize(GeneratorState &state, TokenStream &str) const;

    void evaluate(ExecutionContext &execCtx);
    ExecConstValueAccess getValue(void) const
    {
        return m_value.getValue(m_type);
    }

private:
    std::string m_function;
    VariableType m_type;
    ExecValueStorage m_value;
    Expression *m_child;
};

CustomAbsOp::CustomAbsOp(void) : m_function("abs"), m_type(VariableType::TYPE_FLOAT, 1), m_child(nullptr)
{
    m_value.setStorage(m_type);
}

CustomAbsOp::~CustomAbsOp(void)
{
    delete m_child;
}

void CustomAbsOp::setChild(Expression *expression)
{
    m_child = expression;
}

Expression *CustomAbsOp::createNextChild(GeneratorState &)
{
    DE_ASSERT(0);
    return nullptr;
}

void CustomAbsOp::tokenize(GeneratorState &state, TokenStream &str) const
{
    str << Token(m_function.c_str()) << Token::LEFT_PAREN;
    m_child->tokenize(state, str);
    str << Token::RIGHT_PAREN;
}

void CustomAbsOp::evaluate(ExecutionContext &execCtx)
{
    m_child->evaluate(execCtx);

    ExecConstValueAccess srcValue = m_child->getValue();
    ExecValueAccess dstValue      = m_value.getValue(m_type);

    for (int elemNdx = 0; elemNdx < m_type.getNumElements(); elemNdx++)
    {
        ExecConstValueAccess srcComp = srcValue.component(elemNdx);
        ExecValueAccess dstComp      = dstValue.component(elemNdx);

        for (int compNdx = 0; compNdx < EXEC_VEC_WIDTH; compNdx++)
            dstComp.asFloat(compNdx) = deFloatAbs(srcComp.asFloat(compNdx));
    }
}

typedef BinaryOp<5, ASSOCIATIVITY_LEFT> CustomBinaryBase;

// CustomBinaryOp and CustomAbsOp are used to resolve float comparision corner case.
// CustomBinaryOp supports addition and substraction as only those functionalities
// were needed.
template <typename ComputeValue>
class CustomBinaryOp : public CustomBinaryBase
{
public:
    CustomBinaryOp();
    virtual ~CustomBinaryOp(void)
    {
    }

    void setLeftValue(Expression *expression);
    void setRightValue(Expression *expression);

    void evaluate(ExecValueAccess dst, ExecConstValueAccess a, ExecConstValueAccess b);
};

template <typename ComputeValue>
CustomBinaryOp<ComputeValue>::CustomBinaryOp() : CustomBinaryBase(Token::PLUS)
{
    // By default add operation is assumed, for every other operation
    // separate constructor specialization should be implemented
    m_type = VariableType(VariableType::TYPE_FLOAT, 1);
    m_value.setStorage(m_type);
}

template <>
CustomBinaryOp<EvaluateSub>::CustomBinaryOp() : CustomBinaryBase(Token::MINUS)
{
    // Specialization for substraction
    m_type            = VariableType(VariableType::TYPE_FLOAT, 1);
    m_leftValueRange  = ValueRange(m_type);
    m_rightValueRange = ValueRange(m_type);
    m_value.setStorage(m_type);
}

template <>
CustomBinaryOp<EvaluateLessThan>::CustomBinaryOp() : CustomBinaryBase(Token::CMP_LT)
{
    // Specialization for less_then comparision
    m_type                 = VariableType(VariableType::TYPE_BOOL, 1);
    VariableType floatType = VariableType(VariableType::TYPE_FLOAT, 1);
    m_leftValueRange       = ValueRange(floatType);
    m_rightValueRange      = ValueRange(floatType);
    m_value.setStorage(m_type);
}

template <typename ComputeValue>
void CustomBinaryOp<ComputeValue>::setLeftValue(Expression *expression)
{
    m_leftValueExpr = expression;
}

template <typename ComputeValue>
void CustomBinaryOp<ComputeValue>::setRightValue(Expression *expression)
{
    m_rightValueExpr = expression;
}

template <typename ComputeValue>
void CustomBinaryOp<ComputeValue>::evaluate(ExecValueAccess dst, ExecConstValueAccess a, ExecConstValueAccess b)
{
    DE_ASSERT(dst.getType() == a.getType());
    DE_ASSERT(dst.getType() == b.getType());
    DE_ASSERT(dst.getType().getBaseType() == VariableType::TYPE_FLOAT);

    for (int elemNdx = 0; elemNdx < dst.getType().getNumElements(); elemNdx++)
    {
        for (int compNdx = 0; compNdx < EXEC_VEC_WIDTH; compNdx++)
            dst.component(elemNdx).asFloat(compNdx) =
                ComputeValue()(a.component(elemNdx).asFloat(compNdx), b.component(elemNdx).asFloat(compNdx));
    }
}

template <>
void CustomBinaryOp<EvaluateLessThan>::evaluate(ExecValueAccess dst, ExecConstValueAccess a, ExecConstValueAccess b)
{
    DE_ASSERT(a.getType() == b.getType());
    DE_ASSERT(dst.getType().getBaseType() == VariableType::TYPE_BOOL);

    for (int elemNdx = 0; elemNdx < dst.getType().getNumElements(); elemNdx++)
    {
        for (int compNdx = 0; compNdx < EXEC_VEC_WIDTH; compNdx++)
            dst.component(elemNdx).asBool(compNdx) =
                EvaluateLessThan()(a.component(elemNdx).asFloat(compNdx), b.component(elemNdx).asFloat(compNdx));
    }
}

template <int Precedence, Associativity Assoc>
BinaryOp<Precedence, Assoc>::BinaryOp(Token::Type operatorToken)
    : m_operator(operatorToken)
    , m_leftValueRange(m_type)
    , m_rightValueRange(m_type)
    , m_leftValueExpr(nullptr)
    , m_rightValueExpr(nullptr)
{
}

template <int Precedence, Associativity Assoc>
BinaryOp<Precedence, Assoc>::~BinaryOp(void)
{
    delete m_leftValueExpr;
    delete m_rightValueExpr;
}

template <int Precedence, Associativity Assoc>
Expression *BinaryOp<Precedence, Assoc>::createNextChild(GeneratorState &state)
{
    int leftPrec  = Assoc == ASSOCIATIVITY_LEFT ? Precedence : Precedence - 1;
    int rightPrec = Assoc == ASSOCIATIVITY_LEFT ? Precedence - 1 : Precedence;

    if (m_rightValueExpr == nullptr)
    {
        state.pushPrecedence(rightPrec);
        m_rightValueExpr = Expression::createRandom(state, m_rightValueRange.asAccess());
        state.popPrecedence();
        return m_rightValueExpr;
    }
    else if (m_leftValueExpr == nullptr)
    {
        state.pushPrecedence(leftPrec);
        m_leftValueExpr = Expression::createRandom(state, m_leftValueRange.asAccess());
        state.popPrecedence();
        return m_leftValueExpr;
    }
    else
    {
        // Check for corrner cases
        switch (m_operator)
        {
        case Token::CMP_LE:
        {
            // When comparing two floats epsilon should be included
            // to eliminate the risk that we get different results
            // because of precission error
            VariableType floatType(VariableType::TYPE_FLOAT, 1);
            if (m_rightValueRange.getType() == floatType)
            {
                FloatLiteral *epsilonLiteral = new FloatLiteral(0.001f);

                typedef CustomBinaryOp<EvaluateAdd> CustomAddOp;
                CustomAddOp *addOperation = new CustomAddOp();
                addOperation->setLeftValue(m_rightValueExpr);
                addOperation->setRightValue(epsilonLiteral);

                // add epsilon to right-hand side
                m_rightValueExpr = addOperation;
            }
            break;
        }
        case Token::CMP_GE:
        {
            // When comparing two floats epsilon should be included
            // to eliminate the risk that we get different results
            // because of precission error
            VariableType floatType(VariableType::TYPE_FLOAT, 1);
            if (m_leftValueRange.getType() == floatType)
            {
                FloatLiteral *epsilonLiteral = new FloatLiteral(0.001f);

                typedef CustomBinaryOp<EvaluateAdd> CustomAddOp;
                CustomAddOp *addOperation = new CustomAddOp();
                addOperation->setLeftValue(m_leftValueExpr);
                addOperation->setRightValue(epsilonLiteral);

                // add epsilon to left-hand side
                m_leftValueExpr = addOperation;
            }
            break;
        }
        case Token::CMP_EQ:
        {
            // When comparing two floats epsilon should be included
            // to eliminate the risk that we get different results
            // because of precission error
            VariableType floatType(VariableType::TYPE_FLOAT, 1);
            if (m_leftValueRange.getType() == floatType)
            {
                VariableType boolType(VariableType::TYPE_BOOL, 1);
                const ValueRange boolRange(boolType);

                ParenOp *parenRight = new ParenOp(state, boolRange);
                parenRight->setChild(m_rightValueExpr);

                typedef CustomBinaryOp<EvaluateSub> CustomSubOp;
                CustomSubOp *subOperation = new CustomSubOp();
                subOperation->setLeftValue(m_leftValueExpr);
                subOperation->setRightValue(parenRight);

                CustomAbsOp *absOperation = new CustomAbsOp();
                absOperation->setChild(subOperation);
                FloatLiteral *epsilonLiteral = new FloatLiteral(0.001f);

                typedef CustomBinaryOp<EvaluateLessThan> CustomLessThanOp;
                CustomLessThanOp *lessOperation = new CustomLessThanOp();
                lessOperation->setLeftValue(absOperation);
                lessOperation->setRightValue(epsilonLiteral);

                ParenOp *parenOperation = new ParenOp(state, boolRange);
                parenOperation->setChild(lessOperation);
                BoolLiteral *trueLiteral = new BoolLiteral(true);

                // EQ operation cant be removed so it is replaced with:
                // ((abs(lhs-rhs) < epsilon) == true).
                m_leftValueExpr  = parenOperation;
                m_rightValueExpr = trueLiteral;
            }
            break;
        }
        default:
            break;
        }

        return nullptr;
    }
}

template <int Precedence, Associativity Assoc>
float BinaryOp<Precedence, Assoc>::getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange)
{
    if (state.getPrecedence() < Precedence)
        return 0.0f;

    int availableLevels = state.getShaderParameters().maxExpressionDepth - state.getExpressionDepth();

    if (valueRange.getType().isVoid())
        return availableLevels >= 2 ? unusedValueWeight : 0.0f;

    if (availableLevels < getConservativeValueExprDepth(state, valueRange) + 1)
        return 0.0f;

    return 1.0f;
}

template <int Precedence, Associativity Assoc>
void BinaryOp<Precedence, Assoc>::tokenize(GeneratorState &state, TokenStream &str) const
{
    m_leftValueExpr->tokenize(state, str);
    str << m_operator;
    m_rightValueExpr->tokenize(state, str);
}

template <int Precedence, Associativity Assoc>
void BinaryOp<Precedence, Assoc>::evaluate(ExecutionContext &execCtx)
{
    m_leftValueExpr->evaluate(execCtx);
    m_rightValueExpr->evaluate(execCtx);

    ExecConstValueAccess leftVal  = m_leftValueExpr->getValue();
    ExecConstValueAccess rightVal = m_rightValueExpr->getValue();
    ExecValueAccess dst           = m_value.getValue(m_type);

    evaluate(dst, leftVal, rightVal);
}

template <int Precedence, bool Float, bool Int, bool Bool, class ComputeValueRange, class EvaluateComp>
BinaryVecOp<Precedence, Float, Int, Bool, ComputeValueRange, EvaluateComp>::BinaryVecOp(
    GeneratorState &state, Token::Type operatorToken, ConstValueRangeAccess inValueRange)
    : BinaryOp<Precedence, ASSOCIATIVITY_LEFT>(operatorToken)
{
    ValueRange valueRange = inValueRange;

    if (valueRange.getType().isVoid())
    {
        int availableLevels = state.getShaderParameters().maxExpressionDepth - state.getExpressionDepth();
        vector<VariableType::Type> baseTypes;

        if (Float)
            baseTypes.push_back(VariableType::TYPE_FLOAT);
        if (Int)
            baseTypes.push_back(VariableType::TYPE_INT);
        if (Bool)
            baseTypes.push_back(VariableType::TYPE_BOOL);

        VariableType::Type baseType = state.getRandom().choose<VariableType::Type>(baseTypes.begin(), baseTypes.end());
        int numElements             = state.getRandom().getInt(1, availableLevels >= 3 ? 4 : 1);

        valueRange = ValueRange(VariableType(baseType, numElements));
        computeRandomValueRange(state, valueRange.asAccess());
    }

    // Choose type, allocate storage for execution
    this->m_type = valueRange.getType();
    this->m_value.setStorage(this->m_type);

    // Initialize storage for value ranges
    this->m_rightValueRange = ValueRange(this->m_type);
    this->m_leftValueRange  = ValueRange(this->m_type);

    VariableType::Type baseType = this->m_type.getBaseType();

    // Compute range for b that satisfies requested value range
    for (int elemNdx = 0; elemNdx < this->m_type.getNumElements(); elemNdx++)
    {
        ConstValueRangeAccess dst = valueRange.asAccess().component(elemNdx);
        ValueRangeAccess a        = this->m_leftValueRange.asAccess().component(
            elemNdx); // \todo [2011-03-25 pyry] Commutative: randomize inputs
        ValueRangeAccess b = this->m_rightValueRange.asAccess().component(elemNdx);

        // Just pass undefined ranges
        if ((baseType == VariableType::TYPE_FLOAT || baseType == VariableType::TYPE_INT) && isUndefinedValueRange(dst))
        {
            a.getMin() = dst.getMin().value();
            b.getMin() = dst.getMin().value();
            a.getMax() = dst.getMax().value();
            b.getMax() = dst.getMax().value();
            continue;
        }

        if (baseType == VariableType::TYPE_FLOAT)
            ComputeValueRange()(state.getRandom(), dst.getMin().asFloat(), dst.getMax().asFloat(), a.getMin().asFloat(),
                                a.getMax().asFloat(), b.getMin().asFloat(), b.getMax().asFloat());
        else if (baseType == VariableType::TYPE_INT)
            ComputeValueRange()(state.getRandom(), dst.getMin().asInt(), dst.getMax().asInt(), a.getMin().asInt(),
                                a.getMax().asInt(), b.getMin().asInt(), b.getMax().asInt());
        else
        {
            DE_ASSERT(baseType == VariableType::TYPE_BOOL);
            ComputeValueRange()(state.getRandom(), dst.getMin().asBool(), dst.getMax().asBool(), a.getMin().asBool(),
                                a.getMax().asBool(), b.getMin().asBool(), b.getMax().asBool());
        }
    }
}

template <int Precedence, bool Float, bool Int, bool Bool, class ComputeValueRange, class EvaluateComp>
BinaryVecOp<Precedence, Float, Int, Bool, ComputeValueRange, EvaluateComp>::~BinaryVecOp(void)
{
}

template <int Precedence, bool Float, bool Int, bool Bool, class ComputeValueRange, class EvaluateComp>
void BinaryVecOp<Precedence, Float, Int, Bool, ComputeValueRange, EvaluateComp>::evaluate(ExecValueAccess dst,
                                                                                          ExecConstValueAccess a,
                                                                                          ExecConstValueAccess b)
{
    DE_ASSERT(dst.getType() == a.getType());
    DE_ASSERT(dst.getType() == b.getType());
    switch (dst.getType().getBaseType())
    {
    case VariableType::TYPE_FLOAT:
        for (int elemNdx = 0; elemNdx < dst.getType().getNumElements(); elemNdx++)
        {
            for (int compNdx = 0; compNdx < EXEC_VEC_WIDTH; compNdx++)
                dst.component(elemNdx).asFloat(compNdx) =
                    EvaluateComp()(a.component(elemNdx).asFloat(compNdx), b.component(elemNdx).asFloat(compNdx));
        }
        break;

    case VariableType::TYPE_INT:
        for (int elemNdx = 0; elemNdx < dst.getType().getNumElements(); elemNdx++)
        {
            for (int compNdx = 0; compNdx < EXEC_VEC_WIDTH; compNdx++)
                dst.component(elemNdx).asInt(compNdx) =
                    EvaluateComp()(a.component(elemNdx).asInt(compNdx), b.component(elemNdx).asInt(compNdx));
        }
        break;

    default:
        DE_ASSERT(false); // Invalid type for multiplication
    }
}

void ComputeMulRange::operator()(de::Random &rnd, float dstMin, float dstMax, float &aMin, float &aMax, float &bMin,
                                 float &bMax) const
{
    const float minScale     = 0.25f;
    const float maxScale     = 2.0f;
    const float subRangeStep = 0.25f;
    const float scaleStep    = 0.25f;

    float scale     = getQuantizedFloat(rnd, minScale, maxScale, scaleStep);
    float scaledMin = dstMin / scale;
    float scaledMax = dstMax / scale;

    // Quantize scaled value range if possible
    if (!quantizeFloatRange(scaledMin, scaledMax))
    {
        // Fall back to 1.0 as a scale
        scale     = 1.0f;
        scaledMin = dstMin;
        scaledMax = dstMax;
    }

    float subRangeLen = getQuantizedFloat(rnd, 0.0f, scaledMax - scaledMin, subRangeStep);
    aMin              = scaledMin + getQuantizedFloat(rnd, 0.0f, (scaledMax - scaledMin) - subRangeLen, subRangeStep);
    aMax              = aMin + subRangeLen;

    // Find scale range
    bMin = scale;
    bMax = scale;
    for (int i = 0; i < 5; i++)
    {
        if (de::inBounds(aMin * (scale - (float)i * scaleStep), dstMin, dstMax) &&
            de::inBounds(aMax * (scale - (float)i * scaleStep), dstMin, dstMax))
            bMin = scale - (float)i * scaleStep;

        if (de::inBounds(aMin * (scale + (float)i * scaleStep), dstMin, dstMax) &&
            de::inBounds(aMax * (scale + (float)i * scaleStep), dstMin, dstMax))
            bMax = scale + (float)i * scaleStep;
    }

    // Negative scale?
    if (rnd.getBool())
    {
        std::swap(aMin, aMax);
        std::swap(bMin, bMax);
        aMin *= -1.0f;
        aMax *= -1.0f;
        bMin *= -1.0f;
        bMax *= -1.0f;
    }

#if defined(DE_DEBUG)
    const float eps = 0.001f;
    DE_ASSERT(aMin <= aMax && bMin <= bMax);
    DE_ASSERT(de::inRange(aMin * bMin, dstMin - eps, dstMax + eps));
    DE_ASSERT(de::inRange(aMin * bMax, dstMin - eps, dstMax + eps));
    DE_ASSERT(de::inRange(aMax * bMin, dstMin - eps, dstMax + eps));
    DE_ASSERT(de::inRange(aMax * bMax, dstMin - eps, dstMax + eps));
#endif
}

void ComputeMulRange::operator()(de::Random &rnd, int dstMin, int dstMax, int &aMin, int &aMax, int &bMin,
                                 int &bMax) const
{
    DE_UNREF(rnd);
    aMin = dstMin;
    aMax = dstMax;
    bMin = 1;
    bMax = 1;
}

MulOp::MulOp(GeneratorState &state, ConstValueRangeAccess valueRange) : MulBase(state, Token::MUL, valueRange)
{
}

float MulOp::getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange)
{
    if (valueRange.getType().isVoid() || valueRange.getType().isFloatOrVec() || valueRange.getType().isIntOrVec())
        return MulBase::getWeight(state, valueRange);
    else
        return 0.0f;
}

template <typename T>
void ComputeAddRange::operator()(de::Random &random, T dstMin, T dstMax, T &aMin, T &aMax, T &bMin, T &bMax) const
{
    struct GetRandom
    {
        int operator()(de::Random &rnd, int min, int max) const
        {
            return rnd.getInt(min, max);
        }
        float operator()(de::Random &rnd, float min, float max) const
        {
            return getQuantizedFloat(rnd, min, max, 0.5f);
        }
    };

    T rangeLen    = dstMax - dstMin;
    T subRangeLen = GetRandom()(random, T(0), rangeLen);
    T aOffset     = GetRandom()(random, T(-8), T(8));

    aMin = dstMin + aOffset;
    aMax = aMin + subRangeLen;

    bMin = -aOffset;
    bMax = -aOffset + (rangeLen - subRangeLen);

#if defined(DE_DEBUG)
    T eps = T(0.001);
    DE_ASSERT(aMin <= aMax && bMin <= bMax);
    DE_ASSERT(de::inRange(aMin + bMin, dstMin - eps, dstMax + eps));
    DE_ASSERT(de::inRange(aMin + bMax, dstMin - eps, dstMax + eps));
    DE_ASSERT(de::inRange(aMax + bMin, dstMin - eps, dstMax + eps));
    DE_ASSERT(de::inRange(aMax + bMax, dstMin - eps, dstMax + eps));
#endif
}

template <>
void ComputeAddRange::operator()<bool>(de::Random &, bool, bool, bool &, bool &, bool &, bool &) const
{
    DE_ASSERT(false);
}

AddOp::AddOp(GeneratorState &state, ConstValueRangeAccess valueRange) : AddBase(state, Token::PLUS, valueRange)
{
}

float AddOp::getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange)
{
    if (valueRange.getType().isVoid() || valueRange.getType().isFloatOrVec() || valueRange.getType().isIntOrVec())
        return AddBase::getWeight(state, valueRange);
    else
        return 0.0f;
}

template <typename T>
void ComputeSubRange::operator()(de::Random &random, T dstMin, T dstMax, T &aMin, T &aMax, T &bMin, T &bMax) const
{
    struct GetRandom
    {
        int operator()(de::Random &rnd, int min, int max) const
        {
            return rnd.getInt(min, max);
        }
        float operator()(de::Random &rnd, float min, float max) const
        {
            return getQuantizedFloat(rnd, min, max, 0.5f);
        }
    };

    T rangeLen    = dstMax - dstMin;
    T subRangeLen = GetRandom()(random, T(0), rangeLen);
    T aOffset     = GetRandom()(random, T(-8), T(8));

    aMin = dstMin + aOffset;
    aMax = aMin + subRangeLen;

    bMin = aOffset - (rangeLen - subRangeLen);
    bMax = aOffset;

#if defined(DE_DEBUG)
    T eps = T(0.001);
    DE_ASSERT(aMin <= aMax && bMin <= bMax);
    DE_ASSERT(de::inRange(aMin - bMin, dstMin - eps, dstMax + eps));
    DE_ASSERT(de::inRange(aMin - bMax, dstMin - eps, dstMax + eps));
    DE_ASSERT(de::inRange(aMax - bMin, dstMin - eps, dstMax + eps));
    DE_ASSERT(de::inRange(aMax - bMax, dstMin - eps, dstMax + eps));
#endif
}

template <>
void ComputeSubRange::operator()<bool>(de::Random &, bool, bool, bool &, bool &, bool &, bool &) const
{
    DE_ASSERT(false);
}

SubOp::SubOp(GeneratorState &state, ConstValueRangeAccess valueRange) : SubBase(state, Token::MINUS, valueRange)
{
}

float SubOp::getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange)
{
    if (valueRange.getType().isVoid() || valueRange.getType().isFloatOrVec() || valueRange.getType().isIntOrVec())
        return SubBase::getWeight(state, valueRange);
    else
        return 0.0f;
}

template <class ComputeValueRange, class EvaluateComp>
RelationalOp<ComputeValueRange, EvaluateComp>::RelationalOp(GeneratorState &state, Token::Type operatorToken,
                                                            ConstValueRangeAccess inValueRange)
    : BinaryOp<7, ASSOCIATIVITY_LEFT>(operatorToken)
{
    ValueRange valueRange = inValueRange;

    if (valueRange.getType().isVoid())
    {
        valueRange = ValueRange(VariableType(VariableType::TYPE_BOOL, 1));
        computeRandomValueRange(state, valueRange.asAccess());
    }

    // Choose type, allocate storage for execution
    this->m_type = valueRange.getType();
    this->m_value.setStorage(this->m_type);

    // Choose random input type
    VariableType::Type inBaseTypes[] = {VariableType::TYPE_FLOAT, VariableType::TYPE_INT};
    VariableType::Type inBaseType =
        state.getRandom().choose<VariableType::Type>(&inBaseTypes[0], &inBaseTypes[DE_LENGTH_OF_ARRAY(inBaseTypes)]);

    // Initialize storage for input value ranges
    this->m_rightValueRange = ValueRange(VariableType(inBaseType, 1));
    this->m_leftValueRange  = ValueRange(VariableType(inBaseType, 1));

    // Compute range for b that satisfies requested value range
    {
        bool dstMin        = valueRange.getMin().asBool();
        bool dstMax        = valueRange.getMax().asBool();
        ValueRangeAccess a = this->m_leftValueRange.asAccess();
        ValueRangeAccess b = this->m_rightValueRange.asAccess();

        if (inBaseType == VariableType::TYPE_FLOAT)
            ComputeValueRange()(state.getRandom(), dstMin, dstMax, a.getMin().asFloat(), a.getMax().asFloat(),
                                b.getMin().asFloat(), b.getMax().asFloat());
        else if (inBaseType == VariableType::TYPE_INT)
            ComputeValueRange()(state.getRandom(), dstMin, dstMax, a.getMin().asInt(), a.getMax().asInt(),
                                b.getMin().asInt(), b.getMax().asInt());
    }
}

template <class ComputeValueRange, class EvaluateComp>
RelationalOp<ComputeValueRange, EvaluateComp>::~RelationalOp(void)
{
}

template <class ComputeValueRange, class EvaluateComp>
void RelationalOp<ComputeValueRange, EvaluateComp>::evaluate(ExecValueAccess dst, ExecConstValueAccess a,
                                                             ExecConstValueAccess b)
{
    DE_ASSERT(a.getType() == b.getType());
    switch (a.getType().getBaseType())
    {
    case VariableType::TYPE_FLOAT:
        for (int compNdx = 0; compNdx < EXEC_VEC_WIDTH; compNdx++)
            dst.asBool(compNdx) = EvaluateComp()(a.asFloat(compNdx), b.asFloat(compNdx));
        break;

    case VariableType::TYPE_INT:
        for (int compNdx = 0; compNdx < EXEC_VEC_WIDTH; compNdx++)
            dst.asBool(compNdx) = EvaluateComp()(a.asInt(compNdx), b.asInt(compNdx));
        break;

    default:
        DE_ASSERT(false);
    }
}

template <class ComputeValueRange, class EvaluateComp>
float RelationalOp<ComputeValueRange, EvaluateComp>::getWeight(const GeneratorState &state,
                                                               ConstValueRangeAccess valueRange)
{
    if (!state.getProgramParameters().useComparisonOps)
        return 0.0f;

    if (valueRange.getType().isVoid() ||
        (valueRange.getType().getBaseType() == VariableType::TYPE_BOOL && valueRange.getType().getNumElements() == 1))
        return BinaryOp<7, ASSOCIATIVITY_LEFT>::getWeight(state, valueRange);
    else
        return 0.0f;
}

namespace
{

template <typename T>
T getStep(void);
template <>
inline float getStep(void)
{
    return 0.25f;
}
template <>
inline int getStep(void)
{
    return 1;
}

} // namespace

template <typename T>
void ComputeLessThanRange::operator()(de::Random &rnd, bool dstMin, bool dstMax, T &aMin, T &aMax, T &bMin,
                                      T &bMax) const
{
    struct GetRandom
    {
        int operator()(de::Random &random, int min, int max) const
        {
            return random.getInt(min, max);
        }
        float operator()(de::Random &random, float min, float max) const
        {
            return getQuantizedFloat(random, min, max, getStep<float>());
        }
    };

    // One random range
    T rLen = GetRandom()(rnd, T(0), T(8));
    T rMin = GetRandom()(rnd, T(-4), T(4));
    T rMax = rMin + rLen;

    if (dstMin == false && dstMax == true)
    {
        // Both values are possible, use same range for both inputs
        aMin = rMin;
        aMax = rMax;
        bMin = rMin;
        bMax = rMax;
    }
    else if (dstMin == true && dstMax == true)
    {
        // Compute range that is less than rMin..rMax
        T aLen = GetRandom()(rnd, T(0), T(8) - rLen);

        aMax = rMin - getStep<T>();
        aMin = aMax - aLen;

        bMin = rMin;
        bMax = rMax;
    }
    else
    {
        // Compute range that is greater than or equal to rMin..rMax
        T aLen = GetRandom()(rnd, T(0), T(8) - rLen);

        aMin = rMax;
        aMax = aMin + aLen;

        bMin = rMin;
        bMax = rMax;
    }
}

LessThanOp::LessThanOp(GeneratorState &state, ConstValueRangeAccess valueRange)
    : LessThanBase(state, Token::CMP_LT, valueRange)
{
}

float LessThanOp::getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange)
{
    return LessThanBase::getWeight(state, valueRange);
}

template <typename T>
void ComputeLessOrEqualRange::operator()(de::Random &rnd, bool dstMin, bool dstMax, T &aMin, T &aMax, T &bMin,
                                         T &bMax) const
{
    struct GetRandom
    {
        int operator()(de::Random &random, int min, int max) const
        {
            return random.getInt(min, max);
        }
        float operator()(de::Random &random, float min, float max) const
        {
            return getQuantizedFloat(random, min, max, getStep<float>());
        }
    };

    // One random range
    T rLen = GetRandom()(rnd, T(0), T(8));
    T rMin = GetRandom()(rnd, T(-4), T(4));
    T rMax = rMin + rLen;

    if (dstMin == false && dstMax == true)
    {
        // Both values are possible, use same range for both inputs
        aMin = rMin;
        aMax = rMax;
        bMin = rMin;
        bMax = rMax;
    }
    else if (dstMin == true && dstMax == true)
    {
        // Compute range that is less than or equal to rMin..rMax
        T aLen = GetRandom()(rnd, T(0), T(8) - rLen);

        aMax = rMin;
        aMin = aMax - aLen;

        bMin = rMin;
        bMax = rMax;
    }
    else
    {
        // Compute range that is greater than rMin..rMax
        T aLen = GetRandom()(rnd, T(0), T(8) - rLen);

        aMin = rMax + getStep<T>();
        aMax = aMin + aLen;

        bMin = rMin;
        bMax = rMax;
    }
}

LessOrEqualOp::LessOrEqualOp(GeneratorState &state, ConstValueRangeAccess valueRange)
    : LessOrEqualBase(state, Token::CMP_LE, valueRange)
{
}

float LessOrEqualOp::getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange)
{
    return LessOrEqualBase::getWeight(state, valueRange);
}

GreaterThanOp::GreaterThanOp(GeneratorState &state, ConstValueRangeAccess valueRange)
    : GreaterThanBase(state, Token::CMP_GT, valueRange)
{
}

float GreaterThanOp::getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange)
{
    return GreaterThanBase::getWeight(state, valueRange);
}

GreaterOrEqualOp::GreaterOrEqualOp(GeneratorState &state, ConstValueRangeAccess valueRange)
    : GreaterOrEqualBase(state, Token::CMP_GE, valueRange)
{
}

float GreaterOrEqualOp::getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange)
{
    return GreaterOrEqualBase::getWeight(state, valueRange);
}

namespace
{

template <bool IsEqual, typename T>
void computeEqualityValueRange(de::Random &rnd, bool dstMin, bool dstMax, T &aMin, T &aMax, T &bMin, T &bMax)
{
    if (dstMin == false && dstMax == true)
        ComputeLessThanRange()(rnd, false, true, aMin, aMax, bMin, bMax);
    else if (IsEqual && dstMin == false)
        ComputeLessThanRange()(rnd, true, true, aMin, aMax, bMin, bMax);
    else if (!IsEqual && dstMin == true)
        ComputeLessThanRange()(rnd, true, true, aMin, aMax, bMin, bMax);
    else
    {
        // Must have exactly same values.
        struct GetRandom
        {
            int operator()(de::Random &random, int min, int max) const
            {
                return random.getInt(min, max);
            }
            float operator()(de::Random &random, float min, float max) const
            {
                return getQuantizedFloat(random, min, max, 0.5f);
            }
        };

        T val = GetRandom()(rnd, T(-1), T(1));

        aMin = val;
        aMax = val;
        bMin = val;
        bMax = val;
    }
}

template <>
void computeEqualityValueRange<true, bool>(de::Random &rnd, bool dstMin, bool dstMax, bool &aMin, bool &aMax,
                                           bool &bMin, bool &bMax)
{
    if (dstMin == false && dstMax == true)
    {
        aMin = false;
        aMax = true;
        bMin = false;
        bMax = true;
    }
    else if (dstMin == false)
    {
        DE_ASSERT(dstMax == false);
        bool val = rnd.getBool();

        aMin = val;
        aMax = val;
        bMin = !val;
        bMax = !val;
    }
    else
    {
        DE_ASSERT(dstMin == true && dstMax == true);
        bool val = rnd.getBool();

        aMin = val;
        aMax = val;
        bMin = val;
        bMax = val;
    }
}

template <>
void computeEqualityValueRange<false, bool>(de::Random &rnd, bool dstMin, bool dstMax, bool &aMin, bool &aMax,
                                            bool &bMin, bool &bMax)
{
    if (dstMin == false && dstMax == true)
        computeEqualityValueRange<true>(rnd, dstMin, dstMax, aMin, aMax, bMin, bMax);
    else
        computeEqualityValueRange<true>(rnd, !dstMin, !dstMax, aMin, aMax, bMin, bMax);
}

} // namespace

template <bool IsEqual>
EqualityComparisonOp<IsEqual>::EqualityComparisonOp(GeneratorState &state, ConstValueRangeAccess inValueRange)
    : BinaryOp<8, ASSOCIATIVITY_LEFT>(IsEqual ? Token::CMP_EQ : Token::CMP_NE)
{
    ValueRange valueRange = inValueRange;

    if (valueRange.getType().isVoid())
    {
        valueRange = ValueRange(VariableType(VariableType::TYPE_BOOL, 1));
        computeRandomValueRange(state, valueRange.asAccess());
    }

    // Choose type, allocate storage for execution
    this->m_type = valueRange.getType();
    this->m_value.setStorage(this->m_type);

    // Choose random input type
    VariableType::Type inBaseTypes[] = {VariableType::TYPE_FLOAT, VariableType::TYPE_INT};
    VariableType::Type inBaseType =
        state.getRandom().choose<VariableType::Type>(&inBaseTypes[0], &inBaseTypes[DE_LENGTH_OF_ARRAY(inBaseTypes)]);
    int availableLevels = state.getShaderParameters().maxExpressionDepth - state.getExpressionDepth();
    int numElements     = state.getRandom().getInt(1, availableLevels >= 3 ? 4 : 1);

    // Initialize storage for input value ranges
    this->m_rightValueRange = ValueRange(VariableType(inBaseType, numElements));
    this->m_leftValueRange  = ValueRange(VariableType(inBaseType, numElements));

    // Compute range for b that satisfies requested value range
    for (int elementNdx = 0; elementNdx < numElements; elementNdx++)
    {
        bool dstMin = valueRange.getMin().asBool();
        bool dstMax = valueRange.getMax().asBool();

        ValueRangeAccess a = this->m_leftValueRange.asAccess().component(elementNdx);
        ValueRangeAccess b = this->m_rightValueRange.asAccess().component(elementNdx);

        if (inBaseType == VariableType::TYPE_FLOAT)
            computeEqualityValueRange<IsEqual>(state.getRandom(), dstMin, dstMax, a.getMin().asFloat(),
                                               a.getMax().asFloat(), b.getMin().asFloat(), b.getMax().asFloat());
        else if (inBaseType == VariableType::TYPE_INT)
            computeEqualityValueRange<IsEqual>(state.getRandom(), dstMin, dstMax, a.getMin().asInt(),
                                               a.getMax().asInt(), b.getMin().asInt(), b.getMax().asInt());
        else
        {
            DE_ASSERT(inBaseType == VariableType::TYPE_BOOL);
            computeEqualityValueRange<IsEqual>(state.getRandom(), dstMin, dstMax, a.getMin().asBool(),
                                               a.getMax().asBool(), b.getMin().asBool(), b.getMax().asBool());
        }
    }
}

template <bool IsEqual>
float EqualityComparisonOp<IsEqual>::getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange)
{
    if (!state.getProgramParameters().useComparisonOps)
        return 0.0f;

    // \todo [2011-06-13 pyry] Weight down cases that would force constant inputs.

    if (valueRange.getType().isVoid() ||
        (valueRange.getType().getBaseType() == VariableType::TYPE_BOOL && valueRange.getType().getNumElements() == 1))
        return BinaryOp<8, ASSOCIATIVITY_LEFT>::getWeight(state, valueRange);
    else
        return 0.0f;
}

namespace
{

template <bool IsEqual>
struct EqualityCompare
{
    template <typename T>
    static bool compare(T a, T b);
    static bool combine(bool a, bool b);
};

template <>
template <typename T>
inline bool EqualityCompare<true>::compare(T a, T b)
{
    return a == b;
}

template <>
inline bool EqualityCompare<true>::combine(bool a, bool b)
{
    return a && b;
}

template <>
template <typename T>
inline bool EqualityCompare<false>::compare(T a, T b)
{
    return a != b;
}

template <>
inline bool EqualityCompare<false>::combine(bool a, bool b)
{
    return a || b;
}

} // namespace

template <bool IsEqual>
void EqualityComparisonOp<IsEqual>::evaluate(ExecValueAccess dst, ExecConstValueAccess a, ExecConstValueAccess b)
{
    DE_ASSERT(a.getType() == b.getType());

    switch (a.getType().getBaseType())
    {
    case VariableType::TYPE_FLOAT:
        for (int compNdx = 0; compNdx < EXEC_VEC_WIDTH; compNdx++)
        {
            bool result = IsEqual ? true : false;

            for (int elemNdx = 0; elemNdx < a.getType().getNumElements(); elemNdx++)
                result = EqualityCompare<IsEqual>::combine(
                    result, EqualityCompare<IsEqual>::compare(a.component(elemNdx).asFloat(compNdx),
                                                              b.component(elemNdx).asFloat(compNdx)));

            dst.asBool(compNdx) = result;
        }
        break;

    case VariableType::TYPE_INT:
        for (int compNdx = 0; compNdx < EXEC_VEC_WIDTH; compNdx++)
        {
            bool result = IsEqual ? true : false;

            for (int elemNdx = 0; elemNdx < a.getType().getNumElements(); elemNdx++)
                result = EqualityCompare<IsEqual>::combine(
                    result, EqualityCompare<IsEqual>::compare(a.component(elemNdx).asInt(compNdx),
                                                              b.component(elemNdx).asInt(compNdx)));

            dst.asBool(compNdx) = result;
        }
        break;

    case VariableType::TYPE_BOOL:
        for (int compNdx = 0; compNdx < EXEC_VEC_WIDTH; compNdx++)
        {
            bool result = IsEqual ? true : false;

            for (int elemNdx = 0; elemNdx < a.getType().getNumElements(); elemNdx++)
                result = EqualityCompare<IsEqual>::combine(
                    result, EqualityCompare<IsEqual>::compare(a.component(elemNdx).asBool(compNdx),
                                                              b.component(elemNdx).asBool(compNdx)));

            dst.asBool(compNdx) = result;
        }
        break;

    default:
        DE_ASSERT(false);
    }
}

EqualOp::EqualOp(GeneratorState &state, ConstValueRangeAccess valueRange)
    : EqualityComparisonOp<true>(state, valueRange)
{
}

float EqualOp::getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange)
{
    return EqualityComparisonOp<true>::getWeight(state, valueRange);
}

NotEqualOp::NotEqualOp(GeneratorState &state, ConstValueRangeAccess valueRange)
    : EqualityComparisonOp<false>(state, valueRange)
{
}

float NotEqualOp::getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange)
{
    return EqualityComparisonOp<false>::getWeight(state, valueRange);
}

} // namespace rsg
