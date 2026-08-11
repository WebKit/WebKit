#ifndef _RSGBUILTINFUNCTIONS_HPP
#define _RSGBUILTINFUNCTIONS_HPP
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
 * \brief Built-in Functions.
 *//*--------------------------------------------------------------------*/

#include "rsgDefs.hpp"
#include "rsgExpression.hpp"
#include "rsgUtils.hpp"
#include "deMath.h"

namespace rsg
{

// Template for built-in functions with form "GenType func(GenType val)".
template <class GetValueRangeWeight, class ComputeValueRange, class Evaluate>
class UnaryBuiltinVecFunc : public Expression
{
public:
    UnaryBuiltinVecFunc(GeneratorState &state, const char *function, ConstValueRangeAccess valueRange);
    virtual ~UnaryBuiltinVecFunc(void);

    Expression *createNextChild(GeneratorState &state);
    void tokenize(GeneratorState &state, TokenStream &str) const;

    void evaluate(ExecutionContext &execCtx);
    ExecConstValueAccess getValue(void) const
    {
        return m_value.getValue(m_inValueRange.getType());
    }

    static float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange);

private:
    std::string m_function;
    ValueRange m_inValueRange;
    ExecValueStorage m_value;
    Expression *m_child;
};

template <class GetValueRangeWeight, class ComputeValueRange, class Evaluate>
UnaryBuiltinVecFunc<GetValueRangeWeight, ComputeValueRange, Evaluate>::UnaryBuiltinVecFunc(
    GeneratorState &state, const char *function, ConstValueRangeAccess valueRange)
    : m_function(function)
    , m_inValueRange(valueRange.getType())
    , m_child(nullptr)
{
    DE_UNREF(state);
    DE_ASSERT(valueRange.getType().isFloatOrVec());

    m_value.setStorage(valueRange.getType());

    // Compute input value range
    for (int ndx = 0; ndx < m_inValueRange.getType().getNumElements(); ndx++)
    {
        ConstValueRangeAccess outRange = valueRange.component(ndx);
        ValueRangeAccess inRange       = m_inValueRange.asAccess().component(ndx);

        ComputeValueRange()(outRange.getMin().asFloat(), outRange.getMax().asFloat(), inRange.getMin().asFloat(),
                            inRange.getMax().asFloat());
    }
}

template <class GetValueRangeWeight, class ComputeValueRange, class Evaluate>
UnaryBuiltinVecFunc<GetValueRangeWeight, ComputeValueRange, Evaluate>::~UnaryBuiltinVecFunc(void)
{
    delete m_child;
}

template <class GetValueRangeWeight, class ComputeValueRange, class Evaluate>
Expression *UnaryBuiltinVecFunc<GetValueRangeWeight, ComputeValueRange, Evaluate>::createNextChild(
    GeneratorState &state)
{
    if (m_child)
        return nullptr;

    m_child = Expression::createRandom(state, m_inValueRange.asAccess());
    return m_child;
}

template <class GetValueRangeWeight, class ComputeValueRange, class Evaluate>
void UnaryBuiltinVecFunc<GetValueRangeWeight, ComputeValueRange, Evaluate>::tokenize(GeneratorState &state,
                                                                                     TokenStream &str) const
{
    str << Token(m_function.c_str()) << Token::LEFT_PAREN;
    m_child->tokenize(state, str);
    str << Token::RIGHT_PAREN;
}

template <class GetValueRangeWeight, class ComputeValueRange, class Evaluate>
void UnaryBuiltinVecFunc<GetValueRangeWeight, ComputeValueRange, Evaluate>::evaluate(ExecutionContext &execCtx)
{
    m_child->evaluate(execCtx);

    ExecConstValueAccess srcValue = m_child->getValue();
    ExecValueAccess dstValue      = m_value.getValue(m_inValueRange.getType());

    for (int elemNdx = 0; elemNdx < m_inValueRange.getType().getNumElements(); elemNdx++)
    {
        ExecConstValueAccess srcComp = srcValue.component(elemNdx);
        ExecValueAccess dstComp      = dstValue.component(elemNdx);

        for (int compNdx = 0; compNdx < EXEC_VEC_WIDTH; compNdx++)
            dstComp.asFloat(compNdx) = Evaluate()(srcComp.asFloat(compNdx));
    }
}

template <class GetValueRangeWeight, class ComputeValueRange, class Evaluate>
float UnaryBuiltinVecFunc<GetValueRangeWeight, ComputeValueRange, Evaluate>::getWeight(const GeneratorState &state,
                                                                                       ConstValueRangeAccess valueRange)
{
    // \todo [2011-06-14 pyry] Void support?
    if (!valueRange.getType().isFloatOrVec())
        return 0.0f;

    int availableLevels = state.getShaderParameters().maxExpressionDepth - state.getExpressionDepth();

    if (availableLevels < getConservativeValueExprDepth(state, valueRange) + 1)
        return 0.0f;

    // Compute value range weight
    float combinedWeight = 1.0f;
    for (int elemNdx = 0; elemNdx < valueRange.getType().getNumElements(); elemNdx++)
    {
        float elemWeight = GetValueRangeWeight()(valueRange.component(elemNdx).getMin().asFloat(),
                                                 valueRange.component(elemNdx).getMax().asFloat());
        combinedWeight *= elemWeight;
    }

    return combinedWeight;
}

// Proxy template.
template <class C>
struct GetUnaryBuiltinVecWeight
{
    inline float operator()(float outMin, float outMax) const
    {
        return C::getCompWeight(outMin, outMax);
    }
};

template <class C>
struct ComputeUnaryBuiltinVecRange
{
    inline void operator()(float outMin, float outMax, float &inMin, float &inMax) const
    {
        C::computeValueRange(outMin, outMax, inMin, inMax);
    }
};

template <class C>
struct EvaluateUnaryBuiltinVec
{
    inline float operator()(float inVal) const
    {
        return C::evaluateComp(inVal);
    }
};

template <class C>
class UnaryBuiltinVecTemplateProxy
    : public UnaryBuiltinVecFunc<GetUnaryBuiltinVecWeight<C>, ComputeUnaryBuiltinVecRange<C>,
                                 EvaluateUnaryBuiltinVec<C>>
{
public:
    UnaryBuiltinVecTemplateProxy(GeneratorState &state, const char *function, ConstValueRangeAccess valueRange)
        : UnaryBuiltinVecFunc<GetUnaryBuiltinVecWeight<C>, ComputeUnaryBuiltinVecRange<C>, EvaluateUnaryBuiltinVec<C>>(
              state, function, valueRange)
    {
    }
};

// Template for trigonometric function group.
template <class C>
class UnaryTrigonometricFunc : public UnaryBuiltinVecTemplateProxy<C>
{
public:
    UnaryTrigonometricFunc(GeneratorState &state, const char *function, ConstValueRangeAccess valueRange)
        : UnaryBuiltinVecTemplateProxy<C>(state, function, valueRange)
    {
    }

    static inline float getCompWeight(float outMin, float outMax)
    {
        if (Scalar::min<float>() == outMin || Scalar::max<float>() == outMax)
            return 1.0f; // Infinite value range, anything goes

        // Transform range
        float inMin, inMax;
        if (!C::transformValueRange(outMin, outMax, inMin, inMax))
            return 0.0f; // Not possible to transform value range (out of range perhaps)

        // Quantize
        if (!quantizeFloatRange(inMin, inMax))
            return 0.0f; // Not possible to quantize - would cause accuracy issues

        if (outMin == outMax)
            return 1.0f; // Constant value and passed quantization

        // Evaluate new intersection
        float intersectionLen = C::evaluateComp(inMax) - C::evaluateComp(inMin);
        float valRangeLen     = outMax - outMin;

        return deFloatMax(0.1f, intersectionLen / valRangeLen);
    }

    static inline void computeValueRange(float outMin, float outMax, float &inMin, float &inMax)
    {
        DE_VERIFY(C::transformValueRange(outMin, outMax, inMin, inMax));
        DE_VERIFY(quantizeFloatRange(inMin, inMax));
        DE_ASSERT(inMin <= inMax);
    }

    static float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange)
    {
        if (state.getProgramParameters().trigonometricBaseWeight <= 0.0f)
            return 0.0f;

        return UnaryBuiltinVecTemplateProxy<C>::getWeight(state, valueRange) *
               state.getProgramParameters().trigonometricBaseWeight;
    }
};

class SinOp : public UnaryTrigonometricFunc<SinOp>
{
public:
    SinOp(GeneratorState &state, ConstValueRangeAccess valueRange)
        : UnaryTrigonometricFunc<SinOp>(state, "sin", valueRange)
    {
    }

    static inline bool transformValueRange(float outMin, float outMax, float &inMin, float &inMax)
    {
        if (outMax < -1.0f || outMin > 1.0f)
            return false;

        inMin = (outMin >= -1.0f) ? deFloatAsin(outMin) : -0.5f * DE_PI;
        inMax = (outMax <= +1.0f) ? deFloatAsin(outMax) : +0.5f * DE_PI;

        return true;
    }

    static inline float evaluateComp(float inVal)
    {
        return deFloatSin(inVal);
    }
};

class CosOp : public UnaryTrigonometricFunc<CosOp>
{
public:
    CosOp(GeneratorState &state, ConstValueRangeAccess valueRange)
        : UnaryTrigonometricFunc<CosOp>(state, "cos", valueRange)
    {
    }

    static inline bool transformValueRange(float outMin, float outMax, float &inMin, float &inMax)
    {
        if (outMax < -1.0f || outMin > 1.0f)
            return false;

        inMax = (outMin >= -1.0f) ? deFloatAcos(outMin) : +DE_PI;
        inMin = (outMax <= +1.0f) ? deFloatAcos(outMax) : -DE_PI;

        return true;
    }

    static inline float evaluateComp(float inVal)
    {
        return deFloatCos(inVal);
    }
};

class TanOp : public UnaryTrigonometricFunc<TanOp>
{
public:
    TanOp(GeneratorState &state, ConstValueRangeAccess valueRange)
        : UnaryTrigonometricFunc<TanOp>(state, "tan", valueRange)
    {
    }

    static inline bool transformValueRange(float outMin, float outMax, float &inMin, float &inMax)
    {
        // \note Currently tan() is limited to -4..4 range. Otherwise we will run into accuracy issues
        const float rangeMin = -4.0f;
        const float rangeMax = +4.0f;

        if (outMax < rangeMin || outMin > rangeMax)
            return false;

        inMin = deFloatAtanOver(deFloatMax(outMin, rangeMin));
        inMax = deFloatAtanOver(deFloatMin(outMax, rangeMax));

        return true;
    }

    static inline float evaluateComp(float inVal)
    {
        return deFloatTan(inVal);
    }
};

class AsinOp : public UnaryTrigonometricFunc<AsinOp>
{
public:
    AsinOp(GeneratorState &state, ConstValueRangeAccess valueRange)
        : UnaryTrigonometricFunc<AsinOp>(state, "asin", valueRange)
    {
    }

    static inline bool transformValueRange(float outMin, float outMax, float &inMin, float &inMax)
    {
        const float rangeMin = -DE_PI / 2.0f;
        const float rangeMax = +DE_PI / 2.0f;

        if (outMax < rangeMin || outMin > rangeMax)
            return false; // Out of range

        inMin = deFloatSin(deFloatMax(outMin, rangeMin));
        inMax = deFloatSin(deFloatMin(outMax, rangeMax));

        return true;
    }

    static inline float evaluateComp(float inVal)
    {
        return deFloatAsin(inVal);
    }
};

class AcosOp : public UnaryTrigonometricFunc<AcosOp>
{
public:
    AcosOp(GeneratorState &state, ConstValueRangeAccess valueRange)
        : UnaryTrigonometricFunc<AcosOp>(state, "acos", valueRange)
    {
    }

    static inline bool transformValueRange(float outMin, float outMax, float &inMin, float &inMax)
    {
        const float rangeMin = 0.0f;
        const float rangeMax = DE_PI;

        if (outMax < rangeMin || outMin > rangeMax)
            return false; // Out of range

        inMax = deFloatCos(deFloatMax(outMin, rangeMin));
        inMin = deFloatCos(deFloatMin(outMax, rangeMax));

        return true;
    }

    static inline float evaluateComp(float inVal)
    {
        return deFloatAcos(inVal);
    }
};

class AtanOp : public UnaryTrigonometricFunc<AtanOp>
{
public:
    AtanOp(GeneratorState &state, ConstValueRangeAccess valueRange)
        : UnaryTrigonometricFunc<AtanOp>(state, "atan", valueRange)
    {
    }

    static inline bool transformValueRange(float outMin, float outMax, float &inMin, float &inMax)
    {
        // \note For accuracy reasons output range is limited to -1..1
        const float rangeMin = -1.0f;
        const float rangeMax = +1.0f;

        if (outMax < rangeMin || outMin > rangeMax)
            return false; // Out of range

        inMin = deFloatTan(deFloatMax(outMin, rangeMin));
        inMax = deFloatTan(deFloatMin(outMax, rangeMax));

        return true;
    }

    static inline float evaluateComp(float inVal)
    {
        return deFloatAtanOver(inVal);
    }
};

// Template for exponential function group.
// \todo [2011-07-07 pyry] Shares most of the code with Trigonometric variant..
template <class C>
class UnaryExponentialFunc : public UnaryBuiltinVecTemplateProxy<C>
{
public:
    UnaryExponentialFunc(GeneratorState &state, const char *function, ConstValueRangeAccess valueRange)
        : UnaryBuiltinVecTemplateProxy<C>(state, function, valueRange)
    {
    }

    static inline float getCompWeight(float outMin, float outMax)
    {
        if (Scalar::min<float>() == outMin || Scalar::max<float>() == outMax)
            return 1.0f; // Infinite value range, anything goes

        // Transform range
        float inMin, inMax;
        if (!C::transformValueRange(outMin, outMax, inMin, inMax))
            return 0.0f; // Not possible to transform value range (out of range perhaps)

        // Quantize
        if (!quantizeFloatRange(inMin, inMax))
            return 0.0f; // Not possible to quantize - would cause accuracy issues

        if (outMin == outMax)
            return 1.0f; // Constant value and passed quantization

        // Evaluate new intersection
        float intersectionLen = C::evaluateComp(inMax) - C::evaluateComp(inMin);
        float valRangeLen     = outMax - outMin;

        return deFloatMax(0.1f, intersectionLen / valRangeLen);
    }

    static inline void computeValueRange(float outMin, float outMax, float &inMin, float &inMax)
    {
        DE_VERIFY(C::transformValueRange(outMin, outMax, inMin, inMax));
        DE_VERIFY(quantizeFloatRange(inMin, inMax));
        DE_ASSERT(inMin <= inMax);
    }

    static float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange)
    {
        if (state.getProgramParameters().exponentialBaseWeight <= 0.0f)
            return 0.0f;

        return UnaryBuiltinVecTemplateProxy<C>::getWeight(state, valueRange) *
               state.getProgramParameters().exponentialBaseWeight;
    }
};

class ExpOp : public UnaryExponentialFunc<ExpOp>
{
public:
    ExpOp(GeneratorState &state, ConstValueRangeAccess valueRange)
        : UnaryExponentialFunc<ExpOp>(state, "exp", valueRange)
    {
    }

    static inline bool transformValueRange(float outMin, float outMax, float &inMin, float &inMax)
    {
        // Limited due to accuracy reasons, should be 0..+inf
        const float rangeMin = 0.1f;
        const float rangeMax = 10.0f;

        if (outMax < rangeMin || outMin > rangeMax)
            return false; // Out of range

        inMin = deFloatLog(deFloatMax(outMin, rangeMin));
        inMax = deFloatLog(deFloatMin(outMax, rangeMax));

        return true;
    }

    static inline float evaluateComp(float inVal)
    {
        return deFloatExp(inVal);
    }
};

class LogOp : public UnaryExponentialFunc<LogOp>
{
public:
    LogOp(GeneratorState &state, ConstValueRangeAccess valueRange)
        : UnaryExponentialFunc<LogOp>(state, "log", valueRange)
    {
    }

    static inline bool transformValueRange(float outMin, float outMax, float &inMin, float &inMax)
    {
        // Limited due to accuracy reasons, should be -inf..+inf
        const float rangeMin = 0.1f;
        const float rangeMax = 6.0f;

        if (outMax < rangeMin || outMin > rangeMax)
            return false; // Out of range

        inMin = deFloatExp(deFloatMax(outMin, rangeMin));
        inMax = deFloatExp(deFloatMin(outMax, rangeMax));

        return true;
    }

    static inline float evaluateComp(float inVal)
    {
        return deFloatLog(inVal);
    }
};

class Exp2Op : public UnaryExponentialFunc<Exp2Op>
{
public:
    Exp2Op(GeneratorState &state, ConstValueRangeAccess valueRange)
        : UnaryExponentialFunc<Exp2Op>(state, "exp2", valueRange)
    {
    }

    static inline bool transformValueRange(float outMin, float outMax, float &inMin, float &inMax)
    {
        // Limited due to accuracy reasons, should be 0..+inf
        const float rangeMin = 0.1f;
        const float rangeMax = 10.0f;

        if (outMax < rangeMin || outMin > rangeMax)
            return false; // Out of range

        inMin = deFloatLog2(deFloatMax(outMin, rangeMin));
        inMax = deFloatLog2(deFloatMin(outMax, rangeMax));

        return true;
    }

    static inline float evaluateComp(float inVal)
    {
        return deFloatExp2(inVal);
    }
};

class Log2Op : public UnaryExponentialFunc<Log2Op>
{
public:
    Log2Op(GeneratorState &state, ConstValueRangeAccess valueRange)
        : UnaryExponentialFunc<Log2Op>(state, "log2", valueRange)
    {
    }

    static inline bool transformValueRange(float outMin, float outMax, float &inMin, float &inMax)
    {
        // Limited due to accuracy reasons, should be -inf..+inf
        const float rangeMin = 0.1f;
        const float rangeMax = 6.0f;

        if (outMax < rangeMin || outMin > rangeMax)
            return false; // Out of range

        inMin = deFloatExp2(deFloatMax(outMin, rangeMin));
        inMax = deFloatExp2(deFloatMin(outMax, rangeMax));

        return true;
    }

    static inline float evaluateComp(float inVal)
    {
        return deFloatLog2(inVal);
    }
};

class SqrtOp : public UnaryExponentialFunc<SqrtOp>
{
public:
    SqrtOp(GeneratorState &state, ConstValueRangeAccess valueRange)
        : UnaryExponentialFunc<SqrtOp>(state, "sqrt", valueRange)
    {
    }

    static inline bool transformValueRange(float outMin, float outMax, float &inMin, float &inMax)
    {
        // Limited due to accuracy reasons, should be 0..+inf
        const float rangeMin = 0.0f;
        const float rangeMax = 4.0f;

        if (outMax < rangeMin || outMin > rangeMax)
            return false; // Out of range

        inMin = deFloatMax(outMin, rangeMin);
        inMax = deFloatMin(outMax, rangeMax);

        inMin *= inMin;
        inMax *= inMax;

        return true;
    }

    static inline float evaluateComp(float inVal)
    {
        return deFloatSqrt(inVal);
    }
};

class InvSqrtOp : public UnaryExponentialFunc<InvSqrtOp>
{
public:
    InvSqrtOp(GeneratorState &state, ConstValueRangeAccess valueRange)
        : UnaryExponentialFunc<InvSqrtOp>(state, "inversesqrt", valueRange)
    {
    }

    static inline bool transformValueRange(float outMin, float outMax, float &inMin, float &inMax)
    {
        // Limited due to accuracy reasons
        const float rangeMin = 0.4f;
        const float rangeMax = 3.0f;

        if (outMax < rangeMin || outMin > rangeMax)
            return false; // Out of range

        inMax = 1.0f / deFloatMax(outMin, rangeMin);
        inMin = 1.0f / deFloatMin(outMax, rangeMax);

        inMin *= inMin;
        inMax *= inMax;

        return true;
    }

    static inline float evaluateComp(float inVal)
    {
        return 1.0f / deFloatSqrt(inVal);
    }
};

} // namespace rsg

#endif // _RSGBUILTINFUNCTIONS_HPP
