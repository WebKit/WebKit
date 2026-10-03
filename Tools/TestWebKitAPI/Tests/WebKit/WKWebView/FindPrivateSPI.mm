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

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/FindStateSnapshot.h"
#import "Helpers/cocoa/FindTestFixtures.h"
#import "Helpers/cocoa/PDFTestHelpers.h"
#import "Helpers/cocoa/PrivateFindSPITester.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>

namespace TestWebKitAPI {

class FindPrivateSPI : public ::testing::TestWithParam<SiteIsolation> {
protected:
    static constexpr auto stepOptions = static_cast<_WKFindOptions>(_WKFindOptionsCaseInsensitive | _WKFindOptionsWrapAround | _WKFindOptionsDetermineMatchIndex);
    static constexpr auto stepBackwardsOptions = static_cast<_WKFindOptions>(stepOptions | _WKFindOptionsBackwards);
};

INSTANTIATE_TEST_SUITE_P(FindInPage, FindPrivateSPI, testing::Values(SiteIsolation::Off, SiteIsolation::On), [](auto& info) {
    return std::string { info.param == SiteIsolation::On ? "SiteIsolation" : "NoSiteIsolation" };
});

TEST_P(FindPrivateSPI, CountStringMatches)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello hello hello</p>"_s), GetParam() };
    PrivateFindSPITester find { page.webView() };

    EXPECT_EQ(find.countStringMatches(@"hello", _WKFindOptionsCaseInsensitive), 3u);
    EXPECT_EQ(find.countStringMatches(@"goodbye", _WKFindOptionsCaseInsensitive), 0u);
}

TEST_P(FindPrivateSPI, CountStringMatchesOverMaxCount)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello hello hello</p>"_s), GetParam() };
    PrivateFindSPITester find { page.webView() };

    EXPECT_EQ(find.countStringMatches(@"hello", _WKFindOptionsCaseInsensitive, 2), static_cast<unsigned>(-1));
}

TEST_P(FindPrivateSPI, StepForwardAndBackward)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello hello hello</p>"_s), GetParam() };
    PrivateFindSPITester find { page.webView() };

    for (int expectedIndex : { 0, 1, 2, 0 }) {
        auto result = find.findString(@"hello", stepOptions);
        EXPECT_TRUE(result.found);
        EXPECT_EQ(result.matchCount, 3u);
        EXPECT_EQ(result.matchIndex, expectedIndex);
    }
    for (int expectedIndex : { 2, 1 }) {
        auto result = find.findString(@"hello", stepBackwardsOptions);
        EXPECT_EQ(result.matchIndex, expectedIndex);
    }
}

TEST_P(FindPrivateSPI, StepWithoutWrapFailsAtEnd)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello hello</p>"_s), GetParam() };
    PrivateFindSPITester find { page.webView() };

    auto noWrap = static_cast<_WKFindOptions>(stepOptions & ~_WKFindOptionsWrapAround);
    EXPECT_TRUE(find.findString(@"hello", noWrap).found);
    EXPECT_TRUE(find.findString(@"hello", noWrap).found);
    EXPECT_FALSE(find.findString(@"hello", noWrap).found);
    EXPECT_WK_STREQ("", FindStateSnapshot::capture(page.webView()).frame({ }).selectedText);
}

TEST_P(FindPrivateSPI, CaseSensitivity)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>Hello hello</p>"_s), GetParam() };
    PrivateFindSPITester find { page.webView() };

    EXPECT_EQ(find.countStringMatches(@"hello", _WKFindOptionsCaseInsensitive), 2u);
    EXPECT_EQ(find.countStringMatches(@"hello", static_cast<_WKFindOptions>(0)), 1u);
}

TEST_P(FindPrivateSPI, AtWordStarts)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello othello</p>"_s), GetParam() };
    PrivateFindSPITester find { page.webView() };

    EXPECT_EQ(find.countStringMatches(@"hello", _WKFindOptionsCaseInsensitive), 2u);
    EXPECT_EQ(find.countStringMatches(@"hello", static_cast<_WKFindOptions>(_WKFindOptionsCaseInsensitive | _WKFindOptionsAtWordStarts)), 1u);
}

TEST_P(FindPrivateSPI, ChangingSearchStringResetsMatchIndex)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello hello help</p>"_s), GetParam() };
    PrivateFindSPITester find { page.webView() };

    find.findString(@"hello", stepOptions);
    EXPECT_EQ(find.findString(@"hello", stepOptions).matchIndex, 1);
    auto result = find.findString(@"hel", stepOptions);
    EXPECT_EQ(result.matchCount, 3u);
    EXPECT_EQ(result.matchIndex, 0);
}

TEST_P(FindPrivateSPI, NoMatch)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello</p>"_s), GetParam() };
    PrivateFindSPITester find { page.webView() };

    auto result = find.findString(@"goodbye", stepOptions);
    EXPECT_FALSE(result.found);
}

TEST_P(FindPrivateSPI, HideFindUI)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello</p>"_s), GetParam() };
    PrivateFindSPITester find { page.webView() };

    find.findString(@"hello", static_cast<_WKFindOptions>(stepOptions | _WKFindOptionsShowFindIndicator));
    auto before = FindStateSnapshot::capture(page.webView());
    EXPECT_WK_STREQ("hello", before.frame({ }).selectedText);

    find.hideFindUI();
    auto after = FindStateSnapshot::capture(page.webView());
    EXPECT_WK_STREQ("hello", after.frame({ }).selectedText);
    EXPECT_TRUE(CGRectIsNull(after.textIndicatorRect));
}

TEST_P(FindPrivateSPI, CountAcrossCrossOriginFrames)
{
    FindTestPage page { FindTestFixtures::matchesInEveryFrame(), GetParam() };
    PrivateFindSPITester find { page.webView() };

    EXPECT_EQ(find.countStringMatches(@"hello", _WKFindOptionsCaseInsensitive), 4u);
}

TEST_P(FindPrivateSPI, StepAcrossCrossOriginFrames)
{
    FindTestPage page { FindTestFixtures::matchesInEveryFrame(), GetParam() };
    PrivateFindSPITester find { page.webView() };

    Vector<Vector<unsigned>> expectedFrames { { }, { 0 }, { 1 }, { 2 }, { } };
    // FIXME: With site isolation, the match index doesn't follow frame tree order, even though the selection does.
    Vector<int> expectedIndices = GetParam() == SiteIsolation::On ? Vector<int> { 0, 2, 3, 1, 0 } : Vector<int> { 0, 1, 2, 3, 0 };
    for (size_t i = 0; i < expectedFrames.size(); ++i) {
        auto result = find.findString(@"hello", stepOptions);
        EXPECT_EQ(result.matchCount, 4u);
        EXPECT_EQ(result.matchIndex, expectedIndices[i]) << "step " << i;
        EXPECT_EQ(FindStateSnapshot::capture(page.webView()).framesWithSelection(), Vector<Vector<unsigned>> { expectedFrames[i] }) << "step " << i;
    }
}

TEST_P(FindPrivateSPI, SkipsFramesWithoutMatches)
{
    FindTestPage page { FindTestFixtures::emptyChildFrames(), GetParam() };
    PrivateFindSPITester find { page.webView() };

    find.findString(@"hello", stepOptions);
    find.findString(@"hello", stepOptions);
    auto snapshot = FindStateSnapshot::capture(page.webView());
    EXPECT_WK_STREQ("hello", snapshot.frame({ 1 }).selectedText);
}

TEST_P(FindPrivateSPI, HiddenChildFrame)
{
    FindTestPage page { FindTestFixtures::hiddenChildFrame(), GetParam() };
    PrivateFindSPITester find { page.webView() };

    auto count = find.countStringMatches(@"hello", _WKFindOptionsCaseInsensitive);
    if (GetParam() == SiteIsolation::On)
        EXPECT_EQ(count, 2u); // FIXME: With site isolation, matches inside a display: none cross-origin frame are counted.
    else
        EXPECT_EQ(count, 1u);
}

TEST_P(FindPrivateSPI, NestedFrames)
{
    FindTestPage page { FindTestFixtures::nestedABA(), GetParam() };
    PrivateFindSPITester find { page.webView() };

    EXPECT_EQ(find.countStringMatches(@"hello", _WKFindOptionsCaseInsensitive), 3u);
    find.findString(@"hello", stepOptions);
    find.findString(@"hello", stepOptions);
    auto result = find.findString(@"hello", stepOptions);
    // FIXME: With site isolation, the match index doesn't follow frame tree order, even though the selection does.
    EXPECT_EQ(result.matchIndex, GetParam() == SiteIsolation::On ? 1 : 2);
    EXPECT_WK_STREQ("hello", FindStateSnapshot::capture(page.webView()).frame({ 0, 0 }).selectedText);
}

TEST_P(FindPrivateSPI, MatchAddedBetweenSteps)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p id='p'>hello hello</p>"_s), GetParam() };
    PrivateFindSPITester find { page.webView() };

    EXPECT_EQ(find.findString(@"hello", stepOptions).matchCount, 2u);
    [page.webView() objectByEvaluatingJavaScript:@"p.append(' hello')"];
    auto result = find.findString(@"hello", stepOptions);
    EXPECT_EQ(result.matchCount, 3u);
    EXPECT_EQ(result.matchIndex, 1);
}

#if ENABLE(UNIFIED_PDF)

TEST_P(FindPrivateSPI, CountInPDF)
{
    FindTestPage page { "/test.pdf"_s, GetParam(), configurationForWebViewTestingUnifiedPDF(), FindTestFixtures::pdfResources() };
    PrivateFindSPITester find { page.webView() };

    EXPECT_EQ(find.countStringMatches(@"555", _WKFindOptionsCaseInsensitive), 2u);
}

TEST_P(FindPrivateSPI, CountInCrossOriginPDF)
{
    FindTestPage page { FindTestFixtures::crossOriginChildPDF(), GetParam(), configurationForWebViewTestingUnifiedPDF(), FindTestFixtures::pdfResources() };
    PrivateFindSPITester find { page.webView() };

#if PLATFORM(MAC)
    // FIXME: FindController only searches a PDF plugin in the main frame.
#else
    // A PDF in a subframe is shown as an image rather than in the PDF plugin, so it has no text to find.
#endif
    EXPECT_EQ(find.countStringMatches(@"555", _WKFindOptionsCaseInsensitive), 0u);
}

#endif // ENABLE(UNIFIED_PDF)

} // namespace TestWebKitAPI
