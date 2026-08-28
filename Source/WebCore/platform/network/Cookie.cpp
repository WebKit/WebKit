/*
 * Copyright (C) 2017-2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Cookie.h"

#include <algorithm>
#include <wtf/ASCIICType.h>
#include <wtf/DateMath.h>
#include <wtf/NotFound.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/StringView.h>

namespace WebCore {
    
#if !PLATFORM(COCOA)
bool Cookie::operator==(const Cookie& other) const
{
    return name == other.name
        && domain == other.domain
        && path == other.path
        && secure == other.secure;
}
    
unsigned Cookie::hash() const
{
    return StringHash::hash(name) + StringHash::hash(domain) + StringHash::hash(path) + secure;
}
#endif

namespace CookieUtil {

String defaultPathForURL(const URL& url)
{
    // Algorithm to generate the default path is outlined in https://tools.ietf.org/html/rfc6265#section-5.1.4

    String path = url.path().toString();
    if (path.isEmpty() || !path.startsWith('/'))
        return "/"_s;

    auto lastSlashPosition = path.reverseFind('/');
    if (!lastSlashPosition)
        return "/"_s;

    return path.left(lastSlashPosition);
}

static bool isMonthNameToken(StringView token)
{
    // RFC 6265 section 5.1.1 matches a month by its first three characters, case-insensitively.
    if (token.length() < 3)
        return false;
    auto prefix = token.left(3);
    return std::ranges::any_of(WTF::monthName, [&](auto month) {
        return equalIgnoringASCIICase(prefix, month);
    });
}

std::optional<String> cookieStringWithDayFirstExpires(StringView cookieString)
{
    auto firstSemicolon = cookieString.find(';');
    if (firstSemicolon == notFound)
        return std::nullopt;

    // Locate the Expires value within the original string. The last one wins, per RFC 6265
    // section 5.3.
    size_t valueStart = notFound;
    size_t valueEnd = notFound;
    for (size_t position = firstSemicolon + 1; position <= cookieString.length();) {
        auto semicolon = cookieString.find(';', position);
        auto attributeEnd = semicolon == notFound ? cookieString.length() : semicolon;
        auto attribute = cookieString.substring(position, attributeEnd - position);
        if (auto equals = attribute.find('='); equals != notFound) {
            if (equalLettersIgnoringASCIICase(attribute.left(equals).trim(isTabOrSpace<char16_t>), "expires"_s)) {
                valueStart = position + equals + 1;
                valueEnd = attributeEnd;
            }
        }
        if (semicolon == notFound)
            break;
        position = semicolon + 1;
    }

    if (valueStart == notFound)
        return std::nullopt;

    // Find a month name immediately followed by a one or two digit day of the month. In a day-first
    // value the token after the month is the four digit year, so this does not match and nothing is
    // rewritten.
    auto isSeparator = [](char16_t character) {
        return character == ' ' || character == '\t';
    };
    size_t monthStart = notFound;
    size_t monthEnd = notFound;
    size_t dayStart = notFound;
    size_t dayEnd = notFound;
    for (size_t position = valueStart; position < valueEnd;) {
        while (position < valueEnd && isSeparator(cookieString[position]))
            ++position;
        size_t tokenStart = position;
        while (position < valueEnd && !isSeparator(cookieString[position]))
            ++position;
        if (tokenStart == position)
            break;
        if (monthStart == notFound) {
            if (isMonthNameToken(cookieString.substring(tokenStart, position - tokenStart))) {
                monthStart = tokenStart;
                monthEnd = position;
            }
            continue;
        }
        dayStart = tokenStart;
        dayEnd = position;
        break;
    }

    if (monthStart == notFound || dayStart == notFound)
        return std::nullopt;

    auto day = cookieString.substring(dayStart, dayEnd - dayStart);
    if (day.length() > 2 || !day.containsOnly<isASCIIDigit<char16_t>>())
        return std::nullopt;

    size_t yearStart = dayEnd;
    while (yearStart < valueEnd && isSeparator(cookieString[yearStart]))
        ++yearStart;
    size_t yearEnd = yearStart;
    while (yearEnd < valueEnd && !isSeparator(cookieString[yearEnd]))
        ++yearEnd;
    auto year = cookieString.substring(yearStart, yearEnd - yearStart);
    if (year.length() != 4 || !year.containsOnly<isASCIIDigit<char16_t>>())
        return std::nullopt;

    // Return a new string where we have swapped the two tokens.
    return makeString(cookieString.left(monthStart), day, cookieString.substring(monthEnd, dayStart - monthEnd),
        cookieString.substring(monthStart, monthEnd - monthStart), cookieString.substring(dayEnd));
}

CookieNamePrefix cookieNamePrefix(StringView name)
{
    // Every prefix starts with "__", so this rejects the overwhelmingly common case up front.
    if (!name.startsWith("__"_s))
        return CookieNamePrefix::None;

    // "__Host-Http-" must be tested before "__Host-" and "__Http-", since it starts with the former.
    if (name.startsWithIgnoringASCIICase("__Host-Http-"_s))
        return CookieNamePrefix::HostHttp;
    if (name.startsWithIgnoringASCIICase("__Host-"_s))
        return CookieNamePrefix::Host;
    if (name.startsWithIgnoringASCIICase("__Http-"_s))
        return CookieNamePrefix::Http;
    if (name.startsWithIgnoringASCIICase("__Secure-"_s))
        return CookieNamePrefix::Secure;
    return CookieNamePrefix::None;
}

DOMCookieFields parseDOMCookieFields(StringView cookieString)
{
    DOMCookieFields fields;

    auto firstSemicolon = cookieString.find(';');
    auto nameValuePair = firstSemicolon == notFound ? cookieString : cookieString.left(firstSemicolon);

    // RFC 6265bis section 5.2 would treat a pair with no '=' as a value with an empty name, but the
    // platform cookie stores do not: a name with no '=' becomes that name with an empty value. Match
    // the store rather than the draft, so that the prefix this check sees is the prefix the store
    // ends up applying. An explicitly empty name, as in "=value", carries no prefix at all.
    auto equals = nameValuePair.find('=');
    auto name = equals == notFound ? nameValuePair : nameValuePair.left(equals);
    fields.prefix = cookieNamePrefix(name.trim(isTabOrSpace<char16_t>));

    if (firstSemicolon == notFound)
        return fields;

    for (auto attribute : cookieString.substring(firstSemicolon + 1).split(';')) {
        StringView attributeName;
        StringView attributeValue;
        if (auto equals = attribute.find('='); equals != notFound) {
            attributeName = attribute.left(equals).trim(isTabOrSpace<char16_t>);
            attributeValue = attribute.substring(equals + 1).trim(isTabOrSpace<char16_t>);
        } else
            attributeName = attribute.trim(isTabOrSpace<char16_t>);

        if (equalLettersIgnoringASCIICase(attributeName, "secure"_s))
            fields.isSecure = true;
        else if (equalLettersIgnoringASCIICase(attributeName, "httponly"_s)) {
            // "HttpOnly" is a valueless flag, but section 5.2 discards any value it is given rather
            // than ignoring the attribute, so only the name is examined.
            fields.hasHttpOnlyAttribute = true;
        } else if (equalLettersIgnoringASCIICase(attributeName, "domain"_s)) {
            // Section 5.2.3 ignores an empty Domain entirely, so it never enters the attribute list
            // and cannot be used to clear an earlier one.
            if (!attributeValue.isEmpty())
                fields.hasDomain = true;
        } else if (equalLettersIgnoringASCIICase(attributeName, "path"_s)) {
            // Last Path wins. Only the literal "/" counts; see DOMCookieFields::hasRootPath.
            fields.hasRootPath = attributeValue == "/"_s;
        }
    }

    return fields;
}

bool cookieNamePrefixRequirementsViolated(CookieNamePrefix prefix, bool isSecure, bool httpOnly, bool hasDomain, bool hasRootPath)
{
    switch (prefix) {
    case CookieNamePrefix::None:
        return false;
    case CookieNamePrefix::Secure:
        return !isSecure;
    case CookieNamePrefix::Host:
        return !isSecure || hasDomain || !hasRootPath;
    case CookieNamePrefix::Http:
        return !isSecure || !httpOnly;
    case CookieNamePrefix::HostHttp:
        return !isSecure || !httpOnly || hasDomain || !hasRootPath;
    }

    // Fail closed: a prefix added to the enum without updating this switch must reject rather than
    // silently gain an exemption.
    ASSERT_NOT_REACHED();
    return true;
}

} // namespace CookieUtil

} // namespace WebCore

