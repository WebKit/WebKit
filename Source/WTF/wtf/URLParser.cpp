/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include <mutex>
#include <unicode/uidna.h>
#include <wtf/text/CodePointIterator.h>

namespace WTF {

#define URL_PARSER_DEBUGGING 0

#if URL_PARSER_DEBUGGING
#define URL_PARSER_LOG(...) WTFLogAlways(__VA_ARGS__)
#else
#define URL_PARSER_LOG(...)
#endif

ALWAYS_INLINE static void appendCodePoint(Vector<UChar>& destination, UChar32 codePoint)
{
    if (U_IS_BMP(codePoint)) {
        destination.append(static_cast<UChar>(codePoint));
        return;
    }
    destination.reserveCapacity(destination.size() + 2);
    destination.uncheckedAppend(U16_LEAD(codePoint));
    destination.uncheckedAppend(U16_TRAIL(codePoint));
}

enum URLCharacterClass {
    UserInfo = 0x1,
    Default = 0x2,
    ForbiddenHost = 0x4,
    QueryPercent = 0x8,
    SlashQuestionOrHash = 0x10,
    ValidScheme = 0x20,
};

static const uint8_t characterClassTable[256] = {
    UserInfo | Default | QueryPercent | ForbiddenHost, // 0x0
    UserInfo | Default | QueryPercent, // 0x1
    UserInfo | Default | QueryPercent, // 0x2
    UserInfo | Default | QueryPercent, // 0x3
    UserInfo | Default | QueryPercent, // 0x4
    UserInfo | Default | QueryPercent, // 0x5
    UserInfo | Default | QueryPercent, // 0x6
    UserInfo | Default | QueryPercent, // 0x7
    UserInfo | Default | QueryPercent, // 0x8
    UserInfo | Default | QueryPercent | ForbiddenHost, // 0x9
    UserInfo | Default | QueryPercent | ForbiddenHost, // 0xA
    UserInfo | Default | QueryPercent, // 0xB
    UserInfo | Default | QueryPercent, // 0xC
    UserInfo | Default | QueryPercent | ForbiddenHost, // 0xD
    UserInfo | Default | QueryPercent, // 0xE
    UserInfo | Default | QueryPercent, // 0xF
    UserInfo | Default | QueryPercent, // 0x10
    UserInfo | Default | QueryPercent, // 0x11
    UserInfo | Default | QueryPercent, // 0x12
    UserInfo | Default | QueryPercent, // 0x13
    UserInfo | Default | QueryPercent, // 0x14
    UserInfo | Default | QueryPercent, // 0x15
    UserInfo | Default | QueryPercent, // 0x16
    UserInfo | Default | QueryPercent, // 0x17
    UserInfo | Default | QueryPercent, // 0x18
    UserInfo | Default | QueryPercent, // 0x19
    UserInfo | Default | QueryPercent, // 0x1A
    UserInfo | Default | QueryPercent, // 0x1B
    UserInfo | Default | QueryPercent, // 0x1C
    UserInfo | Default | QueryPercent, // 0x1D
    UserInfo | Default | QueryPercent, // 0x1E
    UserInfo | Default | QueryPercent, // 0x1F
    UserInfo | Default | QueryPercent | ForbiddenHost, // ' '
    0, // '!'
    UserInfo | Default | QueryPercent, // '"'
    UserInfo | Default | QueryPercent | SlashQuestionOrHash | ForbiddenHost, // '#'
    0, // '$'
    ForbiddenHost, // '%'
    0, // '&'
    0, // '\''
    0, // '('
    0, // ')'
    0, // '*'
    ValidScheme, // '+'
    0, // ','
    ValidScheme, // '-'
    ValidScheme, // '.'
    UserInfo | SlashQuestionOrHash | ForbiddenHost, // '/'
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
    UserInfo | ForbiddenHost, // ':'
    UserInfo, // ';'
    UserInfo | Default | QueryPercent | ForbiddenHost, // '<'
    UserInfo, // '='
    UserInfo | Default | QueryPercent | ForbiddenHost, // '>'
    UserInfo | Default | SlashQuestionOrHash | ForbiddenHost, // '?'
    UserInfo | ForbiddenHost, // '@'
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
    UserInfo | ForbiddenHost, // '['
    UserInfo | SlashQuestionOrHash | ForbiddenHost, // '\\'
    UserInfo | ForbiddenHost, // ']'
    UserInfo | ForbiddenHost, // '^'
    0, // '_'
    UserInfo | Default, // '`'
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
    UserInfo | Default, // '{'
    UserInfo, // '|'
    UserInfo | Default, // '}'
    0, // '~'
    QueryPercent, // 0x7F
    QueryPercent, // 0x80
    QueryPercent, // 0x81
    QueryPercent, // 0x82
    QueryPercent, // 0x83
    QueryPercent, // 0x84
    QueryPercent, // 0x85
    QueryPercent, // 0x86
    QueryPercent, // 0x87
    QueryPercent, // 0x88
    QueryPercent, // 0x89
    QueryPercent, // 0x8A
    QueryPercent, // 0x8B
    QueryPercent, // 0x8C
    QueryPercent, // 0x8D
    QueryPercent, // 0x8E
    QueryPercent, // 0x8F
    QueryPercent, // 0x90
    QueryPercent, // 0x91
    QueryPercent, // 0x92
    QueryPercent, // 0x93
    QueryPercent, // 0x94
    QueryPercent, // 0x95
    QueryPercent, // 0x96
    QueryPercent, // 0x97
    QueryPercent, // 0x98
    QueryPercent, // 0x99
    QueryPercent, // 0x9A
    QueryPercent, // 0x9B
    QueryPercent, // 0x9C
    QueryPercent, // 0x9D
    QueryPercent, // 0x9E
    QueryPercent, // 0x9F
    QueryPercent, // 0xA0
    QueryPercent, // 0xA1
    QueryPercent, // 0xA2
    QueryPercent, // 0xA3
    QueryPercent, // 0xA4
    QueryPercent, // 0xA5
    QueryPercent, // 0xA6
    QueryPercent, // 0xA7
    QueryPercent, // 0xA8
    QueryPercent, // 0xA9
    QueryPercent, // 0xAA
    QueryPercent, // 0xAB
    QueryPercent, // 0xAC
    QueryPercent, // 0xAD
    QueryPercent, // 0xAE
    QueryPercent, // 0xAF
    QueryPercent, // 0xB0
    QueryPercent, // 0xB1
    QueryPercent, // 0xB2
    QueryPercent, // 0xB3
    QueryPercent, // 0xB4
    QueryPercent, // 0xB5
    QueryPercent, // 0xB6
    QueryPercent, // 0xB7
    QueryPercent, // 0xB8
    QueryPercent, // 0xB9
    QueryPercent, // 0xBA
    QueryPercent, // 0xBB
    QueryPercent, // 0xBC
    QueryPercent, // 0xBD
    QueryPercent, // 0xBE
    QueryPercent, // 0xBF
    QueryPercent, // 0xC0
    QueryPercent, // 0xC1
    QueryPercent, // 0xC2
    QueryPercent, // 0xC3
    QueryPercent, // 0xC4
    QueryPercent, // 0xC5
    QueryPercent, // 0xC6
    QueryPercent, // 0xC7
    QueryPercent, // 0xC8
    QueryPercent, // 0xC9
    QueryPercent, // 0xCA
    QueryPercent, // 0xCB
    QueryPercent, // 0xCC
    QueryPercent, // 0xCD
    QueryPercent, // 0xCE
    QueryPercent, // 0xCF
    QueryPercent, // 0xD0
    QueryPercent, // 0xD1
    QueryPercent, // 0xD2
    QueryPercent, // 0xD3
    QueryPercent, // 0xD4
    QueryPercent, // 0xD5
    QueryPercent, // 0xD6
    QueryPercent, // 0xD7
    QueryPercent, // 0xD8
    QueryPercent, // 0xD9
    QueryPercent, // 0xDA
    QueryPercent, // 0xDB
    QueryPercent, // 0xDC
    QueryPercent, // 0xDD
    QueryPercent, // 0xDE
    QueryPercent, // 0xDF
    QueryPercent, // 0xE0
    QueryPercent, // 0xE1
    QueryPercent, // 0xE2
    QueryPercent, // 0xE3
    QueryPercent, // 0xE4
    QueryPercent, // 0xE5
    QueryPercent, // 0xE6
    QueryPercent, // 0xE7
    QueryPercent, // 0xE8
    QueryPercent, // 0xE9
    QueryPercent, // 0xEA
    QueryPercent, // 0xEB
    QueryPercent, // 0xEC
    QueryPercent, // 0xED
    QueryPercent, // 0xEE
    QueryPercent, // 0xEF
    QueryPercent, // 0xF0
    QueryPercent, // 0xF1
    QueryPercent, // 0xF2
    QueryPercent, // 0xF3
    QueryPercent, // 0xF4
    QueryPercent, // 0xF5
    QueryPercent, // 0xF6
    QueryPercent, // 0xF7
    QueryPercent, // 0xF8
    QueryPercent, // 0xF9
    QueryPercent, // 0xFA
    QueryPercent, // 0xFB
    QueryPercent, // 0xFC
    QueryPercent, // 0xFD
    QueryPercent, // 0xFE
    QueryPercent, // 0xFF
};

template<typename CharacterType> ALWAYS_INLINE static bool isC0Control(CharacterType character) { return character <= 0x1F; }
template<typename CharacterType> ALWAYS_INLINE static bool isC0ControlOrSpace(CharacterType character) { return character <= 0x20; }
template<typename CharacterType> ALWAYS_INLINE static bool isTabOrNewline(CharacterType character) { return character <= 0xD && character >= 0x9 && character != 0xB && character != 0xC; }
template<typename CharacterType> ALWAYS_INLINE static bool isInSimpleEncodeSet(CharacterType character) { return character > 0x7E || isC0Control(character); }
template<typename CharacterType> ALWAYS_INLINE static bool isInFragmentEncodeSet(CharacterType character) { return character > 0x7E || character == '`' || ((characterClassTable[character] & QueryPercent) && character != '#'); }
template<typename CharacterType> ALWAYS_INLINE static bool isInDefaultEncodeSet(CharacterType character) { return character > 0x7E || characterClassTable[character] & Default; }
template<typename CharacterType> ALWAYS_INLINE static bool isInUserInfoEncodeSet(CharacterType character) { return character > 0x7E || characterClassTable[character] & UserInfo; }
template<typename CharacterType> ALWAYS_INLINE static bool isPercentOrNonASCII(CharacterType character) { return !isASCII(character) || character == '%'; }
template<typename CharacterType> ALWAYS_INLINE static bool isSlashQuestionOrHash(CharacterType character) { return character <= '\\' && characterClassTable[character] & SlashQuestionOrHash; }
template<typename CharacterType> ALWAYS_INLINE static bool isValidSchemeCharacter(CharacterType character) { return character <= 'z' && characterClassTable[character] & ValidScheme; }
template<typename CharacterType> ALWAYS_INLINE static bool isForbiddenHostCodePoint(CharacterType character) { return character <= '^' && characterClassTable[character] & ForbiddenHost; }
ALWAYS_INLINE static bool shouldPercentEncodeQueryByte(uint8_t byte, const bool& urlIsSpecial)
{
    if (characterClassTable[byte] & QueryPercent)
        return true;
    if (byte == '\'' && urlIsSpecial)
        return true;
    return false;
}

bool URLParser::isInUserInfoEncodeSet(UChar c)
{
    return WTF::isInUserInfoEncodeSet(c);
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
    if (iterator.atEnd() || !isASCIIAlpha(*iterator))
        return false;
    advance<CharacterType, ReportSyntaxViolation::No>(iterator);
    if (iterator.atEnd())
        return false;
    if (*iterator == ':')
        return true;
    if (UNLIKELY(*iterator == '|'))
        return true;
    return false;
}

ALWAYS_INLINE void URLParser::appendToASCIIBuffer(UChar32 codePoint)
{
    ASSERT(isASCII(codePoint));
    if (UNLIKELY(m_didSeeSyntaxViolation))
        m_asciiBuffer.append(codePoint);
}

ALWAYS_INLINE void URLParser::appendToASCIIBuffer(const char* characters, size_t length)
{
    if (UNLIKELY(m_didSeeSyntaxViolation))
        m_asciiBuffer.append(characters, length);
}

template<typename CharacterType>
void URLParser::appendWindowsDriveLetter(CodePointIterator<CharacterType>& iterator)
{
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
    if (base.protocolIs("file")) {
        RELEASE_ASSERT(base.m_hostEnd + base.m_portLength < base.m_string.length());
        if (base.m_string.is8Bit()) {
            const LChar* begin = base.m_string.characters8();
            CodePointIterator<LChar> c(begin + base.m_hostEnd + base.m_portLength + 1, begin + base.m_string.length());
            if (isWindowsDriveLetter(c)) {
                appendWindowsDriveLetter(c);
                return true;
            }
        } else {
            const UChar* begin = base.m_string.characters16();
            CodePointIterator<UChar> c(begin + base.m_hostEnd + base.m_portLength + 1, begin + base.m_string.length());
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

const char replacementCharacterUTF8PercentEncoded[10] = "%EF%BF%BD";
const size_t replacementCharacterUTF8PercentEncodedLength = sizeof(replacementCharacterUTF8PercentEncoded) - 1;

template<bool(*isInCodeSet)(UChar32), typename CharacterType>
ALWAYS_INLINE void URLParser::utf8PercentEncode(const CodePointIterator<CharacterType>& iterator)
{
    ASSERT(!iterator.atEnd());
    UChar32 codePoint = *iterator;
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
    
    if (!U_IS_UNICODE_CHAR(codePoint)) {
        appendToASCIIBuffer(replacementCharacterUTF8PercentEncoded, replacementCharacterUTF8PercentEncodedLength);
        return;
    }
    
    uint8_t buffer[U8_MAX_LENGTH];
    int32_t offset = 0;
    U8_APPEND_UNSAFE(buffer, offset, codePoint);
    for (int32_t i = 0; i < offset; ++i)
        percentEncodeByte(buffer[i]);
}

template<typename CharacterType>
ALWAYS_INLINE void URLParser::utf8QueryEncode(const CodePointIterator<CharacterType>& iterator)
{
    ASSERT(!iterator.atEnd());
    UChar32 codePoint = *iterator;
    if (LIKELY(isASCII(codePoint))) {
        if (UNLIKELY(shouldPercentEncodeQueryByte(codePoint, m_urlIsSpecial))) {
            syntaxViolation(iterator);
            percentEncodeByte(codePoint);
        } else
            appendToASCIIBuffer(codePoint);
        return;
    }
    
    syntaxViolation(iterator);
    
    if (!U_IS_UNICODE_CHAR(codePoint)) {
        appendToASCIIBuffer(replacementCharacterUTF8PercentEncoded, replacementCharacterUTF8PercentEncodedLength);
        return;
    }

    uint8_t buffer[U8_MAX_LENGTH];
    int32_t offset = 0;
    U8_APPEND_UNSAFE(buffer, offset, codePoint);
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
    auto encoded = encoding.encodeForURLParsing(StringView(source.data(), source.size()));
    auto* data = encoded.data();
    size_t length = encoded.size();
    
    if (!length == !iterator.atEnd()) {
        syntaxViolation(iterator);
        return;
    }
    
    size_t i = 0;
    for (; i < length; ++i) {
        ASSERT(!iterator.atEnd());
        uint8_t byte = data[i];
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
        uint8_t byte = data[i];
        if (shouldPercentEncodeQueryByte(byte, m_urlIsSpecial))
            percentEncodeByte(byte);
        else
            appendToASCIIBuffer(byte);
    }
}

Optional<uint16_t> URLParser::defaultPortForProtocol(StringView scheme)
{
    static constexpr uint16_t ftpPort = 21;
    static constexpr uint16_t httpPort = 80;
    static constexpr uint16_t httpsPort = 443;
    static constexpr uint16_t wsPort = 80;
    static constexpr uint16_t wssPort = 443;
    
    auto length = scheme.length();
    if (!length)
        return WTF::nullopt;
    switch (scheme[0]) {
    case 'w':
        switch (length) {
        case 2:
            if (scheme[1] == 's')
                return wsPort;
            return WTF::nullopt;
        case 3:
            if (scheme[1] == 's'
                && scheme[2] == 's')
                return wssPort;
            return WTF::nullopt;
        default:
            return false;
        }
    case 'h':
        switch (length) {
        case 4:
            if (scheme[1] == 't'
                && scheme[2] == 't'
                && scheme[3] == 'p')
                return httpPort;
            return WTF::nullopt;
        case 5:
            if (scheme[1] == 't'
                && scheme[2] == 't'
                && scheme[3] == 'p'
                && scheme[4] == 's')
                return httpsPort;
            return WTF::nullopt;
        default:
            return WTF::nullopt;
        }
    case 'f':
        if (length == 3
            && scheme[1] == 't'
            && scheme[2] == 'p')
            return ftpPort;
        return WTF::nullopt;
    default:
        return WTF::nullopt;
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

Optional<String> URLParser::maybeCanonicalizeScheme(const String& scheme)
{
    if (scheme.isEmpty())
        return WTF::nullopt;

    if (!isASCIIAlpha(scheme[0]))
        return WTF::nullopt;

    for (size_t i = 1; i < scheme.length(); ++i) {
        if (isASCIIAlphanumeric(scheme[i]) || scheme[i] == '+' || scheme[i] == '-' || scheme[i] == '.')
            continue;
        return WTF::nullopt;
    }

    return scheme.convertToASCIILowercase();
}

bool URLParser::isSpecialScheme(const String& schemeArg)
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
        appendToASCIIBuffer(string.characters8(), length);
    else {
        const UChar* characters = string.characters16();
        for (size_t i = 0; i < length; ++i) {
            UChar c = characters[i];
            ASSERT_WITH_SECURITY_IMPLICATION(isASCII(c));
            appendToASCIIBuffer(c);
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
    switch (scheme(StringView(m_asciiBuffer.data(), m_url.m_schemeEnd))) {
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
        return;
    }
    ASSERT_NOT_REACHED();
}

static const char dotASCIICode[2] = {'2', 'e'};

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
    if (toASCIILower(*c) == dotASCIICode[1]) {
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
    if (toASCIILower(*c) == dotASCIICode[1]) {
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
        ASSERT(toASCIILower(*c) == dotASCIICode[1]);
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
        ASSERT(toASCIILower(*c) == dotASCIICode[1]);
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
    CodePointIterator<LChar> componentToPop(&m_asciiBuffer[newPathAfterLastSlash], &m_asciiBuffer[0] + m_url.m_pathAfterLastSlash);
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
    m_asciiBuffer.reserveCapacity(m_inputString.length());
    for (size_t i = 0; i < codeUnitsToCopy; ++i) {
        ASSERT(isASCII(m_inputString[i]));
        m_asciiBuffer.uncheckedAppend(m_inputString[i]);
    }
}

void URLParser::failure()
{
    m_url.invalidate();
    m_url.m_string = m_inputString;
}

template<typename CharacterType>
bool URLParser::checkLocalhostCodePoint(CodePointIterator<CharacterType>& iterator, UChar32 codePoint)
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
        return isAtLocalhost(CodePointIterator<LChar>(view.characters8(), view.characters8() + view.length()));
    return isAtLocalhost(CodePointIterator<UChar>(view.characters16(), view.characters16() + view.length()));
}

ALWAYS_INLINE StringView URLParser::parsedDataView(size_t start, size_t length)
{
    if (UNLIKELY(m_didSeeSyntaxViolation)) {
        ASSERT(start + length <= m_asciiBuffer.size());
        return StringView(m_asciiBuffer.data() + start, length);
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

URLParser::URLParser(const String& input, const URL& base, const URLTextEncoding* nonUTF8QueryEncoding)
    : m_inputString(input)
{
    if (input.isNull()) {
        if (base.isValid() && !base.m_cannotBeABaseURL) {
            m_url = base;
            m_url.removeFragmentIdentifier();
        }
        return;
    }

    if (input.is8Bit()) {
        m_inputBegin = input.characters8();
        parse(input.characters8(), input.length(), base, nonUTF8QueryEncoding);
    } else {
        m_inputBegin = input.characters16();
        parse(input.characters16(), input.length(), base, nonUTF8QueryEncoding);
    }

    ASSERT(!m_url.m_isValid
        || m_didSeeSyntaxViolation == (m_url.string() != input)
        || (input.isAllSpecialCharacters<isC0ControlOrSpace>()
            && m_url.m_string == base.m_string.left(base.m_queryEnd)));
    ASSERT(internalValuesConsistent(m_url));
#if ASSERT_ENABLED
    if (!m_didSeeSyntaxViolation) {
        // Force a syntax violation at the beginning to make sure we get the same result.
        URLParser parser(makeString(" ", input), base, nonUTF8QueryEncoding);
        URL parsed = parser.result();
        if (parsed.isValid())
            ASSERT(allValuesEqual(parser.result(), m_url));
    }
#endif // ASSERT_ENABLED
}

template<typename CharacterType>
void URLParser::parse(const CharacterType* input, const unsigned length, const URL& base, const URLTextEncoding* nonUTF8QueryEncoding)
{
    URL_PARSER_LOG("Parsing URL <%s> base <%s>", String(input, length).utf8().data(), base.string().utf8().data());
    m_url = { };
    ASSERT(m_asciiBuffer.isEmpty());

    Vector<UChar> queryBuffer;

    unsigned endIndex = length;
    while (UNLIKELY(endIndex && isC0ControlOrSpace(input[endIndex - 1]))) {
        syntaxViolation(CodePointIterator<CharacterType>(input, input));
        endIndex--;
    }
    CodePointIterator<CharacterType> c(input, input + endIndex);
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
        CannotBeABaseURLPath,
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
                StringView urlScheme = parsedDataView(0, m_url.m_schemeEnd);
                appendToASCIIBuffer(':');
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
                        m_url.m_cannotBeABaseURL = true;
                        state = State::CannotBeABaseURLPath;
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
            if (!base.isValid() || (base.m_cannotBeABaseURL && *c != '#')) {
                failure();
                return;
            }
            if (base.m_cannotBeABaseURL && *c == '#') {
                copyURLPartsUntil(base, URLPart::QueryEnd, c, nonUTF8QueryEncoding);
                state = State::Fragment;
                appendToASCIIBuffer('#');
                ++c;
                break;
            }
            if (!base.protocolIs("file")) {
                state = State::Relative;
                break;
            }
            copyURLPartsUntil(base, URLPart::SchemeEnd, c, nonUTF8QueryEncoding);
            appendToASCIIBuffer(':');
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
                if (currentPosition(c) && parsedDataView(currentPosition(c) - 1) != '/') {
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
                appendToASCIIBuffer("://", 3);
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
                appendToASCIIBuffer("//", 2);
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
                        if (!parseHostAndPort(iterator)) {
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
                if (*c == '/' || *c == '?' || *c == '#') {
                    if (!parseHostAndPort(CodePointIterator<CharacterType>(authorityOrHostBegin, c))) {
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
                if (base.isValid() && base.protocolIs("file")) {
                    copyURLPartsUntil(base, URLPart::PathEnd, c, nonUTF8QueryEncoding);
                    appendToASCIIBuffer('?');
                    ++c;
                } else {
                    appendToASCIIBuffer("///?", 4);
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
                if (base.isValid() && base.protocolIs("file")) {
                    copyURLPartsUntil(base, URLPart::QueryEnd, c, nonUTF8QueryEncoding);
                    appendToASCIIBuffer('#');
                } else {
                    appendToASCIIBuffer("///#", 4);
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
                if (base.isValid() && base.protocolIs("file") && shouldCopyFileURL(c))
                    copyURLPartsUntil(base, URLPart::PathAfterLastSlash, c, nonUTF8QueryEncoding);
                else {
                    appendToASCIIBuffer("///", 3);
                    m_url.m_userStart = currentPosition(c) - 1;
                    m_url.m_userEnd = m_url.m_userStart;
                    m_url.m_passwordEnd = m_url.m_userStart;
                    m_url.m_hostEnd = m_url.m_userStart;
                    m_url.m_portLength = 0;
                    m_url.m_pathAfterLastSlash = m_url.m_userStart + 1;
                    if (isWindowsDriveLetter(c))
                        appendWindowsDriveLetter(c);
                }
                state = State::Path;
                break;
            }
            break;
        case State::FileSlash:
            LOG_STATE("FileSlash");
            if (LIKELY(*c == '/' || *c == '\\')) {
                if (UNLIKELY(*c == '\\'))
                    syntaxViolation(c);
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
            syntaxViolation(c);
            appendToASCIIBuffer("//", 2);
            m_url.m_userStart = currentPosition(c) - 1;
            m_url.m_userEnd = m_url.m_userStart;
            m_url.m_passwordEnd = m_url.m_userStart;
            m_url.m_hostEnd = m_url.m_userStart;
            m_url.m_portLength = 0;
            if (isWindowsDriveLetter(c)) {
                appendWindowsDriveLetter(c);
                m_url.m_pathAfterLastSlash = m_url.m_userStart + 1;
            } else if (copyBaseWindowsDriveLetter(base)) {
                appendToASCIIBuffer('/');
                m_url.m_pathAfterLastSlash = m_url.m_userStart + 4;
            } else
                m_url.m_pathAfterLastSlash = m_url.m_userStart + 1;
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
                            appendToASCIIBuffer("/?", 2);
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
                            appendToASCIIBuffer("/#", 2);
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
                    if (!parseHostAndPort(CodePointIterator<CharacterType>(authorityOrHostBegin, c))) {
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
            utf8PercentEncode<isInDefaultEncodeSet>(c);
            ++c;
            break;
        case State::CannotBeABaseURLPath:
            LOG_STATE("CannotBeABaseURLPath");
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
                utf8PercentEncode<isInSimpleEncodeSet>(c);
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
        if (!currentPosition(c) && base.isValid() && !base.m_cannotBeABaseURL) {
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
        m_url.m_userStart = currentPosition(c);
        m_url.m_userEnd = m_url.m_userStart;
        m_url.m_passwordEnd = m_url.m_userStart;
        m_url.m_hostEnd = m_url.m_userStart;
        m_url.m_portLength = 0;
        m_url.m_pathAfterLastSlash = m_url.m_userStart;
        m_url.m_pathEnd = m_url.m_userStart;
        m_url.m_queryEnd = m_url.m_userStart;
        break;
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
        } else if (!parseHostAndPort(authorityOrHostBegin)) {
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
        if (!parseHostAndPort(authorityOrHostBegin)) {
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
        if (base.isValid() && base.protocolIs("file")) {
            copyURLPartsUntil(base, URLPart::QueryEnd, c, nonUTF8QueryEncoding);
            break;
        }
        syntaxViolation(c);
        appendToASCIIBuffer("///", 3);
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
        m_url.m_userStart = currentPosition(c) + 1;
        appendToASCIIBuffer("//", 2);
        m_url.m_userEnd = m_url.m_userStart;
        m_url.m_passwordEnd = m_url.m_userStart;
        m_url.m_hostEnd = m_url.m_userStart;
        m_url.m_portLength = 0;
        if (copyBaseWindowsDriveLetter(base)) {
            appendToASCIIBuffer('/');
            m_url.m_pathAfterLastSlash = m_url.m_userStart + 4;
        } else
            m_url.m_pathAfterLastSlash = m_url.m_userStart + 1;
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

        if (!parseHostAndPort(CodePointIterator<CharacterType>(authorityOrHostBegin, c))) {
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
    case State::CannotBeABaseURLPath:
        LOG_FINAL_STATE("CannotBeABaseURLPath");
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
    URL_PARSER_LOG("Parsed URL <%s>", m_url.m_string.utf8().data());
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
    LChar buf[sizeof(UnsignedIntegerType) * 3 + 1];
    LChar* end = std::end(buf);
    LChar* p = end;
    do {
        *--p = (number % 10) + '0';
        number /= 10;
    } while (number);
    appendToASCIIBuffer(p, end - p);
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

static Optional<size_t> findLongestZeroSequence(const std::array<uint16_t, 8>& address)
{
    Optional<size_t> longest;
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
                appendToASCIIBuffer("::", 2);
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
    Checked<uint32_t, RecordOverflow> value = 0;
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
            return value.unsafeGet();
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
    return value.unsafeGet();
}

ALWAYS_INLINE static uint64_t pow256(size_t exponent)
{
    RELEASE_ASSERT(exponent <= 4);
    uint64_t values[5] = {1, 256, 256 * 256, 256 * 256 * 256, 256ull * 256 * 256 * 256 };
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
Optional<uint32_t> URLParser::parseIPv4PieceInsideIPv6(CodePointIterator<CharacterType>& iterator)
{
    if (iterator.atEnd())
        return WTF::nullopt;
    uint32_t piece = 0;
    bool leadingZeros = false;
    size_t digitCount = 0;
    while (!iterator.atEnd()) {
        if (!isASCIIDigit(*iterator))
            return WTF::nullopt;
        ++digitCount;
        if (!piece && *iterator == '0') {
            if (leadingZeros)
                return WTF::nullopt;
            leadingZeros = true;
        }
        if (!piece && *iterator == '0')
            leadingZeros = true;
        piece = piece * 10 + *iterator - '0';
        if (piece > 255)
            return WTF::nullopt;
        advance<CharacterType, ReportSyntaxViolation::No>(iterator);
        if (iterator.atEnd())
            break;
        if (*iterator == '.')
            break;
    }
    if (piece && leadingZeros)
        return WTF::nullopt;
    return piece;
}

template<typename CharacterType>
Optional<URLParser::IPv4Address> URLParser::parseIPv4AddressInsideIPv6(CodePointIterator<CharacterType> iterator)
{
    IPv4Address address = 0;
    for (size_t i = 0; i < 4; ++i) {
        if (Optional<uint32_t> piece = parseIPv4PieceInsideIPv6(iterator))
            address = (address << 8) + piece.value();
        else
            return WTF::nullopt;
        if (i < 3) {
            if (iterator.atEnd())
                return WTF::nullopt;
            if (*iterator != '.')
                return WTF::nullopt;
            advance<CharacterType, ReportSyntaxViolation::No>(iterator);
        } else if (!iterator.atEnd())
            return WTF::nullopt;
    }
    ASSERT(iterator.atEnd());
    return address;
}

template<typename CharacterType>
Optional<URLParser::IPv6Address> URLParser::parseIPv6Host(CodePointIterator<CharacterType> c)
{
    ASSERT(*c == '[');
    const auto hostBegin = c;
    advance(c, hostBegin);
    if (c.atEnd())
        return WTF::nullopt;

    IPv6Address address = {{0, 0, 0, 0, 0, 0, 0, 0}};
    size_t piecePointer = 0;
    Optional<size_t> compressPointer;
    bool previousValueWasZero = false;
    bool immediatelyAfterCompress = false;

    if (*c == ':') {
        advance(c, hostBegin);
        if (c.atEnd())
            return WTF::nullopt;
        if (*c != ':')
            return WTF::nullopt;
        advance(c, hostBegin);
        ++piecePointer;
        compressPointer = piecePointer;
        immediatelyAfterCompress = true;
    }
    
    while (!c.atEnd()) {
        if (piecePointer == 8)
            return WTF::nullopt;
        if (*c == ':') {
            if (compressPointer)
                return WTF::nullopt;
            advance(c, hostBegin);
            ++piecePointer;
            compressPointer = piecePointer;
            immediatelyAfterCompress = true;
            if (previousValueWasZero)
                syntaxViolation(hostBegin);
            continue;
        }
        if (piecePointer == 6 || (compressPointer && piecePointer < 6)) {
            if (Optional<IPv4Address> ipv4Address = parseIPv4AddressInsideIPv6(c)) {
                if (compressPointer && piecePointer == 5)
                    return WTF::nullopt;
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
            return WTF::nullopt;
        advance(c, hostBegin);
        if (c.atEnd())
            syntaxViolation(hostBegin);

        immediatelyAfterCompress = false;
    }
    
    if (!c.atEnd())
        return WTF::nullopt;
    
    if (compressPointer) {
        size_t swaps = piecePointer - compressPointer.value();
        piecePointer = 7;
        while (swaps)
            std::swap(address[piecePointer--], address[compressPointer.value() + swaps-- - 1]);
    } else if (piecePointer != 8)
        return WTF::nullopt;

    Optional<size_t> possibleCompressPointer = findLongestZeroSequence(address);
    if (possibleCompressPointer)
        possibleCompressPointer.value()++;
    if (UNLIKELY(compressPointer != possibleCompressPointer))
        syntaxViolation(hostBegin);
    
    return address;
}

template<typename CharacterType>
URLParser::LCharBuffer URLParser::percentDecode(const LChar* input, size_t length, const CodePointIterator<CharacterType>& iteratorForSyntaxViolationPosition)
{
    LCharBuffer output;
    output.reserveInitialCapacity(length);
    
    for (size_t i = 0; i < length; ++i) {
        uint8_t byte = input[i];
        if (byte != '%')
            output.uncheckedAppend(byte);
        else if (length > 2 && i < length - 2) {
            if (isASCIIHexDigit(input[i + 1]) && isASCIIHexDigit(input[i + 2])) {
                syntaxViolation(iteratorForSyntaxViolationPosition);
                output.uncheckedAppend(toASCIIHexValue(input[i + 1], input[i + 2]));
                i += 2;
            } else
                output.uncheckedAppend(byte);
        } else
            output.uncheckedAppend(byte);
    }
    return output;
}
    
URLParser::LCharBuffer URLParser::percentDecode(const LChar* input, size_t length)
{
    LCharBuffer output;
    output.reserveInitialCapacity(length);
    
    for (size_t i = 0; i < length; ++i) {
        uint8_t byte = input[i];
        if (byte != '%')
            output.uncheckedAppend(byte);
        else if (length > 2 && i < length - 2) {
            if (isASCIIHexDigit(input[i + 1]) && isASCIIHexDigit(input[i + 2])) {
                output.uncheckedAppend(toASCIIHexValue(input[i + 1], input[i + 2]));
                i += 2;
            } else
                output.uncheckedAppend(byte);
        } else
            output.uncheckedAppend(byte);
    }
    return output;
}

template<typename CharacterType> Optional<URLParser::LCharBuffer> URLParser::domainToASCII(StringImpl& domain, const CodePointIterator<CharacterType>& iteratorForSyntaxViolationPosition)
{
    LCharBuffer ascii;
    if (domain.isAllASCII()) {
        size_t length = domain.length();
        if (domain.is8Bit()) {
            const LChar* characters = domain.characters8();
            ascii.reserveInitialCapacity(length);
            for (size_t i = 0; i < length; ++i) {
                if (UNLIKELY(isASCIIUpper(characters[i])))
                    syntaxViolation(iteratorForSyntaxViolationPosition);
                ascii.uncheckedAppend(toASCIILower(characters[i]));
            }
        } else {
            const UChar* characters = domain.characters16();
            ascii.reserveInitialCapacity(length);
            for (size_t i = 0; i < length; ++i) {
                if (UNLIKELY(isASCIIUpper(characters[i])))
                    syntaxViolation(iteratorForSyntaxViolationPosition);
                ascii.uncheckedAppend(toASCIILower(characters[i]));
            }
        }
        return ascii;
    }
    
    const size_t maxDomainLength = 64;
    UChar hostnameBuffer[maxDomainLength];
    UErrorCode error = U_ZERO_ERROR;
    UIDNAInfo processingDetails = UIDNA_INFO_INITIALIZER;
    int32_t numCharactersConverted = uidna_nameToASCII(&internationalDomainNameTranscoder(), StringView(domain).upconvertedCharacters(), domain.length(), hostnameBuffer, maxDomainLength, &processingDetails, &error);

    if (U_SUCCESS(error) && !processingDetails.errors) {
#if ASSERT_ENABLED
        for (int32_t i = 0; i < numCharactersConverted; ++i) {
            ASSERT(isASCII(hostnameBuffer[i]));
            ASSERT(!isASCIIUpper(hostnameBuffer[i]));
        }
#else
        UNUSED_PARAM(numCharactersConverted);
#endif // ASSERT_ENABLED
        ascii.append(hostnameBuffer, numCharactersConverted);
        if (domain != StringView(ascii.data(), ascii.size()))
            syntaxViolation(iteratorForSyntaxViolationPosition);
        return ascii;
    }
    return WTF::nullopt;
}

bool URLParser::hasForbiddenHostCodePoint(const URLParser::LCharBuffer& asciiDomain)
{
    for (size_t i = 0; i < asciiDomain.size(); ++i) {
        if (isForbiddenHostCodePoint(asciiDomain[i]))
            return true;
    }
    return false;
}

template<typename CharacterType>
bool URLParser::parsePort(CodePointIterator<CharacterType>& iterator)
{
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
bool URLParser::parseHostAndPort(CodePointIterator<CharacterType> iterator)
{
    if (iterator.atEnd())
        return false;
    if (*iterator == ':')
        return false;
    if (*iterator == '[') {
        auto ipv6End = iterator;
        while (!ipv6End.atEnd() && *ipv6End != ']')
            ++ipv6End;
        if (ipv6End.atEnd())
            return false;
        if (auto address = parseIPv6Host(CodePointIterator<CharacterType>(iterator, ipv6End))) {
            serializeIPv6(address.value());
            if (!ipv6End.atEnd()) {
                advance(ipv6End);
                m_url.m_hostEnd = currentPosition(ipv6End);
                if (!ipv6End.atEnd() && *ipv6End == ':')
                    return parsePort(ipv6End);
                m_url.m_portLength = 0;
                return ipv6End.atEnd();
            }
            m_url.m_hostEnd = currentPosition(ipv6End);
            return true;
        }
        return false;
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
                return false;
            utf8PercentEncode<isInSimpleEncodeSet>(iterator);
        }
        m_url.m_hostEnd = currentPosition(iterator);
        if (iterator.atEnd()) {
            m_url.m_portLength = 0;
            return true;
        }
        return parsePort(iterator);
    }
    
    if (LIKELY(!m_hostHasPercentOrNonASCII)) {
        auto hostIterator = iterator;
        for (; !iterator.atEnd(); ++iterator) {
            if (isTabOrNewline(*iterator))
                continue;
            if (*iterator == ':')
                break;
            if (isForbiddenHostCodePoint(*iterator))
                return false;
        }
        auto address = parseIPv4Host(hostIterator, CodePointIterator<CharacterType>(hostIterator, iterator));
        if (address) {
            serializeIPv4(address.value());
            m_url.m_hostEnd = currentPosition(iterator);
            if (iterator.atEnd()) {
                m_url.m_portLength = 0;
                return true;
            }
            return parsePort(iterator);
        }
        if (address.error() == IPv4ParsingError::Failure)
            return false;
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
        if (!hostIterator.atEnd())
            return parsePort(hostIterator);
        unsigned portLength = currentPosition(iterator) - m_url.m_hostEnd;
        RELEASE_ASSERT(portLength <= URL::maxPortLength);
        m_url.m_portLength = portLength;
        return true;
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

        if (!U_IS_UNICODE_CHAR(*iterator))
            return false;
        uint8_t buffer[U8_MAX_LENGTH];
        int32_t offset = 0;
        U8_APPEND_UNSAFE(buffer, offset, *iterator);
        utf8Encoded.append(buffer, offset);
    }
    LCharBuffer percentDecoded = percentDecode(utf8Encoded.data(), utf8Encoded.size(), hostBegin);
    String domain = String::fromUTF8(percentDecoded.data(), percentDecoded.size());
    if (domain.isNull())
        return false;
    if (domain != StringView(percentDecoded.data(), percentDecoded.size()))
        syntaxViolation(hostBegin);
    auto asciiDomain = domainToASCII(*domain.impl(), hostBegin);
    if (!asciiDomain || hasForbiddenHostCodePoint(asciiDomain.value()))
        return false;
    LCharBuffer& asciiDomainValue = asciiDomain.value();
    const LChar* asciiDomainCharacters = asciiDomainValue.data();

    auto address = parseIPv4Host(hostBegin, CodePointIterator<LChar>(asciiDomainValue.begin(), asciiDomainValue.end()));
    if (address) {
        serializeIPv4(address.value());
        m_url.m_hostEnd = currentPosition(iterator);
        if (iterator.atEnd()) {
            m_url.m_portLength = 0;
            return true;
        }
        return parsePort(iterator);
    }
    if (address.error() == IPv4ParsingError::Failure)
        return false;

    appendToASCIIBuffer(asciiDomainCharacters, asciiDomainValue.size());
    m_url.m_hostEnd = currentPosition(iterator);
    if (!iterator.atEnd())
        return parsePort(iterator);
    m_url.m_portLength = 0;
    return true;
}

Optional<String> URLParser::formURLDecode(StringView input)
{
    auto utf8 = input.utf8(StrictConversion);
    if (utf8.isNull())
        return WTF::nullopt;
    auto percentDecoded = percentDecode(reinterpret_cast<const LChar*>(utf8.data()), utf8.length());
    return String::fromUTF8ReplacingInvalidSequences(percentDecoded.data(), percentDecoded.size());
}

// https://url.spec.whatwg.org/#concept-urlencoded-parser
auto URLParser::parseURLEncodedForm(StringView input) -> URLEncodedForm
{
    URLEncodedForm output;
    for (StringView bytes : input.split('&')) {
        auto equalIndex = bytes.find('=');
        if (equalIndex == notFound) {
            auto name = formURLDecode(bytes.toString().replace('+', 0x20));
            if (name)
                output.append({ name.value(), emptyString() });
        } else {
            auto name = formURLDecode(bytes.substring(0, equalIndex).toString().replace('+', 0x20));
            auto value = formURLDecode(bytes.substring(equalIndex + 1).toString().replace('+', 0x20));
            if (name && value)
                output.append({ name.value(), value.value() });
        }
    }
    return output;
}

static void serializeURLEncodedForm(const String& input, Vector<LChar>& output)
{
    auto utf8 = input.utf8(StrictConversion);
    const char* data = utf8.data();
    for (size_t i = 0; i < utf8.length(); ++i) {
        const char byte = data[i];
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
        RELEASE_ASSERT(U_SUCCESS(error));
        RELEASE_ASSERT(encoder);
    });
    return *encoder;
}

bool URLParser::allValuesEqual(const URL& a, const URL& b)
{
    URL_PARSER_LOG("%d %d %d %d %d %d %d %d %d %d %d %d %s\n%d %d %d %d %d %d %d %d %d %d %d %d %s",
        a.m_isValid,
        a.m_cannotBeABaseURL,
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
        b.m_cannotBeABaseURL,
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
        && a.m_cannotBeABaseURL == b.m_cannotBeABaseURL
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
