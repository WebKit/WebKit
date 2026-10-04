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

#import "config.h"

#if PLATFORM(MAC)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/FindCAPITester.h"
#import "Helpers/cocoa/FindStateSnapshot.h"
#import "Helpers/cocoa/FindTestFixtures.h"
#import "Helpers/cocoa/TestWKWebView.h"

namespace TestWebKitAPI {

class FindCAPI : public ::testing::TestWithParam<SiteIsolation> {
protected:
    static constexpr WKFindOptions caseInsensitiveWrap = kWKFindOptionsCaseInsensitive | kWKFindOptionsWrapAround;
};

INSTANTIATE_TEST_SUITE_P(FindInPage, FindCAPI, testing::Values(SiteIsolation::Off, SiteIsolation::On), [](auto& info) {
    return std::string { info.param == SiteIsolation::On ? "SiteIsolation" : "NoSiteIsolation" };
});

TEST_P(FindCAPI, CountStringMatches)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello hello hello</p>"_s), GetParam() };
    FindCAPITester find { page.webView() };

    EXPECT_EQ(find.countStringMatches(@"hello", kWKFindOptionsCaseInsensitive), 3u);
    EXPECT_EQ(find.countStringMatches(@"hello", kWKFindOptionsCaseInsensitive, 2), static_cast<unsigned>(kWKMoreThanMaximumMatchCount));
}

TEST_P(FindCAPI, FindStringMatchCount)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello hello hello</p>"_s), GetParam() };
    FindCAPITester find { page.webView() };

    auto withoutHighlight = find.findString(@"hello", caseInsensitiveWrap);
    EXPECT_TRUE(withoutHighlight.found);
    EXPECT_EQ(withoutHighlight.matchCount, 1u);

    auto withHighlight = find.findString(@"hello", caseInsensitiveWrap | kWKFindOptionsShowHighlight);
    EXPECT_EQ(withHighlight.matchCount, 3u);
}

TEST_P(FindCAPI, FindStringNoMatch)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello</p>"_s), GetParam() };
    FindCAPITester find { page.webView() };

    EXPECT_FALSE(find.findString(@"goodbye", caseInsensitiveWrap).found);
}

TEST_P(FindCAPI, FindStringMatches)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello hello hello</p>"_s), GetParam() };
    FindCAPITester find { page.webView() };

    auto result = find.findStringMatches(@"hello", kWKFindOptionsCaseInsensitive);
    EXPECT_EQ(result.matchRects.size(), 3u);
    for (auto& rects : result.matchRects)
        EXPECT_EQ(rects.size(), 1u);
    EXPECT_EQ(result.firstIndexAfterSelection, 0);
}

TEST_P(FindCAPI, FindStringMatchesNoMatch)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello</p>"_s), GetParam() };
    FindCAPITester find { page.webView() };

    auto result = find.findStringMatches(@"goodbye", kWKFindOptionsCaseInsensitive);
    EXPECT_TRUE(result.matchRects.isEmpty());
}

TEST_P(FindCAPI, SelectFindMatch)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>one hello two hello</p>"_s), GetParam() };
    FindCAPITester find { page.webView() };

    find.findStringMatches(@"hello", kWKFindOptionsCaseInsensitive);
    find.selectFindMatch(1);
    EXPECT_EQ([[page.webView() objectByEvaluatingJavaScript:@"getSelection().anchorOffset"] intValue], 14);
}

TEST_P(FindCAPI, HideFindUIRemovesIndicator)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello</p>"_s), GetParam() };
    FindCAPITester find { page.webView() };

    find.findString(@"hello", caseInsensitiveWrap | kWKFindOptionsShowFindIndicator);
    find.hideFindUI();
    EXPECT_TRUE(CGRectIsNull(FindStateSnapshot::capture(page.webView()).textIndicatorRect));
}

TEST_P(FindCAPI, CountAcrossCrossOriginFrames)
{
    FindTestPage page { FindTestFixtures::matchesInEveryFrame(), GetParam() };
    FindCAPITester find { page.webView() };

    EXPECT_EQ(find.countStringMatches(@"hello", kWKFindOptionsCaseInsensitive), 4u);
}

TEST_P(FindCAPI, FindStringMatchesAcrossCrossOriginFrames)
{
    FindTestPage page { FindTestFixtures::matchesInEveryFrame(), GetParam() };
    FindCAPITester find { page.webView() };

    auto count = find.findStringMatches(@"hello", kWKFindOptionsCaseInsensitive).matchRects.size();
    if (GetParam() == SiteIsolation::On)
        EXPECT_EQ(count, 1u); // FIXME: findStringMatches is only sent to the main frame's process.
    else
        EXPECT_EQ(count, 4u);
}

TEST_P(FindCAPI, StepAcrossCrossOriginFrames)
{
    FindTestPage page { FindTestFixtures::matchesInEveryFrame(), GetParam() };
    FindCAPITester find { page.webView() };

    Vector<Vector<unsigned>> expectedFrames { { }, { 0 }, { 1 }, { 2 }, { } };
    for (size_t i = 0; i < expectedFrames.size(); ++i) {
        EXPECT_TRUE(find.findString(@"hello", caseInsensitiveWrap).found);
        EXPECT_EQ(FindStateSnapshot::capture(page.webView()).framesWithSelection(), Vector<Vector<unsigned>> { expectedFrames[i] }) << "step " << i;
    }
}

// FIXME: With site isolation, findStringMatches never indexes the child frame's match, and
// selectFindMatch and indicateFindMatch are only sent to the main frame's process.
TEST_P(FindCAPI, SelectFindMatchInCrossOriginFrame)
{
    FindTestPage page { FindTestFixtures::crossOriginChild(), GetParam() };
    FindCAPITester find { page.webView() };

    auto matchCount = find.findStringMatches(@"hello", kWKFindOptionsCaseInsensitive).matchRects.size();
    find.selectFindMatch(1);

    auto snapshot = FindStateSnapshot::capture(page.webView());
    if (GetParam() == SiteIsolation::On) {
        EXPECT_EQ(matchCount, 1u);
        EXPECT_WK_STREQ("", snapshot.frame({ 0 }).selectedText);
    } else {
        EXPECT_EQ(matchCount, 2u);
        EXPECT_WK_STREQ("hello", snapshot.frame({ 0 }).selectedText);
    }
}

// FIXME: With site isolation, findStringMatches never indexes the child frame's match, and
// selectFindMatch and indicateFindMatch are only sent to the main frame's process.
TEST_P(FindCAPI, IndicateFindMatchInCrossOriginFrame)
{
    FindTestPage page { FindTestFixtures::crossOriginChild(), GetParam() };
    FindCAPITester find { page.webView() };

    find.findStringMatches(@"hello", kWKFindOptionsCaseInsensitive);
    EXPECT_TRUE(CGRectIsNull(FindStateSnapshot::capture(page.webView()).textIndicatorRect));
    find.indicateFindMatch(1);

    auto snapshot = FindStateSnapshot::capture(page.webView());
    if (GetParam() == SiteIsolation::On) {
        EXPECT_WK_STREQ("", snapshot.frame({ 0 }).selectedText);
        EXPECT_TRUE(CGRectIsNull(snapshot.textIndicatorRect));
    } else {
        EXPECT_WK_STREQ("hello", snapshot.frame({ 0 }).selectedText);
        EXPECT_FALSE(CGRectIsNull(snapshot.textIndicatorRect));
    }
}

// FIXME: With site isolation, findStringMatches never indexes the child frame's match, and
// getImageForFindMatch is only sent to the main frame's process.
TEST_P(FindCAPI, GetImageForFindMatchInCrossOriginFrame)
{
    FindTestPage page { FindTestFixtures::crossOriginChild(), GetParam() };
    FindCAPITester find { page.webView() };

    auto matchCount = find.findStringMatches(@"hello", kWKFindOptionsCaseInsensitive).matchRects.size();
    auto image = find.getImageForFindMatch(1);

    if (GetParam() == SiteIsolation::On) {
        EXPECT_EQ(matchCount, 1u);
        EXPECT_NULL(image.get());
    } else {
        EXPECT_EQ(matchCount, 2u);
        EXPECT_NOT_NULL(image.get());
    }
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
