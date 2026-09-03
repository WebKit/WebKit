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

#include <WebCore/URLMatch.h>
#include <array>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

using WebCore::URLEnvironment;
using WebCore::URLMatch;
using WebCore::URLMatchContext;
using namespace WebCore::URLRefinement;

static bool matchesURL(const URLMatch& match, ASCIILiteral urlString)
{
    return match.matches(URLMatchContext { URL { urlString } });
}

static constexpr std::array expediaGroupDomains { "hotels.com"_s, "orbitz.com"_s, "wotif.co.nz"_s };
static constexpr std::array excludedNaverHosts { "tv.naver.com"_s, "mail.naver.com"_s, "m.naver.com"_s };

TEST(URLMatchTest, DomainMatchesRegistrableDomain)
{
    auto match = URLMatch::domain("example.com"_s);

    EXPECT_TRUE(matchesURL(match, "https://example.com/"_s));
    EXPECT_TRUE(matchesURL(match, "https://www.example.com/"_s));
    EXPECT_TRUE(matchesURL(match, "https://deep.sub.example.com/some/path?query#fragment"_s));
    EXPECT_TRUE(matchesURL(match, "http://example.com:8080/"_s));

    EXPECT_FALSE(matchesURL(match, "https://example.org/"_s));
    EXPECT_FALSE(matchesURL(match, "https://notexample.com/"_s));

    EXPECT_FALSE(matchesURL(match, "https://example.com.evil.com/"_s));
}

TEST(URLMatchTest, DomainUnderstandsMultiLabelPublicSuffixes)
{
    auto match = URLMatch::domain("bbc.co.uk"_s);

    EXPECT_TRUE(matchesURL(match, "https://www.bbc.co.uk/news"_s));
    EXPECT_TRUE(matchesURL(match, "https://bbc.co.uk/"_s));

    EXPECT_FALSE(matchesURL(match, "https://bbc.com/"_s));
}

TEST(URLMatchTest, DomainsMatchesAnyPatternInTheList)
{
    auto match = URLMatch::domain(expediaGroupDomains);

    EXPECT_TRUE(matchesURL(match, "https://www.hotels.com/"_s));
    EXPECT_TRUE(matchesURL(match, "https://orbitz.com/flights"_s));
    EXPECT_TRUE(matchesURL(match, "https://www.wotif.co.nz/"_s));

    EXPECT_FALSE(matchesURL(match, "https://www.expedia.com/"_s));
    EXPECT_FALSE(matchesURL(match, "https://wotif.com/"_s));
}

TEST(URLMatchTest, HostMatchesExactHostOnly)
{
    auto match = URLMatch::host("docs.google.com"_s);

    EXPECT_TRUE(matchesURL(match, "https://docs.google.com/spreadsheets/d/abc"_s));
    EXPECT_TRUE(matchesURL(match, "https://DOCS.GOOGLE.COM/"_s));

    EXPECT_FALSE(matchesURL(match, "https://google.com/"_s));
    EXPECT_FALSE(matchesURL(match, "https://www.docs.google.com/"_s));
    EXPECT_FALSE(matchesURL(match, "https://sheets.docs.google.com/"_s));
}

TEST(URLMatchTest, HostOrSubdomainOfRespectsLabelBoundaries)
{
    auto match = URLMatch::hostOrSubdomainOf("ceac.state.gov"_s);

    EXPECT_TRUE(matchesURL(match, "https://ceac.state.gov/CEAC/"_s));
    EXPECT_TRUE(matchesURL(match, "https://travel.ceac.state.gov/"_s));

    EXPECT_FALSE(matchesURL(match, "https://notceac.state.gov/"_s));
    EXPECT_FALSE(matchesURL(match, "https://state.gov/"_s));

    EXPECT_FALSE(matchesURL(match, "ftp://ceac.state.gov/"_s));
}

TEST(URLMatchTest, HostOrSubdomainOfCoversShardedHosts)
{
    auto match = URLMatch::hostOrSubdomainOf("onedrive.live.com"_s);

    EXPECT_TRUE(matchesURL(match, "https://onedrive.live.com/"_s));
    EXPECT_TRUE(matchesURL(match, "https://p123.onedrive.live.com/"_s));
    EXPECT_TRUE(matchesURL(match, "https://ONEDRIVE.LIVE.COM/"_s));

    EXPECT_FALSE(matchesURL(match, "https://myonedrive.live.com/"_s));
    EXPECT_FALSE(matchesURL(match, "https://live.com/"_s));
    EXPECT_FALSE(matchesURL(match, "https://onedrive.live.com.evil.com/"_s));
}

TEST(URLMatchTest, AnyTopLevelDomainMatchesEveryPublicSuffix)
{
    auto match = URLMatch::anyTopLevelDomain("amazon"_s);

    EXPECT_TRUE(matchesURL(match, "https://www.amazon.com/"_s));
    EXPECT_TRUE(matchesURL(match, "https://www.amazon.co.uk/gp/video/"_s));
    EXPECT_TRUE(matchesURL(match, "https://amazon.de/"_s));
    EXPECT_TRUE(matchesURL(match, "https://smile.amazon.com/"_s));

    EXPECT_FALSE(matchesURL(match, "https://notamazon.com/"_s));
    EXPECT_FALSE(matchesURL(match, "https://amazon.com.evil.com/"_s));

    EXPECT_FALSE(matchesURL(match, "https://amazon.invalidtld/"_s));
}

TEST(URLMatchTest, PathContainsMatchesAnywhereInThePath)
{
    auto match = URLMatch::anyTopLevelDomain("apple"_s).when(pathContains("/retail"_s));

    EXPECT_TRUE(matchesURL(match, "https://www.apple.com/retail/"_s));
    EXPECT_TRUE(matchesURL(match, "https://www.apple.com/us/retail/store"_s));
    EXPECT_TRUE(matchesURL(match, "https://www.apple.co.uk/retail/"_s));

    EXPECT_FALSE(matchesURL(match, "https://www.apple.com/"_s));
    EXPECT_FALSE(matchesURL(match, "https://www.apple.com/RETAIL/"_s));
    EXPECT_FALSE(matchesURL(match, "https://www.apple.com/?section=/retail"_s));
    EXPECT_FALSE(matchesURL(match, "https://www.apple.com/#/retail"_s));

    EXPECT_FALSE(matchesURL(match, "https://www.example.com/retail/"_s));
}

TEST(URLMatchTest, PathStartsWithIsAnchored)
{
    auto match = URLMatch::host("docs.google.com"_s).when(pathStartsWith("/spreadsheets/"_s));

    EXPECT_TRUE(matchesURL(match, "https://docs.google.com/spreadsheets/d/abc/edit"_s));
    EXPECT_TRUE(matchesURL(match, "https://docs.google.com/SpreadSheets/d/abc"_s));

    EXPECT_FALSE(matchesURL(match, "https://docs.google.com/"_s));
    EXPECT_FALSE(matchesURL(match, "https://docs.google.com/spreadsheets"_s));
    EXPECT_FALSE(matchesURL(match, "https://docs.google.com/a/spreadsheets/d/abc"_s));
}

TEST(URLMatchTest, PathIsMatchesTheWholePath)
{
    auto match = URLMatch::host("shopee.sg"_s).when(pathIs("/payment/account-linking/landing"_s));

    EXPECT_TRUE(matchesURL(match, "https://shopee.sg/payment/account-linking/landing"_s));
    EXPECT_TRUE(matchesURL(match, "https://shopee.sg/payment/account-linking/landing?token=abc#top"_s));

    EXPECT_FALSE(matchesURL(match, "https://shopee.sg/payment/account-linking/landing/"_s));
    EXPECT_FALSE(matchesURL(match, "https://shopee.sg/payment/account-linking/landing/step2"_s));
    EXPECT_FALSE(matchesURL(match, "https://shopee.sg/payment/account-linking"_s));
    EXPECT_FALSE(matchesURL(match, "https://shopee.sg/"_s));

    EXPECT_FALSE(matchesURL(match, "https://shopee.sg/Payment/Account-Linking/Landing"_s));
}

TEST(URLMatchTest, PathOrFragmentContainsSearchesBoth)
{
    auto match = URLMatch::domain("icloud.com"_s).when(pathOrFragmentContains("mail"_s));

    EXPECT_TRUE(matchesURL(match, "https://www.icloud.com/mail/"_s));
    EXPECT_TRUE(matchesURL(match, "https://www.icloud.com/#mail"_s));

    EXPECT_FALSE(matchesURL(match, "https://www.icloud.com/"_s));
    EXPECT_FALSE(matchesURL(match, "https://www.icloud.com/notes/"_s));

    EXPECT_FALSE(matchesURL(match, "https://www.icloud.com/?app=mail"_s));
}

TEST(URLMatchTest, QueryContainsSearchesOnlyTheQuery)
{
    auto match = URLMatch::host("teams.microsoft.com"_s).when(queryContains("Retried+3+times+without+success"_s));

    EXPECT_TRUE(matchesURL(match, "https://teams.microsoft.com/?error=Retried+3+times+without+success"_s));
    EXPECT_TRUE(matchesURL(match, "https://teams.microsoft.com/v2/?a=1&msg=Retried+3+times+without+success&b=2"_s));

    EXPECT_FALSE(matchesURL(match, "https://teams.microsoft.com/"_s));
    EXPECT_FALSE(matchesURL(match, "https://teams.microsoft.com/Retried+3+times+without+success"_s));
    EXPECT_FALSE(matchesURL(match, "https://teams.microsoft.com/#Retried+3+times+without+success"_s));
}

TEST(URLMatchTest, QueryContainsStacksWithAPathRefinement)
{
    auto match = URLMatch::host("example.com"_s).when(pathStartsWith("/app/"_s), queryContains("retry"_s));

    EXPECT_TRUE(matchesURL(match, "https://example.com/app/home?retry=1"_s));

    EXPECT_FALSE(matchesURL(match, "https://example.com/app/home"_s));
    EXPECT_FALSE(matchesURL(match, "https://example.com/other?retry=1"_s));
}

TEST(URLMatchTest, EnvironmentIsANDedWithTheSiteMatch)
{
    auto smallScreenOnly = URLMatch::domain("youtube.com"_s).when(smallScreen());

    EXPECT_EQ(matchesURL(smallScreenOnly, "https://www.youtube.com/"_s), WebCore::evaluateURLEnvironment(URLEnvironment::SmallScreen));

    EXPECT_FALSE(matchesURL(smallScreenOnly, "https://www.example.com/"_s));

#if !PLATFORM(IOS_FAMILY)
    EXPECT_FALSE(WebCore::evaluateURLEnvironment(URLEnvironment::SmallScreen));
    EXPECT_FALSE(WebCore::evaluateURLEnvironment(URLEnvironment::TubularApp));
    EXPECT_FALSE(WebCore::evaluateURLEnvironment(URLEnvironment::LensApp));

    EXPECT_FALSE(matchesURL(smallScreenOnly, "https://www.youtube.com/"_s));
#endif
}

TEST(URLMatchTest, AnyURLMatchesEverySiteWithoutFurtherRefinement)
{
    auto match = URLMatch::anyURL();

    EXPECT_TRUE(matchesURL(match, "https://www.example.com/"_s));
    EXPECT_TRUE(matchesURL(match, "https://webkit.org/"_s));
    EXPECT_FALSE(matchesURL(match, "about:blank"_s));
}

TEST(URLMatchTest, ExceptWhenCarvesOutPagesOfAMatchedSite)
{
    auto match = URLMatch::domain("wix.com"_s).exceptWhen(pathStartsWith("/website/templates/"_s));

    EXPECT_TRUE(matchesURL(match, "https://www.wix.com/"_s));
    EXPECT_TRUE(matchesURL(match, "https://www.wix.com/website/other"_s));
    EXPECT_TRUE(matchesURL(match, "https://www.wix.com/x/website/templates/blank"_s));

    EXPECT_FALSE(matchesURL(match, "https://www.wix.com/website/templates/"_s));
    EXPECT_FALSE(matchesURL(match, "https://www.wix.com/website/templates/blank"_s));

    EXPECT_FALSE(matchesURL(match, "https://www.example.com/website/other"_s));
}

TEST(URLMatchTest, ExceptWhenCarvesOutHostsOfAMatchedSite)
{
    auto match = URLMatch::hostOrSubdomainOf("naver.com"_s).exceptWhen(hostIs(excludedNaverHosts));

    EXPECT_TRUE(matchesURL(match, "https://naver.com/"_s));
    EXPECT_TRUE(matchesURL(match, "https://news.naver.com/"_s));

    EXPECT_FALSE(matchesURL(match, "https://tv.naver.com/"_s));
    EXPECT_FALSE(matchesURL(match, "https://m.naver.com/"_s));

    EXPECT_TRUE(matchesURL(match, "https://sub.tv.naver.com/"_s));
}

TEST(URLMatchTest, ExceptWhenCarvesOutASingleHost)
{
    auto match = URLMatch::hostOrSubdomainOf("naver.com"_s).exceptWhen(hostIs("tv.naver.com"_s));

    EXPECT_TRUE(matchesURL(match, "https://naver.com/"_s));
    EXPECT_TRUE(matchesURL(match, "https://news.naver.com/"_s));

    EXPECT_FALSE(matchesURL(match, "https://tv.naver.com/"_s));

    EXPECT_TRUE(matchesURL(match, "https://sub.tv.naver.com/"_s));
}

TEST(URLMatchTest, HostIsNarrowsAMatchToOneHost)
{
    auto match = URLMatch::domain("naver.com"_s).when(hostIs("tv.naver.com"_s));

    EXPECT_TRUE(matchesURL(match, "https://tv.naver.com/"_s));

    EXPECT_FALSE(matchesURL(match, "https://naver.com/"_s));
    EXPECT_FALSE(matchesURL(match, "https://news.naver.com/"_s));
}

TEST(URLMatchTest, ExceptWhenAndTheMatchKeepSeparateRefinements)
{
    auto match = URLMatch::domain("example.com"_s).when(pathStartsWith("/app"_s)).exceptWhen(pathStartsWith("/app/legacy"_s));

    EXPECT_TRUE(matchesURL(match, "https://www.example.com/app/main"_s));
    EXPECT_FALSE(matchesURL(match, "https://www.example.com/other"_s));
    EXPECT_FALSE(matchesURL(match, "https://www.example.com/app/legacy/page"_s));
}

TEST(URLMatchTest, EnvironmentStacksWithAPathRefinement)
{
    auto match = URLMatch::domain("youtube.com"_s).when(pathStartsWith("/shorts/"_s), smallScreen());

    EXPECT_EQ(matchesURL(match, "https://www.youtube.com/shorts/abc"_s), WebCore::evaluateURLEnvironment(URLEnvironment::SmallScreen));
    EXPECT_FALSE(matchesURL(match, "https://www.youtube.com/watch?v=abc"_s));
}

TEST(URLMatchTest, HostsWithoutAPublicSuffixFallBackToTheHost)
{
    EXPECT_TRUE(matchesURL(URLMatch::domain("localhost"_s), "http://localhost:8080/"_s));
    EXPECT_TRUE(matchesURL(URLMatch::domain("127.0.0.1"_s), "http://127.0.0.1/"_s));
    EXPECT_TRUE(matchesURL(URLMatch::anyTopLevelDomain("127.0.0.1"_s), "http://127.0.0.1/"_s));
}

TEST(URLMatchTest, URLsWithoutAHostMatchNothing)
{
    for (auto urlString : { "about:blank"_s, "data:text/html,hello"_s, ""_s }) {
        URL url { urlString };
        URLMatchContext context { url };

        EXPECT_FALSE(URLMatch::domain("example.com"_s).matches(context));
        EXPECT_FALSE(URLMatch::host("example.com"_s).matches(context));
        EXPECT_FALSE(URLMatch::hostOrSubdomainOf("example.com"_s).matches(context));
        EXPECT_FALSE(URLMatch::anyTopLevelDomain("example"_s).matches(context));
    }
}

TEST(URLMatchTest, ContextDerivesValuesFromItsURL)
{
    URL url { "https://www.bbc.co.uk/news?live=1#top"_s };
    URLMatchContext context { url };

    EXPECT_EQ(context.url(), url);
    EXPECT_EQ(context.host(), "www.bbc.co.uk"_s);
    EXPECT_EQ(context.registrableDomain(), "bbc.co.uk"_s);
    EXPECT_EQ(context.domainWithoutPublicSuffix(), "bbc"_s);
}

TEST(URLMatchTest, ContextCachesDerivedValues)
{
    URL url { "https://www.example.com/"_s };
    URLMatchContext context { url };

    EXPECT_EQ(&context.registrableDomain(), &context.registrableDomain());
    EXPECT_EQ(&context.domainWithoutPublicSuffix(), &context.domainWithoutPublicSuffix());
}

} // namespace TestWebKitAPI
