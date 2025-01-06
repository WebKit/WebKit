/*
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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

#include "config.h"
#include <wtf/URLParser.h>

#include <array>
#include <functional>
#include <mutex>
#include <wtf/text/CodePointIterator.h>
#include <wtf/text/MakeString.h>

namespace WTF {

#define URL_PARSER_DEBUGGING 0

#if URL_PARSER_DEBUGGING
#define URL_PARSER_LOG(...) WTFLogAlways(__VA_ARGS__)
#else
#define URL_PARSER_LOG(...)
#endif

ALWAYS_INLINE static void appendCodePoint(Vector<UChar>& destination, char32_t codePoint)
{
    if (U_IS_BMP(codePoint)) {
        destination.append(static_cast<UChar>(codePoint));
        return;
    }
    destination.appendList({ U16_LEAD(codePoint), U16_TRAIL(codePoint) });
}

enum URLCharacterClass {
    UserInfoEncode = 0x1,
    PathEncode = 0x2,
    ForbiddenHost = 0x4,
    ForbiddenDomain = 0x8,
    QueryEncode = 0x10,
    SlashQuestionOrHash = 0x20,
    ValidScheme = 0x40,
};

static constexpr std::array<uint8_t, 256> characterClassTable {
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenHost | ForbiddenDomain, // 0x0
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0x1
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0x2
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0x3
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0x4
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0x5
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0x6
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0x7
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0x8
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenHost | ForbiddenDomain, // 0x9
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenHost | ForbiddenDomain, // 0xA
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0xB
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0xC
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenHost | ForbiddenDomain, // 0xD
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0xE
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0xF
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0x10
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0x11
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0x12
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0x13
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0x14
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0x15
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0x16
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0x17
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0x18
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0x19
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0x1A
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0x1B
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0x1C
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0x1D
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0x1E
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenDomain, // 0x1F
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenHost | ForbiddenDomain, // ' '
    0, // '!'
    UserInfoEncode | PathEncode | QueryEncode, // '"'
    UserInfoEncode | PathEncode | QueryEncode | SlashQuestionOrHash | ForbiddenHost | ForbiddenDomain, // '#'
    0, // '$'
    ForbiddenDomain, // '%'
    0, // '&'
    0, // '\''
    0, // '('
    0, // ')'
    0, // '*'
    ValidScheme, // '+'
    0, // ','
    ValidScheme, // '-'
    ValidScheme, // '.'
    UserInfoEncode | SlashQuestionOrHash | ForbiddenHost | ForbiddenDomain, // '/'
    ValidScheme, // '0'
    ValidScheme, // '1'
    ValidScheme, // '2'
    ValidScheme, // '3'
    ValidScheme, // '4'
    ValidScheme, // '5'
    ValidScheme, // '6'
    ValidScheme, // '7'
    ValidScheme, // '8'
    ValidScheme, // '9'
    UserInfoEncode | ForbiddenHost | ForbiddenDomain, // ':'
    UserInfoEncode, // ';'
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenHost | ForbiddenDomain, // '<'
    UserInfoEncode, // '='
    UserInfoEncode | PathEncode | QueryEncode | ForbiddenHost | ForbiddenDomain, // '>'
    UserInfoEncode | PathEncode | SlashQuestionOrHash | ForbiddenHost | ForbiddenDomain, // '?'
    UserInfoEncode | ForbiddenHost | ForbiddenDomain, // '@'
    ValidScheme, // 'A'
    ValidScheme, // 'B'
    ValidScheme, // 'C'
    ValidScheme, // 'D'
    ValidScheme, // 'E'
    ValidScheme, // 'F'
    ValidScheme, // 'G'
    ValidScheme, // 'H'
    ValidScheme, // 'I'
    ValidScheme, // 'J'
    ValidScheme, // 'K'
    ValidScheme, // 'L'
    ValidScheme, // 'M'
    ValidScheme, // 'N'
    ValidScheme, // 'O'
    ValidScheme, // 'P'
    ValidScheme, // 'Q'
    ValidScheme, // 'R'
    ValidScheme, // 'S'
    ValidScheme, // 'T'
    ValidScheme, // 'U'
    ValidScheme, // 'V'
    ValidScheme, // 'W'
    ValidScheme, // 'X'
    ValidScheme, // 'Y'
    ValidScheme, // 'Z'
    UserInfoEncode | ForbiddenHost | ForbiddenDomain, // '['
    UserInfoEncode | SlashQuestionOrHash | ForbiddenHost | ForbiddenDomain, // '\\'
    UserInfoEncode | ForbiddenHost | ForbiddenDomain, // ']'
    UserInfoEncode | ForbiddenHost | ForbiddenDomain, // '^'
    0, // '_'
    UserInfoEncode | PathEncode, // '`'
    ValidScheme, // 'a'
    ValidScheme, // 'b'
    ValidScheme, // 'c'
    ValidScheme, // 'd'
    ValidScheme, // 'e'
    ValidScheme, // 'f'
    ValidScheme, // 'g'
    ValidScheme, // 'h'
    ValidScheme, // 'i'
    ValidScheme, // 'j'
    ValidScheme, // 'k'
    ValidScheme, // 'l'
    ValidScheme, // 'm'
    ValidScheme, // 'n'
    ValidScheme, // 'o'
    ValidScheme, // 'p'
    ValidScheme, // 'q'
    ValidScheme, // 'r'
    ValidScheme, // 's'
    ValidScheme, // 't'
    ValidScheme, // 'u'
    ValidScheme, // 'v'
    ValidScheme, // 'w'
    ValidScheme, // 'x'
    ValidScheme, // 'y'
    ValidScheme, // 'z'
    UserInfoEncode | PathEncode, // '{'
    UserInfoEncode | ForbiddenHost | ForbiddenDomain, // '|'
    UserInfoEncode | PathEncode, // '}'
    0, // '~'
    QueryEncode | ForbiddenDomain, // 0x7F
    QueryEncode, // 0x80
    QueryEncode, // 0x81
    QueryEncode, // 0x82
    QueryEncode, // 0x83
    QueryEncode, // 0x84
    QueryEncode, // 0x85
    QueryEncode, // 0x86
    QueryEncode, // 0x87
    QueryEncode, // 0x88
    QueryEncode, // 0x89
    QueryEncode, // 0x8A
    QueryEncode, // 0x8B
    QueryEncode, // 0x8C
    QueryEncode, // 0x8D
    QueryEncode, // 0x8E
    QueryEncode, // 0x8F
    QueryEncode, // 0x90
    QueryEncode, // 0x91
    QueryEncode, // 0x92
    QueryEncode, // 0x93
    QueryEncode, // 0x94
    QueryEncode, // 0x95
    QueryEncode, // 0x96
    QueryEncode, // 0x97
    QueryEncode, // 0x98
    QueryEncode, // 0x99
    QueryEncode, // 0x9A
    QueryEncode, // 0x9B
    QueryEncode, // 0x9C
    QueryEncode, // 0x9D
    QueryEncode, // 0x9E
    QueryEncode, // 0x9F
    QueryEncode, // 0xA0
    QueryEncode, // 0xA1
    QueryEncode, // 0xA2
    QueryEncode, // 0xA3
    QueryEncode, // 0xA4
    QueryEncode, // 0xA5
    QueryEncode, // 0xA6
    QueryEncode, // 0xA7
    QueryEncode, // 0xA8
    QueryEncode, // 0xA9
    QueryEncode, // 0xAA
    QueryEncode, // 0xAB
    QueryEncode, // 0xAC
    QueryEncode, // 0xAD
    QueryEncode, // 0xAE
    QueryEncode, // 0xAF
    QueryEncode, // 0xB0
    QueryEncode, // 0xB1
    QueryEncode, // 0xB2
    QueryEncode, // 0xB3
    QueryEncode, // 0xB4
    QueryEncode, // 0xB5
    QueryEncode, // 0xB6
    QueryEncode, // 0xB7
    QueryEncode, // 0xB8
    QueryEncode, // 0xB9
    QueryEncode, // 0xBA
    QueryEncode, // 0xBB
    QueryEncode, // 0xBC
    QueryEncode, // 0xBD
    QueryEncode, // 0xBE
    QueryEncode, // 0xBF
    QueryEncode, // 0xC0
    QueryEncode, // 0xC1
    QueryEncode, // 0xC2
    QueryEncode, // 0xC3
    QueryEncode, // 0xC4
    QueryEncode, // 0xC5
    QueryEncode, // 0xC6
    QueryEncode, // 0xC7
    QueryEncode, // 0xC8
    QueryEncode, // 0xC9
    QueryEncode, // 0xCA
    QueryEncode, // 0xCB
    QueryEncode, // 0xCC
    QueryEncode, // 0xCD
    QueryEncode, // 0xCE
    QueryEncode, // 0xCF
    QueryEncode, // 0xD0
    QueryEncode, // 0xD1
    QueryEncode, // 0xD2
    QueryEncode, // 0xD3
    QueryEncode, // 0xD4
    QueryEncode, // 0xD5
    QueryEncode, // 0xD6
    QueryEncode, // 0xD7
    QueryEncode, // 0xD8
    QueryEncode, // 0xD9
    QueryEncode, // 0xDA
    QueryEncode, // 0xDB
    QueryEncode, // 0xDC
    QueryEncode, // 0xDD
    QueryEncode, // 0xDE
    QueryEncode, // 0xDF
    QueryEncode, // 0xE0
    QueryEncode, // 0xE1
    QueryEncode, // 0xE2
    QueryEncode, // 0xE3
    QueryEncode, // 0xE4
    QueryEncode, // 0xE5
    QueryEncode, // 0xE6
    QueryEncode, // 0xE7
    QueryEncode, // 0xE8
    QueryEncode, // 0xE9
    QueryEncode, // 0xEA
    QueryEncode, // 0xEB
    QueryEncode, // 0xEC
    QueryEncode, // 0xED
    QueryEncode, // 0xEE
    QueryEncode, // 0xEF
    QueryEncode, // 0xF0
    QueryEncode, // 0xF1
    QueryEncode, // 0xF2
    QueryEncode, // 0xF3
    QueryEncode, // 0xF4
    QueryEncode, // 0xF5
    QueryEncode, // 0xF6
    QueryEncode, // 0xF7
    QueryEncode, // 0xF8
    QueryEncode, // 0xF9
    QueryEncode, // 0xFA
    QueryEncode, // 0xFB
    QueryEncode, // 0xFC
    QueryEncode, // 0xFD
    QueryEncode, // 0xFE
    QueryEncode, // 0xFF
};

template<typename CharacterType> ALWAYS_INLINE static bool isC0Control(CharacterType character) { return character <= 0x1F; }
template<typename CharacterType> ALWAYS_INLINE static bool isC0ControlOrSpace(CharacterType character) { return character <= 0x20; }
template<typename CharacterType> ALWAYS_INLINE static bool isTabOrNewline(CharacterType character) { return character <= 0xD && character >= 0x9 && character != 0xB && character != 0xC; }
template<typename CharacterType> ALWAYS_INLINE static bool isInC0ControlEncodeSet(CharacterType character) { return character > 0x7E || isC0Control(character); }
template<typename CharacterType> ALWAYS_INLINE static bool isInFragmentEncodeSet(CharacterType character) { return character > 0x7E || character == '`' || ((characterClassTable[character] & QueryEncode) && character != '#'); }
template<typename CharacterType> ALWAYS_INLINE static bool isInPathEncodeSet(CharacterType character) { return character > 0x7E || characterClassTable[character] & PathEncode; }
template<typename CharacterType> ALWAYS_INLINE static bool isInUserInfoEncodeSet(CharacterType character) { return character > 0x7E || characterClassTable[character] & UserInfoEncode; }
template<typename CharacterType> ALWAYS_INLINE static bool isPercentOrNonASCII(CharacterType character) { return !isASCII(character) || character == '%'; }
template<typename CharacterType> ALWAYS_INLINE static bool isSlashQuestionOrHash(CharacterType character) { return character <= '\\' && characterClassTable[character] & SlashQuestionOrHash; }
template<typename CharacterType> ALWAYS_INLINE static bool isValidSchemeCharacter(CharacterType character) { return character <= 'z' && characterClassTable[character] & ValidScheme; }
template<typename CharacterType> ALWAYS_INLINE static bool isSpecialCharacterForFragmentDirective(CharacterType character) { return !isASCII(character) || character == ',' || character == '-'; }

template<typename CharacterType>
ALWAYS_INLINE bool URLParser::isForbiddenHostCodePoint(CharacterType character)
{
    ASSERT(!m_urlIsSpecial);
    return character <= 0x7F && characterClassTable[character] & ForbiddenHost;
}

template<typename CharacterType>
ALWAYS_INLINE bool URLParser::isForbiddenDomainCodePoint(CharacterType character)
{
    ASSERT(m_urlIsSpecial);
    return character <= 0x7F && characterClassTable[character] & ForbiddenDomain;
}

ALWAYS_INLINE static bool shouldPercentEncodeQueryByte(uint8_t byte, const bool& urlIsSpecial)
{
    if (characterClassTable[byte] & QueryEncode)
        return true;
    if (byte == '\'' && urlIsSpecial)
        return true;
    return false;
}

bool URLParser::isInUserInfoEncodeSet(UChar c)
{
    return WTF::isInUserInfoEncodeSet(c);
}

bool URLParser::isSpecialCharacterForFragmentDirective(UChar c)
{
    return WTF::isSpecialCharacterForFragmentDirective(c);
}

template<typename CharacterType, URLParser::ReportSyntaxViolation reportSyntaxViolation>
ALWAYS_INLINE void URLParser::advance(CodePointIterator<CharacterType>& iterator, const CodePointIterator<CharacterType>& iteratorForSyntaxViolationPosition)
{
    ++iterator;
    while (UNLIKELY(!iterator.atEnd() && isTabOrNewline(*iterator))) {
        if (reportSyntaxViolation == ReportSyntaxViolation::Yes)
            syntaxViolation(iteratorForSyntaxViolationPosition);
        ++iterator;
    }
}

template<typename CharacterType>
bool URLParser::takesTwoAdvancesUntilEnd(CodePointIterator<CharacterType> iterator)
{
    if (iterator.atEnd())
        return false;
    advance<CharacterType, ReportSyntaxViolation::No>(iterator);
    if (iterator.atEnd())
        return false;
    advance<CharacterType, ReportSyntaxViolation::No>(iterator);
    return iterator.atEnd();
}

template<typename CharacterType>
ALWAYS_INLINE bool URLParser::isWindowsDriveLetter(CodePointIterator<CharacterType> iterator)
{
    // https://url.spec.whatwg.org/#start-with-a-windows-drive-letter
    if (iterator.atEnd() || !isASCIIAlpha(*iterator))
        return false;
    advance<CharacterType, ReportSyntaxViolation::No>(iterator);
    if (iterator.atEnd())
        return false;
    if (*iterator != ':' && *iterator != '|')
        return false;
    advance<CharacterType, ReportSyntaxViolation::No>(iterator);
    return iterator.atEnd() || *iterator == '/' || *iterator == '\\' || *iterator == '?' || *iterator == '#';
}

ALWAYS_INLINE void URLParser::appendToASCIIBuffer(char32_t codePoint)
{
    ASSERT(isASCII(codePoint));
    if (UNLIKELY(m_didSeeSyntaxViolation))
        m_asciiBuffer.append(codePoint);
}

ALWAYS_INLINE void URLParser::appendToASCIIBuffer(std::span<const LChar> characters)
{
    if (UNLIKELY(m_didSeeSyntaxViolation))
        m_asciiBuffer.append(characters);
}

template<typename CharacterType>
void URLParser::appendWindowsDriveLetter(CodePointIterator<CharacterType>& iterator)
{
    auto lengthWithOnlyOneSlashInPath = m_url.m_hostEnd + m_url.m_portLength + 1;
    if (m_url.m_pathAfterLastSlash > lengthWithOnlyOneSlashInPath) {
        syntaxViolation(iterator);
        m_url.m_pathAfterLastSlash = lengthWithOnlyOneSlashInPath;
        m_asciiBuffer.resize(lengthWithOnlyOneSlashInPath);
    }
    ASSERT(isWindowsDriveLetter(iterator));
    appendToASCIIBuffer(*iterator);
    advance(iterator);
    ASSERT(!iterator.atEnd());
    ASSERT(*iterator == ':' || *iterator == '|');
    if (*iterator == '|')
        syntaxViolation(iterator);
    appendToASCIIBuffer(':');
    advance(iterator);
}

bool URLParser::copyBaseWindowsDriveLetter(const URL& base)
{
    if (base.protocolIsFile()) {
        RELEASE_ASSERT(base.m_hostEnd + base.m_portLength < base.m_string.length());
        if (base.m_string.is8Bit()) {
            auto characters = base.m_string.span8();
            CodePointIterator c { characters.subspan(base.m_hostEnd + base.m_portLength + 1) };
            if (isWindowsDriveLetter(c)) {
                appendWindowsDriveLetter(c);
                return true;
            }
        } else {
            auto characters = base.m_string.span16();
            CodePointIterator c { characters.subspan(base.m_hostEnd + base.m_portLength + 1) };
            if (isWindowsDriveLetter(c)) {
                appendWindowsDriveLetter(c);
                return true;
            }
        }
    }
    return false;
}

template<typename CharacterType>
bool URLParser::shouldCopyFileURL(CodePointIterator<CharacterType> iterator)
{
    if (!isWindowsDriveLetter(iterator))
        return true;
    if (iterator.atEnd())
        return false;
    advance(iterator);
    if (iterator.atEnd())
        return true;
    advance(iterator);
    if (iterator.atEnd())
        return true;
    return !isSlashQuestionOrHash(*iterator);
}

static void percentEncodeByte(uint8_t byte, Vector<LChar>& buffer)
{
    buffer.append('%');
    buffer.append(upperNibbleToASCIIHexDigit(byte));
    buffer.append(lowerNibbleToASCIIHexDigit(byte));
}

void URLParser::percentEncodeByte(uint8_t byte)
{
    ASSERT(m_didSeeSyntaxViolation);
    appendToASCIIBuffer('%');
    appendToASCIIBuffer(upperNibbleToASCIIHexDigit(byte));
    appendToASCIIBuffer(lowerNibbleToASCIIHexDigit(byte));
}

static constexpr auto replacementCharacterUTF8PercentEncoded = "%EF%BF%BD"_s;

template<bool(*isInCodeSet)(char32_t), typename CharacterType>
ALWAYS_INLINE void URLParser::utf8PercentEncode(const CodePointIterator<CharacterType>& iterator)
{
    ASSERT(!iterator.atEnd());
    char32_t codePoint = *iterator;
    if (LIKELY(isASCII(codePoint))) {
        if (UNLIKELY(isInCodeSet(codePoint))) {
            syntaxViolation(iterator);
            percentEncodeByte(codePoint);
        } else
            appendToASCIIBuffer(codePoint);
        return;
    }
    ASSERT_WITH_MESSAGE(isInCodeSet(codePoint), "isInCodeSet should always return true for non-ASCII characters");
    syntaxViolation(iterator);

    std::array<uint8_t, U8_MAX_LENGTH> buffer;
    int32_t offset = 0;
    UBool isError = false;
    U8_APPEND(buffer, offset, U8_MAX_LENGTH, codePoint, isError);
    if (isError) {
        appendToASCIIBuffer(replacementCharacterUTF8PercentEncoded.span8());
        return;
    }
    for (int32_t i = 0; i < offset; ++i)
        percentEncodeByte(buffer[i]);
}

template<typename CharacterType>
ALWAYS_INLINE void URLParser::utf8QueryEncode(const CodePointIterator<CharacterType>& iterator)
{
    ASSERT(!iterator.atEnd());
    char32_t codePoint = *iterator;
    if (LIKELY(isASCII(codePoint))) {
        if (UNLIKELY(shouldPercentEncodeQueryByte(codePoint, m_urlIsSpecial))) {
            syntaxViolation(iterator);
            percentEncodeByte(codePoint);
        } else
            appendToASCIIBuffer(codePoint);
        return;
    }

    syntaxViolation(iterator);

    std::array<uint8_t, U8_MAX_LENGTH> buffer;
    int32_t offset = 0;
    UBool isError = false;
    U8_APPEND(buffer, offset, U8_MAX_LENGTH, codePoint, isError);
    if (isError) {
        appendToASCIIBuffer(replacementCharacterUTF8PercentEncoded.span8());
        return;
    }
    for (int32_t i = 0; i < offset; ++i) {
        auto byte = buffer[i];
        if (shouldPercentEncodeQueryByte(byte, m_urlIsSpecial))
            percentEncodeByte(byte);
        else
            appendToASCIIBuffer(byte);
    }
}

template<typename CharacterType>
void URLParser::encodeNonUTF8Query(const Vector<UChar>& source, const URLTextEncoding& encoding, CodePointIterator<CharacterType> iterator)
{
    auto encoded = encoding.encodeForURLParsing(source.span());
    size_t length = encoded.size();
    
    if (!length == !iterator.atEnd()) {
        syntaxViolation(iterator);
        return;
    }
    
    size_t i = 0;
    for (; i < length; ++i) {
        ASSERT(!iterator.atEnd());
        uint8_t byte = encoded[i];
        if (UNLIKELY(byte != *iterator)) {
            syntaxViolation(iterator);
            break;
        }
        if (UNLIKELY(shouldPercentEncodeQueryByte(byte, m_urlIsSpecial))) {
            syntaxViolation(iterator);
            break;
        }
        appendToASCIIBuffer(byte);
        ++iterator;
    }
    while (!iterator.atEnd() && isTabOrNewline(*iterator))
        ++iterator;
    ASSERT((i == length) == iterator.atEnd());
    for (; i < length; ++i) {
        ASSERT(m_didSeeSyntaxViolation);
        uint8_t byte = encoded[i];
        if (shouldPercentEncodeQueryByte(byte, m_urlIsSpecial))
            percentEncodeByte(byte);
        else
            appendToASCIIBuffer(byte);
    }
}

std::optional<uint16_t> URLParser::defaultPortForProtocol(StringView scheme)
{
    static constexpr uint16_t ftpPort = 21;
    static constexpr uint16_t httpPort = 80;
    static constexpr uint16_t httpsPort = 443;
    static constexpr uint16_t wsPort = 80;
    static constexpr uint16_t wssPort = 443;
    
    auto length = scheme.length();
    if (!length)
        return std::nullopt;
    switch (scheme[0]) {
    case 'w':
        switch (length) {
        case 2:
            if (scheme[1] == 's')
                return wsPort;
            return std::nullopt;
        case 3:
            if (scheme[1] == 's'
                && scheme[2] == 's')
                return wssPort;
            return std::nullopt;
        default:
            return std::nullopt;
        }
    case 'h':
        switch (length) {
        case 4:
            if (scheme[1] == 't'
                && scheme[2] == 't'
                && scheme[3] == 'p')
                return httpPort;
            return std::nullopt;
        case 5:
            if (scheme[1] == 't'
                && scheme[2] == 't'
                && scheme[3] == 'p'
                && scheme[4] == 's')
                return httpsPort;
            return std::nullopt;
        default:
            return std::nullopt;
        }
    case 'f':
        if (length == 3
            && scheme[1] == 't'
            && scheme[2] == 'p')
            return ftpPort;
        return std::nullopt;
    default:
        return std::nullopt;
    }
}

enum class Scheme {
    WS,
    WSS,
    File,
    FTP,
    HTTP,
    HTTPS,
    NonSpecial
};

ALWAYS_INLINE static Scheme scheme(StringView scheme)
{
    auto length = scheme.length();
    if (!length)
        return Scheme::NonSpecial;
    switch (scheme[0]) {
    case 'f':
        switch (length) {
        case 3:
            if (scheme[1] == 't'
                && scheme[2] == 'p')
                return Scheme::FTP;
            return Scheme::NonSpecial;
        case 4:
            if (scheme[1] == 'i'
                && scheme[2] == 'l'
                && scheme[3] == 'e')
                return Scheme::File;
            return Scheme::NonSpecial;
        default:
            return Scheme::NonSpecial;
        }
    case 'h':
        switch (length) {
        case 4:
            if (scheme[1] == 't'
                && scheme[2] == 't'
                && scheme[3] == 'p')
                return Scheme::HTTP;
            return Scheme::NonSpecial;
        case 5:
            if (scheme[1] == 't'
                && scheme[2] == 't'
                && scheme[3] == 'p'
                && scheme[4] == 's')
                return Scheme::HTTPS;
            return Scheme::NonSpecial;
        default:
            return Scheme::NonSpecial;
        }
    case 'w':
        switch (length) {
        case 2:
            if (scheme[1] == 's')
                return Scheme::WS;
            return Scheme::NonSpecial;
        case 3:
            if (scheme[1] == 's'
                && scheme[2] == 's')
                return Scheme::WSS;
            return Scheme::NonSpecial;
        default:
            return Scheme::NonSpecial;
        }
    default:
        return Scheme::NonSpecial;
    }
}

std::optional<String> URLParser::maybeCanonicalizeScheme(StringView scheme)
{
    if (scheme.isEmpty())
        return std::nullopt;

    size_t i = 0;
    while (i < scheme.length() && isTabOrNewline(scheme[i]))
        ++i;

    if (i >= scheme.length() || !isASCIIAlpha(scheme[i++]))
        return std::nullopt;

    for (; i < scheme.length(); ++i) {
        if (isASCIIAlphanumeric(scheme[i]) || scheme[i] == '+' || scheme[i] == '-' || scheme[i] == '.' || isTabOrNewline(scheme[i]))
            continue;
        return std::nullopt;
    }

    return scheme.convertToASCIILowercase().removeCharacters([](auto character) {
        return isTabOrNewline(character);
    });
}

bool URLParser::isSpecialScheme(StringView schemeArg)
{
    return scheme(schemeArg) != Scheme::NonSpecial;
}

enum class URLParser::URLPart {
    SchemeEnd,
    UserStart,
    UserEnd,
    PasswordEnd,
    HostEnd,
    PortEnd,
    PathAfterLastSlash,
    PathEnd,
    QueryEnd,
};

size_t URLParser::urlLengthUntilPart(const URL& url, URLPart part)
{
    switch (part) {
    case URLPart::QueryEnd:
        return url.m_queryEnd;
    case URLPart::PathEnd:
        return url.m_pathEnd;
    case URLPart::PathAfterLastSlash:
        return url.m_pathAfterLastSlash;
    case URLPart::PortEnd:
        return url.m_hostEnd + url.m_portLength;
    case URLPart::HostEnd:
        return url.m_hostEnd;
    case URLPart::PasswordEnd:
        return url.m_passwordEnd;
    case URLPart::UserEnd:
        return url.m_userEnd;
    case URLPart::UserStart:
        return url.m_userStart;
    case URLPart::SchemeEnd:
        return url.m_schemeEnd;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

void URLParser::copyASCIIStringUntil(const String& string, size_t length)
{
    RELEASE_ASSERT(length <= string.length());
    if (string.isNull())
        return;
    ASSERT(m_asciiBuffer.isEmpty());
    if (string.is8Bit())
        appendToASCIIBuffer(string.span8().first(length));
    else {
        for (auto character : string.span16().first(length)) {
            ASSERT_WITH_SECURITY_IMPLICATION(isASCII(character));
            appendToASCIIBuffer(character);
        }
    }
}

template<typename CharacterType>
void URLParser::copyURLPartsUntil(const URL& base, URLPart part, const CodePointIterator<CharacterType>& iterator, const URLTextEncoding*& nonUTF8QueryEncoding)
{
    syntaxViolation(iterator);

    m_asciiBuffer.clear();
    copyASCIIStringUntil(base.m_string, urlLengthUntilPart(base, part));
    switch (part) {
    case URLPart::QueryEnd:
        m_url.m_queryEnd = base.m_queryEnd;
        FALLTHROUGH;
    case URLPart::PathEnd:
        m_url.m_pathEnd = base.m_pathEnd;
        FALLTHROUGH;
    case URLPart::PathAfterLastSlash:
        m_url.m_pathAfterLastSlash = base.m_pathAfterLastSlash;
        FALLTHROUGH;
    case URLPart::PortEnd:
        m_url.m_portLength = base.m_portLength;
        FALLTHROUGH;
    case URLPart::HostEnd:
        m_url.m_hostEnd = base.m_hostEnd;
        FALLTHROUGH;
    case URLPart::PasswordEnd:
        m_url.m_passwordEnd = base.m_passwordEnd;
        FALLTHROUGH;
    case URLPart::UserEnd:
        m_url.m_userEnd = base.m_userEnd;
        FALLTHROUGH;
    case URLPart::UserStart:
        m_url.m_userStart = base.m_userStart;
        FALLTHROUGH;
    case URLPart::SchemeEnd:
        m_url.m_isValid = base.m_isValid;
        m_url.m_protocolIsInHTTPFamily = base.m_protocolIsInHTTPFamily;
        m_url.m_schemeEnd = base.m_schemeEnd;
    }

    switch (scheme(m_asciiBuffer.subspan(0, m_url.m_schemeEnd))) {
    case Scheme::WS:
    case Scheme::WSS:
        nonUTF8QueryEncoding = nullptr;
        m_urlIsSpecial = true;
        return;
    case Scheme::File:
        m_urlIsFile = true;
        FALLTHROUGH;
    case Scheme::FTP:
    case Scheme::HTTP:
    case Scheme::HTTPS:
        m_urlIsSpecial = true;
        return;
    case Scheme::NonSpecial:
        m_urlIsSpecial = false;
        nonUTF8QueryEncoding = nullptr;
        auto pathStart = m_url.m_hostEnd + m_url.m_portLength;
        if (pathStart + 2 < m_asciiBuffer.size()
            && m_asciiBuffer[pathStart] == '/'
            && m_asciiBuffer[pathStart + 1] == '.'
            && m_asciiBuffer[pathStart + 2] == '/') {
            m_asciiBuffer.remove(pathStart + 1, 2);
            m_url.m_pathAfterLastSlash = std::max(2u, m_url.m_pathAfterLastSlash) - 2;
            m_url.m_pathEnd = std::max(2u, m_url.m_pathEnd) - 2;
            m_url.m_queryEnd = std::max(2u, m_url.m_queryEnd) - 2;
        }
        return;
    }
    ASSERT_NOT_REACHED();
}

constexpr std::array<uint8_t, 2> dotASCIICode { '2', 'e' };

template<typename CharacterType>
ALWAYS_INLINE bool URLParser::isSingleDotPathSegment(CodePointIterator<CharacterType> c)
{
    if (c.atEnd())
        return false;
    if (*c == '.') {
        advance<CharacterType, ReportSyntaxViolation::No>(c);
        return c.atEnd() || isSlashQuestionOrHash(*c);
    }
    if (*c != '%')
        return false;
    advance<CharacterType, ReportSyntaxViolation::No>(c);
    if (c.atEnd() || *c != dotASCIICode[0])
        return false;
    advance<CharacterType, ReportSyntaxViolation::No>(c);
    if (c.atEnd())
        return false;
    if (isASCIIAlphaCaselessEqual(*c, dotASCIICode[1])) {
        advance<CharacterType, ReportSyntaxViolation::No>(c);
        return c.atEnd() || isSlashQuestionOrHash(*c);
    }
    return false;
}

template<typename CharacterType>
ALWAYS_INLINE bool URLParser::isDoubleDotPathSegment(CodePointIterator<CharacterType> c)
{
    if (c.atEnd())
        return false;
    if (*c == '.') {
        advance<CharacterType, ReportSyntaxViolation::No>(c);
        return isSingleDotPathSegment(c);
    }
    if (*c != '%')
        return false;
    advance<CharacterType, ReportSyntaxViolation::No>(c);
    if (c.atEnd() || *c != dotASCIICode[0])
        return false;
    advance<CharacterType, ReportSyntaxViolation::No>(c);
    if (c.atEnd())
        return false;
    if (isASCIIAlphaCaselessEqual(*c, dotASCIICode[1])) {
        advance<CharacterType, ReportSyntaxViolation::No>(c);
        return isSingleDotPathSegment(c);
    }
    return false;
}

template<typename CharacterType>
void URLParser::consumeSingleDotPathSegment(CodePointIterator<CharacterType>& c)
{
    ASSERT(isSingleDotPathSegment(c));
    if (*c == '.') {
        advance(c);
        if (!c.atEnd()) {
            if (*c == '/' || *c == '\\')
                advance(c);
            else
                ASSERT(*c == '?' || *c == '#');
        }
    } else {
        ASSERT(*c == '%');
        advance(c);
        ASSERT(*c == dotASCIICode[0]);
        advance(c);
        ASSERT(isASCIIAlphaCaselessEqual(*c, dotASCIICode[1]));
        advance(c);
        if (!c.atEnd()) {
            if (*c == '/' || *c == '\\')
                advance(c);
            else
                ASSERT(*c == '?' || *c == '#');
        }
    }
}

template<typename CharacterType>
void URLParser::consumeDoubleDotPathSegment(CodePointIterator<CharacterType>& c)
{
    ASSERT(isDoubleDotPathSegment(c));
    if (*c == '.')
        advance(c);
    else {
        ASSERT(*c == '%');
        advance(c);
        ASSERT(*c == dotASCIICode[0]);
        advance(c);
        ASSERT(isASCIIAlphaCaselessEqual(*c, dotASCIICode[1]));
        advance(c);
    }
    consumeSingleDotPathSegment(c);
}

bool URLParser::shouldPopPath(unsigned newPathAfterLastSlash)
{
    ASSERT(m_didSeeSyntaxViolation);
    if (!m_urlIsFile)
        return true;

    ASSERT(m_url.m_pathAfterLastSlash <= m_asciiBuffer.size());
    CodePointIterator<LChar> componentToPop(m_asciiBuffer.subspan(newPathAfterLastSlash, m_url.m_pathAfterLastSlash - newPathAfterLastSlash));
    if (newPathAfterLastSlash == m_url.m_hostEnd + m_url.m_portLength + 1 && isWindowsDriveLetter(componentToPop))
        return false;
    return true;
}

void URLParser::popPath()
{
    ASSERT(m_didSeeSyntaxViolation);
    if (m_url.m_pathAfterLastSlash > m_url.m_hostEnd + m_url.m_portLength + 1) {
        auto newPathAfterLastSlash = m_url.m_pathAfterLastSlash - 1;
        if (m_asciiBuffer[newPathAfterLastSlash] == '/')
            newPathAfterLastSlash--;
        while (newPathAfterLastSlash > m_url.m_hostEnd + m_url.m_portLength && m_asciiBuffer[newPathAfterLastSlash] != '/')
            newPathAfterLastSlash--;
        newPathAfterLastSlash++;
        if (shouldPopPath(newPathAfterLastSlash))
            m_url.m_pathAfterLastSlash = newPathAfterLastSlash;
    }
    m_asciiBuffer.resize(m_url.m_pathAfterLastSlash);
}

template<typename CharacterType>
void URLParser::syntaxViolation(const CodePointIterator<CharacterType>& iterator)
{
    if (m_didSeeSyntaxViolation)
        return;
    m_didSeeSyntaxViolation = true;
    
    ASSERT(m_asciiBuffer.isEmpty());
    size_t codeUnitsToCopy = iterator.codeUnitsSince(reinterpret_cast<const CharacterType*>(m_inputBegin));
    RELEASE_ASSERT(codeUnitsToCopy <= m_inputString.length());
    if (m_inputString.is8Bit())
        m_asciiBuffer.append(m_inputString.span8().first(codeUnitsToCopy));
    else
        m_asciiBuffer.append(m_inputString.span16().first(codeUnitsToCopy));
}

void URLParser::failure()
{
    m_url.invalidate();
    m_url.m_string = m_inputString;
}

template<typename CharacterType>
bool URLParser::checkLocalhostCodePoint(CodePointIterator<CharacterType>& iterator, char32_t codePoint)
{
    if (iterator.atEnd() || toASCIILower(*iterator) != codePoint)
        return false;
    advance<CharacterType, ReportSyntaxViolation::No>(iterator);
    return true;
}

template<typename CharacterType>
bool URLParser::isAtLocalhost(CodePointIterator<CharacterType> iterator)
{
    if (!checkLocalhostCodePoint(iterator, 'l'))
        return false;
    if (!checkLocalhostCodePoint(iterator, 'o'))
        return false;
    if (!checkLocalhostCodePoint(iterator, 'c'))
        return false;
    if (!checkLocalhostCodePoint(iterator, 'a'))
        return false;
    if (!checkLocalhostCodePoint(iterator, 'l'))
        return false;
    if (!checkLocalhostCodePoint(iterator, 'h'))
        return false;
    if (!checkLocalhostCodePoint(iterator, 'o'))
        return false;
    if (!checkLocalhostCodePoint(iterator, 's'))
        return false;
    if (!checkLocalhostCodePoint(iterator, 't'))
        return false;
    return iterator.atEnd();
}

bool URLParser::isLocalhost(StringView view)
{
    if (view.is8Bit())
        return isAtLocalhost<LChar>(view.span8());
    return isAtLocalhost<UChar>(view.span16());
}

ALWAYS_INLINE StringView URLParser::parsedDataView(size_t start, size_t length)
{
    if (UNLIKELY(m_didSeeSyntaxViolation)) {
        ASSERT(start + length <= m_asciiBuffer.size());
        return m_asciiBuffer.subspan(start, length);
    }
    ASSERT(start + length <= m_inputString.length());
    return StringView(m_inputString).substring(start, length);
}

ALWAYS_INLINE UChar URLParser::parsedDataView(size_t position)
{
    if (UNLIKELY(m_didSeeSyntaxViolation))
        return m_asciiBuffer[position];
    return m_inputString[position];
}

template<typename CharacterType>
ALWAYS_INLINE size_t URLParser::currentPosition(const CodePointIterator<CharacterType>& iterator)
{
    if (UNLIKELY(m_didSeeSyntaxViolation))
        return m_asciiBuffer.size();
    
    return iterator.codeUnitsSince(reinterpret_cast<const CharacterType*>(m_inputBegin));
}

URLParser::URLParser(String&& input, const URL& base, const URLTextEncoding* nonUTF8QueryEncoding)
    : m_inputString(WTFMove(input))
{
    if (m_inputString.isNull()) {
        if (base.isValid() && !base.m_hasOpaquePath) {
            m_url = base;
            m_url.removeFragmentIdentifier();
        }
        return;
    }

    if (m_inputString.is8Bit()) {
        auto characters = m_inputString.span8();
        m_inputBegin = characters.data();
        parse(characters, base, nonUTF8QueryEncoding);
    } else {
        auto characters = m_inputString.span16();
        m_inputBegin = characters.data();
        parse(characters, base, nonUTF8QueryEncoding);
    }

    ASSERT(!m_url.m_isValid
        || m_didSeeSyntaxViolation == (m_url.string() != m_inputString)
        || (m_inputString.containsOnly<isC0ControlOrSpace>() && m_url.m_string == base.m_string.left(base.m_queryEnd))
        || (base.isValid() && base.protocolIsFile()));
    ASSERT(internalValuesConsistent(m_url));
#if ASSERT_ENABLED
    if (!m_didSeeSyntaxViolation) {
        // Force a syntax violation at the beginning to make sure we get the same result.
        URLParser parser(makeString(' ', m_inputString), base, nonUTF8QueryEncoding);
        URL parsed = parser.result();
        if (parsed.isValid())
            ASSERT(allValuesEqual(parser.result(), m_url));
    }
#endif // ASSERT_ENABLED

    if (UNLIKELY(needsNonSpecialDotSlash()))
        addNonSpecialDotSlash();
}

template<typename CharacterType>
void URLParser::parse(std::span<const CharacterType> input, const URL& base, const URLTextEncoding* nonUTF8QueryEncoding)
{
    URL_PARSER_LOG("Parsing URL <%s> base <%s>", String(input).utf8().data(), base.string().utf8().data());
    m_url = { };
    ASSERT(m_asciiBuffer.isEmpty());

    Vector<UChar> queryBuffer;

    auto endIndex = input.size();
    if (UNLIKELY(nonUTF8QueryEncoding == URLTextEncodingSentinelAllowingC0AtEnd))
        nonUTF8QueryEncoding = nullptr;
    else {
        while (UNLIKELY(endIndex && isC0ControlOrSpace(input[endIndex - 1]))) {
            syntaxViolation<CharacterType>(input);
            endIndex--;
        }
    }
    CodePointIterator<CharacterType> c(input.first(endIndex));
    CodePointIterator<CharacterType> authorityOrHostBegin;
    CodePointIterator<CharacterType> queryBegin;
    while (UNLIKELY(!c.atEnd() && isC0ControlOrSpace(*c))) {
        syntaxViolation(c);
        ++c;
    }
    auto beginAfterControlAndSpace = c;

    enum class State : uint8_t {
        SchemeStart,
        Scheme,
        NoScheme,
        SpecialRelativeOrAuthority,
        PathOrAuthority,
        Relative,
        RelativeSlash,
        SpecialAuthoritySlashes,
        SpecialAuthorityIgnoreSlashes,
        AuthorityOrHost,
        Host,
        File,
        FileSlash,
        FileHost,
        PathStart,
        Path,
        OpaquePath,
        UTF8Query,
        NonUTF8Query,
        Fragment,
    };

#define LOG_STATE(x) URL_PARSER_LOG("State %s, code point %c, parsed data <%s> size %zu", x, *c, parsedDataView(0, currentPosition(c)).utf8().data(), currentPosition(c))
#define LOG_FINAL_STATE(x) URL_PARSER_LOG("Final State: %s", x)

    State state = State::SchemeStart;
    while (!c.atEnd()) {
        if (UNLIKELY(isTabOrNewline(*c))) {
            syntaxViolation(c);
            ++c;
            continue;
        }

        switch (state) {
        case State::SchemeStart:
            LOG_STATE("SchemeStart");
            if (isASCIIAlpha(*c)) {
                if (UNLIKELY(isASCIIUpper(*c)))
                    syntaxViolation(c);
                appendToASCIIBuffer(toASCIILower(*c));
                advance(c);
                if (c.atEnd()) {
                    m_asciiBuffer.clear();
                    state = State::NoScheme;
                    c = beginAfterControlAndSpace;
                    break;
                }
                state = State::Scheme;
            } else
                state = State::NoScheme;
            break;
        case State::Scheme:
            LOG_STATE("Scheme");
            if (isValidSchemeCharacter(*c)) {
                if (UNLIKELY(isASCIIUpper(*c)))
                    syntaxViolation(c);
                appendToASCIIBuffer(toASCIILower(*c));
            } else if (*c == ':') {
                unsigned schemeEnd = currentPosition(c);
                if (schemeEnd > URL::maxSchemeLength) {
                    failure();
                    return;
                }
                m_url.m_schemeEnd = schemeEnd;
                appendToASCIIBuffer(':');
                StringView urlScheme = parsedDataView(0, m_url.m_schemeEnd);
                switch (scheme(urlScheme)) {
                case Scheme::File:
                    m_urlIsSpecial = true;
                    m_urlIsFile = true;
                    state = State::File;
                    ++c;
                    break;
                case Scheme::WS:
                case Scheme::WSS:
                    nonUTF8QueryEncoding = nullptr;
                    m_urlIsSpecial = true;
                    if (base.protocolIs(urlScheme))
                        state = State::SpecialRelativeOrAuthority;
                    else
                        state = State::SpecialAuthoritySlashes;
                    ++c;
                    break;
                case Scheme::HTTP:
                case Scheme::HTTPS:
                    m_url.m_protocolIsInHTTPFamily = true;
                    FALLTHROUGH;
                case Scheme::FTP:
                    m_urlIsSpecial = true;
                    if (base.protocolIs(urlScheme))
                        state = State::SpecialRelativeOrAuthority;
                    else
                        state = State::SpecialAuthoritySlashes;
                    ++c;
                    break;
                case Scheme::NonSpecial:
                    nonUTF8QueryEncoding = nullptr;
                    auto maybeSlash = c;
                    advance(maybeSlash);
                    if (!maybeSlash.atEnd() && *maybeSlash == '/') {
                        appendToASCIIBuffer('/');
                        c = maybeSlash;
                        state = State::PathOrAuthority;
                        ASSERT(*c == '/');
                        ++c;
                        m_url.m_userStart = currentPosition(c);
                    } else {
                        ++c;
                        m_url.m_userStart = currentPosition(c);
                        m_url.m_userEnd = m_url.m_userStart;
                        m_url.m_passwordEnd = m_url.m_userStart;
                        m_url.m_hostEnd = m_url.m_userStart;
                        m_url.m_portLength = 0;
                        m_url.m_pathAfterLastSlash = m_url.m_userStart;
                        m_url.m_hasOpaquePath = true;
                        state = State::OpaquePath;
                    }
                    break;
                }
                break;
            } else {
                m_asciiBuffer.clear();
                state = State::NoScheme;
                c = beginAfterControlAndSpace;
                break;
            }
            advance(c);
            if (c.atEnd()) {
                m_asciiBuffer.clear();
                state = State::NoScheme;
                c = beginAfterControlAndSpace;
            }
            break;
        case State::NoScheme:
            LOG_STATE("NoScheme");
            if (!base.isValid() || (base.m_hasOpaquePath && *c != '#')) {
                failure();
                return;
            }
            if (base.m_hasOpaquePath && *c == '#') {
                copyURLPartsUntil(base, URLPart::QueryEnd, c, nonUTF8QueryEncoding);
                state = State::Fragment;
                appendToASCIIBuffer('#');
                ++c;
                break;
            }
            if (!base.protocolIsFile()) {
                state = State::Relative;
                break;
            }
            state = State::File;
            break;
        case State::SpecialRelativeOrAuthority:
            LOG_STATE("SpecialRelativeOrAuthority");
            if (*c == '/') {
                appendToASCIIBuffer('/');
                advance(c);
                if (c.atEnd()) {
                    failure();
                    return;
                }
                if (*c == '/') {
                    appendToASCIIBuffer('/');
                    state = State::SpecialAuthorityIgnoreSlashes;
                    ++c;
                } else
                    state = State::RelativeSlash;
            } else
                state = State::Relative;
            break;
        case State::PathOrAuthority:
            LOG_STATE("PathOrAuthority");
            if (*c == '/') {
                appendToASCIIBuffer('/');
                state = State::AuthorityOrHost;
                advance(c);
                m_url.m_userStart = currentPosition(c);
                authorityOrHostBegin = c;
            } else {
                ASSERT(parsedDataView(currentPosition(c) - 1) == '/');
                m_url.m_userStart = currentPosition(c) - 1;
                m_url.m_userEnd = m_url.m_userStart;
                m_url.m_passwordEnd = m_url.m_userStart;
                m_url.m_hostEnd = m_url.m_userStart;
                m_url.m_portLength = 0;
                m_url.m_pathAfterLastSlash = m_url.m_userStart + 1;
                state = State::Path;
            }
            break;
        case State::Relative:
            LOG_STATE("Relative");
            switch (*c) {
            case '/':
            case '\\':
                state = State::RelativeSlash;
                ++c;
                break;
            case '?':
                copyURLPartsUntil(base, URLPart::PathEnd, c, nonUTF8QueryEncoding);
                appendToASCIIBuffer('?');
                ++c;
                if (nonUTF8QueryEncoding) {
                    queryBegin = c;
                    state = State::NonUTF8Query;
                } else
                    state = State::UTF8Query;
                break;
            case '#':
                copyURLPartsUntil(base, URLPart::QueryEnd, c, nonUTF8QueryEncoding);
                appendToASCIIBuffer('#');
                state = State::Fragment;
                ++c;
                break;
            default:
                copyURLPartsUntil(base, URLPart::PathAfterLastSlash, c, nonUTF8QueryEncoding);
                if ((currentPosition(c) && parsedDataView(currentPosition(c) - 1) != '/')
                    || (base.host().isEmpty() && base.path().isEmpty())) {
                    appendToASCIIBuffer('/');
                    m_url.m_pathAfterLastSlash = currentPosition(c);
                }
                state = State::Path;
                break;
            }
            break;
        case State::RelativeSlash:
            LOG_STATE("RelativeSlash");
            if (*c == '/' || *c == '\\') {
                ++c;
                copyURLPartsUntil(base, URLPart::SchemeEnd, c, nonUTF8QueryEncoding);
                appendToASCIIBuffer("://"_span);
                if (m_urlIsSpecial)
                    state = State::SpecialAuthorityIgnoreSlashes;
                else {
                    m_url.m_userStart = currentPosition(c);
                    state = State::AuthorityOrHost;
                    authorityOrHostBegin = c;
                }
            } else {
                copyURLPartsUntil(base, URLPart::PortEnd, c, nonUTF8QueryEncoding);
                appendToASCIIBuffer('/');
                m_url.m_pathAfterLastSlash = base.m_hostEnd + base.m_portLength + 1;
                state = State::Path;
            }
            break;
        case State::SpecialAuthoritySlashes:
            LOG_STATE("SpecialAuthoritySlashes");
            if (LIKELY(*c == '/' || *c == '\\')) {
                if (UNLIKELY(*c == '\\'))
                    syntaxViolation(c);
                appendToASCIIBuffer('/');
                advance(c);
                if (LIKELY(!c.atEnd() && (*c == '/' || *c == '\\'))) {
                    if (UNLIKELY(*c == '\\'))
                        syntaxViolation(c);
                    ++c;
                    appendToASCIIBuffer('/');
                } else {
                    syntaxViolation(c);
                    appendToASCIIBuffer('/');
                }
            } else {
                syntaxViolation(c);
                appendToASCIIBuffer("//"_span);
            }
            state = State::SpecialAuthorityIgnoreSlashes;
            break;
        case State::SpecialAuthorityIgnoreSlashes:
            LOG_STATE("SpecialAuthorityIgnoreSlashes");
            if (*c == '/' || *c == '\\') {
                syntaxViolation(c);
                ++c;
            } else {
                m_url.m_userStart = currentPosition(c);
                state = State::AuthorityOrHost;
                authorityOrHostBegin = c;
            }
            break;
        case State::AuthorityOrHost:
            do {
                LOG_STATE("AuthorityOrHost");
                if (*c == '@') {
                    auto lastAt = c;
                    auto findLastAt = c;
                    while (!findLastAt.atEnd()) {
                        URL_PARSER_LOG("Finding last @: %c", *findLastAt);
                        if (*findLastAt == '@')
                            lastAt = findLastAt;
                        bool isSlash = *findLastAt == '/' || (m_urlIsSpecial && *findLastAt == '\\');
                        if (isSlash || *findLastAt == '?' || *findLastAt == '#')
                            break;
                        ++findLastAt;
                    }
                    parseAuthority(CodePointIterator<CharacterType>(authorityOrHostBegin, lastAt));
                    c = lastAt;
                    advance(c);
                    authorityOrHostBegin = c;
                    state = State::Host;
                    m_hostHasPercentOrNonASCII = false;
                    break;
                }
                bool isSlash = *c == '/' || (m_urlIsSpecial && *c == '\\');
                if (isSlash || *c == '?' || *c == '#') {
                    auto iterator = CodePointIterator<CharacterType>(authorityOrHostBegin, c);
                    if (iterator.atEnd()) {
                        if (m_urlIsSpecial)
                            return failure();
                        m_url.m_userEnd = currentPosition(c);
                        m_url.m_passwordEnd = m_url.m_userEnd;
                        m_url.m_hostEnd = m_url.m_userEnd;
                        m_url.m_portLength = 0;
                        m_url.m_pathAfterLastSlash = m_url.m_userEnd;
                    } else {
                        m_url.m_userEnd = currentPosition(authorityOrHostBegin);
                        m_url.m_passwordEnd = m_url.m_userEnd;
                        if (parseHostAndPort(iterator) == HostParsingResult::InvalidHost) {
                            failure();
                            return;
                        }
                        if (UNLIKELY(!isSlash)) {
                            if (m_urlIsSpecial) {
                                syntaxViolation(c);
                                appendToASCIIBuffer('/');
                            }
                            m_url.m_pathAfterLastSlash = currentPosition(c);
                        }
                    }
                    state = State::Path;
                    break;
                }
                if (isPercentOrNonASCII(*c))
                    m_hostHasPercentOrNonASCII = true;
                ++c;
            } while (!c.atEnd());
            break;
        case State::Host:
            do {
                LOG_STATE("Host");
                bool isSlash = *c == '/' || (m_urlIsSpecial && *c == '\\');
                if (isSlash || *c == '?' || *c == '#') {
                    if (parseHostAndPort(CodePointIterator<CharacterType>(authorityOrHostBegin, c)) == HostParsingResult::InvalidHost) {
                        failure();
                        return;
                    }
                    if (*c == '?' || *c == '#') {
                        syntaxViolation(c);
                        appendToASCIIBuffer('/');
                        m_url.m_pathAfterLastSlash = currentPosition(c);
                    }
                    state = State::Path;
                    break;
                }
                if (isPercentOrNonASCII(*c))
                    m_hostHasPercentOrNonASCII = true;
                ++c;
            } while (!c.atEnd());
            break;
        case State::File:
            LOG_STATE("File");
            switch (*c) {
            case '\\':
                syntaxViolation(c);
                FALLTHROUGH;
            case '/':
                appendToASCIIBuffer('/');
                state = State::FileSlash;
                ++c;
                break;
            case '?':
                syntaxViolation(c);
                if (base.isValid() && base.protocolIsFile()) {
                    copyURLPartsUntil(base, URLPart::PathEnd, c, nonUTF8QueryEncoding);
                    appendToASCIIBuffer('?');
                    ++c;
                } else {
                    appendToASCIIBuffer("///?"_span);
                    ++c;
                    m_url.m_userStart = currentPosition(c) - 2;
                    m_url.m_userEnd = m_url.m_userStart;
                    m_url.m_passwordEnd = m_url.m_userStart;
                    m_url.m_hostEnd = m_url.m_userStart;
                    m_url.m_portLength = 0;
                    m_url.m_pathAfterLastSlash = m_url.m_userStart + 1;
                    m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
                }
                if (nonUTF8QueryEncoding) {
                    queryBegin = c;
                    state = State::NonUTF8Query;
                } else
                    state = State::UTF8Query;
                break;
            case '#':
                syntaxViolation(c);
                if (base.isValid() && base.protocolIsFile()) {
                    copyURLPartsUntil(base, URLPart::QueryEnd, c, nonUTF8QueryEncoding);
                    appendToASCIIBuffer('#');
                } else {
                    appendToASCIIBuffer("///#"_span);
                    m_url.m_userStart = currentPosition(c) - 2;
                    m_url.m_userEnd = m_url.m_userStart;
                    m_url.m_passwordEnd = m_url.m_userStart;
                    m_url.m_hostEnd = m_url.m_userStart;
                    m_url.m_portLength = 0;
                    m_url.m_pathAfterLastSlash = m_url.m_userStart + 1;
                    m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
                    m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
                }
                state = State::Fragment;
                ++c;
                break;
            default:
                syntaxViolation(c);
                if (base.isValid() && base.protocolIsFile() && shouldCopyFileURL(c))
                    copyURLPartsUntil(base, URLPart::PathAfterLastSlash, c, nonUTF8QueryEncoding);
                else {
                    bool copiedHost = false;
                    if (base.isValid() && base.protocolIsFile()) {
                        if (base.host().isEmpty()) {
                            copyURLPartsUntil(base, URLPart::SchemeEnd, c, nonUTF8QueryEncoding);
                            appendToASCIIBuffer(":///"_span);
                        } else {
                            copyURLPartsUntil(base, URLPart::PortEnd, c, nonUTF8QueryEncoding);
                            appendToASCIIBuffer('/');
                            copiedHost = true;
                        }
                    } else
                        appendToASCIIBuffer("///"_span);
                    if (!copiedHost) {
                        m_url.m_userStart = currentPosition(c) - 1;
                        m_url.m_userEnd = m_url.m_userStart;
                        m_url.m_passwordEnd = m_url.m_userStart;
                        m_url.m_hostEnd = m_url.m_userStart;
                        m_url.m_portLength = 0;
                    }
                    m_url.m_pathAfterLastSlash = m_url.m_hostEnd + 1;
                }
                if (isWindowsDriveLetter(c))
                    appendWindowsDriveLetter(c);
                state = State::Path;
                break;
            }
            break;
        case State::FileSlash:
            LOG_STATE("FileSlash");
            if (LIKELY(*c == '/' || *c == '\\')) {
                if (UNLIKELY(*c == '\\'))
                    syntaxViolation(c);
                if (base.isValid() && base.protocolIsFile()) {
                    copyURLPartsUntil(base, URLPart::SchemeEnd, c, nonUTF8QueryEncoding);
                    appendToASCIIBuffer(":/"_span);
                }
                appendToASCIIBuffer('/');
                advance(c);
                m_url.m_userStart = currentPosition(c);
                m_url.m_userEnd = m_url.m_userStart;
                m_url.m_passwordEnd = m_url.m_userStart;
                m_url.m_hostEnd = m_url.m_userStart;
                m_url.m_portLength = 0;
                authorityOrHostBegin = c;
                state = State::FileHost;
                break;
            }
            {
                bool copiedHost = false;
                if (base.isValid() && base.protocolIsFile()) {
                    if (base.host().isEmpty()) {
                        copyURLPartsUntil(base, URLPart::SchemeEnd, c, nonUTF8QueryEncoding);
                        appendToASCIIBuffer(":///"_span);
                    } else {
                        copyURLPartsUntil(base, URLPart::PortEnd, c, nonUTF8QueryEncoding);
                        appendToASCIIBuffer('/');
                        copiedHost = true;
                    }
                } else {
                    syntaxViolation(c);
                    appendToASCIIBuffer("//"_span);
                }
                if (!copiedHost) {
                    m_url.m_userStart = currentPosition(c) - 1;
                    m_url.m_userEnd = m_url.m_userStart;
                    m_url.m_passwordEnd = m_url.m_userStart;
                    m_url.m_hostEnd = m_url.m_userStart;
                    m_url.m_portLength = 0;
                }
            }
            if (isWindowsDriveLetter(c)) {
                appendWindowsDriveLetter(c);
                m_url.m_pathAfterLastSlash = m_url.m_hostEnd + 1;
            } else if (copyBaseWindowsDriveLetter(base)) {
                appendToASCIIBuffer('/');
                m_url.m_pathAfterLastSlash = m_url.m_hostEnd + 4;
            } else
                m_url.m_pathAfterLastSlash = m_url.m_hostEnd + 1;
            state = State::Path;
            break;
        case State::FileHost:
            do {
                LOG_STATE("FileHost");
                if (isSlashQuestionOrHash(*c)) {
                    bool windowsQuirk = takesTwoAdvancesUntilEnd(CodePointIterator<CharacterType>(authorityOrHostBegin, c))
                        && isWindowsDriveLetter(authorityOrHostBegin);
                    if (windowsQuirk) {
                        syntaxViolation(authorityOrHostBegin);
                        appendToASCIIBuffer('/');
                        appendWindowsDriveLetter(authorityOrHostBegin);
                    }
                    if (windowsQuirk || authorityOrHostBegin == c) {
                        ASSERT(windowsQuirk || parsedDataView(currentPosition(c) - 1) == '/');
                        if (UNLIKELY(*c == '?')) {
                            syntaxViolation(c);
                            appendToASCIIBuffer("/?"_span);
                            ++c;
                            if (nonUTF8QueryEncoding) {
                                queryBegin = c;
                                state = State::NonUTF8Query;
                            } else
                                state = State::UTF8Query;
                            m_url.m_pathAfterLastSlash = currentPosition(c) - 1;
                            m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
                            break;
                        }
                        if (UNLIKELY(*c == '#')) {
                            syntaxViolation(c);
                            appendToASCIIBuffer("/#"_span);
                            ++c;
                            m_url.m_pathAfterLastSlash = currentPosition(c) - 1;
                            m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
                            m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
                            state = State::Fragment;
                            break;
                        }
                        state = State::Path;
                        break;
                    }
                    if (parseHostAndPort(CodePointIterator<CharacterType>(authorityOrHostBegin, c)) == HostParsingResult::InvalidHost) {
                        failure();
                        return;
                    }
                    if (UNLIKELY(isLocalhost(parsedDataView(m_url.m_passwordEnd, currentPosition(c) - m_url.m_passwordEnd)))) {
                        syntaxViolation(c);
                        m_asciiBuffer.shrink(m_url.m_passwordEnd);
                        m_url.m_hostEnd = currentPosition(c);
                        m_url.m_portLength = 0;
                    }
                    
                    state = State::PathStart;
                    break;
                }
                if (isPercentOrNonASCII(*c))
                    m_hostHasPercentOrNonASCII = true;
                ++c;
            } while (!c.atEnd());
            break;
        case State::PathStart:
            LOG_STATE("PathStart");
            if (*c != '/' && *c != '\\') {
                syntaxViolation(c);
                appendToASCIIBuffer('/');
            }
            m_url.m_pathAfterLastSlash = currentPosition(c);
            state = State::Path;
            break;
        case State::Path:
            LOG_STATE("Path");
            if (*c == '/' || (m_urlIsSpecial && *c == '\\')) {
                if (UNLIKELY(m_urlIsSpecial && *c == '\\'))
                    syntaxViolation(c);
                appendToASCIIBuffer('/');
                ++c;
                m_url.m_pathAfterLastSlash = currentPosition(c);
                break;
            }
            if (UNLIKELY(currentPosition(c) && parsedDataView(currentPosition(c) - 1) == '/')) {
                if (UNLIKELY(isDoubleDotPathSegment(c))) {
                    syntaxViolation(c);
                    consumeDoubleDotPathSegment(c);
                    popPath();
                    break;
                }
                if (UNLIKELY(isSingleDotPathSegment(c))) {
                    syntaxViolation(c);
                    consumeSingleDotPathSegment(c);
                    break;
                }
            }
            if (*c == '?') {
                m_url.m_pathEnd = currentPosition(c);
                appendToASCIIBuffer('?');
                ++c;
                if (nonUTF8QueryEncoding) {
                    queryBegin = c;
                    state = State::NonUTF8Query;
                } else
                    state = State::UTF8Query;
                break;
            }
            if (*c == '#') {
                m_url.m_pathEnd = currentPosition(c);
                m_url.m_queryEnd = m_url.m_pathEnd;
                state = State::Fragment;
                break;
            }
            utf8PercentEncode<isInPathEncodeSet>(c);
            ++c;
            break;
        case State::OpaquePath:
            LOG_STATE("OpaquePath");
            if (*c == '?') {
                m_url.m_pathEnd = currentPosition(c);
                appendToASCIIBuffer('?');
                ++c;
                if (nonUTF8QueryEncoding) {
                    queryBegin = c;
                    state = State::NonUTF8Query;
                } else
                    state = State::UTF8Query;
            } else if (*c == '#') {
                m_url.m_pathEnd = currentPosition(c);
                m_url.m_queryEnd = m_url.m_pathEnd;
                state = State::Fragment;
            } else if (*c == '/') {
                appendToASCIIBuffer('/');
                ++c;
                m_url.m_pathAfterLastSlash = currentPosition(c);
            } else {
                utf8PercentEncode<isInC0ControlEncodeSet>(c);
                ++c;
            }
            break;
        case State::UTF8Query:
            LOG_STATE("UTF8Query");
            ASSERT(queryBegin == CodePointIterator<CharacterType>());
            if (*c == '#') {
                m_url.m_queryEnd = currentPosition(c);
                state = State::Fragment;
                break;
            }
            ASSERT(!nonUTF8QueryEncoding);
            utf8QueryEncode(c);
            ++c;
            break;
        case State::NonUTF8Query:
            do {
                LOG_STATE("NonUTF8Query");
                ASSERT(queryBegin != CodePointIterator<CharacterType>());
                if (*c == '#') {
                    encodeNonUTF8Query(queryBuffer, *nonUTF8QueryEncoding, CodePointIterator<CharacterType>(queryBegin, c));
                    m_url.m_queryEnd = currentPosition(c);
                    state = State::Fragment;
                    break;
                }
                appendCodePoint(queryBuffer, *c);
                advance(c, queryBegin);
            } while (!c.atEnd());
            break;
        case State::Fragment:
            URL_PARSER_LOG("State Fragment");
            utf8PercentEncode<isInFragmentEncodeSet>(c);
            ++c;
            break;
        }
    }

    switch (state) {
    case State::SchemeStart:
        LOG_FINAL_STATE("SchemeStart");
        if (!currentPosition(c) && base.isValid() && !base.m_hasOpaquePath) {
            m_url = base;
            m_url.removeFragmentIdentifier();
            return;
        }
        failure();
        return;
    case State::Scheme:
        LOG_FINAL_STATE("Scheme");
        failure();
        return;
    case State::NoScheme:
        LOG_FINAL_STATE("NoScheme");
        RELEASE_ASSERT_NOT_REACHED();
    case State::SpecialRelativeOrAuthority:
        LOG_FINAL_STATE("SpecialRelativeOrAuthority");
        copyURLPartsUntil(base, URLPart::QueryEnd, c, nonUTF8QueryEncoding);
        break;
    case State::PathOrAuthority:
        LOG_FINAL_STATE("PathOrAuthority");
        ASSERT(m_url.m_userStart);
        ASSERT(m_url.m_userStart == currentPosition(c));
        ASSERT(parsedDataView(currentPosition(c) - 1) == '/');
        m_url.m_userStart--;
        m_url.m_userEnd = m_url.m_userStart;
        m_url.m_passwordEnd = m_url.m_userStart;
        m_url.m_hostEnd = m_url.m_userStart;
        m_url.m_portLength = 0;
        m_url.m_pathAfterLastSlash = m_url.m_userStart + 1;
        m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
        m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
        break;
    case State::Relative:
        LOG_FINAL_STATE("Relative");
        RELEASE_ASSERT_NOT_REACHED();
    case State::RelativeSlash:
        LOG_FINAL_STATE("RelativeSlash");
        copyURLPartsUntil(base, URLPart::PortEnd, c, nonUTF8QueryEncoding);
        appendToASCIIBuffer('/');
        m_url.m_pathAfterLastSlash = m_url.m_hostEnd + m_url.m_portLength + 1;
        m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
        m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
        break;
    case State::SpecialAuthoritySlashes:
        LOG_FINAL_STATE("SpecialAuthoritySlashes");
        failure();
        return;
    case State::SpecialAuthorityIgnoreSlashes:
        LOG_FINAL_STATE("SpecialAuthorityIgnoreSlashes");
        failure();
        return;
    case State::AuthorityOrHost:
        LOG_FINAL_STATE("AuthorityOrHost");
        m_url.m_userEnd = currentPosition(authorityOrHostBegin);
        m_url.m_passwordEnd = m_url.m_userEnd;
        if (authorityOrHostBegin.atEnd()) {
            m_url.m_userEnd = m_url.m_userStart;
            m_url.m_passwordEnd = m_url.m_userStart;
            m_url.m_hostEnd = m_url.m_userStart;
            m_url.m_portLength = 0;
            m_url.m_pathEnd = m_url.m_userStart;
        } else if (parseHostAndPort(authorityOrHostBegin) == HostParsingResult::InvalidHost) {
            failure();
            return;
        } else {
            if (m_urlIsSpecial) {
                syntaxViolation(c);
                appendToASCIIBuffer('/');
                m_url.m_pathEnd = m_url.m_hostEnd + m_url.m_portLength + 1;
            } else
                m_url.m_pathEnd = m_url.m_hostEnd + m_url.m_portLength;
        }
        m_url.m_pathAfterLastSlash = m_url.m_pathEnd;
        m_url.m_queryEnd = m_url.m_pathEnd;
        break;
    case State::Host:
        LOG_FINAL_STATE("Host");
        if (parseHostAndPort(authorityOrHostBegin) == HostParsingResult::InvalidHost) {
            failure();
            return;
        }
        if (m_urlIsSpecial) {
            syntaxViolation(c);
            appendToASCIIBuffer('/');
            m_url.m_pathEnd = m_url.m_hostEnd + m_url.m_portLength + 1;
        } else
            m_url.m_pathEnd = m_url.m_hostEnd + m_url.m_portLength;
        m_url.m_pathAfterLastSlash = m_url.m_pathEnd;
        m_url.m_queryEnd = m_url.m_pathEnd;
        break;
    case State::File:
        LOG_FINAL_STATE("File");
        if (base.isValid() && base.protocolIsFile()) {
            copyURLPartsUntil(base, URLPart::QueryEnd, c, nonUTF8QueryEncoding);
            break;
        }
        syntaxViolation(c);
        appendToASCIIBuffer("///"_span);
        m_url.m_userStart = currentPosition(c) - 1;
        m_url.m_userEnd = m_url.m_userStart;
        m_url.m_passwordEnd = m_url.m_userStart;
        m_url.m_hostEnd = m_url.m_userStart;
        m_url.m_portLength = 0;
        m_url.m_pathAfterLastSlash = m_url.m_userStart + 1;
        m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
        m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
        break;
    case State::FileSlash:
        LOG_FINAL_STATE("FileSlash");
        syntaxViolation(c);
        {
            bool copiedHost = false;
            if (base.isValid() && base.protocolIsFile()) {
                if (base.host().isEmpty()) {
                    copyURLPartsUntil(base, URLPart::SchemeEnd, c, nonUTF8QueryEncoding);
                    appendToASCIIBuffer(":/"_span);
                } else {
                    copyURLPartsUntil(base, URLPart::PortEnd, c, nonUTF8QueryEncoding);
                    appendToASCIIBuffer('/');
                    copiedHost = true;
                }
            }
            if (!copiedHost) {
                m_url.m_userStart = currentPosition(c) + 1;
                appendToASCIIBuffer("//"_span);
                m_url.m_userEnd = m_url.m_userStart;
                m_url.m_passwordEnd = m_url.m_userStart;
                m_url.m_hostEnd = m_url.m_userStart;
                m_url.m_portLength = 0;
            }
        }
        if (copyBaseWindowsDriveLetter(base)) {
            appendToASCIIBuffer('/');
            m_url.m_pathAfterLastSlash = m_url.m_hostEnd + 4;
        } else
            m_url.m_pathAfterLastSlash = m_url.m_hostEnd + 1;
        m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
        m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
        break;
    case State::FileHost:
        LOG_FINAL_STATE("FileHost");
        if (takesTwoAdvancesUntilEnd(CodePointIterator<CharacterType>(authorityOrHostBegin, c))
            && isWindowsDriveLetter(authorityOrHostBegin)) {
            syntaxViolation(authorityOrHostBegin);
            appendToASCIIBuffer('/');
            appendWindowsDriveLetter(authorityOrHostBegin);
            m_url.m_pathAfterLastSlash = currentPosition(c);
            m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
            m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
            break;
        }
        
        if (authorityOrHostBegin == c) {
            syntaxViolation(c);
            appendToASCIIBuffer('/');
            m_url.m_userStart = currentPosition(c) - 1;
            m_url.m_userEnd = m_url.m_userStart;
            m_url.m_passwordEnd = m_url.m_userStart;
            m_url.m_hostEnd = m_url.m_userStart;
            m_url.m_portLength = 0;
            m_url.m_pathAfterLastSlash = m_url.m_userStart + 1;
            m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
            m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
            break;
        }

        if (parseHostAndPort(CodePointIterator<CharacterType>(authorityOrHostBegin, c)) == HostParsingResult::InvalidHost) {
            failure();
            return;
        }

        syntaxViolation(c);
        if (isLocalhost(parsedDataView(m_url.m_passwordEnd, currentPosition(c) - m_url.m_passwordEnd))) {
            m_asciiBuffer.shrink(m_url.m_passwordEnd);
            m_url.m_hostEnd = currentPosition(c);
            m_url.m_portLength = 0;
        }
        appendToASCIIBuffer('/');
        m_url.m_pathAfterLastSlash = m_url.m_hostEnd + m_url.m_portLength + 1;
        m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
        m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
        break;
    case State::PathStart:
        LOG_FINAL_STATE("PathStart");
        RELEASE_ASSERT_NOT_REACHED();
    case State::Path:
        LOG_FINAL_STATE("Path");
        m_url.m_pathEnd = currentPosition(c);
        m_url.m_queryEnd = m_url.m_pathEnd;
        break;
    case State::OpaquePath:
        LOG_FINAL_STATE("OpaquePath");
        m_url.m_pathEnd = currentPosition(c);
        m_url.m_queryEnd = m_url.m_pathEnd;
        break;
    case State::UTF8Query:
        LOG_FINAL_STATE("UTF8Query");
        ASSERT(queryBegin == CodePointIterator<CharacterType>());
        m_url.m_queryEnd = currentPosition(c);
        break;
    case State::NonUTF8Query:
        LOG_FINAL_STATE("NonUTF8Query");
        ASSERT(queryBegin != CodePointIterator<CharacterType>());
        encodeNonUTF8Query(queryBuffer, *nonUTF8QueryEncoding, CodePointIterator<CharacterType>(queryBegin, c));
        m_url.m_queryEnd = currentPosition(c);
        break;
    case State::Fragment:
        LOG_FINAL_STATE("Fragment");
        break;
    }

    if (LIKELY(!m_didSeeSyntaxViolation)) {
        m_url.m_string = m_inputString;
        ASSERT(m_asciiBuffer.isEmpty());
    } else
        m_url.m_string = String::adopt(WTFMove(m_asciiBuffer));
    m_url.m_isValid = true;
    URL_PARSER_LOG("Parsed URL <%s>\n\n", m_url.m_string.utf8().data());
}

template<typename CharacterType>
void URLParser::parseAuthority(CodePointIterator<CharacterType> iterator)
{
    if (UNLIKELY(iterator.atEnd())) {
        syntaxViolation(iterator);
        m_url.m_userEnd = currentPosition(iterator);
        m_url.m_passwordEnd = m_url.m_userEnd;
        return;
    }
    for (; !iterator.atEnd(); advance(iterator)) {
        if (*iterator == ':') {
            m_url.m_userEnd = currentPosition(iterator);
            auto iteratorAtColon = iterator;
            ++iterator;
            bool tabOrNewlineAfterColon = false;
            while (UNLIKELY(!iterator.atEnd() && isTabOrNewline(*iterator))) {
                tabOrNewlineAfterColon = true;
                ++iterator;
            }
            if (UNLIKELY(iterator.atEnd())) {
                syntaxViolation(iteratorAtColon);
                m_url.m_passwordEnd = m_url.m_userEnd;
                if (m_url.m_userEnd > m_url.m_userStart)
                    appendToASCIIBuffer('@');
                return;
            }
            if (tabOrNewlineAfterColon)
                syntaxViolation(iteratorAtColon);
            appendToASCIIBuffer(':');
            break;
        }
        utf8PercentEncode<WTF::isInUserInfoEncodeSet>(iterator);
    }
    for (; !iterator.atEnd(); advance(iterator))
        utf8PercentEncode<WTF::isInUserInfoEncodeSet>(iterator);
    m_url.m_passwordEnd = currentPosition(iterator);
    if (!m_url.m_userEnd)
        m_url.m_userEnd = m_url.m_passwordEnd;
    appendToASCIIBuffer('@');
}

template<typename UnsignedIntegerType>
void URLParser::appendNumberToASCIIBuffer(UnsignedIntegerType number)
{
    constexpr size_t bufferSize = sizeof(UnsignedIntegerType) * 3 + 1;
    std::array<LChar, bufferSize> buffer;
    size_t index = bufferSize;
    do {
        buffer[--index] = (number % 10) + '0';
        number /= 10;
    } while (number);

    appendToASCIIBuffer(std::span { buffer }.subspan(index));
}

void URLParser::serializeIPv4(IPv4Address address)
{
    appendNumberToASCIIBuffer<uint8_t>(address >> 24);
    appendToASCIIBuffer('.');
    appendNumberToASCIIBuffer<uint8_t>(address >> 16);
    appendToASCIIBuffer('.');
    appendNumberToASCIIBuffer<uint8_t>(address >> 8);
    appendToASCIIBuffer('.');
    appendNumberToASCIIBuffer<uint8_t>(address);
}
    
static size_t zeroSequenceLength(const std::array<uint16_t, 8>& address, size_t begin)
{
    size_t end = begin;
    for (; end < 8; end++) {
        if (address[end])
            break;
    }
    return end - begin;
}

static std::optional<size_t> findLongestZeroSequence(const std::array<uint16_t, 8>& address)
{
    std::optional<size_t> longest;
    size_t longestLength = 0;
    for (size_t i = 0; i < 8; i++) {
        size_t length = zeroSequenceLength(address, i);
        if (length) {
            if (length > 1 && (!longest || longestLength < length)) {
                longest = i;
                longestLength = length;
            }
            i += length;
        }
    }
    return longest;
}

void URLParser::serializeIPv6Piece(uint16_t piece)
{
    bool printed = false;
    if (auto nibble0 = piece >> 12) {
        appendToASCIIBuffer(lowerNibbleToLowercaseASCIIHexDigit(nibble0));
        printed = true;
    }
    auto nibble1 = piece >> 8 & 0xF;
    if (printed || nibble1) {
        appendToASCIIBuffer(lowerNibbleToLowercaseASCIIHexDigit(nibble1));
        printed = true;
    }
    auto nibble2 = piece >> 4 & 0xF;
    if (printed || nibble2)
        appendToASCIIBuffer(lowerNibbleToLowercaseASCIIHexDigit(nibble2));
    appendToASCIIBuffer(lowerNibbleToLowercaseASCIIHexDigit(piece & 0xF));
}

void URLParser::serializeIPv6(URLParser::IPv6Address address)
{
    appendToASCIIBuffer('[');
    auto compressPointer = findLongestZeroSequence(address);
    for (size_t piece = 0; piece < 8; piece++) {
        if (compressPointer && compressPointer.value() == piece) {
            ASSERT(!address[piece]);
            if (piece)
                appendToASCIIBuffer(':');
            else
                appendToASCIIBuffer("::"_span);
            while (piece < 8 && !address[piece])
                piece++;
            if (piece == 8)
                break;
        }
        serializeIPv6Piece(address[piece]);
        if (piece < 7)
            appendToASCIIBuffer(':');
    }
    appendToASCIIBuffer(']');
}

enum class URLParser::IPv4PieceParsingError {
    Failure,
    Overflow,
};

template<typename CharacterType>
Expected<uint32_t, URLParser::IPv4PieceParsingError> URLParser::parseIPv4Piece(CodePointIterator<CharacterType>& iterator, bool& didSeeSyntaxViolation)
{
    enum class State : uint8_t {
        UnknownBase,
        Decimal,
        OctalOrHex,
        Octal,
        Hex,
    };
    State state = State::UnknownBase;
    CheckedUint32 value = 0;
    if (!iterator.atEnd() && *iterator == '.')
        return makeUnexpected(IPv4PieceParsingError::Failure);
    while (!iterator.atEnd()) {
        if (isTabOrNewline(*iterator)) {
            didSeeSyntaxViolation = true;
            ++iterator;
            continue;
        }
        if (*iterator == '.') {
            ASSERT(!value.hasOverflowed());
            return value.value();
        }
        switch (state) {
        case State::UnknownBase:
            if (UNLIKELY(*iterator == '0')) {
                ++iterator;
                state = State::OctalOrHex;
                break;
            }
            state = State::Decimal;
            break;
        case State::OctalOrHex:
            didSeeSyntaxViolation = true;
            if (*iterator == 'x' || *iterator == 'X') {
                ++iterator;
                state = State::Hex;
                break;
            }
            state = State::Octal;
            break;
        case State::Decimal:
            if (!isASCIIDigit(*iterator))
                return makeUnexpected(IPv4PieceParsingError::Failure);
            value *= 10;
            value += *iterator - '0';
            if (UNLIKELY(value.hasOverflowed()))
                return makeUnexpected(IPv4PieceParsingError::Overflow);
            ++iterator;
            break;
        case State::Octal:
            ASSERT(didSeeSyntaxViolation);
            if (*iterator < '0' || *iterator > '7')
                return makeUnexpected(IPv4PieceParsingError::Failure);
            value *= 8;
            value += *iterator - '0';
            if (UNLIKELY(value.hasOverflowed()))
                return makeUnexpected(IPv4PieceParsingError::Overflow);
            ++iterator;
            break;
        case State::Hex:
            ASSERT(didSeeSyntaxViolation);
            if (!isASCIIHexDigit(*iterator))
                return makeUnexpected(IPv4PieceParsingError::Failure);
            value *= 16;
            value += toASCIIHexValue(*iterator);
            if (UNLIKELY(value.hasOverflowed()))
                return makeUnexpected(IPv4PieceParsingError::Overflow);
            ++iterator;
            break;
        }
    }
    ASSERT(!value.hasOverflowed());
    return value.value();
}

ALWAYS_INLINE static uint64_t pow256(size_t exponent)
{
    RELEASE_ASSERT(exponent <= 4);
    static constexpr std::array<uint64_t, 5> values { 1, 256, 256 * 256, 256 * 256 * 256, 256ull * 256 * 256 * 256 };
    return values[exponent];
}

enum class URLParser::IPv4ParsingError {
    Failure,
    NotIPv4,
};

template<typename CharacterTypeForSyntaxViolation, typename CharacterType>
Expected<URLParser::IPv4Address, URLParser::IPv4ParsingError> URLParser::parseIPv4Host(const CodePointIterator<CharacterTypeForSyntaxViolation>& iteratorForSyntaxViolationPosition, CodePointIterator<CharacterType> iterator)
{
    Vector<Expected<uint32_t, URLParser::IPv4PieceParsingError>, 4> items;
    bool didSeeSyntaxViolation = false;
    if (!iterator.atEnd() && *iterator == '.')
        return makeUnexpected(IPv4ParsingError::NotIPv4);
    while (!iterator.atEnd()) {
        if (isTabOrNewline(*iterator)) {
            didSeeSyntaxViolation = true;
            ++iterator;
            continue;
        }
        if (items.size() >= 4)
            return makeUnexpected(IPv4ParsingError::NotIPv4);
        items.append(parseIPv4Piece(iterator, didSeeSyntaxViolation));
        if (!iterator.atEnd() && *iterator == '.') {
            ++iterator;
            if (iterator.atEnd())
                didSeeSyntaxViolation = true;
            else if (*iterator == '.')
                return makeUnexpected(IPv4ParsingError::NotIPv4);
        }
    }
    if (!iterator.atEnd() || !items.size() || items.size() > 4)
        return makeUnexpected(IPv4ParsingError::NotIPv4);
    for (const auto& item : items) {
        if (!item.has_value() && item.error() == IPv4PieceParsingError::Failure)
            return makeUnexpected(IPv4ParsingError::NotIPv4);
    }
    for (const auto& item : items) {
        if (!item.has_value() && item.error() == IPv4PieceParsingError::Overflow)
            return makeUnexpected(IPv4ParsingError::Failure);
    }
    if (items.size() > 1) {
        for (size_t i = 0; i < items.size() - 1; i++) {
            if (items[i].value() > 255)
                return makeUnexpected(IPv4ParsingError::Failure);
        }
    }
    if (items[items.size() - 1].value() >= pow256(5 - items.size()))
        return makeUnexpected(IPv4ParsingError::Failure);

    if (didSeeSyntaxViolation)
        syntaxViolation(iteratorForSyntaxViolationPosition);
    for (const auto& item : items) {
        if (item.value() > 255)
            syntaxViolation(iteratorForSyntaxViolationPosition);
    }

    if (UNLIKELY(items.size() != 4))
        syntaxViolation(iteratorForSyntaxViolationPosition);

    IPv4Address ipv4 = items.takeLast().value();
    for (size_t counter = 0; counter < items.size(); ++counter)
        ipv4 += items[counter].value() * pow256(3 - counter);
    return ipv4;
}

template<typename CharacterType>
std::optional<uint32_t> URLParser::parseIPv4PieceInsideIPv6(CodePointIterator<CharacterType>& iterator)
{
    if (iterator.atEnd())
        return std::nullopt;
    uint32_t piece = 0;
    bool leadingZeros = false;
    while (!iterator.atEnd()) {
        if (!isASCIIDigit(*iterator))
            return std::nullopt;
        if (!piece && *iterator == '0') {
            if (leadingZeros)
                return std::nullopt;
            leadingZeros = true;
        }
        piece = piece * 10 + *iterator - '0';
        if (piece > 255)
            return std::nullopt;
        advance<CharacterType, ReportSyntaxViolation::No>(iterator);
        if (iterator.atEnd())
            break;
        if (*iterator == '.')
            break;
    }
    if (piece && leadingZeros)
        return std::nullopt;
    return piece;
}

template<typename CharacterType>
std::optional<URLParser::IPv4Address> URLParser::parseIPv4AddressInsideIPv6(CodePointIterator<CharacterType> iterator)
{
    IPv4Address address = 0;
    for (size_t i = 0; i < 4; ++i) {
        if (std::optional<uint32_t> piece = parseIPv4PieceInsideIPv6(iterator))
            address = (address << 8) + piece.value();
        else
            return std::nullopt;
        if (i < 3) {
            if (iterator.atEnd())
                return std::nullopt;
            if (*iterator != '.')
                return std::nullopt;
            advance<CharacterType, ReportSyntaxViolation::No>(iterator);
        } else if (!iterator.atEnd())
            return std::nullopt;
    }
    ASSERT(iterator.atEnd());
    return address;
}

template<typename CharacterType>
std::optional<URLParser::IPv6Address> URLParser::parseIPv6Host(CodePointIterator<CharacterType> c)
{
    ASSERT(*c == '[');
    const auto hostBegin = c;
    advance(c, hostBegin);
    if (c.atEnd())
        return std::nullopt;

    IPv6Address address = {{0, 0, 0, 0, 0, 0, 0, 0}};
    size_t piecePointer = 0;
    std::optional<size_t> compressPointer;
    bool previousValueWasZero = false;
    bool immediatelyAfterCompress = false;

    if (*c == ':') {
        advance(c, hostBegin);
        if (c.atEnd())
            return std::nullopt;
        if (*c != ':')
            return std::nullopt;
        advance(c, hostBegin);
        ++piecePointer;
        compressPointer = piecePointer;
        immediatelyAfterCompress = true;
    }
    
    while (!c.atEnd()) {
        if (piecePointer == 8)
            return std::nullopt;
        if (*c == ':') {
            if (compressPointer)
                return std::nullopt;
            advance(c, hostBegin);
            ++piecePointer;
            compressPointer = piecePointer;
            immediatelyAfterCompress = true;
            if (previousValueWasZero)
                syntaxViolation(hostBegin);
            continue;
        }
        if (piecePointer == 6 || (compressPointer && piecePointer < 6)) {
            if (std::optional<IPv4Address> ipv4Address = parseIPv4AddressInsideIPv6(c)) {
                if (compressPointer && piecePointer == 5)
                    return std::nullopt;
                syntaxViolation(hostBegin);
                address[piecePointer++] = ipv4Address.value() >> 16;
                address[piecePointer++] = ipv4Address.value() & 0xFFFF;
                c = { };
                break;
            }
        }
        uint16_t value = 0;
        size_t length = 0;
        bool leadingZeros = false;
        for (; length < 4; length++) {
            if (c.atEnd())
                break;
            if (!isASCIIHexDigit(*c))
                break;
            if (isASCIIUpper(*c))
                syntaxViolation(hostBegin);
            if (*c == '0' && !length)
                leadingZeros = true;
            value = value * 0x10 + toASCIIHexValue(*c);
            advance(c, hostBegin);
        }
        
        previousValueWasZero = !value;
        if (UNLIKELY((value && leadingZeros) || (previousValueWasZero && (length > 1 || immediatelyAfterCompress))))
            syntaxViolation(hostBegin);

        address[piecePointer++] = value;
        if (c.atEnd())
            break;
        if (piecePointer == 8 || *c != ':')
            return std::nullopt;
        advance(c, hostBegin);
        if (c.atEnd())
            syntaxViolation(hostBegin);

        immediatelyAfterCompress = false;
    }
    
    if (!c.atEnd())
        return std::nullopt;
    
    if (compressPointer) {
        size_t swaps = piecePointer - compressPointer.value();
        piecePointer = 7;
        while (swaps)
            std::swap(address[piecePointer--], address[compressPointer.value() + swaps-- - 1]);
    } else if (piecePointer != 8)
        return std::nullopt;

    std::optional<size_t> possibleCompressPointer = findLongestZeroSequence(address);
    if (possibleCompressPointer)
        possibleCompressPointer.value()++;
    if (UNLIKELY(compressPointer != possibleCompressPointer))
        syntaxViolation(hostBegin);
    
    return address;
}

template<typename CharacterType>
URLParser::LCharBuffer URLParser::percentDecode(std::span<const LChar> input, const CodePointIterator<CharacterType>& iteratorForSyntaxViolationPosition)
{
    LCharBuffer output;
    output.reserveInitialCapacity(input.size());
    
    for (size_t i = 0; i < input.size(); ++i) {
        uint8_t byte = input[i];
        if (byte != '%')
            output.append(byte);
        else if (input.size() > 2 && i < input.size() - 2) {
            if (isASCIIHexDigit(input[i + 1]) && isASCIIHexDigit(input[i + 2])) {
                syntaxViolation(iteratorForSyntaxViolationPosition);
                output.append(toASCIIHexValue(input[i + 1], input[i + 2]));
                i += 2;
            } else
                output.append(byte);
        } else
            output.append(byte);
    }
    return output;
}
    
URLParser::LCharBuffer URLParser::percentDecode(std::span<const LChar> input)
{
    LCharBuffer output;
    output.reserveInitialCapacity(input.size());
    
    for (size_t i = 0; i < input.size(); ++i) {
        uint8_t byte = input[i];
        if (byte != '%')
            output.append(byte);
        else if (input.size() > 2 && i < input.size() - 2) {
            if (isASCIIHexDigit(input[i + 1]) && isASCIIHexDigit(input[i + 2])) {
                output.append(toASCIIHexValue(input[i + 1], input[i + 2]));
                i += 2;
            } else
                output.append(byte);
        } else
            output.append(byte);
    }
    return output;
}

bool URLParser::needsNonSpecialDotSlash() const
{
    auto pathStart = m_url.m_hostEnd + m_url.m_portLength;
    return !m_urlIsSpecial
        && pathStart == m_url.m_schemeEnd + 1U
        && pathStart + 1 < m_url.m_string.length()
        && m_url.m_string[pathStart] == '/'
        && m_url.m_string[pathStart + 1] == '/';
}

void URLParser::addNonSpecialDotSlash()
{
    auto oldPathStart = m_url.m_hostEnd + m_url.m_portLength;
    auto& oldString = m_url.m_string;
    m_url.m_string = makeString(StringView(oldString).left(oldPathStart + 1), "./"_s, StringView(oldString).substring(oldPathStart + 1));
    m_url.m_pathAfterLastSlash += 2;
    m_url.m_pathEnd += 2;
    m_url.m_queryEnd += 2;
}

template<typename CharacterType> std::optional<URLParser::LCharBuffer> URLParser::domainToASCII(StringImpl& domain, const CodePointIterator<CharacterType>& iteratorForSyntaxViolationPosition)
{
    LCharBuffer ascii;
    if (domain.containsOnlyASCII() && !subdomainStartsWithXNDashDash(domain)) {
        size_t length = domain.length();
        if (domain.is8Bit()) {
            auto characters = domain.span8();
            ascii.appendUsingFunctor(length, [&](size_t i) {
                if (UNLIKELY(isASCIIUpper(characters[i])))
                    syntaxViolation(iteratorForSyntaxViolationPosition);
                return toASCIILower(characters[i]);
            });
        } else {
            auto characters = domain.span16();
            ascii.appendUsingFunctor(length, [&](size_t i) {
                if (UNLIKELY(isASCIIUpper(characters[i])))
                    syntaxViolation(iteratorForSyntaxViolationPosition);
                return toASCIILower(characters[i]);
            });
        }
        return ascii;
    }

    std::array<UChar, hostnameBufferLength> hostnameBuffer;
    UErrorCode error = U_ZERO_ERROR;
    UIDNAInfo processingDetails = UIDNA_INFO_INITIALIZER;
    size_t numCharactersConverted = uidna_nameToASCII(&internationalDomainNameTranscoder(), StringView(domain).upconvertedCharacters(), domain.length(), hostnameBuffer.data(), hostnameBufferLength, &processingDetails, &error);

    if (U_SUCCESS(error) && !(processingDetails.errors & ~allowedNameToASCIIErrors) && numCharactersConverted) {
#if ASSERT_ENABLED
        for (size_t i = 0; i < numCharactersConverted; ++i) {
            ASSERT(isASCII(hostnameBuffer[i]));
            ASSERT(!isASCIIUpper(hostnameBuffer[i]));
        }
#else
        UNUSED_PARAM(numCharactersConverted);
#endif // ASSERT_ENABLED
        ascii.append(std::span { hostnameBuffer }.first(numCharactersConverted));
        if (domain != StringView(ascii.span()))
            syntaxViolation(iteratorForSyntaxViolationPosition);
        return ascii;
    }
    return std::nullopt;
}

bool URLParser::hasForbiddenHostCodePoint(const URLParser::LCharBuffer& asciiDomain)
{
    for (auto character : asciiDomain) {
        if (isForbiddenDomainCodePoint(character))
            return true;
    }
    return false;
}

template<typename CharacterType>
bool URLParser::parsePort(CodePointIterator<CharacterType>& iterator)
{
    if (UNLIKELY(m_urlIsFile))
        return false;

    ASSERT(*iterator == ':');
    auto colonIterator = iterator;
    advance(iterator, colonIterator);
    uint32_t port = 0;
    if (UNLIKELY(iterator.atEnd())) {
        unsigned portLength = currentPosition(colonIterator) - m_url.m_hostEnd;
        RELEASE_ASSERT(portLength <= URL::maxPortLength);
        m_url.m_portLength = portLength;
        syntaxViolation(colonIterator);
        return true;
    }
    size_t digitCount = 0;
    bool leadingZeros = false;
    for (; !iterator.atEnd(); ++iterator) {
        if (UNLIKELY(isTabOrNewline(*iterator))) {
            syntaxViolation(colonIterator);
            continue;
        }
        if (isASCIIDigit(*iterator)) {
            if (*iterator == '0' && !digitCount)
                leadingZeros = true;
            ++digitCount;
            port = port * 10 + *iterator - '0';
            if (port > std::numeric_limits<uint16_t>::max())
                return false;
        } else
            return false;
    }

    if (port && leadingZeros)
        syntaxViolation(colonIterator);
    
    if (!port && digitCount > 1)
        syntaxViolation(colonIterator);

    ASSERT(port == static_cast<uint16_t>(port));
    if (UNLIKELY(defaultPortForProtocol(parsedDataView(0, m_url.m_schemeEnd)) == static_cast<uint16_t>(port)))
        syntaxViolation(colonIterator);
    else {
        appendToASCIIBuffer(':');
        ASSERT(port <= std::numeric_limits<uint16_t>::max());
        appendNumberToASCIIBuffer<uint16_t>(static_cast<uint16_t>(port));
    }

    unsigned portLength = currentPosition(iterator) - m_url.m_hostEnd;
    RELEASE_ASSERT(portLength <= URL::maxPortLength);
    m_url.m_portLength = portLength;
    return true;
}

template<typename CharacterType>
bool URLParser::subdomainStartsWithXNDashDash(CodePointIterator<CharacterType> iterator)
{
    enum class State : uint8_t {
        NotAtSubdomainBeginOrInXNDashDash,
        AtSubdomainBegin,
        AfterX,
        AfterN,
        AfterFirstDash,
    } state { State::AtSubdomainBegin };

    for (; !iterator.atEnd(); advance<CharacterType, ReportSyntaxViolation::No>(iterator)) {
        CharacterType c = *iterator;

        // These characters indicate the end of the host.
        if (c == ':' || c == '/' || c == '?' || c == '#')
            return false;

        switch (state) {
        case State::NotAtSubdomainBeginOrInXNDashDash:
            break;
        case State::AtSubdomainBegin:
            if (c == 'x' || c == 'X') {
                state = State::AfterX;
                continue;
            }
            break;
        case State::AfterX:
            if (c == 'n' || c == 'N') {
                state = State::AfterN;
                continue;
            }
            break;
        case State::AfterN:
            if (c == '-') {
                state = State::AfterFirstDash;
                continue;
            }
            break;
        case State::AfterFirstDash:
            if (c == '-')
                return true;
            break;
        }

        if (c == '.')
            state = State::AtSubdomainBegin;
        else
            state = State::NotAtSubdomainBeginOrInXNDashDash;
    }
    return false;
}

bool URLParser::subdomainStartsWithXNDashDash(StringImpl& host)
{
    if (host.is8Bit())
        return subdomainStartsWithXNDashDash<LChar>(host.span8());
    return subdomainStartsWithXNDashDash<UChar>(host.span16());
}

static bool dnsNameEndsInNumber(StringView name)
{
    // https://url.spec.whatwg.org/#ends-in-a-number-checker
    auto containsOctalDecimalOrHexNumber = [] (StringView segment) {
        const auto segmentLength = segment.length();
        if (!UNLIKELY(segmentLength))
            return false;
        auto firstCodeUnit = segment[0];
        if (LIKELY(!isASCIIDigit(firstCodeUnit)))
            return false;
        if (segmentLength == 1)
            return true;
        auto secondCodeUnit = segment[1];
        if ((secondCodeUnit == 'x' || secondCodeUnit == 'X') && firstCodeUnit == '0')
            return segment.find(std::not_fn(isASCIIHexDigit<UChar>), 2) == notFound;
        return !segment.contains(std::not_fn(isASCIIDigit<UChar>));
    };

    size_t lastDotLocation = name.reverseFind('.');
    if (lastDotLocation == notFound)
        return containsOctalDecimalOrHexNumber(name);
    size_t lastSegmentEnd = name.length();
    if (lastDotLocation == lastSegmentEnd - 1) {
        lastSegmentEnd = lastDotLocation;
        lastDotLocation = name.reverseFind('.', lastDotLocation - 1);
    }
    StringView lastPart = name.substring(lastDotLocation == notFound ? 0 : lastDotLocation + 1, lastSegmentEnd - lastDotLocation - 1);
    return containsOctalDecimalOrHexNumber(lastPart);
}

template<typename CharacterType>
auto URLParser::parseHostAndPort(CodePointIterator<CharacterType> iterator) -> HostParsingResult
{
    if (iterator.atEnd())
        return HostParsingResult::InvalidHost;
    if (*iterator == ':')
        return HostParsingResult::InvalidHost;
    if (*iterator == '[') {
        auto ipv6End = iterator;
        while (!ipv6End.atEnd() && *ipv6End != ']')
            ++ipv6End;
        if (ipv6End.atEnd())
            return HostParsingResult::InvalidHost;
        if (auto address = parseIPv6Host(CodePointIterator<CharacterType>(iterator, ipv6End))) {
            serializeIPv6(address.value());
            if (!ipv6End.atEnd()) {
                advance(ipv6End);
                m_url.m_hostEnd = currentPosition(ipv6End);
                if (!ipv6End.atEnd() && *ipv6End == ':')
                    return parsePort(ipv6End) ? HostParsingResult::IPv6WithPort : HostParsingResult::InvalidHost;
                m_url.m_portLength = 0;
                return ipv6End.atEnd() ? HostParsingResult::IPv6WithoutPort : HostParsingResult::InvalidHost;
            }
            m_url.m_hostEnd = currentPosition(ipv6End);
            return HostParsingResult::IPv6WithoutPort;
        }
        return HostParsingResult::InvalidHost;
    }

    if (!m_urlIsSpecial) {
        for (; !iterator.atEnd(); ++iterator) {
            if (UNLIKELY(isTabOrNewline(*iterator))) {
                syntaxViolation(iterator);
                continue;
            }
            if (*iterator == ':')
                break;
            if (UNLIKELY(isForbiddenHostCodePoint(*iterator) && *iterator != '%'))
                return HostParsingResult::InvalidHost;
            utf8PercentEncode<isInC0ControlEncodeSet>(iterator);
        }
        m_url.m_hostEnd = currentPosition(iterator);
        if (iterator.atEnd()) {
            m_url.m_portLength = 0;
            return HostParsingResult::NonSpecialHostWithoutPort;
        }
        return parsePort(iterator) ? HostParsingResult::NonSpecialHostWithPort : HostParsingResult::InvalidHost;
    }
    
    if (LIKELY(!m_hostHasPercentOrNonASCII && !subdomainStartsWithXNDashDash(iterator))) {
        auto hostIterator = iterator;
        for (; !iterator.atEnd(); ++iterator) {
            if (isTabOrNewline(*iterator))
                continue;
            if (*iterator == ':')
                break;
            if (isForbiddenDomainCodePoint(*iterator))
                return HostParsingResult::InvalidHost;
        }
        auto address = parseIPv4Host(hostIterator, CodePointIterator<CharacterType>(hostIterator, iterator));
        if (address) {
            serializeIPv4(address.value());
            m_url.m_hostEnd = currentPosition(iterator);
            if (iterator.atEnd()) {
                m_url.m_portLength = 0;
                return HostParsingResult::IPv4WithoutPort;
            }
            return parsePort(iterator) ? HostParsingResult::IPv4WithPort : HostParsingResult::InvalidHost;
        }
        if (address.error() == IPv4ParsingError::Failure)
            return HostParsingResult::InvalidHost;
        for (; hostIterator != iterator; ++hostIterator) {
            if (UNLIKELY(isTabOrNewline(*hostIterator))) {
                syntaxViolation(hostIterator);
                continue;
            }
            if (UNLIKELY(isASCIIUpper(*hostIterator)))
                syntaxViolation(hostIterator);
            appendToASCIIBuffer(toASCIILower(*hostIterator));
        }
        m_url.m_hostEnd = currentPosition(iterator);
        auto hostStart = m_url.hostStart();
        if (UNLIKELY(dnsNameEndsInNumber(parsedDataView(hostStart, m_url.m_hostEnd - hostStart))))
            return HostParsingResult::InvalidHost;
        if (!hostIterator.atEnd())
            return parsePort(hostIterator) ? HostParsingResult::DNSNameWithPort : HostParsingResult::InvalidHost;
        m_url.m_portLength = 0;
        return HostParsingResult::DNSNameWithoutPort;
    }
    
    const auto hostBegin = iterator;
    
    LCharBuffer utf8Encoded;
    for (; !iterator.atEnd(); ++iterator) {
        if (UNLIKELY(isTabOrNewline(*iterator))) {
            syntaxViolation(hostBegin);
            continue;
        }
        if (*iterator == ':')
            break;
        if (UNLIKELY(!isASCII(*iterator)))
            syntaxViolation(hostBegin);

        std::array<uint8_t, U8_MAX_LENGTH> buffer;
        size_t offset = 0;
        UBool isError = false;
        U8_APPEND(buffer, offset, U8_MAX_LENGTH, *iterator, isError);
        if (isError)
            return HostParsingResult::InvalidHost;
        utf8Encoded.append(std::span { buffer }.first(offset));
    }
    LCharBuffer percentDecoded = percentDecode(utf8Encoded.span(), hostBegin);
    String domain = String::fromUTF8(percentDecoded.span());
    if (domain.isNull())
        return HostParsingResult::InvalidHost;
    if (domain != StringView(percentDecoded.span()))
        syntaxViolation(hostBegin);
    auto asciiDomain = domainToASCII(*domain.impl(), hostBegin);
    if (!asciiDomain || hasForbiddenHostCodePoint(asciiDomain.value()))
        return HostParsingResult::InvalidHost;
    LCharBuffer& asciiDomainValue = asciiDomain.value();

    auto address = parseIPv4Host<CharacterType, LChar>(hostBegin, asciiDomainValue.span());
    if (address) {
        serializeIPv4(address.value());
        m_url.m_hostEnd = currentPosition(iterator);
        if (iterator.atEnd()) {
            m_url.m_portLength = 0;
            return HostParsingResult::IPv4WithoutPort;
        }
        return parsePort(iterator) ? HostParsingResult::IPv4WithPort : HostParsingResult::InvalidHost;
    }
    if (address.error() == IPv4ParsingError::Failure)
        return HostParsingResult::InvalidHost;

    appendToASCIIBuffer(asciiDomainValue.span());
    m_url.m_hostEnd = currentPosition(iterator);
    auto hostStart = m_url.hostStart();
    if (UNLIKELY(dnsNameEndsInNumber(parsedDataView(hostStart, m_url.m_hostEnd - hostStart))))
        return HostParsingResult::InvalidHost;
    if (!iterator.atEnd())
        return parsePort(iterator) ? HostParsingResult::DNSNameWithPort : HostParsingResult::InvalidHost;

    m_url.m_portLength = 0;
    return HostParsingResult::DNSNameWithoutPort;
}

std::optional<String> URLParser::formURLDecode(StringView input)
{
    auto utf8 = input.utf8(StrictConversion);
    if (utf8.isNull())
        return std::nullopt;
    auto percentDecoded = percentDecode(utf8.span());
    return String::fromUTF8ReplacingInvalidSequences(percentDecoded.span());
}

// https://url.spec.whatwg.org/#concept-urlencoded-parser
auto URLParser::parseURLEncodedForm(StringView input) -> URLEncodedForm
{
    URLEncodedForm output;
    for (StringView bytes : input.split('&')) {
        if (auto nameAndValue = parseQueryNameAndValue(bytes))
            output.append(WTFMove(*nameAndValue));
    }
    return output;
}

std::optional<KeyValuePair<String, String>> URLParser::parseQueryNameAndValue(StringView bytes)
{
    auto equalIndex = bytes.find('=');
    if (equalIndex == notFound) {
        auto name = formURLDecode(makeStringByReplacingAll(bytes, '+', ' '));
        if (name)
            return { { WTFMove(*name), emptyString() } };
    } else {
        auto name = formURLDecode(makeStringByReplacingAll(bytes.left(equalIndex), '+', ' '));
        auto value = formURLDecode(makeStringByReplacingAll(bytes.substring(equalIndex + 1), '+', ' '));
        if (name && value)
            return { { WTFMove(*name), WTFMove(*value) } };
    }
    return std::nullopt;
}

static void serializeURLEncodedForm(const String& input, Vector<LChar>& output)
{
    auto utf8 = input.utf8(StrictConversion);
    for (char byte : utf8.span()) {
        if (byte == 0x20)
            output.append(0x2B);
        else if (byte == 0x2A
            || byte == 0x2D
            || byte == 0x2E
            || (byte >= 0x30 && byte <= 0x39)
            || (byte >= 0x41 && byte <= 0x5A)
            || byte == 0x5F
            || (byte >= 0x61 && byte <= 0x7A)) // FIXME: Put these in the characterClassTable to avoid branches.
            output.append(byte);
        else
            percentEncodeByte(byte, output);
    }
}
    
String URLParser::serialize(const URLEncodedForm& tuples)
{
    if (tuples.isEmpty())
        return { };

    Vector<LChar> output;
    for (auto& tuple : tuples) {
        if (!output.isEmpty())
            output.append('&');
        serializeURLEncodedForm(tuple.key, output);
        output.append('=');
        serializeURLEncodedForm(tuple.value, output);
    }
    return String::adopt(WTFMove(output));
}

const UIDNA& URLParser::internationalDomainNameTranscoder()
{
    static UIDNA* encoder;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        UErrorCode error = U_ZERO_ERROR;
        encoder = uidna_openUTS46(UIDNA_CHECK_BIDI | UIDNA_CHECK_CONTEXTJ | UIDNA_NONTRANSITIONAL_TO_UNICODE | UIDNA_NONTRANSITIONAL_TO_ASCII, &error);
        if (UNLIKELY(U_FAILURE(error)))
            CRASH_WITH_INFO(error);
        RELEASE_ASSERT(encoder);
    });
    return *encoder;
}

bool URLParser::allValuesEqual(const URL& a, const URL& b)
{
    URL_PARSER_LOG("%d %d %d %d %d %d %d %d %d %d %d %d %s\n%d %d %d %d %d %d %d %d %d %d %d %d %s",
        a.m_isValid,
        a.m_hasOpaquePath,
        a.m_protocolIsInHTTPFamily,
        a.m_schemeEnd,
        a.m_userStart,
        a.m_userEnd,
        a.m_passwordEnd,
        a.m_hostEnd,
        a.m_hostEnd + a.m_portLength,
        a.m_pathAfterLastSlash,
        a.m_pathEnd,
        a.m_queryEnd,
        a.m_string.utf8().data(),
        b.m_isValid,
        b.m_hasOpaquePath,
        b.m_protocolIsInHTTPFamily,
        b.m_schemeEnd,
        b.m_userStart,
        b.m_userEnd,
        b.m_passwordEnd,
        b.m_hostEnd,
        b.m_hostEnd + b.m_portLength,
        b.m_pathAfterLastSlash,
        b.m_pathEnd,
        b.m_queryEnd,
        b.m_string.utf8().data());

    return a.m_string == b.m_string
        && a.m_isValid == b.m_isValid
        && a.m_hasOpaquePath == b.m_hasOpaquePath
        && a.m_protocolIsInHTTPFamily == b.m_protocolIsInHTTPFamily
        && a.m_schemeEnd == b.m_schemeEnd
        && a.m_userStart == b.m_userStart
        && a.m_userEnd == b.m_userEnd
        && a.m_passwordEnd == b.m_passwordEnd
        && a.m_hostEnd == b.m_hostEnd
        && a.m_portLength == b.m_portLength
        && a.m_pathAfterLastSlash == b.m_pathAfterLastSlash
        && a.m_pathEnd == b.m_pathEnd
        && a.m_queryEnd == b.m_queryEnd;
}

bool URLParser::internalValuesConsistent(const URL& url)
{
    return url.m_schemeEnd <= url.m_userStart
        && url.m_userStart <= url.m_userEnd
        && url.m_userEnd <= url.m_passwordEnd
        && url.m_passwordEnd <= url.m_hostEnd
        && url.m_hostEnd + url.m_portLength <= url.m_pathAfterLastSlash
        && url.m_pathAfterLastSlash <= url.m_pathEnd
        && url.m_pathEnd <= url.m_queryEnd
        && url.m_queryEnd <= url.m_string.length();
}

} // namespace WTF
