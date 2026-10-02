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

#if HAVE(UIFINDINTERACTION)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/FindInteractionTester.h"
#import "Helpers/cocoa/FindStateSnapshot.h"
#import "Helpers/cocoa/FindTestFixtures.h"
#import "Helpers/cocoa/PDFTestHelpers.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import <WebKit/WKWebViewPrivate.h>

namespace TestWebKitAPI {

class FindInteraction : public ::testing::TestWithParam<SiteIsolation> {
protected:
    static RetainPtr<UITextSearchOptions> searchOptions(UITextSearchMatchMethod method, NSStringCompareOptions compareOptions = NSCaseInsensitiveSearch)
    {
        RetainPtr options = adoptNS([UITextSearchOptions new]);
        [options setWordMatchMethod:method];
        [options setStringCompareOptions:compareOptions];
        return options;
    }
};

INSTANTIATE_TEST_SUITE_P(FindInPage, FindInteraction, testing::Values(SiteIsolation::Off, SiteIsolation::On), [](auto& info) {
    return std::string { info.param == SiteIsolation::On ? "SiteIsolation" : "NoSiteIsolation" };
});

TEST_P(FindInteraction, PerformTextSearch)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello hello hello</p>"_s), GetParam() };
    FindInteractionTester find { page.webView() };

    EXPECT_EQ([find.performTextSearch(@"hello") count], 3u);
    EXPECT_EQ([find.performTextSearch(@"goodbye") count], 0u);
}

TEST_P(FindInteraction, CaseSensitivity)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>Hello hello</p>"_s), GetParam() };
    FindInteractionTester find { page.webView() };

    EXPECT_EQ([find.performTextSearch(@"hello", searchOptions(UITextSearchMatchMethodContains).get()) count], 2u);
    EXPECT_EQ([find.performTextSearch(@"hello", searchOptions(UITextSearchMatchMethodContains, 0).get()) count], 1u);
}

TEST_P(FindInteraction, WordMatchMethods)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello othello helloworld</p>"_s), GetParam() };
    FindInteractionTester find { page.webView() };

    EXPECT_EQ([find.performTextSearch(@"hello", searchOptions(UITextSearchMatchMethodContains).get()) count], 3u);
    EXPECT_EQ([find.performTextSearch(@"hello", searchOptions(UITextSearchMatchMethodStartsWith).get()) count], 2u);
    EXPECT_EQ([find.performTextSearch(@"hello", searchOptions(UITextSearchMatchMethodFullWord).get()) count], 1u);
}

TEST_P(FindInteraction, DecorateShowsOverlay)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello</p>"_s), GetParam() };
    FindInteractionTester find { page.webView() };

    find.didBeginTextSearchOperation();
    auto ranges = find.performTextSearch(@"hello");
    find.decorateFoundTextRange([ranges firstObject], UITextSearchFoundTextStyleHighlighted);
    EXPECT_TRUE(FindStateSnapshot::capture(page.webView()).hasFindOverlayLayer);

    find.didEndTextSearchOperation();
    EXPECT_FALSE(FindStateSnapshot::capture(page.webView()).hasFindOverlayLayer);
}

TEST_P(FindInteraction, OnlyHighlightedDecorationSelectsMatch)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello</p>"_s), GetParam() };
    FindInteractionTester find { page.webView() };

    auto ranges = find.performTextSearch(@"hello");
    find.decorateFoundTextRange([ranges firstObject], UITextSearchFoundTextStyleFound);
    EXPECT_WK_STREQ("", FindStateSnapshot::capture(page.webView()).frame({ }).selectedText);

    find.decorateFoundTextRange([ranges firstObject], UITextSearchFoundTextStyleHighlighted);
    EXPECT_WK_STREQ("hello", FindStateSnapshot::capture(page.webView()).frame({ }).selectedText);
}

TEST_P(FindInteraction, ScrollRangeToVisible)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>top</p><p style='margin-top: 2000px'>hello</p>"_s), GetParam() };
    FindInteractionTester find { page.webView() };

    auto ranges = find.performTextSearch(@"hello");
    find.scrollRangeToVisible([ranges firstObject]);
    EXPECT_GT(FindStateSnapshot::capture(page.webView()).contentOffset.y, 1000);
}

TEST_P(FindInteraction, ReplaceRequiresEditableWebView)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p id='p'>hello</p>"_s), GetParam() };
    FindInteractionTester find { page.webView() };

    auto ranges = find.performTextSearch(@"hello");
    find.replaceFoundTextInRange([ranges firstObject], @"goodbye");
    EXPECT_WK_STREQ([page.webView() objectByEvaluatingJavaScript:@"p.textContent"], "hello");

    [page.webView() _setEditable:YES];
    ranges = find.performTextSearch(@"hello");
    find.replaceFoundTextInRange([ranges firstObject], @"goodbye");
    EXPECT_WK_STREQ([page.webView() objectByEvaluatingJavaScript:@"p.textContent"], "goodbye");
}

TEST_P(FindInteraction, RectForFoundTextRange)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello</p>"_s), GetParam() };
    FindInteractionTester find { page.webView() };

    auto rect = find.rectForFoundTextRange([find.performTextSearch(@"hello") firstObject]);
    EXPECT_FALSE(CGRectIsEmpty(rect));
}

TEST_P(FindInteraction, SearchAcrossCrossOriginFrames)
{
    FindTestPage page { FindTestFixtures::matchesInEveryFrame(), GetParam() };
    FindInteractionTester find { page.webView() };

    EXPECT_EQ([find.performTextSearch(@"hello") count], 4u);
}

TEST_P(FindInteraction, NestedFrames)
{
    FindTestPage page { FindTestFixtures::nestedABA(), GetParam() };
    FindInteractionTester find { page.webView() };

    EXPECT_EQ([find.performTextSearch(@"hello") count], 3u);
}

TEST_P(FindInteraction, HiddenChildFrame)
{
    FindTestPage page { FindTestFixtures::hiddenChildFrame(), GetParam() };
    FindInteractionTester find { page.webView() };

    auto count = [find.performTextSearch(@"hello") count];
    if (GetParam() == SiteIsolation::On)
        EXPECT_EQ(count, 2u); // FIXME: With site isolation, matches inside a display: none cross-origin frame are found.
    else
        EXPECT_EQ(count, 1u);
}

TEST_P(FindInteraction, ShadowDOM)
{
    FindTestPage page { FindTestFixtures::shadowDOM(), GetParam() };
    FindInteractionTester find { page.webView() };

    EXPECT_EQ([find.performTextSearch(@"hello") count], 1u);
}

TEST_P(FindInteraction, TextControls)
{
    FindTestPage page { FindTestFixtures::textControls(), GetParam() };
    FindInteractionTester find { page.webView() };

    EXPECT_EQ([find.performTextSearch(@"hello") count], 2u);
}

TEST_P(FindInteraction, MatchAddedAfterSearch)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p id='p'>hello</p>"_s), GetParam() };
    FindInteractionTester find { page.webView() };

    EXPECT_EQ([find.performTextSearch(@"hello") count], 1u);
    [page.webView() objectByEvaluatingJavaScript:@"p.append(' hello')"];
    EXPECT_EQ([find.performTextSearch(@"hello") count], 2u);
}

TEST_P(FindInteraction, SearchFindsCaptionCueInCrossOriginFrame)
{
    FindTestPage page { FindTestFixtures::crossOriginChildWithCaptionedVideo(), GetParam(), FindTestFixtures::findInVideoConfiguration(), FindTestFixtures::captionedVideoResources() };
    FindInteractionTester find { page.webView() };

    auto ranges = find.performTextSearch(@"hello");
    EXPECT_EQ([ranges count], 2u);

    find.decorateFoundTextRange([ranges lastObject], UITextSearchFoundTextStyleHighlighted);
    RetainPtr childFrame = [page.webView() firstChildFrame];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[page.webView() objectByEvaluatingJavaScript:@"document.querySelector('video').currentTime" inFrame:childFrame.get()] doubleValue] == 2;
    }));
}

#if ENABLE(UNIFIED_PDF)

TEST_P(FindInteraction, SearchAndRequestRectInPDF)
{
    FindTestPage page { "/test.pdf"_s, GetParam(), configurationForWebViewTestingUnifiedPDF(), FindTestFixtures::pdfResources() };
    FindInteractionTester find { page.webView() };

    auto ranges = find.performTextSearch(@"555");
    EXPECT_EQ([ranges count], 2u);

    find.decorateFoundTextRange([ranges firstObject], UITextSearchFoundTextStyleHighlighted);
    // FIXME: requestRectForFoundTextRange has no case for PDF matches.
    EXPECT_TRUE(CGRectIsEmpty(find.rectForFoundTextRange([ranges firstObject])));
}

TEST_P(FindInteraction, SearchInCrossOriginPDF)
{
    FindTestPage page { FindTestFixtures::crossOriginChildPDF(), GetParam(), configurationForWebViewTestingUnifiedPDF(), FindTestFixtures::pdfResources() };
    FindInteractionTester find { page.webView() };

    EXPECT_EQ([find.performTextSearch(@"555") count], 0u);
}

#endif // ENABLE(UNIFIED_PDF)

} // namespace TestWebKitAPI

#endif // HAVE(UIFINDINTERACTION)
