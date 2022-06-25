/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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
#include <wtf/text/ASCIILiteral.h>

namespace WTF {

template<typename CharacterType> inline bool isLatin1(CharacterType character)
{
    using UnsignedCharacterType = typename std::make_unsigned<CharacterType>::type;
    return static_cast<UnsignedCharacterType>(character) <= static_cast<UnsignedCharacterType>(0xFF);
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
ALWAYS_INLINE bool equalCommon(const StringClassA& a, const StringClassB& b)
{
    unsigned length = a.length();
    if (length != b.length())
        return false;

    if (a.is8Bit()) {
        if (b.is8Bit())
            return equal(a.characters8(), b.characters8(), length);

        return equal(a.characters8(), b.characters16(), length);
    }

    if (b.is8Bit())
        return equal(a.characters16(), b.characters8(), length);

    return equal(a.characters16(), b.characters16(), length);
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

    return matchLength < searchLength ? findIgnoringASCIICase(source, matchCharacters, 0, searchLength, matchLength) : notFound;
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

template<typename CharacterType, std::enable_if_t<std::is_integral_v<CharacterType>>* = nullptr>
inline size_t find(const CharacterType* characters, unsigned length, CharacterType matchCharacter, unsigned index = 0)
{
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

}

using WTF::equalIgnoringASCIICase;
using WTF::equalLettersIgnoringASCIICase;
using WTF::isLatin1;
