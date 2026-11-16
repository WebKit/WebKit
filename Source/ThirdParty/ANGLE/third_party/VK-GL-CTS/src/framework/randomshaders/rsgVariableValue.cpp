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
 * \brief Variable Value class.
 *//*--------------------------------------------------------------------*/

#include "rsgVariableValue.hpp"

namespace rsg
{

namespace
{

template <class CompareOp>
bool compareValueRangesAllTrue(const ConstValueRangeAccess &a, const ConstValueRangeAccess &b)
{
    DE_ASSERT(a.getType() == b.getType());

    if (a.getType().isStruct())
    {
        int numMembers = (int)a.getType().getMembers().size();
        for (int ndx = 0; ndx < numMembers; ndx++)
        {
            if (!compareValueRangesAllTrue<CompareOp>(a.member(ndx), b.member(ndx)))
                return false;
        }
    }
    else if (a.getType().isArray())
    {
        int numElements = (int)a.getType().getNumElements();
        for (int ndx = 0; ndx < numElements; ndx++)
        {
            if (!compareValueRangesAllTrue<CompareOp>(a.arrayElement(ndx), b.arrayElement(ndx)))
                return false;
        }
    }
    else
    {
        int numElements = (int)a.getType().getNumElements();
        switch (a.getType().getBaseType())
        {
        case VariableType::TYPE_FLOAT:
            for (int ndx = 0; ndx < numElements; ndx++)
            {
                float aMin = a.component(ndx).getMin().asFloat();
                float aMax = a.component(ndx).getMax().asFloat();
                float bMin = b.component(ndx).getMin().asFloat();
                float bMax = b.component(ndx).getMax().asFloat();

                if (!CompareOp()(aMin, aMax, bMin, bMax))
                    return false;
            }
            break;

        case VariableType::TYPE_INT:
        case VariableType::TYPE_SAMPLER_2D:
        case VariableType::TYPE_SAMPLER_CUBE:
            for (int ndx = 0; ndx < numElements; ndx++)
            {
                int aMin = a.component(ndx).getMin().asInt();
                int aMax = a.component(ndx).getMax().asInt();
                int bMin = b.component(ndx).getMin().asInt();
                int bMax = b.component(ndx).getMax().asInt();

                if (!CompareOp()(aMin, aMax, bMin, bMax))
                    return false;
            }
            break;

        case VariableType::TYPE_BOOL:
            for (int ndx = 0; ndx < numElements; ndx++)
            {
                bool aMin = a.component(ndx).getMin().asBool();
                bool aMax = a.component(ndx).getMax().asBool();
                bool bMin = b.component(ndx).getMin().asBool();
                bool bMax = b.component(ndx).getMax().asBool();

                if (!CompareOp()(aMin, aMax, bMin, bMax))
                    return false;
            }
            break;

        default:
            DE_ASSERT(false);
            return false;
        }
    }

    return true;
}

inline int toInt(bool boolVal)
{
    return boolVal ? 1 : 0;
}

struct CompareIntersection
{
    inline bool operator()(float aMin, float aMax, float bMin, float bMax) const
    {
        return (aMin <= bMax && bMin <= aMax);
    }
    inline bool operator()(int aMin, int aMax, int bMin, int bMax) const
    {
        return (aMin <= bMax && bMin <= aMax);
    }

    inline bool operator()(bool aMin, bool aMax, bool bMin, bool bMax) const
    {
        return CompareIntersection()(toInt(aMin), toInt(aMax), toInt(bMin), toInt(bMax));
    }
};

struct CompareIsSubsetOf
{
    inline bool operator()(float aMin, float aMax, float bMin, float bMax) const
    {
        return de::inRange(aMin, bMin, bMax) && de::inRange(aMax, bMin, bMax);
    }

    inline bool operator()(int aMin, int aMax, int bMin, int bMax) const
    {
        return de::inRange(aMin, bMin, bMax) && de::inRange(aMax, bMin, bMax);
    }

    inline bool operator()(bool aMin, bool aMax, bool bMin, bool bMax) const
    {
        return CompareIsSubsetOf()(toInt(aMin), toInt(aMax), toInt(bMin), toInt(bMax));
    }
};

} // namespace

bool ConstValueRangeAccess::intersects(const ConstValueRangeAccess &other) const
{
    return compareValueRangesAllTrue<CompareIntersection>(*this, other);
}

bool ConstValueRangeAccess::isSubsetOf(const ConstValueRangeAccess &other) const
{
    return compareValueRangesAllTrue<CompareIsSubsetOf>(*this, other);
}

bool ConstValueRangeAccess::isSupersetOf(const ConstValueRangeAccess &other) const
{
    return other.isSubsetOf(*this);
}

ValueRange::ValueRange(const VariableType &type)
    : m_type(type)
    , m_min(type.getScalarSize())
    , m_max(type.getScalarSize())
{
}

ValueRange::ValueRange(const VariableType &type, const ConstValueAccess &minVal, const ConstValueAccess &maxVal)
    : m_type(type)
    , m_min(type.getScalarSize())
    , m_max(type.getScalarSize())
{
    getMin() = minVal.value();
    getMax() = maxVal.value();
}

ValueRange::ValueRange(const VariableType &type, const Scalar *minVal, const Scalar *maxVal)
    : m_type(type)
    , m_min(type.getScalarSize())
    , m_max(type.getScalarSize())
{
    getMin() = ConstValueAccess(type, minVal).value();
    getMax() = ConstValueAccess(type, maxVal).value();
}

ValueRange::ValueRange(ConstValueRangeAccess other)
    : m_type(other.getType())
    , m_min(other.getType().getScalarSize())
    , m_max(other.getType().getScalarSize())
{
    getMin() = other.getMin().value();
    getMax() = other.getMax().value();
}

ValueRange::~ValueRange(void)
{
}

void ValueRange::computeIntersection(ValueRange &dst, const ConstValueRangeAccess &a, const ConstValueRangeAccess &b)
{
    computeIntersection(dst.asAccess(), a, b);
}

void ValueRange::computeIntersection(ValueRangeAccess dst, const ConstValueRangeAccess &a,
                                     const ConstValueRangeAccess &b)
{
    DE_ASSERT(dst.getType() == a.getType() && dst.getType() == b.getType());

    if (a.getType().isStruct())
    {
        int numMembers = (int)a.getType().getMembers().size();
        for (int ndx = 0; ndx < numMembers; ndx++)
            computeIntersection(dst.member(ndx), a.member(ndx), b.member(ndx));
    }
    else if (a.getType().isArray())
    {
        int numElements = (int)a.getType().getNumElements();
        for (int ndx = 0; ndx < numElements; ndx++)
            computeIntersection(dst.arrayElement(ndx), a.arrayElement(ndx), b.arrayElement(ndx));
    }
    else
    {
        int numElements = (int)a.getType().getNumElements();
        switch (a.getType().getBaseType())
        {
        case VariableType::TYPE_FLOAT:
            for (int ndx = 0; ndx < numElements; ndx++)
            {
                float aMin = a.component(ndx).getMin().asFloat();
                float aMax = a.component(ndx).getMax().asFloat();
                float bMin = b.component(ndx).getMin().asFloat();
                float bMax = b.component(ndx).getMax().asFloat();

                dst.component(ndx).getMin() = de::max(aMin, bMin);
                dst.component(ndx).getMax() = de::min(aMax, bMax);
            }
            break;

        case VariableType::TYPE_INT:
        case VariableType::TYPE_SAMPLER_2D:
        case VariableType::TYPE_SAMPLER_CUBE:
            for (int ndx = 0; ndx < numElements; ndx++)
            {
                int aMin = a.component(ndx).getMin().asInt();
                int aMax = a.component(ndx).getMax().asInt();
                int bMin = b.component(ndx).getMin().asInt();
                int bMax = b.component(ndx).getMax().asInt();

                dst.component(ndx).getMin() = de::max(aMin, bMin);
                dst.component(ndx).getMax() = de::min(aMax, bMax);
            }
            break;

        case VariableType::TYPE_BOOL:
            for (int ndx = 0; ndx < numElements; ndx++)
            {
                bool aMin = a.component(ndx).getMin().asBool();
                bool aMax = a.component(ndx).getMax().asBool();
                bool bMin = b.component(ndx).getMin().asBool();
                bool bMax = b.component(ndx).getMax().asBool();

                dst.component(ndx).getMin() = aMin || bMin;
                dst.component(ndx).getMax() = aMax && bMax;
            }
            break;

        default:
            DE_ASSERT(false);
        }
    }
}

VariableValue::VariableValue(const VariableValue &other)
    : m_variable(other.m_variable)
    , m_storage(other.m_variable->getType())
{
    m_storage.getValue(getType()) = other.getValue().value();
}

VariableValue &VariableValue::operator=(const VariableValue &other)
{
    m_variable = other.m_variable;
    m_storage.setStorage(getType());
    m_storage.getValue(getType()) = other.getValue().value();
    return *this;
}

} // namespace rsg
