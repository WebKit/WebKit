/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CookieUtil.h"

#if USE(CURL)

#include "Cookie.h"

#include <wtf/DateMath.h>
#include <wtf/WallTime.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/WTFString.h>

/* This is the maximum line length we accept for a cookie line. RFC 2109
   section 6.3 says:

   "at least 4096 bytes per cookie (as measured by the size of the characters
   that comprise the cookie non-terminal in the syntax description of the
   Set-Cookie header)"
*/

#define MAX_COOKIE_LINE 5000
#define MAX_COOKIE_LINE_TXT "4999"

#define MAX_NAME 1024
#define MAX_NAME_TXT "1023"

namespace WebCore {

namespace CookieUtil {

bool isIPAddress(StringImpl* hostname)
{
    return URL::hostIsIPAddress(hostname);
}

bool domainMatch(StringImpl* cookieDomain, StringImpl* host)
{
    size_t index = find(host, cookieDomain);

    bool tailMatch = (index != notFound && index + length(cookieDomain) == length(host));

    // Check if host equals cookie domain.
    if (tailMatch && !index)
        return true;

    // Check if host is a subdomain of the domain in the cookie.
    // Curl uses a '.' in front of domains to indicate it's valid on subdomains.
    if (tailMatch && index > 0 && codeUnitAt(host, index) == '.')
        return true;

    // Check the special case where host equals the cookie domain, except for a leading '.' in the cookie domain.
    // E.g. cookie domain is .apple.com and host is apple.com.
    if (codeUnitAt(cookieDomain, 0) == '.' && find(cookieDomain, host) == 1)
        return true;

    return false;
}

static std::optional<double> parseExpiresMS(std::span<const Latin1Character> expires)
{
    double tmp = parseDate(expires);
    if (isnan(tmp))
        return { };

    return std::optional<double> {tmp};
}

static void parseCookieAttributes(StringImpl* attribute, bool& hasMaxAge, Cookie& result)
{
    size_t assignmentPosition = find(attribute, '=');

    RefPtr<StringImpl> attributeName;
    RefPtr<StringImpl> attributeValue;

    if (assignmentPosition != notFound) {
        attributeName = trim(left(attribute, assignmentPosition), deprecatedIsSpaceOrNewline);
        attributeValue = trim(substring(attribute, assignmentPosition + 1), deprecatedIsSpaceOrNewline);
    } else
        attributeName = trim(attribute, deprecatedIsSpaceOrNewline);

    if (equalLettersIgnoringASCIICase(attributeName, "httponly"_s))
        result.httpOnly = true;
    else if (equalLettersIgnoringASCIICase(attributeName, "secure"_s))
        result.secure = true;
    else if (equalLettersIgnoringASCIICase(attributeName, "domain"_s)) {
        if (isEmpty(attributeValue))
            return;

        // Enforce a dot character prefix to hostnames which are not ip addresses and not single value hostnames such as localhost
        if (!isIPAddress(attributeValue) && !startsWith(attributeValue, '.') && find(attributeValue, '.') != notFound)
            attributeValue = makeString('.', attributeValue);

        result.domain = convertToASCIILowercase(attributeValue);

    } else if (equalLettersIgnoringASCIICase(attributeName, "max-age"_s)) {
        if (auto maxAgeSeconds = parseIntegerAllowingTrailingJunk<int64_t>(attributeValue)) {
            result.expires = (WallTime::now().secondsSinceEpoch().value() + *maxAgeSeconds) * msPerSecond;
            result.session = false;

            // If there is a max-age attribute as well as an expires attribute
            // the rightmost max-age attribute takes precedence.
            hasMaxAge = true;
        } else {
            result.session = true;
            result.expires = std::nullopt;
        }
    } else if (equalLettersIgnoringASCIICase(attributeName, "expires"_s) && !hasMaxAge) {
        // FIXME: This code passes a UTF-8 buffer to a function that expects to parse Latin1.
        if (auto expiryTime = parseExpiresMS(byteCast<Latin1Character>(utf8(attributeValue).span()))) {
            result.expires = expiryTime.value();
            result.session = false;
        } else if (!hasMaxAge) {
            result.session = true;
            result.expires = std::nullopt;
        }
    } else if (equalLettersIgnoringASCIICase(attributeName, "path"_s)) {
        if (!isEmpty(attributeValue) && startsWith(attributeValue, '/'))
            result.path = attributeValue;
        else
            result.path = emptyString();
    }
}

std::optional<Cookie> parseCookieHeader(StringImpl* cookieLine)
{
    if (length(cookieLine) >= MAX_COOKIE_LINE)
        return std::nullopt;

    // This Algorithm is based on the algorithm defined in RFC 6265 5.2 https://tools.ietf.org/html/rfc6265#section-5.2/

    size_t separatorPosition = find(cookieLine, ';');

    RefPtr<StringImpl> cookiePair = separatorPosition == notFound ? RefPtr<StringImpl> { cookieLine } : left(cookieLine, separatorPosition);

    RefPtr<StringImpl> cookieName;
    RefPtr<StringImpl> cookieValue;
    size_t assignmentPosition = find(cookiePair, '=');

    // RFC6265 says to ignore cookies pairs with empty names or no assignment character
    // but browsers seem to treat this type of cookie string as the cookie value
    if (assignmentPosition == notFound)
        cookieValue = cookiePair;
    else {
        cookieName = left(cookiePair, assignmentPosition);
        cookieValue = substring(cookiePair, assignmentPosition + 1);
    }

    Cookie cookie;
    cookie.name = trim(cookieName, deprecatedIsSpaceOrNewline);
    cookie.value = trim(cookieValue, deprecatedIsSpaceOrNewline);

    bool hasMaxAge = false;
    cookie.session = true;

    for (auto attribute : splitAllowingEmptyEntries(cookieLine, ';'))
        parseCookieAttributes(attribute, hasMaxAge, cookie);

    return cookie;
}

} // namespace CookieUtil

} // namespace WebCore

#endif
