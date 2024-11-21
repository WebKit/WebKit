/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

// FIXME: Deprecate these functions once Webkit tree version is >= MacOS 12.0 (aka MacOS >= Sequoia). We are temporarily importing code from JavaScriptCore/parser/Lexer module to replace system-determined ICU implementations of u_isIDPart and u_isIDStart.

#include "config.h"
#include "URLPatternICUHelpers.h"

#include <unicode/utf16.h>
#include <unicode/utf8.h>

namespace WebCore {
namespace URLPatternUtilities {

enum CharacterType : uint8_t {
    // Types for the main switch

    // The first three types are fixed, and also used for identifying
    // ASCII alpha and alphanumeric characters (see isIdentStart and isIdentPart).
    CharacterIdentifierStart,
    CharacterZero,
    CharacterNumber,

    // For single-byte characters grandfathered into Other_ID_Continue -- namely just U+00B7 MIDDLE DOT.
    // (http://unicode.org/reports/tr31/#Backward_Compatibility)
    CharacterOtherIdentifierPart,

    CharacterInvalid,
    CharacterLineTerminator,
    CharacterExclamationMark,
    CharacterOpenParen,
    CharacterCloseParen,
    CharacterOpenBracket,
    CharacterCloseBracket,
    CharacterComma,
    CharacterColon,
    CharacterQuestion,
    CharacterTilde,
    CharacterQuote,
    CharacterBackQuote,
    CharacterDot,
    CharacterSlash,
    CharacterBackSlash,
    CharacterSemicolon,
    CharacterOpenBrace,
    CharacterCloseBrace,

    CharacterAdd,
    CharacterSub,
    CharacterMultiply,
    CharacterModulo,
    CharacterAnd,
    CharacterXor,
    CharacterOr,
    CharacterLess,
    CharacterGreater,
    CharacterEqual,

    // Other types (only one so far)
    CharacterWhiteSpace,
    CharacterHash,
    CharacterPrivateIdentifierStart
};

// 256 Latin-1 codes
static constexpr const CharacterType typesOfLatin1Characters[256] = {
/*   0 - Null               */ CharacterInvalid,
/*   1 - Start of Heading   */ CharacterInvalid,
/*   2 - Start of Text      */ CharacterInvalid,
/*   3 - End of Text        */ CharacterInvalid,
/*   4 - End of Transm.     */ CharacterInvalid,
/*   5 - Enquiry            */ CharacterInvalid,
/*   6 - Acknowledgment     */ CharacterInvalid,
/*   7 - Bell               */ CharacterInvalid,
/*   8 - Back Space         */ CharacterInvalid,
/*   9 - Horizontal Tab     */ CharacterWhiteSpace,
/*  10 - Line Feed          */ CharacterLineTerminator,
/*  11 - Vertical Tab       */ CharacterWhiteSpace,
/*  12 - Form Feed          */ CharacterWhiteSpace,
/*  13 - Carriage Return    */ CharacterLineTerminator,
/*  14 - Shift Out          */ CharacterInvalid,
/*  15 - Shift In           */ CharacterInvalid,
/*  16 - Data Line Escape   */ CharacterInvalid,
/*  17 - Device Control 1   */ CharacterInvalid,
/*  18 - Device Control 2   */ CharacterInvalid,
/*  19 - Device Control 3   */ CharacterInvalid,
/*  20 - Device Control 4   */ CharacterInvalid,
/*  21 - Negative Ack.      */ CharacterInvalid,
/*  22 - Synchronous Idle   */ CharacterInvalid,
/*  23 - End of Transmit    */ CharacterInvalid,
/*  24 - Cancel             */ CharacterInvalid,
/*  25 - End of Medium      */ CharacterInvalid,
/*  26 - Substitute         */ CharacterInvalid,
/*  27 - Escape             */ CharacterInvalid,
/*  28 - File Separator     */ CharacterInvalid,
/*  29 - Group Separator    */ CharacterInvalid,
/*  30 - Record Separator   */ CharacterInvalid,
/*  31 - Unit Separator     */ CharacterInvalid,
/*  32 - Space              */ CharacterWhiteSpace,
/*  33 - !                  */ CharacterExclamationMark,
/*  34 - "                  */ CharacterQuote,
/*  35 - #                  */ CharacterHash,
/*  36 - $                  */ CharacterIdentifierStart,
/*  37 - %                  */ CharacterModulo,
/*  38 - &                  */ CharacterAnd,
/*  39 - '                  */ CharacterQuote,
/*  40 - (                  */ CharacterOpenParen,
/*  41 - )                  */ CharacterCloseParen,
/*  42 - *                  */ CharacterMultiply,
/*  43 - +                  */ CharacterAdd,
/*  44 - ,                  */ CharacterComma,
/*  45 - -                  */ CharacterSub,
/*  46 - .                  */ CharacterDot,
/*  47 - /                  */ CharacterSlash,
/*  48 - 0                  */ CharacterZero,
/*  49 - 1                  */ CharacterNumber,
/*  50 - 2                  */ CharacterNumber,
/*  51 - 3                  */ CharacterNumber,
/*  52 - 4                  */ CharacterNumber,
/*  53 - 5                  */ CharacterNumber,
/*  54 - 6                  */ CharacterNumber,
/*  55 - 7                  */ CharacterNumber,
/*  56 - 8                  */ CharacterNumber,
/*  57 - 9                  */ CharacterNumber,
/*  58 - :                  */ CharacterColon,
/*  59 - ;                  */ CharacterSemicolon,
/*  60 - <                  */ CharacterLess,
/*  61 - =                  */ CharacterEqual,
/*  62 - >                  */ CharacterGreater,
/*  63 - ?                  */ CharacterQuestion,
/*  64 - @                  */ CharacterPrivateIdentifierStart,
/*  65 - A                  */ CharacterIdentifierStart,
/*  66 - B                  */ CharacterIdentifierStart,
/*  67 - C                  */ CharacterIdentifierStart,
/*  68 - D                  */ CharacterIdentifierStart,
/*  69 - E                  */ CharacterIdentifierStart,
/*  70 - F                  */ CharacterIdentifierStart,
/*  71 - G                  */ CharacterIdentifierStart,
/*  72 - H                  */ CharacterIdentifierStart,
/*  73 - I                  */ CharacterIdentifierStart,
/*  74 - J                  */ CharacterIdentifierStart,
/*  75 - K                  */ CharacterIdentifierStart,
/*  76 - L                  */ CharacterIdentifierStart,
/*  77 - M                  */ CharacterIdentifierStart,
/*  78 - N                  */ CharacterIdentifierStart,
/*  79 - O                  */ CharacterIdentifierStart,
/*  80 - P                  */ CharacterIdentifierStart,
/*  81 - Q                  */ CharacterIdentifierStart,
/*  82 - R                  */ CharacterIdentifierStart,
/*  83 - S                  */ CharacterIdentifierStart,
/*  84 - T                  */ CharacterIdentifierStart,
/*  85 - U                  */ CharacterIdentifierStart,
/*  86 - V                  */ CharacterIdentifierStart,
/*  87 - W                  */ CharacterIdentifierStart,
/*  88 - X                  */ CharacterIdentifierStart,
/*  89 - Y                  */ CharacterIdentifierStart,
/*  90 - Z                  */ CharacterIdentifierStart,
/*  91 - [                  */ CharacterOpenBracket,
/*  92 - \                  */ CharacterBackSlash,
/*  93 - ]                  */ CharacterCloseBracket,
/*  94 - ^                  */ CharacterXor,
/*  95 - _                  */ CharacterIdentifierStart,
/*  96 - `                  */ CharacterBackQuote,
/*  97 - a                  */ CharacterIdentifierStart,
/*  98 - b                  */ CharacterIdentifierStart,
/*  99 - c                  */ CharacterIdentifierStart,
/* 100 - d                  */ CharacterIdentifierStart,
/* 101 - e                  */ CharacterIdentifierStart,
/* 102 - f                  */ CharacterIdentifierStart,
/* 103 - g                  */ CharacterIdentifierStart,
/* 104 - h                  */ CharacterIdentifierStart,
/* 105 - i                  */ CharacterIdentifierStart,
/* 106 - j                  */ CharacterIdentifierStart,
/* 107 - k                  */ CharacterIdentifierStart,
/* 108 - l                  */ CharacterIdentifierStart,
/* 109 - m                  */ CharacterIdentifierStart,
/* 110 - n                  */ CharacterIdentifierStart,
/* 111 - o                  */ CharacterIdentifierStart,
/* 112 - p                  */ CharacterIdentifierStart,
/* 113 - q                  */ CharacterIdentifierStart,
/* 114 - r                  */ CharacterIdentifierStart,
/* 115 - s                  */ CharacterIdentifierStart,
/* 116 - t                  */ CharacterIdentifierStart,
/* 117 - u                  */ CharacterIdentifierStart,
/* 118 - v                  */ CharacterIdentifierStart,
/* 119 - w                  */ CharacterIdentifierStart,
/* 120 - x                  */ CharacterIdentifierStart,
/* 121 - y                  */ CharacterIdentifierStart,
/* 122 - z                  */ CharacterIdentifierStart,
/* 123 - {                  */ CharacterOpenBrace,
/* 124 - |                  */ CharacterOr,
/* 125 - }                  */ CharacterCloseBrace,
/* 126 - ~                  */ CharacterTilde,
/* 127 - Delete             */ CharacterInvalid,
/* 128 - Cc category        */ CharacterInvalid,
/* 129 - Cc category        */ CharacterInvalid,
/* 130 - Cc category        */ CharacterInvalid,
/* 131 - Cc category        */ CharacterInvalid,
/* 132 - Cc category        */ CharacterInvalid,
/* 133 - Cc category        */ CharacterInvalid,
/* 134 - Cc category        */ CharacterInvalid,
/* 135 - Cc category        */ CharacterInvalid,
/* 136 - Cc category        */ CharacterInvalid,
/* 137 - Cc category        */ CharacterInvalid,
/* 138 - Cc category        */ CharacterInvalid,
/* 139 - Cc category        */ CharacterInvalid,
/* 140 - Cc category        */ CharacterInvalid,
/* 141 - Cc category        */ CharacterInvalid,
/* 142 - Cc category        */ CharacterInvalid,
/* 143 - Cc category        */ CharacterInvalid,
/* 144 - Cc category        */ CharacterInvalid,
/* 145 - Cc category        */ CharacterInvalid,
/* 146 - Cc category        */ CharacterInvalid,
/* 147 - Cc category        */ CharacterInvalid,
/* 148 - Cc category        */ CharacterInvalid,
/* 149 - Cc category        */ CharacterInvalid,
/* 150 - Cc category        */ CharacterInvalid,
/* 151 - Cc category        */ CharacterInvalid,
/* 152 - Cc category        */ CharacterInvalid,
/* 153 - Cc category        */ CharacterInvalid,
/* 154 - Cc category        */ CharacterInvalid,
/* 155 - Cc category        */ CharacterInvalid,
/* 156 - Cc category        */ CharacterInvalid,
/* 157 - Cc category        */ CharacterInvalid,
/* 158 - Cc category        */ CharacterInvalid,
/* 159 - Cc category        */ CharacterInvalid,
/* 160 - Zs category (nbsp) */ CharacterWhiteSpace,
/* 161 - Po category        */ CharacterInvalid,
/* 162 - Sc category        */ CharacterInvalid,
/* 163 - Sc category        */ CharacterInvalid,
/* 164 - Sc category        */ CharacterInvalid,
/* 165 - Sc category        */ CharacterInvalid,
/* 166 - So category        */ CharacterInvalid,
/* 167 - So category        */ CharacterInvalid,
/* 168 - Sk category        */ CharacterInvalid,
/* 169 - So category        */ CharacterInvalid,
/* 170 - Ll category        */ CharacterIdentifierStart,
/* 171 - Pi category        */ CharacterInvalid,
/* 172 - Sm category        */ CharacterInvalid,
/* 173 - Cf category        */ CharacterInvalid,
/* 174 - So category        */ CharacterInvalid,
/* 175 - Sk category        */ CharacterInvalid,
/* 176 - So category        */ CharacterInvalid,
/* 177 - Sm category        */ CharacterInvalid,
/* 178 - No category        */ CharacterInvalid,
/* 179 - No category        */ CharacterInvalid,
/* 180 - Sk category        */ CharacterInvalid,
/* 181 - Ll category        */ CharacterIdentifierStart,
/* 182 - So category        */ CharacterInvalid,
/* 183 - Po category        */ CharacterOtherIdentifierPart,
/* 184 - Sk category        */ CharacterInvalid,
/* 185 - No category        */ CharacterInvalid,
/* 186 - Ll category        */ CharacterIdentifierStart,
/* 187 - Pf category        */ CharacterInvalid,
/* 188 - No category        */ CharacterInvalid,
/* 189 - No category        */ CharacterInvalid,
/* 190 - No category        */ CharacterInvalid,
/* 191 - Po category        */ CharacterInvalid,
/* 192 - Lu category        */ CharacterIdentifierStart,
/* 193 - Lu category        */ CharacterIdentifierStart,
/* 194 - Lu category        */ CharacterIdentifierStart,
/* 195 - Lu category        */ CharacterIdentifierStart,
/* 196 - Lu category        */ CharacterIdentifierStart,
/* 197 - Lu category        */ CharacterIdentifierStart,
/* 198 - Lu category        */ CharacterIdentifierStart,
/* 199 - Lu category        */ CharacterIdentifierStart,
/* 200 - Lu category        */ CharacterIdentifierStart,
/* 201 - Lu category        */ CharacterIdentifierStart,
/* 202 - Lu category        */ CharacterIdentifierStart,
/* 203 - Lu category        */ CharacterIdentifierStart,
/* 204 - Lu category        */ CharacterIdentifierStart,
/* 205 - Lu category        */ CharacterIdentifierStart,
/* 206 - Lu category        */ CharacterIdentifierStart,
/* 207 - Lu category        */ CharacterIdentifierStart,
/* 208 - Lu category        */ CharacterIdentifierStart,
/* 209 - Lu category        */ CharacterIdentifierStart,
/* 210 - Lu category        */ CharacterIdentifierStart,
/* 211 - Lu category        */ CharacterIdentifierStart,
/* 212 - Lu category        */ CharacterIdentifierStart,
/* 213 - Lu category        */ CharacterIdentifierStart,
/* 214 - Lu category        */ CharacterIdentifierStart,
/* 215 - Sm category        */ CharacterInvalid,
/* 216 - Lu category        */ CharacterIdentifierStart,
/* 217 - Lu category        */ CharacterIdentifierStart,
/* 218 - Lu category        */ CharacterIdentifierStart,
/* 219 - Lu category        */ CharacterIdentifierStart,
/* 220 - Lu category        */ CharacterIdentifierStart,
/* 221 - Lu category        */ CharacterIdentifierStart,
/* 222 - Lu category        */ CharacterIdentifierStart,
/* 223 - Ll category        */ CharacterIdentifierStart,
/* 224 - Ll category        */ CharacterIdentifierStart,
/* 225 - Ll category        */ CharacterIdentifierStart,
/* 226 - Ll category        */ CharacterIdentifierStart,
/* 227 - Ll category        */ CharacterIdentifierStart,
/* 228 - Ll category        */ CharacterIdentifierStart,
/* 229 - Ll category        */ CharacterIdentifierStart,
/* 230 - Ll category        */ CharacterIdentifierStart,
/* 231 - Ll category        */ CharacterIdentifierStart,
/* 232 - Ll category        */ CharacterIdentifierStart,
/* 233 - Ll category        */ CharacterIdentifierStart,
/* 234 - Ll category        */ CharacterIdentifierStart,
/* 235 - Ll category        */ CharacterIdentifierStart,
/* 236 - Ll category        */ CharacterIdentifierStart,
/* 237 - Ll category        */ CharacterIdentifierStart,
/* 238 - Ll category        */ CharacterIdentifierStart,
/* 239 - Ll category        */ CharacterIdentifierStart,
/* 240 - Ll category        */ CharacterIdentifierStart,
/* 241 - Ll category        */ CharacterIdentifierStart,
/* 242 - Ll category        */ CharacterIdentifierStart,
/* 243 - Ll category        */ CharacterIdentifierStart,
/* 244 - Ll category        */ CharacterIdentifierStart,
/* 245 - Ll category        */ CharacterIdentifierStart,
/* 246 - Ll category        */ CharacterIdentifierStart,
/* 247 - Sm category        */ CharacterInvalid,
/* 248 - Ll category        */ CharacterIdentifierStart,
/* 249 - Ll category        */ CharacterIdentifierStart,
/* 250 - Ll category        */ CharacterIdentifierStart,
/* 251 - Ll category        */ CharacterIdentifierStart,
/* 252 - Ll category        */ CharacterIdentifierStart,
/* 253 - Ll category        */ CharacterIdentifierStart,
/* 254 - Ll category        */ CharacterIdentifierStart,
/* 255 - Ll category        */ CharacterIdentifierStart
};

UNUSED_FUNCTION static bool isNonLatin1IdentStart(char32_t c)
{
    return u_hasBinaryProperty(c, UCHAR_ID_START);
}

UNUSED_FUNCTION NEVER_INLINE static bool isNonLatin1IdentPart(char32_t c)
{
    return u_hasBinaryProperty(c, UCHAR_ID_CONTINUE) || c == 0x200C || c == 0x200D;
}

template<typename CharacterType> inline constexpr bool isLatin1(CharacterType character)
{
    using UnsignedCharacterType = typename std::make_unsigned<CharacterType>::type;
    return static_cast<UnsignedCharacterType>(character) <= static_cast<UnsignedCharacterType>(0xFF);
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
template<typename CharacterType>
ALWAYS_INLINE bool isIdentStart(CharacterType c)
{
    static_assert(std::is_same_v<CharacterType, LChar> || std::is_same_v<CharacterType, char32_t>, "Call isSingleCharacterIdentStart for UChars that don't need to check for surrogate pairs");
    if (!isLatin1(c))
        return isNonLatin1IdentStart(c);
    return typesOfLatin1Characters[static_cast<LChar>(c)] == CharacterIdentifierStart;
}

ALWAYS_INLINE bool isSingleCharacterIdentStart(UChar c)
{
    if (LIKELY(isLatin1(c)))
        return isIdentStart(static_cast<LChar>(c));
    return !U16_IS_SURROGATE(c) && isIdentStart(static_cast<char32_t>(c));
}

template<typename CharacterType>
ALWAYS_INLINE bool isIdentPart(CharacterType c)
{
    static_assert(std::is_same_v<CharacterType, LChar> || std::is_same_v<CharacterType, char32_t>, "Call isSingleCharacterIdentPart for UChars that don't need to check for surrogate pairs");
    if (!isLatin1(c))
        return isNonLatin1IdentPart(c);

    // Character types are divided into two groups depending on whether they can be part of an
    // identifier or not. Those whose type value is less or equal than CharacterOtherIdentifierPart can be
    // part of an identifier. (See the CharacterType definition for more details.)
    return typesOfLatin1Characters[static_cast<LChar>(c)] <= CharacterOtherIdentifierPart;
}

ALWAYS_INLINE bool isSingleCharacterIdentPart(UChar c)
{
    if (LIKELY(isLatin1(c)))
        return isIdentPart(static_cast<LChar>(c));
    return !U16_IS_SURROGATE(c) && isIdentPart(static_cast<char32_t>(c));
}
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
} // namespace URLPatternUtilities
} // namespace WebCore
