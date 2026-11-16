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

#include <WebCore/QuirkMatchPattern.h>
#include <array>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

using WebCore::QuirkMatchPattern;
using WebCore::URLMatchContext;

static QuirkMatchPattern parsePattern(ASCIILiteral patternString)
{
    auto pattern = QuirkMatchPattern::parse(patternString);
    EXPECT_TRUE(pattern) << patternString.characters();
    return *pattern;
}

static bool matchesURL(const QuirkMatchPattern& pattern, ASCIILiteral urlString)
{
    return pattern.matches(URLMatchContext { URL { urlString } });
}

TEST(QuirkMatchPatternTest, ExactHostMatchesOnlyThatHost)
{
    auto pattern = parsePattern("*://www.example.com/*"_s);

    EXPECT_TRUE(matchesURL(pattern, "https://www.example.com/"_s));
    EXPECT_TRUE(matchesURL(pattern, "http://www.example.com/some/path?query#fragment"_s));
    EXPECT_TRUE(matchesURL(pattern, "https://WWW.EXAMPLE.COM/"_s));

    EXPECT_FALSE(matchesURL(pattern, "https://example.com/"_s));
    EXPECT_FALSE(matchesURL(pattern, "https://sub.www.example.com/"_s));
    EXPECT_FALSE(matchesURL(pattern, "https://www.example.com.evil.com/"_s));
}

TEST(QuirkMatchPatternTest, SubdomainWildcardMatchesHostAndSubdomains)
{
    auto pattern = parsePattern("*://*.example.com/*"_s);

    EXPECT_TRUE(matchesURL(pattern, "https://example.com/"_s));
    EXPECT_TRUE(matchesURL(pattern, "https://www.example.com/"_s));
    EXPECT_TRUE(matchesURL(pattern, "https://deep.sub.example.com/"_s));

    EXPECT_FALSE(matchesURL(pattern, "https://notexample.com/"_s));
    EXPECT_FALSE(matchesURL(pattern, "https://example.com.evil.com/"_s));
}

TEST(QuirkMatchPatternTest, WildcardSchemeMatchesOnlyHTTPFamily)
{
    auto pattern = parsePattern("*://*/*"_s);

    EXPECT_TRUE(matchesURL(pattern, "https://example.com/"_s));
    EXPECT_TRUE(matchesURL(pattern, "http://example.org/path"_s));

    EXPECT_FALSE(matchesURL(pattern, "ftp://example.com/"_s));
    EXPECT_FALSE(matchesURL(pattern, "about:blank"_s));
    EXPECT_FALSE(matchesURL(pattern, "data:text/plain,hello"_s));
}

TEST(QuirkMatchPatternTest, ExplicitSchemeMatchesOnlyThatScheme)
{
    auto pattern = parsePattern("https://example.com/*"_s);

    EXPECT_TRUE(matchesURL(pattern, "https://example.com/"_s));
    EXPECT_FALSE(matchesURL(pattern, "http://example.com/"_s));
}

TEST(QuirkMatchPatternTest, PathIsAGlobOverThePathAndQuery)
{
    auto prefix = parsePattern("*://example.com/store/*"_s);
    EXPECT_TRUE(matchesURL(prefix, "https://example.com/store/"_s));
    EXPECT_TRUE(matchesURL(prefix, "https://example.com/store/item/42"_s));
    EXPECT_TRUE(matchesURL(prefix, "https://example.com/store/?query"_s));
    EXPECT_FALSE(matchesURL(prefix, "https://example.com/store"_s));
    EXPECT_FALSE(matchesURL(prefix, "https://example.com/storefront"_s));

    auto exact = parsePattern("*://example.com/gp/video/"_s);
    EXPECT_TRUE(matchesURL(exact, "https://example.com/gp/video/"_s));
    EXPECT_TRUE(matchesURL(exact, "https://example.com/gp/video/#fragment"_s));
    EXPECT_FALSE(matchesURL(exact, "https://example.com/gp/video/?query"_s));
    EXPECT_FALSE(matchesURL(exact, "https://example.com/gp/video/detail"_s));

    auto exactWithQuery = parsePattern("*://example.com/gp/video/?*"_s);
    EXPECT_TRUE(matchesURL(exactWithQuery, "https://example.com/gp/video/?query#fragment"_s));
    EXPECT_TRUE(matchesURL(exactWithQuery, "https://example.com/gp/video/?"_s));
    EXPECT_FALSE(matchesURL(exactWithQuery, "https://example.com/gp/video/"_s));

    auto contains = parsePattern("*://*/*/pushdownload.*"_s);
    EXPECT_TRUE(matchesURL(contains, "https://cdn.example.com/a/b/pushdownload.js"_s));
    EXPECT_FALSE(matchesURL(contains, "https://cdn.example.com/pushdownload"_s));

    auto suffix = parsePattern("*://*/*wordeditords.js"_s);
    EXPECT_TRUE(matchesURL(suffix, "https://example.com/scripts/wordeditords.js"_s));
    EXPECT_FALSE(matchesURL(suffix, "https://example.com/scripts/wordeditords.js.map"_s));
    EXPECT_FALSE(matchesURL(suffix, "https://example.com/scripts/wordeditords.js?v=2"_s));

    auto query = parsePattern("*://example.com/*foo*"_s);
    EXPECT_TRUE(matchesURL(query, "https://example.com/?foo"_s));
    EXPECT_FALSE(matchesURL(query, "https://example.com/#foo"_s));

    auto specificQuery = parsePattern("*://example.com/page?id=*"_s);
    EXPECT_TRUE(matchesURL(specificQuery, "https://example.com/page?id=42"_s));
    EXPECT_FALSE(matchesURL(specificQuery, "https://example.com/page?other=1"_s));
}

TEST(QuirkMatchPatternTest, FragmentInPatternNeverMatches)
{
    // The fragment is removed from the URL before matching, so a "#" in the pattern parses but cannot match.
    auto pattern = parsePattern("*://example.com/page#section"_s);
    EXPECT_FALSE(matchesURL(pattern, "https://example.com/page#section"_s));
    EXPECT_FALSE(matchesURL(pattern, "https://example.com/page"_s));

    auto anyPublicSuffix = parsePattern("*://amazon.*/page#section"_s);
    EXPECT_FALSE(matchesURL(anyPublicSuffix, "https://amazon.com/page#section"_s));
}

TEST(QuirkMatchPatternTest, AnyPublicSuffixMatchesRegistrableDomain)
{
    auto pattern = parsePattern("*://amazon.*/*"_s);

    EXPECT_TRUE(matchesURL(pattern, "https://amazon.com/"_s));
    EXPECT_TRUE(matchesURL(pattern, "https://amazon.co.uk/gp/"_s));
    EXPECT_TRUE(matchesURL(pattern, "https://AMAZON.de/"_s));

    EXPECT_FALSE(matchesURL(pattern, "https://www.amazon.com/"_s));
    EXPECT_FALSE(matchesURL(pattern, "https://notamazon.com/"_s));
    EXPECT_FALSE(matchesURL(pattern, "https://amazon.example.com/"_s));
}

TEST(QuirkMatchPatternTest, AnyPublicSuffixWithSubdomainWildcard)
{
    auto pattern = parsePattern("*://*.amazon.*/*"_s);

    EXPECT_TRUE(matchesURL(pattern, "https://amazon.com/"_s));
    EXPECT_TRUE(matchesURL(pattern, "https://www.amazon.com/"_s));
    EXPECT_TRUE(matchesURL(pattern, "https://smile.amazon.co.uk/"_s));

    EXPECT_FALSE(matchesURL(pattern, "https://amazon.example.com/"_s));
    EXPECT_FALSE(matchesURL(pattern, "https://www.notamazon.com/"_s));
    EXPECT_FALSE(matchesURL(pattern, "ftp://www.amazon.com/"_s));
}

TEST(QuirkMatchPatternTest, AnyPublicSuffixRespectsPath)
{
    auto pattern = parsePattern("*://*.apple.*/*/retail*"_s);

    EXPECT_TRUE(matchesURL(pattern, "https://www.apple.com/uk/retail/"_s));
    EXPECT_FALSE(matchesURL(pattern, "https://www.apple.com/uk/store/"_s));
}

TEST(QuirkMatchPatternTest, AnyPublicSuffixDoesNotMatchIPAddresses)
{
    auto pattern = parsePattern("*://*.amazon.*/*"_s);

    EXPECT_FALSE(matchesURL(pattern, "https://127.0.0.1/"_s));
    EXPECT_FALSE(matchesURL(pattern, "https://[::1]/"_s));
}

TEST(QuirkMatchPatternTest, RejectsInvalidPatterns)
{
    EXPECT_FALSE(QuirkMatchPattern::parse(""_s));
    EXPECT_FALSE(QuirkMatchPattern::parse("example.com/*"_s));
    EXPECT_FALSE(QuirkMatchPattern::parse("*://example.com"_s));
    EXPECT_FALSE(QuirkMatchPattern::parse("*:///*"_s));
    EXPECT_FALSE(QuirkMatchPattern::parse("*://www.*.example.com/*"_s));
    EXPECT_FALSE(QuirkMatchPattern::parse("*://example.com:8080/*"_s));
    EXPECT_FALSE(QuirkMatchPattern::parse("*://user@example.com/*"_s));

    // Only lowercase HTTP-family schemes are allowed.
    EXPECT_FALSE(QuirkMatchPattern::parse("HTTPS://example.com/*"_s));
    EXPECT_FALSE(QuirkMatchPattern::parse(" *://example.com/*"_s));
    EXPECT_FALSE(QuirkMatchPattern::parse("ftp://example.com/*"_s));
    EXPECT_FALSE(QuirkMatchPattern::parse("file://localhost/*"_s));
    EXPECT_FALSE(QuirkMatchPattern::parse("example.com/x://foo/"_s));

    EXPECT_FALSE(QuirkMatchPattern::parse("*://.*/*"_s));
    EXPECT_FALSE(QuirkMatchPattern::parse("*://*.*/*"_s));
    EXPECT_FALSE(QuirkMatchPattern::parse("*://www.amazon.*/*"_s));
    EXPECT_FALSE(QuirkMatchPattern::parse("*://*.*.amazon.*/*"_s));
    EXPECT_FALSE(QuirkMatchPattern::parse("*://amazon:80.*/*"_s));
}

TEST(QuirkMatchPatternTest, KeepsItsSourceString)
{
    EXPECT_TRUE(parsePattern("*://*.amazon.*/*"_s).string() == "*://*.amazon.*/*"_s);
    EXPECT_TRUE(parsePattern("*://*.amazon.*/*"_s) == parsePattern("*://*.amazon.*/*"_s));
    EXPECT_FALSE(parsePattern("*://*.amazon.*/*"_s) == parsePattern("*://amazon.*/*"_s));
}

TEST(QuirkMatchPatternTest, SubdomainWildcardUnderstandsMultiLabelPublicSuffixes)
{
    auto pattern = parsePattern("*://*.bbc.co.uk/*"_s);

    EXPECT_TRUE(matchesURL(pattern, "https://www.bbc.co.uk/news"_s));
    EXPECT_TRUE(matchesURL(pattern, "https://bbc.co.uk/"_s));
    EXPECT_FALSE(matchesURL(pattern, "https://bbc.com/"_s));
    EXPECT_FALSE(matchesURL(pattern, "https://co.uk/"_s));
}

TEST(QuirkMatchPatternTest, PortsDoNotAffectMatching)
{
    EXPECT_TRUE(matchesURL(parsePattern("*://*.example.com/*"_s), "http://example.com:8080/"_s));
    EXPECT_TRUE(matchesURL(parsePattern("*://localhost/*"_s), "http://localhost:8080/"_s));
    EXPECT_TRUE(matchesURL(parsePattern("*://127.0.0.1/*"_s), "http://127.0.0.1:8000/"_s));
}

TEST(QuirkMatchPatternTest, URLsWithoutAHostMatchNothing)
{
    for (auto pattern : { "*://*/*"_s, "*://*.example.com/*"_s, "*://*.example.*/*"_s }) {
        for (auto urlString : { "about:blank"_s, "data:text/html,hello"_s, ""_s })
            EXPECT_FALSE(matchesURL(parsePattern(pattern), urlString)) << pattern.characters() << " " << urlString.characters();
    }
}

TEST(QuirkMatchPatternTest, PathComponentPrefixNeedsTwoPatterns)
{
    // A path component and everything under it, but not a longer component with the same prefix.
    std::array patterns { parsePattern("*://*.google.*/maps"_s), parsePattern("*://*.google.*/maps/*"_s) };
    auto matchesEither = [&](ASCIILiteral urlString) {
        return std::ranges::any_of(patterns, [&](auto& pattern) {
            return matchesURL(pattern, urlString);
        });
    };

    EXPECT_TRUE(matchesEither("https://www.google.com/maps"_s));
    EXPECT_TRUE(matchesEither("https://www.google.com/maps/place/1"_s));
    EXPECT_FALSE(matchesEither("https://www.google.com/mapsearch"_s));
}

TEST(QuirkMatchPatternTest, LastPathComponentNeedsFourPatterns)
{
    // A file name at the root or in any directory, but not a longer name ending with it. Each needs a "?*" twin to allow a query.
    std::array patterns {
        parsePattern("*://*/CheckBrowserClose.js"_s), parsePattern("*://*/CheckBrowserClose.js?*"_s),
        parsePattern("*://*/*/CheckBrowserClose.js"_s), parsePattern("*://*/*/CheckBrowserClose.js?*"_s)
    };
    auto matchesEither = [&](ASCIILiteral urlString) {
        return std::ranges::any_of(patterns, [&](auto& pattern) {
            return matchesURL(pattern, urlString);
        });
    };

    EXPECT_TRUE(matchesEither("https://ceac.state.gov/CheckBrowserClose.js"_s));
    EXPECT_TRUE(matchesEither("https://ceac.state.gov/scripts/CheckBrowserClose.js?v=2"_s));
    EXPECT_FALSE(matchesEither("https://ceac.state.gov/scripts/NoCheckBrowserClose.js"_s));
}

TEST(QuirkMatchPatternTest, ContextDerivesValuesFromItsURL)
{
    URL url { "https://www.bbc.co.uk/news?live=1#top"_s };
    URLMatchContext context { url };

    EXPECT_EQ(context.url(), url);
    EXPECT_TRUE(context.host() == "www.bbc.co.uk"_s);
    EXPECT_TRUE(context.registrableDomain() == "bbc.co.uk"_s);
    EXPECT_TRUE(context.domainWithoutPublicSuffix() == "bbc"_s);
}

TEST(QuirkMatchPatternTest, ContextCachesDerivedValues)
{
    URLMatchContext context { URL { "https://www.example.com/"_s } };

    EXPECT_EQ(&context.registrableDomain(), &context.registrableDomain());
    EXPECT_EQ(&context.domainWithoutPublicSuffix(), &context.domainWithoutPublicSuffix());
}

} // namespace TestWebKitAPI
