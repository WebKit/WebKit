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

#include "Algorithm.h"
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <type_traits>
#include <chrono>

namespace bmalloc {

// Versions of min and max that are compatible with compile-time constants.
template<typename T> inline constexpr T max(T a, T b)
{
    return a > b ? a : b;
}
    
template<typename T> inline constexpr T min(T a, T b)
{
    return a < b ? a : b;
}
    
template<typename T> inline constexpr T mask(T value, uintptr_t mask)
{
    return reinterpret_cast<T>(reinterpret_cast<uintptr_t>(value) & mask);
}

template<typename T> inline constexpr bool test(T value, uintptr_t mask)
{
    return !!(reinterpret_cast<uintptr_t>(value) & mask);
}

template<size_t divisor, typename T> inline constexpr T roundUpToMultipleOf(T x)
{
    static_assert(divisor && !(divisor & (divisor - 1)), "'divisor' must be a power of two.");
    return reinterpret_cast<T>((reinterpret_cast<uintptr_t>(x) + (divisor - 1ul)) & ~(divisor - 1ul));
}

template<size_t divisor, typename T> inline constexpr T roundDownToMultipleOf(T x)
{
    static_assert(divisor && !(divisor & (divisor - 1)), "'divisor' must be a power of two.");
    return reinterpret_cast<T>(mask(reinterpret_cast<uintptr_t>(x), ~(divisor - 1ul)));
}

// Version of sizeof that returns 0 for empty classes.

template<typename T> inline constexpr size_t sizeOf()
{
    return std::is_empty<T>::value ? 0 : sizeof(T);
}

template<typename T> inline constexpr size_t bitCount()
{
    return sizeof(T) * 8;
}

inline constexpr bool isPowerOfTwo(size_t size)
{
    return !(size & (size - 1));
}

} // namespace bmalloc

#endif // Algorithm_h
