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

Optional<URL> URLParser::parse(const String& input, const URL& base, const TextEncoding&)
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
                state = State::Scheme;
            } else
                state = State::NoScheme;
            ++c;
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
                // FIXME: Find a way to start over here.
                notImplemented();
                continue;
            }
            ++c;
            break;
        case State::SchemeEndCheckForSlashes:
            LOG_STATE("SchemeEndCheckForSlashes");
            if (*c == '/') {
                state = State::PathOrAuthority;
                ++c;
            } else
                state = State::CannotBeABaseURLPath;
            break;
        case State::NoScheme:
            LOG_STATE("NoScheme");
            notImplemented();
            ++c;
            break;
        case State::SpecialRelativeOrAuthority:
            LOG_STATE("SpecialRelativeOrAuthority");
            if (*c == '/') {
                ++c;
                if (c == end)
                    return Nullopt;
                if (*c == '/') {
                    state = State::SpecialAuthorityIgnoreSlashes;
                    ++c;
                } else
                    notImplemented();
            } else
                state = State::Relative;
            break;
        case State::PathOrAuthority:
            LOG_STATE("PathOrAuthority");
            notImplemented();
            ++c;
            break;
        case State::Relative:
            LOG_STATE("Relative");
            notImplemented();
            ++c;
            break;
        case State::RelativeSlash:
            LOG_STATE("RelativeSlash");
            notImplemented();
            ++c;
            break;
        case State::SpecialAuthoritySlashes:
            LOG_STATE("SpecialAuthoritySlashes");
            if (*c == '/') {
                ++c;
                if (c == end)
                    return Nullopt;
                m_buffer.append('/');
                if (*c == '/') {
                    m_buffer.append('/');
                    state = State::SpecialAuthorityIgnoreSlashes;
                    ++c;
                    break;
                }
                notImplemented();
            } else
                notImplemented();
            ++c;
            break;
        case State::SpecialAuthorityIgnoreSlashes:
            LOG_STATE("SpecialAuthorityIgnoreSlashes");
            if (*c == '/' || *c == '\\')
                ++c;
            m_url.m_userStart = m_buffer.length();
            state = State::AuthorityOrHost;
            break;
        case State::AuthorityOrHost:
            LOG_STATE("AuthorityOrHost");
            if (*c == '@') {
                authorityEndReached();
                state = State::Host;
            } else if (*c == '/' || *c == '?' || *c == '#') {
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
                continue;
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
            state = State::Path;
            continue;
        case State::Path:
            LOG_STATE("Path");
            if (*c == '/') {
                m_buffer.append('/');
                m_url.m_pathAfterLastSlash = m_buffer.length();
                ++c;
                if (c == end)
                    break;
                if (*c == '.') {
                    ++c;
                    if (c == end)
                        return Nullopt;
                    if (*c == '.')
                        notImplemented();
                    notImplemented();
                }
            } else if (*c == '?') {
                m_url.m_pathEnd = m_buffer.length();
                state = State::Query;
                continue;
            } else if (*c == '#') {
                m_url.m_pathEnd = m_buffer.length();
                state = State::Fragment;
                continue;
            }
            // FIXME: Percent encode c
            m_buffer.append(*c);
            ++c;
            break;
        case State::CannotBeABaseURLPath:
            LOG_STATE("CannotBeABaseURLPath");
            notImplemented();
            ++c;
            break;
        case State::Query:
            LOG_STATE("Query");
            if (*c == '#') {
                m_url.m_queryEnd = m_buffer.length();
                state = State::Fragment;
                continue;
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
        return Nullopt;
        break;
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

void URLParser::hostEndReached()
{
    auto codePoints = StringView(m_authorityOrHostBuffer.toString()).codePoints();
    auto iterator = codePoints.begin();
    auto end = codePoints.end();
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
