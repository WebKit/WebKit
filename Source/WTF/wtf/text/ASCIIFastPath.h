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

// Note: This function assume the input is likely all ASCII, and
// does not leave early if it is not the case.
template<typename CharacterType>
inline bool charactersAreAllASCII(std::span<const CharacterType> span)
{
    MachineWord allCharBits = 0;

    // Prologue: align the input.
    while (!span.empty() && !isAlignedToMachineWord(span.data()))
        allCharBits |= WTF::consume(span);

    // Compare the values of CPU word size.
    size_t sizeAfterAlignedEnd = std::to_address(span.end()) - alignToMachineWord(std::to_address(span.end()));
    const size_t loopIncrement = sizeof(MachineWord) / sizeof(CharacterType);
    while (span.size() > sizeAfterAlignedEnd)
        allCharBits |= reinterpretCastSpanStartTo<const MachineWord>(consumeSpan(span, loopIncrement));

    // Process the remaining bytes.
    while (!span.empty())
        allCharBits |= WTF::consume(span);

    MachineWord nonASCIIBitMask = NonASCIIMask<sizeof(MachineWord), CharacterType>::value();
    return !(allCharBits & nonASCIIBitMask);
}

ALWAYS_INLINE bool charactersAreAllLatin1(std::span<const Latin1Character>)
{
    return true;
}

inline bool charactersAreAllLatin1(std::span<const char16_t> span)
{
#if CPU(ARM64)
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    const auto* characters = span.data();
    size_t length = span.size();

    const auto* end = characters + length;
    const auto* simdEnd = characters + (length & ~7); // Process 8 chars at a time.

    uint16x8_t mask = vdupq_n_u16(0xFF00);

    // SIMD loop with early exit.
    while (characters < simdEnd) {
        uint16x8_t chunk = vld1q_u16(reinterpret_cast<const uint16_t*>(characters));
        uint16x8_t nonLatin1Bits = vandq_u16(chunk, mask);

        // Early exit: check if any non-Latin1 character found.
        if (vmaxvq_u16(nonLatin1Bits))
            return false;

        characters += 8;
    }

    // Scalar tail with early exit.
    while (characters < end) {
        if (!isLatin1(*characters++))
            return false;
    }
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
#else
    constexpr size_t loopIncrement = sizeof(MachineWord) / sizeof(char16_t);
    MachineWord nonLatin1BitMask = NonLatin1Mask<sizeof(MachineWord), char16_t>::value();

    // Align to machine word.
    while (!span.empty() && !isAlignedToMachineWord(span.data())) {
        if (!isLatin1(WTF::consume(span)))
            return false;
    }

    // Process machine words with early exit.
    while (span.size() >= loopIncrement) {
        auto word = reinterpretCastSpanStartTo<const MachineWord>(consumeSpan(span, loopIncrement));
        if (word & nonLatin1BitMask)
            return false;
    }

    // Process remaining characters.
    while (!span.empty()) {
        if (!isLatin1(WTF::consume(span)))
            return false;
    }
#endif
    return true;
}

} // namespace WTF

using WTF::charactersAreAllASCII;
using WTF::isLatin1;
using WTF::makeLatin1CharacterBitSet;
