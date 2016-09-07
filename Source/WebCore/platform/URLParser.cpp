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
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

template<typename CharacterType> static bool isC0Control(CharacterType character) { return character <= 0x0001F; }
template<typename CharacterType> static bool isC0ControlOrSpace(CharacterType character) { return isC0Control(character) || character == 0x0020; }
template<typename CharacterType> static bool isTabOrNewline(CharacterType character) { return character == 0x0009 || character == 0x000A || character == 0x000D; }
template<typename CharacterType> static bool isInSimpleEncodeSet(CharacterType character) { return isC0Control(character) || character > 0x007E; }
template<typename CharacterType> static bool isInDefaultEncodeSet(CharacterType character) { return isInSimpleEncodeSet(character) || character == 0x0020 || character == '"' || character == '#' || character == '<' || character == '>' || character == '?' || character == '`' || character == '{' || character == '}'; }
template<typename CharacterType> static bool isInUserInfoEncodeSet(CharacterType character) { return isInDefaultEncodeSet(character) || character == '/' || character == ':' || character == ';' || character == '=' || character == '@' || character == '[' || character == '\\' || character == ']' || character == '^' || character == '|'; }
template<typename CharacterType> static bool isInvalidDomainCharacter(CharacterType character) { return character == 0x0000 || character == 0x0009 || character == 0x000A || character == 0x000D || character == 0x0020 || character == '#' || character == '%' || character == '/' || character == ':' || character == '?' || character == '@' || character == '[' || character == '\\' || character == ']'; }
    
static bool isWindowsDriveLetter(StringView::CodePoints::Iterator iterator, const StringView::CodePoints::Iterator& end)
{
    if (iterator == end || !isASCIIAlpha(*iterator))
        return false;
    ++iterator;
    if (iterator == end)
        return false;
    return *iterator == ':' || *iterator == '|';
}

static bool isWindowsDriveLetter(const StringBuilder& builder, size_t index)
{
    if (builder.length() < index + 2)
        return false;
    return isASCIIAlpha(builder[index]) && (builder[index + 1] == ':' || builder[index + 1] == '|');
}

static bool shouldCopyFileURL(StringView::CodePoints::Iterator iterator, const StringView::CodePoints::Iterator end)
{
    if (isWindowsDriveLetter(iterator, end))
        return true;
    if (iterator == end)
        return false;
    ++iterator;
    if (iterator == end)
        return true;
    ++iterator;
    if (iterator == end)
        return true;
    return *iterator != '/' && *iterator != '\\' && *iterator != '?' && *iterator != '#';
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

static bool shouldPercentEncodeQueryByte(uint8_t byte)
{
    if (byte < 0x21)
        return true;
    if (byte > 0x7E)
        return true;
    if (byte == 0x22)
        return true;
    if (byte == 0x23)
        return true;
    if (byte == 0x3C)
        return true;
    return byte == 0x3E;
}

static void encodeQuery(const StringBuilder& source, StringBuilder& destination, const TextEncoding& encoding)
{
    // FIXME: It is unclear in the spec what to do when encoding fails. The behavior should be specified and tested.
    CString encoded = encoding.encode(StringView(source.toStringPreserveCapacity()), URLEncodedEntitiesForUnencodables);
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

static bool isDefaultPort(const String& scheme, uint16_t port)
{
    static NeverDestroyed<HashMap<String, uint16_t>> defaultPorts(HashMap<String, uint16_t>({
        {"ftp", 21},
        {"gopher", 70},
        {"http", 80},
        {"https", 443},
        {"ws", 80},
        {"wss", 443}}));
    return defaultPorts.get().get(scheme) == port;
}

static bool isSpecialScheme(const String& scheme)
{
    return scheme == "ftp"
        || scheme == "file"
        || scheme == "gopher"
        || scheme == "http"
        || scheme == "https"
        || scheme == "ws"
        || scheme == "wss";
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
}

static const char* dotASCIICode = "2e";

static bool isPercentEncodedDot(StringView::CodePoints::Iterator c, const StringView::CodePoints::Iterator& end)
{
    if (c == end)
        return false;
    if (*c != '%')
        return false;
    ++c;
    if (c == end)
        return false;
    if (*c != dotASCIICode[0])
        return false;
    ++c;
    if (c == end)
        return false;
    return toASCIILower(*c) == dotASCIICode[1];
}

static bool isSingleDotPathSegment(StringView::CodePoints::Iterator c, const StringView::CodePoints::Iterator& end)
{
    if (c == end)
        return false;
    if (*c == '.') {
        ++c;
        return c == end || *c == '/' || *c == '\\' || *c == '?' || *c == '#';
    }
    if (*c != '%')
        return false;
    ++c;
    if (c == end || *c != dotASCIICode[0])
        return false;
    ++c;
    if (c == end)
        return false;
    if (toASCIILower(*c) == dotASCIICode[1]) {
        ++c;
        return c == end || *c == '/' || *c == '\\' || *c == '?' || *c == '#';
    }
    return false;
}
    
static bool isDoubleDotPathSegment(StringView::CodePoints::Iterator c, const StringView::CodePoints::Iterator& end)
{
    if (c == end)
        return false;
    if (*c == '.') {
        ++c;
        return isSingleDotPathSegment(c, end);
    }
    if (*c != '%')
        return false;
    ++c;
    if (c == end || *c != dotASCIICode[0])
        return false;
    ++c;
    if (c == end)
        return false;
    if (toASCIILower(*c) == dotASCIICode[1]) {
        ++c;
        return isSingleDotPathSegment(c, end);
    }
    return false;
}

static void consumeSingleDotPathSegment(StringView::CodePoints::Iterator& c, const StringView::CodePoints::Iterator end)
{
    ASSERT(isSingleDotPathSegment(c, end));
    if (*c == '.') {
        ++c;
        if (c != end) {
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
        if (c != end) {
            if (*c == '/' || *c == '\\')
                ++c;
            else
                ASSERT(*c == '?' || *c == '#');
        }
    }
}

static void consumeDoubleDotPathSegment(StringView::CodePoints::Iterator& c, const StringView::CodePoints::Iterator end)
{
    ASSERT(isDoubleDotPathSegment(c, end));
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
    consumeSingleDotPathSegment(c, end);
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

URL URLParser::parse(const String& input, const URL& base, const TextEncoding& encoding)
{
    LOG(URLParser, "Parsing URL <%s> base <%s>", input.utf8().data(), base.string().utf8().data());
    m_url = { };
    m_buffer.clear();
    m_buffer.reserveCapacity(input.length());
    
    // FIXME: We shouldn't need to allocate another buffer for this.
    StringBuilder queryBuffer;

    auto codePoints = StringView(input).codePoints();
    auto c = codePoints.begin();
    auto end = codePoints.end();
    auto authorityOrHostBegin = codePoints.begin();
    while (c != end && isC0ControlOrSpace(*c))
        ++c;
    
    enum class State : uint8_t {
        SchemeStart,
        Scheme,
        SchemeEndCheckForSlashes,
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
    while (c != end) {
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
                String urlScheme = m_buffer.toString(); // FIXME: Find a way to do this without shrinking the m_buffer.
                m_url.m_protocolIsInHTTPFamily = urlScheme == "http" || urlScheme == "https";
                if (urlScheme == "file") {
                    state = State::File;
                    m_buffer.append(':');
                    ++c;
                    break;
                }
                if (isSpecialScheme(urlScheme)) {
                    if (base.protocol() == urlScheme)
                        state = State::SpecialRelativeOrAuthority;
                    else
                        state = State::SpecialAuthoritySlashes;
                } else
                    state = State::SchemeEndCheckForSlashes;
                m_buffer.append(':');
            } else {
                m_buffer.clear();
                state = State::NoScheme;
                c = codePoints.begin();
                break;
            }
            ++c;
            while (c != end && isTabOrNewline(*c))
                ++c;
            if (c == end) {
                m_buffer.clear();
                state = State::NoScheme;
                c = codePoints.begin();
            }
            break;
        case State::SchemeEndCheckForSlashes:
            LOG_STATE("SchemeEndCheckForSlashes");
            if (*c == '/') {
                m_buffer.append("//");
                m_url.m_userStart = m_buffer.length();
                state = State::PathOrAuthority;
                ++c;
            } else {
                m_url.m_userStart = m_buffer.length();
                m_url.m_userEnd = m_url.m_userStart;
                m_url.m_passwordEnd = m_url.m_userStart;
                m_url.m_hostEnd = m_url.m_userStart;
                m_url.m_portEnd = m_url.m_userStart;
                m_url.m_pathAfterLastSlash = m_url.m_userStart,
                state = State::CannotBeABaseURLPath;
            }
            break;
        case State::NoScheme:
            LOG_STATE("NoScheme");
            if (base.isNull()) {
                if (*c == '#') {
                    copyURLPartsUntil(base, URLPart::QueryEnd);
                    state = State::Fragment;
                    ++c;
                } else
                    return { };
            } else if (base.protocol() == "file") {
                copyURLPartsUntil(base, URLPart::SchemeEnd);
                m_buffer.append(':');
                state = State::File;
            } else
                state = State::Relative;
            break;
        case State::SpecialRelativeOrAuthority:
            LOG_STATE("SpecialRelativeOrAuthority");
            if (*c == '/') {
                m_buffer.append('/');
                ++c;
                while (c != end && isTabOrNewline(*c))
                    ++c;
                if (c == end)
                    return { };
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
            if (*c == '/') {
                ++c;
                while (c != end && isTabOrNewline(*c))
                    ++c;
                if (c == end)
                    return { };
                m_buffer.append('/');
                if (*c == '/') {
                    m_buffer.append('/');
                    ++c;
                }
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
            if (*c == '@') {
                parseAuthority(authorityOrHostBegin, c);
                ++c;
                while (c != end && isTabOrNewline(*c))
                    ++c;
                authorityOrHostBegin = c;
                state = State::Host;
                break;
            } else if (*c == '/' || *c == '?' || *c == '#') {
                m_url.m_userEnd = m_buffer.length();
                m_url.m_passwordEnd = m_url.m_userEnd;
                if (!parseHost(authorityOrHostBegin, c))
                    return { };
                if (*c != '/') {
                    m_buffer.append('/');
                    m_url.m_pathAfterLastSlash = m_buffer.length();
                }
                state = State::Path;
                break;
            }
            ++c;
            break;
        case State::Host:
            LOG_STATE("Host");
            if (*c == '/' || *c == '?' || *c == '#') {
                if (!parseHost(authorityOrHostBegin, c))
                    return { };
                state = State::Path;
                break;
            }
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
                if (!base.isNull() && base.protocolIs("file") && shouldCopyFileURL(c, end))
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
            if (!base.isNull() && base.protocol() == "file") {
                String basePath = base.path();
                auto basePathCodePoints = StringView(basePath).codePoints();
                if (basePath.length() >= 2 && isWindowsDriveLetter(basePathCodePoints.begin(), basePathCodePoints.end())) {
                    m_buffer.append(basePath[0]);
                    m_buffer.append(basePath[1]);
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
            if (*c == '/' || *c == '\\' || *c == '?' || *c == '#') {
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
                if (!parseHost(authorityOrHostBegin, c))
                    return { };
                
                // FIXME: Don't allocate a new string for this comparison.
                if (m_buffer.toString().substring(m_url.m_passwordEnd) == "localhost")  {
                    m_buffer.resize(m_url.m_passwordEnd);
                    m_url.m_hostEnd = m_buffer.length();
                    m_url.m_portEnd = m_url.m_hostEnd;
                }
                
                state = State::PathStart;
                break;
            }
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
            if (*c == '/') {
                m_buffer.append('/');
                m_url.m_pathAfterLastSlash = m_buffer.length();
                ++c;
                break;
            }
            if (m_buffer.length() && m_buffer[m_buffer.length() - 1] == '/') {
                if (isDoubleDotPathSegment(c, end)) {
                    consumeDoubleDotPathSegment(c, end);
                    popPath();
                    break;
                }
                if (m_buffer[m_buffer.length() - 1] == '/' && isSingleDotPathSegment(c, end)) {
                    consumeSingleDotPathSegment(c, end);
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
            if (isPercentEncodedDot(c, end)) {
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
                m_buffer.append(*c);
                ++c;
            }
            break;
        case State::Query:
            LOG_STATE("Query");
            if (*c == '#') {
                encodeQuery(queryBuffer, m_buffer, encoding);
                m_url.m_queryEnd = m_buffer.length();
                state = State::Fragment;
                break;
            }
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
        return { };
    case State::Scheme:
        LOG_FINAL_STATE("Scheme");
        break;
    case State::SchemeEndCheckForSlashes:
        LOG_FINAL_STATE("SchemeEndCheckForSlashes");
        break;
    case State::NoScheme:
        LOG_FINAL_STATE("NoScheme");
        break;
    case State::SpecialRelativeOrAuthority:
        LOG_FINAL_STATE("SpecialRelativeOrAuthority");
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
        break;
    case State::SpecialAuthoritySlashes:
        LOG_FINAL_STATE("SpecialAuthoritySlashes");
        break;
    case State::SpecialAuthorityIgnoreSlashes:
        LOG_FINAL_STATE("SpecialAuthorityIgnoreSlashes");
        break;
    case State::AuthorityOrHost:
        LOG_FINAL_STATE("AuthorityOrHost");
        m_url.m_userEnd = m_buffer.length();
        m_url.m_passwordEnd = m_url.m_userEnd;
        FALLTHROUGH;
    case State::Host:
        if (state == State::Host)
            LOG_FINAL_STATE("Host");
        if (!parseHost(authorityOrHostBegin, end))
            return { };
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

        m_url.m_pathAfterLastSlash = m_url.m_userStart + 1;
        m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
        m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
        m_url.m_fragmentEnd = m_url.m_pathAfterLastSlash;
        if (!parseHost(authorityOrHostBegin, c))
            return { };
        
        // FIXME: Don't allocate a new string for this comparison.
        if (m_buffer.toString().substring(m_url.m_passwordEnd) == "localhost")  {
            m_buffer.resize(m_url.m_passwordEnd);
            m_url.m_hostEnd = m_buffer.length();
            m_url.m_portEnd = m_url.m_hostEnd;
            m_buffer.append('/');
            m_url.m_pathAfterLastSlash = m_url.m_hostEnd + 1;
            m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
            m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
            m_url.m_fragmentEnd = m_url.m_pathAfterLastSlash;
        }
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

void URLParser::parseAuthority(StringView::CodePoints::Iterator& iterator, const StringView::CodePoints::Iterator& end)
{
    for (; iterator != end; ++iterator) {
        m_buffer.append(*iterator);
        if (*iterator == ':') {
            ++iterator;
            m_url.m_userEnd = m_buffer.length() - 1;
            break;
        }
    }
    for (; iterator != end; ++iterator)
        m_buffer.append(*iterator);
    m_url.m_passwordEnd = m_buffer.length();
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
            while (!address[piece])
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

static Optional<uint32_t> parseIPv4Number(StringView::CodePoints::Iterator& iterator, const StringView::CodePoints::Iterator& end)
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
    while (iterator != end) {
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

static Optional<uint32_t> parseIPv4Host(StringView::CodePoints::Iterator iterator, const StringView::CodePoints::Iterator& end)
{
    Vector<uint32_t, 4> items;
    items.reserveInitialCapacity(4);
    while (iterator != end) {
        if (items.size() >= 4)
            return Nullopt;
        if (auto item = parseIPv4Number(iterator, end))
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

static Optional<std::array<uint16_t, 8>> parseIPv6Host(StringView::CodePoints::Iterator c, StringView::CodePoints::Iterator end)
{
    if (c == end)
        return Nullopt;

    std::array<uint16_t, 8> address = {{0, 0, 0, 0, 0, 0, 0, 0}};
    size_t piecePointer = 0;
    Optional<size_t> compressPointer;

    if (*c == ':') {
        ++c;
        if (c == end)
            return Nullopt;
        if (*c != ':')
            return Nullopt;
        ++c;
        ++piecePointer;
        compressPointer = piecePointer;
    }
    
    while (c != end) {
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
            if (c == end)
                break;
            if (!isASCIIHexDigit(*c))
                break;
            value = value * 0x10 + toASCIIHexValue(*c);
            ++c;
        }
        address[piecePointer++] = value;
        if (c == end)
            break;
        if (*c != ':')
            return Nullopt;
        ++c;
    }
    
    if (c != end) {
        if (piecePointer > 6)
            return Nullopt;
        size_t dotsSeen = 0;
        while (c != end) {
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
                if (c == end)
                    return Nullopt;
                if (value.value() > 255)
                    return Nullopt;
            }
            if (dotsSeen < 3 && *c != '.')
                return Nullopt;
            address[piecePointer] = address[piecePointer] * 0x100 + value.valueOr(0);
            if (dotsSeen == 1 || dotsSeen == 3)
                piecePointer++;
            if (c != end)
                ++c;
            if (dotsSeen == 3 && c != end)
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

static String percentDecode(const String& input)
{
    StringBuilder output;
    RELEASE_ASSERT(input.is8Bit());
    const LChar* inputBytes = input.characters8();
    size_t length = input.length();
    
    for (size_t i = 0; i < length; ++i) {
        uint8_t byte = inputBytes[i];
        if (byte != '%')
            output.append(byte);
        else if (i < length - 2) {
            if (isASCIIHexDigit(inputBytes[i + 1]) && isASCIIHexDigit(inputBytes[i + 2])) {
                output.append(toASCIIHexValue(inputBytes[i + 1], inputBytes[i + 2]));
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
            return domain;
        Vector<LChar, hostnameBufferLength> buffer;
        size_t length = domain.length();
        buffer.reserveInitialCapacity(length);
        for (size_t i = 0; i < length; ++i)
            buffer.append(domain[i]);
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

bool URLParser::parsePort(StringView::CodePoints::Iterator& iterator, const StringView::CodePoints::Iterator& end)
{
    uint32_t port = 0;
    ASSERT(iterator != end);
    for (; iterator != end; ++iterator) {
        if (isTabOrNewline(*iterator))
            continue;
        if (isASCIIDigit(*iterator)) {
            port = port * 10 + *iterator - '0';
            if (port > std::numeric_limits<uint16_t>::max())
                return false;
        } else
            return false;
    }
    
    // FIXME: This shouldn't need a String allocation.
    String scheme = m_buffer.toStringPreserveCapacity().substring(0, m_url.m_schemeEnd);
    if (isDefaultPort(scheme, port)) {
        ASSERT(m_buffer[m_buffer.length() - 1] == ':');
        m_buffer.resize(m_buffer.length() - 1);
    } else
        m_buffer.appendNumber(port);

    return true;
}

bool URLParser::parseHost(StringView::CodePoints::Iterator& iterator, const StringView::CodePoints::Iterator& end)
{
    if (iterator == end)
        return false;
    if (*iterator == '[') {
        ++iterator;
        auto ipv6End = iterator;
        while (ipv6End != end && *ipv6End != ']')
            ++ipv6End;
        if (auto address = parseIPv6Host(iterator, ipv6End)) {
            serializeIPv6(address.value(), m_buffer);
            m_url.m_hostEnd = m_buffer.length();
            // FIXME: Handle the port correctly.
            m_url.m_portEnd = m_buffer.length();            
            return true;
        }
    }

    // FIXME: We probably don't need to make so many buffers and String copies.
    StringBuilder utf8Encoded;
    for (; iterator != end; ++iterator) {
        if (isTabOrNewline(*iterator))
            continue;
        if (*iterator == ':')
            break;
        uint8_t buffer[U8_MAX_LENGTH];
        int32_t offset = 0;
        UBool error = false;
        U8_APPEND(buffer, offset, U8_MAX_LENGTH, *iterator, error);
        // FIXME: Check error.
        utf8Encoded.append(buffer, offset);
    }
    String percentDecoded = percentDecode(utf8Encoded.toStringPreserveCapacity());
    RELEASE_ASSERT(percentDecoded.is8Bit());
    String domain = String::fromUTF8(percentDecoded.characters8(), percentDecoded.length());
    auto asciiDomain = domainToASCII(domain);
    if (!asciiDomain || hasInvalidDomainCharacter(asciiDomain.value()))
        return false;
    
    auto asciiDomainCodePoints = StringView(asciiDomain.value()).codePoints();
    if (auto address = parseIPv4Host(asciiDomainCodePoints.begin(), asciiDomainCodePoints.end())) {
        serializeIPv4(address.value(), m_buffer);
        m_url.m_hostEnd = m_buffer.length();
        // FIXME: Handle the port correctly.
        m_url.m_portEnd = m_buffer.length();
        return true;
    }
    
    m_buffer.append(asciiDomain.value());
    m_url.m_hostEnd = m_buffer.length();
    if (iterator != end) {
        ASSERT(*iterator == ':');
        ++iterator;
        while (iterator != end && isTabOrNewline(*iterator))
            ++iterator;
        if (iterator != end) {
            m_buffer.append(':');
            if (!parsePort(iterator, end))
                return false;
        }
    }
    m_url.m_portEnd = m_buffer.length();
    return true;
}

bool URLParser::allValuesEqual(const URL& a, const URL& b)
{
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
