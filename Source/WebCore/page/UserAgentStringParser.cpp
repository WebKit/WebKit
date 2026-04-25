/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "UserAgentStringParser.h"

#include "RFC7230.h"
#include "UserAgentStringData.h"
#include <optional>
#include <wtf/ASCIICType.h>
#include <wtf/StdLibExtras.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringCommon.h>
#include <wtf/text/StringImpl.h>
#include <wtf/text/WTFString.h>

/*
 * GRAMMAR:
 * https://www.rfc-editor.org/rfc/rfc9110#name-user-agent
 * User-Agent      = product *( RWS ( product / comment ) )
 * product         = token ["/" product-version]
 * product-version = token
 * token           = 1*tchar
 * tchar           = "!" / "#" / "$" / "%" / "&" / "'" / "*" / "+" / "-" / "." / "^" / "_" / "`" / "|" / "~" / DIGIT / ALPHA ; any VCHAR, except delimiters
 * RWS             = 1*( SP / HTAB ); required whitespace
 * comment         = "(" *( ctext / quoted-pair / comment ) ")"
 * ctext           = HTAB / SP / %x21-27 / %x2A-5B / %x5D-7E / obs-text
 * quoted-pair     = "\" ( HTAB / SP / VCHAR / obs-text )
 * obs-text        = %x80-FF
 * HTAB            = <ASCII horizontal tab %x09, aka '\t'>
 * SP              = <ASCII space, i.e. " ">
 * VCHAR           = <any visible US-ASCII character>
 *
 * REFERENCE:
 * https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Headers/User-Agent#syntax
 *
 * NOTE:
 * User agent strings come in many different forms, but most browsers conform to a common pattern.
 * This class is attempting to determine attributes about the user agent by expecting common forms
 * of user agent strings. There is a link below that contains a list of of these strings grouped
 * by platform, browser, layout engine, etc. I tried to pick the most frequent ones to parse out
 * viable information.
 *
 * https://explore.whatismybrowser.com/useragents/explore/
 *
 * Some user agent strings, while valid grammatically, list their info in odd locations.
 * This parser will not be able to pick out the correct information from those.
 */

namespace WebCore {
UserAgentStringParser::UserAgentStringParser(const String& userAgentString)
    : m_userAgentString(userAgentString)
    , data(UserAgentStringData::create()) { };

Ref<UserAgentStringParser> UserAgentStringParser::create(const String& userAgentString)
{
    return adoptRef(*new UserAgentStringParser(userAgentString));
}

std::optional<Ref<UserAgentStringData>> UserAgentStringParser::parse()
{
    data = UserAgentStringData::create();

    if (atEnd())
        return { };

    consumeProduct();

    while (!atEnd()) {
        if (!isTabOrSpace(peek()))
            return { };

        consumeRWS();
        start = pos;
        if (peek() == '(')
            consumeComment();
        else
            consumeProduct();

        if (malformed)
            return { };
    }

    populateUserAgentData();
    return data;
}

void UserAgentStringParser::consumeProduct()
{
    consumeToken();
    if (malformed)
        return;

    auto product = Product { .name = getSubstring(), .version = { } };
    if (!atEnd() && peek() == '/') {
        increment();
        start = pos;
        consumeToken();
        if (malformed)
            return;
        product.version = getSubstring();
    }
    start = pos;
    segments.append(product);
}

void UserAgentStringParser::consumeRWS()
{
    while (!atEnd() && isTabOrSpace(peek()))
        increment();
}

void UserAgentStringParser::consumeComment()
{
    ASSERT(peek() == '(');
    increment(); // pass first '('
    start = pos;

    if (atEnd()) {
        malformed = true;
        return;
    }

    auto c = peek();
    while (!atEnd() && c != ')') {
        if (c == '(')
            consumeComment();
        else if (c == '\\')
            consumeQuotedPair();
        else if (RFC7230::isCommentText(c))
            increment();

        if (malformed)
            return;

        c = peek();
    }

    if (atEnd()) {
        malformed = true;
        return;
    }

    auto s = getSubstring();
    if (!s.isEmpty()) {
        auto comment = Comment { .parts = s.split(';') };
        segments.append(comment);
    }
    increment();
    start = pos;
    // malformed user agent string
}

void UserAgentStringParser::consumeToken()
{
    if (!RFC7230::isTokenCharacter(peek())) {
        malformed = true;
        return;
    }

    do {
        increment();
    } while (!atEnd() && RFC7230::isTokenCharacter(peek()));
}

void UserAgentStringParser::consumeQuotedPair()
{
    ASSERT(peek() == '\\');
    increment(); // pass '\'

    if (RFC7230::isQuotedPairSecondOctet(peek())) {
        increment();
        return;
    }

    malformed = true;
}

inline char16_t UserAgentStringParser::peek()
{
    return m_userAgentString[this->pos];
}

inline void UserAgentStringParser::increment()
{
    this->pos++;
}

inline bool UserAgentStringParser::atEnd()
{
    return this->pos >= this->m_userAgentString.length();
}

inline String UserAgentStringParser::getSubstring()
{
    return m_userAgentString.substring(start, pos - start);
}

struct BrowsersSeen {
    bool brave : 1 { false };
    bool firefox : 1 { false };
    bool chrome : 1 { false };
    bool safari : 1 { false };
    bool opera : 1 { false };
    bool edge : 1 { false };
    String braveVersion;
    String firefoxVersion;
    String chromeVersion;
    String safariVersion;
    String operaVersion;
    String edgeVersion;
};

void UserAgentStringParser::populateUserAgentData()
{
    BrowsersSeen browsersSeen;
    auto weakThis = WeakPtr { *this };
    bool linuxSeen { false };
    for (const auto& segment : segments) {
        WTF::switchOn(segment, [&browsersSeen, weakThis](const Product& p) {
            if (p.name == "Mobile") {
                weakThis->data->mobile = true;
                return;
            }
            if (p.name == "Brave") {
                browsersSeen.braveVersion = p.version;
                browsersSeen.brave = true;
                return;
            }
            if (p.name == "Firefox" || p.name == "fxiOS") {
                browsersSeen.firefoxVersion = p.version;
                browsersSeen.firefox = true;
                return;
            }
            if (p.name == "Chrome") {
                browsersSeen.chromeVersion = p.version;
                browsersSeen.chrome = true;
                return;
            }
            if (p.name == "Safari") {
                browsersSeen.firefoxVersion = p.version;
                browsersSeen.safari = true;
                return;
            }
            if (p.name == "OPR") {
                browsersSeen.operaVersion = p.version;
                browsersSeen.opera = true;
                return;
            }
            if (p.name.contains("Edg")) {
                browsersSeen.edgeVersion = p.version;
                browsersSeen.edge = true;
                return;
            } }, [weakThis, &linuxSeen](const Comment& c) {
            for (const auto& part : c.parts) {
                if (part.contains("Windows")) {
                    weakThis->data->platform = "Windows"_s;
                    return;
                }
                if (part == "Macintosh") {
                    weakThis->data->platform = "macOS"_s;
                    return;
                }
                if (part == "iPhone") {
                    weakThis->data->platform = "iOS"_s;
                    return;
                }
                if (part == "iPad") {
                    weakThis->data->platform = "iOS"_s;
                    return;
                }
                if (part.contains("Android")) {
                    weakThis->data->platform = "Android"_s;
                    return;
                }
                if (part.contains("Linux")) {
                    linuxSeen = true;
                    return;
                }
                if (part.contains("CrOS")) {
                    weakThis->data->platform = "ChromeOS"_s;
                    return;
                }
            } });
    }

    // android user agents sometimes list linux and android, but linux user agents don't list androids
    if (linuxSeen && data->platform.isEmpty())
        data->platform = "Linux"_s;

    // both chrome and firefox sometimes list safari in their user agent strings
    if (browsersSeen.safari && !browsersSeen.chrome && !browsersSeen.firefox) {
        data->browserName = "Safari"_s;
        data->browserVersion = browsersSeen.safariVersion;
        return;
    }

    // no other browser typically list firefox
    if (browsersSeen.firefox) {
        data->browserName = "Firefox"_s;
        data->browserVersion = browsersSeen.firefoxVersion;
    }

    // chrome based browsers typically list chrome
    if (browsersSeen.chrome) {
        if (browsersSeen.edge) {
            data->browserName = "Microsoft Edge"_s;
            data->browserVersion = browsersSeen.edgeVersion;
        } else if (browsersSeen.brave) {
            data->browserName = "Brave"_s;
            data->browserVersion = browsersSeen.braveVersion;
        } else if (browsersSeen.opera) {
            data->browserName = "Opera"_s;
            data->browserVersion = browsersSeen.operaVersion;
        } else {
            data->browserName = "Google Chrome"_s;
            data->browserVersion = browsersSeen.chromeVersion;
        }
    }
}
};
