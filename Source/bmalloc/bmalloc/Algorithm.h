/*
 * Copyright (C) 2014-2021 Apple Inc. All rights reserved.
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

#pragma once

#include "BAssert.h"
#include <algorithm>
#include <climits>
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

template<typename T> constexpr T roundUpToMultipleOfImpl(size_t divisor, T x)
{
    static_assert(sizeof(T) == sizeof(uintptr_t), "sizeof(T) must be equal to sizeof(uintptr_t).");
    return static_cast<T>((static_cast<uintptr_t>(x) + (divisor - 1)) & ~(divisor - 1));
}

template<typename T> inline T roundUpToMultipleOf(size_t divisor, T x)
{
    BASSERT(isPowerOfTwo(divisor));
    return roundUpToMultipleOfImpl(divisor, x);
}

template<size_t divisor, typename T> constexpr T roundUpToMultipleOf(T x)
{
    static_assert(isPowerOfTwo(divisor), "'divisor' must be a power of two.");
    return roundUpToMultipleOfImpl(divisor, x);
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

template<typename T> constexpr T divideRoundingUp(T numerator, T denominator)
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

template <typename T>
constexpr unsigned clzConstexpr(T value)
{
    constexpr unsigned bitSize = sizeof(T) * CHAR_BIT;

    using UT = typename std::make_unsigned<T>::type;
    UT uValue = value;

    unsigned zeroCount = 0;
    for (int i = bitSize - 1; i >= 0; i--) {
        if (uValue >> i)
            break;
        zeroCount++;
    }
    return zeroCount;
}

constexpr unsigned long log2(unsigned long value)
{
    return bitCount<unsigned long>() - 1 - __builtin_clzl(value);
}

#define BOFFSETOF(class, field) (reinterpret_cast<ptrdiff_t>(&(reinterpret_cast<class*>(0x4000)->field)) - 0x4000)

template <typename T>
constexpr unsigned ctzConstexpr(T value)
{
    constexpr unsigned bitSize = sizeof(T) * CHAR_BIT;

    using UT = typename std::make_unsigned<T>::type;
    UT uValue = value;

    unsigned zeroCount = 0;
    for (unsigned i = 0; i < bitSize; i++) {
        if (uValue & 1)
            break;

        zeroCount++;
        uValue >>= 1;
    }
    return zeroCount;
}

template<typename T>
inline unsigned ctz(T value)
{
    constexpr unsigned bitSize = sizeof(T) * CHAR_BIT;

    using UT = typename std::make_unsigned<T>::type;
    UT uValue = value;

#if BCOMPILER(GCC_COMPATIBLE)
    if (uValue)
        return __builtin_ctzll(uValue);
    return bitSize;
#elif BCOMPILER(MSVC) && !BCPU(X86)
    unsigned long ret = 0;
    if (_BitScanForward64(&ret, uValue))
        return ret;
    return bitSize;
#else
    UNUSED_PARAM(bitSize);
    UNUSED_PARAM(uValue);
    return ctzConstexpr(value);
#endif
}

template<typename T>
bool findBitInWord(T word, size_t& startOrResultIndex, size_t endIndex, bool value)
{
    static_assert(std::is_unsigned<T>::value, "Type used in findBitInWord must be unsigned");
    constexpr size_t bitsInWord = sizeof(word) * 8;
    BASSERT(startOrResultIndex <= bitsInWord && endIndex <= bitsInWord);
    BUNUSED(bitsInWord);
    
    size_t index = startOrResultIndex;
    word >>= index;

#if BCOMPILER(GCC_COMPATIBLE) && (BCPU(X86_64) || BCPU(ARM64))
    // We should only use ctz() when we know that ctz() is implementated using
    // a fast hardware instruction. Otherwise, this will actually result in
    // worse performance.

    word ^= (static_cast<T>(value) - 1);
    index += ctz(word);
    if (index < endIndex) {
        startOrResultIndex = index;
        return true;
    }
#else
    while (index < endIndex) {
        if ((word & 1) == static_cast<T>(value)) {
            startOrResultIndex = index;
            return true;
        }
        index++;
        word >>= 1;
    }
#endif

    startOrResultIndex = endIndex;
    return false;
}

template<typename T>
constexpr unsigned getLSBSetNonZeroConstexpr(T t)
{
    return ctzConstexpr(t);
}

template<typename T>
constexpr unsigned getMSBSetConstexpr(T t)
{
    constexpr unsigned bitSize = sizeof(T) * CHAR_BIT;
    return bitSize - 1 - clzConstexpr(t);
}

// From http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
constexpr uint32_t roundUpToPowerOfTwo(uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

} // namespace bmalloc
