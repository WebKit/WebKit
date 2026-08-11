#ifndef _RSGVARIABLEVALUE_HPP
#define _RSGVARIABLEVALUE_HPP
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

#include "rsgDefs.hpp"
#include "rsgVariableType.hpp"
#include "rsgVariable.hpp"
#include "tcuVector.hpp"

#include <algorithm>

namespace rsg
{

union Scalar
{
    int intVal;
    float floatVal;
    bool boolVal;

    Scalar(void) : intVal(0)
    {
    }
    Scalar(float v) : floatVal(v)
    {
    }
    Scalar(int v) : intVal(v)
    {
    }
    Scalar(bool v) : boolVal(v)
    {
    }

    // Bit-exact compare
    bool operator==(Scalar other) const
    {
        return intVal == other.intVal;
    }
    bool operator!=(Scalar other) const
    {
        return intVal != other.intVal;
    }

    template <typename T>
    static Scalar min(void);
    template <typename T>
    static Scalar max(void);

    template <typename T>
    T as(void) const;
    template <typename T>
    T &as(void);
};
DE_STATIC_ASSERT(sizeof(Scalar) == sizeof(uint32_t));

template <>
inline Scalar Scalar::min<float>(void)
{
    return Scalar((int)0xff800000);
}
template <>
inline Scalar Scalar::max<float>(void)
{
    return Scalar((int)0x7f800000);
}
template <>
inline Scalar Scalar::min<int>(void)
{
    return Scalar((int)0x80000000);
}
template <>
inline Scalar Scalar::max<int>(void)
{
    return Scalar((int)0x7fffffff);
}
template <>
inline Scalar Scalar::min<bool>(void)
{
    return Scalar(false);
}
template <>
inline Scalar Scalar::max<bool>(void)
{
    return Scalar(true);
}

template <>
inline float Scalar::as<float>(void) const
{
    return floatVal;
}
template <>
inline float &Scalar::as<float>(void)
{
    return floatVal;
}
template <>
inline int Scalar::as<int>(void) const
{
    return intVal;
}
template <>
inline int &Scalar::as<int>(void)
{
    return intVal;
}
template <>
inline bool Scalar::as<bool>(void) const
{
    return boolVal;
}
template <>
inline bool &Scalar::as<bool>(void)
{
    return boolVal;
}

template <int Stride>
class StridedValueRead
{
public:
    StridedValueRead(const VariableType &type, const Scalar *value) : m_type(type), m_value(value)
    {
    }

    const VariableType &getType(void) const
    {
        return m_type;
    }
    const Scalar *getValuePtr(void) const
    {
        return m_value;
    }

private:
    const VariableType &m_type;
    const Scalar *m_value;
};

template <int Stride>
class ConstStridedValueAccess
{
public:
    ConstStridedValueAccess(void) : m_type(nullptr), m_value(nullptr)
    {
    }
    ConstStridedValueAccess(const VariableType &type, const Scalar *valuePtr)
        : m_type(&type)
        , m_value(const_cast<Scalar *>(valuePtr))
    {
    }

    const VariableType &getType(void) const
    {
        return *m_type;
    }

    // Read-only access
    ConstStridedValueAccess component(int compNdx) const
    {
        return ConstStridedValueAccess(getType().getElementType(), m_value + Stride * compNdx);
    }
    ConstStridedValueAccess arrayElement(int elementNdx) const
    {
        return ConstStridedValueAccess(getType().getElementType(),
                                       m_value + Stride * getType().getElementScalarOffset(elementNdx));
    }
    ConstStridedValueAccess member(int memberNdx) const
    {
        return ConstStridedValueAccess(getType().getMembers()[memberNdx].getType(),
                                       m_value + Stride * getType().getMemberScalarOffset(memberNdx));
    }

    float asFloat(void) const
    {
        DE_STATIC_ASSERT(Stride == 1);
        return m_value->floatVal;
    }
    int asInt(void) const
    {
        DE_STATIC_ASSERT(Stride == 1);
        return m_value->intVal;
    }
    bool asBool(void) const
    {
        DE_STATIC_ASSERT(Stride == 1);
        return m_value->boolVal;
    }
    Scalar asScalar(void) const
    {
        DE_STATIC_ASSERT(Stride == 1);
        return *m_value;
    }

    float asFloat(int ndx) const
    {
        DE_ASSERT(de::inBounds(ndx, 0, Stride));
        return m_value[ndx].floatVal;
    }
    int asInt(int ndx) const
    {
        DE_ASSERT(de::inBounds(ndx, 0, Stride));
        return m_value[ndx].intVal;
    }
    bool asBool(int ndx) const
    {
        DE_ASSERT(de::inBounds(ndx, 0, Stride));
        return m_value[ndx].boolVal;
    }
    Scalar asScalar(int ndx) const
    {
        DE_ASSERT(de::inBounds(ndx, 0, Stride));
        return m_value[ndx];
    }

    template <typename T>
    T as(int ndx) const
    {
        DE_ASSERT(de::inBounds(ndx, 0, Stride));
        return this->m_value[ndx].template as<T>();
    }

    // For assignment: b = a.value()
    StridedValueRead<Stride> value(void) const
    {
        return StridedValueRead<Stride>(getType(), m_value);
    }

protected:
    const VariableType *m_type;
    Scalar
        *m_value; // \note Non-const internal pointer is used so that ValueAccess can extend this class with RW access
};

template <int Stride>
class StridedValueAccess : public ConstStridedValueAccess<Stride>
{
public:
    StridedValueAccess(void)
    {
    }
    StridedValueAccess(const VariableType &type, Scalar *valuePtr) : ConstStridedValueAccess<Stride>(type, valuePtr)
    {
    }

    // Read-write access
    StridedValueAccess component(int compNdx)
    {
        return StridedValueAccess(this->getType().getElementType(), this->m_value + Stride * compNdx);
    }
    StridedValueAccess arrayElement(int elementNdx)
    {
        return StridedValueAccess(this->getType().getElementType(),
                                  this->m_value + Stride * this->getType().getElementScalarOffset(elementNdx));
    }
    StridedValueAccess member(int memberNdx)
    {
        return StridedValueAccess(this->getType().getMembers()[memberNdx].getType(),
                                  this->m_value + Stride * this->getType().getMemberScalarOffset(memberNdx));
    }

    float &asFloat(void)
    {
        DE_STATIC_ASSERT(Stride == 1);
        return this->m_value->floatVal;
    }
    int &asInt(void)
    {
        DE_STATIC_ASSERT(Stride == 1);
        return this->m_value->intVal;
    }
    bool &asBool(void)
    {
        DE_STATIC_ASSERT(Stride == 1);
        return this->m_value->boolVal;
    }
    Scalar &asScalar(void)
    {
        DE_STATIC_ASSERT(Stride == 1);
        return *this->m_value;
    }

    float &asFloat(int ndx)
    {
        DE_ASSERT(de::inBounds(ndx, 0, Stride));
        return this->m_value[ndx].floatVal;
    }
    int &asInt(int ndx)
    {
        DE_ASSERT(de::inBounds(ndx, 0, Stride));
        return this->m_value[ndx].intVal;
    }
    bool &asBool(int ndx)
    {
        DE_ASSERT(de::inBounds(ndx, 0, Stride));
        return this->m_value[ndx].boolVal;
    }
    Scalar &asScalar(int ndx)
    {
        DE_ASSERT(de::inBounds(ndx, 0, Stride));
        return this->m_value[ndx];
    }

    template <typename T>
    T &as(int ndx)
    {
        DE_ASSERT(de::inBounds(ndx, 0, Stride));
        return this->m_value[ndx].template as<T>();
    }

    template <int SrcStride>
    StridedValueAccess &operator=(const StridedValueRead<SrcStride> &value);

    // Helpers, work only in Stride == 1 case
    template <int Size>
    StridedValueAccess &operator=(const tcu::Vector<float, Size> &vec);
    StridedValueAccess &operator=(float floatVal)
    {
        asFloat() = floatVal;
        return *this;
    }
    StridedValueAccess &operator=(int intVal)
    {
        asInt() = intVal;
        return *this;
    }
    StridedValueAccess &operator=(bool boolVal)
    {
        asBool() = boolVal;
        return *this;
    }
    StridedValueAccess &operator=(Scalar val)
    {
        asScalar() = val;
        return *this;
    }
};

template <int Stride>
template <int SrcStride>
StridedValueAccess<Stride> &StridedValueAccess<Stride>::operator=(const StridedValueRead<SrcStride> &valueRead)
{
    DE_STATIC_ASSERT(SrcStride == Stride || SrcStride == 1);
    DE_ASSERT(this->getType() == valueRead.getType());

    int scalarSize = this->getType().getScalarSize();

    if (scalarSize == 0)
        return *this; // Happens when void value range is copied

    if (Stride == SrcStride)
        std::copy(valueRead.getValuePtr(), valueRead.getValuePtr() + scalarSize * Stride, this->m_value);
    else
    {
        for (int scalarNdx = 0; scalarNdx < scalarSize; scalarNdx++)
            std::fill(this->m_value + scalarNdx * Stride, this->m_value + (scalarNdx + 1) * Stride,
                      valueRead.getValuePtr()[scalarNdx]);
    }

    return *this;
}

template <int Stride>
template <int Size>
StridedValueAccess<Stride> &StridedValueAccess<Stride>::operator=(const tcu::Vector<float, Size> &vec)
{
    DE_ASSERT(this->getType() == VariableType(VariableType::TYPE_FLOAT, Size));
    for (int comp = 0; comp < 4; comp++)
        component(comp).asFloat() = vec.getPtr()[comp];

    return *this;
}

// Typedefs for stride == 1 case
typedef ConstStridedValueAccess<1> ConstValueAccess;
typedef StridedValueAccess<1> ValueAccess;

class ConstValueRangeAccess
{
public:
    ConstValueRangeAccess(void) : m_type(nullptr), m_min(nullptr), m_max(nullptr)
    {
    }
    ConstValueRangeAccess(const VariableType &type, const Scalar *minVal, const Scalar *maxVal)
        : m_type(&type)
        , m_min(const_cast<Scalar *>(minVal))
        , m_max(const_cast<Scalar *>(maxVal))
    {
    }

    const VariableType &getType(void) const
    {
        return *m_type;
    }
    ConstValueAccess getMin(void) const
    {
        return ConstValueAccess(*m_type, m_min);
    }
    ConstValueAccess getMax(void) const
    {
        return ConstValueAccess(*m_type, m_max);
    }

    // Read-only access
    ConstValueRangeAccess component(int compNdx) const;
    ConstValueRangeAccess arrayElement(int elementNdx) const;
    ConstValueRangeAccess member(int memberNdx) const;

    // Set operations - tests condition for all elements
    bool intersects(const ConstValueRangeAccess &other) const;
    bool isSupersetOf(const ConstValueRangeAccess &other) const;
    bool isSubsetOf(const ConstValueRangeAccess &other) const;

protected:
    const VariableType *m_type;
    Scalar *m_min; // \note See note in ConstValueAccess
    Scalar *m_max;
};

inline ConstValueRangeAccess ConstValueRangeAccess::component(int compNdx) const
{
    return ConstValueRangeAccess(m_type->getElementType(), m_min + compNdx, m_max + compNdx);
}

inline ConstValueRangeAccess ConstValueRangeAccess::arrayElement(int elementNdx) const
{
    int offset = m_type->getElementScalarOffset(elementNdx);
    return ConstValueRangeAccess(m_type->getElementType(), m_min + offset, m_max + offset);
}

inline ConstValueRangeAccess ConstValueRangeAccess::member(int memberNdx) const
{
    int offset = m_type->getMemberScalarOffset(memberNdx);
    return ConstValueRangeAccess(m_type->getMembers()[memberNdx].getType(), m_min + offset, m_max + offset);
}

class ValueRangeAccess : public ConstValueRangeAccess
{
public:
    ValueRangeAccess(const VariableType &type, Scalar *minVal, Scalar *maxVal)
        : ConstValueRangeAccess(type, minVal, maxVal)
    {
    }

    // Read-write access
    ValueAccess getMin(void)
    {
        return ValueAccess(*m_type, m_min);
    }
    ValueAccess getMax(void)
    {
        return ValueAccess(*m_type, m_max);
    }

    ValueRangeAccess component(int compNdx);
    ValueRangeAccess arrayElement(int elementNdx);
    ValueRangeAccess member(int memberNdx);
};

inline ValueRangeAccess ValueRangeAccess::component(int compNdx)
{
    return ValueRangeAccess(m_type->getElementType(), m_min + compNdx, m_max + compNdx);
}

inline ValueRangeAccess ValueRangeAccess::arrayElement(int elementNdx)
{
    int offset = m_type->getElementScalarOffset(elementNdx);
    return ValueRangeAccess(m_type->getElementType(), m_min + offset, m_max + offset);
}

inline ValueRangeAccess ValueRangeAccess::member(int memberNdx)
{
    int offset = m_type->getMemberScalarOffset(memberNdx);
    return ValueRangeAccess(m_type->getMembers()[memberNdx].getType(), m_min + offset, m_max + offset);
}

class ValueRange
{
public:
    ValueRange(const VariableType &type);
    ValueRange(const VariableType &type, const ConstValueAccess &minVal, const ConstValueAccess &maxVal);
    ValueRange(const VariableType &type, const Scalar *minVal, const Scalar *maxVal);
    ValueRange(ConstValueRangeAccess other);
    ~ValueRange(void);

    const VariableType &getType(void) const
    {
        return m_type;
    }

    ValueAccess getMin(void)
    {
        return ValueAccess(m_type, getMinPtr());
    }
    ValueAccess getMax(void)
    {
        return ValueAccess(m_type, getMaxPtr());
    }

    ConstValueAccess getMin(void) const
    {
        return ConstValueAccess(m_type, getMinPtr());
    }
    ConstValueAccess getMax(void) const
    {
        return ConstValueAccess(m_type, getMaxPtr());
    }

    ValueRangeAccess asAccess(void)
    {
        return ValueRangeAccess(m_type, getMinPtr(), getMaxPtr());
    }
    ConstValueRangeAccess asAccess(void) const
    {
        return ConstValueRangeAccess(m_type, getMinPtr(), getMaxPtr());
    }

    operator ConstValueRangeAccess(void) const
    {
        return asAccess();
    }
    operator ValueRangeAccess(void)
    {
        return asAccess();
    }

    static void computeIntersection(ValueRangeAccess dst, const ConstValueRangeAccess &a,
                                    const ConstValueRangeAccess &b);
    static void computeIntersection(ValueRange &dst, const ConstValueRangeAccess &a, const ConstValueRangeAccess &b);

private:
    const Scalar *getMinPtr(void) const
    {
        return m_min.empty() ? nullptr : &m_min[0];
    }
    const Scalar *getMaxPtr(void) const
    {
        return m_max.empty() ? nullptr : &m_max[0];
    }

    Scalar *getMinPtr(void)
    {
        return m_min.empty() ? nullptr : &m_min[0];
    }
    Scalar *getMaxPtr(void)
    {
        return m_max.empty() ? nullptr : &m_max[0];
    }

    VariableType m_type;
    std::vector<Scalar> m_min;
    std::vector<Scalar> m_max;
};

template <int Stride>
class ValueStorage
{
public:
    ValueStorage(void);
    ValueStorage(const VariableType &type);

    void setStorage(const VariableType &type);

    StridedValueAccess<Stride> getValue(const VariableType &type)
    {
        return StridedValueAccess<Stride>(type, &m_value[0]);
    }
    ConstStridedValueAccess<Stride> getValue(const VariableType &type) const
    {
        return ConstStridedValueAccess<Stride>(type, &m_value[0]);
    }

private:
    ValueStorage(const ValueStorage &other);
    ValueStorage operator=(const ValueStorage &other);

    std::vector<Scalar> m_value;
};

template <int Stride>
ValueStorage<Stride>::ValueStorage(void)
{
}

template <int Stride>
ValueStorage<Stride>::ValueStorage(const VariableType &type)
{
    setStorage(type);
}

template <int Stride>
void ValueStorage<Stride>::setStorage(const VariableType &type)
{
    m_value.resize(type.getScalarSize() * Stride);
}

class VariableValue
{
public:
    VariableValue(const Variable *variable) : m_variable(variable), m_storage(m_variable->getType())
    {
    }
    ~VariableValue(void)
    {
    }

    const Variable *getVariable(void) const
    {
        return m_variable;
    }
    ValueAccess getValue(void)
    {
        return m_storage.getValue(m_variable->getType());
    }
    ConstValueAccess getValue(void) const
    {
        return m_storage.getValue(m_variable->getType());
    }

    VariableValue(const VariableValue &other);
    VariableValue &operator=(const VariableValue &other);

private:
    const VariableType &getType(void) const
    {
        return m_variable->getType();
    }

    const Variable *m_variable;
    ValueStorage<1> m_storage;
};

} // namespace rsg

#endif // _RSGVARIABLEVALUE_HPP
