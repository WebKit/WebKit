/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef Algorithm_h
#define Algorithm_h

#include "BAssert.h"
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <string.h>
#include <type_traits>
#include <chrono>

namespace bmalloc {

// Versions of min and max that are compatible with compile-time constants.
template<typename T> constexpr T max(T a, T b)
{
    return a > b ? a : b;
}
    
template<typename T> constexpr T min(T a, T b)
{
    return a < b ? a : b;
}

template<typename T> constexpr T mask(T value, uintptr_t mask)
{
    static_assert(sizeof(T) == sizeof(uintptr_t), "sizeof(T) must be equal to sizeof(uintptr_t).");
    return static_cast<T>(static_cast<uintptr_t>(value) & mask);
}

template<typename T> inline T* mask(T* value, uintptr_t mask)
{
    return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(value) & mask);
}

template<typename T> constexpr bool test(T value, uintptr_t mask)
{
    return !!(reinterpret_cast<uintptr_t>(value) & mask);
}

template <typename T>
constexpr bool isPowerOfTwo(T size)
{
    static_assert(std::is_integral<T>::value, "");
    return size && !(size & (size - 1));
}

template<typename T> inline T roundUpToMultipleOf(size_t divisor, T x)
{
    BASSERT(isPowerOfTwo(divisor));
    static_assert(sizeof(T) == sizeof(uintptr_t), "sizeof(T) must be equal to sizeof(uintptr_t).");
    return static_cast<T>((static_cast<uintptr_t>(x) + (divisor - 1)) & ~(divisor - 1));
}

template<size_t divisor, typename T> inline T roundUpToMultipleOf(T x)
{
    static_assert(isPowerOfTwo(divisor), "'divisor' must be a power of two.");
    return roundUpToMultipleOf(divisor, x);
}

template<typename T> inline T* roundUpToMultipleOf(size_t divisor, T* x)
{
    BASSERT(isPowerOfTwo(divisor));
    return reinterpret_cast<T*>((reinterpret_cast<uintptr_t>(x) + (divisor - 1)) & ~(divisor - 1));
}

template<size_t divisor, typename T> inline T* roundUpToMultipleOf(T* x)
{
    static_assert(isPowerOfTwo(divisor), "'divisor' must be a power of two.");
    return roundUpToMultipleOf(divisor, x);
}

template<typename T> inline T roundDownToMultipleOf(size_t divisor, T x)
{
    BASSERT(isPowerOfTwo(divisor));
    static_assert(sizeof(T) == sizeof(uintptr_t), "sizeof(T) must be equal to sizeof(uintptr_t).");
    return static_cast<T>(mask(static_cast<uintptr_t>(x), ~(divisor - 1ul)));
}

template<typename T> inline T* roundDownToMultipleOf(size_t divisor, T* x)
{
    BASSERT(isPowerOfTwo(divisor));
    return reinterpret_cast<T*>(mask(reinterpret_cast<uintptr_t>(x), ~(divisor - 1ul)));
}

template<size_t divisor, typename T> constexpr T roundDownToMultipleOf(T x)
{
    static_assert(isPowerOfTwo(divisor), "'divisor' must be a power of two.");
    return roundDownToMultipleOf(divisor, x);
}

template<typename T> inline void divideRoundingUp(T numerator, T denominator, T& quotient, T& remainder)
{
    // We expect the compiler to emit a single divide instruction to extract both the quotient and the remainder.
    quotient = numerator / denominator;
    remainder = numerator % denominator;
    if (remainder)
        quotient += 1;
}

template<typename T> inline T divideRoundingUp(T numerator, T denominator)
{
    return (numerator + denominator - 1) / denominator;
}

template<typename T> inline T roundUpToMultipleOfNonPowerOfTwo(size_t divisor, T x)
{
    return divideRoundingUp(x, divisor) * divisor;
}

// Version of sizeof that returns 0 for empty classes.

template<typename T> constexpr size_t sizeOf()
{
    return std::is_empty<T>::value ? 0 : sizeof(T);
}

template<typename T> constexpr size_t bitCount()
{
    return sizeof(T) * 8;
}

#if BOS(WINDOWS)
template<int depth> __forceinline constexpr unsigned long clzl(unsigned long value)
{
    return value & (1UL << (bitCount<unsigned long>() - 1)) ? 0 : 1 + clzl<depth - 1>(value << 1);
}

template<> __forceinline constexpr unsigned long clzl<1>(unsigned long value)
{
    return 0;
}

__forceinline constexpr unsigned long __builtin_clzl(unsigned long value)
{
    return value == 0 ? 32 : clzl<bitCount<unsigned long>()>(value);
}
#endif

constexpr unsigned long log2(unsigned long value)
{
    return bitCount<unsigned long>() - 1 - __builtin_clzl(value);
}

#define BOFFSETOF(class, field) (reinterpret_cast<ptrdiff_t>(&(reinterpret_cast<class*>(0x4000)->field)) - 0x4000)

template<typename T>
bool findBitInWord(T word, size_t& index, size_t endIndex, bool value)
{
    static_assert(std::is_unsigned<T>::value, "Type used in findBitInWord must be unsigned");
    
    word >>= index;
    
    while (index < endIndex) {
        if ((word & 1) == static_cast<T>(value))
            return true;
        index++;
        word >>= 1;
    }
    
    index = endIndex;
    return false;
}

} // namespace bmalloc

#endif // Algorithm_h
