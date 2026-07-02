/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <stdint.h>
#include <unicode/utypes.h>
#include <wtf/ASCIICType.h>
#include <wtf/BitSet.h>
#include <wtf/SIMDHelpers.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/Latin1Character.h>

#if CPU(X86_SSE2)
#include <emmintrin.h>
#endif

namespace WTF {

template<typename CharacterType> ALWAYS_INLINE constexpr bool isLatin1(CharacterType character)
{
    return unsignedCast(character) <= 0xFFu;
}

template<> ALWAYS_INLINE constexpr bool isLatin1(Latin1Character)
{
    return true;
}

inline constexpr BitSet<256> makeLatin1CharacterBitSet(ASCIILiteral characters)
{
    BitSet<256> bitmap;
    for (char character : characters.span())
        bitmap.set(character);
    return bitmap;
}

inline constexpr BitSet<256> makeLatin1CharacterBitSet(NOESCAPE const Invocable<bool(Latin1Character)> auto& matches)
{
    BitSet<256> bitmap;
    for (unsigned i = 0; i < bitmap.size(); ++i) {
        if (matches(static_cast<Latin1Character>(i)))
            bitmap.set(i);
    }
    return bitmap;
}

template <uintptr_t mask>
inline bool isAlignedTo(const void* pointer)
{
    return !(reinterpret_cast<uintptr_t>(pointer) & mask);
}

// Assuming that a pointer is the size of a "machine word", then
// uintptr_t is an integer type that is also a machine word.
typedef uintptr_t MachineWord;
const uintptr_t machineWordAlignmentMask = sizeof(MachineWord) - 1;

inline bool isAlignedToMachineWord(const void* pointer)
{
    return isAlignedTo<machineWordAlignmentMask>(pointer);
}

template<typename T> inline T* alignToMachineWord(T* pointer)
{
    return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(pointer) & ~machineWordAlignmentMask);
}

template<size_t size, typename CharacterType> struct NonASCIIMask;
template<> struct NonASCIIMask<4, char16_t> {
    static inline uint32_t value() { return 0xFF80FF80U; }
};
template<> struct NonASCIIMask<4, Latin1Character> {
    static inline uint32_t value() { return 0x80808080U; }
};
template<> struct NonASCIIMask<4, char8_t> {
    static inline uint32_t value() { return 0x80808080U; }
};
template<> struct NonASCIIMask<8, char16_t> {
    static inline uint64_t value() { return 0xFF80FF80FF80FF80ULL; }
};
template<> struct NonASCIIMask<8, Latin1Character> {
    static inline uint64_t value() { return 0x8080808080808080ULL; }
};
template<> struct NonASCIIMask<8, char8_t> {
    static inline uint64_t value() { return 0x8080808080808080ULL; }
};

template<size_t size, typename CharacterType> struct NonLatin1Mask;
template<> struct NonLatin1Mask<4, char16_t> {
    static inline uint32_t value() { return 0xFF00FF00U; }
};
template<> struct NonLatin1Mask<8, char16_t> {
    static inline uint64_t value() { return 0xFF00FF00FF00FF00ULL; }
};

template<typename CharacterType>
inline bool containsOnlyASCII(MachineWord word)
{
    return !(word & NonASCIIMask<sizeof(MachineWord), CharacterType>::value());
}

template<typename CharacterType>
SUPPRESS_NODELETE inline bool NODELETE charactersAreAllASCII(std::span<const CharacterType> span)
{
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    using UnsignedType = SameSizeUnsignedInteger<CharacterType>;
    constexpr size_t simdStride = SIMD::stride<UnsignedType>;
    constexpr size_t chunkSize = 8 * simdStride;

    const auto* characters = span.data();
    size_t length = span.size();
    const auto* end = characters + length;

    if (length >= simdStride) {
        constexpr auto nonASCIIMask = static_cast<UnsignedType>(~UnsignedType { 0x7F });
        auto mask = SIMD::splat<UnsignedType>(nonASCIIMask);

        // Process chunkSize elements per chunk (8 x SIMD vectors), check once per chunk.
        const auto* chunkEnd = characters + (length & ~(chunkSize - 1));
        while (characters < chunkEnd) {
            auto acc = SIMD::load(std::bit_cast<const UnsignedType*>(characters));
            acc = SIMD::bitOr2(acc, SIMD::load(std::bit_cast<const UnsignedType*>(characters + simdStride)));
            acc = SIMD::bitOr2(acc, SIMD::load(std::bit_cast<const UnsignedType*>(characters + 2 * simdStride)));
            acc = SIMD::bitOr2(acc, SIMD::load(std::bit_cast<const UnsignedType*>(characters + 3 * simdStride)));
            acc = SIMD::bitOr2(acc, SIMD::load(std::bit_cast<const UnsignedType*>(characters + 4 * simdStride)));
            acc = SIMD::bitOr2(acc, SIMD::load(std::bit_cast<const UnsignedType*>(characters + 5 * simdStride)));
            acc = SIMD::bitOr2(acc, SIMD::load(std::bit_cast<const UnsignedType*>(characters + 6 * simdStride)));
            acc = SIMD::bitOr2(acc, SIMD::load(std::bit_cast<const UnsignedType*>(characters + 7 * simdStride)));
            if (SIMD::isNonZero(SIMD::bitAnd2(acc, mask)))
                return false;
            characters += chunkSize;
        }
        // Handle remaining SIMD vectors.
        const auto* simdEnd = characters + (static_cast<size_t>(end - characters) & ~(simdStride - 1));
        auto acc = SIMD::splat<UnsignedType>(0);
        while (characters < simdEnd) {
            acc = SIMD::bitOr2(acc, SIMD::load(std::bit_cast<const UnsignedType*>(characters)));
            characters += simdStride;
        }
        if (SIMD::isNonZero(SIMD::bitAnd2(acc, mask)))
            return false;
    }

    // Scalar tail with early exit.
    while (characters < end) {
        if (!isASCII(*characters++))
            return false;
    }
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    return true;
}

ALWAYS_INLINE constexpr bool charactersAreAllLatin1(std::span<const Latin1Character>)
{
    return true;
}

inline constexpr bool charactersAreAllLatin1(std::span<const char16_t> span)
{
    if (std::is_constant_evaluated()) {
        for (auto character : span) {
            if (static_cast<uint16_t>(character) > 0xFF)
                return false;
        }
    } else {
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        constexpr size_t simdStride = SIMD::stride<uint16_t>;

        const auto* characters = span.data();
        const auto* end = characters + span.size();
        const auto* simdEnd = characters + (span.size() & ~(simdStride - 1));

        auto nonLatin1Mask = SIMD::splat16(0xFF00);
        while (characters < simdEnd) {
            auto chunk = SIMD::load(reinterpret_cast<const uint16_t*>(characters));
            if (SIMD::isNonZero(SIMD::bitAnd2(chunk, nonLatin1Mask)))
                return false;
            characters += simdStride;
        }

        while (characters < end) {
            if (!isLatin1(*characters++))
                return false;
        }
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    }
    return true;
}

} // namespace WTF

using WTF::charactersAreAllASCII;
using WTF::isLatin1;
using WTF::makeLatin1CharacterBitSet;
