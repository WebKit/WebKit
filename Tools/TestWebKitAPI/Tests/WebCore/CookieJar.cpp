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

// MARK: - CookieUtil::cookieHeaderNeedsRepair

#if HAVE(BROKEN_COOKIE_DATE_PARSER)
TEST(CookieUtil, NeedsRepairRejectsAHeaderWithNoExpires)
{
    EXPECT_FALSE(WebCore::CookieUtil::cookieHeaderNeedsRepair("a=1; Path=/; Secure; HttpOnly"_s));
    EXPECT_FALSE(WebCore::CookieUtil::cookieHeaderNeedsRepair("a=1; Max-Age=3600"_s));
    EXPECT_FALSE(WebCore::CookieUtil::cookieHeaderNeedsRepair("a=1"_s));
    EXPECT_FALSE(WebCore::CookieUtil::cookieHeaderNeedsRepair(""_s));
    // UTF-8 bytes arrive one code unit per byte, which is the form script-written cookies are stored in too.
    EXPECT_FALSE(WebCore::CookieUtil::cookieHeaderNeedsRepair(String::fromLatin1("a=\xE5\x8C\x97")));
}

TEST(CookieUtil, NeedsRepairRejectsAWellFormedDatedHeader)
{
    // Most persistent cookies carry an Expires, so these must still cost no policy work.
    EXPECT_FALSE(WebCore::CookieUtil::cookieHeaderNeedsRepair("a=1; Expires=Tue, 05 Jan 2027 00:00:00 GMT; Path=/"_s));
    EXPECT_FALSE(WebCore::CookieUtil::cookieHeaderNeedsRepair("a=1; Expires=Tue, 05 Jan 2027 00:00:00 GMT, b=2; Expires=Wed, 06-Jan-2027 00:00:00 GMT"_s));
}

TEST(CookieUtil, NeedsRepairAcceptsEachDefect)
{
    EXPECT_TRUE(WebCore::CookieUtil::cookieHeaderNeedsRepair("a=1; Expires=Sun Jan 05 2027 00:00:00 GMT"_s));
    EXPECT_TRUE(WebCore::CookieUtil::cookieHeaderNeedsRepair("a=1; EXPIRES=Sun, 05 JAN 2027 00:00:00 GMT"_s));
    // Only the second of two coalesced headers needs it.
    EXPECT_TRUE(WebCore::CookieUtil::cookieHeaderNeedsRepair("a=1; Path=/, b=2; Expires=Sun Jan 05 2027 00:00:00 GMT"_s));
}
#endif

// MARK: - CookieUtil::splitCoalescedSetCookieHeader

#if HAVE(BROKEN_COOKIE_DATE_PARSER)
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

TEST(CookieUtil, RepairedExpiresComposesBothRepairsInOnePass)
{
    // The Expires range is found once, before the swap, and reused for the case pass.
    auto repaired = WebCore::CookieUtil::cookieStringWithRepairedExpires("a=1; Path=/; Expires=sun JAN 05 2027 00:00:00 GMT; Secure"_s);
    ASSERT_TRUE(!!repaired);
    EXPECT_STREQ(repaired->utf8().legacyCStringPointer(), "a=1; Path=/; Expires=Sun 05 Jan 2027 00:00:00 GMT; Secure");

    auto swapOnly = WebCore::CookieUtil::cookieStringWithRepairedExpires("a=1; Expires=Sun Jan 05 2027 00:00:00 GMT"_s);
    ASSERT_TRUE(!!swapOnly);
    EXPECT_STREQ(swapOnly->utf8().legacyCStringPointer(), "a=1; Expires=Sun 05 Jan 2027 00:00:00 GMT");

    EXPECT_FALSE(!!WebCore::CookieUtil::cookieStringWithRepairedExpires("a=1; Expires=Sun, 05 Jan 2027 00:00:00 GMT"_s));
    EXPECT_FALSE(!!WebCore::CookieUtil::cookieStringWithRepairedExpires("a=1; Path=/"_s));
}
#endif

// MARK: - CookieUtil::cookieNameAndValue

#if HAVE(BROKEN_COOKIE_DATE_PARSER)
TEST(CookieUtil, NameAndValueSplitsAtTheFirstEquals)
{
    auto pair = WebCore::CookieUtil::cookieNameAndValue("a=b=c; path=/"_s);
    ASSERT_TRUE(!!pair);
    EXPECT_STREQ(pair->first.toString().utf8().legacyCStringPointer(), "a");
    EXPECT_STREQ(pair->second.toString().utf8().legacyCStringPointer(), "b=c");
}

TEST(CookieUtil, NameAndValueKeepsQuotesAsPartOfTheValue)
{
    // RFC 6265 does not strip quotes, so the stored value keeps them and the lookup has to as well.
    // cookieNameAndValue() returns views into its argument, so the string has to outlive them.
    auto cookieString = u"test=\"3春节\"; path=/"_str;
    auto pair = WebCore::CookieUtil::cookieNameAndValue(cookieString);
    ASSERT_TRUE(!!pair);
    EXPECT_STREQ(pair->second.toString().utf8().legacyCStringPointer(), "\"3春节\"");
}

TEST(CookieUtil, NameAndValueStopsAtTheFirstSemicolon)
{
    // A trailing token without an equals sign is an attribute, not part of the value.
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
