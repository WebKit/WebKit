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
#include "NotImplemented.h"

#include <wtf/text/StringBuilder.h>

namespace WebCore {

static bool isC0Control(const StringView::CodePoints::Iterator& c) { return *c <= 0x001F; }
static bool isC0ControlOrSpace(const StringView::CodePoints::Iterator& c) { return isC0Control(c) || *c == 0x0020; }
static bool isTabOrNewline(const StringView::CodePoints::Iterator& c) { return *c == 0x0009 || *c == 0x000A || *c == 0x000D; }
static bool isASCIIDigit(const StringView::CodePoints::Iterator& c) { return *c >= 0x0030  && *c <= 0x0039; }
static bool isASCIIAlpha(const StringView::CodePoints::Iterator& c) { return (*c >= 0x0041 && *c <= 0x005A) || (*c >= 0x0061 && *c <= 0x007A); }
static bool isASCIIAlphanumeric(const StringView::CodePoints::Iterator& c) { return isASCIIDigit(c) || isASCIIAlpha(c); }
    
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
    URL url;
    
    auto codePoints = StringView(input).codePoints();
    auto c = codePoints.begin();
    auto end = codePoints.end();
    StringBuilder buffer;
    while (isC0ControlOrSpace(c))
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
        Authority,
        Host,
        Hostname,
        Port,
        File,
        FileSlash,
        FileHost,
        PathStart,
        Path,
        CannotBeABaseURLPath,
        Query,
        Fragment,
    };

#define LOG_STATE(x)

    State state = State::SchemeStart;
    while (c != end) {
        if (isTabOrNewline(c)) {
            ++c;
            continue;
        }

        switch (state) {
        case State::SchemeStart:
            LOG_STATE("SchemeStart");
            if (isASCIIAlpha(c)) {
                buffer.append(toASCIILower(*c));
                state = State::Scheme;
            } else
                state = State::NoScheme;
            ++c;
            break;
        case State::Scheme:
            LOG_STATE("Scheme");
            if (isASCIIAlphanumeric(c) || *c == '+' || *c == '-' || *c == '.')
                buffer.append(toASCIILower(*c));
            else if (*c == ':') {
                url.m_schemeEnd = buffer.length();
                String urlScheme = buffer.toString(); // FIXME: Find a way to do this without shrinking the buffer.
                url.m_protocolIsInHTTPFamily = urlScheme == "http" || urlScheme == "https";
                if (urlScheme == "file")
                    state = State::File;
                else if (isSpecialScheme(urlScheme)) {
                    if (base.protocol() == urlScheme)
                        state = State::SpecialRelativeOrAuthority;
                    else
                        state = State::SpecialAuthoritySlashes;
                } else
                    state = State::SchemeEndCheckForSlashes;
                buffer.append(':');
            } else {
                buffer.clear();
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
                buffer.append('/');
                if (*c == '/') {
                    buffer.append('/');
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
            if (*c != '/' && *c != '\\') {
                state = State::Authority;
                break;
            }
            notImplemented();
            ++c;
            break;
        case State::Authority:
            LOG_STATE("Authority");
            if (!url.m_userStart)
                url.m_userStart = buffer.length();
            if (*c == '@') {
                url.m_passwordEnd = buffer.length();
                buffer.append('@');
                state = State::Host;
                notImplemented();
            } else if (*c == ':') {
                url.m_userEnd = buffer.length();
                buffer.append(*c);
            } else {
                if (*c == '/' || *c == '?' || *c == '#') {
                    url.m_passwordEnd = buffer.length();
                    state = State::Host;
                }
                buffer.append(*c);
            }
            ++c;
            break;
        case State::Host:
        case State::Hostname:
            LOG_STATE("Host/Hostname");
            if (*c == ':') {
                url.m_hostEnd = buffer.length();
                buffer.append(':');
                state = State::Port;
            } else if (*c == '/' || *c == '?' || *c == '#') {
                url.m_hostEnd = buffer.length();
                state = State::Path;
                continue;
            } else
                buffer.append(*c);
            ++c;
            break;
        case State::Port:
            LOG_STATE("Port");
            if (isASCIIDigit(c)) {
                buffer.append(*c);
            } else if (*c == '/' || *c == '?' || *c == '#') {
                url.m_portEnd = buffer.length();
                state = State::PathStart;
                continue;
            } else
                return Nullopt;
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
                buffer.append('/');
                url.m_pathAfterLastSlash = buffer.length();
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
                url.m_pathEnd = buffer.length();
                state = State::Query;
                continue;
            } else if (*c == '#') {
                url.m_pathEnd = buffer.length();
                state = State::Fragment;
                continue;
            }
            // FIXME: Percent encode c
            buffer.append(*c);
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
                url.m_queryEnd = buffer.length();
                state = State::Fragment;
                continue;
            }
            buffer.append(*c);
            ++c;
            break;
        case State::Fragment:
            LOG_STATE("Fragment");
            buffer.append(*c);
            ++c;
            break;
        }
    }
    
    switch (state) {
    case State::SchemeStart:
    case State::Scheme:
    case State::SchemeEndCheckForSlashes:
    case State::NoScheme:
    case State::SpecialRelativeOrAuthority:
    case State::PathOrAuthority:
    case State::Relative:
    case State::RelativeSlash:
    case State::SpecialAuthoritySlashes:
    case State::SpecialAuthorityIgnoreSlashes:
    case State::Authority:
        break;
    case State::Host:
    case State::Hostname:
        url.m_hostEnd = buffer.length();
        url.m_portEnd = url.m_hostEnd;
        buffer.append('/');
        url.m_pathEnd = url.m_hostEnd + 1;
        url.m_pathAfterLastSlash = url.m_pathEnd;
        url.m_queryEnd = url.m_pathEnd;
        url.m_fragmentEnd = url.m_pathEnd;
        break;
    case State::Port:
        url.m_portEnd = buffer.length();
        buffer.append('/');
        url.m_pathEnd = url.m_portEnd + 1;
        url.m_pathAfterLastSlash = url.m_pathEnd;
        url.m_queryEnd = url.m_pathEnd;
        url.m_fragmentEnd = url.m_pathEnd;
        break;
    case State::File:
    case State::FileSlash:
    case State::FileHost:
    case State::PathStart:
    case State::Path:
        url.m_pathEnd = buffer.length();
        url.m_queryEnd = url.m_pathEnd;
        url.m_fragmentEnd = url.m_pathEnd;
        break;
    case State::CannotBeABaseURLPath:
        break;
    case State::Query:
        url.m_queryEnd = buffer.length();
        url.m_fragmentEnd = url.m_queryEnd;
        break;
    case State::Fragment:
        url.m_fragmentEnd = buffer.length();
        break;
    }

    url.m_string = buffer.toString();
    url.m_isValid = true;
    return url;
}

bool URLParser::allValuesEqual(const URL& a, const URL& b)
{
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
