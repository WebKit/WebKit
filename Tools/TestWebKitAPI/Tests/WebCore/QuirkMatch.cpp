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

#include <WebCore/QuirkMatch.h>
#include <array>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

using WebCore::QuirkEnvironment;
using WebCore::QuirkMatch;
using WebCore::QuirkMatchContext;
using namespace WebCore::QuirkRefinement;

static bool matchesURL(const QuirkMatch& match, ASCIILiteral urlString)
{
    return match.matches(QuirkMatchContext { URL { urlString } });
}

static constexpr std::array expediaGroupDomains { "hotels.com"_s, "orbitz.com"_s, "wotif.co.nz"_s };
static constexpr std::array excludedNaverHosts { "tv.naver.com"_s, "mail.naver.com"_s, "m.naver.com"_s };

TEST(QuirkMatchTest, DomainMatchesRegistrableDomain)
{
    auto match = QuirkMatch::domain("example.com"_s);

    EXPECT_TRUE(matchesURL(match, "https://example.com/"_s));
    EXPECT_TRUE(matchesURL(match, "https://www.example.com/"_s));
    EXPECT_TRUE(matchesURL(match, "https://deep.sub.example.com/some/path?query#fragment"_s));
    EXPECT_TRUE(matchesURL(match, "http://example.com:8080/"_s));

    EXPECT_FALSE(matchesURL(match, "https://example.org/"_s));
    EXPECT_FALSE(matchesURL(match, "https://notexample.com/"_s));

    EXPECT_FALSE(matchesURL(match, "https://example.com.evil.com/"_s));
}

TEST(QuirkMatchTest, DomainUnderstandsMultiLabelPublicSuffixes)
{
    auto match = QuirkMatch::domain("bbc.co.uk"_s);

    EXPECT_TRUE(matchesURL(match, "https://www.bbc.co.uk/news"_s));
    EXPECT_TRUE(matchesURL(match, "https://bbc.co.uk/"_s));

    EXPECT_FALSE(matchesURL(match, "https://bbc.com/"_s));
}

TEST(QuirkMatchTest, DomainsMatchesAnyPatternInTheList)
{
    auto match = QuirkMatch::domain(expediaGroupDomains);

    EXPECT_TRUE(matchesURL(match, "https://www.hotels.com/"_s));
    EXPECT_TRUE(matchesURL(match, "https://orbitz.com/flights"_s));
    EXPECT_TRUE(matchesURL(match, "https://www.wotif.co.nz/"_s));

    EXPECT_FALSE(matchesURL(match, "https://www.expedia.com/"_s));
    EXPECT_FALSE(matchesURL(match, "https://wotif.com/"_s));
}

TEST(QuirkMatchTest, HostMatchesExactHostOnly)
{
    auto match = QuirkMatch::host("docs.google.com"_s);

    EXPECT_TRUE(matchesURL(match, "https://docs.google.com/spreadsheets/d/abc"_s));
    EXPECT_TRUE(matchesURL(match, "https://DOCS.GOOGLE.COM/"_s));

    EXPECT_FALSE(matchesURL(match, "https://google.com/"_s));
    EXPECT_FALSE(matchesURL(match, "https://www.docs.google.com/"_s));
    EXPECT_FALSE(matchesURL(match, "https://sheets.docs.google.com/"_s));
}

TEST(QuirkMatchTest, HostOrSubdomainOfRespectsLabelBoundaries)
{
    auto match = QuirkMatch::hostOrSubdomainOf("ceac.state.gov"_s);

    EXPECT_TRUE(matchesURL(match, "https://ceac.state.gov/CEAC/"_s));
    EXPECT_TRUE(matchesURL(match, "https://travel.ceac.state.gov/"_s));

    EXPECT_FALSE(matchesURL(match, "https://notceac.state.gov/"_s));
    EXPECT_FALSE(matchesURL(match, "https://state.gov/"_s));

    EXPECT_FALSE(matchesURL(match, "ftp://ceac.state.gov/"_s));
}

TEST(QuirkMatchTest, HostOrSubdomainOfCoversShardedHosts)
{
    auto match = QuirkMatch::hostOrSubdomainOf("onedrive.live.com"_s);

    EXPECT_TRUE(matchesURL(match, "https://onedrive.live.com/"_s));
    EXPECT_TRUE(matchesURL(match, "https://p123.onedrive.live.com/"_s));
    EXPECT_TRUE(matchesURL(match, "https://ONEDRIVE.LIVE.COM/"_s));

    EXPECT_FALSE(matchesURL(match, "https://myonedrive.live.com/"_s));
    EXPECT_FALSE(matchesURL(match, "https://live.com/"_s));
    EXPECT_FALSE(matchesURL(match, "https://onedrive.live.com.evil.com/"_s));
}

TEST(QuirkMatchTest, AnyTopLevelDomainMatchesEveryPublicSuffix)
{
    auto match = QuirkMatch::anyTopLevelDomain("amazon"_s);

    EXPECT_TRUE(matchesURL(match, "https://www.amazon.com/"_s));
    EXPECT_TRUE(matchesURL(match, "https://www.amazon.co.uk/gp/video/"_s));
    EXPECT_TRUE(matchesURL(match, "https://amazon.de/"_s));
    EXPECT_TRUE(matchesURL(match, "https://smile.amazon.com/"_s));

    EXPECT_FALSE(matchesURL(match, "https://notamazon.com/"_s));
    EXPECT_FALSE(matchesURL(match, "https://amazon.com.evil.com/"_s));

    EXPECT_FALSE(matchesURL(match, "https://amazon.invalidtld/"_s));
}

TEST(QuirkMatchTest, PathContainsMatchesAnywhereInThePath)
{
    auto match = QuirkMatch::anyTopLevelDomain("apple"_s).when(pathContains("/retail"_s));

    EXPECT_TRUE(matchesURL(match, "https://www.apple.com/retail/"_s));
    EXPECT_TRUE(matchesURL(match, "https://www.apple.com/us/retail/store"_s));
    EXPECT_TRUE(matchesURL(match, "https://www.apple.co.uk/retail/"_s));

    EXPECT_FALSE(matchesURL(match, "https://www.apple.com/"_s));
    EXPECT_FALSE(matchesURL(match, "https://www.apple.com/RETAIL/"_s));
    EXPECT_FALSE(matchesURL(match, "https://www.apple.com/?section=/retail"_s));
    EXPECT_FALSE(matchesURL(match, "https://www.apple.com/#/retail"_s));

    EXPECT_FALSE(matchesURL(match, "https://www.example.com/retail/"_s));
}

TEST(QuirkMatchTest, PathStartsWithIsAnchored)
{
    auto match = QuirkMatch::host("docs.google.com"_s).when(pathStartsWith("/spreadsheets/"_s));

    EXPECT_TRUE(matchesURL(match, "https://docs.google.com/spreadsheets/d/abc/edit"_s));
    EXPECT_TRUE(matchesURL(match, "https://docs.google.com/SpreadSheets/d/abc"_s));

    EXPECT_FALSE(matchesURL(match, "https://docs.google.com/"_s));
    EXPECT_FALSE(matchesURL(match, "https://docs.google.com/spreadsheets"_s));
    EXPECT_FALSE(matchesURL(match, "https://docs.google.com/a/spreadsheets/d/abc"_s));
}

TEST(QuirkMatchTest, PathOrFragmentContainsSearchesBoth)
{
    auto match = QuirkMatch::domain("icloud.com"_s).when(pathOrFragmentContains("mail"_s));

    EXPECT_TRUE(matchesURL(match, "https://www.icloud.com/mail/"_s));
    EXPECT_TRUE(matchesURL(match, "https://www.icloud.com/#mail"_s));

    EXPECT_FALSE(matchesURL(match, "https://www.icloud.com/"_s));
    EXPECT_FALSE(matchesURL(match, "https://www.icloud.com/notes/"_s));

    EXPECT_FALSE(matchesURL(match, "https://www.icloud.com/?app=mail"_s));
}

TEST(QuirkMatchTest, LastPathComponentRefinementsAreAnchoredToTheWholeComponent)
{
    auto exact = QuirkMatch::anyURL().when(lastPathComponentIs("CheckBrowserClose.js"_s));

    EXPECT_TRUE(matchesURL(exact, "https://ceac.state.gov/js/CheckBrowserClose.js"_s));

    EXPECT_FALSE(matchesURL(exact, "https://ceac.state.gov/js/CheckBrowserClose.js.map"_s));
    EXPECT_FALSE(matchesURL(exact, "https://ceac.state.gov/CheckBrowserClose.js/inner.js"_s));

    auto prefix = QuirkMatch::anyURL().when(lastPathComponentStartsWith("pushdownload."_s));

    EXPECT_TRUE(matchesURL(prefix, "https://www.webex.com/x/pushdownload.min.js"_s));
    EXPECT_FALSE(matchesURL(prefix, "https://www.webex.com/pushdownload./x.js"_s));

    auto suffix = QuirkMatch::anyURL().when(lastPathComponentEndsWith("lre.js"_s));

    EXPECT_TRUE(matchesURL(suffix, "https://player.anyclip.com/foo-lre.js"_s));
    EXPECT_FALSE(matchesURL(suffix, "https://player.anyclip.com/lre.js/foo.js"_s));
}

TEST(QuirkMatchTest, EnvironmentIsANDedWithTheSiteMatch)
{
    auto smallScreenOnly = QuirkMatch::domain("youtube.com"_s).when(smallScreen());

    EXPECT_EQ(matchesURL(smallScreenOnly, "https://www.youtube.com/"_s), WebCore::evaluateQuirkEnvironment(QuirkEnvironment::SmallScreen));

    EXPECT_FALSE(matchesURL(smallScreenOnly, "https://www.example.com/"_s));

#if !PLATFORM(IOS_FAMILY)
    EXPECT_FALSE(WebCore::evaluateQuirkEnvironment(QuirkEnvironment::SmallScreen));
    EXPECT_FALSE(WebCore::evaluateQuirkEnvironment(QuirkEnvironment::TubularApp));
    EXPECT_FALSE(WebCore::evaluateQuirkEnvironment(QuirkEnvironment::LensApp));

    EXPECT_FALSE(matchesURL(smallScreenOnly, "https://www.youtube.com/"_s));
#endif
}

TEST(QuirkMatchTest, AnyURLMatchesEveryURLWithAHostWithoutFurtherRefinement)
{
    auto match = QuirkMatch::anyURL();

    EXPECT_TRUE(matchesURL(match, "https://www.example.com/"_s));
    EXPECT_TRUE(matchesURL(match, "https://webkit.org/"_s));
    EXPECT_FALSE(matchesURL(match, "about:blank"_s));
}

TEST(QuirkMatchTest, ExceptWhenCarvesOutPagesOfAMatchedSite)
{
    auto match = QuirkMatch::domain("wix.com"_s).exceptWhen(pathStartsWith("/website/templates/"_s));

    EXPECT_TRUE(matchesURL(match, "https://www.wix.com/"_s));
    EXPECT_TRUE(matchesURL(match, "https://www.wix.com/website/other"_s));
    // The exception reuses pathStartsWith(), so it is anchored the same way.
    EXPECT_TRUE(matchesURL(match, "https://www.wix.com/x/website/templates/blank"_s));

    EXPECT_FALSE(matchesURL(match, "https://www.wix.com/website/templates/"_s));
    EXPECT_FALSE(matchesURL(match, "https://www.wix.com/website/templates/blank"_s));

    // The exception only ever narrows: a page it describes on another site is still no match.
    EXPECT_FALSE(matchesURL(match, "https://www.example.com/website/other"_s));
}

TEST(QuirkMatchTest, ExceptWhenCarvesOutHostsOfAMatchedSite)
{
    auto match = QuirkMatch::hostOrSubdomainOf("naver.com"_s).exceptWhen(hostIs(excludedNaverHosts));

    EXPECT_TRUE(matchesURL(match, "https://naver.com/"_s));
    EXPECT_TRUE(matchesURL(match, "https://news.naver.com/"_s));

    EXPECT_FALSE(matchesURL(match, "https://tv.naver.com/"_s));
    EXPECT_FALSE(matchesURL(match, "https://m.naver.com/"_s));

    EXPECT_TRUE(matchesURL(match, "https://sub.tv.naver.com/"_s));
}

TEST(QuirkMatchTest, ExceptWhenCarvesOutASingleHost)
{
    auto match = QuirkMatch::hostOrSubdomainOf("naver.com"_s).exceptWhen(hostIs("tv.naver.com"_s));

    EXPECT_TRUE(matchesURL(match, "https://naver.com/"_s));
    EXPECT_TRUE(matchesURL(match, "https://news.naver.com/"_s));

    EXPECT_FALSE(matchesURL(match, "https://tv.naver.com/"_s));

    EXPECT_TRUE(matchesURL(match, "https://sub.tv.naver.com/"_s));
}

TEST(QuirkMatchTest, HostIsNarrowsAMatchToOneHost)
{
    auto match = QuirkMatch::domain("naver.com"_s).when(hostIs("tv.naver.com"_s));

    EXPECT_TRUE(matchesURL(match, "https://tv.naver.com/"_s));

    EXPECT_FALSE(matchesURL(match, "https://naver.com/"_s));
    EXPECT_FALSE(matchesURL(match, "https://news.naver.com/"_s));
}

TEST(QuirkMatchTest, ExceptWhenAndTheMatchKeepSeparateRefinements)
{
    auto match = QuirkMatch::domain("example.com"_s).when(pathStartsWith("/app"_s)).exceptWhen(pathStartsWith("/app/legacy"_s));

    EXPECT_TRUE(matchesURL(match, "https://www.example.com/app/main"_s));
    EXPECT_FALSE(matchesURL(match, "https://www.example.com/other"_s));
    EXPECT_FALSE(matchesURL(match, "https://www.example.com/app/legacy/page"_s));
}

TEST(QuirkMatchTest, ExceptWhenRequiresEveryRefinementToExclude)
{
    auto match = QuirkMatch::domain("example.com"_s).exceptWhen(pathStartsWith("/embed"_s), hostIs("legacy.example.com"_s));

    EXPECT_FALSE(matchesURL(match, "https://legacy.example.com/embed/abc"_s));

    // Neither half of the exception excludes on its own.
    EXPECT_TRUE(matchesURL(match, "https://www.example.com/embed/abc"_s));
    EXPECT_TRUE(matchesURL(match, "https://legacy.example.com/other"_s));

    EXPECT_FALSE(matchesURL(match, "https://webkit.org/"_s));
}

TEST(QuirkMatchTest, RefinementsOfDifferentKindsAreAllANDed)
{
    auto match = QuirkMatch::anyTopLevelDomain("theguardian"_s).when(pathStartsWith("/film/"_s), hostIs("www.theguardian.com"_s));

    EXPECT_TRUE(matchesURL(match, "https://www.theguardian.com/film/2026/trailer"_s));

    EXPECT_FALSE(matchesURL(match, "https://www.example.com/film/2026/trailer"_s));
    EXPECT_FALSE(matchesURL(match, "https://www.theguardian.com/news/2026/story"_s));
    EXPECT_FALSE(matchesURL(match, "https://amp.theguardian.com/film/2026/trailer"_s));
}

TEST(QuirkMatchTest, EnvironmentStacksWithAPathRefinement)
{
    auto match = QuirkMatch::domain("youtube.com"_s).when(pathStartsWith("/shorts/"_s), smallScreen());

    EXPECT_EQ(matchesURL(match, "https://www.youtube.com/shorts/abc"_s), WebCore::evaluateQuirkEnvironment(QuirkEnvironment::SmallScreen));
    EXPECT_FALSE(matchesURL(match, "https://www.youtube.com/watch?v=abc"_s));
}

TEST(QuirkMatchTest, ContextDerivesValuesFromTheRightURL)
{
    URL url { "https://www.bbc.co.uk/news?live=1#top"_s };
    QuirkMatchContext context { url };

    EXPECT_EQ(context.url(), url);
    EXPECT_EQ(context.host(), "www.bbc.co.uk"_s);
    EXPECT_EQ(context.registrableDomain(), "bbc.co.uk"_s);
    EXPECT_EQ(context.domainWithoutPublicSuffix(), "bbc"_s);
}

TEST(QuirkMatchTest, ContextCachesDerivedValues)
{
    URL url { "https://www.example.com/"_s };
    QuirkMatchContext context { url };

    EXPECT_EQ(&context.registrableDomain(), &context.registrableDomain());
    EXPECT_EQ(&context.domainWithoutPublicSuffix(), &context.domainWithoutPublicSuffix());
}

TEST(QuirkMatchTest, HostsWithoutAPublicSuffixFallBackToTheHost)
{
    EXPECT_TRUE(matchesURL(QuirkMatch::domain("localhost"_s), "http://localhost:8080/"_s));
    EXPECT_TRUE(matchesURL(QuirkMatch::domain("127.0.0.1"_s), "http://127.0.0.1/"_s));
    EXPECT_TRUE(matchesURL(QuirkMatch::anyTopLevelDomain("127.0.0.1"_s), "http://127.0.0.1/"_s));
}

TEST(QuirkMatchTest, URLsWithoutAHostMatchNothing)
{
    for (auto urlString : { "about:blank"_s, "data:text/html,hello"_s, ""_s }) {
        QuirkMatchContext context { URL { urlString } };

        EXPECT_FALSE(QuirkMatch::domain("example.com"_s).matches(context));
        EXPECT_FALSE(QuirkMatch::host("example.com"_s).matches(context));
        EXPECT_FALSE(QuirkMatch::hostOrSubdomainOf("example.com"_s).matches(context));
        EXPECT_FALSE(QuirkMatch::anyTopLevelDomain("example"_s).matches(context));
    }
}

} // namespace TestWebKitAPI
