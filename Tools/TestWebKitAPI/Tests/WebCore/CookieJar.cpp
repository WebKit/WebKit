/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/Cookie.h>
#include <WebCore/CookieJar.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

// Expose the protected static method for testing.
struct CookieJarForTesting : public WebCore::CookieJar {
    using WebCore::CookieJar::shouldIncludeSecureCookies;
};

TEST(CookieJar, ShouldIncludeSecureCookiesForHTTPS)
{
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("https://example.com/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("https://8.8.8.8/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("https://example.com:8443/"_s)), WebCore::IncludeSecureCookies::Yes);
}

TEST(CookieJar, ShouldNotIncludeSecureCookiesForPlainHTTP)
{
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://example.com/"_s)), WebCore::IncludeSecureCookies::No);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://8.8.8.8/"_s)), WebCore::IncludeSecureCookies::No);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://192.168.1.1/"_s)), WebCore::IncludeSecureCookies::No);
}

TEST(CookieJar, ShouldNotIncludeSecureCookiesForNonLocalHostnames)
{
    // "localhost" must appear exactly or as the rightmost label.
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://notlocalhost/"_s)), WebCore::IncludeSecureCookies::No);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://localhost.example.com/"_s)), WebCore::IncludeSecureCookies::No);
}

TEST(CookieJar, ShouldNotIncludeSecureCookiesForNonLoopbackIPv4)
{
    // Starts with "127." but has non-digit characters.
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://127.example.com/"_s)), WebCore::IncludeSecureCookies::No);
}

#if HAVE(LOCALHOST_TIED_TO_LOOPBACK)
TEST(CookieJar, ShouldIncludeSecureCookiesForIPv6Loopback)
{
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://[::1]/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://[::1]:8080/"_s)), WebCore::IncludeSecureCookies::Yes);
}

TEST(CookieJar, ShouldIncludeSecureCookiesForIPv4Loopback)
{
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://127.0.0.1/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://127.0.0.2/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://127.1.2.3/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://127.255.255.255/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://127.0.0.1:8080/"_s)), WebCore::IncludeSecureCookies::Yes);
}

TEST(CookieJar, ShouldIncludeSecureCookiesForLocalhost)
{
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://localhost/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://localhost:3000/"_s)), WebCore::IncludeSecureCookies::Yes);

    // Case insensitive.
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://LOCALHOST/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://LocalHost/"_s)), WebCore::IncludeSecureCookies::Yes);
}

TEST(CookieJar, ShouldIncludeSecureCookiesForLocalhostSubdomains)
{
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://foo.localhost/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://foo.bar.localhost/"_s)), WebCore::IncludeSecureCookies::Yes);

    // Case insensitive.
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://foo.LOCALHOST/"_s)), WebCore::IncludeSecureCookies::Yes);
}

TEST(CookieJar, ShouldIncludeSecureCookiesForNormalizedIPv4Loopback)
{
    // The WHATWG URL parser normalizes abbreviated IPv4 addresses, so "127.0.1"
    // expands to "127.0.0.1" before shouldIncludeSecureCookies sees the host.
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://127.0.1/"_s)), WebCore::IncludeSecureCookies::Yes);
}
#endif

// MARK: - CookieUtil::cookieHeaderMayNeedRepair

#if HAVE(BROKEN_COOKIE_DATE_PARSER) || HAVE(BROKEN_NON_ASCII_COOKIE_PARSER)
TEST(CookieUtil, MayNeedRepairRejectsTheOverwhelminglyCommonCase)
{
    // This is the pre-scan that keeps an ordinary response from paying for a split and several
    // attribute walks. Anything without an Expires or a non-ASCII byte cannot need repairing.
    EXPECT_FALSE(WebCore::CookieUtil::cookieHeaderMayNeedRepair("a=1; Path=/; Secure; HttpOnly"_s));
    EXPECT_FALSE(WebCore::CookieUtil::cookieHeaderMayNeedRepair("a=1; Max-Age=3600"_s));
    EXPECT_FALSE(WebCore::CookieUtil::cookieHeaderMayNeedRepair("a=1"_s));
    EXPECT_FALSE(WebCore::CookieUtil::cookieHeaderMayNeedRepair(""_s));
}

TEST(CookieUtil, MayNeedRepairAcceptsEveryShapeThatCouldNeedIt)
{
    // Over-approximating is fine, missing one is not. Both date defects require an Expires, in
    // any casing, and the charset defect always shows up as a non-ASCII byte.
    EXPECT_TRUE(WebCore::CookieUtil::cookieHeaderMayNeedRepair("a=1; Expires=Sun Jan 05 2027 00:00:00 GMT"_s));
    EXPECT_TRUE(WebCore::CookieUtil::cookieHeaderMayNeedRepair("a=1; EXPIRES=Sun, 05 JAN 2027 00:00:00 GMT"_s));
    EXPECT_TRUE(WebCore::CookieUtil::cookieHeaderMayNeedRepair("a=1; expires=Tue, 05 Jan 2027 00:00:00 GMT"_s));
    // Mojibake from the charset defect is all below U+0100, so "is there a wide character" would
    // miss it; the scan has to test against ASCII rather than Latin-1.
    EXPECT_TRUE(WebCore::CookieUtil::cookieHeaderMayNeedRepair(u"test=1æ¥"_str));
    EXPECT_TRUE(WebCore::CookieUtil::cookieHeaderMayNeedRepair(u"春节回=4家路"_str));
}
#endif

// MARK: - CookieUtil::splitCoalescedSetCookieHeader

#if HAVE(BROKEN_COOKIE_DATE_PARSER) || HAVE(BROKEN_NON_ASCII_COOKIE_PARSER)
static Vector<String> splitHeader(ASCIILiteral header)
{
    return WTF::map(WebCore::CookieUtil::splitCoalescedSetCookieHeader(StringView { header }), [](auto segment) {
        return segment.toString();
    });
}

TEST(CookieUtil, SplitDoesNotBreakOnACommaInsideACookieDate)
{
    // The whole point of this function: an RFC 1123 cookie-date contains ", " of its own, so a
    // naive split on every comma would tear this single cookie in half.
    auto cookies = splitHeader("a=1; expires=Sun, 05 Jan 2027 00:00:00 GMT; path=/"_s);
    EXPECT_EQ(cookies.size(), 1u);
    EXPECT_STREQ(cookies[0].utf8().legacyCStringPointer(), "a=1; expires=Sun, 05 Jan 2027 00:00:00 GMT; path=/");
}

TEST(CookieUtil, SplitSeparatesTwoDatedCookies)
{
    auto cookies = splitHeader("a=1; expires=Sun, 05 Jan 2027 00:00:00 GMT; path=/, b=2; expires=Mon, 06 Jan 2027 00:00:00 GMT"_s);
    EXPECT_EQ(cookies.size(), 2u);
    EXPECT_STREQ(cookies[0].utf8().legacyCStringPointer(), "a=1; expires=Sun, 05 Jan 2027 00:00:00 GMT; path=/");
    EXPECT_STREQ(cookies[1].utf8().legacyCStringPointer(), " b=2; expires=Mon, 06 Jan 2027 00:00:00 GMT");
}

TEST(CookieUtil, SplitHandlesTheDayFirstDashedDateForm)
{
    auto cookies = splitHeader("a=1; expires=Sun, 05-Jan-2027 00:00:00 GMT, b=2"_s);
    EXPECT_EQ(cookies.size(), 2u);
    EXPECT_STREQ(cookies[0].utf8().legacyCStringPointer(), "a=1; expires=Sun, 05-Jan-2027 00:00:00 GMT");
}

TEST(CookieUtil, SplitHandlesUndatedCookies)
{
    EXPECT_EQ(splitHeader("a=1; path=/, b=2; path=/, c=3"_s).size(), 3u);
    EXPECT_EQ(splitHeader("a=1"_s).size(), 1u);
    EXPECT_EQ(splitHeader(""_s).size(), 0u);
}

TEST(CookieUtil, SplitHandlesTheMonthFirstDateFormWhichHasNoComma)
{
    // Date.prototype.toString() output contains no comma at all, so a following cookie is the
    // only thing a comma here can mean.
    auto single = splitHeader("a=1; expires=Sun Jan 05 2027 00:00:00 GMT-0800 (PST); path=/"_s);
    EXPECT_EQ(single.size(), 1u);

    auto pair = splitHeader("a=1; expires=Sun Jan 05 2027 00:00:00 GMT-0800 (PST), b=2; path=/"_s);
    EXPECT_EQ(pair.size(), 2u);
    EXPECT_STREQ(pair[1].utf8().legacyCStringPointer(), " b=2; path=/");
}

TEST(CookieUtil, SplitTreatsAQuotedCommaAsASeparatorWhichIsAKnownLimitation)
{
    // CFNetwork joins repeated headers with ", ", so a quoted value containing ", name=" is
    // genuinely ambiguous and is knowingly out of scope. This pins the behaviour rather than
    // claiming it is correct.
    //
    // Over-splitting here would otherwise let the repair STORE a cookie the server never sent,
    // because the trailing fragment parses as a well-formed cookie of its own. What stops that is
    // in repairCookiesFromHTTPResponse(): the expires-only repair re-stores a cookie only when one
    // with that exact name and value is already in the jar, which a synthesized fragment never is.
    EXPECT_EQ(splitHeader("a=\"1, b=2\"; path=/"_s).size(), 2u);
}

TEST(CookieUtil, SplitDoesNotSplitOnACommaInsideACookieValue)
{
    // A comma in a value is only ambiguous when followed by something shaped like an
    // assignment. "b" here is not, so this stays one cookie.
    EXPECT_EQ(splitHeader("a=1,2,3; path=/"_s).size(), 1u);
    EXPECT_EQ(splitHeader("a=x, b; path=/"_s).size(), 1u);
}
#endif

// MARK: - CookieUtil::cookieStringWithDayFirstExpires

#if HAVE(BROKEN_COOKIE_DATE_PARSER) || USE(SOUP)
TEST(CookieUtil, DayFirstRewritesTheMonthFirstOrdering)
{
    auto rewritten = WebCore::CookieUtil::cookieStringWithDayFirstExpires("a=1; Expires=Sun Jan 05 2027 00:00:00 GMT-0800 (PST)"_s);
    ASSERT_TRUE(!!rewritten);
    EXPECT_STREQ(rewritten->utf8().legacyCStringPointer(), "a=1; Expires=Sun 05 Jan 2027 00:00:00 GMT-0800 (PST)");
}

TEST(CookieUtil, DayFirstLeavesAWellFormedDateAlone)
{
    // A day-first value must not be rewritten: the token after the month is a four digit year,
    // not a one or two digit day, so nothing matches.
    EXPECT_FALSE(!!WebCore::CookieUtil::cookieStringWithDayFirstExpires("a=1; Expires=Sun, 05 Jan 2027 00:00:00 GMT"_s));
    EXPECT_FALSE(!!WebCore::CookieUtil::cookieStringWithDayFirstExpires("a=1; path=/"_s));
    EXPECT_FALSE(!!WebCore::CookieUtil::cookieStringWithDayFirstExpires("a=1"_s));
}

TEST(CookieUtil, DayFirstIgnoresAnExpiresLikeCookieName)
{
    // "date_expires" is a cookie NAME, not the Expires attribute, and must not be treated as one.
    EXPECT_FALSE(!!WebCore::CookieUtil::cookieStringWithDayFirstExpires("date_expires=Jan 05 2027; path=/"_s));
}

TEST(CookieUtil, DayFirstUsesTheLastExpiresAttribute)
{
    // RFC 6265 section 5.3: when an attribute repeats, the last occurrence wins.
    auto rewritten = WebCore::CookieUtil::cookieStringWithDayFirstExpires("a=1; Expires=Sun, 05 Jan 2027 00:00:00 GMT; Expires=Mon Feb 06 2028 00:00:00 GMT"_s);
    ASSERT_TRUE(!!rewritten);
    EXPECT_STREQ(rewritten->utf8().legacyCStringPointer(), "a=1; Expires=Sun, 05 Jan 2027 00:00:00 GMT; Expires=Mon 06 Feb 2028 00:00:00 GMT");
}

TEST(CookieUtil, DayFirstHandlesANonASCIITimeZoneComment)
{
    // Date.prototype.toString() localizes only the parenthesized time zone name. This is the
    // shape that made the defect locale dependent.
    auto rewritten = WebCore::CookieUtil::cookieStringWithDayFirstExpires(u"a=1; Expires=Sun Jan 05 2027 00:00:00 GMT-0800 (日本標準時)"_str);
    ASSERT_TRUE(!!rewritten);
    EXPECT_TRUE(rewritten->contains("05 Jan 2027"_s));
}
#endif

// MARK: - CookieUtil::cookieStringWithTitleCasedExpiresNames

#if HAVE(BROKEN_COOKIE_DATE_PARSER)
static String titleCased(StringView cookieString)
{
    auto rewritten = WebCore::CookieUtil::cookieStringWithTitleCasedExpiresNames(cookieString);
    // Report "(no rewrite)" rather than a null String, so a regression reads as a diff against the
    // expected text instead of an EXPECT_STREQ against nullptr.
    return rewritten ? *rewritten : "(no rewrite)"_str;
}

TEST(CookieUtil, TitleCaseFixesAnUppercaseMonth)
{
    EXPECT_STREQ(titleCased("a=1; Expires=Tue, 05 JAN 2027 00:00:00 GMT"_s).utf8().legacyCStringPointer(), "a=1; Expires=Tue, 05 Jan 2027 00:00:00 GMT");
}

TEST(CookieUtil, TitleCaseFixesAnUppercaseDayNameSeparatelyFromTheMonth)
{
    // The two are worth asserting apart: CFNetwork accepts an all-lowercase month abbreviation
    // but no non-title-cased day name at all, so the two names go through different lookups.
    EXPECT_STREQ(titleCased("a=1; Expires=TUE, 05 Jan 2027 00:00:00 GMT"_s).utf8().legacyCStringPointer(), "a=1; Expires=Tue, 05 Jan 2027 00:00:00 GMT");
    EXPECT_STREQ(titleCased("a=1; Expires=tue, 05 Jan 2027 00:00:00 GMT"_s).utf8().legacyCStringPointer(), "a=1; Expires=Tue, 05 Jan 2027 00:00:00 GMT");
}

TEST(CookieUtil, TitleCaseFixesBothNamesAtOnce)
{
    EXPECT_STREQ(titleCased("a=1; Expires=SUN, 05 JAN 2027 00:00:00 GMT"_s).utf8().legacyCStringPointer(), "a=1; Expires=Sun, 05 Jan 2027 00:00:00 GMT");
}

TEST(CookieUtil, TitleCaseHandlesFullDayAndMonthNames)
{
    EXPECT_STREQ(titleCased("a=1; Expires=TUESDAY, 05 JANUARY 2027 00:00:00 GMT"_s).utf8().legacyCStringPointer(), "a=1; Expires=Tuesday, 05 January 2027 00:00:00 GMT");
}

TEST(CookieUtil, TitleCaseHandlesADayNameWithNoComma)
{
    // This is the shape cookieStringWithDayFirstExpires produces, so the day name has to be
    // recognized without relying on a trailing comma.
    EXPECT_STREQ(titleCased("a=1; Expires=SUN 05 Jan 2027 00:00:00 GMT"_s).utf8().legacyCStringPointer(), "a=1; Expires=Sun 05 Jan 2027 00:00:00 GMT");
}

TEST(CookieUtil, TitleCaseHandlesTheDashedDateForm)
{
    // "05-JAN-2027" is a single space-delimited token, so the month has to be found across the
    // dashes as well as across spaces.
    EXPECT_STREQ(titleCased("a=1; Expires=TUE, 05-JAN-2027 00:00:00 GMT"_s).utf8().legacyCStringPointer(), "a=1; Expires=Tue, 05-Jan-2027 00:00:00 GMT");
}

TEST(CookieUtil, TitleCaseLeavesAnAcceptedDateAlone)
{
    EXPECT_FALSE(!!WebCore::CookieUtil::cookieStringWithTitleCasedExpiresNames("a=1; Expires=Tue, 05 Jan 2027 00:00:00 GMT"_s));
    EXPECT_FALSE(!!WebCore::CookieUtil::cookieStringWithTitleCasedExpiresNames("a=1; Expires=05 Jan 2027 00:00:00 GMT"_s));
    EXPECT_FALSE(!!WebCore::CookieUtil::cookieStringWithTitleCasedExpiresNames("a=1; path=/"_s));
    EXPECT_FALSE(!!WebCore::CookieUtil::cookieStringWithTitleCasedExpiresNames("a=1"_s));
}

TEST(CookieUtil, TitleCaseNormalizesALowercaseMonthEvenThoughItIsAccepted)
{
    // CFNetwork accepts "jan", so this rewrite is not required. It is done anyway because the
    // full name "january" is rejected, and one uniform rule is easier to reason about than a
    // rule that depends on the length of the month name.
    EXPECT_STREQ(titleCased("a=1; Expires=Tue, 05 jan 2027 00:00:00 GMT"_s).utf8().legacyCStringPointer(), "a=1; Expires=Tue, 05 Jan 2027 00:00:00 GMT");
}

TEST(CookieUtil, TitleCaseLeavesTheTimeZoneAndCommentAlone)
{
    // The time zone is already matched case-insensitively, and the parenthesized comment is
    // localized, so neither may be rewritten.
    EXPECT_STREQ(titleCased("a=1; Expires=SUN, 05 JAN 2027 00:00:00 GMT-0800 (PST)"_s).utf8().legacyCStringPointer(), "a=1; Expires=Sun, 05 Jan 2027 00:00:00 GMT-0800 (PST)");
    auto localized = WebCore::CookieUtil::cookieStringWithTitleCasedExpiresNames(u"a=1; Expires=SUN, 05 JAN 2027 00:00:00 GMT-0800 (日本標準時)"_str);
    ASSERT_TRUE(!!localized);
    EXPECT_TRUE(localized->endsWith(u"(日本標準時)"_str));
}

TEST(CookieUtil, TitleCaseIgnoresAnExpiresLikeCookieName)
{
    EXPECT_FALSE(!!WebCore::CookieUtil::cookieStringWithTitleCasedExpiresNames("date_expires=TUE, 05 JAN 2027; path=/"_s));
}

TEST(CookieUtil, TitleCaseUsesTheLastExpiresAttribute)
{
    // RFC 6265 section 5.3: when an attribute repeats, the last occurrence wins.
    EXPECT_STREQ(titleCased("a=1; Expires=SUN, 05 JAN 2027 00:00:00 GMT; Expires=MON, 06 FEB 2028 00:00:00 GMT"_s).utf8().legacyCStringPointer(),
        "a=1; Expires=SUN, 05 JAN 2027 00:00:00 GMT; Expires=Mon, 06 Feb 2028 00:00:00 GMT");
}

TEST(CookieUtil, TitleCaseComposesWithTheMonthBeforeDaySwap)
{
    // A value can carry both defects. The swap runs first and moves the month, so the case pass
    // has to see the swapped string; neither transform alone leaves an accepted date.
    auto swapped = WebCore::CookieUtil::cookieStringWithDayFirstExpires("a=1; Expires=Sun JAN 05 2027 00:00:00 GMT"_s);
    ASSERT_TRUE(!!swapped);
    EXPECT_STREQ(swapped->utf8().legacyCStringPointer(), "a=1; Expires=Sun 05 JAN 2027 00:00:00 GMT");
    EXPECT_STREQ(titleCased(*swapped).utf8().legacyCStringPointer(), "a=1; Expires=Sun 05 Jan 2027 00:00:00 GMT");
}
#endif

// MARK: - CookieUtil::cookieStringWithRecoveredUTF8

#if HAVE(BROKEN_NON_ASCII_COOKIE_PARSER)
TEST(CookieUtil, RecoverUTF8UndoesTheBytePerCodeUnitMapping)
{
    // "test=1春节; path=/" as it arrives from CFNetwork.
    auto received = String::fromLatin1("test=1\xE6\x98\xA5\xE8\x8A\x82; path=/");
    // Precondition: the mangled form really has nothing above U+00FF, which is why a width test
    // cannot be used to detect it.
    ASSERT_TRUE(received.containsOnlyLatin1());

    auto recovered = WebCore::CookieUtil::cookieStringWithRecoveredUTF8(received);
    ASSERT_TRUE(!!recovered);
    EXPECT_STREQ(recovered->utf8().legacyCStringPointer(), "test=1春节; path=/");
}

TEST(CookieUtil, RecoverUTF8LeavesPureASCIIAlone)
{
    EXPECT_FALSE(!!WebCore::CookieUtil::cookieStringWithRecoveredUTF8("test=1; path=/"_s));
    EXPECT_FALSE(!!WebCore::CookieUtil::cookieStringWithRecoveredUTF8(""_s));
}

TEST(CookieUtil, RecoverUTF8LeavesGenuineLatin1Alone)
{
    // 0xE9 alone is "é" in ISO-8859-1 but is not valid UTF-8. Failing the decode is what makes
    // the detection safe rather than a heuristic: a header that really is Latin-1 is untouched.
    auto received = String::fromLatin1("t=caf\xE9; path=/");
    EXPECT_FALSE(!!WebCore::CookieUtil::cookieStringWithRecoveredUTF8(received));
}

TEST(CookieUtil, RecoverUTF8HandlesANonASCIICookieName)
{
    // Four of the six cookies/encoding/charset.html subtests have a non-ASCII NAME, which today
    // drops the cookie entirely rather than truncating it.
    auto received = String::fromLatin1("\xD1\x82\xD0\xB5\xD1\x81\xD1\x82=2");
    auto recovered = WebCore::CookieUtil::cookieStringWithRecoveredUTF8(received);
    ASSERT_TRUE(!!recovered);
    EXPECT_STREQ(recovered->utf8().legacyCStringPointer(), "тест=2");
}

// MARK: - CookieUtil::cookieStringWithNonASCIIReplaced

TEST(CookieUtil, NonASCIIReplacementPreservesShape)
{
    // The stand-in must keep every delimiter, so CFNetwork parses the attributes exactly as it
    // would have for the real string.
    auto placeholder = WebCore::CookieUtil::cookieStringWithNonASCIIReplaced(u"春节回=4家路; Path=/; Secure"_str);
    EXPECT_STREQ(placeholder.utf8().legacyCStringPointer(), "xxx=4xx; Path=/; Secure");
    EXPECT_EQ(placeholder.length(), 23u);
}

TEST(CookieUtil, NonASCIIReplacementPreservesACookieNamePrefix)
{
    // __Host- enforcement is keyed off the name, so the prefix has to survive into the stand-in.
    auto placeholder = WebCore::CookieUtil::cookieStringWithNonASCIIReplaced(u"__Host-春节=1; Path=/; Secure"_str);
    EXPECT_TRUE(placeholder.startsWith("__Host-"_s));
}

TEST(CookieUtil, NonASCIIReplacementDropsAQuoteFromTheName)
{
    // CFNetwork rejects a quoted cookie NAME outright, so a stand-in that kept the quote would
    // fail to parse and the cookie would be lost entirely. The real name is substituted back
    // afterwards with its quotes intact, so nothing is lost by masking it here. This does mean
    // Set-Cookie: "á=b" is accepted where the all-ASCII Set-Cookie: "a=b" is still rejected.
    auto placeholder = WebCore::CookieUtil::cookieStringWithNonASCIIReplaced(u"\"春节回=5家路\"; path=/"_str);
    EXPECT_STREQ(placeholder.utf8().legacyCStringPointer(), "xxxx=5xx\"; path=/");

    auto valueOnly = WebCore::CookieUtil::cookieStringWithNonASCIIReplaced(u"test=\"3春节\"; path=/"_str);
    EXPECT_STREQ(valueOnly.utf8().legacyCStringPointer(), "test=\"3xx\"; path=/");
}

TEST(CookieUtil, NonASCIIReplacementLeavesControlCharactersInPlace)
{
    // Only characters above 0x7F are masked. A control character has to survive into the stand-in
    // so CFNetwork still rejects the string exactly as it would have without this workaround --
    // masking CR/LF would launder a header-injection attempt into an innocuous-looking value that
    // then gets substituted back verbatim.
    auto placeholder = WebCore::CookieUtil::cookieStringWithNonASCIIReplaced(u"a=b\r\nEvil: 1中"_str);
    EXPECT_STREQ(placeholder.utf8().legacyCStringPointer(), "a=b\r\nEvil: 1x");
}

// MARK: - CookieUtil::cookieAttributesContainNonASCII

TEST(CookieUtil, AttributesContainNonASCIIDetectsANonASCIIPath)
{
    // Only the name and value are substituted back after the stand-in is parsed, so a non-ASCII
    // Path would otherwise be stored as "/cafx". Callers decline the repair entirely instead.
    EXPECT_TRUE(WebCore::CookieUtil::cookieAttributesContainNonASCII(u"a=b中; Path=/café"_str));
    EXPECT_TRUE(WebCore::CookieUtil::cookieAttributesContainNonASCII(u"a=b; Domain=exámple.com"_str));
    // Attribute names match case-insensitively, as RFC 6265 section 5.2 requires.
    EXPECT_TRUE(WebCore::CookieUtil::cookieAttributesContainNonASCII(u"a=b中; PATH=/café"_str));
}

TEST(CookieUtil, AttributesContainNonASCIIIgnoresAnUnknownAttribute)
{
    // An attribute CFNetwork does not recognize is discarded during parsing, so non-ASCII there
    // cannot be stored corrupted and must not suppress the repair. This is the shape
    // cookies/encoding/charset.html exercises, and treating it as unsafe loses the cookie.
    EXPECT_FALSE(WebCore::CookieUtil::cookieAttributesContainNonASCII(u"春节回=6家路·春运; 完全手册"_str));
    EXPECT_FALSE(WebCore::CookieUtil::cookieAttributesContainNonASCII(u"a=b; SomeAttr=café"_str));
}

TEST(CookieUtil, AttributesContainNonASCIIIgnoresTheNameAndValue)
{
    // Non-ASCII confined to the name-value pair is precisely what the repair handles.
    EXPECT_FALSE(WebCore::CookieUtil::cookieAttributesContainNonASCII(u"春节回=4家路; Path=/; Secure"_str));
    EXPECT_FALSE(WebCore::CookieUtil::cookieAttributesContainNonASCII(u"春节回=4家路"_str));
    EXPECT_FALSE(WebCore::CookieUtil::cookieAttributesContainNonASCII("a=b; Path=/"_s));
}

// MARK: - CookieUtil::cookieNameAndValue

TEST(CookieUtil, NameAndValueSplitsAtTheFirstEquals)
{
    auto pair = WebCore::CookieUtil::cookieNameAndValue("a=b=c; path=/"_s);
    ASSERT_TRUE(!!pair);
    EXPECT_STREQ(pair->first.toString().utf8().legacyCStringPointer(), "a");
    EXPECT_STREQ(pair->second.toString().utf8().legacyCStringPointer(), "b=c");
}

TEST(CookieUtil, NameAndValueKeepsQuotesAsPartOfTheValue)
{
    // RFC 6265 does not strip quotes, and charset.html expects them retained verbatim.
    // cookieNameAndValue() returns views into its argument, so the string has to outlive them.
    auto cookieString = u"test=\"3春节\"; path=/"_str;
    auto pair = WebCore::CookieUtil::cookieNameAndValue(cookieString);
    ASSERT_TRUE(!!pair);
    EXPECT_STREQ(pair->second.toString().utf8().legacyCStringPointer(), "\"3春节\"");
}

TEST(CookieUtil, NameAndValueStopsAtTheFirstSemicolon)
{
    // charset.html's last subtest relies on the trailing non-ASCII token being an attribute,
    // not part of the value.
    auto cookieString = u"春节回=6家路; 完全手册"_str;
    auto pair = WebCore::CookieUtil::cookieNameAndValue(cookieString);
    ASSERT_TRUE(!!pair);
    EXPECT_STREQ(pair->first.toString().utf8().legacyCStringPointer(), "春节回");
    EXPECT_STREQ(pair->second.toString().utf8().legacyCStringPointer(), "6家路");
}

TEST(CookieUtil, NameAndValueRejectsAStringWithNoEquals)
{
    EXPECT_FALSE(!!WebCore::CookieUtil::cookieNameAndValue("justaname; path=/"_s));
}
#endif

} // namespace TestWebKitAPI
