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
#include <wtf/Vector.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
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

#if HAVE(BROKEN_COOKIE_DATE_PARSER) || USE(SOUP)
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

using ExpiresValueRange = std::pair<size_t, size_t>;

// Returns the range of the last Expires attribute's value, or nullopt when there is none.
// RFC 6265 section 5.3: when an attribute repeats, the last occurrence wins.
static std::optional<ExpiresValueRange> findLastExpiresValue(StringView cookieString)
{
    auto firstSemicolon = cookieString.find(';');
    if (firstSemicolon == notFound)
        return std::nullopt;

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
    return std::make_pair(valueStart, valueEnd);
}

static std::optional<String> cookieStringWithDayFirstExpires(StringView cookieString, ExpiresValueRange expiresValue)
{
    auto [valueStart, valueEnd] = expiresValue;

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

std::optional<String> cookieStringWithDayFirstExpires(StringView cookieString)
{
    auto expiresValue = findLastExpiresValue(cookieString);
    if (!expiresValue)
        return std::nullopt;
    return cookieStringWithDayFirstExpires(cookieString, *expiresValue);
}
#endif

#if HAVE(BROKEN_COOKIE_DATE_PARSER)
static bool isDayNameToken(StringView token)
{
    // The first three characters match both the abbreviation and the full name.
    if (token.length() < 3)
        return false;
    auto prefix = token.left(3);
    return std::ranges::any_of(WTF::weekdayName, [&](auto day) {
        return equalIgnoringASCIICase(prefix, day);
    });
}

// FIXME: <rdar://186224951> Remove this once CFNetwork's cookie-date parser matches the day
// and month names case-insensitively, as RFC 6265 section 5.1.1 requires.
static std::optional<String> cookieStringWithTitleCasedExpiresNames(StringView cookieString, ExpiresValueRange expiresValue)
{
    auto [valueStart, valueEnd] = expiresValue;

    // '-' is a separator so the dd-Mon-yyyy form is covered too.
    auto isSeparator = [](char16_t character) {
        return character == ' ' || character == '\t' || character == ',' || character == '-';
    };

    // Only the leading day-of-week and the first month name are rewritten, which keeps the time zone
    // and the localized trailing comment out of it.
    std::optional<std::pair<size_t, size_t>> dayNameRange;
    std::optional<std::pair<size_t, size_t>> monthNameRange;
    bool isFirstWord = true;
    for (size_t position = valueStart; position < valueEnd;) {
        while (position < valueEnd && isSeparator(cookieString[position]))
            ++position;
        size_t wordStart = position;
        while (position < valueEnd && !isSeparator(cookieString[position]))
            ++position;
        if (wordStart == position)
            break;
        auto word = cookieString.substring(wordStart, position - wordStart);
        if (isFirstWord) {
            isFirstWord = false;
            if (isDayNameToken(word)) {
                dayNameRange = std::make_pair(wordStart, position);
                continue;
            }
        }
        if (isMonthNameToken(word)) {
            monthNameRange = std::make_pair(wordStart, position);
            break;
        }
    }

    auto needsTitleCasing = [&](std::pair<size_t, size_t> range) {
        if (isASCIILower(cookieString[range.first]))
            return true;
        for (size_t i = range.first + 1; i < range.second; ++i) {
            if (isASCIIUpper(cookieString[i]))
                return true;
        }
        return false;
    };
    if (dayNameRange && !needsTitleCasing(*dayNameRange))
        dayNameRange = std::nullopt;
    if (monthNameRange && !needsTitleCasing(*monthNameRange))
        monthNameRange = std::nullopt;
    if (!dayNameRange && !monthNameRange)
        return std::nullopt;

    StringBuilder builder;
    size_t copiedThrough = 0;
    for (auto range : { dayNameRange, monthNameRange }) {
        if (!range)
            continue;
        builder.append(cookieString.substring(copiedThrough, range->first - copiedThrough));
        builder.append(toASCIIUpper(cookieString[range->first]));
        for (size_t i = range->first + 1; i < range->second; ++i)
            builder.append(toASCIILower(cookieString[i]));
        copiedThrough = range->second;
    }
    builder.append(cookieString.substring(copiedThrough));
    return builder.toString();
}

std::optional<String> cookieStringWithTitleCasedExpiresNames(StringView cookieString)
{
    auto expiresValue = findLastExpiresValue(cookieString);
    if (!expiresValue)
        return std::nullopt;
    return cookieStringWithTitleCasedExpiresNames(cookieString, *expiresValue);
}

// Runs the two Expires workarounds in the order that lets them compose.
std::optional<String> cookieStringWithRepairedExpires(StringView cookieString)
{
    auto expiresValue = findLastExpiresValue(cookieString);
    if (!expiresValue)
        return std::nullopt;
    // The swap exchanges two tokens inside the value, so the value's range is unchanged.
    auto swapped = cookieStringWithDayFirstExpires(cookieString, *expiresValue);
    ASSERT(!swapped || swapped->length() == cookieString.length());
    if (auto cased = cookieStringWithTitleCasedExpiresNames(swapped ? StringView { *swapped } : cookieString, *expiresValue))
        return cased;
    return swapped;
}

// Answers from the header alone, so a response that needs nothing costs no cookie policy work.
bool cookieHeaderNeedsRepair(StringView header)
{
    if (!header.containsIgnoringASCIICase("expires"_s))
        return false;
    for (auto cookieString : splitCoalescedSetCookieHeader(header)) {
        if (cookieStringWithRepairedExpires(cookieString))
            return true;
    }
    return false;
}

Vector<StringView, 4> splitCoalescedSetCookieHeader(StringView header)
{
    // Repeated Set-Cookie headers arrive comma-joined, and a cookie-date has a comma of its own, so
    // split only at a comma followed by a "token=". A comma inside a value splits too; callers
    // must check any segment against what CFNetwork actually stored.
    if (header.isEmpty())
        return { };

    Vector<StringView, 4> cookies;
    auto comma = header.find(',');
    if (comma == notFound)
        return { header };

    size_t segmentStart = 0;
    for (; comma != notFound; comma = header.find(',', comma + 1)) {
        size_t candidate = comma + 1;
        while (candidate < header.length() && isTabOrSpace(header[candidate]))
            ++candidate;
        size_t tokenStart = candidate;
        while (candidate < header.length()) {
            auto character = header[candidate];
            if (character == '=' || character == ';' || character == ',' || isTabOrSpace(character) || character == '\n' || character == '\r')
                break;
            ++candidate;
        }
        if (candidate == tokenStart || candidate >= header.length())
            continue;
        while (candidate < header.length() && isTabOrSpace(header[candidate]))
            ++candidate;
        if (candidate >= header.length() || header[candidate] != '=')
            continue;

        cookies.append(header.substring(segmentStart, comma - segmentStart));
        segmentStart = comma + 1;
    }
    if (segmentStart < header.length())
        cookies.append(header.substring(segmentStart));
    return cookies;
}

std::optional<std::pair<StringView, StringView>> cookieNameAndValue(StringView cookieString)
{
    // RFC 6265 section 5.2. Quotes are part of the value and are not stripped.
    auto semicolon = cookieString.find(';');
    auto pair = semicolon == notFound ? cookieString : cookieString.left(semicolon);
    auto equals = pair.find('=');
    if (equals == notFound)
        return std::nullopt;
    auto name = pair.left(equals).trim(isTabOrSpace<char16_t>);
    auto value = pair.substring(equals + 1).trim(isTabOrSpace<char16_t>);
    return std::make_pair(name, value);
}
#endif

// The `cookieStringFor...` functions can reinterpret a cookie string heading from DOM to storage
// or from storage to DOM. Cocoa is the only platform that needs to do this at the moment.
// These functions are a no-op on other platforms.
String cookieStringForScript(const String& storedValue)
{
    return cookieStringForScript(String { storedValue });
}

#if !PLATFORM(COCOA)
String cookieStringForScript(String&& storedValue)
{
    return WTF::move(storedValue);
}

String cookieStringForStorage(const String& scriptValue)
{
    return scriptValue;
}
#endif

} // namespace CookieUtil

} // namespace WebCore

