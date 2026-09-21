/*
 * Copyright (C) 2026 Anthropic PBC.
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

#include <JavaScriptCore/JSExportMacros.h>
#include <JavaScriptCore/LexerUnicodeProperties.h>
#include <span>
#include <wtf/ASCIICType.h>
#include <wtf/BitSet.h>
#include <wtf/SIMDHelpers.h>
#include <wtf/text/ASCIIFastPath.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

JS_EXPORT_PRIVATE extern const WTF::BitSet<256> whiteSpaceTable;

// JavaScript white space does not include the line terminators; those are isLineTerminator below.
template<typename CharType> ALWAYS_INLINE bool isWhiteSpace(CharType);

template<> ALWAYS_INLINE bool isWhiteSpace<Latin1Character>(Latin1Character character)
{
    return whiteSpaceTable.get(character);
}

template<> ALWAYS_INLINE bool isWhiteSpace<char16_t>(char16_t character)
{
    if (isLatin1(character))
        return isWhiteSpace<Latin1Character>(static_cast<Latin1Character>(character));

    // Non-Latin1 Zs category (Space_Separator) + BOM
    // Generated from UnicodeData.txt by generateLexerUnicodePropertyTables.py
    return isNonLatin1WhiteSpace(character);
}

// JavaScript has four line terminators, not two: ECMA-262 #11.3 adds LS and PS to LF and CR.
template<typename CharType> ALWAYS_INLINE bool isLineTerminator(CharType);

template<> ALWAYS_INLINE bool isLineTerminator<Latin1Character>(Latin1Character character)
{
    return character == '\r' || character == '\n';
}

template<> ALWAYS_INLINE bool isLineTerminator<char16_t>(char16_t character)
{
    // (c & ~1) == 0x2028 covers both LS (U+2028) and PS (U+2029).
    return character == '\r' || character == '\n' || (character & ~1) == 0x2028;
}

// Equivalent to `character < 0xE` for an 8-bit source: for c >= 0xE the subtraction is at most
// 0xF1, and for c < 0xE it wraps to a value with bit 13 set, which no Latin-1 character reaches.
template<typename CharType>
ALWAYS_INLINE bool characterNeedsLiteralSpecialHandling(CharType character)
{
    return (static_cast<unsigned>(character) - 0xE) & 0x2000;
}

template<typename CharType>
ALWAYS_INLINE bool isCRLFPair(CharType first, CharType second)
{
    return first == '\r' && second == '\n';
}

template<typename CharType>
ALWAYS_INLINE size_t lineStartAfterTerminator(std::span<const CharType> text, size_t indexOfTerminator)
{
    ASSERT(indexOfTerminator < text.size());
    ASSERT(isLineTerminator(text[indexOfTerminator]));
    if (indexOfTerminator + 1 < text.size() && isCRLFPair(text[indexOfTerminator], text[indexOfTerminator + 1]))
        return indexOfTerminator + 2;
    return indexOfTerminator + 1;
}

template<typename CharType>
ALWAYS_INLINE const CharType* findLineTerminator(std::span<const CharType> text)
{
    using UnsignedType = SameSizeUnsignedInteger<CharType>;
    auto vectorMatch = [](auto input) ALWAYS_INLINE_LAMBDA {
        constexpr auto lineFeedMask = SIMD::splat<UnsignedType>('\n');
        constexpr auto carriageReturnMask = SIMD::splat<UnsignedType>('\r');
        auto matches = SIMD::bitOr(SIMD::equal(input, lineFeedMask), SIMD::equal(input, carriageReturnMask));
        if constexpr (!std::is_same_v<CharType, Latin1Character>) {
            // LS and PS are single UTF-16 code units, so they compare directly in a 16-bit lane.
            constexpr auto lineSeparatorMask = SIMD::splat<UnsignedType>(static_cast<UnsignedType>(0x2028));
            constexpr auto paragraphSeparatorMask = SIMD::splat<UnsignedType>(static_cast<UnsignedType>(0x2029));
            matches = SIMD::bitOr(matches, SIMD::equal(input, lineSeparatorMask), SIMD::equal(input, paragraphSeparatorMask));
        }
        return SIMD::findFirstNonZeroIndex(matches);
    };
    auto scalarMatch = [](CharType character) ALWAYS_INLINE_LAMBDA {
        return isLineTerminator(character);
    };
    return SIMD::find(text, vectorMatch, scalarMatch);
}

ALWAYS_INLINE unsigned char convertHex(int c1, int c2)
{
    return (toASCIIHexValue(c1) << 4) | toASCIIHexValue(c2);
}

ALWAYS_INLINE char16_t convertUnicode(int c1, int c2, int c3, int c4)
{
    return (convertHex(c1, c2) << 8) | convertHex(c3, c4);
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
