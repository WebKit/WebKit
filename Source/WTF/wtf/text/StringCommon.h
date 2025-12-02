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
#include <concepts>
#include <unicode/uchar.h>
#include <wtf/ASCIICType.h>
#include <wtf/Float16.h>
#include <wtf/MathExtras.h>
#include <wtf/NotFound.h>
#include <wtf/SIMDHelpers.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UnalignedAccess.h>
#include <wtf/text/ASCIIFastPath.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/unicode/UTF8Conversion.h>

namespace WTF {

inline std::span<const Latin1Character> span(const Latin1Character& character)
{
    return unsafeMakeSpan(&character, 1);
}

inline std::span<const char16_t> span(const char16_t& character)
{
    return unsafeMakeSpan(&character, 1);
}

inline std::span<const char> unsafeSpan(const char* string)
{
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    return unsafeMakeSpan(string, string ? strlen(string) : 0);
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

inline std::span<const char> unsafeSpanIncludingNullTerminator(const char* string)
{
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    return unsafeMakeSpan(string, string ? strlen(string) + 1 : 0);
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
inline std::span<const char16_t> unsafeSpan(const char16_t* string)
{
    if (!string)
        return { };
    size_t length = 0;
    while (string[length])
        ++length;
    return unsafeMakeSpan(string, length);
}
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#if !HAVE(MISSING_U8STRING)
inline std::span<const char8_t> span(const std::u8string& string)
{
    return unsafeMakeSpan(string.data(), string.length());
}
#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

template<typename T, std::size_t Extent>
size_t strlenSpan(std::span<T, Extent> span) requires(sizeof(T) == 1)
{
    size_t i = 0;
    while (span[i] != '\0')
        ++i;
    return i;
}

template<typename CharacterType> inline constexpr bool isLatin1(CharacterType character)
{
    return unsignedCast(character) <= 0xFFu;
}

template<> ALWAYS_INLINE constexpr bool isLatin1(Latin1Character)
{
    return true;
}

using CodeUnitMatchFunction = bool (*)(char16_t);

template<typename CharacterTypeA, typename CharacterTypeB>
    requires(TriviallyComparableCodeUnits<CharacterTypeA, CharacterTypeB>)
bool equalIgnoringASCIICase(std::span<const CharacterTypeA>, std::span<const CharacterTypeB>);

template<typename StringClassA, typename StringClassB> bool equalIgnoringASCIICaseCommon(const StringClassA&, const StringClassB&);

template<typename CharacterType> bool equalLettersIgnoringASCIICase(std::span<const CharacterType>, std::span<const Latin1Character> lowercaseLetters);
template<typename CharacterType> bool equalLettersIgnoringASCIICase(std::span<const CharacterType>, ASCIILiteral);

template<typename StringClass> bool equalLettersIgnoringASCIICaseCommon(const StringClass&, ASCIILiteral);

bool equalIgnoringASCIICase(const char*, const char*);

template<typename T>
concept OneByteCharacterType = std::is_same_v<std::remove_const_t<T>, Latin1Character> || std::is_same_v<std::remove_const_t<T>, char8_t> || std::is_same_v<std::remove_const_t<T>, char>;

// Do comparisons 8 or 4 bytes-at-a-time on architectures where it's safe.
#if (CPU(X86_64) || CPU(ARM64)) && !ASAN_ENABLED
template<OneByteCharacterType CharacterType>
ALWAYS_INLINE bool equal(const CharacterType* a, std::span<const CharacterType> b)
{
    ASSERT(b.size() <= std::numeric_limits<unsigned>::max());
    unsigned length = b.size();

    // These branches could be combined into one, but it's measurably faster
    // for length 0 or 1 strings to separate them out like this.
    if (!length)
        return true;
    if (length == 1)
        return *a == b.front();

    switch (sizeof(unsigned) * CHAR_BIT - clz(length - 1)) { // Works as really fast log2, since length != 0.
    case 0:
        RELEASE_ASSERT_NOT_REACHED();
    case 1: // Length is 2.
        return unalignedLoad<uint16_t>(a) == unalignedLoad<uint16_t>(b.data());
    case 2: // Length is 3 or 4.
        return unalignedLoad<uint16_t>(a) == unalignedLoad<uint16_t>(b.data())
            && unalignedLoad<uint16_t>(a + length - 2) == unalignedLoad<uint16_t>(b.data() + length - 2);
    case 3: // Length is between 5 and 8 inclusive.
        return unalignedLoad<uint32_t>(a) == unalignedLoad<uint32_t>(b.data())
            && unalignedLoad<uint32_t>(a + length - 4) == unalignedLoad<uint32_t>(b.data() + length - 4);
    case 4: // Length is between 9 and 16 inclusive.
        return unalignedLoad<uint64_t>(a) == unalignedLoad<uint64_t>(b.data())
            && unalignedLoad<uint64_t>(a + length - 8) == unalignedLoad<uint64_t>(b.data() + length - 8);
#if CPU(ARM64)
    case 5: // Length is between 17 and 32 inclusive.
        return vminvq_u8(vandq_u8(
            vceqq_u8(unalignedLoad<uint8x16_t>(a), unalignedLoad<uint8x16_t>(b.data())),
            vceqq_u8(unalignedLoad<uint8x16_t>(a + length - 16), unalignedLoad<uint8x16_t>(b.data() + length - 16))
        ));
    default: // Length is longer than 32 bytes.
        if (!vminvq_u8(vceqq_u8(unalignedLoad<uint8x16_t>(a), unalignedLoad<uint8x16_t>(b.data()))))
            return false;
        for (unsigned i = length % 16; i < length; i += 16) {
            if (!vminvq_u8(vceqq_u8(unalignedLoad<uint8x16_t>(a + i), unalignedLoad<uint8x16_t>(b.data() + i))))
                return false;
        }
        return true;
#else
    default: // Length is longer than 16 bytes.
        if (unalignedLoad<uint64_t>(a) != unalignedLoad<uint64_t>(b.data()))
            return false;
        for (unsigned i = length % 8; i < length; i += 8) {
            if (unalignedLoad<uint64_t>(a + i) != unalignedLoad<uint64_t>(b.data() + i))
                return false;
        }
        return true;
#endif
    }
}

ALWAYS_INLINE bool equal(const char16_t* a, std::span<const char16_t> b)
{
    ASSERT(b.size() <= std::numeric_limits<unsigned>::max());
    unsigned length = b.size();

    if (!length)
        return true;
    if (length == 1)
        return *a == b.front();

    switch (sizeof(unsigned) * CHAR_BIT - clz(length - 1)) { // Works as really fast log2, since length != 0.
    case 0:
        RELEASE_ASSERT_NOT_REACHED();
    case 1: // Length is 2 (4 bytes).
        return unalignedLoad<uint32_t>(a) == unalignedLoad<uint32_t>(b.data());
    case 2: // Length is 3 or 4 (6-8 bytes).
        return unalignedLoad<uint32_t>(a) == unalignedLoad<uint32_t>(b.data())
            && unalignedLoad<uint32_t>(a + length - 2) == unalignedLoad<uint32_t>(b.data() + length - 2);
    case 3: // Length is between 5 and 8 inclusive (10-16 bytes).
        return unalignedLoad<uint64_t>(a) == unalignedLoad<uint64_t>(b.data())
            && unalignedLoad<uint64_t>(a + length - 4) == unalignedLoad<uint64_t>(b.data() + length - 4);
#if CPU(ARM64)
    case 4: // Length is between 9 and 16 inclusive (18-32 bytes).
        return vminvq_u16(vandq_u16(
            vceqq_u16(unalignedLoad<uint16x8_t>(a), unalignedLoad<uint16x8_t>(b.data())),
            vceqq_u16(unalignedLoad<uint16x8_t>(a + length - 8), unalignedLoad<uint16x8_t>(b.data() + length - 8))
        ));
    default: // Length is longer than 16 (32 bytes).
        if (!vminvq_u16(vceqq_u16(unalignedLoad<uint16x8_t>(a), unalignedLoad<uint16x8_t>(b.data()))))
            return false;
        for (unsigned i = length % 8; i < length; i += 8) {
            if (!vminvq_u16(vceqq_u16(unalignedLoad<uint16x8_t>(a + i), unalignedLoad<uint16x8_t>(b.data() + i))))
                return false;
        }
        return true;
#else
    default: // Length is longer than 8 (16 bytes).
        if (unalignedLoad<uint64_t>(a) != unalignedLoad<uint64_t>(b.data()))
            return false;
        for (unsigned i = length % 4; i < length; i += 4) {
            if (unalignedLoad<uint64_t>(a + i) != unalignedLoad<uint64_t>(b.data() + i))
                return false;
        }
        return true;
#endif
    }
}
#elif CPU(X86) && !ASAN_ENABLED
template<OneByteCharacterType CharacterType>
ALWAYS_INLINE bool equal(const CharacterType* a, std::span<const CharacterType> b)
{
    ASSERT(b.size() <= std::numeric_limits<unsigned>::max());
    unsigned length = b.size();

    const char* aString = byteCast<char>(a);
    const char* bString = byteCast<char>(b.data());

    unsigned wordLength = length >> 2;
    for (unsigned i = 0; i != wordLength; ++i) {
        if (unalignedLoad<uint32_t>(aString) != unalignedLoad<uint32_t>(bString))
            return false;
        aString += sizeof(uint32_t);
        bString += sizeof(uint32_t);
    }

    length &= 3;

    if (length) {
        auto* aRemainder = byteCast<CharacterType>(aString);
        auto* bRemainder = byteCast<CharacterType>(bString);

        for (unsigned i = 0; i < length; ++i) {
            if (aRemainder[i] != bRemainder[i])
                return false;
        }
    }

    return true;
}

ALWAYS_INLINE bool equal(const char16_t* a, std::span<const char16_t> b)
{
    ASSERT(b.size() <= std::numeric_limits<unsigned>::max());
    unsigned length = b.size();

    const char* aString = reinterpret_cast<const char*>(a);
    const char* bString = reinterpret_cast<const char*>(b.data());

    unsigned wordLength = length >> 1;
    for (unsigned i = 0; i != wordLength; ++i) {
        if (unalignedLoad<uint32_t>(aString) != unalignedLoad<uint32_t>(bString))
            return false;
        aString += sizeof(uint32_t);
        bString += sizeof(uint32_t);
    }

    if (length & 1 && *reinterpret_cast<const char16_t*>(aString) != *reinterpret_cast<const char16_t*>(bString))
        return false;

    return true;
}
#else
template<OneByteCharacterType CharacterType>
ALWAYS_INLINE bool equal(const CharacterType* a, std::span<const CharacterType> b)
{
    return !memcmp(a, b.data(), b.size());
}
ALWAYS_INLINE bool equal(const char16_t* a, std::span<const char16_t> b) { return !memcmp(a, b.data(), b.size_bytes()); }
#endif

ALWAYS_INLINE bool equal(const Latin1Character* a, std::span<const char16_t> b)
{
#if CPU(ARM64)
    ASSERT(b.size() <= std::numeric_limits<unsigned>::max());
    unsigned length = b.size();

    if (length >= 8) {
        uint16x8_t aHalves = vmovl_u8(unalignedLoad<uint8x8_t>(a)); // Extends 8 Latin1Characters into 8 UTF-16 code units.
        uint16x8_t bHalves = unalignedLoad<uint16x8_t>(b.data());
        if (!vminvq_u16(vceqq_u16(aHalves, bHalves)))
            return false;
        for (unsigned i = length % 8; i < length; i += 8) {
            aHalves = vmovl_u8(unalignedLoad<uint8x8_t>(a + i));
            bHalves = unalignedLoad<uint16x8_t>(b.data() + i);
            if (!vminvq_u16(vceqq_u16(aHalves, bHalves)))
                return false;
        }
        return true;
    }
    if (length >= 4) {
        auto read4 = [](const Latin1Character* p) ALWAYS_INLINE_LAMBDA {
            // Copy 32 bits and expand to 64 bits.
            uint32_t v32 = unalignedLoad<uint32_t>(p);
            uint64_t v64 = static_cast<uint64_t>(v32);
            v64 = (v64 | (v64 << 16)) & 0x0000ffff0000ffffULL;
            return static_cast<uint64_t>((v64 | (v64 << 8)) & 0x00ff00ff00ff00ffULL);
        };

        return static_cast<unsigned>(read4(a) == unalignedLoad<uint64_t>(b.data())) & static_cast<unsigned>(read4(a + (length % 4)) == unalignedLoad<uint64_t>(b.data() + (length % 4)));
    }
    if (length >= 2) {
        auto read2 = [](const Latin1Character* p) ALWAYS_INLINE_LAMBDA {
            // Copy 16 bits and expand to 32 bits.
            uint16_t v16 = unalignedLoad<uint16_t>(p);
            uint32_t v32 = static_cast<uint32_t>(v16);
            return static_cast<uint32_t>((v32 | (v32 << 8)) & 0x00ff00ffUL);
        };
        return static_cast<unsigned>(read2(a) == unalignedLoad<uint32_t>(b.data())) & static_cast<unsigned>(read2(a + (length % 2)) == unalignedLoad<uint32_t>(b.data() + (length % 2)));
    }
    if (length == 1)
        return *a == b.front();
    return true;
#else
    for (size_t i = 0; i < b.size(); ++i) {
        if (a[i] != b[i])
            return false;
    }
    return true;
#endif
}

ALWAYS_INLINE bool equal(const char16_t* a, std::span<const Latin1Character> b)
{
    return equal(b.data(), { a, b.size() });
}

template<OneByteCharacterType CharacterType>
ALWAYS_INLINE bool equal(std::span<const CharacterType> a, std::span<const CharacterType> b)
{
    if (a.size() != b.size())
        return false;
    return equal(a.data(), b);
}

template<OneByteCharacterType CharacterType>
ALWAYS_INLINE bool equal(std::span<const CharacterType> a, ASCIILiteral b)
{
    return equal(a, byteCast<CharacterType>(b.span()));
}

template<typename StringClassA, typename StringClassB>
ALWAYS_INLINE bool equalCommon(const StringClassA& a, const StringClassB& b, unsigned length)
{
    if (!length)
        return true;

    if (a.is8Bit()) {
        auto aSpan = a.span8();
        if (b.is8Bit()) {
            auto bSpan = b.span8();
            return aSpan.front() == bSpan.front() && equal(aSpan.data() + 1, bSpan.subspan(1));
        }
        auto bSpan = b.span16();
        return aSpan.front() == bSpan.front() && equal(aSpan.data() + 1, bSpan.subspan(1));
    }

    auto aSpan = a.span16();
    if (b.is8Bit()) {
        auto bSpan = b.span8();
        return aSpan.front() == bSpan.front() && equal(aSpan.data() + 1, bSpan.subspan(1));
    }
    auto bSpan = b.span16();
    return aSpan.front() == bSpan.front() && equal(aSpan.data() + 1, bSpan.subspan(1));
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

template<typename StringClass, unsigned length> bool equal(const StringClass& a, const char16_t (&codeUnits)[length])
{
    if (a.length() != length)
        return false;

    if (a.is8Bit())
        return equal(a.span8().data(), { codeUnits, length });

    return equal(a.span16().data(), { codeUnits, length });
}

template<typename T>
concept ContainsEncodingAwareSpans = requires(T t)
{
    { t.is8Bit() } -> std::convertible_to<bool>;
    { t.span8() } -> std::convertible_to<std::span<const Latin1Character>>;
    { t.span16() } -> std::convertible_to<std::span<const char16_t>>;
};

template<ContainsEncodingAwareSpans StringClass>
bool equal(const StringClass& string, std::span<const char8_t> span)
{
    if (string.is8Bit())
        return Unicode::equal(string.span8(), span);

    return Unicode::equal(string.span16(), span);
}

template<typename CharacterTypeA, typename CharacterTypeB> inline bool equalIgnoringASCIICaseWithLength(std::span<const CharacterTypeA> a, std::span<const CharacterTypeB> b, size_t lengthToCheck)
{
    ASSERT(a.size() >= lengthToCheck);
    ASSERT(b.size() >= lengthToCheck);
    for (size_t i = 0; i < lengthToCheck; ++i) {
        if (toASCIILower(a[i]) != toASCIILower(b[i]))
            return false;
    }
    return true;
}

template<typename CharacterTypeA, typename CharacterTypeB> inline bool spanHasPrefixIgnoringASCIICase(std::span<const CharacterTypeA> span, std::span<const CharacterTypeB> prefix)
{
    if (span.size() < prefix.size())
        return false;
    return equalIgnoringASCIICaseWithLength(span, prefix, prefix.size());
}

template<typename CharacterTypeA, typename CharacterTypeB>
    requires(TriviallyComparableCodeUnits<CharacterTypeA, CharacterTypeB>)
inline bool equalIgnoringASCIICase(std::span<const CharacterTypeA> a, std::span<const CharacterTypeB> b)
{
    return a.size() == b.size() && equalIgnoringASCIICaseWithLength(a, b, a.size());
}

template<OneByteCharacterType CharacterType>
inline bool equalIgnoringASCIICase(std::span<const CharacterType> a, ASCIILiteral b)
{
    return equalIgnoringASCIICase(a, byteCast<CharacterType>(b.span()));
}

template<typename StringClassA, typename StringClassB>
bool equalIgnoringASCIICaseCommon(const StringClassA& a, const StringClassB& b)
{
    if (a.length() != b.length())
        return false;

    if (a.is8Bit()) {
        if (b.is8Bit())
            return equalIgnoringASCIICaseWithLength(a.span8(), b.span8(), b.length());
        return equalIgnoringASCIICaseWithLength(a.span8(), b.span16(), b.length());
    }
    if (b.is8Bit())
        return equalIgnoringASCIICaseWithLength(a.span16(), b.span8(), b.length());
    return equalIgnoringASCIICaseWithLength(a.span16(), b.span16(), b.length());
}

template<typename StringClassA> bool equalIgnoringASCIICaseCommon(const StringClassA& a, const char* b)
{
    auto bSpan = unsafeSpan(b);
    if (a.length() != bSpan.size())
        return false;
    if (a.is8Bit())
        return equalIgnoringASCIICaseWithLength(a.span8(), bSpan, bSpan.size());
    return equalIgnoringASCIICaseWithLength(a.span16(), bSpan, bSpan.size());
}

template<typename SearchCharacterType, typename MatchCharacterType>
    requires(TriviallyComparableCodeUnits<SearchCharacterType, MatchCharacterType>)
size_t findIgnoringASCIICase(std::span<const SearchCharacterType> source, std::span<const MatchCharacterType> matchCharacters, size_t startOffset = 0)
{
    ASSERT(source.size() >= matchCharacters.size());

    auto startSearchedCharacters = source.subspan(startOffset);

    // delta is the number of additional times to test; delta == 0 means test only once.
    size_t delta = startSearchedCharacters.size() - matchCharacters.size();

    for (size_t i = 0; i <= delta; ++i) {
        if (equalIgnoringASCIICaseWithLength(startSearchedCharacters.subspan(i), matchCharacters, matchCharacters.size()))
            return startOffset + i;
    }
    return notFound;
}

template<OneByteCharacterType CharacterType>
size_t findIgnoringASCIICase(std::span<const CharacterType> source, ASCIILiteral matchCharacters)
{
    return findIgnoringASCIICase(source, byteCast<CharacterType>(matchCharacters.span()));
}

template<typename SearchCharacterType, typename MatchCharacterType>
bool containsIgnoringASCIICase(std::span<const SearchCharacterType> source, std::span<const MatchCharacterType> matchCharacters)
{
    return findIgnoringASCIICase(source, matchCharacters) != notFound;
}

template<typename CharacterType>
bool containsIgnoringASCIICase(std::span<const CharacterType> source, ASCIILiteral matchCharacters)
{
    return containsIgnoringASCIICase(source, byteCast<CharacterType>(matchCharacters.span()));
}

inline size_t findIgnoringASCIICaseWithoutLength(const char* source, const char* matchCharacters)
{
    auto searchSpan = unsafeSpan(source);
    auto matchSpan = unsafeSpan(matchCharacters);

    return matchSpan.size() <= searchSpan.size() ? findIgnoringASCIICase(searchSpan, matchSpan, 0) : notFound;
}

template <typename SearchCharacterType, typename MatchCharacterType>
ALWAYS_INLINE static size_t findInner(std::span<const SearchCharacterType> searchCharacters, std::span<const MatchCharacterType> matchCharacters, size_t index)
{
    // Optimization: keep a running hash of the strings,
    // only call equal() if the hashes match.

    // delta is the number of additional times to test; delta == 0 means test only once.
    size_t delta = searchCharacters.size() - matchCharacters.size();

    unsigned searchHash = 0;
    unsigned matchHash = 0;

    for (size_t i = 0; i < matchCharacters.size(); ++i) {
        searchHash += searchCharacters[i];
        matchHash += matchCharacters[i];
    }

    size_t i = 0;
    // keep looping until we match
    while (searchHash != matchHash || !equal(searchCharacters.data() + i, matchCharacters)) {
        if (i == delta)
            return notFound;
        searchHash += searchCharacters[i + matchCharacters.size()];
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

template<typename UnsignedType>
ALWAYS_INLINE const UnsignedType* findImpl(const UnsignedType* pointer, UnsignedType character, size_t length)
{
    auto charactersVector = SIMD::splat<UnsignedType>(character);
    auto vectorMatch = [&](auto value) ALWAYS_INLINE_LAMBDA {
        auto mask = SIMD::equal(value, charactersVector);
        return SIMD::findFirstNonZeroIndex(mask);
    };

    auto scalarMatch = [&](auto current) ALWAYS_INLINE_LAMBDA {
        return current == character;
    };

    constexpr size_t threshold = 32;
    auto* end = pointer + length;
    auto* cursor = SIMD::find<UnsignedType, threshold>(std::span { pointer, end }, vectorMatch, scalarMatch);
    if (cursor == end)
        return nullptr;
    return cursor;
}

ALWAYS_INLINE const uint16_t* find16(const uint16_t* pointer, uint16_t character, size_t length)
{
    return findImpl(pointer, character, length);
}

ALWAYS_INLINE const uint32_t* find32(const uint32_t* pointer, uint32_t character, size_t length)
{
    return findImpl(pointer, character, length);
}

ALWAYS_INLINE const uint64_t* find64(const uint64_t* pointer, uint64_t character, size_t length)
{
    return findImpl(pointer, character, length);
}

ALWAYS_INLINE const Float16* findFloat16(const Float16* pointer, Float16 target, size_t length)
{
    for (size_t index = 0; index < length; ++index) {
        if (pointer[index] == target)
            return pointer + index;
    }
    return nullptr;
}

WTF_EXPORT_PRIVATE const float* findFloatAlignedImpl(const float* pointer, float target, size_t length);

#if CPU(ARM64)
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

WTF_EXPORT_PRIVATE const double* findDoubleAlignedImpl(const double* pointer, double target, size_t length);

#if CPU(ARM64)
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

WTF_EXPORT_PRIVATE const Latin1Character* find8NonASCIIAlignedImpl(std::span<const Latin1Character>);
WTF_EXPORT_PRIVATE const char16_t* find16NonASCIIAlignedImpl(std::span<const char16_t>);

#if CPU(ARM64)
ALWAYS_INLINE const Latin1Character* find8NonASCII(std::span<const Latin1Character> data)
{
    constexpr size_t thresholdLength = 16;
    static_assert(!(thresholdLength % (16 / sizeof(Latin1Character))), "length threshold should be 16-byte aligned to make find8NonASCIIAlignedImpl simpler");
    auto* pointer = data.data();
    auto length = data.size();
    uintptr_t unaligned = reinterpret_cast<uintptr_t>(pointer) & 0xf;

    size_t index = 0;
    size_t runway = std::min(thresholdLength - (unaligned / sizeof(Latin1Character)), length);
    for (; index < runway; ++index) {
        if (!isASCII(pointer[index]))
            return pointer + index;
    }
    if (runway == length)
        return nullptr;

    ASSERT(index < length);
    return find8NonASCIIAlignedImpl({ pointer + index, length - index });
}

ALWAYS_INLINE const char16_t* find16NonASCII(std::span<const char16_t> data)
{
    constexpr size_t thresholdLength = 16;
    static_assert(!(thresholdLength % (16 / sizeof(char16_t))), "length threshold should be 16-byte aligned to make find16NonASCIIAlignedImpl simpler");
    auto* pointer = data.data();
    auto length = data.size();
    uintptr_t unaligned = reinterpret_cast<uintptr_t>(pointer) & 0xf;

    size_t index = 0;
    size_t runway = std::min(thresholdLength - (unaligned / sizeof(char16_t)), length);
    for (; index < runway; ++index) {
        if (!isASCII(pointer[index]))
            return pointer + index;
    }
    if (runway == length)
        return nullptr;

    ASSERT(index < length);
    return find16NonASCIIAlignedImpl({ pointer + index, length - index });
}
#endif

template<std::integral CharacterType1, std::integral CharacterType2>
    requires (sizeof(CharacterType1) == sizeof(CharacterType2))
inline size_t find(std::span<const CharacterType1> characters, CharacterType2 matchCharacter, size_t index = 0)
{
    if constexpr (sizeof(CharacterType1) == 1) {
        if (index >= characters.size())
            return notFound;
        auto* result = reinterpret_cast<const CharacterType1*>(find8(std::bit_cast<const uint8_t*>(characters.data() + index), matchCharacter, characters.size() - index));
        ASSERT(!result || static_cast<unsigned>(result - characters.data()) >= index);
        if (result)
            return result - characters.data();
        return notFound;
    }

    if constexpr (sizeof(CharacterType1) == 2) {
        if (index >= characters.size())
            return notFound;
        auto* result = reinterpret_cast<const CharacterType1*>(find16(std::bit_cast<const uint16_t*>(characters.data() + index), matchCharacter, characters.size() - index));
        ASSERT(!result || static_cast<unsigned>(result - characters.data()) >= index);
        if (result)
            return result - characters.data();
        return notFound;
    }

    while (index < characters.size()) {
        if (characters[index] == matchCharacter)
            return index;
        ++index;
    }
    return notFound;
}

ALWAYS_INLINE size_t find(std::span<const char16_t> characters, Latin1Character matchCharacter, size_t index = 0)
{
    return find(characters, static_cast<char16_t>(matchCharacter), index);
}

inline size_t find(std::span<const Latin1Character> characters, char16_t matchCharacter, size_t index = 0)
{
    if (!isLatin1(matchCharacter))
        return notFound;
    return find(characters, static_cast<Latin1Character>(matchCharacter), index);
}

template<OneByteCharacterType CharacterType>
inline size_t find(std::span<const CharacterType> characters, ASCIILiteral matchCharacters)
{
    return find(characters, byteCast<CharacterType>(matchCharacters.span()));
}

template<std::integral CharacterType1, std::integral CharacterType2>
inline bool contains(std::span<const CharacterType1> characters, CharacterType2 matchCharacter, size_t index = 0)
{
    return find(characters, matchCharacter, index) != notFound;
}

template<OneByteCharacterType CharacterType>
inline bool contains(std::span<const CharacterType> characters, ASCIILiteral matchCharacters)
{
    return contains(characters, byteCast<CharacterType>(matchCharacters.span()));
}

template <typename SearchCharacterType, typename MatchCharacterType>
ALWAYS_INLINE static size_t reverseFindInner(std::span<const SearchCharacterType> searchCharacters, std::span<const MatchCharacterType> matchCharacters, size_t start)
{
    // Optimization: keep a running hash of the strings,
    // only call equal if the hashes match.

    // delta is the number of additional times to test; delta == 0 means test only once.
    size_t delta = std::min(start, searchCharacters.size() - matchCharacters.size());

    unsigned searchHash = 0;
    unsigned matchHash = 0;
    for (size_t i = 0; i < matchCharacters.size(); ++i) {
        searchHash += searchCharacters[delta + i];
        matchHash += matchCharacters[i];
    }

    // keep looping until we match
    while (searchHash != matchHash || !equal(searchCharacters.data() + delta, matchCharacters)) {
        if (!delta)
            return notFound;
        --delta;
        searchHash -= searchCharacters[delta + matchCharacters.size()];
        searchHash += searchCharacters[delta];
    }
    return delta;
}

template<typename SearchCharacterType, typename MatchCharacterType>
    requires(TriviallyComparableCodeUnits<SearchCharacterType, MatchCharacterType>)
ALWAYS_INLINE static size_t reverseFind(std::span<const SearchCharacterType> searchCharacters, std::span<const MatchCharacterType> matchCharacters, size_t start = std::numeric_limits<size_t>::max())
{
    return reverseFindInner(searchCharacters, matchCharacters, start);
}

template<OneByteCharacterType CharacterType>
ALWAYS_INLINE static size_t reverseFind(std::span<const CharacterType> searchCharacters, ASCIILiteral matchCharacters)
{
    return reverseFind(searchCharacters, byteCast<CharacterType>(matchCharacters.span()));
}

template<typename CharT, typename OneByteCharT>
concept SearchableStringByOneByteCharacter =
    sizeof(OneByteCharT) == 1
    && (std::is_same_v<std::remove_const_t<CharT>, std::remove_const_t<OneByteCharT>>
    || std::is_same_v<std::remove_const_t<OneByteCharT>, Latin1Character>);

template<typename CharacterType, typename OneByteCharacterType>
    requires SearchableStringByOneByteCharacter<CharacterType, OneByteCharacterType>
inline bool equalLettersIgnoringASCIICaseWithLength(std::span<const CharacterType> characters, std::span<const OneByteCharacterType> lowercaseLetters, size_t length)
{
    ASSERT(characters.size() >= length);
    ASSERT(lowercaseLetters.size() >= length);
    for (size_t i = 0; i < length; ++i) {
        if (!isASCIIAlphaCaselessEqual(characters[i], lowercaseLetters[i]))
            return false;
    }
    return true;
}

template<typename CharacterType> inline bool equalLettersIgnoringASCIICase(std::span<const CharacterType> characters, std::span<const Latin1Character> lowercaseLetters)
{
    return characters.size() == lowercaseLetters.size() && equalLettersIgnoringASCIICaseWithLength(characters, lowercaseLetters, lowercaseLetters.size());
}

template<typename CharacterType> inline bool equalLettersIgnoringASCIICase(std::span<const CharacterType> characters, std::span<const char> lowercaseLetters)
{
    return equalLettersIgnoringASCIICase(characters, byteCast<Latin1Character>(lowercaseLetters));
}

template<typename CharacterType> inline bool equalLettersIgnoringASCIICase(std::span<const CharacterType> characters, ASCIILiteral lowercaseLetters)
{
    return equalLettersIgnoringASCIICase(characters, lowercaseLetters.span8());
}

template<typename StringClass> bool inline hasPrefixWithLettersIgnoringASCIICaseCommon(const StringClass& string, std::span<const Latin1Character> lowercaseLetters)
{
#if ASSERT_ENABLED
    ASSERT(lowercaseLetters.front());
    for (auto lowercaseLetter : lowercaseLetters)
        ASSERT(!lowercaseLetter || toASCIILowerUnchecked(lowercaseLetter) == lowercaseLetter);
#endif
    ASSERT(string.length() >= lowercaseLetters.size());

    if (string.is8Bit())
        return equalLettersIgnoringASCIICaseWithLength(string.span8(), lowercaseLetters, lowercaseLetters.size());
    return equalLettersIgnoringASCIICaseWithLength(string.span16(), lowercaseLetters, lowercaseLetters.size());
}

// This is intentionally not marked inline because it's used often and is not speed-critical enough to want it inlined everywhere.
template<typename StringClass> bool equalLettersIgnoringASCIICaseCommon(const StringClass& string, std::span<const Latin1Character> literal)
{
    if (string.length() != literal.size())
        return false;
    return hasPrefixWithLettersIgnoringASCIICaseCommon(string, literal);
}

template<typename SearchCharacterType, typename MatchCharacterType>
    requires(TriviallyComparableCodeUnits<SearchCharacterType, MatchCharacterType>)
bool startsWith(std::span<const SearchCharacterType> string, std::span<const MatchCharacterType> prefix)
{
    if (prefix.size() > string.size())
        return false;

    return equal(string.data(), prefix);
}

template<OneByteCharacterType CharacterType>
bool startsWith(std::span<const CharacterType> string, ASCIILiteral prefix)
{
    return startsWith(string, byteCast<CharacterType>(prefix.span()));
}

template<typename SearchCharacterType, typename MatchCharacterType>
    requires(TriviallyComparableCodeUnits<SearchCharacterType, MatchCharacterType>)
bool endsWith(std::span<const SearchCharacterType> string, std::span<const MatchCharacterType> suffix)
{
    unsigned suffixSize = suffix.size();
    unsigned referenceSize = string.size();
    if (suffixSize > referenceSize)
        return false;

    unsigned startOffset = referenceSize - suffixSize;

    return equal(string.subspan(startOffset).data(), suffix);
}

template<OneByteCharacterType CharacterType>
bool endsWith(std::span<const CharacterType> string, ASCIILiteral suffix)
{
    return endsWith(string, byteCast<CharacterType>(suffix.span()));
}

template<typename SearchCharacterType, typename MatchCharacterType>
    requires(TriviallyComparableCodeUnits<SearchCharacterType, MatchCharacterType>)
bool endsWithLettersIgnoringASCIICaseCommon(std::span<const SearchCharacterType> string, std::span<const MatchCharacterType> suffix)
{
    unsigned suffixLength = suffix.size();
    unsigned referenceLength = string.size();
    if (suffixLength > referenceLength)
        return false;

    unsigned startOffset = referenceLength - suffixLength;

    return equalIgnoringASCIICaseWithLength(string.subspan(startOffset), suffix, suffixLength);
}

template<typename SearchCharacterType, typename MatchCharacterType>
    requires(TriviallyComparableCodeUnits<SearchCharacterType, MatchCharacterType>)
bool endsWithLettersIgnoringASCIICase(std::span<const SearchCharacterType> string, std::span<const MatchCharacterType> suffix)
{
    return endsWithLettersIgnoringASCIICaseCommon(string, suffix);
}

template<OneByteCharacterType CharacterType>
bool endsWithLettersIgnoringASCIICase(std::span<const CharacterType> string, ASCIILiteral suffix)
{
    return endsWithLettersIgnoringASCIICase(string, byteCast<CharacterType>(suffix.span()));
}

template<typename SearchCharacterType, typename MatchCharacterType>
    requires(TriviallyComparableCodeUnits<SearchCharacterType, MatchCharacterType>)
bool startsWithLettersIgnoringASCIICaseCommon(std::span<const SearchCharacterType> string, std::span<const MatchCharacterType> prefix)
{
    if (prefix.empty())
        return true;
    if (string.size() < prefix.size())
        return false;
    return equalLettersIgnoringASCIICaseWithLength(string, prefix, prefix.size());
}

template<typename SearchCharacterType, typename MatchCharacterType>
    requires(TriviallyComparableCodeUnits<SearchCharacterType, MatchCharacterType>)
bool startsWithLettersIgnoringASCIICase(std::span<const SearchCharacterType> string, std::span<const MatchCharacterType> prefix)
{
    return startsWithLettersIgnoringASCIICaseCommon(string, prefix);
}

template<OneByteCharacterType CharacterType>
bool startsWithLettersIgnoringASCIICase(std::span<const CharacterType> string, ASCIILiteral prefix)
{
    return startsWithLettersIgnoringASCIICase(string, byteCast<CharacterType>(prefix.span()));
}

template<typename StringClass> bool startsWithLettersIgnoringASCIICaseCommon(const StringClass& string, std::span<const Latin1Character> prefix)
{
    if (prefix.empty())
        return true;
    if (string.length() < prefix.size())
        return false;
    return hasPrefixWithLettersIgnoringASCIICaseCommon(string, prefix);
}

template<typename StringClass> inline bool equalLettersIgnoringASCIICaseCommon(const StringClass& string, ASCIILiteral literal)
{
    return equalLettersIgnoringASCIICaseCommon(string, literal.span8());
}

template<typename StringClass> inline bool startsWithLettersIgnoringASCIICaseCommon(const StringClass& string, ASCIILiteral literal)
{
    return startsWithLettersIgnoringASCIICaseCommon(string, literal.span8());
}

inline bool equalIgnoringASCIICase(const char* a, const char* b)
{
    return equalIgnoringASCIICase(unsafeSpan(a), unsafeSpan(b));
}

inline bool equalLettersIgnoringASCIICase(ASCIILiteral a, ASCIILiteral b)
{
    return equalLettersIgnoringASCIICase(a.span8(), b.span8());
}

inline bool equalIgnoringASCIICase(const char* string, ASCIILiteral literal)
{
    return equalIgnoringASCIICase(unsafeSpan(string), literal.span());
}

inline bool equalIgnoringASCIICase(ASCIILiteral a, ASCIILiteral b)
{
    return equalIgnoringASCIICase(a.span8(), b.span8());
}

template<typename ElementType>
inline void copyElements(std::span<ElementType> destinationSpan, std::span<const ElementType> sourceSpan)
{
    ASSERT(!spansOverlap(destinationSpan, sourceSpan));
    ASSERT(destinationSpan.size() >= sourceSpan.size());
    auto* __restrict destination = destinationSpan.data();
    auto* __restrict source = sourceSpan.data();
    if (sourceSpan.size() == 1)
        *destination = *source;
    else if (!sourceSpan.empty())
        std::memcpy(destination, source, sourceSpan.size_bytes());
}

inline void copyElements(std::span<uint16_t> destinationSpan, std::span<const uint8_t> sourceSpan)
{
    ASSERT(!spansOverlap(destinationSpan, sourceSpan));
    ASSERT(destinationSpan.size() >= sourceSpan.size());
    auto* __restrict destination = destinationSpan.data();
    auto* __restrict source = sourceSpan.data();
    size_t length = sourceSpan.size();

#if CPU(ARM64)
    // SIMD Upconvert.
    const auto* end = destination + length;
    constexpr uintptr_t memoryAccessSize = 64;

    if (length >= memoryAccessSize) {
        constexpr uintptr_t memoryAccessMask = memoryAccessSize - 1;
        const auto* simdEnd = destination + (length & ~memoryAccessMask);
        simde_uint8x16_t zeros = simde_vdupq_n_u8(0);
        do {
            simde_uint8x16x4_t bytes = simde_vld1q_u8_x4(std::bit_cast<const uint8_t*>(source));
            source += memoryAccessSize;

            simde_vst2q_u8(std::bit_cast<uint8_t*>(destination), (simde_uint8x16x2_t { bytes.val[0], zeros }));
            destination += memoryAccessSize / 4;
            simde_vst2q_u8(std::bit_cast<uint8_t*>(destination), (simde_uint8x16x2_t { bytes.val[1], zeros }));
            destination += memoryAccessSize / 4;
            simde_vst2q_u8(std::bit_cast<uint8_t*>(destination), (simde_uint8x16x2_t { bytes.val[2], zeros }));
            destination += memoryAccessSize / 4;
            simde_vst2q_u8(std::bit_cast<uint8_t*>(destination), (simde_uint8x16x2_t { bytes.val[3], zeros }));
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

inline void copyElements(std::span<uint8_t> destinationSpan, std::span<const uint16_t> sourceSpan)
{
    ASSERT(!spansOverlap(destinationSpan, sourceSpan));
    ASSERT(destinationSpan.size() >= sourceSpan.size());
    auto* __restrict destination = destinationSpan.data();
    auto* __restrict source = sourceSpan.data();
    size_t length = sourceSpan.size();

#if CPU(X86_SSE2)
    const uintptr_t memoryAccessSize = 16; // Memory accesses on 16 byte (128 bit) alignment
    const uintptr_t memoryAccessMask = memoryAccessSize - 1;

    unsigned i = 0;
    for (; i < length && !isAlignedTo<memoryAccessMask>(&source[i]); ++i)
        destination[i] = source[i];

    const uintptr_t sourceLoadSize = 32; // Process 32 bytes (16 uint16_ts) each iteration
    const unsigned utf16CodeUnitsPerLoop = sourceLoadSize / sizeof(uint16_t);
    if (length > utf16CodeUnitsPerLoop) {
        const unsigned endLength = length - utf16CodeUnitsPerLoop + 1;
        for (; i < endLength; i += utf16CodeUnitsPerLoop) {
            __m128i first8Uint16s = _mm_load_si128(reinterpret_cast<const __m128i*>(&source[i]));
            __m128i second8Uint16s = _mm_load_si128(reinterpret_cast<const __m128i*>(&source[i+8]));
            __m128i packedChars = _mm_packus_epi16(first8Uint16s, second8Uint16s);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&destination[i]), packedChars);
        }
    }

    for (; i < length; ++i)
        destination[i] = source[i];
#elif CPU(ARM64) && CPU(ADDRESS64) && !ASSERT_ENABLED
    const uint8_t* const end = destination + length;
    const uintptr_t memoryAccessSize = 16;

    if (length >= memoryAccessSize) {
        const uintptr_t memoryAccessMask = memoryAccessSize - 1;

        // Vector interleaved unpack, we only store the lower 8 bits.
        const uintptr_t lengthLeft = end - destination;
        const uint8_t* const simdEnd = destination + (lengthLeft & ~memoryAccessMask);
        do {
            __asm__(
                "ld2   { v0.16B, v1.16B }, [%[SOURCE]], #32\n\t"
                "st1   { v0.16B }, [%[DESTINATION]], #16\n\t"
                : [SOURCE]"+r" (source), [DESTINATION]"+r" (destination)
                :
                : "memory", "v0", "v1");
        } while (destination != simdEnd);
    }

    while (destination != end)
        *destination++ = static_cast<uint8_t>(*source++);
#elif CPU(ARM_NEON) && !(CPU(BIG_ENDIAN) || CPU(MIDDLE_ENDIAN)) && !ASSERT_ENABLED
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
            __asm__(
                "vld2.8   { d0-d1 }, [%[SOURCE]] !\n\t"
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

inline void copyElements(std::span<uint16_t> destinationSpan, std::span<const uint32_t> sourceSpan)
{
    ASSERT(!spansOverlap(destinationSpan, sourceSpan));
    ASSERT(destinationSpan.size() >= sourceSpan.size());
    auto* __restrict destination = destinationSpan.data();
    auto* __restrict source = sourceSpan.data();
    size_t length = sourceSpan.size();

    const auto* end = destination + length;
#if CPU(ARM64) && CPU(ADDRESS64)
    const uintptr_t memoryAccessSize = 32 / sizeof(uint32_t);
    if (length >= memoryAccessSize) {
        const uintptr_t memoryAccessMask = memoryAccessSize - 1;
        const uintptr_t lengthLeft = end - destination;
        const auto* const simdEnd = destination + (lengthLeft & ~memoryAccessMask);
        // Use ld2 to load lower 16bit of 8 uint32_t.
        do {
            __asm__(
                "ld2   { v0.8H, v1.8H }, [%[SOURCE]], #32\n\t"
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

inline void copyElements(std::span<uint32_t> destinationSpan, std::span<const uint64_t> sourceSpan)
{
    ASSERT(!spansOverlap(destinationSpan, sourceSpan));
    ASSERT(destinationSpan.size() >= sourceSpan.size());
    auto* __restrict destination = destinationSpan.data();
    auto* __restrict source = sourceSpan.data();
    size_t length = sourceSpan.size();

    const auto* end = destination + length;
#if CPU(ARM64) && CPU(ADDRESS64)
    const uintptr_t memoryAccessSize = 32 / sizeof(uint64_t);
    if (length >= memoryAccessSize) {
        const uintptr_t memoryAccessMask = memoryAccessSize - 1;
        const uintptr_t lengthLeft = end - destination;
        const auto* const simdEnd = destination + (lengthLeft & ~memoryAccessMask);
        // Use ld2 to load lower 32bit of 4 uint64_t.
        do {
            __asm__(
                "ld2   { v0.4S, v1.4S }, [%[SOURCE]], #32\n\t"
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

inline void copyElements(std::span<uint16_t> destinationSpan, std::span<const uint64_t> sourceSpan)
{
    ASSERT(!spansOverlap(destinationSpan, sourceSpan));
    ASSERT(destinationSpan.size() >= sourceSpan.size());
    auto* __restrict destination = destinationSpan.data();
    auto* __restrict source = sourceSpan.data();
    size_t length = sourceSpan.size();

    const auto* end = destination + length;
#if CPU(ARM64) && CPU(ADDRESS64)
    const uintptr_t memoryAccessSize = 64 / sizeof(uint64_t);
    if (length >= memoryAccessSize) {
        const uintptr_t memoryAccessMask = memoryAccessSize - 1;
        const uintptr_t lengthLeft = end - destination;
        const auto* const simdEnd = destination + (lengthLeft & ~memoryAccessMask);
        // Use ld4 to load lower 16bit of 8 uint64_t.
        do {
            __asm__(
                "ld4   { v0.8H, v1.8H, v2.8H, v3.8H }, [%[SOURCE]], #64\n\t"
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

inline void copyElements(std::span<uint8_t> destinationSpan, std::span<const uint64_t> sourceSpan)
{
    ASSERT(!spansOverlap(destinationSpan, sourceSpan));
    ASSERT(destinationSpan.size() >= sourceSpan.size());
    auto* __restrict destination = destinationSpan.data();
    auto* __restrict source = sourceSpan.data();
    size_t length = sourceSpan.size();

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
            __asm__(
                "ld4   { v0.8H, v1.8H, v2.8H, v3.8H }, [%[SOURCE]], #64\n\t"
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

inline void copyElements(std::span<float> destinationSpan, std::span<const double> sourceSpan)
{
    ASSERT(!spansOverlap(destinationSpan, sourceSpan));
    ASSERT(destinationSpan.size() >= sourceSpan.size());
    auto* __restrict destination = destinationSpan.data();
    auto* __restrict source = sourceSpan.data();
    size_t length = sourceSpan.size();

    const auto* end = destination + length;
    const uintptr_t memoryAccessSize = 64 / sizeof(double);
    if (length >= memoryAccessSize) {
        const uintptr_t memoryAccessMask = memoryAccessSize - 1;
        const uintptr_t lengthLeft = end - destination;
        const auto* const simdEnd = destination + (lengthLeft & ~memoryAccessMask);
        do {
            simde_float64x2x4_t result = simde_vld1q_f64_x4(source);
            source += memoryAccessSize;
            simde_float32x4_t converted0 = simde_vcvt_high_f32_f64(simde_vcvt_f32_f64(result.val[0]), result.val[1]);
            simde_float32x4_t converted1 = simde_vcvt_high_f32_f64(simde_vcvt_f32_f64(result.val[2]), result.val[3]);
            simde_vst1q_f32_x2(destination, simde_float32x4x2_t { converted0, converted1 });
            destination += memoryAccessSize;
        } while (destination != simdEnd);
    }
    while (destination != end)
        *destination++ = *source++;
}

#ifndef __swift__ // FIXME: rdar://136156228
inline void copyElements(std::span<char16_t> destination, std::span<const Latin1Character> source)
{
    copyElements(spanReinterpretCast<uint16_t>(destination), byteCast<uint8_t>(source));
}

inline void copyElements(std::span<Latin1Character> destination, std::span<const char16_t> source)
{
    copyElements(byteCast<uint8_t>(destination), spanReinterpretCast<const uint16_t>(source));
}
#endif

template<typename CharacterType, CharacterType... characters>
ALWAYS_INLINE bool compareEach(CharacterType input)
{
    // Use | intentionally to reduce branches. Cast to int to silence "use of bitwise '|' with boolean operands" warning.
    return (... | static_cast<int>(input == characters));
}

template<typename CharacterType, CharacterType... characters>
ALWAYS_INLINE bool charactersContain(std::span<const CharacterType> span)
{
    auto* data = span.data();
    size_t length = span.size();

#if CPU(ARM64) || CPU(X86_64)
    constexpr size_t stride = SIMD::stride<CharacterType>;
    using UnsignedType = SameSizeUnsignedInteger<CharacterType>;
    using BulkType = decltype(SIMD::load(static_cast<const UnsignedType*>(nullptr)));
    if (length >= stride) {
        size_t index = 0;
        BulkType accumulated { };
        for (; index + stride <= length; index += stride)
            accumulated = SIMD::bitOr(accumulated, SIMD::equal<characters...>(SIMD::load(std::bit_cast<const UnsignedType*>(data + index))));

        if (index < length)
            accumulated = SIMD::bitOr(accumulated, SIMD::equal<characters...>(SIMD::load(std::bit_cast<const UnsignedType*>(data + length - stride))));

        return SIMD::isNonZero(accumulated);
    }
#endif

    for (const auto* end = data + length; data != end; ++data) {
        if (compareEach<CharacterType, characters...>(*data))
            return true;
    }
    return false;
}

template<typename CharacterType>
inline size_t countMatchedCharacters(std::span<const CharacterType> span, CharacterType character)
{
    using UnsignedType = SameSizeUnsignedInteger<CharacterType>;
    auto mask = SIMD::splat<UnsignedType>(character);
    auto vectorMatch = [&](auto input) ALWAYS_INLINE_LAMBDA {
        return SIMD::equal(input, mask);
    };
    auto scalarMatch = [&](auto input) ALWAYS_INLINE_LAMBDA {
        return input == character;
    };
    return SIMD::count(span, vectorMatch, scalarMatch);
}

struct NewlinePosition {
    size_t position { notFound };
    size_t length { 0 };
};

template<typename CharacterType>
inline NewlinePosition findNextNewline(std::span<const CharacterType> span, size_t startPosition = 0)
{
    // Find newlines matching the pattern \r\n?|\n
    // This handles: LF (\n), CR (\r), and CRLF (\r\n)

    if (startPosition >= span.size())
        return { };

    auto searchSpan = span.subspan(startPosition);
    using UnsignedType = SameSizeUnsignedInteger<CharacterType>;

    auto lfVector = SIMD::splat<UnsignedType>('\n');
    auto crVector = SIMD::splat<UnsignedType>('\r');

    auto vectorMatch = [&](auto value) ALWAYS_INLINE_LAMBDA {
        auto lfMask = SIMD::equal(value, lfVector);
        auto crMask = SIMD::equal(value, crVector);
        auto combinedMask = SIMD::bitOr(lfMask, crMask);
        return SIMD::findFirstNonZeroIndex(combinedMask);
    };

    auto scalarMatch = [&](auto current) ALWAYS_INLINE_LAMBDA {
        return current == '\n' || current == '\r';
    };

    constexpr size_t threshold = 32;
    auto* ptr = SIMD::find<CharacterType, threshold>(searchSpan, vectorMatch, scalarMatch);

    if (ptr == searchSpan.data() + searchSpan.size())
        return { };

    CharacterType ch = *ptr;
    size_t pos = ptr - searchSpan.data();

    if (ch == '\r') {
        if (pos + 1 < searchSpan.size() && searchSpan[pos + 1] == '\n')
            return { startPosition + pos, 2 };
        return { startPosition + pos, 1 };
    }

    return { startPosition + pos, 1 };
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

}

using WTF::charactersContain;
using WTF::contains;
using WTF::containsIgnoringASCIICase;
using WTF::endsWith;
using WTF::endsWithLettersIgnoringASCIICase;
using WTF::equalIgnoringASCIICase;
using WTF::equalIgnoringASCIICaseWithLength;
using WTF::equalLettersIgnoringASCIICase;
using WTF::equalLettersIgnoringASCIICaseWithLength;
using WTF::findIgnoringASCIICase;
using WTF::isLatin1;
using WTF::reverseFind;
using WTF::span;
using WTF::spanHasPrefixIgnoringASCIICase;
using WTF::startsWith;
using WTF::startsWithLettersIgnoringASCIICase;
using WTF::strlenSpan;
using WTF::unsafeSpan;
using WTF::unsafeSpanIncludingNullTerminator;
