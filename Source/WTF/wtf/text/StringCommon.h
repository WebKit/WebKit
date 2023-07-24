/*
 * Copyright (C) 2015-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2022 Jarred Sumner. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <algorithm>
#include <unicode/uchar.h>
#include <wtf/ASCIICType.h>
#include <wtf/NotFound.h>
#include <wtf/UnalignedAccess.h>
#include <wtf/text/ASCIIFastPath.h>
#include <wtf/text/ASCIILiteral.h>

#if CPU(ARM64)
#include <arm_neon.h>
#endif

namespace WTF {

template<typename CharacterType> inline bool isLatin1(CharacterType character)
{
    using UnsignedCharacterType = typename std::make_unsigned<CharacterType>::type;
    return static_cast<UnsignedCharacterType>(character) <= static_cast<UnsignedCharacterType>(0xFF);
}

template<> ALWAYS_INLINE bool isLatin1(LChar)
{
    return true;
}

using CodeUnitMatchFunction = bool (*)(UChar);

template<typename CharacterTypeA, typename CharacterTypeB> bool equalIgnoringASCIICase(const CharacterTypeA*, const CharacterTypeB*, unsigned length);
template<typename CharacterTypeA, typename CharacterTypeB> bool equalIgnoringASCIICase(const CharacterTypeA*, unsigned lengthA, const CharacterTypeB*, unsigned lengthB);

template<typename StringClassA, typename StringClassB> bool equalIgnoringASCIICaseCommon(const StringClassA&, const StringClassB&);

template<typename CharacterType> bool equalLettersIgnoringASCIICase(const CharacterType*, const char* lowercaseLetters, unsigned length);
template<typename CharacterType> bool equalLettersIgnoringASCIICase(const CharacterType*, unsigned charactersLength, ASCIILiteral);

template<typename StringClass> bool equalLettersIgnoringASCIICaseCommon(const StringClass&, ASCIILiteral);

bool equalIgnoringASCIICase(const char*, const char*);
bool equalLettersIgnoringASCIICase(const char*, ASCIILiteral);

// Do comparisons 8 or 4 bytes-at-a-time on architectures where it's safe.
#if (CPU(X86_64) || CPU(ARM64)) && !ASAN_ENABLED
ALWAYS_INLINE bool equal(const LChar* aLChar, const LChar* bLChar, unsigned length)
{
    unsigned dwordLength = length >> 3;

    const char* a = reinterpret_cast<const char*>(aLChar);
    const char* b = reinterpret_cast<const char*>(bLChar);

    if (dwordLength) {
        for (unsigned i = 0; i != dwordLength; ++i) {
            if (unalignedLoad<uint64_t>(a) != unalignedLoad<uint64_t>(b))
                return false;

            a += sizeof(uint64_t);
            b += sizeof(uint64_t);
        }
    }

    if (length & 4) {
        if (unalignedLoad<uint32_t>(a) != unalignedLoad<uint32_t>(b))
            return false;

        a += sizeof(uint32_t);
        b += sizeof(uint32_t);
    }

    if (length & 2) {
        if (unalignedLoad<uint16_t>(a) != unalignedLoad<uint16_t>(b))
            return false;

        a += sizeof(uint16_t);
        b += sizeof(uint16_t);
    }

    if (length & 1 && (*reinterpret_cast<const LChar*>(a) != *reinterpret_cast<const LChar*>(b)))
        return false;

    return true;
}

ALWAYS_INLINE bool equal(const UChar* aUChar, const UChar* bUChar, unsigned length)
{
    unsigned dwordLength = length >> 2;

    const char* a = reinterpret_cast<const char*>(aUChar);
    const char* b = reinterpret_cast<const char*>(bUChar);

    if (dwordLength) {
        for (unsigned i = 0; i != dwordLength; ++i) {
            if (unalignedLoad<uint64_t>(a) != unalignedLoad<uint64_t>(b))
                return false;

            a += sizeof(uint64_t);
            b += sizeof(uint64_t);
        }
    }

    if (length & 2) {
        if (unalignedLoad<uint32_t>(a) != unalignedLoad<uint32_t>(b))
            return false;

        a += sizeof(uint32_t);
        b += sizeof(uint32_t);
    }

    if (length & 1 && (*reinterpret_cast<const UChar*>(a) != *reinterpret_cast<const UChar*>(b)))
        return false;

    return true;
}
#elif CPU(X86) && !ASAN_ENABLED
ALWAYS_INLINE bool equal(const LChar* aLChar, const LChar* bLChar, unsigned length)
{
    const char* a = reinterpret_cast<const char*>(aLChar);
    const char* b = reinterpret_cast<const char*>(bLChar);

    unsigned wordLength = length >> 2;
    for (unsigned i = 0; i != wordLength; ++i) {
        if (unalignedLoad<uint32_t>(a) != unalignedLoad<uint32_t>(b))
            return false;
        a += sizeof(uint32_t);
        b += sizeof(uint32_t);
    }

    length &= 3;

    if (length) {
        const LChar* aRemainder = reinterpret_cast<const LChar*>(a);
        const LChar* bRemainder = reinterpret_cast<const LChar*>(b);

        for (unsigned i = 0; i <  length; ++i) {
            if (aRemainder[i] != bRemainder[i])
                return false;
        }
    }

    return true;
}

ALWAYS_INLINE bool equal(const UChar* aUChar, const UChar* bUChar, unsigned length)
{
    const char* a = reinterpret_cast<const char*>(aUChar);
    const char* b = reinterpret_cast<const char*>(bUChar);

    unsigned wordLength = length >> 1;
    for (unsigned i = 0; i != wordLength; ++i) {
        if (unalignedLoad<uint32_t>(a) != unalignedLoad<uint32_t>(b))
            return false;
        a += sizeof(uint32_t);
        b += sizeof(uint32_t);
    }

    if (length & 1 && *reinterpret_cast<const UChar*>(a) != *reinterpret_cast<const UChar*>(b))
        return false;

    return true;
}
#elif OS(DARWIN) && WTF_ARM_ARCH_AT_LEAST(7) && !ASAN_ENABLED
ALWAYS_INLINE bool equal(const LChar* a, const LChar* b, unsigned length)
{
    bool isEqual = false;
    uint32_t aValue;
    uint32_t bValue;
    asm("subs   %[length], #4\n"
        "blo    2f\n"

        "0:\n" // Label 0 = Start of loop over 32 bits.
        "ldr    %[aValue], [%[a]], #4\n"
        "ldr    %[bValue], [%[b]], #4\n"
        "cmp    %[aValue], %[bValue]\n"
        "bne    66f\n"
        "subs   %[length], #4\n"
        "bhs    0b\n"

        // At this point, length can be:
        // -0: 00000000000000000000000000000000 (0 bytes left)
        // -1: 11111111111111111111111111111111 (3 bytes left)
        // -2: 11111111111111111111111111111110 (2 bytes left)
        // -3: 11111111111111111111111111111101 (1 byte left)
        // -4: 11111111111111111111111111111100 (length was 0)
        // The pointers are at the correct position.
        "2:\n" // Label 2 = End of loop over 32 bits, check for pair of characters.
        "tst    %[length], #2\n"
        "beq    1f\n"
        "ldrh   %[aValue], [%[a]], #2\n"
        "ldrh   %[bValue], [%[b]], #2\n"
        "cmp    %[aValue], %[bValue]\n"
        "bne    66f\n"

        "1:\n" // Label 1 = Check for a single character left.
        "tst    %[length], #1\n"
        "beq    42f\n"
        "ldrb   %[aValue], [%[a]]\n"
        "ldrb   %[bValue], [%[b]]\n"
        "cmp    %[aValue], %[bValue]\n"
        "bne    66f\n"

        "42:\n" // Label 42 = Success.
        "mov    %[isEqual], #1\n"
        "66:\n" // Label 66 = End without changing isEqual to 1.
        : [length]"+r"(length), [isEqual]"+r"(isEqual), [a]"+r"(a), [b]"+r"(b), [aValue]"+r"(aValue), [bValue]"+r"(bValue)
        :
        :
        );
    return isEqual;
}

ALWAYS_INLINE bool equal(const UChar* a, const UChar* b, unsigned length)
{
    bool isEqual = false;
    uint32_t aValue;
    uint32_t bValue;
    asm("subs   %[length], #2\n"
        "blo    1f\n"

        "0:\n" // Label 0 = Start of loop over 32 bits.
        "ldr    %[aValue], [%[a]], #4\n"
        "ldr    %[bValue], [%[b]], #4\n"
        "cmp    %[aValue], %[bValue]\n"
        "bne    66f\n"
        "subs   %[length], #2\n"
        "bhs    0b\n"

        // At this point, length can be:
        // -0: 00000000000000000000000000000000 (0 bytes left)
        // -1: 11111111111111111111111111111111 (1 character left, 2 bytes)
        // -2: 11111111111111111111111111111110 (length was zero)
        // The pointers are at the correct position.
        "1:\n" // Label 1 = Check for a single character left.
        "tst    %[length], #1\n"
        "beq    42f\n"
        "ldrh   %[aValue], [%[a]]\n"
        "ldrh   %[bValue], [%[b]]\n"
        "cmp    %[aValue], %[bValue]\n"
        "bne    66f\n"

        "42:\n" // Label 42 = Success.
        "mov    %[isEqual], #1\n"
        "66:\n" // Label 66 = End without changing isEqual to 1.
        : [length]"+r"(length), [isEqual]"+r"(isEqual), [a]"+r"(a), [b]"+r"(b), [aValue]"+r"(aValue), [bValue]"+r"(bValue)
        :
        :
        );
    return isEqual;
}
#elif !ASAN_ENABLED
ALWAYS_INLINE bool equal(const LChar* a, const LChar* b, unsigned length) { return !memcmp(a, b, length); }
ALWAYS_INLINE bool equal(const UChar* a, const UChar* b, unsigned length) { return !memcmp(a, b, length * sizeof(UChar)); }
#else
ALWAYS_INLINE bool equal(const LChar* a, const LChar* b, unsigned length)
{
    for (unsigned i = 0; i < length; ++i) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}
ALWAYS_INLINE bool equal(const UChar* a, const UChar* b, unsigned length)
{
    for (unsigned i = 0; i < length; ++i) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}
#endif

ALWAYS_INLINE bool equal(const LChar* a, const UChar* b, unsigned length)
{
    for (unsigned i = 0; i < length; ++i) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

ALWAYS_INLINE bool equal(const UChar* a, const LChar* b, unsigned length) { return equal(b, a, length); }

template<typename StringClassA, typename StringClassB>
ALWAYS_INLINE bool equalCommon(const StringClassA& a, const StringClassB& b, unsigned length)
{
    if (!length)
        return true;

    if (a.is8Bit()) {
        if (b.is8Bit())
            return *a.characters8() == *b.characters8() && equal(a.characters8() + 1, b.characters8() + 1, length - 1);
        return *a.characters8() == *b.characters16() && equal(a.characters8() + 1, b.characters16() + 1, length - 1);
    }

    if (b.is8Bit())
        return *a.characters16() == *b.characters8() && equal(a.characters16() + 1, b.characters8() + 1, length - 1);
    return *a.characters16() == *b.characters16() && equal(a.characters16() + 1, b.characters16() + 1, length - 1);
}

template<typename StringClassA, typename StringClassB>
ALWAYS_INLINE bool equalCommon(const StringClassA& a, const StringClassB& b)
{
    unsigned length = a.length();
    if (length != b.length())
        return false;

    return equalCommon(a, b, length);
}

template<typename StringClassA, typename StringClassB>
ALWAYS_INLINE bool equalCommon(const StringClassA* a, const StringClassB* b)
{
    if (a == b)
        return true;
    if (!a || !b)
        return false;
    return equal(*a, *b);
}

template<typename StringClass, unsigned length> bool equal(const StringClass& a, const UChar (&codeUnits)[length])
{
    if (a.length() != length)
        return false;

    if (a.is8Bit())
        return equal(a.characters8(), codeUnits, length);

    return equal(a.characters16(), codeUnits, length);
}

template<typename CharacterTypeA, typename CharacterTypeB>
inline bool equalIgnoringASCIICase(const CharacterTypeA* a, const CharacterTypeB* b, unsigned length)
{
    for (unsigned i = 0; i < length; ++i) {
        if (toASCIILower(a[i]) != toASCIILower(b[i]))
            return false;
    }
    return true;
}

template<typename CharacterTypeA, typename CharacterTypeB> inline bool equalIgnoringASCIICase(const CharacterTypeA* a, unsigned lengthA, const CharacterTypeB* b, unsigned lengthB)
{
    return lengthA == lengthB && equalIgnoringASCIICase(a, b, lengthA);
}

template<typename StringClassA, typename StringClassB>
bool equalIgnoringASCIICaseCommon(const StringClassA& a, const StringClassB& b)
{
    unsigned length = a.length();
    if (length != b.length())
        return false;

    if (a.is8Bit()) {
        if (b.is8Bit())
            return equalIgnoringASCIICase(a.characters8(), b.characters8(), length);

        return equalIgnoringASCIICase(a.characters8(), b.characters16(), length);
    }

    if (b.is8Bit())
        return equalIgnoringASCIICase(a.characters16(), b.characters8(), length);

    return equalIgnoringASCIICase(a.characters16(), b.characters16(), length);
}

template<typename StringClassA> bool equalIgnoringASCIICaseCommon(const StringClassA& a, const char* b)
{
    unsigned length = a.length();
    if (length != strlen(b))
        return false;

    if (a.is8Bit())
        return equalIgnoringASCIICase(a.characters8(), b, length);

    return equalIgnoringASCIICase(a.characters16(), b, length);
}

template <typename SearchCharacterType, typename MatchCharacterType>
size_t findIgnoringASCIICase(const SearchCharacterType* source, const MatchCharacterType* matchCharacters, unsigned startOffset, unsigned searchLength, unsigned matchLength)
{
    ASSERT(searchLength >= matchLength);

    const SearchCharacterType* startSearchedCharacters = source + startOffset;

    // delta is the number of additional times to test; delta == 0 means test only once.
    unsigned delta = searchLength - matchLength;

    for (unsigned i = 0; i <= delta; ++i) {
        if (equalIgnoringASCIICase(startSearchedCharacters + i, matchCharacters, matchLength))
            return startOffset + i;
    }
    return notFound;
}

inline size_t findIgnoringASCIICaseWithoutLength(const char* source, const char* matchCharacters)
{
    unsigned searchLength = strlen(source);
    unsigned matchLength = strlen(matchCharacters);

    return matchLength <= searchLength ? findIgnoringASCIICase(source, matchCharacters, 0, searchLength, matchLength) : notFound;
}

template <typename SearchCharacterType, typename MatchCharacterType>
ALWAYS_INLINE static size_t findInner(const SearchCharacterType* searchCharacters, const MatchCharacterType* matchCharacters, unsigned index, unsigned searchLength, unsigned matchLength)
{
    // Optimization: keep a running hash of the strings,
    // only call equal() if the hashes match.

    // delta is the number of additional times to test; delta == 0 means test only once.
    unsigned delta = searchLength - matchLength;

    unsigned searchHash = 0;
    unsigned matchHash = 0;

    for (unsigned i = 0; i < matchLength; ++i) {
        searchHash += searchCharacters[i];
        matchHash += matchCharacters[i];
    }

    unsigned i = 0;
    // keep looping until we match
    while (searchHash != matchHash || !equal(searchCharacters + i, matchCharacters, matchLength)) {
        if (i == delta)
            return notFound;
        searchHash += searchCharacters[i + matchLength];
        searchHash -= searchCharacters[i];
        ++i;
    }
    return index + i;
}

ALWAYS_INLINE const uint8_t* find8(const uint8_t* pointer, uint8_t character, size_t length)
{
    constexpr size_t thresholdLength = 16;

    size_t index = 0;
    size_t runway = std::min(thresholdLength, length);
    for (; index < runway; ++index) {
        if (pointer[index] == character)
            return pointer + index;
    }
    if (runway == length)
        return nullptr;

    ASSERT(index < length);
    // We rely on memchr already having SIMD optimization, so we don’t have to write our own.
    return static_cast<const uint8_t*>(memchr(pointer + index, character, length - index));
}

#if CPU(ARM64)
WTF_EXPORT_PRIVATE const uint16_t* find16AlignedImpl(const uint16_t* pointer, uint16_t character, size_t length);

ALWAYS_INLINE const uint16_t* find16(const uint16_t* pointer, uint16_t character, size_t length)
{
    // We take `size_t` length instead of `unsigned` because,
    // 1. It is aligned to memchr.
    // 2. It allows us to use find16 for 4GB~ vectors, which can be used in JSC ArrayBuffer (4GB wasm memory).

    // If the pointer is unaligned to 16bit access, then SIMD implementation does not work. But ARM64 allows unaligned access.
    // Fallback to a simple implementation. We also use it for smaller memory where length is less than 16.
    constexpr size_t thresholdLength = 32;
    static_assert(!(thresholdLength % (16 / sizeof(uint16_t))), "length threshold should be16-byte aligned to make find16AlignedImpl simpler");

    // For first check `threshold - (unaligned >> 1)` characters, we use normal loop.
    // This can (1) align pointer to 16-byte size so that SIMD loop gets simpler and (2) handle cases
    // having a character in the beginning of the string efficiently.
    uintptr_t unaligned = reinterpret_cast<uintptr_t>(pointer) & 0xf;

    size_t index = 0;
    size_t runway = std::min(thresholdLength - (unaligned / sizeof(uint16_t)), length);
    for (; index < runway; ++index) {
        if (pointer[index] == character)
            return pointer + index;
    }
    if (runway == length)
        return nullptr;

    ASSERT(index < length);
    return find16AlignedImpl(pointer + index, character, length - index);
}
#else
ALWAYS_INLINE const uint16_t* find16(const uint16_t* pointer, uint16_t character, size_t length)
{
    for (size_t index = 0; index < length; ++index) {
        if (pointer[index] == character)
            return pointer + index;
    }
    return nullptr;
}
#endif

#if CPU(ARM64)
WTF_EXPORT_PRIVATE const uint32_t* find32AlignedImpl(const uint32_t* pointer, uint32_t character, size_t length);

ALWAYS_INLINE const uint32_t* find32(const uint32_t* pointer, uint32_t character, size_t length)
{
    constexpr size_t thresholdLength = 32;
    static_assert(!(thresholdLength % (16 / sizeof(uint32_t))), "it should be 16-byte aligned to make find32AlignedImpl simpler");

    uintptr_t unaligned = reinterpret_cast<uintptr_t>(pointer) & 0xf;

    size_t index = 0;
    size_t runway = std::min(thresholdLength - (unaligned / sizeof(uint32_t)), length);
    for (; index < runway; ++index) {
        if (pointer[index] == character)
            return pointer + index;
    }
    if (runway == length)
        return nullptr;

    ASSERT(index < length);
    return find32AlignedImpl(pointer + index, character, length - index);
}
#else
ALWAYS_INLINE const uint32_t* find32(const uint32_t* pointer, uint32_t character, size_t length)
{
    for (size_t index = 0; index < length; ++index) {
        if (pointer[index] == character)
            return pointer + index;
    }
    return nullptr;
}
#endif

#if CPU(ARM64)
WTF_EXPORT_PRIVATE const uint64_t* find64AlignedImpl(const uint64_t* pointer, uint64_t character, size_t length);

ALWAYS_INLINE const uint64_t* find64(const uint64_t* pointer, uint64_t character, size_t length)
{
    constexpr size_t thresholdLength = 32;
    static_assert(!(thresholdLength % (16 / sizeof(uint64_t))), "length threshold should be16-byte aligned to make find64AlignedImpl simpler");

    uintptr_t unaligned = reinterpret_cast<uintptr_t>(pointer) & 0xf;

    size_t index = 0;
    size_t runway = std::min(thresholdLength - (unaligned / sizeof(uint64_t)), length);
    for (; index < runway; ++index) {
        if (pointer[index] == character)
            return pointer + index;
    }
    if (runway == length)
        return nullptr;

    ASSERT(index < length);
    return find64AlignedImpl(pointer + index, character, length - index);
}
#else
ALWAYS_INLINE const uint64_t* find64(const uint64_t* pointer, uint64_t character, size_t length)
{
    for (size_t index = 0; index < length; ++index) {
        if (pointer[index] == character)
            return pointer + index;
    }
    return nullptr;
}
#endif

#if CPU(ARM64)
WTF_EXPORT_PRIVATE const float* findFloatAlignedImpl(const float* pointer, float target, size_t length);

ALWAYS_INLINE const float* findFloat(const float* pointer, float target, size_t length)
{
    constexpr size_t thresholdLength = 32;
    static_assert(!(thresholdLength % (16 / sizeof(float))), "length threshold should be16-byte aligned to make floatFindAlignedImpl simpler");

    uintptr_t unaligned = reinterpret_cast<uintptr_t>(pointer) & 0xf;

    size_t index = 0;
    size_t runway = std::min(thresholdLength - (unaligned / sizeof(float)), length);
    for (; index < runway; ++index) {
        if (pointer[index] == target)
            return pointer + index;
    }
    if (runway == length)
        return nullptr;

    ASSERT(index < length);
    return findFloatAlignedImpl(pointer + index, target, length - index);
}
#else
ALWAYS_INLINE const float* findFloat(const float* pointer, float target, size_t length)
{
    for (size_t index = 0; index < length; ++index) {
        if (pointer[index] == target)
            return pointer + index;
    }
    return nullptr;
}
#endif

#if CPU(ARM64)
WTF_EXPORT_PRIVATE const double* findDoubleAlignedImpl(const double* pointer, double target, size_t length);

ALWAYS_INLINE const double* findDouble(const double* pointer, double target, size_t length)
{
    constexpr size_t thresholdLength = 32;
    static_assert(!(thresholdLength % (16 / sizeof(double))), "length threshold should be16-byte aligned to make doubleFindAlignedImpl simpler");

    uintptr_t unaligned = reinterpret_cast<uintptr_t>(pointer) & 0xf;

    size_t index = 0;
    size_t runway = std::min(thresholdLength - (unaligned / sizeof(double)), length);
    for (; index < runway; ++index) {
        if (pointer[index] == target)
            return pointer + index;
    }
    if (runway == length)
        return nullptr;

    ASSERT(index < length);
    return findDoubleAlignedImpl(pointer + index, target, length - index);
}
#else
ALWAYS_INLINE const double* findDouble(const double* pointer, double target, size_t length)
{
    for (size_t index = 0; index < length; ++index) {
        if (pointer[index] == target)
            return pointer + index;
    }
    return nullptr;
}
#endif

#if CPU(ARM64)
WTF_EXPORT_PRIVATE const LChar* find8NonASCIIAlignedImpl(const LChar* pointer, size_t length);

ALWAYS_INLINE const LChar* find8NonASCII(const LChar* pointer, size_t length)
{
    constexpr size_t thresholdLength = 16;
    static_assert(!(thresholdLength % (16 / sizeof(LChar))), "length threshold should be 16-byte aligned to make find8NonASCIIAlignedImpl simpler");
    uintptr_t unaligned = reinterpret_cast<uintptr_t>(pointer) & 0xf;

    size_t index = 0;
    size_t runway = std::min(thresholdLength - (unaligned / sizeof(LChar)), length);
    for (; index < runway; ++index) {
        if (!isASCII(pointer[index]))
            return pointer + index;
    }
    if (runway == length)
        return nullptr;

    ASSERT(index < length);
    return find8NonASCIIAlignedImpl(pointer + index, length - index);
}

WTF_EXPORT_PRIVATE const UChar* find16NonASCIIAlignedImpl(const UChar* pointer, size_t length);

ALWAYS_INLINE const UChar* find16NonASCII(const UChar* pointer, size_t length)
{
    constexpr size_t thresholdLength = 16;
    static_assert(!(thresholdLength % (16 / sizeof(UChar))), "length threshold should be 16-byte aligned to make find16NonASCIIAlignedImpl simpler");
    uintptr_t unaligned = reinterpret_cast<uintptr_t>(pointer) & 0xf;

    size_t index = 0;
    size_t runway = std::min(thresholdLength - (unaligned / sizeof(UChar)), length);
    for (; index < runway; ++index) {
        if (!isASCII(pointer[index]))
            return pointer + index;
    }
    if (runway == length)
        return nullptr;

    ASSERT(index < length);
    return find16NonASCIIAlignedImpl(pointer + index, length - index);
}
#endif

template<typename CharacterType, std::enable_if_t<std::is_integral_v<CharacterType>>* = nullptr>
inline size_t find(const CharacterType* characters, unsigned length, CharacterType matchCharacter, unsigned index = 0)
{
    if constexpr (sizeof(CharacterType) == 1) {
        if (index >= length)
            return notFound;
        auto* result = reinterpret_cast<const CharacterType*>(find8(bitwise_cast<const uint8_t*>(characters + index), matchCharacter, length - index));
        ASSERT(!result || static_cast<unsigned>(result - characters) >= index);
        if (result)
            return result - characters;
        return notFound;
    }

    if constexpr (sizeof(CharacterType) == 2) {
        if (index >= length)
            return notFound;
        auto* result = reinterpret_cast<const CharacterType*>(find16(bitwise_cast<const uint16_t*>(characters + index), matchCharacter, length - index));
        ASSERT(!result || static_cast<unsigned>(result - characters) >= index);
        if (result)
            return result - characters;
        return notFound;
    }

    while (index < length) {
        if (characters[index] == matchCharacter)
            return index;
        ++index;
    }
    return notFound;
}

ALWAYS_INLINE size_t find(const UChar* characters, unsigned length, LChar matchCharacter, unsigned index = 0)
{
    return find(characters, length, static_cast<UChar>(matchCharacter), index);
}

inline size_t find(const LChar* characters, unsigned length, UChar matchCharacter, unsigned index = 0)
{
    if (!isLatin1(matchCharacter))
        return notFound;
    return find(characters, length, static_cast<LChar>(matchCharacter), index);
}

template <typename SearchCharacterType, typename MatchCharacterType>
ALWAYS_INLINE static size_t reverseFindInner(const SearchCharacterType* searchCharacters, const MatchCharacterType* matchCharacters, unsigned start, unsigned length, unsigned matchLength)
{
    // Optimization: keep a running hash of the strings,
    // only call equal if the hashes match.

    // delta is the number of additional times to test; delta == 0 means test only once.
    unsigned delta = std::min(start, length - matchLength);

    unsigned searchHash = 0;
    unsigned matchHash = 0;
    for (unsigned i = 0; i < matchLength; ++i) {
        searchHash += searchCharacters[delta + i];
        matchHash += matchCharacters[i];
    }

    // keep looping until we match
    while (searchHash != matchHash || !equal(searchCharacters + delta, matchCharacters, matchLength)) {
        if (!delta)
            return notFound;
        --delta;
        searchHash -= searchCharacters[delta + matchLength];
        searchHash += searchCharacters[delta];
    }
    return delta;
}

// This is marked inline since it's mostly used in non-inline functions for each string type.
// When used directly in code it's probably OK to be inline; maybe the loop will be unrolled.
template<typename CharacterType> inline bool equalLettersIgnoringASCIICase(const CharacterType* characters, const char* lowercaseLetters, unsigned length)
{
    for (unsigned i = 0; i < length; ++i) {
        if (!isASCIIAlphaCaselessEqual(characters[i], lowercaseLetters[i]))
            return false;
    }
    return true;
}

template<typename CharacterType> inline bool equalLettersIgnoringASCIICase(const CharacterType* characters, unsigned charactersLength, ASCIILiteral literal)
{
    unsigned literalLength = literal.length();
    return charactersLength == literalLength && equalLettersIgnoringASCIICase(characters, literal.characters(), literalLength);
}

template<typename StringClass> bool inline hasPrefixWithLettersIgnoringASCIICaseCommon(const StringClass& string, const char* lowercaseLetters, unsigned length)
{
#if ASSERT_ENABLED
    ASSERT(*lowercaseLetters);
    for (const char* letter = lowercaseLetters; *letter; ++letter)
        ASSERT(toASCIILowerUnchecked(*letter) == *letter);
#endif
    ASSERT(string.length() >= length);

    if (string.is8Bit())
        return equalLettersIgnoringASCIICase(string.characters8(), lowercaseLetters, length);
    return equalLettersIgnoringASCIICase(string.characters16(), lowercaseLetters, length);
}

// This is intentionally not marked inline because it's used often and is not speed-critical enough to want it inlined everywhere.
template<typename StringClass> bool equalLettersIgnoringASCIICaseCommon(const StringClass& string, const char* literal, unsigned literalLength)
{
    unsigned length = string.length();
    if (length != literalLength)
        return false;
    return hasPrefixWithLettersIgnoringASCIICaseCommon(string, literal, length);
}

template<typename StringClass> bool startsWithLettersIgnoringASCIICaseCommon(const StringClass& string, const char* prefix, unsigned prefixLength)
{
    if (!prefixLength)
        return true;
    if (string.length() < prefixLength)
        return false;
    return hasPrefixWithLettersIgnoringASCIICaseCommon(string, prefix, prefixLength);
}

template<typename StringClass> inline bool equalLettersIgnoringASCIICaseCommon(const StringClass& string, ASCIILiteral literal)
{
    return equalLettersIgnoringASCIICaseCommon(string, literal.characters(), literal.length());
}

template<typename StringClass> inline bool startsWithLettersIgnoringASCIICaseCommon(const StringClass& string, ASCIILiteral literal)
{
    return startsWithLettersIgnoringASCIICaseCommon(string, literal.characters(), literal.length());
}

inline bool equalIgnoringASCIICase(const char* a, const char* b)
{
    auto length = strlen(a);
    return length == strlen(b) && equalIgnoringASCIICase(a, b, length);
}

inline bool equalLettersIgnoringASCIICase(ASCIILiteral a, ASCIILiteral b)
{
    auto bLength = b.length();
    return a.length() == bLength && equalLettersIgnoringASCIICase(a.characters(), b.characters(), bLength);
}

inline bool equalLettersIgnoringASCIICase(const char* string, ASCIILiteral literal)
{
    auto literalLength = literal.length();
    return strlen(string) == literalLength && equalLettersIgnoringASCIICase(string, literal.characters(), literalLength);
}

inline bool equalIgnoringASCIICase(const char* string, ASCIILiteral literal)
{
    auto literalLength = literal.length();
    return strlen(string) == literal.length() && equalIgnoringASCIICase(string, literal.characters(), literalLength);
}

inline bool equalIgnoringASCIICase(ASCIILiteral a, ASCIILiteral b)
{
    return equalIgnoringASCIICase(a.characters(), a.length(), b.characters(), b.length());
}

template<typename ElementType>
inline void copyElements(ElementType* __restrict destination, const ElementType* __restrict source, size_t length)
{
    if (length == 1)
        *destination = *source;
    else if (length)
        std::memcpy(destination, source, length * sizeof(ElementType));
}

inline void copyElements(uint16_t* __restrict destination, const uint8_t* __restrict source, size_t length)
{
#if CPU(ARM64)
    // SIMD Upconvert.
    const auto* end = destination + length;
    constexpr uintptr_t memoryAccessSize = 64;

    if (length >= memoryAccessSize) {
        constexpr uintptr_t memoryAccessMask = memoryAccessSize - 1;
        const auto* simdEnd = destination + (length & ~memoryAccessMask);
        uint8x16_t zeros = vdupq_n_u8(0);
        do {
            uint8x16x4_t bytes = vld1q_u8_x4(bitwise_cast<const uint8_t*>(source));
            source += memoryAccessSize;

            vst2q_u8(bitwise_cast<uint8_t*>(destination), (uint8x16x2_t { bytes.val[0], zeros }));
            destination += memoryAccessSize / 4;
            vst2q_u8(bitwise_cast<uint8_t*>(destination), (uint8x16x2_t { bytes.val[1], zeros }));
            destination += memoryAccessSize / 4;
            vst2q_u8(bitwise_cast<uint8_t*>(destination), (uint8x16x2_t { bytes.val[2], zeros }));
            destination += memoryAccessSize / 4;
            vst2q_u8(bitwise_cast<uint8_t*>(destination), (uint8x16x2_t { bytes.val[3], zeros }));
            destination += memoryAccessSize / 4;
        } while (destination != simdEnd);
    }

    while (destination != end)
        *destination++ = *source++;
#else
    for (unsigned i = 0; i < length; ++i)
        destination[i] = source[i];
#endif
}

inline void copyElements(uint8_t* __restrict destination, const uint16_t* __restrict source, size_t length)
{
#if CPU(X86_SSE2)
    const uintptr_t memoryAccessSize = 16; // Memory accesses on 16 byte (128 bit) alignment
    const uintptr_t memoryAccessMask = memoryAccessSize - 1;

    unsigned i = 0;
    for (; i < length && !isAlignedTo<memoryAccessMask>(&source[i]); ++i)
        destination[i] = source[i];

    const uintptr_t sourceLoadSize = 32; // Process 32 bytes (16 uint16_ts) each iteration
    const unsigned ucharsPerLoop = sourceLoadSize / sizeof(uint16_t);
    if (length > ucharsPerLoop) {
        const unsigned endLength = length - ucharsPerLoop + 1;
        for (; i < endLength; i += ucharsPerLoop) {
            __m128i first8Uint16s = _mm_load_si128(reinterpret_cast<const __m128i*>(&source[i]));
            __m128i second8Uint16s = _mm_load_si128(reinterpret_cast<const __m128i*>(&source[i+8]));
            __m128i packedChars = _mm_packus_epi16(first8Uint16s, second8Uint16s);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&destination[i]), packedChars);
        }
    }

    for (; i < length; ++i)
        destination[i] = source[i];
#elif COMPILER(GCC_COMPATIBLE) && CPU(ARM64) && CPU(ADDRESS64) && !ASSERT_ENABLED
    const uint8_t* const end = destination + length;
    const uintptr_t memoryAccessSize = 16;

    if (length >= memoryAccessSize) {
        const uintptr_t memoryAccessMask = memoryAccessSize - 1;

        // Vector interleaved unpack, we only store the lower 8 bits.
        const uintptr_t lengthLeft = end - destination;
        const uint8_t* const simdEnd = destination + (lengthLeft & ~memoryAccessMask);
        do {
            asm("ld2   { v0.16B, v1.16B }, [%[SOURCE]], #32\n\t"
                "st1   { v0.16B }, [%[DESTINATION]], #16\n\t"
                : [SOURCE]"+r" (source), [DESTINATION]"+r" (destination)
                :
                : "memory", "v0", "v1");
        } while (destination != simdEnd);
    }

    while (destination != end)
        *destination++ = static_cast<uint8_t>(*source++);
#elif COMPILER(GCC_COMPATIBLE) && CPU(ARM_NEON) && !(CPU(BIG_ENDIAN) || CPU(MIDDLE_ENDIAN)) && !ASSERT_ENABLED
    const uint8_t* const end = destination + length;
    const uintptr_t memoryAccessSize = 8;

    if (length >= (2 * memoryAccessSize) - 1) {
        // Prefix: align dst on 64 bits.
        const uintptr_t memoryAccessMask = memoryAccessSize - 1;
        while (!isAlignedTo<memoryAccessMask>(destination))
            *destination++ = static_cast<uint8_t>(*source++);

        // Vector interleaved unpack, we only store the lower 8 bits.
        const uintptr_t lengthLeft = end - destination;
        const uint8_t* const simdEnd = end - (lengthLeft % memoryAccessSize);
        do {
            asm("vld2.8   { d0-d1 }, [%[SOURCE]] !\n\t"
                "vst1.8   { d0 }, [%[DESTINATION],:64] !\n\t"
                : [SOURCE]"+r" (source), [DESTINATION]"+r" (destination)
                :
                : "memory", "d0", "d1");
        } while (destination != simdEnd);
    }

    while (destination != end)
        *destination++ = static_cast<uint8_t>(*source++);
#else
    for (unsigned i = 0; i < length; ++i)
        destination[i] = static_cast<uint8_t>(source[i]);
#endif
}

inline void copyElements(uint16_t* __restrict destination, const uint32_t* __restrict source, size_t length)
{
    const auto* end = destination + length;
#if CPU(ARM64) && CPU(ADDRESS64)
    const uintptr_t memoryAccessSize = 32 / sizeof(uint32_t);
    if (length >= memoryAccessSize) {
        const uintptr_t memoryAccessMask = memoryAccessSize - 1;
        const uintptr_t lengthLeft = end - destination;
        const auto* const simdEnd = destination + (lengthLeft & ~memoryAccessMask);
        // Use ld2 to load lower 16bit of 8 uint32_t.
        do {
            asm("ld2   { v0.8H, v1.8H }, [%[SOURCE]], #32\n\t"
                "st1   { v0.8H }, [%[DESTINATION]], #16\n\t"
                : [SOURCE]"+r" (source), [DESTINATION]"+r" (destination)
                :
                : "memory", "v0", "v1");
        } while (destination != simdEnd);
    }
#endif
    while (destination != end)
        *destination++ = *source++;
}

inline void copyElements(uint32_t* __restrict destination, const uint64_t* __restrict source, size_t length)
{
    const auto* end = destination + length;
#if CPU(ARM64) && CPU(ADDRESS64)
    const uintptr_t memoryAccessSize = 32 / sizeof(uint64_t);
    if (length >= memoryAccessSize) {
        const uintptr_t memoryAccessMask = memoryAccessSize - 1;
        const uintptr_t lengthLeft = end - destination;
        const auto* const simdEnd = destination + (lengthLeft & ~memoryAccessMask);
        // Use ld2 to load lower 32bit of 4 uint64_t.
        do {
            asm("ld2   { v0.4S, v1.4S }, [%[SOURCE]], #32\n\t"
                "st1   { v0.4S }, [%[DESTINATION]], #16\n\t"
                : [SOURCE]"+r" (source), [DESTINATION]"+r" (destination)
                :
                : "memory", "v0", "v1");
        } while (destination != simdEnd);
    }
#endif
    while (destination != end)
        *destination++ = *source++;
}

inline void copyElements(uint16_t* __restrict destination, const uint64_t* __restrict source, size_t length)
{
    const auto* end = destination + length;
#if CPU(ARM64) && CPU(ADDRESS64)
    const uintptr_t memoryAccessSize = 64 / sizeof(uint64_t);
    if (length >= memoryAccessSize) {
        const uintptr_t memoryAccessMask = memoryAccessSize - 1;
        const uintptr_t lengthLeft = end - destination;
        const auto* const simdEnd = destination + (lengthLeft & ~memoryAccessMask);
        // Use ld4 to load lower 16bit of 8 uint64_t.
        do {
            asm("ld4   { v0.8H, v1.8H, v2.8H, v3.8H }, [%[SOURCE]], #64\n\t"
                "st1   { v0.8H }, [%[DESTINATION]], #16\n\t"
                : [SOURCE]"+r" (source), [DESTINATION]"+r" (destination)
                :
                : "memory", "v0", "v1", "v2", "v3");
        } while (destination != simdEnd);
    }
#endif
    while (destination != end)
        *destination++ = *source++;
}

inline void copyElements(uint8_t* __restrict destination, const uint64_t* __restrict source, size_t length)
{
    const auto* end = destination + length;
#if CPU(ARM64) && CPU(ADDRESS64)
    const uintptr_t memoryAccessSize = 64 / sizeof(uint64_t);
    if (length >= memoryAccessSize) {
        const uintptr_t memoryAccessMask = memoryAccessSize - 1;
        const uintptr_t lengthLeft = end - destination;
        const auto* const simdEnd = destination + (lengthLeft & ~memoryAccessMask);
        // Since ARM64 does not ld8, we use ld4 to load lower 16bit of 8 uint64_t.
        // And then narrow 8 16bit lanes into 8 8bit lanes and store it to the destination.
        do {
            asm("ld4   { v0.8H, v1.8H, v2.8H, v3.8H }, [%[SOURCE]], #64\n\t"
                "xtn   v0.8B, v0.8H\n\t"
                "st1   { v0.8B }, [%[DESTINATION]], #8\n\t"
                : [SOURCE]"+r" (source), [DESTINATION]"+r" (destination)
                :
                : "memory", "v0", "v1", "v2", "v3");
        } while (destination != simdEnd);
    }
#endif
    while (destination != end)
        *destination++ = *source++;
}

}

using WTF::equalIgnoringASCIICase;
using WTF::equalLettersIgnoringASCIICase;
using WTF::isLatin1;
