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
 * \brief Expressions.
 *//*--------------------------------------------------------------------*/

#include "rsgExpression.hpp"
#include "rsgVariableManager.hpp"
#include "rsgBinaryOps.hpp"
#include "rsgBuiltinFunctions.hpp"
#include "rsgUtils.hpp"
#include "deMath.h"

using std::vector;

namespace rsg
{

namespace
{

class IsReadableEntry
{
public:
    typedef ValueEntryIterator<IsReadableEntry> Iterator;

    IsReadableEntry(uint32_t exprFlags) : m_exprFlags(exprFlags)
    {
    }

    bool operator()(const ValueEntry *entry) const
    {
        if ((m_exprFlags & CONST_EXPR) && (entry->getVariable()->getStorage() != Variable::STORAGE_CONST))
            return false;

        return true;
    }

private:
    uint32_t m_exprFlags;
};

class IsReadableIntersectingEntry : public IsReadableEntry
{
public:
    typedef ValueEntryIterator<IsReadableIntersectingEntry> Iterator;

    IsReadableIntersectingEntry(ConstValueRangeAccess valueRange, uint32_t exprFlags)
        : IsReadableEntry(exprFlags)
        , m_valueRange(valueRange)
    {
    }

    bool operator()(const ValueEntry *entry) const
    {
        if (!IsReadableEntry::operator()(entry))
            return false;

        if (entry->getValueRange().getType() != m_valueRange.getType())
            return false;

        if (!entry->getValueRange().intersects(m_valueRange))
            return false;

        return true;
    }

private:
    ConstValueRangeAccess m_valueRange;
};

class IsWritableIntersectingEntry : public IsWritableEntry
{
public:
    typedef ValueEntryIterator<IsWritableIntersectingEntry> Iterator;

    IsWritableIntersectingEntry(ConstValueRangeAccess valueRange) : m_valueRange(valueRange)
    {
    }

    bool operator()(const ValueEntry *entry) const
    {
        return IsWritableEntry::operator()(entry) && entry->getVariable()->getType() == m_valueRange.getType() &&
               entry->getValueRange().intersects(m_valueRange);
    }

private:
    ConstValueRangeAccess m_valueRange;
};

class IsWritableSupersetEntry : public IsWritableEntry
{
public:
    typedef ValueEntryIterator<IsWritableSupersetEntry> Iterator;

    IsWritableSupersetEntry(ConstValueRangeAccess valueRange) : m_valueRange(valueRange)
    {
    }

    bool operator()(const ValueEntry *entry) const
    {
        return IsWritableEntry()(entry) && entry->getVariable()->getType() == m_valueRange.getType() &&
               entry->getValueRange().isSupersetOf(m_valueRange);
    }

private:
    ConstValueRangeAccess m_valueRange;
};

class IsSamplerEntry
{
public:
    typedef ValueEntryIterator<IsSamplerEntry> Iterator;

    IsSamplerEntry(VariableType::Type type) : m_type(type)
    {
        DE_ASSERT(m_type == VariableType::TYPE_SAMPLER_2D || m_type == VariableType::TYPE_SAMPLER_CUBE);
    }

    bool operator()(const ValueEntry *entry) const
    {
        if (entry->getVariable()->getType() == VariableType(m_type, 1))
        {
            DE_ASSERT(entry->getVariable()->getStorage() == Variable::STORAGE_UNIFORM);
            return true;
        }
        else
            return false;
    }

private:
    VariableType::Type m_type;
};

inline bool getWeightedBool(de::Random &random, float trueWeight)
{
    DE_ASSERT(de::inRange<float>(trueWeight, 0.0f, 1.0f));
    return (random.getFloat() < trueWeight);
}

void computeRandomValueRangeForInfElements(GeneratorState &state, ValueRangeAccess valueRange)
{
    const VariableType &type = valueRange.getType();
    de::Random &rnd          = state.getRandom();

    switch (type.getBaseType())
    {
    case VariableType::TYPE_BOOL:
        // No need to handle bool as it will be false, true
        break;

    case VariableType::TYPE_INT:
        for (int ndx = 0; ndx < type.getNumElements(); ndx++)
        {
            if (valueRange.getMin().component(ndx).asScalar() != Scalar::min<int>() ||
                valueRange.getMax().component(ndx).asScalar() != Scalar::max<int>())
                continue;

            const int minIntVal   = -16;
            const int maxIntVal   = 16;
            const int maxRangeLen = maxIntVal - minIntVal;

            int rangeLen = rnd.getInt(0, maxRangeLen);
            int minVal   = minIntVal + rnd.getInt(0, maxRangeLen - rangeLen);
            int maxVal   = minVal + rangeLen;

            valueRange.getMin().component(ndx).asInt() = minVal;
            valueRange.getMax().component(ndx).asInt() = maxVal;
        }
        break;

    case VariableType::TYPE_FLOAT:
        for (int ndx = 0; ndx < type.getNumElements(); ndx++)
        {
            if (valueRange.getMin().component(ndx).asScalar() != Scalar::min<float>() ||
                valueRange.getMax().component(ndx).asScalar() != Scalar::max<float>())
                continue;

            const float step        = 0.1f;
            const int maxSteps      = 320;
            const float minFloatVal = -16.0f;

            int rangeLen = rnd.getInt(0, maxSteps);
            int minStep  = rnd.getInt(0, maxSteps - rangeLen);

            float minVal = minFloatVal + step * (float)minStep;
            float maxVal = minVal + step * (float)rangeLen;

            valueRange.getMin().component(ndx).asFloat() = minVal;
            valueRange.getMax().component(ndx).asFloat() = maxVal;
        }
        break;

    default:
        DE_ASSERT(false);
        throw Exception("computeRandomValueRangeForInfElements(): unsupported type");
    }
}

void setInfiniteRange(ValueRangeAccess valueRange)
{
    const VariableType &type = valueRange.getType();

    switch (type.getBaseType())
    {
    case VariableType::TYPE_BOOL:
        for (int ndx = 0; ndx < type.getNumElements(); ndx++)
        {
            valueRange.getMin().component(ndx) = Scalar::min<bool>();
            valueRange.getMax().component(ndx) = Scalar::max<bool>();
        }
        break;

    case VariableType::TYPE_INT:
        for (int ndx = 0; ndx < type.getNumElements(); ndx++)
        {
            valueRange.getMin().component(ndx) = Scalar::min<int>();
            valueRange.getMax().component(ndx) = Scalar::max<int>();
        }
        break;

    case VariableType::TYPE_FLOAT:
        for (int ndx = 0; ndx < type.getNumElements(); ndx++)
        {
            valueRange.getMin().component(ndx) = Scalar::min<float>();
            valueRange.getMax().component(ndx) = Scalar::max<float>();
        }
        break;

    default:
        DE_ASSERT(false);
        throw Exception("setInfiniteRange(): unsupported type");
    }
}

bool canAllocateVariable(const GeneratorState &state, const VariableType &type)
{
    DE_ASSERT(!type.isVoid());

    if (state.getExpressionFlags() & NO_VAR_ALLOCATION)
        return false;

    if (state.getVariableManager().getNumAllocatedScalars() + type.getScalarSize() >
        state.getShaderParameters().maxCombinedVariableScalars)
        return false;

    return true;
}

template <class T>
float getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange)
{
    return T::getWeight(state, valueRange);
}
template <class T>
Expression *create(GeneratorState &state, ConstValueRangeAccess valueRange)
{
    return new T(state, valueRange);
}

struct ExpressionSpec
{
    float (*getWeight)(const GeneratorState &state, ConstValueRangeAccess valueRange);
    Expression *(*create)(GeneratorState &state, ConstValueRangeAccess valueRange);
};

static const ExpressionSpec s_expressionSpecs[] = {{getWeight<FloatLiteral>, create<FloatLiteral>},
                                                   {getWeight<IntLiteral>, create<IntLiteral>},
                                                   {getWeight<BoolLiteral>, create<BoolLiteral>},
                                                   {getWeight<ConstructorOp>, create<ConstructorOp>},
                                                   {getWeight<AssignOp>, create<AssignOp>},
                                                   {getWeight<VariableRead>, create<VariableRead>},
                                                   {getWeight<MulOp>, create<MulOp>},
                                                   {getWeight<AddOp>, create<AddOp>},
                                                   {getWeight<SubOp>, create<SubOp>},
                                                   {getWeight<LessThanOp>, create<LessThanOp>},
                                                   {getWeight<LessOrEqualOp>, create<LessOrEqualOp>},
                                                   {getWeight<GreaterThanOp>, create<GreaterThanOp>},
                                                   {getWeight<GreaterOrEqualOp>, create<GreaterOrEqualOp>},
                                                   {getWeight<EqualOp>, create<EqualOp>},
                                                   {getWeight<NotEqualOp>, create<NotEqualOp>},
                                                   {getWeight<SwizzleOp>, create<SwizzleOp>},
                                                   {getWeight<SinOp>, create<SinOp>},
                                                   {getWeight<CosOp>, create<CosOp>},
                                                   {getWeight<TanOp>, create<TanOp>},
                                                   {getWeight<AsinOp>, create<AsinOp>},
                                                   {getWeight<AcosOp>, create<AcosOp>},
                                                   {getWeight<AtanOp>, create<AtanOp>},
                                                   {getWeight<ExpOp>, create<ExpOp>},
                                                   {getWeight<LogOp>, create<LogOp>},
                                                   {getWeight<Exp2Op>, create<Exp2Op>},
                                                   {getWeight<Log2Op>, create<Log2Op>},
                                                   {getWeight<SqrtOp>, create<SqrtOp>},
                                                   {getWeight<InvSqrtOp>, create<InvSqrtOp>},
                                                   {getWeight<ParenOp>, create<ParenOp>},
                                                   {getWeight<TexLookup>, create<TexLookup>}};

static const ExpressionSpec s_lvalueSpecs[] = {{getWeight<VariableWrite>, create<VariableWrite>}};

#if !defined(DE_MAX)
#define DE_MAX(a, b) ((b) > (a) ? (b) : (a))
#endif

enum
{
    MAX_EXPRESSION_SPECS = (int)DE_MAX(DE_LENGTH_OF_ARRAY(s_expressionSpecs), DE_LENGTH_OF_ARRAY(s_lvalueSpecs))
};

const ExpressionSpec *chooseExpression(GeneratorState &state, const ExpressionSpec *specs, int numSpecs,
                                       ConstValueRangeAccess valueRange)
{
    float weights[MAX_EXPRESSION_SPECS];

    DE_ASSERT(numSpecs <= (int)DE_LENGTH_OF_ARRAY(weights));

    // Compute weights
    for (int ndx = 0; ndx < numSpecs; ndx++)
        weights[ndx] = specs[ndx].getWeight(state, valueRange);

    // Choose
    return &state.getRandom().chooseWeighted<const ExpressionSpec &>(specs, specs + numSpecs, weights);
}

} // namespace

Expression::~Expression(void)
{
}

Expression *Expression::createRandom(GeneratorState &state, ConstValueRangeAccess valueRange)
{
    return chooseExpression(state, s_expressionSpecs, (int)DE_LENGTH_OF_ARRAY(s_expressionSpecs), valueRange)
        ->create(state, valueRange);
}

Expression *Expression::createRandomLValue(GeneratorState &state, ConstValueRangeAccess valueRange)
{
    return chooseExpression(state, s_lvalueSpecs, (int)DE_LENGTH_OF_ARRAY(s_lvalueSpecs), valueRange)
        ->create(state, valueRange);
}

FloatLiteral::FloatLiteral(GeneratorState &state, ConstValueRangeAccess valueRange)
    : m_value(VariableType::getScalarType(VariableType::TYPE_FLOAT))
{
    float minVal = -10.0f;
    float maxVal = +10.0f;
    float step   = 0.25f;

    if (valueRange.getType() == VariableType(VariableType::TYPE_FLOAT, 1))
    {
        minVal = valueRange.getMin().component(0).asFloat();
        maxVal = valueRange.getMax().component(0).asFloat();

        if (Scalar::min<float>() == minVal)
            minVal = -10.0f;

        if (Scalar::max<float>() == maxVal)
            maxVal = +10.0f;
    }

    int numSteps = (int)((maxVal - minVal) / step) + 1;

    const float value      = deFloatClamp(minVal + step * (float)state.getRandom().getInt(0, numSteps), minVal, maxVal);
    ExecValueAccess access = m_value.getValue(VariableType::getScalarType(VariableType::TYPE_FLOAT));

    for (int ndx = 0; ndx < EXEC_VEC_WIDTH; ndx++)
        access.asFloat(ndx) = value;
}

FloatLiteral::FloatLiteral(float customValue) : m_value(VariableType::getScalarType(VariableType::TYPE_FLOAT))
{
    // This constructor is required to handle corner case in which comparision
    // of two same floats produced different results - this was resolved by
    // adding FloatLiteral containing epsilon to one of values
    ExecValueAccess access = m_value.getValue(VariableType::getScalarType(VariableType::TYPE_FLOAT));

    for (int ndx = 0; ndx < EXEC_VEC_WIDTH; ndx++)
        access.asFloat(ndx) = customValue;
}

float FloatLiteral::getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange)
{
    DE_UNREF(state);
    const VariableType &type = valueRange.getType();
    if (type == VariableType(VariableType::TYPE_FLOAT, 1))
    {
        float minVal = valueRange.getMin().asFloat();
        float maxVal = valueRange.getMax().asFloat();

        if (Scalar::min<float>() == minVal && Scalar::max<float>() == maxVal)
            return 0.1f;

        // Weight based on value range length
        float rangeLength = maxVal - minVal;

        DE_ASSERT(rangeLength >= 0.0f);
        return deFloatMax(0.1f, 1.0f - rangeLength);
    }
    else if (type.isVoid())
        return unusedValueWeight;
    else
        return 0.0f;
}

void FloatLiteral::tokenize(GeneratorState &state, TokenStream &str) const
{
    DE_UNREF(state);
    str << Token(m_value.getValue(VariableType::getScalarType(VariableType::TYPE_FLOAT)).asFloat(0));
}

IntLiteral::IntLiteral(GeneratorState &state, ConstValueRangeAccess valueRange)
    : m_value(VariableType::getScalarType(VariableType::TYPE_INT))
{
    int minVal = -16;
    int maxVal = +16;

    if (valueRange.getType() == VariableType(VariableType::TYPE_INT, 1))
    {
        minVal = valueRange.getMin().component(0).asInt();
        maxVal = valueRange.getMax().component(0).asInt();

        if (Scalar::min<int>() == minVal)
            minVal = -16;

        if (Scalar::max<int>() == maxVal)
            maxVal = 16;
    }

    int value              = state.getRandom().getInt(minVal, maxVal);
    ExecValueAccess access = m_value.getValue(VariableType::getScalarType(VariableType::TYPE_INT));

    for (int ndx = 0; ndx < EXEC_VEC_WIDTH; ndx++)
        access.asInt(ndx) = value;
}

float IntLiteral::getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange)
{
    DE_UNREF(state);
    const VariableType &type = valueRange.getType();
    if (type == VariableType(VariableType::TYPE_INT, 1))
    {
        int minVal = valueRange.getMin().asInt();
        int maxVal = valueRange.getMax().asInt();

        if (Scalar::min<int>() == minVal && Scalar::max<int>() == maxVal)
            return 0.1f;

        int rangeLength = maxVal - minVal;

        DE_ASSERT(rangeLength >= 0);
        return deFloatMax(0.1f, 1.0f - (float)rangeLength / 4.0f);
    }
    else if (type.isVoid())
        return unusedValueWeight;
    else
        return 0.0f;
}

void IntLiteral::tokenize(GeneratorState &state, TokenStream &str) const
{
    DE_UNREF(state);
    str << Token(m_value.getValue(VariableType::getScalarType(VariableType::TYPE_INT)).asInt(0));
}

BoolLiteral::BoolLiteral(GeneratorState &state, ConstValueRangeAccess valueRange)
    : m_value(VariableType::getScalarType(VariableType::TYPE_BOOL))
{
    int minVal = 0;
    int maxVal = 1;

    if (valueRange.getType() == VariableType(VariableType::TYPE_BOOL, 1))
    {
        minVal = valueRange.getMin().component(0).asBool() ? 1 : 0;
        maxVal = valueRange.getMax().component(0).asBool() ? 1 : 0;
    }

    bool value             = state.getRandom().getInt(minVal, maxVal) == 1;
    ExecValueAccess access = m_value.getValue(VariableType::getScalarType(VariableType::TYPE_BOOL));

    for (int ndx = 0; ndx < EXEC_VEC_WIDTH; ndx++)
        access.asBool(ndx) = value;
}

BoolLiteral::BoolLiteral(bool customValue) : m_value(VariableType::getScalarType(VariableType::TYPE_BOOL))
{
    // This constructor is required to handle corner case in which comparision
    // of two same floats produced different results - this was resolved by
    // adding FloatLiteral containing epsilon to one of values
    ExecValueAccess access = m_value.getValue(VariableType::getScalarType(VariableType::TYPE_BOOL));

    for (int ndx = 0; ndx < EXEC_VEC_WIDTH; ndx++)
        access.asBool(ndx) = customValue;
}

float BoolLiteral::getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange)
{
    DE_UNREF(state);
    const VariableType &type = valueRange.getType();
    if (type == VariableType(VariableType::TYPE_BOOL, 1))
        return 0.5f;
    else if (type.isVoid())
        return unusedValueWeight;
    else
        return 0.0f;
}

void BoolLiteral::tokenize(GeneratorState &state, TokenStream &str) const
{
    DE_UNREF(state);
    str << Token(m_value.getValue(VariableType::getScalarType(VariableType::TYPE_BOOL)).asBool(0));
}

namespace
{

// \note int-bool and float-bool conversions handled in a special way.
template <typename SrcType, typename DstType>
inline DstType convert(SrcType src)
{
    if (Scalar::min<SrcType>() == src)
        return Scalar::min<DstType>().template as<DstType>();
    else if (Scalar::max<SrcType>() == src)
        return Scalar::max<DstType>().template as<DstType>();
    else
        return DstType(src);
}

// According to GLSL ES spec.
template <>
inline bool convert<float, bool>(float src)
{
    return src != 0.0f;
}
template <>
inline bool convert<int, bool>(int src)
{
    return src != 0;
}
template <>
inline bool convert<bool, bool>(bool src)
{
    return src;
}
template <>
inline float convert<bool, float>(bool src)
{
    return src ? 1.0f : 0.0f;
}
template <>
inline int convert<bool, int>(bool src)
{
    return src ? 1 : 0;
}

template <>
inline int convert<float, int>(float src)
{
    if (Scalar::min<float>() == src)
        return Scalar::min<int>().as<int>();
    else if (Scalar::max<float>() == src)
        return Scalar::max<int>().as<int>();
    else if (src > 0.0f)
        return (int)deFloatFloor(src);
    else
        return (int)deFloatCeil(src);
}

template <typename SrcType, typename DstType>
inline void convertValueRange(SrcType srcMin, SrcType srcMax, DstType &dstMin, DstType &dstMax)
{
    dstMin = convert<SrcType, DstType>(srcMin);
    dstMax = convert<SrcType, DstType>(srcMax);
}

template <>
inline void convertValueRange<float, int>(float srcMin, float srcMax, int &dstMin, int &dstMax)
{
    if (Scalar::min<float>() == srcMin)
        dstMin = Scalar::min<int>().as<int>();
    else
        dstMin = (int)deFloatCeil(srcMin);

    if (Scalar::max<float>() == srcMax)
        dstMax = Scalar::max<int>().as<int>();
    else
        dstMax = (int)deFloatFloor(srcMax);
}

template <>
inline void convertValueRange<float, bool>(float srcMin, float srcMax, bool &dstMin, bool &dstMax)
{
    dstMin = srcMin > 0.0f;
    dstMax = srcMax > 0.0f;
}

// \todo [pyry] More special cases?

// Returns whether it is possible to convert some SrcType value range to given DstType valueRange
template <typename SrcType, typename DstType>
bool isConversionOk(DstType min, DstType max)
{
    SrcType sMin, sMax;
    convertValueRange(min, max, sMin, sMax);
    return sMin <= sMax && de::inRange(convert<SrcType, DstType>(sMin), min, max) &&
           de::inRange(convert<SrcType, DstType>(sMax), min, max);
}

// Work-around for non-deterministic float behavior
template <>
bool isConversionOk<float, float>(float, float)
{
    return true;
}

// \todo [2011-03-26 pyry] Provide this in ValueAccess?
template <typename T>
T getValueAccessValue(ConstValueAccess access);
template <>
inline float getValueAccessValue<float>(ConstValueAccess access)
{
    return access.asFloat();
}
template <>
inline int getValueAccessValue<int>(ConstValueAccess access)
{
    return access.asInt();
}
template <>
inline bool getValueAccessValue<bool>(ConstValueAccess access)
{
    return access.asBool();
}

template <typename T>
T &getValueAccessValue(ValueAccess access);
template <>
inline float &getValueAccessValue<float>(ValueAccess access)
{
    return access.asFloat();
}
template <>
inline int &getValueAccessValue<int>(ValueAccess access)
{
    return access.asInt();
}
template <>
inline bool &getValueAccessValue<bool>(ValueAccess access)
{
    return access.asBool();
}

template <typename SrcType, typename DstType>
bool isConversionOk(ConstValueRangeAccess valueRange)
{
    return isConversionOk<SrcType>(getValueAccessValue<DstType>(valueRange.getMin()),
                                   getValueAccessValue<DstType>(valueRange.getMax()));
}

template <typename SrcType, typename DstType>
void convertValueRangeTempl(ConstValueRangeAccess src, ValueRangeAccess dst)
{
    DstType dMin, dMax;
    convertValueRange(getValueAccessValue<SrcType>(src.getMin()), getValueAccessValue<SrcType>(src.getMax()), dMin,
                      dMax);
    getValueAccessValue<DstType>(dst.getMin()) = dMin;
    getValueAccessValue<DstType>(dst.getMax()) = dMax;
}

template <typename SrcType, typename DstType>
void convertExecValueTempl(ExecConstValueAccess src, ExecValueAccess dst)
{
    for (int ndx = 0; ndx < EXEC_VEC_WIDTH; ndx++)
        dst.as<DstType>(ndx) = convert<SrcType, DstType>(src.as<SrcType>(ndx));
}

typedef bool (*IsConversionOkFunc)(ConstValueRangeAccess);
typedef void (*ConvertValueRangeFunc)(ConstValueRangeAccess, ValueRangeAccess);
typedef void (*ConvertExecValueFunc)(ExecConstValueAccess, ExecValueAccess);

inline int getBaseTypeConvNdx(VariableType::Type type)
{
    switch (type)
    {
    case VariableType::TYPE_FLOAT:
        return 0;
    case VariableType::TYPE_INT:
        return 1;
    case VariableType::TYPE_BOOL:
        return 2;
    default:
        return -1;
    }
}

bool isConversionOk(VariableType::Type srcType, VariableType::Type dstType, ConstValueRangeAccess valueRange)
{
    // [src][dst]
    static const IsConversionOkFunc convTable[3][3] = {
        {isConversionOk<float, float>, isConversionOk<float, int>, isConversionOk<float, bool>},
        {isConversionOk<int, float>, isConversionOk<int, int>, isConversionOk<int, bool>},
        {isConversionOk<bool, float>, isConversionOk<bool, int>, isConversionOk<bool, bool>}};
    return convTable[getBaseTypeConvNdx(srcType)][getBaseTypeConvNdx(dstType)](valueRange);
}

void convertValueRange(ConstValueRangeAccess src, ValueRangeAccess dst)
{
    // [src][dst]
    static const ConvertValueRangeFunc convTable[3][3] = {
        {convertValueRangeTempl<float, float>, convertValueRangeTempl<float, int>, convertValueRangeTempl<float, bool>},
        {convertValueRangeTempl<int, float>, convertValueRangeTempl<int, int>, convertValueRangeTempl<int, bool>},
        {convertValueRangeTempl<bool, float>, convertValueRangeTempl<bool, int>, convertValueRangeTempl<bool, bool>}};

    convTable[getBaseTypeConvNdx(src.getType().getBaseType())][getBaseTypeConvNdx(dst.getType().getBaseType())](src,
                                                                                                                dst);
}

void convertExecValue(ExecConstValueAccess src, ExecValueAccess dst)
{
    // [src][dst]
    static const ConvertExecValueFunc convTable[3][3] = {
        {convertExecValueTempl<float, float>, convertExecValueTempl<float, int>, convertExecValueTempl<float, bool>},
        {convertExecValueTempl<int, float>, convertExecValueTempl<int, int>, convertExecValueTempl<int, bool>},
        {convertExecValueTempl<bool, float>, convertExecValueTempl<bool, int>, convertExecValueTempl<bool, bool>}};

    convTable[getBaseTypeConvNdx(src.getType().getBaseType())][getBaseTypeConvNdx(dst.getType().getBaseType())](src,
                                                                                                                dst);
}

} // namespace

ConstructorOp::ConstructorOp(GeneratorState &state, ConstValueRangeAccess valueRange) : m_valueRange(valueRange)
{
    if (valueRange.getType().isVoid())
    {
        // Use random range
        const int maxScalars = 4; // We don't have to be able to assign this value to anywhere
        m_valueRange         = ValueRange(computeRandomType(state, maxScalars));
        computeRandomValueRange(state, m_valueRange.asAccess());
    }

    // \todo [2011-03-26 pyry] Vector conversions
    // int remainingDepth = state.getShaderParameters().maxExpressionDepth - state.getExpressionDepth();

    const VariableType &type    = m_valueRange.getType();
    VariableType::Type baseType = type.getBaseType();
    int numScalars              = type.getNumElements();
    int curScalarNdx            = 0;

    // \todo [2011-03-26 pyry] Separate op for struct constructors!
    DE_ASSERT(type.isFloatOrVec() || type.isIntOrVec() || type.isBoolOrVec());

    bool scalarConversions = state.getProgramParameters().useScalarConversions;

    while (curScalarNdx < numScalars)
    {
        ConstValueRangeAccess comp = m_valueRange.asAccess().component(curScalarNdx);

        if (scalarConversions)
        {
            int numInTypes = 0;
            VariableType::Type inTypes[3];

            if (isConversionOk(VariableType::TYPE_FLOAT, baseType, comp))
                inTypes[numInTypes++] = VariableType::TYPE_FLOAT;
            if (isConversionOk(VariableType::TYPE_INT, baseType, comp))
                inTypes[numInTypes++] = VariableType::TYPE_INT;
            if (isConversionOk(VariableType::TYPE_BOOL, baseType, comp))
                inTypes[numInTypes++] = VariableType::TYPE_BOOL;

            DE_ASSERT(numInTypes > 0); // At least nop conversion should be ok

            // Choose random
            VariableType::Type inType =
                state.getRandom().choose<VariableType::Type>(&inTypes[0], &inTypes[0] + numInTypes);

            // Compute converted value range
            ValueRange inValueRange(VariableType(inType, 1));
            convertValueRange(comp, inValueRange);
            m_inputValueRanges.push_back(inValueRange);

            curScalarNdx += 1;
        }
        else
        {
            m_inputValueRanges.push_back(ValueRange(comp));
            curScalarNdx += 1;
        }
    }
}

ConstructorOp::~ConstructorOp(void)
{
    for (vector<Expression *>::iterator i = m_inputExpressions.begin(); i != m_inputExpressions.end(); i++)
        delete *i;
}

Expression *ConstructorOp::createNextChild(GeneratorState &state)
{
    int numChildren   = (int)m_inputExpressions.size();
    Expression *child = nullptr;

    // \note Created in reverse order!
    if (numChildren < (int)m_inputValueRanges.size())
    {
        const ValueRange &inValueRange = m_inputValueRanges[m_inputValueRanges.size() - 1 - numChildren];
        child                          = Expression::createRandom(state, inValueRange);
        try
        {
            m_inputExpressions.push_back(child);
        }
        catch (const std::exception &)
        {
            delete child;
            throw;
        }
    }

    return child;
}

float ConstructorOp::getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange)
{
    if (valueRange.getType().isVoid())
        return unusedValueWeight;

    if (!valueRange.getType().isFloatOrVec() && !valueRange.getType().isIntOrVec() &&
        !valueRange.getType().isBoolOrVec())
        return 0.0f;

    if (state.getExpressionDepth() + getTypeConstructorDepth(valueRange.getType()) >
        state.getShaderParameters().maxExpressionDepth)
        return 0.0f;

    return 1.0f;
}

void ConstructorOp::tokenize(GeneratorState &state, TokenStream &str) const
{
    const VariableType &type = m_valueRange.getType();
    DE_ASSERT(type.getPrecision() == VariableType::PRECISION_NONE);
    type.tokenizeShortType(str);

    str << Token::LEFT_PAREN;

    for (vector<Expression *>::const_reverse_iterator i = m_inputExpressions.rbegin(); i != m_inputExpressions.rend();
         i++)
    {
        if (i != m_inputExpressions.rbegin())
            str << Token::COMMA;
        (*i)->tokenize(state, str);
    }

    str << Token::RIGHT_PAREN;
}

void ConstructorOp::evaluate(ExecutionContext &evalCtx)
{
    // Evaluate children
    for (vector<Expression *>::reverse_iterator i = m_inputExpressions.rbegin(); i != m_inputExpressions.rend(); i++)
        (*i)->evaluate(evalCtx);

    // Compute value
    const VariableType &type = m_valueRange.getType();
    m_value.setStorage(type);

    ExecValueAccess dst = m_value.getValue(type);
    int curScalarNdx    = 0;

    for (vector<Expression *>::reverse_iterator i = m_inputExpressions.rbegin(); i != m_inputExpressions.rend(); i++)
    {
        ExecConstValueAccess src = (*i)->getValue();

        for (int elemNdx = 0; elemNdx < src.getType().getNumElements(); elemNdx++)
            convertExecValue(src.component(elemNdx), dst.component(curScalarNdx++));
    }
}

AssignOp::AssignOp(GeneratorState &state, ConstValueRangeAccess valueRange)
    : m_valueRange(valueRange)
    , m_lvalueExpr(nullptr)
    , m_rvalueExpr(nullptr)
{
    if (m_valueRange.getType().isVoid())
    {
        // Compute random value range
        int maxScalars = state.getShaderParameters().maxCombinedVariableScalars -
                         state.getVariableManager().getNumAllocatedScalars();
        bool useRandomRange = !state.getVariableManager().hasEntry<IsWritableEntry>() ||
                              ((maxScalars > 0) && getWeightedBool(state.getRandom(), 0.1f));

        if (useRandomRange)
        {
            DE_ASSERT(maxScalars > 0);
            m_valueRange = ValueRange(computeRandomType(state, maxScalars));
            computeRandomValueRange(state, m_valueRange.asAccess());
        }
        else
        {
            // Use value range from random entry
            // \todo [2011-02-28 pyry] Give lower weight to entries without range? Choose subtype range?
            const ValueEntry *entry =
                state.getRandom().choose<const ValueEntry *>(state.getVariableManager().getBegin<IsWritableEntry>(),
                                                             state.getVariableManager().getEnd<IsWritableEntry>());
            m_valueRange = ValueRange(entry->getValueRange());

            computeRandomValueRangeForInfElements(state, m_valueRange.asAccess());

            DE_ASSERT(state.getVariableManager().hasEntry(IsWritableIntersectingEntry(m_valueRange.asAccess())));
        }
    }

    IsWritableIntersectingEntry::Iterator first =
        state.getVariableManager().getBegin(IsWritableIntersectingEntry(m_valueRange.asAccess()));
    IsWritableIntersectingEntry::Iterator end =
        state.getVariableManager().getEnd(IsWritableIntersectingEntry(m_valueRange.asAccess()));

    bool possiblyCreateVar = canAllocateVariable(state, m_valueRange.getType()) &&
                             (first == end || getWeightedBool(state.getRandom(), 0.5f));

    if (!possiblyCreateVar)
    {
        // Find all possible valueranges matching given type and intersecting with valuerange
        // \todo [pyry] Actually collect all ValueRanges, currently operates only on whole variables
        DE_ASSERT(first != end);

        // Try to select one closest to given range but bigger (eg. superset)
        bool supersetExists = false;
        for (IsWritableIntersectingEntry::Iterator i = first; i != end; i++)
        {
            if ((*i)->getValueRange().isSupersetOf(m_valueRange.asAccess()))
            {
                supersetExists = true;
                break;
            }
        }

        if (!supersetExists)
        {
            // Select some other range and compute intersection
            // \todo [2011-02-03 pyry] Use some heuristics to select the range?
            ConstValueRangeAccess selectedRange =
                state.getRandom().choose<const ValueEntry *>(first, end)->getValueRange();

            ValueRange::computeIntersection(m_valueRange.asAccess(), m_valueRange.asAccess(), selectedRange);
        }
    }
}

AssignOp::~AssignOp(void)
{
    delete m_lvalueExpr;
    delete m_rvalueExpr;
}

float AssignOp::getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange)
{
    if (!valueRange.getType().isVoid() && !canAllocateVariable(state, valueRange.getType()) &&
        !state.getVariableManager().hasEntry(IsWritableIntersectingEntry(valueRange)))
        return 0.0f; // Would require creating a new variable

    if (!valueRange.getType().isVoid() &&
        state.getExpressionDepth() + getTypeConstructorDepth(valueRange.getType()) + 1 >=
            state.getShaderParameters().maxExpressionDepth)
        return 0.0f;

    if (valueRange.getType().isVoid() && !state.getVariableManager().hasEntry<IsWritableEntry>() &&
        state.getVariableManager().getNumAllocatedScalars() >= state.getShaderParameters().maxCombinedVariableScalars)
        return 0.0f; // Can not allocate a new entry

    if (state.getExpressionDepth() == 0)
        return 4.0f;
    else
        return 0.0f; // \todo [pyry] Fix assign ops
}

Expression *AssignOp::createNextChild(GeneratorState &state)
{
    if (m_lvalueExpr == nullptr)
    {
        // Construct lvalue
        // \todo [2011-03-14 pyry] Proper l-value generation:
        //  - pure L-value part is generated first
        //  - variable valuerange is made unbound
        //  - R-value is generated
        //  - R-values in L-value are generated
        m_lvalueExpr = Expression::createRandomLValue(state, m_valueRange.asAccess());
        return m_lvalueExpr;
    }
    else if (m_rvalueExpr == nullptr)
    {
        // Construct value expr
        m_rvalueExpr = Expression::createRandom(state, m_valueRange.asAccess());
        return m_rvalueExpr;
    }
    else
        return nullptr;
}

void AssignOp::tokenize(GeneratorState &state, TokenStream &str) const
{
    m_lvalueExpr->tokenize(state, str);
    str << Token::EQUAL;
    m_rvalueExpr->tokenize(state, str);
}

void AssignOp::evaluate(ExecutionContext &evalCtx)
{
    // Evaluate l-value
    m_lvalueExpr->evaluate(evalCtx);

    // Evaluate value
    m_rvalueExpr->evaluate(evalCtx);
    m_value.setStorage(m_valueRange.getType());
    m_value.getValue(m_valueRange.getType()) = m_rvalueExpr->getValue().value();

    // Assign
    assignMasked(m_lvalueExpr->getLValue(), m_value.getValue(m_valueRange.getType()), evalCtx.getExecutionMask());
}

namespace
{

inline bool isShaderInOutSupportedType(const VariableType &type)
{
    // \todo [2011-03-11 pyry] Float arrays, structs?
    return type.getBaseType() == VariableType::TYPE_FLOAT;
}

Variable *allocateNewVariable(GeneratorState &state, ConstValueRangeAccess valueRange)
{
    Variable *variable = state.getVariableManager().allocate(valueRange.getType());

    // Update value range
    state.getVariableManager().setValue(variable, valueRange);

    // Random storage \todo [pyry] Check that scalar count in uniform/input classes is not exceeded
    static const Variable::Storage storages[] = {Variable::STORAGE_CONST, Variable::STORAGE_UNIFORM,
                                                 Variable::STORAGE_LOCAL, Variable::STORAGE_SHADER_IN};
    float weights[DE_LENGTH_OF_ARRAY(storages)];

    // Dynamic vs. constant weight.
    float dynWeight = computeDynamicRangeWeight(valueRange);
    int numScalars  = valueRange.getType().getScalarSize();
    bool uniformOk  = state.getVariableManager().getNumAllocatedUniformScalars() + numScalars <=
                     state.getShaderParameters().maxUniformScalars;
    bool shaderInOk = isShaderInOutSupportedType(valueRange.getType()) &&
                      (state.getVariableManager().getNumAllocatedShaderInVariables() + NUM_RESERVED_SHADER_INPUTS <
                       state.getShaderParameters().maxInputVariables);

    weights[0] = de::max(1.0f - dynWeight, 0.1f);
    weights[1] = uniformOk ? dynWeight * 0.5f : 0.0f;
    weights[2] = dynWeight;
    weights[3] = shaderInOk ? dynWeight * 2.0f : 0.0f;

    state.getVariableManager().setStorage(variable,
                                          state.getRandom().chooseWeighted<Variable::Storage>(
                                              &storages[0], &storages[DE_LENGTH_OF_ARRAY(storages)], &weights[0]));

    return variable;
}

inline float combineWeight(float curCombinedWeight, float partialWeight)
{
    return curCombinedWeight * partialWeight;
}

float computeEntryReadWeight(ConstValueRangeAccess entryValueRange, ConstValueRangeAccess readValueRange)
{
    const VariableType &type = entryValueRange.getType();
    DE_ASSERT(type == readValueRange.getType());

    float weight = 1.0f;

    switch (type.getBaseType())
    {
    case VariableType::TYPE_FLOAT:
    {
        for (int elemNdx = 0; elemNdx < type.getNumElements(); elemNdx++)
        {
            float entryMin = entryValueRange.component(elemNdx).getMin().asFloat();
            float entryMax = entryValueRange.component(elemNdx).getMax().asFloat();
            float readMin  = readValueRange.component(elemNdx).getMin().asFloat();
            float readMax  = readValueRange.component(elemNdx).getMax().asFloat();

            // Check for -inf..inf ranges - they don't bring down the weight.
            if (Scalar::min<float>() == entryMin && Scalar::max<float>() == entryMax)
                continue;

            // Intersection to entry value range length ratio.
            float intersectionMin = deFloatMax(entryMin, readMin);
            float intersectionMax = deFloatMin(entryMax, readMax);
            float entryRangeLen   = entryMax - entryMin;
            float readRangeLen    = readMax - readMin;
            float intersectionLen = intersectionMax - intersectionMin;
            float entryRatio      = (entryRangeLen > 0.0f) ? (intersectionLen / entryRangeLen) : 1.0f;
            float readRatio       = (readRangeLen > 0.0f) ? (intersectionLen / readRangeLen) : 1.0f;
            float elementWeight   = 0.5f * readRatio + 0.5f * entryRatio;

            weight = combineWeight(weight, elementWeight);
        }
        break;
    }

    case VariableType::TYPE_INT:
    {
        for (int elemNdx = 0; elemNdx < type.getNumElements(); elemNdx++)
        {
            int entryMin = entryValueRange.component(elemNdx).getMin().asInt();
            int entryMax = entryValueRange.component(elemNdx).getMax().asInt();
            int readMin  = readValueRange.component(elemNdx).getMin().asInt();
            int readMax  = readValueRange.component(elemNdx).getMax().asInt();

            // Check for -inf..inf ranges - they don't bring down the weight.
            if (Scalar::min<int>() == entryMin && Scalar::max<int>() == entryMax)
                continue;

            // Intersection to entry value range length ratio.
            int intersectionMin     = deMax32(entryMin, readMin);
            int intersectionMax     = deMin32(entryMax, readMax);
            int64_t entryRangeLen   = (int64_t)entryMax - (int64_t)entryMin;
            int64_t readRangeLen    = (int64_t)readMax - (int64_t)readMin;
            int64_t intersectionLen = (int64_t)intersectionMax - (int64_t)intersectionMin;
            float entryRatio        = (entryRangeLen > 0) ? ((float)intersectionLen / (float)entryRangeLen) : 1.0f;
            float readRatio         = (readRangeLen > 0) ? ((float)intersectionLen / (float)readRangeLen) : 1.0f;
            float elementWeight     = 0.5f * readRatio + 0.5f * entryRatio;

            weight = combineWeight(weight, elementWeight);
        }
        break;
    }

    case VariableType::TYPE_BOOL:
    {
        // \todo
        break;
    }

    case VariableType::TYPE_ARRAY:
    case VariableType::TYPE_STRUCT:

    default:
        TCU_FAIL("Unsupported type");
    }

    return deFloatMax(weight, 0.01f);
}

} // namespace

VariableRead::VariableRead(GeneratorState &state, ConstValueRangeAccess valueRange)
{
    if (valueRange.getType().isVoid())
    {
        IsReadableEntry filter = IsReadableEntry(state.getExpressionFlags());
        int maxScalars         = state.getShaderParameters().maxCombinedVariableScalars -
                         state.getVariableManager().getNumAllocatedScalars();
        bool useRandomRange = !state.getVariableManager().hasEntry(filter) ||
                              ((maxScalars > 0) && getWeightedBool(state.getRandom(), 0.5f));

        if (useRandomRange)
        {
            // Allocate a new variable
            DE_ASSERT(maxScalars > 0);
            ValueRange newVarRange(computeRandomType(state, maxScalars));
            computeRandomValueRange(state, newVarRange.asAccess());

            m_variable = allocateNewVariable(state, newVarRange.asAccess());
        }
        else
        {
            // Use random entry \todo [pyry] Handle -inf..inf ranges?
            m_variable = state.getRandom()
                             .choose<const ValueEntry *>(state.getVariableManager().getBegin(filter),
                                                         state.getVariableManager().getEnd(filter))
                             ->getVariable();
        }
    }
    else
    {
        // Find variable that has value range that intersects with given range
        IsReadableIntersectingEntry::Iterator first =
            state.getVariableManager().getBegin(IsReadableIntersectingEntry(valueRange, state.getExpressionFlags()));
        IsReadableIntersectingEntry::Iterator end =
            state.getVariableManager().getEnd(IsReadableIntersectingEntry(valueRange, state.getExpressionFlags()));

        const float createOnReadWeight = 0.5f;
        bool createVar                 = canAllocateVariable(state, valueRange.getType()) &&
                         (first == end || getWeightedBool(state.getRandom(), createOnReadWeight));

        if (createVar)
        {
            m_variable = allocateNewVariable(state, valueRange);
        }
        else
        {
            // Copy value entries for computing weights.
            std::vector<const ValueEntry *> availableVars;
            std::vector<float> weights;

            std::copy(first, end, std::inserter(availableVars, availableVars.begin()));

            // Compute weights.
            weights.resize(availableVars.size());
            for (int ndx = 0; ndx < (int)availableVars.size(); ndx++)
                weights[ndx] = computeEntryReadWeight(availableVars[ndx]->getValueRange(), valueRange);

            // Select.
            const ValueEntry *entry = state.getRandom().chooseWeighted<const ValueEntry *>(
                availableVars.begin(), availableVars.end(), weights.begin());
            m_variable = entry->getVariable();

            // Compute intersection
            ValueRange intersection(m_variable->getType());
            ValueRange::computeIntersection(intersection, entry->getValueRange(), valueRange);
            state.getVariableManager().setValue(m_variable, intersection.asAccess());
        }
    }
}

VariableRead::VariableRead(const Variable *variable)
{
    m_variable = variable;
}

float VariableRead::getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange)
{
    if (valueRange.getType().isVoid())
    {
        if (state.getVariableManager().hasEntry(IsReadableEntry(state.getExpressionFlags())) ||
            state.getVariableManager().getNumAllocatedScalars() <
                state.getShaderParameters().maxCombinedVariableScalars)
            return unusedValueWeight;
        else
            return 0.0f;
    }

    if (!canAllocateVariable(state, valueRange.getType()) &&
        !state.getVariableManager().hasEntry(IsReadableIntersectingEntry(valueRange, state.getExpressionFlags())))
        return 0.0f;
    else
        return 1.0f;
}

VariableWrite::VariableWrite(GeneratorState &state, ConstValueRangeAccess valueRange)
{
    DE_ASSERT(!valueRange.getType().isVoid());

    // Find variable with range that is superset of given range
    IsWritableSupersetEntry::Iterator first = state.getVariableManager().getBegin(IsWritableSupersetEntry(valueRange));
    IsWritableSupersetEntry::Iterator end   = state.getVariableManager().getEnd(IsWritableSupersetEntry(valueRange));

    const float createOnAssignWeight = 0.1f; // Will essentially create an unused variable
    bool createVar                   = canAllocateVariable(state, valueRange.getType()) &&
                     (first == end || getWeightedBool(state.getRandom(), createOnAssignWeight));

    if (createVar)
    {
        m_variable = state.getVariableManager().allocate(valueRange.getType());
        // \note Storage will be LOCAL
    }
    else
    {
        // Choose random
        DE_ASSERT(first != end);
        const ValueEntry *entry = state.getRandom().choose<const ValueEntry *>(first, end);
        m_variable              = entry->getVariable();
    }

    DE_ASSERT(m_variable);

    // Reset value range.
    const ValueEntry *parentEntry = state.getVariableManager().getParentValue(m_variable);
    if (parentEntry)
    {
        // Use parent value range.
        state.getVariableManager().setValue(m_variable, parentEntry->getValueRange());
    }
    else
    {
        // Use infinite range.
        ValueRange infRange(m_variable->getType());
        setInfiniteRange(infRange);

        state.getVariableManager().setValue(m_variable, infRange.asAccess());
    }
}

float VariableWrite::getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange)
{
    if (!canAllocateVariable(state, valueRange.getType()) &&
        !state.getVariableManager().hasEntry(IsWritableSupersetEntry(valueRange)))
        return 0.0f;
    else
        return 1.0f;
}

void VariableAccess::evaluate(ExecutionContext &evalCtx)
{
    m_valueAccess = evalCtx.getValue(m_variable);
}

ParenOp::ParenOp(GeneratorState &state, ConstValueRangeAccess valueRange) : m_valueRange(valueRange), m_child(nullptr)
{
    DE_UNREF(state);
}

ParenOp::~ParenOp(void)
{
    delete m_child;
}

Expression *ParenOp::createNextChild(GeneratorState &state)
{
    if (m_child == nullptr)
    {
        m_child = Expression::createRandom(state, m_valueRange.asAccess());
        return m_child;
    }
    else
        return nullptr;
}

void ParenOp::tokenize(GeneratorState &state, TokenStream &str) const
{
    str << Token::LEFT_PAREN;
    m_child->tokenize(state, str);
    str << Token::RIGHT_PAREN;
}

void ParenOp::setChild(Expression *expression)
{
    m_child = expression;
}

float ParenOp::getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange)
{
    if (valueRange.getType().isVoid())
        return state.getExpressionDepth() + 2 <= state.getShaderParameters().maxExpressionDepth ? unusedValueWeight :
                                                                                                  0.0f;
    else
    {
        int requiredDepth = 1 + getConservativeValueExprDepth(state, valueRange);
        return state.getExpressionDepth() + requiredDepth <= state.getShaderParameters().maxExpressionDepth ? 1.0f :
                                                                                                              0.0f;
    }
}

const int swizzlePrecedence = 2;

SwizzleOp::SwizzleOp(GeneratorState &state, ConstValueRangeAccess valueRange)
    : m_outValueRange(valueRange)
    , m_numInputElements(0)
    , m_child(nullptr)
{
    DE_ASSERT(!m_outValueRange.getType().isVoid()); // \todo [2011-06-13 pyry] Void support
    DE_ASSERT(m_outValueRange.getType().isFloatOrVec() || m_outValueRange.getType().isIntOrVec() ||
              m_outValueRange.getType().isBoolOrVec());

    m_value.setStorage(m_outValueRange.getType());

    int numOutputElements = m_outValueRange.getType().getNumElements();

    // \note Swizzle works for vector types only.
    // \todo [2011-06-13 pyry] Use components multiple times.
    m_numInputElements = state.getRandom().getInt(deMax32(numOutputElements, 2), 4);

    std::set<int> availableElements;
    for (int ndx = 0; ndx < m_numInputElements; ndx++)
        availableElements.insert(ndx);

    // Randomize swizzle.
    for (int elemNdx = 0; elemNdx < (int)DE_LENGTH_OF_ARRAY(m_swizzle); elemNdx++)
    {
        if (elemNdx < numOutputElements)
        {
            int inElemNdx = state.getRandom().choose<int>(availableElements.begin(), availableElements.end());
            availableElements.erase(inElemNdx);
            m_swizzle[elemNdx] = (uint8_t)inElemNdx;
        }
        else
            m_swizzle[elemNdx] = 0;
    }
}

SwizzleOp::~SwizzleOp(void)
{
    delete m_child;
}

Expression *SwizzleOp::createNextChild(GeneratorState &state)
{
    if (m_child)
        return nullptr;

    // Compute input value range.
    VariableType inVarType  = VariableType(m_outValueRange.getType().getBaseType(), m_numInputElements);
    ValueRange inValueRange = ValueRange(inVarType);

    // Initialize all inputs to -inf..inf
    setInfiniteRange(inValueRange);

    // Compute intersections.
    int numOutputElements = m_outValueRange.getType().getNumElements();
    for (int outElemNdx = 0; outElemNdx < numOutputElements; outElemNdx++)
    {
        int inElemNdx = m_swizzle[outElemNdx];
        ValueRange::computeIntersection(inValueRange.asAccess().component(inElemNdx),
                                        inValueRange.asAccess().component(inElemNdx),
                                        m_outValueRange.asAccess().component(outElemNdx));
    }

    // Create child.
    state.pushPrecedence(swizzlePrecedence);
    m_child = Expression::createRandom(state, inValueRange.asAccess());
    state.popPrecedence();

    return m_child;
}

void SwizzleOp::tokenize(GeneratorState &state, TokenStream &str) const
{
    const char *rgbaSet[]   = {"r", "g", "b", "a"};
    const char *xyzwSet[]   = {"x", "y", "z", "w"};
    const char *stpqSet[]   = {"s", "t", "p", "q"};
    const char **swizzleSet = nullptr;

    switch (state.getRandom().getInt(0, 2))
    {
    case 0:
        swizzleSet = rgbaSet;
        break;
    case 1:
        swizzleSet = xyzwSet;
        break;
    case 2:
        swizzleSet = stpqSet;
        break;
    default:
        DE_ASSERT(false);
    }

    std::string swizzleStr;
    for (int elemNdx = 0; elemNdx < m_outValueRange.getType().getNumElements(); elemNdx++)
        swizzleStr += swizzleSet[m_swizzle[elemNdx]];

    m_child->tokenize(state, str);
    str << Token::DOT << Token(swizzleStr.c_str());
}

float SwizzleOp::getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange)
{
    if (!state.getProgramParameters().useSwizzle)
        return 0.0f;

    if (state.getPrecedence() < swizzlePrecedence)
        return 0.0f;

    if (!valueRange.getType().isFloatOrVec() && !valueRange.getType().isIntOrVec() &&
        !valueRange.getType().isBoolOrVec())
        return 0.0f;

    int availableLevels = state.getShaderParameters().maxExpressionDepth - state.getExpressionDepth();

    // Swizzle + Constructor + Values
    if (availableLevels < 3)
        return 0.0f;

    return 1.0f;
}

void SwizzleOp::evaluate(ExecutionContext &execCtx)
{
    m_child->evaluate(execCtx);

    ExecConstValueAccess inValue = m_child->getValue();
    ExecValueAccess outValue     = m_value.getValue(m_outValueRange.getType());

    for (int outElemNdx = 0; outElemNdx < outValue.getType().getNumElements(); outElemNdx++)
    {
        int inElemNdx                  = m_swizzle[outElemNdx];
        outValue.component(outElemNdx) = inValue.component(inElemNdx).value();
    }
}

static int countSamplers(const VariableManager &varManager, VariableType::Type samplerType)
{
    int numSamplers = 0;

    IsSamplerEntry::Iterator i   = varManager.getBegin(IsSamplerEntry(samplerType));
    IsSamplerEntry::Iterator end = varManager.getEnd(IsSamplerEntry(samplerType));

    for (; i != end; i++)
        numSamplers += 1;

    return numSamplers;
}

TexLookup::TexLookup(GeneratorState &state, ConstValueRangeAccess valueRange)
    : m_type(TYPE_LAST)
    , m_coordExpr(nullptr)
    , m_lodBiasExpr(nullptr)
    , m_valueType(VariableType::TYPE_FLOAT, 4)
    , m_value(m_valueType)
{
    DE_ASSERT(valueRange.getType() == VariableType(VariableType::TYPE_FLOAT, 4));
    DE_UNREF(valueRange); // Texture output value range is constant.

    // Select type.
    vector<Type> typeCandidates;
    if (state.getShaderParameters().useTexture2D)
    {
        typeCandidates.push_back(TYPE_TEXTURE2D);
        typeCandidates.push_back(TYPE_TEXTURE2D_LOD);
        typeCandidates.push_back(TYPE_TEXTURE2D_PROJ);
        typeCandidates.push_back(TYPE_TEXTURE2D_PROJ_LOD);
    }

    if (state.getShaderParameters().useTextureCube)
    {
        typeCandidates.push_back(TYPE_TEXTURECUBE);
        typeCandidates.push_back(TYPE_TEXTURECUBE_LOD);
    }

    m_type = state.getRandom().choose<Type>(typeCandidates.begin(), typeCandidates.end());

    // Select or allocate sampler.
    VariableType::Type samplerType = VariableType::TYPE_LAST;
    switch (m_type)
    {
    case TYPE_TEXTURE2D:
    case TYPE_TEXTURE2D_LOD:
    case TYPE_TEXTURE2D_PROJ:
    case TYPE_TEXTURE2D_PROJ_LOD:
        samplerType = VariableType::TYPE_SAMPLER_2D;
        break;

    case TYPE_TEXTURECUBE:
    case TYPE_TEXTURECUBE_LOD:
        samplerType = VariableType::TYPE_SAMPLER_CUBE;
        break;

    default:
        DE_ASSERT(false);
    }

    int sampler2DCount   = countSamplers(state.getVariableManager(), VariableType::TYPE_SAMPLER_2D);
    int samplerCubeCount = countSamplers(state.getVariableManager(), VariableType::TYPE_SAMPLER_CUBE);
    bool canAllocSampler = sampler2DCount + samplerCubeCount < state.getShaderParameters().maxSamplers;
    bool hasSampler      = samplerType == VariableType::TYPE_SAMPLER_2D ? (sampler2DCount > 0) : (samplerCubeCount > 0);
    bool allocSampler    = !hasSampler || (canAllocSampler && state.getRandom().getBool());

    if (allocSampler)
    {
        Variable *sampler = state.getVariableManager().allocate(VariableType(samplerType, 1));
        state.getVariableManager().setStorage(sampler, Variable::STORAGE_UNIFORM); // Samplers are always uniforms.
        m_sampler = sampler;
    }
    else
        m_sampler = state.getRandom()
                        .choose<const ValueEntry *>(state.getVariableManager().getBegin(IsSamplerEntry(samplerType)),
                                                    state.getVariableManager().getEnd(IsSamplerEntry(samplerType)))
                        ->getVariable();
}

TexLookup::~TexLookup(void)
{
    delete m_coordExpr;
    delete m_lodBiasExpr;
}

Expression *TexLookup::createNextChild(GeneratorState &state)
{
    bool hasLodBias =
        m_type == TYPE_TEXTURE2D_LOD || m_type == TYPE_TEXTURE2D_PROJ_LOD || m_type == TYPE_TEXTURECUBE_LOD;

    if (hasLodBias && !m_lodBiasExpr)
    {
        ValueRange lodRange(VariableType(VariableType::TYPE_FLOAT, 1));
        setInfiniteRange(lodRange); // Any value is valid.

        m_lodBiasExpr = Expression::createRandom(state, lodRange.asAccess());
        return m_lodBiasExpr;
    }

    if (!m_coordExpr)
    {
        if (m_type == TYPE_TEXTURECUBE || m_type == TYPE_TEXTURECUBE_LOD)
        {
            // Make sure major axis selection can be done.
            int majorAxisNdx = state.getRandom().getInt(0, 2);

            ValueRange coordRange(VariableType(VariableType::TYPE_FLOAT, 3));

            for (int ndx = 0; ndx < 3; ndx++)
            {
                if (ndx == majorAxisNdx)
                {
                    bool neg                           = state.getRandom().getBool();
                    coordRange.getMin().component(ndx) = neg ? -4.0f : 2.25f;
                    coordRange.getMax().component(ndx) = neg ? -2.25f : 4.0f;
                }
                else
                {
                    coordRange.getMin().component(ndx) = -2.0f;
                    coordRange.getMax().component(ndx) = 2.0f;
                }
            }

            m_coordExpr = Expression::createRandom(state, coordRange.asAccess());
        }
        else
        {
            bool isProj         = m_type == TYPE_TEXTURE2D_PROJ || m_type == TYPE_TEXTURE2D_PROJ_LOD;
            int coordScalarSize = isProj ? 3 : 2;

            ValueRange coordRange(VariableType(VariableType::TYPE_FLOAT, coordScalarSize));
            setInfiniteRange(coordRange); // Initialize base range with -inf..inf

            if (isProj)
            {
                // w coordinate must be something sane, and not 0.
                bool neg                         = state.getRandom().getBool();
                coordRange.getMin().component(2) = neg ? -4.0f : 0.25f;
                coordRange.getMax().component(2) = neg ? -0.25f : 4.0f;
            }

            m_coordExpr = Expression::createRandom(state, coordRange.asAccess());
        }

        DE_ASSERT(m_coordExpr);
        return m_coordExpr;
    }

    return nullptr; // Done.
}

void TexLookup::tokenize(GeneratorState &state, TokenStream &str) const
{
    bool isVertex = state.getShader().getType() == Shader::TYPE_VERTEX;

    if (state.getProgramParameters().version == VERSION_300)
    {
        switch (m_type)
        {
        case TYPE_TEXTURE2D:
            str << "texture";
            break;
        case TYPE_TEXTURE2D_LOD:
            str << (isVertex ? "textureLod" : "texture");
            break;
        case TYPE_TEXTURE2D_PROJ:
            str << "textureProj";
            break;
        case TYPE_TEXTURE2D_PROJ_LOD:
            str << (isVertex ? "textureProjLod" : "textureProj");
            break;
        case TYPE_TEXTURECUBE:
            str << "texture";
            break;
        case TYPE_TEXTURECUBE_LOD:
            str << (isVertex ? "textureLod" : "texture");
            break;
        default:
            DE_ASSERT(false);
        }
    }
    else
    {
        switch (m_type)
        {
        case TYPE_TEXTURE2D:
            str << "texture2D";
            break;
        case TYPE_TEXTURE2D_LOD:
            str << (isVertex ? "texture2DLod" : "texture2D");
            break;
        case TYPE_TEXTURE2D_PROJ:
            str << "texture2DProj";
            break;
        case TYPE_TEXTURE2D_PROJ_LOD:
            str << (isVertex ? "texture2DProjLod" : "texture2DProj");
            break;
        case TYPE_TEXTURECUBE:
            str << "textureCube";
            break;
        case TYPE_TEXTURECUBE_LOD:
            str << (isVertex ? "textureCubeLod" : "textureCube");
            break;
        default:
            DE_ASSERT(false);
        }
    }

    str << Token::LEFT_PAREN;
    str << m_sampler->getName();
    str << Token::COMMA;
    m_coordExpr->tokenize(state, str);

    if (m_lodBiasExpr)
    {
        str << Token::COMMA;
        m_lodBiasExpr->tokenize(state, str);
    }

    str << Token::RIGHT_PAREN;
}

float TexLookup::getWeight(const GeneratorState &state, ConstValueRangeAccess valueRange)
{
    if (state.getShaderParameters().texLookupBaseWeight <= 0.0f)
        return 0.0f;

    int availableLevels = state.getShaderParameters().maxExpressionDepth - state.getExpressionDepth();

    // Lookup + Constructor + Values
    if (availableLevels < 3)
        return 0.0f;

    if (state.getExpressionFlags() & (CONST_EXPR | NO_VAR_ALLOCATION))
        return 0.0f;

    if (valueRange.getType() != VariableType(VariableType::TYPE_FLOAT, 4))
        return 0.0f;

    ValueRange texOutputRange(VariableType(VariableType::TYPE_FLOAT, 4));
    for (int ndx = 0; ndx < 4; ndx++)
    {
        texOutputRange.getMin().component(ndx) = 0.0f;
        texOutputRange.getMax().component(ndx) = 1.0f;
    }

    if (!valueRange.isSupersetOf(texOutputRange.asAccess()))
        return 0.0f;

    return state.getShaderParameters().texLookupBaseWeight;
}

void TexLookup::evaluate(ExecutionContext &execCtx)
{
    // Evaluate coord and bias.
    m_coordExpr->evaluate(execCtx);
    if (m_lodBiasExpr)
        m_lodBiasExpr->evaluate(execCtx);

    ExecConstValueAccess coords = m_coordExpr->getValue();
    ExecValueAccess dst         = m_value.getValue(m_valueType);

    switch (m_type)
    {
    case TYPE_TEXTURE2D:
    {
        const Sampler2D &tex = execCtx.getSampler2D(m_sampler);
        for (int i = 0; i < EXEC_VEC_WIDTH; i++)
        {
            float s     = coords.component(0).asFloat(i);
            float t     = coords.component(1).asFloat(i);
            tcu::Vec4 p = tex.sample(s, t, 0.0f);

            for (int comp = 0; comp < 4; comp++)
                dst.component(comp).asFloat(i) = p[comp];
        }
        break;
    }

    case TYPE_TEXTURE2D_LOD:
    {
        ExecConstValueAccess lod = m_lodBiasExpr->getValue();
        const Sampler2D &tex     = execCtx.getSampler2D(m_sampler);
        for (int i = 0; i < EXEC_VEC_WIDTH; i++)
        {
            float s     = coords.component(0).asFloat(i);
            float t     = coords.component(1).asFloat(i);
            float l     = lod.component(0).asFloat(i);
            tcu::Vec4 p = tex.sample(s, t, l);

            for (int comp = 0; comp < 4; comp++)
                dst.component(comp).asFloat(i) = p[comp];
        }
        break;
    }

    case TYPE_TEXTURE2D_PROJ:
    {
        const Sampler2D &tex = execCtx.getSampler2D(m_sampler);
        for (int i = 0; i < EXEC_VEC_WIDTH; i++)
        {
            float s     = coords.component(0).asFloat(i);
            float t     = coords.component(1).asFloat(i);
            float w     = coords.component(2).asFloat(i);
            tcu::Vec4 p = tex.sample(s / w, t / w, 0.0f);

            for (int comp = 0; comp < 4; comp++)
                dst.component(comp).asFloat(i) = p[comp];
        }
        break;
    }

    case TYPE_TEXTURE2D_PROJ_LOD:
    {
        ExecConstValueAccess lod = m_lodBiasExpr->getValue();
        const Sampler2D &tex     = execCtx.getSampler2D(m_sampler);
        for (int i = 0; i < EXEC_VEC_WIDTH; i++)
        {
            float s     = coords.component(0).asFloat(i);
            float t     = coords.component(1).asFloat(i);
            float w     = coords.component(2).asFloat(i);
            float l     = lod.component(0).asFloat(i);
            tcu::Vec4 p = tex.sample(s / w, t / w, l);

            for (int comp = 0; comp < 4; comp++)
                dst.component(comp).asFloat(i) = p[comp];
        }
        break;
    }

    case TYPE_TEXTURECUBE:
    {
        const SamplerCube &tex = execCtx.getSamplerCube(m_sampler);
        for (int i = 0; i < EXEC_VEC_WIDTH; i++)
        {
            float s     = coords.component(0).asFloat(i);
            float t     = coords.component(1).asFloat(i);
            float r     = coords.component(2).asFloat(i);
            tcu::Vec4 p = tex.sample(s, t, r, 0.0f);

            for (int comp = 0; comp < 4; comp++)
                dst.component(comp).asFloat(i) = p[comp];
        }
        break;
    }

    case TYPE_TEXTURECUBE_LOD:
    {
        ExecConstValueAccess lod = m_lodBiasExpr->getValue();
        const SamplerCube &tex   = execCtx.getSamplerCube(m_sampler);
        for (int i = 0; i < EXEC_VEC_WIDTH; i++)
        {
            float s     = coords.component(0).asFloat(i);
            float t     = coords.component(1).asFloat(i);
            float r     = coords.component(2).asFloat(i);
            float l     = lod.component(0).asFloat(i);
            tcu::Vec4 p = tex.sample(s, t, r, l);

            for (int comp = 0; comp < 4; comp++)
                dst.component(comp).asFloat(i) = p[comp];
        }
        break;
    }

    default:
        DE_ASSERT(false);
    }
}

} // namespace rsg
