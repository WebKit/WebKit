/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "URLParser.h"

#include "Logging.h"
#include <array>
#include <unicode/uidna.h>
#include <unicode/utypes.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

template<typename CharacterType>
class CodePointIterator {
public:
    CodePointIterator() { }
    CodePointIterator(const CharacterType* begin, const CharacterType* end)
        : m_begin(begin)
        , m_end(end)
    {
    }
    
    CodePointIterator(const CodePointIterator& begin, const CodePointIterator& end)
        : CodePointIterator(begin.m_begin, end.m_begin)
    {
        ASSERT(end.m_begin >= begin.m_begin);
    }
    
    UChar32 operator*() const;
    CodePointIterator& operator++();

    bool operator==(const CodePointIterator& other) const
    {
        return m_begin == other.m_begin
            && m_end == other.m_end;
    }
    bool operator!=(const CodePointIterator& other) const { return !(*this == other); }
    
    CodePointIterator& operator=(const CodePointIterator& other)
    {
        m_begin = other.m_begin;
        m_end = other.m_end;
        return *this;
    }

    bool atEnd() const
    {
        ASSERT(m_begin <= m_end);
        return m_begin >= m_end;
    }
    
private:
    const CharacterType* m_begin { nullptr };
    const CharacterType* m_end { nullptr };
};

template<>
UChar32 CodePointIterator<LChar>::operator*() const
{
    ASSERT(!atEnd());
    return *m_begin;
}

template<>
auto CodePointIterator<LChar>::operator++() -> CodePointIterator&
{
    ASSERT(!atEnd());
    m_begin++;
    return *this;
}

template<>
UChar32 CodePointIterator<UChar>::operator*() const
{
    ASSERT(!atEnd());
    UChar32 c;
    U16_GET(m_begin, 0, 0, m_end - m_begin, c);
    return c;
}

template<>
auto CodePointIterator<UChar>::operator++() -> CodePointIterator&
{
    ASSERT(!atEnd());
    if (U16_IS_LEAD(m_begin[0]) && m_begin < m_end && U16_IS_TRAIL(m_begin[1]))
        m_begin += 2;
    else
        m_begin++;
    return *this;
}

enum URLCharacterClass {
    UserInfo = 0x1,
    Default = 0x2,
    InvalidDomain = 0x4,
    QueryPercent = 0x8,
    SlashQuestionOrHash = 0x10,
};

static const uint8_t characterClassTable[256] = {
    UserInfo | Default | InvalidDomain | QueryPercent, // 0x0
    UserInfo | Default | QueryPercent, // 0x1
    UserInfo | Default | QueryPercent, // 0x2
    UserInfo | Default | QueryPercent, // 0x3
    UserInfo | Default | QueryPercent, // 0x4
    UserInfo | Default | QueryPercent, // 0x5
    UserInfo | Default | QueryPercent, // 0x6
    UserInfo | Default | QueryPercent, // 0x7
    UserInfo | Default | QueryPercent, // 0x8
    UserInfo | Default | InvalidDomain | QueryPercent, // 0x9
    UserInfo | Default | InvalidDomain | QueryPercent, // 0xA
    UserInfo | Default | QueryPercent, // 0xB
    UserInfo | Default | QueryPercent, // 0xC
    UserInfo | Default | InvalidDomain | QueryPercent, // 0xD
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
    UserInfo | Default | InvalidDomain | QueryPercent, // ' '
    0, // '!'
    UserInfo | Default | QueryPercent, // '"'
    UserInfo | Default | InvalidDomain | QueryPercent | SlashQuestionOrHash, // '#'
    0, // '$'
    InvalidDomain, // '%'
    0, // '&'
    0, // '''
    0, // '('
    0, // ')'
    0, // '*'
    0, // '+'
    0, // ','
    0, // '-'
    0, // '.'
    UserInfo | InvalidDomain | SlashQuestionOrHash, // '/'
    0, // '0'
    0, // '1'
    0, // '2'
    0, // '3'
    0, // '4'
    0, // '5'
    0, // '6'
    0, // '7'
    0, // '8'
    0, // '9'
    UserInfo | InvalidDomain, // ':'
    UserInfo, // ';'
    UserInfo | Default | QueryPercent, // '<'
    UserInfo, // '='
    UserInfo | Default | QueryPercent, // '>'
    UserInfo | Default | InvalidDomain | SlashQuestionOrHash, // '?'
    UserInfo | InvalidDomain, // '@'
    0, // 'A'
    0, // 'B'
    0, // 'C'
    0, // 'D'
    0, // 'E'
    0, // 'F'
    0, // 'G'
    0, // 'H'
    0, // 'I'
    0, // 'J'
    0, // 'K'
    0, // 'L'
    0, // 'M'
    0, // 'N'
    0, // 'O'
    0, // 'P'
    0, // 'Q'
    0, // 'R'
    0, // 'S'
    0, // 'T'
    0, // 'U'
    0, // 'V'
    0, // 'W'
    0, // 'X'
    0, // 'Y'
    0, // 'Z'
    UserInfo | InvalidDomain, // '['
    UserInfo | InvalidDomain | SlashQuestionOrHash, // '\\'
    UserInfo | InvalidDomain, // ']'
    UserInfo, // '^'
    0, // '_'
    UserInfo | Default, // '`'
    0, // 'a'
    0, // 'b'
    0, // 'c'
    0, // 'd'
    0, // 'e'
    0, // 'f'
    0, // 'g'
    0, // 'h'
    0, // 'i'
    0, // 'j'
    0, // 'k'
    0, // 'l'
    0, // 'm'
    0, // 'n'
    0, // 'o'
    0, // 'p'
    0, // 'q'
    0, // 'r'
    0, // 's'
    0, // 't'
    0, // 'u'
    0, // 'v'
    0, // 'w'
    0, // 'x'
    0, // 'y'
    0, // 'z'
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

template<typename CharacterType> static bool isC0Control(CharacterType character) { return character <= 0x1F; }
template<typename CharacterType> static bool isC0ControlOrSpace(CharacterType character) { return character <= 0x20; }
template<typename CharacterType> static bool isTabOrNewline(CharacterType character) { return character <= 0xD && character >= 0x9 && character != 0xB && character != 0xC; }
template<typename CharacterType> static bool isInSimpleEncodeSet(CharacterType character) { return character > 0x7E || isC0Control(character); }
template<typename CharacterType> static bool isInDefaultEncodeSet(CharacterType character) { return character > 0x7E || characterClassTable[character] & Default; }
template<typename CharacterType> static bool isInUserInfoEncodeSet(CharacterType character) { return character > 0x7E || characterClassTable[character] & UserInfo; }
template<typename CharacterType> static bool isInvalidDomainCharacter(CharacterType character) { return character <= ']' && characterClassTable[character] & InvalidDomain; }
template<typename CharacterType> static bool isPercentOrNonASCII(CharacterType character) { return !isASCII(character) || character == '%'; }
template<typename CharacterType> static bool isSlashQuestionOrHash(CharacterType character) { return character <= '\\' && characterClassTable[character] & SlashQuestionOrHash; }
static bool shouldPercentEncodeQueryByte(uint8_t byte) { return characterClassTable[byte] & QueryPercent; }
    
template<typename CharacterType>
static bool isWindowsDriveLetter(CodePointIterator<CharacterType> iterator)
{
    if (iterator.atEnd() || !isASCIIAlpha(*iterator))
        return false;
    ++iterator;
    if (iterator.atEnd())
        return false;
    return *iterator == ':' || *iterator == '|';
}

static bool isWindowsDriveLetter(const StringBuilder& builder, size_t index)
{
    if (builder.length() < index + 2)
        return false;
    return isASCIIAlpha(builder[index]) && (builder[index + 1] == ':' || builder[index + 1] == '|');
}

template<typename CharacterType>
static bool shouldCopyFileURL(CodePointIterator<CharacterType> iterator)
{
    if (isWindowsDriveLetter(iterator))
        return true;
    if (iterator.atEnd())
        return false;
    ++iterator;
    if (iterator.atEnd())
        return true;
    ++iterator;
    if (iterator.atEnd())
        return true;
    return !isSlashQuestionOrHash(*iterator);
}

static void percentEncode(uint8_t byte, StringBuilder& builder)
{
    builder.append('%');
    builder.append(upperNibbleToASCIIHexDigit(byte));
    builder.append(lowerNibbleToASCIIHexDigit(byte));
}

static void utf8PercentEncode(UChar32 codePoint, StringBuilder& builder, bool(*isInCodeSet)(UChar32))
{
    if (isInCodeSet(codePoint)) {
        uint8_t buffer[U8_MAX_LENGTH];
        int32_t offset = 0;
        UBool error = false;
        U8_APPEND(buffer, offset, U8_MAX_LENGTH, codePoint, error);
        // FIXME: Check error.
        for (int32_t i = 0; i < offset; ++i)
            percentEncode(buffer[i], builder);
    } else
        builder.append(codePoint);
}

static void utf8PercentEncodeQuery(UChar32 codePoint, StringBuilder& builder)
{
    uint8_t buffer[U8_MAX_LENGTH];
    int32_t offset = 0;
    UBool error = false;
    U8_APPEND(buffer, offset, U8_MAX_LENGTH, codePoint, error);
    ASSERT_WITH_SECURITY_IMPLICATION(offset <= static_cast<int32_t>(sizeof(buffer)));
    // FIXME: Check error.
    for (int32_t i = 0; i < offset; ++i) {
        auto byte = buffer[i];
        if (shouldPercentEncodeQueryByte(byte))
            percentEncode(byte, builder);
        else
            builder.append(byte);
    }
}
    
static void encodeQuery(const StringBuilder& source, StringBuilder& destination, const TextEncoding& encoding)
{
    // FIXME: It is unclear in the spec what to do when encoding fails. The behavior should be specified and tested.
    CString encoded = encoding.encode(source.toStringPreserveCapacity(), URLEncodedEntitiesForUnencodables);
    const char* data = encoded.data();
    size_t length = encoded.length();
    for (size_t i = 0; i < length; ++i) {
        uint8_t byte = data[i];
        if (shouldPercentEncodeQueryByte(byte))
            percentEncode(byte, destination);
        else
            destination.append(byte);
    }
}

static bool isDefaultPort(StringView scheme, uint16_t port)
{
    static const uint16_t ftpPort = 21;
    static const uint16_t gopherPort = 70;
    static const uint16_t httpPort = 80;
    static const uint16_t httpsPort = 443;
    static const uint16_t wsPort = 80;
    static const uint16_t wssPort = 443;
    
    auto length = scheme.length();
    if (!length)
        return false;
    switch (scheme[0]) {
    case 'w':
        switch (length) {
        case 2:
            return scheme[1] == 's'
                && port == wsPort;
        case 3:
            return scheme[1] == 's'
                && scheme[2] == 's'
                && port == wssPort;
        default:
            return false;
        }
    case 'h':
        switch (length) {
        case 4:
            return scheme[1] == 't'
                && scheme[2] == 't'
                && scheme[3] == 'p'
                && port == httpPort;
        case 5:
            return scheme[1] == 't'
                && scheme[2] == 't'
                && scheme[3] == 'p'
                && scheme[4] == 's'
                && port == httpsPort;
        default:
            return false;
        }
    case 'g':
        return length == 6
            && scheme[1] == 'o'
            && scheme[2] == 'p'
            && scheme[3] == 'h'
            && scheme[4] == 'e'
            && scheme[5] == 'r'
            && port == gopherPort;
    case 'f':
        return length == 3
            && scheme[1] == 't'
            && scheme[2] == 'p'
            && port == ftpPort;
        return false;
    default:
        return false;
    }
}

static bool isSpecialScheme(StringView scheme)
{
    auto length = scheme.length();
    if (!length)
        return false;
    switch (scheme[0]) {
    case 'f':
        switch (length) {
        case 3:
            return scheme[1] == 't'
                && scheme[2] == 'p';
        case 4:
            return scheme[1] == 'i'
                && scheme[2] == 'l'
                && scheme[3] == 'e';
        default:
            return false;
        }
    case 'g':
        return length == 6
            && scheme[1] == 'o'
            && scheme[2] == 'p'
            && scheme[3] == 'h'
            && scheme[4] == 'e'
            && scheme[5] == 'r';
    case 'h':
        switch (length) {
        case 4:
            return scheme[1] == 't'
                && scheme[2] == 't'
                && scheme[3] == 'p';
        case 5:
            return scheme[1] == 't'
                && scheme[2] == 't'
                && scheme[3] == 'p'
                && scheme[4] == 's';
        default:
            return false;
        }
    case 'w':
        switch (length) {
        case 2:
            return scheme[1] == 's';
        case 3:
            return scheme[1] == 's'
                && scheme[2] == 's';
        default:
            return false;
        }
    default:
        return false;
    }
}

template<typename T>
static StringView bufferView(const T& buffer, unsigned start, unsigned length)
{
    ASSERT(buffer.length() >= length);
    if (buffer.is8Bit())
        return StringView(buffer.characters8() + start, length);
    return StringView(buffer.characters16() + start, length);
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
    FragmentEnd,
};

size_t URLParser::urlLengthUntilPart(const URL& url, URLPart part)
{
    switch (part) {
    case URLPart::FragmentEnd:
        return url.m_fragmentEnd;
    case URLPart::QueryEnd:
        return url.m_queryEnd;
    case URLPart::PathEnd:
        return url.m_pathEnd;
    case URLPart::PathAfterLastSlash:
        return url.m_pathAfterLastSlash;
    case URLPart::PortEnd:
        return url.m_portEnd;
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
    
void URLParser::copyURLPartsUntil(const URL& base, URLPart part)
{
    m_buffer.clear();
    m_buffer.append(base.m_string.substring(0, urlLengthUntilPart(base, part)));
    switch (part) {
    case URLPart::FragmentEnd:
        m_url.m_fragmentEnd = base.m_fragmentEnd;
        FALLTHROUGH;
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
        m_url.m_portEnd = base.m_portEnd;
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
    m_urlIsSpecial = isSpecialScheme(bufferView(m_buffer, 0, m_url.m_schemeEnd));
}

static const char* dotASCIICode = "2e";

template<typename CharacterType>
static bool isPercentEncodedDot(CodePointIterator<CharacterType> c)
{
    if (c.atEnd())
        return false;
    if (*c != '%')
        return false;
    ++c;
    if (c.atEnd())
        return false;
    if (*c != dotASCIICode[0])
        return false;
    ++c;
    if (c.atEnd())
        return false;
    return toASCIILower(*c) == dotASCIICode[1];
}

template<typename CharacterType>
static bool isSingleDotPathSegment(CodePointIterator<CharacterType> c)
{
    if (c.atEnd())
        return false;
    if (*c == '.') {
        ++c;
        return c.atEnd() || isSlashQuestionOrHash(*c);
    }
    if (*c != '%')
        return false;
    ++c;
    if (c.atEnd() || *c != dotASCIICode[0])
        return false;
    ++c;
    if (c.atEnd())
        return false;
    if (toASCIILower(*c) == dotASCIICode[1]) {
        ++c;
        return c.atEnd() || isSlashQuestionOrHash(*c);
    }
    return false;
}

template<typename CharacterType>
static bool isDoubleDotPathSegment(CodePointIterator<CharacterType> c)
{
    if (c.atEnd())
        return false;
    if (*c == '.') {
        ++c;
        return isSingleDotPathSegment(c);
    }
    if (*c != '%')
        return false;
    ++c;
    if (c.atEnd() || *c != dotASCIICode[0])
        return false;
    ++c;
    if (c.atEnd())
        return false;
    if (toASCIILower(*c) == dotASCIICode[1]) {
        ++c;
        return isSingleDotPathSegment(c);
    }
    return false;
}

template<typename CharacterType>
static void consumeSingleDotPathSegment(CodePointIterator<CharacterType>& c)
{
    ASSERT(isSingleDotPathSegment(c));
    if (*c == '.') {
        ++c;
        if (!c.atEnd()) {
            if (*c == '/' || *c == '\\')
                ++c;
            else
                ASSERT(*c == '?' || *c == '#');
        }
    } else {
        ASSERT(*c == '%');
        ++c;
        ASSERT(*c == dotASCIICode[0]);
        ++c;
        ASSERT(toASCIILower(*c) == dotASCIICode[1]);
        ++c;
        if (!c.atEnd()) {
            if (*c == '/' || *c == '\\')
                ++c;
            else
                ASSERT(*c == '?' || *c == '#');
        }
    }
}

template<typename CharacterType>
static void consumeDoubleDotPathSegment(CodePointIterator<CharacterType>& c)
{
    ASSERT(isDoubleDotPathSegment(c));
    if (*c == '.')
        ++c;
    else {
        ASSERT(*c == '%');
        ++c;
        ASSERT(*c == dotASCIICode[0]);
        ++c;
        ASSERT(toASCIILower(*c) == dotASCIICode[1]);
        ++c;
    }
    consumeSingleDotPathSegment(c);
}

void URLParser::popPath()
{
    if (m_url.m_pathAfterLastSlash > m_url.m_portEnd + 1) {
        m_url.m_pathAfterLastSlash--;
        if (m_buffer[m_url.m_pathAfterLastSlash] == '/')
            m_url.m_pathAfterLastSlash--;
        while (m_url.m_pathAfterLastSlash > m_url.m_portEnd && m_buffer[m_url.m_pathAfterLastSlash] != '/')
            m_url.m_pathAfterLastSlash--;
        m_url.m_pathAfterLastSlash++;
    }
    m_buffer.resize(m_url.m_pathAfterLastSlash);
}

template<typename CharacterType>
URL URLParser::failure(const CharacterType* input, unsigned length)
{
    URL url;
    url.m_isValid = false;
    url.m_protocolIsInHTTPFamily = false;
    url.m_cannotBeABaseURL = false;
    url.m_schemeEnd = 0;
    url.m_userStart = 0;
    url.m_userEnd = 0;
    url.m_passwordEnd = 0;
    url.m_hostEnd = 0;
    url.m_portEnd = 0;
    url.m_pathAfterLastSlash = 0;
    url.m_pathEnd = 0;
    url.m_queryEnd = 0;
    url.m_fragmentEnd = 0;
    url.m_string = String(input, length);
    return url;
}

URL URLParser::parse(const String& input, const URL& base, const TextEncoding& encoding)
{
    if (input.is8Bit())
        return parse(input.characters8(), input.length(), base, encoding);
    return parse(input.characters16(), input.length(), base, encoding);
}

template<typename CharacterType>
URL URLParser::parse(const CharacterType* input, const unsigned length, const URL& base, const TextEncoding& encoding)
{
    LOG(URLParser, "Parsing URL <%s> base <%s>", String(input, length).utf8().data(), base.string().utf8().data());
    m_url = { };
    m_buffer.clear();
    m_buffer.reserveCapacity(length);
    
    bool isUTF8Encoding = encoding == UTF8Encoding();
    StringBuilder queryBuffer;

    unsigned endIndex = length;
    while (endIndex && isC0ControlOrSpace(input[endIndex - 1]))
        endIndex--;
    CodePointIterator<CharacterType> c(input, input + endIndex);
    CodePointIterator<CharacterType> authorityOrHostBegin;
    while (!c.atEnd() && isC0ControlOrSpace(*c))
        ++c;
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
        Query,
        Fragment,
    };

#define LOG_STATE(x) LOG(URLParser, "State %s, code point %c, buffer length %d", x, *c, m_buffer.length())
#define LOG_FINAL_STATE(x) LOG(URLParser, "Final State: %s", x)

    State state = State::SchemeStart;
    while (!c.atEnd()) {
        if (isTabOrNewline(*c)) {
            ++c;
            continue;
        }

        switch (state) {
        case State::SchemeStart:
            LOG_STATE("SchemeStart");
            if (isASCIIAlpha(*c)) {
                m_buffer.append(toASCIILower(*c));
                ++c;
                state = State::Scheme;
            } else
                state = State::NoScheme;
            break;
        case State::Scheme:
            LOG_STATE("Scheme");
            if (isASCIIAlphanumeric(*c) || *c == '+' || *c == '-' || *c == '.')
                m_buffer.append(toASCIILower(*c));
            else if (*c == ':') {
                m_url.m_schemeEnd = m_buffer.length();
                StringView urlScheme = bufferView(m_buffer, 0, m_url.m_schemeEnd);
                m_url.m_protocolIsInHTTPFamily = urlScheme == "http" || urlScheme == "https";
                if (urlScheme == "file") {
                    m_urlIsSpecial = true;
                    state = State::File;
                    m_buffer.append(':');
                    ++c;
                    break;
                }
                m_buffer.append(':');
                if (isSpecialScheme(urlScheme)) {
                    m_urlIsSpecial = true;
                    if (base.protocol() == urlScheme)
                        state = State::SpecialRelativeOrAuthority;
                    else
                        state = State::SpecialAuthoritySlashes;
                } else {
                    m_url.m_userStart = m_buffer.length();
                    m_url.m_userEnd = m_url.m_userStart;
                    m_url.m_passwordEnd = m_url.m_userStart;
                    m_url.m_hostEnd = m_url.m_userStart;
                    m_url.m_portEnd = m_url.m_userStart;
                    auto maybeSlash = c;
                    ++maybeSlash;
                    while (!maybeSlash.atEnd() && isTabOrNewline(*maybeSlash))
                        ++maybeSlash;
                    if (!maybeSlash.atEnd() && *maybeSlash == '/') {
                        m_buffer.append('/');
                        m_url.m_pathAfterLastSlash = m_url.m_userStart + 1;
                        state = State::PathOrAuthority;
                        c = maybeSlash;
                        ASSERT(*c == '/');
                    } else {
                        m_url.m_pathAfterLastSlash = m_url.m_userStart;
                        m_url.m_cannotBeABaseURL = true;
                        state = State::CannotBeABaseURLPath;
                    }
                }
                ++c;
                break;
            } else {
                m_buffer.clear();
                state = State::NoScheme;
                c = beginAfterControlAndSpace;
                break;
            }
            ++c;
            while (!c.atEnd() && isTabOrNewline(*c))
                ++c;
            if (c.atEnd()) {
                m_buffer.clear();
                state = State::NoScheme;
                c = beginAfterControlAndSpace;
            }
            break;
        case State::NoScheme:
            LOG_STATE("NoScheme");
            if (base.isNull() || (base.m_cannotBeABaseURL && *c != '#'))
                return failure(input, length);
            if (base.m_cannotBeABaseURL && *c == '#') {
                copyURLPartsUntil(base, URLPart::QueryEnd);
                state = State::Fragment;
                m_buffer.append('#');
                ++c;
                break;
            }
            if (base.protocol() != "file") {
                state = State::Relative;
                break;
            }
            copyURLPartsUntil(base, URLPart::SchemeEnd);
            m_buffer.append(':');
            state = State::File;
            break;
        case State::SpecialRelativeOrAuthority:
            LOG_STATE("SpecialRelativeOrAuthority");
            if (*c == '/') {
                m_buffer.append('/');
                ++c;
                while (!c.atEnd() && isTabOrNewline(*c))
                    ++c;
                if (c.atEnd())
                    return failure(input, length);
                if (*c == '/') {
                    m_buffer.append('/');
                    state = State::SpecialAuthorityIgnoreSlashes;
                    ++c;
                }
            } else
                state = State::Relative;
            break;
        case State::PathOrAuthority:
            LOG_STATE("PathOrAuthority");
            if (*c == '/') {
                m_buffer.append('/');
                m_url.m_userStart = m_buffer.length();
                state = State::AuthorityOrHost;
                ++c;
                authorityOrHostBegin = c;
            } else
                state = State::Path;
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
                copyURLPartsUntil(base, URLPart::PathEnd);
                m_buffer.append('?');
                state = State::Query;
                ++c;
                break;
            case '#':
                copyURLPartsUntil(base, URLPart::QueryEnd);
                m_buffer.append('#');
                state = State::Fragment;
                ++c;
                break;
            default:
                copyURLPartsUntil(base, URLPart::PathAfterLastSlash);
                state = State::Path;
                break;
            }
            break;
        case State::RelativeSlash:
            LOG_STATE("RelativeSlash");
            if (*c == '/' || *c == '\\') {
                ++c;
                copyURLPartsUntil(base, URLPart::SchemeEnd);
                m_buffer.append("://");
                state = State::SpecialAuthorityIgnoreSlashes;
            } else {
                copyURLPartsUntil(base, URLPart::PortEnd);
                m_buffer.append('/');
                m_url.m_pathAfterLastSlash = base.m_portEnd + 1;
                state = State::Path;
            }
            break;
        case State::SpecialAuthoritySlashes:
            LOG_STATE("SpecialAuthoritySlashes");
            m_buffer.append("//");
            if (*c == '/' || *c == '\\') {
                ++c;
                while (!c.atEnd() && isTabOrNewline(*c))
                    ++c;
                if (c.atEnd())
                    return failure(input, length);
                if (*c == '/' || *c == '\\')
                    ++c;
            }
            state = State::SpecialAuthorityIgnoreSlashes;
            break;
        case State::SpecialAuthorityIgnoreSlashes:
            LOG_STATE("SpecialAuthorityIgnoreSlashes");
            if (*c == '/' || *c == '\\') {
                m_buffer.append('/');
                ++c;
            }
            m_url.m_userStart = m_buffer.length();
            state = State::AuthorityOrHost;
            authorityOrHostBegin = c;
            break;
        case State::AuthorityOrHost:
            LOG_STATE("AuthorityOrHost");
            {
                if (*c == '@') {
                    parseAuthority(CodePointIterator<CharacterType>(authorityOrHostBegin, c));
                    ++c;
                    while (!c.atEnd() && isTabOrNewline(*c))
                        ++c;
                    authorityOrHostBegin = c;
                    state = State::Host;
                    m_hostHasPercentOrNonASCII = false;
                    break;
                }
                bool isSlash = *c == '/' || (m_urlIsSpecial && *c == '\\');
                if (isSlash || *c == '?' || *c == '#') {
                    m_url.m_userEnd = m_buffer.length();
                    m_url.m_passwordEnd = m_url.m_userEnd;
                    if (!parseHost(CodePointIterator<CharacterType>(authorityOrHostBegin, c)))
                        return failure(input, length);
                    if (!isSlash) {
                        m_buffer.append('/');
                        m_url.m_pathAfterLastSlash = m_buffer.length();
                    }
                    state = State::Path;
                    break;
                }
                if (isPercentOrNonASCII(*c))
                    m_hostHasPercentOrNonASCII = true;
                ++c;
            }
            break;
        case State::Host:
            LOG_STATE("Host");
            if (*c == '/' || *c == '?' || *c == '#') {
                if (!parseHost(CodePointIterator<CharacterType>(authorityOrHostBegin, c)))
                    return failure(input, length);
                state = State::Path;
                break;
            }
            if (isPercentOrNonASCII(*c))
                m_hostHasPercentOrNonASCII = true;
            ++c;
            break;
        case State::File:
            LOG_STATE("File");
            switch (*c) {
            case '/':
            case '\\':
                m_buffer.append('/');
                state = State::FileSlash;
                ++c;
                break;
            case '?':
                if (!base.isNull() && base.protocolIs("file"))
                    copyURLPartsUntil(base, URLPart::PathEnd);
                m_buffer.append("///?");
                m_url.m_userStart = m_buffer.length() - 2;
                m_url.m_userEnd = m_url.m_userStart;
                m_url.m_passwordEnd = m_url.m_userStart;
                m_url.m_hostEnd = m_url.m_userStart;
                m_url.m_portEnd = m_url.m_userStart;
                m_url.m_pathAfterLastSlash = m_url.m_userStart + 1;
                m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
                state = State::Query;
                ++c;
                break;
            case '#':
                if (!base.isNull() && base.protocolIs("file"))
                    copyURLPartsUntil(base, URLPart::QueryEnd);
                m_buffer.append("///#");
                m_url.m_userStart = m_buffer.length() - 2;
                m_url.m_userEnd = m_url.m_userStart;
                m_url.m_passwordEnd = m_url.m_userStart;
                m_url.m_hostEnd = m_url.m_userStart;
                m_url.m_portEnd = m_url.m_userStart;
                m_url.m_pathAfterLastSlash = m_url.m_userStart + 1;
                m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
                m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
                state = State::Fragment;
                ++c;
                break;
            default:
                if (!base.isNull() && base.protocolIs("file") && shouldCopyFileURL(c))
                    copyURLPartsUntil(base, URLPart::PathAfterLastSlash);
                else {
                    m_buffer.append("///");
                    m_url.m_userStart = m_buffer.length() - 1;
                    m_url.m_userEnd = m_url.m_userStart;
                    m_url.m_passwordEnd = m_url.m_userStart;
                    m_url.m_hostEnd = m_url.m_userStart;
                    m_url.m_portEnd = m_url.m_userStart;
                    m_url.m_pathAfterLastSlash = m_url.m_userStart + 1;
                }
                state = State::Path;
                break;
            }
            break;
        case State::FileSlash:
            LOG_STATE("FileSlash");
            if (*c == '/' || *c == '\\') {
                ++c;
                m_buffer.append('/');
                m_url.m_userStart = m_buffer.length();
                m_url.m_userEnd = m_url.m_userStart;
                m_url.m_passwordEnd = m_url.m_userStart;
                m_url.m_hostEnd = m_url.m_userStart;
                m_url.m_portEnd = m_url.m_userStart;
                authorityOrHostBegin = c;
                state = State::FileHost;
                break;
            }
            if (!base.isNull() && base.protocolIs("file")) {
                // FIXME: This String copy is unnecessary.
                String basePath = base.path();
                if (basePath.length() >= 2) {
                    bool windowsQuirk = basePath.is8Bit()
                        ? isWindowsDriveLetter(CodePointIterator<LChar>(basePath.characters8(), basePath.characters8() + basePath.length()))
                        : isWindowsDriveLetter(CodePointIterator<UChar>(basePath.characters16(), basePath.characters16() + basePath.length()));
                    if (windowsQuirk) {
                        m_buffer.append(basePath[0]);
                        m_buffer.append(basePath[1]);
                    }
                }
                state = State::Path;
                break;
            }
            m_buffer.append("//");
            m_url.m_userStart = m_buffer.length() - 1;
            m_url.m_userEnd = m_url.m_userStart;
            m_url.m_passwordEnd = m_url.m_userStart;
            m_url.m_hostEnd = m_url.m_userStart;
            m_url.m_portEnd = m_url.m_userStart;
            m_url.m_pathAfterLastSlash = m_url.m_userStart + 1;
            state = State::Path;
            break;
        case State::FileHost:
            LOG_STATE("FileHost");
            if (isSlashQuestionOrHash(*c)) {
                if (isWindowsDriveLetter(m_buffer, m_url.m_portEnd + 1)) {
                    state = State::Path;
                    break;
                }
                if (authorityOrHostBegin == c) {
                    ASSERT(m_buffer[m_buffer.length() - 1] == '/');
                    if (*c == '?') {
                        m_buffer.append("/?");
                        m_url.m_pathAfterLastSlash = m_buffer.length() - 1;
                        m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
                        state = State::Query;
                        ++c;
                        break;
                    }
                    if (*c == '#') {
                        m_buffer.append("/#");
                        m_url.m_pathAfterLastSlash = m_buffer.length() - 1;
                        m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
                        m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
                        state = State::Fragment;
                        ++c;
                        break;
                    }
                    state = State::Path;
                    break;
                }
                if (!parseHost(CodePointIterator<CharacterType>(authorityOrHostBegin, c)))
                    return failure(input, length);
                
                if (bufferView(m_buffer, m_url.m_passwordEnd, m_buffer.length() - m_url.m_passwordEnd) == "localhost")  {
                    m_buffer.resize(m_url.m_passwordEnd);
                    m_url.m_hostEnd = m_buffer.length();
                    m_url.m_portEnd = m_url.m_hostEnd;
                }
                
                state = State::PathStart;
                break;
            }
            if (isPercentOrNonASCII(*c))
                m_hostHasPercentOrNonASCII = true;
            ++c;
            break;
        case State::PathStart:
            LOG_STATE("PathStart");
            if (*c != '/' && *c != '\\')
                ++c;
            state = State::Path;
            break;
        case State::Path:
            LOG_STATE("Path");
            if (*c == '/' || (m_urlIsSpecial && *c == '\\')) {
                m_buffer.append('/');
                m_url.m_pathAfterLastSlash = m_buffer.length();
                ++c;
                break;
            }
            if (m_buffer.length() && m_buffer[m_buffer.length() - 1] == '/') {
                if (isDoubleDotPathSegment(c)) {
                    consumeDoubleDotPathSegment(c);
                    popPath();
                    break;
                }
                if (m_buffer[m_buffer.length() - 1] == '/' && isSingleDotPathSegment(c)) {
                    consumeSingleDotPathSegment(c);
                    break;
                }
            }
            if (*c == '?') {
                m_url.m_pathEnd = m_buffer.length();
                state = State::Query;
                break;
            }
            if (*c == '#') {
                m_url.m_pathEnd = m_buffer.length();
                m_url.m_queryEnd = m_url.m_pathEnd;
                state = State::Fragment;
                break;
            }
            if (isPercentEncodedDot(c)) {
                m_buffer.append('.');
                ASSERT(*c == '%');
                ++c;
                ASSERT(*c == dotASCIICode[0]);
                ++c;
                ASSERT(toASCIILower(*c) == dotASCIICode[1]);
                ++c;
                break;
            }
            utf8PercentEncode(*c, m_buffer, isInDefaultEncodeSet);
            ++c;
            break;
        case State::CannotBeABaseURLPath:
            LOG_STATE("CannotBeABaseURLPath");
            if (*c == '?') {
                m_url.m_pathEnd = m_buffer.length();
                state = State::Query;
            } else if (*c == '#') {
                m_url.m_pathEnd = m_buffer.length();
                m_url.m_queryEnd = m_url.m_pathEnd;
                state = State::Fragment;
            } else {
                utf8PercentEncode(*c, m_buffer, isInSimpleEncodeSet);
                ++c;
            }
            break;
        case State::Query:
            LOG_STATE("Query");
            if (*c == '#') {
                if (!isUTF8Encoding)
                    encodeQuery(queryBuffer, m_buffer, encoding);
                m_url.m_queryEnd = m_buffer.length();
                state = State::Fragment;
                break;
            }
            if (isUTF8Encoding)
                utf8PercentEncodeQuery(*c, m_buffer);
            else
                queryBuffer.append(*c);
            ++c;
            break;
        case State::Fragment:
            LOG_STATE("Fragment");
            m_buffer.append(*c);
            ++c;
            break;
        }
    }

    switch (state) {
    case State::SchemeStart:
        LOG_FINAL_STATE("SchemeStart");
        if (!m_buffer.length() && !base.isNull())
            return base;
        return failure(input, length);
    case State::Scheme:
        LOG_FINAL_STATE("Scheme");
        break;
    case State::NoScheme:
        LOG_FINAL_STATE("NoScheme");
        break;
    case State::SpecialRelativeOrAuthority:
        LOG_FINAL_STATE("SpecialRelativeOrAuthority");
        copyURLPartsUntil(base, URLPart::QueryEnd);
        m_url.m_fragmentEnd = m_url.m_queryEnd;
        break;
    case State::PathOrAuthority:
        LOG_FINAL_STATE("PathOrAuthority");
        break;
    case State::Relative:
        LOG_FINAL_STATE("Relative");
        copyURLPartsUntil(base, URLPart::FragmentEnd);
        break;
    case State::RelativeSlash:
        LOG_FINAL_STATE("RelativeSlash");
        copyURLPartsUntil(base, URLPart::PortEnd);
        m_buffer.append('/');
        m_url.m_pathAfterLastSlash = base.m_portEnd + 1;
        m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
        m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
        m_url.m_fragmentEnd = m_url.m_pathAfterLastSlash;
        break;
    case State::SpecialAuthoritySlashes:
        LOG_FINAL_STATE("SpecialAuthoritySlashes");
        m_url.m_userStart = m_buffer.length();
        m_url.m_userEnd = m_url.m_userStart;
        m_url.m_passwordEnd = m_url.m_userStart;
        m_url.m_hostEnd = m_url.m_userStart;
        m_url.m_portEnd = m_url.m_userStart;
        m_url.m_pathAfterLastSlash = m_url.m_userStart;
        m_url.m_pathEnd = m_url.m_userStart;
        m_url.m_queryEnd = m_url.m_userStart;
        m_url.m_fragmentEnd = m_url.m_userStart;
        break;
    case State::SpecialAuthorityIgnoreSlashes:
        LOG_FINAL_STATE("SpecialAuthorityIgnoreSlashes");
        return failure(input, length);
    case State::AuthorityOrHost:
        LOG_FINAL_STATE("AuthorityOrHost");
        m_url.m_userEnd = m_buffer.length();
        m_url.m_passwordEnd = m_url.m_userEnd;
        FALLTHROUGH;
    case State::Host:
        if (state == State::Host)
            LOG_FINAL_STATE("Host");
        if (!parseHost(authorityOrHostBegin))
            return failure(input, length);
        m_buffer.append('/');
        m_url.m_pathEnd = m_url.m_portEnd + 1;
        m_url.m_pathAfterLastSlash = m_url.m_pathEnd;
        m_url.m_queryEnd = m_url.m_pathEnd;
        m_url.m_fragmentEnd = m_url.m_pathEnd;
        break;
    case State::File:
        LOG_FINAL_STATE("File");
        if (!base.isNull() && base.protocol() == "file") {
            copyURLPartsUntil(base, URLPart::QueryEnd);
            m_buffer.append(':');
        }
        m_buffer.append("///");
        m_url.m_userStart = m_buffer.length() - 1;
        m_url.m_userEnd = m_url.m_userStart;
        m_url.m_passwordEnd = m_url.m_userStart;
        m_url.m_hostEnd = m_url.m_userStart;
        m_url.m_portEnd = m_url.m_userStart;
        m_url.m_pathAfterLastSlash = m_url.m_userStart + 1;
        m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
        m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
        m_url.m_fragmentEnd = m_url.m_pathAfterLastSlash;
        break;
    case State::FileSlash:
        LOG_FINAL_STATE("FileSlash");
        m_buffer.append("//");
        m_url.m_userStart = m_buffer.length() - 1;
        m_url.m_userEnd = m_url.m_userStart;
        m_url.m_passwordEnd = m_url.m_userStart;
        m_url.m_hostEnd = m_url.m_userStart;
        m_url.m_portEnd = m_url.m_userStart;
        m_url.m_pathAfterLastSlash = m_url.m_userStart + 1;
        m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
        m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
        m_url.m_fragmentEnd = m_url.m_pathAfterLastSlash;
        break;
    case State::FileHost:
        LOG_FINAL_STATE("FileHost");
        if (authorityOrHostBegin == c) {
            m_buffer.append('/');
            m_url.m_userStart = m_buffer.length() - 1;
            m_url.m_userEnd = m_url.m_userStart;
            m_url.m_passwordEnd = m_url.m_userStart;
            m_url.m_hostEnd = m_url.m_userStart;
            m_url.m_portEnd = m_url.m_userStart;
            m_url.m_pathAfterLastSlash = m_url.m_userStart + 1;
            m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
            m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
            m_url.m_fragmentEnd = m_url.m_pathAfterLastSlash;
            break;
        }

        if (!parseHost(CodePointIterator<CharacterType>(authorityOrHostBegin, c)))
            return failure(input, length);
        
        if (bufferView(m_buffer, m_url.m_passwordEnd, m_buffer.length() - m_url.m_passwordEnd) == "localhost")  {
            m_buffer.resize(m_url.m_passwordEnd);
            m_url.m_hostEnd = m_buffer.length();
            m_url.m_portEnd = m_url.m_hostEnd;
        }
        m_buffer.append('/');
        m_url.m_pathAfterLastSlash = m_url.m_hostEnd + 1;
        m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
        m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
        m_url.m_fragmentEnd = m_url.m_pathAfterLastSlash;
        break;
    case State::PathStart:
        LOG_FINAL_STATE("PathStart");
        break;
    case State::Path:
        LOG_FINAL_STATE("Path");
        m_url.m_pathEnd = m_buffer.length();
        m_url.m_queryEnd = m_url.m_pathEnd;
        m_url.m_fragmentEnd = m_url.m_pathEnd;
        break;
    case State::CannotBeABaseURLPath:
        LOG_FINAL_STATE("CannotBeABaseURLPath");
        m_url.m_pathEnd = m_buffer.length();
        m_url.m_queryEnd = m_url.m_pathEnd;
        m_url.m_fragmentEnd = m_url.m_pathEnd;
        break;
    case State::Query:
        LOG_FINAL_STATE("Query");
        if (!isUTF8Encoding)
            encodeQuery(queryBuffer, m_buffer, encoding);
        m_url.m_queryEnd = m_buffer.length();
        m_url.m_fragmentEnd = m_url.m_queryEnd;
        break;
    case State::Fragment:
        LOG_FINAL_STATE("Fragment");
        m_url.m_fragmentEnd = m_buffer.length();
        break;
    }

    m_url.m_string = m_buffer.toString();
    m_url.m_isValid = true;
    LOG(URLParser, "Parsed URL <%s>", m_url.m_string.utf8().data());
    return m_url;
}

template<typename CharacterType>
void URLParser::parseAuthority(CodePointIterator<CharacterType> iterator)
{
    if (iterator.atEnd()) {
        m_url.m_userEnd = m_buffer.length();
        m_url.m_passwordEnd = m_url.m_userEnd;
        return;
    }
    for (; !iterator.atEnd(); ++iterator) {
        if (*iterator == ':') {
            ++iterator;
            m_url.m_userEnd = m_buffer.length();
            if (iterator.atEnd()) {
                m_url.m_passwordEnd = m_url.m_userEnd;
                if (m_url.m_userEnd > m_url.m_userStart)
                    m_buffer.append('@');
                return;
            }
            m_buffer.append(':');
            break;
        }
        m_buffer.append(*iterator);
    }
    for (; !iterator.atEnd(); ++iterator)
        m_buffer.append(*iterator);
    m_url.m_passwordEnd = m_buffer.length();
    if (!m_url.m_userEnd)
        m_url.m_userEnd = m_url.m_passwordEnd;
    m_buffer.append('@');
}

static void serializeIPv4(uint32_t address, StringBuilder& buffer)
{
    buffer.appendNumber(address >> 24);
    buffer.append('.');
    buffer.appendNumber((address >> 16) & 0xFF);
    buffer.append('.');
    buffer.appendNumber((address >> 8) & 0xFF);
    buffer.append('.');
    buffer.appendNumber(address & 0xFF);
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
    
static void serializeIPv6Piece(uint16_t piece, StringBuilder& buffer)
{
    bool printed = false;
    if (auto nibble0 = piece >> 12) {
        buffer.append(lowerNibbleToLowercaseASCIIHexDigit(nibble0));
        printed = true;
    }
    auto nibble1 = piece >> 8 & 0xF;
    if (printed || nibble1) {
        buffer.append(lowerNibbleToLowercaseASCIIHexDigit(nibble1));
        printed = true;
    }
    auto nibble2 = piece >> 4 & 0xF;
    if (printed || nibble2)
        buffer.append(lowerNibbleToLowercaseASCIIHexDigit(nibble2));
    buffer.append(lowerNibbleToLowercaseASCIIHexDigit(piece & 0xF));
}

static void serializeIPv6(std::array<uint16_t, 8> address, StringBuilder& buffer)
{
    buffer.append('[');
    auto compressPointer = findLongestZeroSequence(address);
    for (size_t piece = 0; piece < 8; piece++) {
        if (compressPointer && compressPointer.value() == piece) {
            ASSERT(!address[piece]);
            if (piece)
                buffer.append(':');
            else
                buffer.append("::");
            while (piece < 8 && !address[piece])
                piece++;
            if (piece == 8)
                break;
        }
        serializeIPv6Piece(address[piece], buffer);
        if (piece < 7)
            buffer.append(':');
    }
    buffer.append(']');
}

template<typename CharacterType>
static Optional<uint32_t> parseIPv4Number(CodePointIterator<CharacterType>& iterator)
{
    // FIXME: Check for overflow.
    enum class State : uint8_t {
        UnknownBase,
        Decimal,
        OctalOrHex,
        Octal,
        Hex,
    };
    State state = State::UnknownBase;
    uint32_t value = 0;
    while (!iterator.atEnd()) {
        if (*iterator == '.') {
            ++iterator;
            return value;
        }
        switch (state) {
        case State::UnknownBase:
            if (*iterator == '0') {
                ++iterator;
                state = State::OctalOrHex;
                break;
            }
            state = State::Decimal;
            break;
        case State::OctalOrHex:
            if (*iterator == 'x' || *iterator == 'X') {
                ++iterator;
                state = State::Hex;
                break;
            }
            state = State::Octal;
            break;
        case State::Decimal:
            if (*iterator < '0' || *iterator > '9')
                return Nullopt;
            value *= 10;
            value += *iterator - '0';
            ++iterator;
            break;
        case State::Octal:
            if (*iterator < '0' || *iterator > '7')
                return Nullopt;
            value *= 8;
            value += *iterator - '0';
            ++iterator;
            break;
        case State::Hex:
            if (!isASCIIHexDigit(*iterator))
                return Nullopt;
            value *= 16;
            value += toASCIIHexValue(*iterator);
            ++iterator;
            break;
        }
    }
    return value;
}

static uint64_t pow256(size_t exponent)
{
    RELEASE_ASSERT(exponent <= 4);
    uint64_t values[5] = {1, 256, 256 * 256, 256 * 256 * 256, 256ull * 256 * 256 * 256 };
    return values[exponent];
}

template<typename CharacterType>
static Optional<uint32_t> parseIPv4Host(CodePointIterator<CharacterType> iterator)
{
    Vector<uint32_t, 4> items;
    items.reserveInitialCapacity(4);
    while (!iterator.atEnd()) {
        if (items.size() >= 4)
            return Nullopt;
        if (auto item = parseIPv4Number(iterator))
            items.append(item.value());
        else
            return Nullopt;
    }
    if (!items.size() || items.size() > 4)
        return Nullopt;
    for (size_t i = 0; i < items.size() - 2; i++) {
        if (items[i] > 255)
            return Nullopt;
    }
    if (items[items.size() - 1] >= pow256(5 - items.size()))
        return Nullopt;
    for (auto item : items) {
        if (item > 255)
            return Nullopt;
    }
    uint32_t ipv4 = items.takeLast();
    for (size_t counter = 0; counter < items.size(); ++counter)
        ipv4 += items[counter] * pow256(3 - counter);
    return ipv4;
}
    
template<typename CharacterType>
static Optional<std::array<uint16_t, 8>> parseIPv6Host(CodePointIterator<CharacterType> c)
{
    if (c.atEnd())
        return Nullopt;

    std::array<uint16_t, 8> address = {{0, 0, 0, 0, 0, 0, 0, 0}};
    size_t piecePointer = 0;
    Optional<size_t> compressPointer;

    if (*c == ':') {
        ++c;
        if (c.atEnd())
            return Nullopt;
        if (*c != ':')
            return Nullopt;
        ++c;
        ++piecePointer;
        compressPointer = piecePointer;
    }
    
    while (!c.atEnd()) {
        if (piecePointer == 8)
            return Nullopt;
        if (*c == ':') {
            if (compressPointer)
                return Nullopt;
            ++c;
            ++piecePointer;
            compressPointer = piecePointer;
            continue;
        }
        uint16_t value = 0;
        for (size_t length = 0; length < 4; length++) {
            if (c.atEnd())
                break;
            if (!isASCIIHexDigit(*c))
                break;
            value = value * 0x10 + toASCIIHexValue(*c);
            ++c;
        }
        address[piecePointer++] = value;
        if (c.atEnd())
            break;
        if (*c != ':')
            return Nullopt;
        ++c;
    }
    
    if (!c.atEnd()) {
        if (piecePointer > 6)
            return Nullopt;
        size_t dotsSeen = 0;
        while (!c.atEnd()) {
            Optional<uint16_t> value;
            if (!isASCIIDigit(*c))
                return Nullopt;
            while (isASCIIDigit(*c)) {
                auto number = *c - '0';
                if (!value)
                    value = number;
                else if (!value.value())
                    return Nullopt;
                else
                    value = value.value() * 10 + number;
                ++c;
                if (c.atEnd())
                    return Nullopt;
                if (value.value() > 255)
                    return Nullopt;
            }
            if (dotsSeen < 3 && *c != '.')
                return Nullopt;
            address[piecePointer] = address[piecePointer] * 0x100 + value.valueOr(0);
            if (dotsSeen == 1 || dotsSeen == 3)
                piecePointer++;
            if (!c.atEnd())
                ++c;
            if (dotsSeen == 3 && !c.atEnd())
                return Nullopt;
            dotsSeen++;
        }
    }
    if (compressPointer) {
        size_t swaps = piecePointer - compressPointer.value();
        piecePointer = 7;
        while (swaps)
            std::swap(address[piecePointer--], address[compressPointer.value() + swaps-- - 1]);
    } else if (piecePointer != 8)
        return Nullopt;
    return address;
}

// FIXME: This should return a CString.
static String percentDecode(const LChar* input, size_t length)
{
    StringBuilder output;
    
    for (size_t i = 0; i < length; ++i) {
        uint8_t byte = input[i];
        if (byte != '%')
            output.append(byte);
        else if (i < length - 2) {
            if (isASCIIHexDigit(input[i + 1]) && isASCIIHexDigit(input[i + 2])) {
                output.append(toASCIIHexValue(input[i + 1], input[i + 2]));
                i += 2;
            } else
                output.append(byte);
        } else
            output.append(byte);
    }
    return output.toStringPreserveCapacity();
}

static bool containsOnlyASCII(const String& string)
{
    if (string.is8Bit())
        return charactersAreAllASCII(string.characters8(), string.length());
    return charactersAreAllASCII(string.characters16(), string.length());
}

static Optional<String> domainToASCII(const String& domain)
{
    const unsigned hostnameBufferLength = 2048;

    if (containsOnlyASCII(domain)) {
        if (domain.is8Bit())
            return domain.convertToASCIILowercase();
        Vector<LChar, hostnameBufferLength> buffer;
        size_t length = domain.length();
        buffer.reserveInitialCapacity(length);
        for (size_t i = 0; i < length; ++i)
            buffer.append(toASCIILower(domain[i]));
        return String(buffer.data(), length);
    }
    
    UChar hostnameBuffer[hostnameBufferLength];
    UErrorCode error = U_ZERO_ERROR;
    
    int32_t numCharactersConverted = uidna_IDNToASCII(StringView(domain).upconvertedCharacters(), domain.length(), hostnameBuffer, hostnameBufferLength, UIDNA_ALLOW_UNASSIGNED, nullptr, &error);

    if (error == U_ZERO_ERROR) {
        LChar buffer[hostnameBufferLength];
        for (int32_t i = 0; i < numCharactersConverted; ++i) {
            ASSERT(isASCII(hostnameBuffer[i]));
            buffer[i] = hostnameBuffer[i];
        }
        return String(buffer, numCharactersConverted);
    }

    // FIXME: Check for U_BUFFER_OVERFLOW_ERROR and retry with an allocated buffer.
    return Nullopt;
}

static bool hasInvalidDomainCharacter(const String& asciiDomain)
{
    RELEASE_ASSERT(asciiDomain.is8Bit());
    const LChar* characters = asciiDomain.characters8();
    for (size_t i = 0; i < asciiDomain.length(); ++i) {
        if (isInvalidDomainCharacter(characters[i]))
            return true;
    }
    return false;
}

template<typename CharacterType>
bool URLParser::parsePort(CodePointIterator<CharacterType>& iterator)
{
    uint32_t port = 0;
    if (iterator.atEnd()) {
        m_url.m_portEnd = m_buffer.length();
        return true;
    }
    m_buffer.append(':');
    for (; !iterator.atEnd(); ++iterator) {
        if (isTabOrNewline(*iterator))
            continue;
        if (isASCIIDigit(*iterator)) {
            port = port * 10 + *iterator - '0';
            if (port > std::numeric_limits<uint16_t>::max())
                return false;
        } else
            return false;
    }
    
    if (isDefaultPort(bufferView(m_buffer, 0, m_url.m_schemeEnd), port)) {
        ASSERT(m_buffer[m_buffer.length() - 1] == ':');
        m_buffer.resize(m_buffer.length() - 1);
    } else
        m_buffer.appendNumber(port);

    m_url.m_portEnd = m_buffer.length();
    return true;
}

template<typename CharacterType>
bool URLParser::parseHost(CodePointIterator<CharacterType> iterator)
{
    if (iterator.atEnd())
        return false;
    if (*iterator == '[') {
        ++iterator;
        auto ipv6End = iterator;
        while (!ipv6End.atEnd() && *ipv6End != ']')
            ++ipv6End;
        if (auto address = parseIPv6Host(CodePointIterator<CharacterType>(iterator, ipv6End))) {
            serializeIPv6(address.value(), m_buffer);
            m_url.m_hostEnd = m_buffer.length();
            if (!ipv6End.atEnd()) {
                ++ipv6End;
                if (!ipv6End.atEnd() && *ipv6End == ':') {
                    ++ipv6End;
                    return parsePort(ipv6End);
                }
                m_url.m_portEnd = m_buffer.length();
                return true;
            }
            return true;
        }
    }
    
    if (!m_hostHasPercentOrNonASCII) {
        auto hostIterator = iterator;
        for (; !iterator.atEnd(); ++iterator) {
            if (isTabOrNewline(*iterator))
                continue;
            if (*iterator == ':')
                break;
            if (isInvalidDomainCharacter(*iterator))
                return false;
        }
        if (auto address = parseIPv4Host(CodePointIterator<CharacterType>(hostIterator, iterator))) {
            serializeIPv4(address.value(), m_buffer);
            m_url.m_hostEnd = m_buffer.length();
            if (iterator.atEnd()) {
                m_url.m_portEnd = m_buffer.length();
                return true;
            }
            ++iterator;
            return parsePort(iterator);
        }
        for (; hostIterator != iterator; ++hostIterator) {
            if (!isTabOrNewline(*hostIterator))
                m_buffer.append(toASCIILower(*hostIterator));
        }
        m_url.m_hostEnd = m_buffer.length();
        if (!hostIterator.atEnd()) {
            ASSERT(*hostIterator == ':');
            ++hostIterator;
            while (!hostIterator.atEnd() && isTabOrNewline(*hostIterator))
                ++hostIterator;
            return parsePort(hostIterator);
        }
        m_url.m_portEnd = m_buffer.length();
        return true;
    }

    // FIXME: We probably don't need to make so many buffers and String copies.
    StringBuilder utf8Encoded;
    for (; !iterator.atEnd(); ++iterator) {
        if (isTabOrNewline(*iterator))
            continue;
        if (*iterator == ':')
            break;
        uint8_t buffer[U8_MAX_LENGTH];
        int32_t offset = 0;
        UBool error = false;
        U8_APPEND(buffer, offset, U8_MAX_LENGTH, *iterator, error);
        ASSERT_WITH_SECURITY_IMPLICATION(offset <= static_cast<int32_t>(sizeof(buffer)));
        // FIXME: Check error.
        utf8Encoded.append(buffer, offset);
    }
    RELEASE_ASSERT(utf8Encoded.is8Bit());
    String percentDecoded = percentDecode(utf8Encoded.characters8(), utf8Encoded.length());
    RELEASE_ASSERT(percentDecoded.is8Bit());
    String domain = String::fromUTF8(percentDecoded.characters8(), percentDecoded.length());
    auto asciiDomain = domainToASCII(domain);
    if (!asciiDomain || hasInvalidDomainCharacter(asciiDomain.value()))
        return false;
    String& asciiDomainValue = asciiDomain.value();
    RELEASE_ASSERT(asciiDomainValue.is8Bit());
    const LChar* asciiDomainCharacters = asciiDomainValue.characters8();
    
    if (auto address = parseIPv4Host(CodePointIterator<LChar>(asciiDomainCharacters, asciiDomainCharacters + asciiDomainValue.length()))) {
        serializeIPv4(address.value(), m_buffer);
        m_url.m_hostEnd = m_buffer.length();
        if (iterator.atEnd()) {
            m_url.m_portEnd = m_buffer.length();
            return true;
        }
        ++iterator;
        return parsePort(iterator);
    }
    
    m_buffer.append(asciiDomain.value());
    m_url.m_hostEnd = m_buffer.length();
    if (!iterator.atEnd()) {
        ASSERT(*iterator == ':');
        ++iterator;
        while (!iterator.atEnd() && isTabOrNewline(*iterator))
            ++iterator;
        return parsePort(iterator);
    }
    m_url.m_portEnd = m_buffer.length();
    return true;
}

static Optional<String> formURLDecode(StringView input)
{
    auto utf8 = input.utf8(StrictConversion);
    if (utf8.isNull())
        return Nullopt;
    auto percentDecoded = percentDecode(reinterpret_cast<const LChar*>(utf8.data()), utf8.length());
    RELEASE_ASSERT(percentDecoded.is8Bit());
    return String::fromUTF8(percentDecoded.characters8(), percentDecoded.length());
}

auto URLParser::parseURLEncodedForm(StringView input) -> URLEncodedForm
{
    Vector<StringView> sequences = input.split('&');

    URLEncodedForm output;
    for (auto& bytes : sequences) {
        auto valueStart = bytes.find('=');
        if (valueStart == notFound) {
            if (auto name = formURLDecode(bytes))
                output.append({name.value().replace('+', 0x20), emptyString()});
        } else {
            auto name = formURLDecode(bytes.substring(0, valueStart));
            auto value = formURLDecode(bytes.substring(valueStart + 1));
            if (name && value)
                output.append(std::make_pair(name.value().replace('+', 0x20), value.value().replace('+', 0x20)));
        }
    }
    return output;
}

static void serializeURLEncodedForm(const String& input, StringBuilder& output)
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
            || (byte >= 0x61 && byte <= 0x7A))
            output.append(byte);
        else
            percentEncode(byte, output);
    }
}
    
String URLParser::serialize(const URLEncodedForm& tuples)
{
    StringBuilder output;
    for (auto& tuple : tuples) {
        if (!output.isEmpty())
            output.append('&');
        serializeURLEncodedForm(tuple.first, output);
        output.append('=');
        serializeURLEncodedForm(tuple.second, output);
    }
    return output.toString();
}

bool URLParser::allValuesEqual(const URL& a, const URL& b)
{
    // FIXME: m_cannotBeABaseURL is not compared because the old URL::parse did not use it,
    // but once we get rid of URL::parse its value should be tested.
    LOG(URLParser, "%d %d %d %d %d %d %d %d %d %d %d %d %s\n%d %d %d %d %d %d %d %d %d %d %d %d %s",
        a.m_isValid,
        a.m_protocolIsInHTTPFamily,
        a.m_schemeEnd,
        a.m_userStart,
        a.m_userEnd,
        a.m_passwordEnd,
        a.m_hostEnd,
        a.m_portEnd,
        a.m_pathAfterLastSlash,
        a.m_pathEnd,
        a.m_queryEnd,
        a.m_fragmentEnd,
        a.m_string.utf8().data(),
        b.m_isValid,
        b.m_protocolIsInHTTPFamily,
        b.m_schemeEnd,
        b.m_userStart,
        b.m_userEnd,
        b.m_passwordEnd,
        b.m_hostEnd,
        b.m_portEnd,
        b.m_pathAfterLastSlash,
        b.m_pathEnd,
        b.m_queryEnd,
        b.m_fragmentEnd,
        b.m_string.utf8().data());

    return a.m_string == b.m_string
        && a.m_isValid == b.m_isValid
        && a.m_protocolIsInHTTPFamily == b.m_protocolIsInHTTPFamily
        && a.m_schemeEnd == b.m_schemeEnd
        && a.m_userStart == b.m_userStart
        && a.m_userEnd == b.m_userEnd
        && a.m_passwordEnd == b.m_passwordEnd
        && a.m_hostEnd == b.m_hostEnd
        && a.m_portEnd == b.m_portEnd
        && a.m_pathAfterLastSlash == b.m_pathAfterLastSlash
        && a.m_pathEnd == b.m_pathEnd
        && a.m_queryEnd == b.m_queryEnd
        && a.m_fragmentEnd == b.m_fragmentEnd;
}

static bool urlParserEnabled = false;

void URLParser::setEnabled(bool enabled)
{
    urlParserEnabled = enabled;
}

bool URLParser::enabled()
{
    return urlParserEnabled;
}

} // namespace WebCore
