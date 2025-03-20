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
#include <wtf/StdLibExtras.h>
#include <wtf/text/LChar.h>

#if CPU(X86_SSE2)
#include <emmintrin.h>
#endif

namespace WTF {

template<unsigned charactersCount>
inline constexpr BitSet<256> makeLatin1CharacterBitSet(const char (&characters)[charactersCount])
{
    static_assert(charactersCount > 0, "Since string literal is null terminated, characterCount is always larger than 0");
    BitSet<256> bitmap;
    for (unsigned i = 0; i < charactersCount - 1; ++i)
        bitmap.set(characters[i]);
    return bitmap;
}

inline constexpr BitSet<256> makeLatin1CharacterBitSet(NOESCAPE const Invocable<bool(LChar)> auto& matches)
{
    BitSet<256> bitmap;
    for (unsigned i = 0; i < bitmap.size(); ++i) {
        if (matches(static_cast<LChar>(i)))
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
template<> struct NonASCIIMask<4, UChar> {
    static inline uint32_t value() { return 0xFF80FF80U; }
};
template<> struct NonASCIIMask<4, LChar> {
    static inline uint32_t value() { return 0x80808080U; }
};
template<> struct NonASCIIMask<4, char8_t> {
    static inline uint32_t value() { return 0x80808080U; }
};
template<> struct NonASCIIMask<8, UChar> {
    static inline uint64_t value() { return 0xFF80FF80FF80FF80ULL; }
};
template<> struct NonASCIIMask<8, LChar> {
    static inline uint64_t value() { return 0x8080808080808080ULL; }
};
template<> struct NonASCIIMask<8, char8_t> {
    static inline uint64_t value() { return 0x8080808080808080ULL; }
};

template<size_t size, typename CharacterType> struct NonLatin1Mask;
template<> struct NonLatin1Mask<4, UChar> {
    static inline uint32_t value() { return 0xFF00FF00U; }
};
template<> struct NonLatin1Mask<8, UChar> {
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

// Note: This function assume the input is likely all Latin1, and
// does not leave early if it is not the case.
template<typename CharacterType>
inline bool charactersAreAllLatin1(std::span<const CharacterType> span)
{
    if constexpr (sizeof(CharacterType) == 1)
        return true;
    else {
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

        MachineWord nonLatin1BitMask = NonLatin1Mask<sizeof(MachineWord), CharacterType>::value();
        return !(allCharBits & nonLatin1BitMask);
    }
}

} // namespace WTF

using WTF::charactersAreAllASCII;
using WTF::makeLatin1CharacterBitSet;
