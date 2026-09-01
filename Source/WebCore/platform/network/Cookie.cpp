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
#include <wtf/text/ASCIIFastPath.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuffer.h>
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

#if HAVE(BROKEN_MONTH_BEFORE_DAY_EXPIRES_COOKIE_PARSER) || USE(SOUP)
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

// Returns the range of the last Expires attribute's value, or nullopt when there is none.
// RFC 6265 section 5.3: when an attribute repeats, the last occurrence wins.
static std::optional<std::pair<size_t, size_t>> findLastExpiresValue(StringView cookieString)
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

std::optional<String> cookieStringWithDayFirstExpires(StringView cookieString)
{
    auto expiresValue = findLastExpiresValue(cookieString);
    if (!expiresValue)
        return std::nullopt;
    auto [valueStart, valueEnd] = *expiresValue;

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
#endif

#if HAVE(BROKEN_MONTH_BEFORE_DAY_EXPIRES_COOKIE_PARSER)
static bool isDayNameToken(StringView token)
{
    // RFC 6265 section 5.1.1 tells a parser to ignore the day-of-week, so only its spelling
    // matters here, never whether it agrees with the rest of the date. Matching on the first
    // three characters accepts both the abbreviation and the full name.
    if (token.length() < 3)
        return false;
    auto prefix = token.left(3);
    return std::ranges::any_of(WTF::weekdayName, [&](auto day) {
        return equalIgnoringASCIICase(prefix, day);
    });
}

std::optional<String> cookieStringWithTitleCasedExpiresNames(StringView cookieString)
{
    // FIXME: <rdar://186224951> Remove this once CFNetwork's cookie-date parser matches the day
    // and month names case-insensitively, as RFC 6265 section 5.1.1 requires.
    //
    // CFNetwork currently requires both names in title case. When the date is rejected,
    // the cookie silently becomes session scoped.
    auto expiresValue = findLastExpiresValue(cookieString);
    if (!expiresValue)
        return std::nullopt;
    auto [valueStart, valueEnd] = *expiresValue;

    // Split on the separators that can surround a name, so the dd-Mon-yyyy form is covered as
    // well as the space separated one: "05-JAN-2027" is a single space-delimited token, and its
    // month is just as badly handled as a standalone "JAN".
    auto isSeparator = [](char16_t character) {
        return character == ' ' || character == '\t' || character == ',' || character == '-';
    };

    // Only two words are ever rewritten: the leading day-of-week, and the first month name. Any
    // later word is left alone, which is what keeps the time zone and the trailing parenthesized
    // comment out of this. That comment is localized -- Date.prototype.toString() can emit
    // "(日本標準時)" -- and rewriting it would be gratuitous.
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

    // Already title cased means nothing to do. Testing for the characters that are actually wrong,
    // rather than requiring every trailing character to be lowercase, keeps a word that contains a
    // digit or a non-ASCII letter from being reported as needing a rewrite that would not change it.
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
#endif

#if HAVE(BROKEN_MONTH_BEFORE_DAY_EXPIRES_COOKIE_PARSER) || HAVE(BROKEN_NON_ASCII_COOKIE_PARSER)
Vector<StringView> splitCoalescedSetCookieHeader(StringView header)
{
    // CFHTTPMessageCopyAllHeaderFields collapses repeated Set-Cookie headers into a single
    // comma-joined value, and an RFC 1123 cookie-date contains ", " of its own
    // ("Sun, 05 Jan 2027 00:00:00 GMT"). Splitting naively on every comma therefore tears
    // every dated cookie in half.
    //
    // The disambiguator: only split at a comma whose following token is an attribute-or-name
    // assignment, i.e. a run of non-whitespace, non-delimiter characters followed by '='.
    // Inside a cookie-date the text after the comma is " 05 Jan 2027 ...", where the first
    // token is followed by a space rather than '=', so no split can occur there. This is exact
    // for the RFC 1123 and dd-Mon-yyyy forms, which are the only cookie-date shapes that
    // contain a comma at all.
    if (header.isEmpty())
        return { };

    Vector<StringView> cookies;
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

std::optional<String> cookieStringWithRecoveredUTF8(StringView cookieString)
{
    // CFNetwork builds the CFStrings in a response's header map by treating each raw header BYTE
    // as one code unit, so a UTF-8 header arrives as ISO-8859-1 mojibake: the bytes E6 98 A5
    // ("春") become U+00E6 U+0098 U+00A5. Nothing above U+00FF ever appears, which is why a
    // simple "is there a wide character" test cannot detect this.
    //
    // The recovery is to reverse that byte-per-code-unit mapping and decode the result as UTF-8.
    // This doubles as the detector: a header that is genuinely ISO-8859-1 (a lone 0xE9 for "é",
    // say) is not valid UTF-8, so the decode fails and we leave it alone. That makes the test
    // decidable rather than heuristic.
    //
    // A response header string is 8-bit backed in practice, since String(CFStringRef) takes the
    // Latin-1 path, so the byte span the decoder wants is already there.
    if (cookieString.is8Bit()) {
        auto bytes = cookieString.span8();
        if (charactersAreAllASCII(bytes))
            return std::nullopt;
        auto recovered = String::fromUTF8(bytes);
        if (recovered.isNull() || recovered == cookieString)
            return std::nullopt;
        return recovered;
    }

    // is8Bit() is not the same question as "nothing above U+00FF", so a 16-bit backed view still
    // has to be checked before its code units can be treated as bytes.
    if (!WTF::charactersAreAllLatin1(cookieString.span16()))
        return std::nullopt;

    Vector<Latin1Character> bytes;
    bytes.reserveInitialCapacity(cookieString.length());
    bool sawNonASCII = false;
    for (auto character : cookieString.span16()) {
        if (character > 0x7F)
            sawNonASCII = true;
        bytes.append(static_cast<Latin1Character>(character));
    }
    if (!sawNonASCII)
        return std::nullopt;

    auto recovered = String::fromUTF8(bytes.span());
    if (recovered.isNull() || recovered == cookieString)
        return std::nullopt;
    return recovered;
}

String cookieStringWithNonASCIIReplaced(StringView cookieString)
{
    // Produce a same-shape, all-ASCII stand-in so CFNetwork can parse the ATTRIBUTES for us.
    // Delimiters, quoting, and length are preserved, so attribute parsing, cookie-name prefix
    // enforcement, and size limits behave exactly as they would for the real string. Only the
    // name and value are substituted back afterwards, and those are the sole things the charset
    // defect damages.
    //
    // Tab is kept alongside the printable range because it is legal cookie whitespace, and
    // replacing it would change how surrounding attributes are trimmed.
    StringBuffer<Latin1Character> buffer(cookieString.length());
    auto replaced = buffer.span();
    auto nameEnd = cookieString.find('=');
    for (unsigned i = 0; i < cookieString.length(); ++i) {
        auto character = cookieString[i];
        bool isInName = nameEnd == notFound || i < nameEnd;
        bool keep = (isASCIIPrintable(character) || character == '\t') && !(isInName && character == '"');
        replaced[i] = keep ? static_cast<Latin1Character>(character) : 'x';
    }
    return String::adopt(WTF::move(buffer));
}

std::optional<std::pair<StringView, StringView>> cookieNameAndValue(StringView cookieString)
{
    // RFC 6265 section 5.2: the name-value-pair is everything up to the first ';'; the name is
    // what precedes the first '=' and the value is the remainder, each trimmed of surrounding
    // whitespace. Quotes are part of the value and are deliberately not stripped.
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

} // namespace CookieUtil

} // namespace WebCore

