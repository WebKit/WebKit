/*
 * Copyright (C) 2018 Yusuke Suzuki <yusukesuzuki@slowstart.org>.
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

#include <type_traits>
#include <wtf/Int128.h>
#include <wtf/Platform.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

template<typename Type>
inline Type unalignedLoad(const void* pointer)
{
    static_assert(std::is_trivially_copyable<Type>::value);
    Type result { };
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    memcpy(&result, pointer, sizeof(Type));
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    return result;
}

template<typename Type>
inline void unalignedStore(void* pointer, Type value)
{
    static_assert(std::is_trivially_copyable<Type>::value);
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    memcpy(pointer, &value, sizeof(Type));
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

// The widest type a single unaligned load or store lowers to one machine access.
#if HAVE(INT128_T)
using WidestUnalignedUnit = UInt128;
#else
using WidestUnalignedUnit = uint64_t;
#endif

constexpr size_t maxSmallCopySize = 4 * sizeof(WidestUnalignedUnit);

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

// Both source reads happen before either destination write, which is what makes this safe for
// overlapping ranges without comparing the two pointers.
template<typename Type>
ALWAYS_INLINE void copyOverlappingEnds(uint8_t* destination, const uint8_t* source, size_t count)
{
    ASSERT(count >= sizeof(Type) && count <= 2 * sizeof(Type));
    Type low = unalignedLoad<Type>(source);
    Type high = unalignedLoad<Type>(source + count - sizeof(Type));
    unalignedStore<Type>(destination, low);
    unalignedStore<Type>(destination + count - sizeof(Type), high);
}

template<typename Type>
ALWAYS_INLINE void copyOverlappingEndPairs(uint8_t* destination, const uint8_t* source, size_t count)
{
    ASSERT(count >= 2 * sizeof(Type) && count <= 4 * sizeof(Type));
    Type low0 = unalignedLoad<Type>(source);
    Type low1 = unalignedLoad<Type>(source + sizeof(Type));
    Type high0 = unalignedLoad<Type>(source + count - 2 * sizeof(Type));
    Type high1 = unalignedLoad<Type>(source + count - sizeof(Type));
    unalignedStore<Type>(destination, low0);
    unalignedStore<Type>(destination + sizeof(Type), low1);
    unalignedStore<Type>(destination + count - 2 * sizeof(Type), high0);
    unalignedStore<Type>(destination + count - sizeof(Type), high1);
}

// Copies a count the caller already knows to be in [minimumCount, maximumCount] without calling
// libc, which spends more on dispatching by size than a copy this short costs. Overlapping ranges
// are safe, as with memmove. Each bound the caller can tighten drops a compare and an arm.
template<size_t minimumCount = sizeof(uint16_t), size_t maximumCount = maxSmallCopySize>
ALWAYS_INLINE void copySmallMemory(uint8_t* destination, const uint8_t* source, size_t count)
{
    static_assert(minimumCount >= sizeof(uint16_t));
    static_assert(maximumCount <= maxSmallCopySize);
    ASSERT(count >= minimumCount);
    ASSERT(count <= maximumCount);

    if constexpr (minimumCount < 2 * sizeof(uint16_t)) {
        if (maximumCount <= 2 * sizeof(uint16_t) || count < 2 * sizeof(uint16_t)) {
            copyOverlappingEnds<uint16_t>(destination, source, count);
            return;
        }
    }
    if constexpr (minimumCount < 2 * sizeof(uint32_t)) {
        if (maximumCount <= 2 * sizeof(uint32_t) || count < 2 * sizeof(uint32_t)) {
            copyOverlappingEnds<uint32_t>(destination, source, count);
            return;
        }
    }
    if constexpr (minimumCount < 2 * sizeof(uint64_t)) {
        if (maximumCount <= 2 * sizeof(uint64_t) || count < 2 * sizeof(uint64_t)) {
            copyOverlappingEnds<uint64_t>(destination, source, count);
            return;
        }
    }
    if constexpr (minimumCount < 2 * sizeof(WidestUnalignedUnit)) {
        if (maximumCount <= 2 * sizeof(WidestUnalignedUnit) || count < 2 * sizeof(WidestUnalignedUnit)) {
            copyOverlappingEnds<WidestUnalignedUnit>(destination, source, count);
            return;
        }
    }
    copyOverlappingEndPairs<WidestUnalignedUnit>(destination, source, count);
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

} // namespace WTF
