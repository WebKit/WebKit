#ifndef _DEDEFS_HPP
#define _DEDEFS_HPP
/*-------------------------------------------------------------------------
 * drawElements C++ Base Library
 * -----------------------------
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
 * \brief Basic definitions.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"
#include "deInt32.h"

#if !defined(__cplusplus)
#error "C++ is required"
#endif

#include <type_traits>
#include <utility>

namespace de
{

//! Compute absolute value of x.
template <typename T>
inline T abs(T x)
{
    return x < T(0) ? -x : x;
}

//! Get minimum of x and y.
template <typename T>
inline T min(T x, T y)
{
    return x <= y ? x : y;
}

//! Get maximum of x and y.
template <typename T>
inline T max(T x, T y)
{
    return x >= y ? x : y;
}

//! Clamp x in range a <= x <= b.
template <typename T>
inline T clamp(T x, T a, T b)
{
    DE_ASSERT(a <= b);
    return x < a ? a : (x > b ? b : x);
}

//! Test if x is in bounds a <= x < b.
template <typename T>
inline bool inBounds(T x, T a, T b)
{
    return a <= x && x < b;
}

//! Test if x is in range a <= x <= b.
template <typename T>
inline bool inRange(T x, T a, T b)
{
    return a <= x && x <= b;
}

//! Return T with low n bits set
template <typename T>
inline T rightSetMask(T n)
{
    DE_ASSERT(n < T(sizeof(T) * 8));
    T one = T(1);
    return T((one << n) - one);
}

//! Return T with low n bits reset
template <typename T>
inline T rightZeroMask(T n)
{
    return T(~rightSetMask(n));
}

//! Return T with high n bits set
template <typename T>
inline T leftSetMask(T n)
{
    const T tlen = T(sizeof(T) * 8);
    return T(~rightSetMask(tlen >= n ? tlen - n : T(0)));
}

//! Return T with high n bits reset
template <typename T>
inline T leftZeroMask(T n)
{
    return T(~leftSetMask(n));
}

//! Round x up to a multiple of y.
template <typename T>
inline T roundUp(T x, T y)
{
    DE_ASSERT(y != T(0));
    const T mod = x % y;
    return x + ((mod == T(0)) ? T(0) : (y - mod));
}

//! Round x down to a multiple of y.
template <typename T>
inline T roundDown(T x, T y)
{
    DE_ASSERT(y != T(0));
    return (x / y) * y;
}

//! Find the greatest common divisor of x and y.
template <typename T>
T gcd(T x, T y)
{
    DE_ASSERT(std::is_integral<T>::value && std::is_unsigned<T>::value);

    // Euclidean algorithm.
    while (y != T{0})
    {
        T mod = x % y;
        x     = y;
        y     = mod;
    }

    return x;
}

//! Find the least common multiple of x and y.
template <typename T>
T lcm(T x, T y)
{
    DE_ASSERT(std::is_integral<T>::value && std::is_unsigned<T>::value);

    T prod = x * y;
    DE_ASSERT(x == 0 || prod / x == y); // Check overflow just in case.
    return (prod) / gcd(x, y);
}

//! Helper for DE_CHECK() macros.
void throwRuntimeError(const char *message, const char *expr, const char *file, int line);

//! Default deleter.
template <typename T>
struct DefaultDeleter
{
    inline DefaultDeleter(void)
    {
    }
    template <typename U>
    inline DefaultDeleter(const DefaultDeleter<U> &)
    {
    }
    template <typename U>
    inline DefaultDeleter<T> &operator=(const DefaultDeleter<U> &)
    {
        return *this;
    }
    inline void operator()(T *ptr) const
    {
        delete ptr;
    }
};

//! A deleter for arrays
template <typename T>
struct ArrayDeleter
{
    inline ArrayDeleter(void)
    {
    }
    template <typename U>
    inline ArrayDeleter(const ArrayDeleter<U> &)
    {
    }
    template <typename U>
    inline ArrayDeleter<T> &operator=(const ArrayDeleter<U> &)
    {
        return *this;
    }
    inline void operator()(T *ptr) const
    {
        delete[] ptr;
    }
};

//! Get required memory alignment for type
template <typename T>
size_t alignOf(void)
{
    struct PaddingCheck
    {
        uint8_t b;
        T t;
    };
    return (size_t)offsetof(PaddingCheck, t);
}

//! Similar to DE_LENGTH_OF_ARRAY but constexpr and without auxiliar helpers.
template <typename T, size_t N>
constexpr size_t arrayLength(const T (&)[N])
{
    return N;
}

//! Get least significant bit index
inline int findLSB(uint32_t value)
{
    return value ? deCtz32(value) : (-1);
}

//! Get most significant bit index
inline int findMSB(uint32_t value)
{
    return 31 - deClz32(value);
}

//! Get most significant bit index
inline int findMSB(int32_t value)
{
    return (value < 0) ? findMSB(~(uint32_t)value) : findMSB((uint32_t)value);
}

} // namespace de

// sizeof(X) as uint32_t
#define DE_SIZEOF32(X) (static_cast<uint32_t>(sizeof(X)))

/*--------------------------------------------------------------------*//*!
 * \brief Throw runtime error if condition is not met.
 * \param X        Condition to check.
 *
 * This macro throws std::runtime_error if condition X is not met.
 *//*--------------------------------------------------------------------*/
#define DE_CHECK_RUNTIME_ERR(X)                                       \
    do                                                                \
    {                                                                 \
        if ((!false && (X)) ? false : true)                           \
            ::de::throwRuntimeError(nullptr, #X, __FILE__, __LINE__); \
    } while (false)

/*--------------------------------------------------------------------*//*!
 * \brief Throw runtime error if condition is not met.
 * \param X        Condition to check.
 * \param MSG    Additional message to include in the exception.
 *
 * This macro throws std::runtime_error with message MSG if condition X is
 * not met.
 *//*--------------------------------------------------------------------*/
#define DE_CHECK_RUNTIME_ERR_MSG(X, MSG)                          \
    do                                                            \
    {                                                             \
        if ((!false && (X)) ? false : true)                       \
            ::de::throwRuntimeError(MSG, #X, __FILE__, __LINE__); \
    } while (false)

//! Get array start pointer.
#define DE_ARRAY_BEGIN(ARR) (&(ARR)[0])

//! Get array end pointer.
#define DE_ARRAY_END(ARR) (DE_ARRAY_BEGIN(ARR) + DE_LENGTH_OF_ARRAY(ARR))

//! Empty C++ compilation unit silencing.
#if (DE_COMPILER == DE_COMPILER_MSC)
#define DE_EMPTY_CPP_FILE \
    namespace             \
    {                     \
    uint8_t unused;       \
    }
#else
#define DE_EMPTY_CPP_FILE
#endif

// Warn if type is constructed, but left unused
//
// Used in types with non-trivial ctor/dtor but with ctor-dtor pair causing no (observable)
// side-effects.
//
// \todo add attribute for GCC
#if (DE_COMPILER == DE_COMPILER_CLANG) && defined(__has_attribute)
#if __has_attribute(warn_unused)
#define DE_WARN_UNUSED_TYPE __attribute__((warn_unused))
#else
#define DE_WARN_UNUSED_TYPE
#endif
#else
#define DE_WARN_UNUSED_TYPE
#endif

#endif // _DEDEFS_HPP
