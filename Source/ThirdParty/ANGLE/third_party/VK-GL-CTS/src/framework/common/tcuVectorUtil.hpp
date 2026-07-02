#ifndef _TCUVECTORUTIL_HPP
#define _TCUVECTORUTIL_HPP
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
 * \brief Vector utility functions.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuVector.hpp"
#include "deRandom.hpp"
#include "deMeta.hpp"
#include "deMath.h"
#include "deInt32.h"

#include <ostream>
#include <math.h>

namespace tcu
{

static const float PI = 3.141592653589793238f;

#if (DE_OS == DE_OS_ANDROID) || (DE_OS == DE_OS_UNIX) || (DE_OS == DE_OS_QNX) || \
    (DE_OS == DE_OS_WIN32 && DE_COMPILER == DE_COMPILER_CLANG)
inline float abs(float f)
{
    return deFloatAbs(f);
}
#endif

template <typename T>
inline T add(T a, T b)
{
    return a + b;
}
template <typename T>
inline T sub(T a, T b)
{
    return a - b;
}
template <typename T>
inline T mul(T a, T b)
{
    return a * b;
}
template <typename T>
inline T div(T a, T b)
{
    return a / b;
}

template <typename T>
inline T bitwiseNot(T a)
{
    return ~a;
}
template <typename T>
inline T bitwiseAnd(T a, T b)
{
    return a & b;
}
template <typename T>
inline T bitwiseOr(T a, T b)
{
    return a | b;
}
template <typename T>
inline T bitwiseXor(T a, T b)
{
    return a ^ b;
}

template <typename T>
inline T logicalNot(T a)
{
    return !a;
}
template <typename T>
inline T logicalAnd(T a, T b)
{
    return a && b;
}
template <typename T>
inline T logicalOr(T a, T b)
{
    return a || b;
}

template <typename T>
inline T mod(T a, T b)
{
    return a % b;
}
template <>
inline float mod(float x, float y)
{
    return x - y * deFloatFloor(x / y);
}

template <typename T>
inline T negate(T f)
{
    return -f;
}
template <>
inline uint32_t negate<uint32_t>(uint32_t f)
{
    return (uint32_t) - (int)f;
}

inline float radians(float f)
{
    return deFloatRadians(f);
}
inline float degrees(float f)
{
    return deFloatDegrees(f);
}
inline float inverseSqrt(float f)
{
    return deFloatRsq(f);
}
inline float sign(float f)
{
    return (f < 0.0f) ? -1.0f : ((f > 0.0f) ? +1.0f : 0.0f);
}
inline float fract(float f)
{
    return f - deFloatFloor(f);
}
inline float mix(float x, float y, float a)
{
    return x * (1.0f - a) + y * a;
}
inline float step(float edge, float x)
{
    return (x < edge) ? 0.0f : 1.0f;
}
inline float smoothStep(float edge0, float edge1, float x)
{
    if (x <= edge0)
        return 0.0f;
    if (x >= edge1)
        return 1.0f;
    float t = de::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline double mix(double x, double y, double a)
{
    return x * (1.0 - a) + y * a;
}
inline double step(double edge, double x)
{
    return (x < edge) ? 0.0 : 1.0;
}

inline float length(float f)
{
    return deFloatAbs(f);
}
inline float distance(float x, float y)
{
    return deFloatAbs(x - y);
}
inline float dot(float x, float y)
{
    return (x * y);
}

inline float normalize(float f)
{
    return sign(f);
}
inline float faceForward(float n, float i, float ref)
{
    return ((ref * i) < 0.0f) ? n : -n;
}
inline float reflect(float i, float n)
{
    return i - 2.0f * (n * i) * n;
}
inline float refract(float i, float n, float eta)
{
    float cosAngle = (n * i);
    float k        = 1.0f - eta * eta * (1.0f - cosAngle * cosAngle);
    if (k < 0.0f)
        return 0.0f;
    else
        return eta * i - (eta * cosAngle + deFloatSqrt(k)) * n;
}

template <typename T>
inline bool lessThan(T a, T b)
{
    return (a < b);
}
template <typename T>
inline bool lessThanEqual(T a, T b)
{
    return (a <= b);
}
template <typename T>
inline bool greaterThan(T a, T b)
{
    return (a > b);
}
template <typename T>
inline bool greaterThanEqual(T a, T b)
{
    return (a >= b);
}
template <typename T>
inline bool equal(T a, T b)
{
    return (a == b);
}
template <typename T>
inline bool notEqual(T a, T b)
{
    return (a != b);
}
template <typename T>
inline bool allEqual(T a, T b)
{
    return (a == b);
}
template <typename T>
inline bool anyNotEqual(T a, T b)
{
    return (a != b);
}

inline bool boolNot(bool a)
{
    return !a;
}

inline int chopToInt(float a)
{
    return deChopFloatToInt32(a);
}

inline float roundToEven(float a)
{
    float q = deFloatFrac(a);
    float r = a - q;

    if (q > 0.5f)
        r += 1.0f;
    else if (q == 0.5 && (((int)r) % 2 != 0))
        r += 1.0f;

    return r;
}

template <typename T, int Size>
inline T dot(const Vector<T, Size> &a, const Vector<T, Size> &b)
{
    T res = T();
    for (int i = 0; i < Size; i++)
        res += a.m_data[i] * b.m_data[i];
    return res;
}

template <typename T, int Size>
inline T lengthSquared(const Vector<T, Size> &a)
{
    T sqSum = T();
    for (int i = 0; i < Size; i++)
        sqSum += a.m_data[i] * a.m_data[i];
    return sqSum;
}

template <typename T, int Size>
inline typename de::meta::EnableIf<T, de::meta::TypesSame<T, double>::Value>::Type length(const Vector<T, Size> &a)
{
    return ::sqrt(lengthSquared(a));
}

template <typename T, int Size>
inline typename de::meta::EnableIf<T, de::meta::TypesSame<T, float>::Value>::Type length(const Vector<T, Size> &a)
{
    return deFloatSqrt(lengthSquared(a));
}

template <typename T, int Size>
inline T distance(const Vector<T, Size> &a, const Vector<T, Size> &b)
{
    return length(a - b);
}

template <typename T, int Size>
inline Vector<T, Size> cross(const Vector<T, Size> &a, const Vector<T, Size> &b)
{
    DE_STATIC_ASSERT(Size == 3);
    return Vector<T, Size>(a.y() * b.z() - b.y() * a.z(), a.z() * b.x() - b.z() * a.x(), a.x() * b.y() - b.x() * a.y());
}

template <typename T, int Size>
inline Vector<T, Size> normalize(const Vector<T, Size> &a)
{
    T ooLen = T(1) / length(a);
    Vector<T, Size> res;
    for (int i = 0; i < Size; i++)
        res.m_data[i] = ooLen * a.m_data[i];
    return res;
}

template <typename T, int Size>
inline Vector<T, Size> faceForward(const Vector<T, Size> &n, const Vector<T, Size> &i, const Vector<T, Size> &ref)
{
    return (dot(ref, i) < T(0)) ? n : -n;
}

template <typename T, int Size>
inline Vector<T, Size> reflect(const Vector<T, Size> &i, const Vector<T, Size> &n)
{
    return i - T(2) * dot(n, i) * n;
}

template <typename T, int Size>
inline Vector<T, Size> refract(const Vector<T, Size> &i, const Vector<T, Size> &n, T eta)
{
    T cosAngle = dot(n, i);
    T k        = T(1) - eta * eta * (T(1) - cosAngle * cosAngle);
    if (k < T(0))
        return Vector<T, Size>(T(0));
    else
        return i * eta - n * T(eta * cosAngle + ::sqrt(k));
}

template <int Size>
Vector<float, Size> mix(const Vector<float, Size> &x, const Vector<float, Size> &y, float a)
{
    Vector<float, Size> res;
    for (int i = 0; i < Size; i++)
        res.m_data[i] = deFloatMix(x.m_data[i], y.m_data[i], a);
    return res;
}

template <int Size>
Vector<double, Size> mix(const Vector<double, Size> &x, const Vector<double, Size> &y, double a)
{
    Vector<double, Size> res;
    for (int i = 0; i < Size; i++)
        res.m_data[i] = deMix(x.m_data[i], y.m_data[i], a);
    return res;
}

// Piece-wise compare operators.

template <typename T, int Size>
inline Vector<bool, Size> equal(const Vector<T, Size> &a, const Vector<T, Size> &b)
{
    Vector<bool, Size> res;
    for (int i = 0; i < Size; i++)
        res.m_data[i] = a.m_data[i] == b.m_data[i];
    return res;
}

template <typename T, int Size>
inline Vector<bool, Size> notEqual(const Vector<T, Size> &a, const Vector<T, Size> &b)
{
    Vector<bool, Size> res;
    for (int i = 0; i < Size; i++)
        res.m_data[i] = a.m_data[i] != b.m_data[i];
    return res;
}

template <typename T, int Size>
inline Vector<bool, Size> lessThan(const Vector<T, Size> &a, const Vector<T, Size> &b)
{
    Vector<bool, Size> res;
    for (int i = 0; i < Size; i++)
        res.m_data[i] = a.m_data[i] < b.m_data[i];
    return res;
}

template <typename T, int Size>
inline Vector<bool, Size> lessThanEqual(const Vector<T, Size> &a, const Vector<T, Size> &b)
{
    Vector<bool, Size> res;
    for (int i = 0; i < Size; i++)
        res.m_data[i] = a.m_data[i] <= b.m_data[i];
    return res;
}

template <typename T, int Size>
inline Vector<bool, Size> greaterThan(const Vector<T, Size> &a, const Vector<T, Size> &b)
{
    Vector<bool, Size> res;
    for (int i = 0; i < Size; i++)
        res.m_data[i] = a.m_data[i] > b.m_data[i];
    return res;
}

template <typename T, int Size>
inline Vector<bool, Size> greaterThanEqual(const Vector<T, Size> &a, const Vector<T, Size> &b)
{
    Vector<bool, Size> res;
    for (int i = 0; i < Size; i++)
        res.m_data[i] = a.m_data[i] >= b.m_data[i];
    return res;
}

// Equality comparison operators.

template <typename T, int Size>
inline bool allEqual(const Vector<T, Size> &a, const Vector<T, Size> &b)
{
    bool res = true;
    for (int i = 0; i < Size; i++)
        res = res && a.m_data[i] == b.m_data[i];
    return res;
}

template <typename T, int Size>
inline bool anyNotEqual(const Vector<T, Size> &a, const Vector<T, Size> &b)
{
    bool res = false;
    for (int i = 0; i < Size; i++)
        res = res || a.m_data[i] != b.m_data[i];
    return res;
}

// Boolean built-ins.

template <int Size>
inline Vector<bool, Size> boolNot(const Vector<bool, Size> &a)
{
    Vector<bool, Size> res;
    for (int i = 0; i < Size; i++)
        res.m_data[i] = !a.m_data[i];
    return res;
}

template <int Size>
inline bool boolAny(const Vector<bool, Size> &a)
{
    for (int i = 0; i < Size; i++)
        if (a.m_data[i] == true)
            return true;
    return false;
}

template <int Size>
inline bool boolAll(const Vector<bool, Size> &a)
{
    for (int i = 0; i < Size; i++)
        if (a.m_data[i] == false)
            return false;
    return true;
}

template <int Size>
Vector<int, Size> chopToInt(const Vector<float, Size> &v)
{
    Vector<int, Size> res;
    for (int i = 0; i < Size; i++)
        res.m_data[i] = chopToInt(v.m_data[i]);
    return res;
}

// Vector construction using selection based on boolean vector.

template <typename T, int Size>
inline Vector<T, Size> select(T trueVal, T falseVal, const Vector<bool, Size> &cond)
{
    Vector<T, Size> res;
    for (int i = 0; i < Size; i++)
        res[i] = cond[i] ? trueVal : falseVal;
    return res;
}

// Component-wise selection.

template <typename T, int Size>
inline Vector<T, Size> select(const Vector<T, Size> &trueVal, const Vector<T, Size> &falseVal,
                              const Vector<bool, Size> &cond)
{
    Vector<T, Size> res;
    for (int i = 0; i < Size; i++)
        res[i] = cond[i] ? trueVal[i] : falseVal[i];
    return res;
}

// Absolute difference (abs(a - b))

template <typename T, int Size>
static inline Vector<T, Size> absDiff(const Vector<T, Size> &a, const Vector<T, Size> &b)
{
    Vector<T, Size> res;

    for (int ndx = 0; ndx < Size; ndx++)
        res[ndx] = (a[ndx] > b[ndx]) ? (a[ndx] - b[ndx]) : (b[ndx] - a[ndx]);

    return res;
}

template <typename T, int Size>
inline tcu::Vector<T, Size> randomVector(de::Random &rnd, const tcu::Vector<T, Size> &minValue,
                                         const tcu::Vector<T, Size> &maxValue)
{
    tcu::Vector<T, Size> res;

    for (int ndx = 0; ndx < Size; ndx++)
        res[ndx] = de::randomScalar<T>(rnd, minValue[ndx], maxValue[ndx]);

    return res;
}

inline Vector<float, 2> randomVec2(de::Random &rnd)
{
    return randomVector<float, 2>(rnd, tcu::Vector<float, 2>(0.0f), tcu::Vector<float, 2>(1.0f));
}

inline Vector<float, 3> randomVec3(de::Random &rnd)
{
    return randomVector<float, 3>(rnd, tcu::Vector<float, 3>(0.0f), tcu::Vector<float, 3>(1.0f));
}

inline Vector<float, 4> randomVec4(de::Random &rnd)
{
    return randomVector<float, 4>(rnd, tcu::Vector<float, 4>(0.0f), tcu::Vector<float, 4>(1.0f));
}

// Macros for component-wise ops.

#define TCU_DECLARE_VECTOR_UNARY_FUNC(FUNC_NAME, OP_NAME) \
    template <typename T, int Size>                       \
    Vector<T, Size> FUNC_NAME(const Vector<T, Size> &v)   \
    {                                                     \
        Vector<T, Size> res;                              \
        for (int i = 0; i < Size; i++)                    \
            res.m_data[i] = OP_NAME(v.m_data[i]);         \
        return res;                                       \
    }

#define TCU_DECLARE_VECTOR_BINARY_FUNC(FUNC_NAME, OP_NAME)                        \
    template <typename T, int Size>                                               \
    Vector<T, Size> FUNC_NAME(const Vector<T, Size> &a, const Vector<T, Size> &b) \
    {                                                                             \
        Vector<T, Size> res;                                                      \
        for (int i = 0; i < Size; i++)                                            \
            res.m_data[i] = OP_NAME(a.m_data[i], b.m_data[i]);                    \
        return res;                                                               \
    }

#define TCU_DECLARE_VECTOR_TERNARY_FUNC(FUNC_NAME, OP_NAME)                                                 \
    template <typename T, int Size>                                                                         \
    Vector<T, Size> FUNC_NAME(const Vector<T, Size> &a, const Vector<T, Size> &b, const Vector<T, Size> &c) \
    {                                                                                                       \
        Vector<T, Size> res;                                                                                \
        for (int i = 0; i < Size; i++)                                                                      \
            res.m_data[i] = OP_NAME(a.m_data[i], b.m_data[i], c.m_data[i]);                                 \
        return res;                                                                                         \
    }

// \todo [2011-07-01 pyry] Add some prefix to vector funcs and remove this hack.
#if defined(min)
#undef min
#endif
#if defined(max)
#undef max
#endif

TCU_DECLARE_VECTOR_UNARY_FUNC(negate, negate)
TCU_DECLARE_VECTOR_UNARY_FUNC(bitwiseNot, bitwiseNot)
TCU_DECLARE_VECTOR_BINARY_FUNC(add, add)
TCU_DECLARE_VECTOR_BINARY_FUNC(sub, sub)
TCU_DECLARE_VECTOR_BINARY_FUNC(mul, mul)
TCU_DECLARE_VECTOR_BINARY_FUNC(div, div)
TCU_DECLARE_VECTOR_BINARY_FUNC(mod, mod)
TCU_DECLARE_VECTOR_BINARY_FUNC(bitwiseAnd, bitwiseAnd)
TCU_DECLARE_VECTOR_BINARY_FUNC(bitwiseOr, bitwiseOr)
TCU_DECLARE_VECTOR_BINARY_FUNC(bitwiseXor, bitwiseXor)
TCU_DECLARE_VECTOR_UNARY_FUNC(logicalNot, logicalNot)
TCU_DECLARE_VECTOR_BINARY_FUNC(logicalAnd, logicalAnd)
TCU_DECLARE_VECTOR_BINARY_FUNC(logicalOr, logicalOr)

TCU_DECLARE_VECTOR_UNARY_FUNC(radians, deFloatRadians)
TCU_DECLARE_VECTOR_UNARY_FUNC(degrees, deFloatDegrees)
TCU_DECLARE_VECTOR_UNARY_FUNC(sin, deFloatSin)
TCU_DECLARE_VECTOR_UNARY_FUNC(cos, deFloatCos)
TCU_DECLARE_VECTOR_UNARY_FUNC(tan, deFloatTan)
TCU_DECLARE_VECTOR_UNARY_FUNC(asin, deFloatAsin)
TCU_DECLARE_VECTOR_UNARY_FUNC(acos, deFloatAcos)
TCU_DECLARE_VECTOR_UNARY_FUNC(atan, deFloatAtanOver)
TCU_DECLARE_VECTOR_BINARY_FUNC(atan2, deFloatAtan2)
TCU_DECLARE_VECTOR_UNARY_FUNC(sinh, deFloatSinh)
TCU_DECLARE_VECTOR_UNARY_FUNC(cosh, deFloatCosh)
TCU_DECLARE_VECTOR_UNARY_FUNC(tanh, deFloatTanh)
TCU_DECLARE_VECTOR_UNARY_FUNC(asinh, deFloatAsinh)
TCU_DECLARE_VECTOR_UNARY_FUNC(acosh, deFloatAcosh)
TCU_DECLARE_VECTOR_UNARY_FUNC(atanh, deFloatAtanh)

TCU_DECLARE_VECTOR_BINARY_FUNC(pow, deFloatPow)
TCU_DECLARE_VECTOR_UNARY_FUNC(exp, deFloatExp)
TCU_DECLARE_VECTOR_UNARY_FUNC(log, deFloatLog)
TCU_DECLARE_VECTOR_UNARY_FUNC(exp2, deFloatExp2)
TCU_DECLARE_VECTOR_UNARY_FUNC(log2, deFloatLog2)
TCU_DECLARE_VECTOR_UNARY_FUNC(sqrt, deFloatSqrt)
TCU_DECLARE_VECTOR_UNARY_FUNC(inverseSqrt, deFloatRsq)

TCU_DECLARE_VECTOR_UNARY_FUNC(abs, de::abs)
TCU_DECLARE_VECTOR_UNARY_FUNC(sign, deFloatSign)
TCU_DECLARE_VECTOR_UNARY_FUNC(floor, deFloatFloor)
TCU_DECLARE_VECTOR_UNARY_FUNC(trunc, deFloatTrunc)
TCU_DECLARE_VECTOR_UNARY_FUNC(roundToEven, roundToEven)
TCU_DECLARE_VECTOR_UNARY_FUNC(ceil, deFloatCeil)
TCU_DECLARE_VECTOR_UNARY_FUNC(fract, deFloatFrac)
TCU_DECLARE_VECTOR_BINARY_FUNC(min, de::min)
TCU_DECLARE_VECTOR_BINARY_FUNC(max, de::max)
TCU_DECLARE_VECTOR_TERNARY_FUNC(clamp, de::clamp)
TCU_DECLARE_VECTOR_TERNARY_FUNC(mix, deFloatMix)
TCU_DECLARE_VECTOR_BINARY_FUNC(step, deFloatStep)
TCU_DECLARE_VECTOR_TERNARY_FUNC(smoothStep, deFloatSmoothStep)

} // namespace tcu

#endif // _TCUVECTORUTIL_HPP
