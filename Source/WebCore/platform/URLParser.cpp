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
#include "NotImplemented.h"
#include <array>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

template<typename CharacterType> static bool isC0Control(CharacterType character) { return character <= 0x0001F; }
template<typename CharacterType> static bool isC0ControlOrSpace(CharacterType character) { return isC0Control(character) || character == 0x0020; }
template<typename CharacterType> static bool isTabOrNewline(CharacterType character) { return character == 0x0009 || character == 0x000A || character == 0x000D; }
    
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
    
URL URLParser::parse(const String& input, const URL& base, const TextEncoding&)
{
    m_url = { };
    m_buffer.clear();
    m_authorityOrHostBuffer.clear();

    auto codePoints = StringView(input).codePoints();
    auto c = codePoints.begin();
    auto end = codePoints.end();
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

#define LOG_STATE(x) LOG(URLParser, x)
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
                if (urlScheme == "file")
                    state = State::File;
                else if (isSpecialScheme(urlScheme)) {
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
            if (c == end) {
                m_buffer.clear();
                state = State::NoScheme;
                c = codePoints.begin();
            }
            break;
        case State::SchemeEndCheckForSlashes:
            LOG_STATE("SchemeEndCheckForSlashes");
            if (*c == '/') {
                m_buffer.append('/');
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
            } else if (base.protocol() == "file")
                state = State::File;
            else
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
                state = State::Query;
                ++c;
                break;
            case '#':
                copyURLPartsUntil(base, URLPart::QueryEnd);
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
            break;
        case State::AuthorityOrHost:
            LOG_STATE("AuthorityOrHost");
            if (*c == '@') {
                authorityEndReached();
                state = State::Host;
            } else if (*c == '/' || *c == '?' || *c == '#') {
                m_url.m_userEnd = m_buffer.length();
                m_url.m_passwordEnd = m_url.m_userEnd;
                hostEndReached();
                state = State::Path;
                break;
            } else
                m_authorityOrHostBuffer.append(*c);
            ++c;
            break;
        case State::Host:
            LOG_STATE("Host");
            if (*c == '/' || *c == '?' || *c == '#') {
                hostEndReached();
                state = State::Path;
                break;
            }
            m_authorityOrHostBuffer.append(*c);
            ++c;
            break;
        case State::File:
            LOG_STATE("File");
            notImplemented();
            ++c;
            break;
        case State::FileSlash:
            LOG_STATE("FileSlash");
            notImplemented();
            ++c;
            break;
        case State::FileHost:
            LOG_STATE("FileHost");
            notImplemented();
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
                while (c != end && isTabOrNewline(*c))
                    ++c;
                if (c == end)
                    break;
                if (*c == '.') {
                    ++c;
                    while (c != end && isTabOrNewline(*c))
                        ++c;
                    if (c == end)
                        return { };
                    if (*c == '.')
                        notImplemented();
                    notImplemented();
                }
            } else if (*c == '?') {
                m_url.m_pathEnd = m_buffer.length();
                state = State::Query;
                break;
            } else if (*c == '#') {
                m_url.m_pathEnd = m_buffer.length();
                state = State::Fragment;
                break;
            }
            // FIXME: Percent encode c
            m_buffer.append(*c);
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
                m_url.m_queryEnd = m_buffer.length();
                state = State::Fragment;
                break;
            }
            m_buffer.append(*c);
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
        hostEndReached();
        m_buffer.append('/');
        m_url.m_pathEnd = m_url.m_portEnd + 1;
        m_url.m_pathAfterLastSlash = m_url.m_pathEnd;
        m_url.m_queryEnd = m_url.m_pathEnd;
        m_url.m_fragmentEnd = m_url.m_pathEnd;
        break;
    case State::File:
        LOG_FINAL_STATE("File");
        break;
    case State::FileSlash:
        LOG_FINAL_STATE("FileSlash");
        break;
    case State::FileHost:
        LOG_FINAL_STATE("FileHost");
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
    return m_url;
}

void URLParser::authorityEndReached()
{
    auto codePoints = StringView(m_authorityOrHostBuffer.toString()).codePoints();
    auto iterator = codePoints.begin();
    auto end = codePoints.end();
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
    m_authorityOrHostBuffer.clear();
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

static Optional<std::array<uint16_t, 8>> parseIPv6Host(StringView::CodePoints::Iterator, StringView::CodePoints::Iterator)
{
    notImplemented();
    return Nullopt;
}

void URLParser::hostEndReached()
{
    auto codePoints = StringView(m_authorityOrHostBuffer.toString()).codePoints();
    auto iterator = codePoints.begin();
    auto end = codePoints.end();
    if (iterator == end)
        return;
    if (*iterator == '[') {
        ++iterator;
        parseIPv6Host(iterator, end);
        return;
    }
    if (auto address = parseIPv4Host(iterator, end)) {
        serializeIPv4(address.value(), m_buffer);
        m_url.m_hostEnd = m_buffer.length();
        // FIXME: Handle the port correctly.
        m_url.m_portEnd = m_buffer.length();
        return;
    }
    for (; iterator != end; ++iterator) {
        if (*iterator == ':') {
            ++iterator;
            m_url.m_hostEnd = m_buffer.length();
            m_buffer.append(':');
            for (; iterator != end; ++iterator)
                m_buffer.append(*iterator);
            m_url.m_portEnd = m_buffer.length();
            return;
        }
        m_buffer.append(*iterator);
    }
    m_url.m_hostEnd = m_buffer.length();
    m_url.m_portEnd = m_url.m_hostEnd;
    m_authorityOrHostBuffer.clear();
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

} // namespace WebCore
