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
#include "TestPageHarness.h"

#include <WebCore/CSSPropertyNames.h>
#include <WebCore/CSSValueKeywords.h>
#include <WebCore/CachedMatchFinder.h>
#include <WebCore/Element.h>
#include <WebCore/FindOptions.h>
#include <WebCore/HTMLCollection.h>
#include <WebCore/ShadowRoot.h>
#include <WebCore/SimpleRange.h>
#include <WebCore/StyledElement.h>
#include <WebCore/TextIterator.h>

namespace TestWebKitAPI {

using namespace WebCore;

TEST(CachedMatchFinder, CountMatchesFindsMatches)
{
    auto testPage = TestPageHarness::create();
    testPage.loadHTML("<p>The quick brown fox jumped over the brown dog.</p>"_s);

    CachedMatchFinder finder(testPage.document());
    auto result = finder.countMatches(std::nullopt, "brown"_s, { });

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 2u);
}

TEST(CachedMatchFinder, CountMatchesReturnsZeroWhenNoMatches)
{
    auto testPage = TestPageHarness::create();
    testPage.loadHTML("<p>The quick brown fox jumped over the lazy dog.</p>"_s);

    CachedMatchFinder finder(testPage.document());
    auto result = finder.countMatches(std::nullopt, "purple"_s, { });

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 0u);
}

TEST(CachedMatchFinder, CountMatchesWithEmptyTargetReturnsZero)
{
    auto testPage = TestPageHarness::create();
    testPage.loadHTML("<p>The quick brown fox jumped over the brown dog.</p>"_s);

    CachedMatchFinder finder(testPage.document());
    auto result = finder.countMatches(std::nullopt, ""_s, { });

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 0u);
}

TEST(CachedMatchFinder, CountMatchesRespectsLimit)
{
    auto testPage = TestPageHarness::create();
    testPage.loadHTML("<p>brown brown brown brown</p>"_s);

    CachedMatchFinder finder(testPage.document());
    auto result = finder.countMatches(std::nullopt, "brown"_s, { }, 2u);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 2u);
}

TEST(CachedMatchFinder, FindMatchesReturnsAllMatchingRanges)
{
    auto testPage = TestPageHarness::create();
    testPage.loadHTML("<p>The quick brown fox jumped over the brown dog.</p>"_s);

    CachedMatchFinder finder(testPage.document());
    auto result = finder.findMatches(std::nullopt, "brown"_s, { });

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), 2u);
    EXPECT_EQ(plainText(result.value()[0]), "brown"_s);
    EXPECT_EQ(plainText(result.value()[1]), "brown"_s);
}

TEST(CachedMatchFinder, FindMatchesReturnsEmptyVectorWhenNoMatches)
{
    auto testPage = TestPageHarness::create();
    testPage.loadHTML("<p>The quick brown fox jumped over the lazy dog.</p>"_s);

    CachedMatchFinder finder(testPage.document());
    auto result = finder.findMatches(std::nullopt, "purple"_s, { });

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().isEmpty());
}

TEST(CachedMatchFinder, FindMatchesRespectsLimit)
{
    auto testPage = TestPageHarness::create();
    testPage.loadHTML("<p>brown brown brown brown</p>"_s);

    CachedMatchFinder finder(testPage.document());
    auto result = finder.findMatches(std::nullopt, "brown"_s, { }, 1u);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 1u);
}

TEST(CachedMatchFinder, FindMatchFromReturnsFirstMatchWhenStartingWithoutReferenceRange)
{
    auto testPage = TestPageHarness::create();
    testPage.loadHTML("<p>The quick brown fox jumped over the brown dog.</p>"_s);

    CachedMatchFinder finder(testPage.document());
    auto result = finder.findMatchFrom(std::nullopt, "brown"_s, { });

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result.value().has_value());
    EXPECT_EQ(plainText(*result.value()), "brown"_s);
}

TEST(CachedMatchFinder, FindMatchFromReturnsNulloptWhenNoMatches)
{
    auto testPage = TestPageHarness::create();
    testPage.loadHTML("<p>The quick brown fox jumped over the lazy dog.</p>"_s);

    CachedMatchFinder finder(testPage.document());
    auto result = finder.findMatchFrom(std::nullopt, "purple"_s, { });

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value().has_value());
}

TEST(CachedMatchFinder, FindMatchFromAdvancesPastReferenceRangeToNextMatch)
{
    auto testPage = TestPageHarness::create();
    testPage.loadHTML("<p>The quick brown fox jumped over the brown dog.</p>"_s);

    CachedMatchFinder finder(testPage.document());
    auto firstMatch = finder.findMatchFrom(std::nullopt, "brown"_s, { });
    ASSERT_TRUE(firstMatch.has_value());
    ASSERT_TRUE(firstMatch.value().has_value());

    auto secondMatch = finder.findMatchFrom(*firstMatch.value(), "brown"_s, { });
    ASSERT_TRUE(secondMatch.has_value());
    ASSERT_TRUE(secondMatch.value().has_value());
    EXPECT_NE(*firstMatch.value(), *secondMatch.value());
    EXPECT_EQ(plainText(*secondMatch.value()), "brown"_s);

    auto thirdMatch = finder.findMatchFrom(*secondMatch.value(), "brown"_s, { });
    ASSERT_TRUE(thirdMatch.has_value());
    EXPECT_FALSE(thirdMatch.value().has_value());
}

TEST(CachedMatchFinder, FindMatchFromWithBackwardsOptionFindsPrecedingMatch)
{
    auto testPage = TestPageHarness::create();
    testPage.loadHTML("<p>The quick brown fox jumped over the brown dog.</p>"_s);

    CachedMatchFinder finder(testPage.document());
    auto lastMatch = finder.findMatchFrom(std::nullopt, "brown"_s, FindOption::Backwards);
    ASSERT_TRUE(lastMatch.has_value());
    ASSERT_TRUE(lastMatch.value().has_value());

    auto precedingMatch = finder.findMatchFrom(*lastMatch.value(), "brown"_s, FindOption::Backwards);
    ASSERT_TRUE(precedingMatch.has_value());
    ASSERT_TRUE(precedingMatch.value().has_value());
    EXPECT_NE(*lastMatch.value(), *precedingMatch.value());

    auto noMoreMatches = finder.findMatchFrom(*precedingMatch.value(), "brown"_s, FindOption::Backwards);
    ASSERT_TRUE(noMoreMatches.has_value());
    EXPECT_FALSE(noMoreMatches.value().has_value());
}

TEST(CachedMatchFinder, FindMatchFromWithoutWrapAroundStopsAtEndOfDocument)
{
    auto testPage = TestPageHarness::create();
    testPage.loadHTML("<p>brown fox, then more brown text.</p>"_s);

    CachedMatchFinder finder(testPage.document());
    auto allMatches = finder.findMatches(std::nullopt, "brown"_s, { });
    ASSERT_TRUE(allMatches.has_value());
    ASSERT_EQ(allMatches.value().size(), 2u);

    auto result = finder.findMatchFrom(allMatches.value().last(), "brown"_s, { });
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value().has_value());
}

TEST(CachedMatchFinder, FindMatchFromWithWrapAroundWrapsToBeginningOfDocument)
{
    auto testPage = TestPageHarness::create();
    testPage.loadHTML("<p>brown fox, then more brown text.</p>"_s);

    CachedMatchFinder finder(testPage.document());
    auto allMatches = finder.findMatches(std::nullopt, "brown"_s, { });
    ASSERT_TRUE(allMatches.has_value());
    ASSERT_EQ(allMatches.value().size(), 2u);

    auto result = finder.findMatchFrom(allMatches.value().last(), "brown"_s, FindOption::WrapAround);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result.value().has_value());
    EXPECT_EQ(*result.value(), allMatches.value().first());
}

TEST(CachedMatchFinder, CaseInsensitiveOptionMatchesRegardlessOfCase)
{
    auto testPage = TestPageHarness::create();
    testPage.loadHTML("<p>Brown paper, brown bag.</p>"_s);

    CachedMatchFinder finder(testPage.document());

    auto caseSensitiveResult = finder.countMatches(std::nullopt, "brown"_s, { });
    ASSERT_TRUE(caseSensitiveResult.has_value());
    EXPECT_EQ(caseSensitiveResult.value(), 1u);

    auto caseInsensitiveResult = finder.countMatches(std::nullopt, "brown"_s, FindOption::CaseInsensitive);
    ASSERT_TRUE(caseInsensitiveResult.has_value());
    EXPECT_EQ(caseInsensitiveResult.value(), 2u);
}

TEST(CachedMatchFinder, AtWordStartsOptionExcludesMidWordMatches)
{
    auto testPage = TestPageHarness::create();
    testPage.loadHTML("<p>unbrown brown</p>"_s);

    CachedMatchFinder finder(testPage.document());

    auto withoutWordStarts = finder.countMatches(std::nullopt, "brown"_s, { });
    ASSERT_TRUE(withoutWordStarts.has_value());
    EXPECT_EQ(withoutWordStarts.value(), 2u);

    auto withWordStarts = finder.countMatches(std::nullopt, "brown"_s, FindOption::AtWordStarts);
    ASSERT_TRUE(withWordStarts.has_value());
    EXPECT_EQ(withWordStarts.value(), 1u);
}

TEST(CachedMatchFinder, TextBufferCacheIsInvalidatedByDOMMutation)
{
    auto testPage = TestPageHarness::create();
    testPage.loadHTML("<p id=\"content\">The quick brown fox jumped over the brown dog.</p>"_s);

    CachedMatchFinder finder(testPage.document());
    auto initialResult = finder.countMatches(std::nullopt, "brown"_s, { });
    ASSERT_TRUE(initialResult.has_value());
    EXPECT_EQ(initialResult.value(), 2u);

    RefPtr content = testPage.getElementById("content"_s);
    ASSERT_TRUE(content);
    auto setInnerHTMLResult = content->setInnerHTML(String { "brown brown brown"_s });
    EXPECT_FALSE(setInnerHTMLResult.hasException());

    auto updatedResult = finder.countMatches(std::nullopt, "brown"_s, { });
    ASSERT_TRUE(updatedResult.has_value());
    EXPECT_EQ(updatedResult.value(), 3u);
}

TEST(CachedMatchFinder, TextBufferCacheIsInvalidatedByStyleChangeWithoutDOMMutation)
{
    auto testPage = TestPageHarness::create();
    testPage.loadHTML("<p id=\"content\" style=\"display: none;\">brown fox</p>"_s);

    CachedMatchFinder finder(testPage.document());
    auto hiddenResult = finder.countMatches(std::nullopt, "brown"_s, { });
    ASSERT_TRUE(hiddenResult.has_value());
    EXPECT_EQ(hiddenResult.value(), 0u);

    RefPtr content = dynamicDowncast<StyledElement>(testPage.getElementById("content"_s));
    ASSERT_TRUE(content);
    content->setInlineStyleProperty(CSSPropertyDisplay, CSSValueBlock);
    testPage.document().updateLayoutIgnorePendingStylesheets();

    auto visibleResult = finder.countMatches(std::nullopt, "brown"_s, { });
    ASSERT_TRUE(visibleResult.has_value());
    EXPECT_EQ(visibleResult.value(), 1u);
}

TEST(CachedMatchFinder, FlatTreeAndLightTreeBufferCachesRemainIndependentAcrossRepeatedSwitches)
{
    auto testPage = TestPageHarness::create();
    testPage.loadHTML("<div id=\"host\"><template shadowrootmode=\"open\"><p>brown fox</p></template></div>"_s);

    RefPtr host = testPage.getElementById("host"_s);
    ASSERT_TRUE(host);
    ASSERT_TRUE(host->shadowRoot());

    CachedMatchFinder finder(testPage.document());

    for (unsigned index = 0; index < 3; ++index) {
        auto flatTreeResult = finder.countMatches(std::nullopt, "brown"_s, { });
        ASSERT_TRUE(flatTreeResult.has_value());
        EXPECT_EQ(flatTreeResult.value(), 1u);

        auto lightTreeResult = finder.countMatches(std::nullopt, "brown"_s, FindOption::DoNotTraverseFlatTree);
        ASSERT_TRUE(lightTreeResult.has_value());
        EXPECT_EQ(lightTreeResult.value(), 0u);
    }
}

TEST(CachedMatchFinder, SearchResultCacheDistinguishesDifferentTargets)
{
    auto testPage = TestPageHarness::create();
    testPage.loadHTML("<p>The quick brown fox jumped over the brown dog.</p>"_s);

    CachedMatchFinder finder(testPage.document());

    auto brownResult = finder.countMatches(std::nullopt, "brown"_s, { });
    ASSERT_TRUE(brownResult.has_value());
    EXPECT_EQ(brownResult.value(), 2u);

    auto foxResult = finder.countMatches(std::nullopt, "fox"_s, { });
    ASSERT_TRUE(foxResult.has_value());
    EXPECT_EQ(foxResult.value(), 1u);

    auto brownResultAgain = finder.countMatches(std::nullopt, "brown"_s, { });
    ASSERT_TRUE(brownResultAgain.has_value());
    EXPECT_EQ(brownResultAgain.value(), 2u);
}

TEST(CachedMatchFinder, SearchResultCacheDistinguishesDifferentOptions)
{
    auto testPage = TestPageHarness::create();
    testPage.loadHTML("<p>Brown paper, brown bag.</p>"_s);

    CachedMatchFinder finder(testPage.document());

    auto caseSensitiveResult = finder.countMatches(std::nullopt, "brown"_s, { });
    ASSERT_TRUE(caseSensitiveResult.has_value());
    EXPECT_EQ(caseSensitiveResult.value(), 1u);

    auto caseInsensitiveResult = finder.countMatches(std::nullopt, "brown"_s, FindOption::CaseInsensitive);
    ASSERT_TRUE(caseInsensitiveResult.has_value());
    EXPECT_EQ(caseInsensitiveResult.value(), 2u);

    auto caseSensitiveResultAgain = finder.countMatches(std::nullopt, "brown"_s, { });
    ASSERT_TRUE(caseSensitiveResultAgain.has_value());
    EXPECT_EQ(caseSensitiveResultAgain.value(), 1u);
}

TEST(CachedMatchFinder, FindMatchFromWithinShadowTreeUsesShadowIncludingAncestorTraversal)
{
    auto testPage = TestPageHarness::create();
    testPage.loadHTML("<div id=\"host\"><template shadowrootmode=\"open\"><p>brown fox, brown dog</p></template></div>"_s);

    RefPtr host = testPage.getElementById("host"_s);
    ASSERT_TRUE(host);
    RefPtr shadowRoot = host->shadowRoot();
    ASSERT_TRUE(shadowRoot);

    RefPtr paragraph = shadowRoot->firstElementChild();
    ASSERT_TRUE(paragraph);
    auto contentsRange = makeRangeSelectingNodeContents(*paragraph);
    SimpleRange referenceRange { contentsRange.start, contentsRange.start };

    CachedMatchFinder finder(testPage.document());
    auto result = finder.findMatchFrom(referenceRange, "brown"_s, FindOption::DoNotTraverseFlatTree);

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result.value().has_value());
    EXPECT_EQ(plainText(*result.value()), "brown"_s);
}

TEST(CachedMatchFinder, OversizedDocumentReturnsCacheUnusableError)
{
    auto testPage = TestPageHarness::create();
    testPage.loadHTML("<p>The quick brown fox jumped over the brown dog.</p>"_s);

    CachedMatchFinder::setMaximumRunCountForTesting(0u);

    CachedMatchFinder finder(testPage.document());
    auto countResult = finder.countMatches(std::nullopt, "brown"_s, { });
    EXPECT_FALSE(countResult.has_value());
    if (!countResult.has_value())
        EXPECT_EQ(countResult.error(), CachedMatchFinder::CacheUnusable::Oversized);

    auto matchesResult = finder.findMatches(std::nullopt, "brown"_s, { });
    EXPECT_FALSE(matchesResult.has_value());
    if (!matchesResult.has_value())
        EXPECT_EQ(matchesResult.error(), CachedMatchFinder::CacheUnusable::Oversized);

    auto matchFromResult = finder.findMatchFrom(std::nullopt, "brown"_s, { });
    EXPECT_FALSE(matchFromResult.has_value());
    if (!matchFromResult.has_value())
        EXPECT_EQ(matchFromResult.error(), CachedMatchFinder::CacheUnusable::Oversized);

    CachedMatchFinder::setMaximumRunCountForTesting(std::nullopt);
}

} // namespace TestWebKitAPI
