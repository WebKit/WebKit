#ifndef _TCUVECTOR_HPP
#define _TCUVECTOR_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
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
 * \brief Generic vector template.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuVectorType.hpp"
#include "deInt32.h"

#include <ostream>

namespace tcu
{

// Accessor proxy class for Vectors.
template <typename T, int VecSize, int Size>
class VecAccess
{
public:
    explicit VecAccess(Vector<T, VecSize> &v, int x, int y);
    explicit VecAccess(Vector<T, VecSize> &v, int x, int y, int z);
    explicit VecAccess(Vector<T, VecSize> &v, int x, int y, int z, int w);

    VecAccess &operator=(const Vector<T, Size> &v);

    operator Vector<T, Size>(void) const;

private:
    Vector<T, VecSize> &m_vector;
    int m_index[Size];
};

template <typename T, int VecSize, int Size>
VecAccess<T, VecSize, Size>::VecAccess(Vector<T, VecSize> &v, int x, int y) : m_vector(v)
{
    DE_STATIC_ASSERT(Size == 2);
    m_index[0] = x;
    m_index[1] = y;
}

template <typename T, int VecSize, int Size>
VecAccess<T, VecSize, Size>::VecAccess(Vector<T, VecSize> &v, int x, int y, int z) : m_vector(v)
{
    DE_STATIC_ASSERT(Size == 3);
    m_index[0] = x;
    m_index[1] = y;
    m_index[2] = z;
}

template <typename T, int VecSize, int Size>
VecAccess<T, VecSize, Size>::VecAccess(Vector<T, VecSize> &v, int x, int y, int z, int w) : m_vector(v)
{
    DE_STATIC_ASSERT(Size == 4);
    m_index[0] = x;
    m_index[1] = y;
    m_index[2] = z;
    m_index[3] = w;
}

template <typename T, int VecSize, int Size>
VecAccess<T, VecSize, Size> &VecAccess<T, VecSize, Size>::operator=(const Vector<T, Size> &v)
{
    for (int i = 0; i < Size; i++)
        m_vector.m_data[m_index[i]] = v.m_data[i];
    return *this;
}

// Vector class.
template <typename T, int Size>
class Vector
{
public:
    typedef T Element;
    enum
    {
        SIZE = Size,
    };

    T m_data[Size];

    // Constructors.
    explicit Vector(void);
    explicit Vector(T s_); // replicate
    Vector(T x_, T y_);
    Vector(T x_, T y_, T z_);
    Vector(T x_, T y_, T z_, T w_);
    Vector(T x_, T y_, T z_, T w_, T v_);
    Vector(const T (&v)[Size]);

    const T *getPtr(void) const
    {
        return &m_data[0];
    }
    T *getPtr(void)
    {
        return &m_data[0];
    }

    // Read-only access.
    T x(void) const
    {
        return m_data[0];
    }
    T y(void) const
    {
        DE_STATIC_ASSERT(Size >= 2);
        return m_data[1];
    }
    T z(void) const
    {
        DE_STATIC_ASSERT(Size >= 3);
        return m_data[2];
    }
    T w(void) const
    {
        DE_STATIC_ASSERT(Size >= 4);
        return m_data[3];
    }

    // Read-write access.
    T &x(void)
    {
        return m_data[0];
    }
    T &y(void)
    {
        DE_STATIC_ASSERT(Size >= 2);
        return m_data[1];
    }
    T &z(void)
    {
        DE_STATIC_ASSERT(Size >= 3);
        return m_data[2];
    }
    T &w(void)
    {
        DE_STATIC_ASSERT(Size >= 4);
        return m_data[3];
    }

    // Writable accessors.
    VecAccess<T, Size, 2> xy(void)
    {
        DE_ASSERT(Size >= 2);
        return VecAccess<T, Size, 2>(*this, 0, 1);
    }
    VecAccess<T, Size, 2> xz(void)
    {
        DE_ASSERT(Size >= 2);
        return VecAccess<T, Size, 2>(*this, 0, 2);
    }
    VecAccess<T, Size, 2> xw(void)
    {
        DE_ASSERT(Size >= 2);
        return VecAccess<T, Size, 2>(*this, 0, 3);
    }
    VecAccess<T, Size, 2> yz(void)
    {
        DE_ASSERT(Size >= 2);
        return VecAccess<T, Size, 2>(*this, 1, 2);
    }
    VecAccess<T, Size, 2> yw(void)
    {
        DE_ASSERT(Size >= 2);
        return VecAccess<T, Size, 2>(*this, 1, 3);
    }
    VecAccess<T, Size, 2> zw(void)
    {
        DE_ASSERT(Size >= 2);
        return VecAccess<T, Size, 2>(*this, 2, 3);
    }
    VecAccess<T, Size, 3> xyz(void)
    {
        DE_ASSERT(Size >= 3);
        return VecAccess<T, Size, 3>(*this, 0, 1, 2);
    }
    VecAccess<T, Size, 3> xyw(void)
    {
        DE_ASSERT(Size >= 3);
        return VecAccess<T, Size, 3>(*this, 0, 1, 3);
    }
    VecAccess<T, Size, 3> xzw(void)
    {
        DE_ASSERT(Size >= 3);
        return VecAccess<T, Size, 3>(*this, 0, 2, 3);
    }
    VecAccess<T, Size, 3> zyx(void)
    {
        DE_ASSERT(Size >= 3);
        return VecAccess<T, Size, 3>(*this, 2, 1, 0);
    }
    VecAccess<T, Size, 3> yzw(void)
    {
        DE_ASSERT(Size >= 3);
        return VecAccess<T, Size, 3>(*this, 1, 2, 3);
    }
    VecAccess<T, Size, 3> wzy(void)
    {
        DE_ASSERT(Size >= 3);
        return VecAccess<T, Size, 3>(*this, 3, 2, 1);
    }
    VecAccess<T, Size, 4> xyzw(void)
    {
        DE_ASSERT(Size >= 4);
        return VecAccess<T, Size, 4>(*this, 0, 1, 2, 3);
    }

    // Swizzles.
    Vector<T, 1> swizzle(int a) const
    {
        DE_ASSERT(a >= 0 && a < Size);
        return Vector<T, 1>(m_data[a]);
    }
    Vector<T, 2> swizzle(int a, int b) const
    {
        DE_ASSERT(a >= 0 && a < Size);
        DE_ASSERT(b >= 0 && b < Size);
        return Vector<T, 2>(m_data[a], m_data[b]);
    }
    Vector<T, 3> swizzle(int a, int b, int c) const
    {
        DE_ASSERT(a >= 0 && a < Size);
        DE_ASSERT(b >= 0 && b < Size);
        DE_ASSERT(c >= 0 && c < Size);
        return Vector<T, 3>(m_data[a], m_data[b], m_data[c]);
    }
    Vector<T, 4> swizzle(int a, int b, int c, int d) const
    {
        DE_ASSERT(a >= 0 && a < Size);
        DE_ASSERT(b >= 0 && b < Size);
        DE_ASSERT(c >= 0 && c < Size);
        DE_ASSERT(d >= 0 && d < Size);
        return Vector<T, 4>(m_data[a], m_data[b], m_data[c], m_data[d]);
    }

    Vector<float, Size> asFloat(void) const
    {
        return cast<float>();
    }
    Vector<int, Size> asInt(void) const
    {
        return cast<int>();
    }
    Vector<uint32_t, Size> asUint(void) const
    {
        return cast<uint32_t>();
    }
    Vector<bool, Size> asBool(void) const
    {
        return cast<bool>();
    }

    // Operators.
    Vector<T, Size> &operator+=(const Vector<T, Size> &v);
    Vector<T, Size> &operator-=(const Vector<T, Size> &v);

    const T &operator[](int ndx) const
    {
        DE_ASSERT(de::inBounds(ndx, 0, Size));
        return m_data[ndx];
    }
    T &operator[](int ndx)
    {
        DE_ASSERT(de::inBounds(ndx, 0, Size));
        return m_data[ndx];
    }

    bool operator==(const Vector<T, Size> &v) const
    {
        for (int i = 0; i < Size; i++)
            if (m_data[i] != v.m_data[i])
                return false;
        return true;
    }
    bool operator!=(const Vector<T, Size> &v) const
    {
        return !(*this == v);
    }

    // Miscellaneous conversions.
    template <typename NewT>
    Vector<NewT, Size> cast(void) const;

    template <typename NewT>
    Vector<NewT, Size> bitCast(void) const;

    template <int NewSize>
    Vector<T, NewSize> toWidth(void) const;
} DE_WARN_UNUSED_TYPE;

template <typename T, int Size>
inline Vector<T, Size>::Vector(void)
{
    for (int i = 0; i < Size; i++)
        m_data[i] = T();
}

template <typename T, int Size>
inline Vector<T, Size>::Vector(T s)
{
    for (int i = 0; i < Size; i++)
        m_data[i] = s;
}

template <typename T, int Size>
inline Vector<T, Size>::Vector(T x_, T y_)
{
    DE_STATIC_ASSERT(Size == 2);
    m_data[0] = x_;
    m_data[1] = y_;
}

template <typename T, int Size>
inline Vector<T, Size>::Vector(T x_, T y_, T z_)
{
    DE_STATIC_ASSERT(Size == 3);
    m_data[0] = x_;
    m_data[1] = y_;
    m_data[2] = z_;
}

template <typename T, int Size>
inline Vector<T, Size>::Vector(T x_, T y_, T z_, T w_)
{
    DE_STATIC_ASSERT(Size == 4);
    m_data[0] = x_;
    m_data[1] = y_;
    m_data[2] = z_;
    m_data[3] = w_;
}

template <typename T, int Size>
inline Vector<T, Size>::Vector(T x_, T y_, T z_, T w_, T v_)
{
    DE_STATIC_ASSERT(Size == 5);
    m_data[0] = x_;
    m_data[1] = y_;
    m_data[2] = z_;
    m_data[3] = w_;
    m_data[4] = v_;
}

template <typename T, int Size>
inline Vector<T, Size>::Vector(const T (&v)[Size])
{
    for (int i = 0; i < Size; i++)
        m_data[i] = v[i];
}

// VecAccess to Vector cast.
template <typename T, int VecSize, int Size>
VecAccess<T, VecSize, Size>::operator Vector<T, Size>(void) const
{
    Vector<T, Size> vec;
    for (int i = 0; i < Size; i++)
        vec.m_data[i] = m_vector.m_data[m_index[i]];
    return vec;
}

// Type cast.
template <typename T, int Size>
template <typename NewT>
inline Vector<NewT, Size> Vector<T, Size>::cast(void) const
{
    Vector<NewT, Size> res;
    for (int i = 0; i < Size; i++)
        res.m_data[i] = NewT(m_data[i]);
    return res;
}

// Bit-exact reinterpret cast.
template <typename T, int Size>
template <typename NewT>
inline Vector<NewT, Size> Vector<T, Size>::bitCast(void) const
{
    Vector<NewT, Size> res;
    DE_STATIC_ASSERT(sizeof(res.m_data) == sizeof(m_data));
    memcpy(res.m_data, m_data, sizeof(m_data));
    return res;
}

// Size cast.
template <typename T, int Size>
template <int NewSize>
inline Vector<T, NewSize> Vector<T, Size>::toWidth(void) const
{
    Vector<T, NewSize> res;
    int i;
    for (i = 0; i < deMin32(Size, NewSize); i++)
        res.m_data[i] = m_data[i];
    for (; i < NewSize; i++)
        res.m_data[i] = T(0);
    return res;
}

// Operators.

template <typename T, int Size>
inline Vector<T, Size> operator-(const Vector<T, Size> &a)
{
    Vector<T, Size> res;
    for (int i = 0; i < Size; i++)
        res.m_data[i] = -a.m_data[i];
    return res;
}

template <typename T, int Size>
inline Vector<T, Size> operator+(const Vector<T, Size> &a, const Vector<T, Size> &b)
{
    Vector<T, Size> res;
    for (int i = 0; i < Size; i++)
        res.m_data[i] = a.m_data[i] + b.m_data[i];
    return res;
}

template <typename T, int Size>
inline Vector<T, Size> operator-(const Vector<T, Size> &a, const Vector<T, Size> &b)
{
    Vector<T, Size> res;
    for (int i = 0; i < Size; i++)
        res.m_data[i] = a.m_data[i] - b.m_data[i];
    return res;
}

template <typename T, int Size>
inline Vector<T, Size> operator*(const Vector<T, Size> &a, const Vector<T, Size> &b)
{
    Vector<T, Size> res;
    for (int i = 0; i < Size; i++)
        res.m_data[i] = a.m_data[i] * b.m_data[i];
    return res;
}

template <typename T, int Size>
inline Vector<T, Size> operator/(const Vector<T, Size> &a, const Vector<T, Size> &b)
{
    Vector<T, Size> res;
    for (int i = 0; i < Size; i++)
        res.m_data[i] = a.m_data[i] / b.m_data[i];
    return res;
}

template <typename T, int Size>
inline Vector<T, Size> operator<<(const Vector<T, Size> &a, const Vector<T, Size> &b)
{
    Vector<T, Size> res;
    for (int i = 0; i < Size; i++)
        res.m_data[i] = a.m_data[i] << b.m_data[i];
    return res;
}

template <typename T, int Size>
inline Vector<T, Size> operator>>(const Vector<T, Size> &a, const Vector<T, Size> &b)
{
    Vector<T, Size> res;
    for (int i = 0; i < Size; i++)
        res.m_data[i] = a.m_data[i] >> b.m_data[i];
    return res;
}

template <typename T, int Size>
inline Vector<T, Size> operator*(T s, const Vector<T, Size> &a)
{
    Vector<T, Size> res;
    for (int i = 0; i < Size; i++)
        res.m_data[i] = s * a.m_data[i];
    return res;
}

template <typename T, int Size>
inline Vector<T, Size> operator+(T s, const Vector<T, Size> &a)
{
    Vector<T, Size> res;
    for (int i = 0; i < Size; i++)
        res.m_data[i] = s + a.m_data[i];
    return res;
}

template <typename T, int Size>
inline Vector<T, Size> operator-(T s, const Vector<T, Size> &a)
{
    Vector<T, Size> res;
    for (int i = 0; i < Size; i++)
        res.m_data[i] = s - a.m_data[i];
    return res;
}

template <typename T, int Size>
inline Vector<T, Size> operator-(const Vector<T, Size> &a, T s)
{
    Vector<T, Size> res;
    for (int i = 0; i < Size; i++)
        res.m_data[i] = a.m_data[i] - s;
    return res;
}

template <typename T, int Size>
inline Vector<T, Size> operator/(T s, const Vector<T, Size> &a)
{
    Vector<T, Size> res;
    for (int i = 0; i < Size; i++)
        res.m_data[i] = s / a.m_data[i];
    return res;
}

template <typename T, int Size>
inline Vector<T, Size> operator*(const Vector<T, Size> &a, T s)
{
    return s * a;
}

template <typename T, int Size>
inline Vector<T, Size> operator+(const Vector<T, Size> &a, T s)
{
    return s + a;
}

template <typename T, int Size>
inline Vector<T, Size> operator/(const Vector<T, Size> &a, T s)
{
    Vector<T, Size> res;
    for (int i = 0; i < Size; i++)
        res.m_data[i] = a.m_data[i] / s;
    return res;
}

template <typename T, int Size>
inline Vector<T, Size> &Vector<T, Size>::operator+=(const Vector<T, Size> &v)
{
    for (int i = 0; i < Size; i++)
        m_data[i] += v.m_data[i];
    return *this;
}

template <typename T, int Size>
inline Vector<T, Size> &Vector<T, Size>::operator-=(const Vector<T, Size> &v)
{
    for (int i = 0; i < Size; i++)
        m_data[i] -= v.m_data[i];
    return *this;
}

// Stream operator.
template <typename T, int Size>
std::ostream &operator<<(std::ostream &stream, const tcu::Vector<T, Size> &vec)
{
    stream << "(";
    for (int i = 0; i < Size; i++)
    {
        if (i != 0)
            stream << ", ";
        stream << vec.m_data[i];
    }
    stream << ")";
    return stream;
}

} // namespace tcu

#endif // _TCUVECTOR_HPP
