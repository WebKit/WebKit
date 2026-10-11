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
#import "Helpers/cocoa/FindStateSnapshot.h"
#import "Helpers/cocoa/FindTestFixtures.h"
#import "Helpers/cocoa/PDFTestHelpers.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/TextFinderTester.h"
#import <WebKit/WKWebViewPrivate.h>

namespace TestWebKitAPI {

class FindTextFinder : public ::testing::TestWithParam<SiteIsolation> {
protected:
    static constexpr auto caseInsensitive = NSTextFinderAsynchronousDocumentFindOptionsCaseInsensitive;
    static constexpr auto caseInsensitiveWrap = static_cast<NSTextFinderAsynchronousDocumentFindOptions>(NSTextFinderAsynchronousDocumentFindOptionsCaseInsensitive | NSTextFinderAsynchronousDocumentFindOptionsWrap);
};

INSTANTIATE_TEST_SUITE_P(FindInPage, FindTextFinder, testing::Values(SiteIsolation::Off, SiteIsolation::On), [](auto& info) {
    return std::string { info.param == SiteIsolation::On ? "SiteIsolation" : "NoSiteIsolation" };
});

TEST_P(FindTextFinder, FindAllMatches)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello hello hello</p>"_s), GetParam() };
    TextFinderTester find { page.webView() };

    auto result = find.findMatches(@"hello", caseInsensitive);
    EXPECT_EQ([result.matches count], 3u);
    for (id<NSTextFinderAsynchronousDocumentFindMatch> match in result.matches.get())
        EXPECT_EQ(match.textRects.count, 1u);
}

TEST_P(FindTextFinder, FindAllMatchesWithoutPlatformFindUIHasNoRects)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello hello hello</p>"_s), GetParam() };
    [page.webView() _setUsePlatformFindUI:NO];
    TextFinderTester find { page.webView() };

    auto result = find.findMatches(@"hello", caseInsensitive);
    EXPECT_EQ([result.matches count], 3u);
    EXPECT_EQ([result.matches firstObject].textRects.count, 0u);
    EXPECT_NULL(find.imageForMatch([result.matches firstObject]));
}

TEST_P(FindTextFinder, MaxResultsIsCappedAt1000)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p id='p'></p><script>p.textContent = 'a '.repeat(1100)</script>"_s), GetParam() };
    TextFinderTester find { page.webView() };

    EXPECT_EQ([find.findMatches(@"a", caseInsensitive, NSUIntegerMax).matches count], 1000u);
}

TEST_P(FindTextFinder, StepReportsWrap)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello hello</p>"_s), GetParam() };
    TextFinderTester find { page.webView() };

    EXPECT_FALSE(find.findMatches(@"hello", caseInsensitiveWrap, 1).didWrap);
    EXPECT_FALSE(find.findMatches(@"hello", caseInsensitiveWrap, 1).didWrap);
    EXPECT_TRUE(find.findMatches(@"hello", caseInsensitiveWrap, 1).didWrap);
}

TEST_P(FindTextFinder, StepWithoutWrapFindsNothingAtEnd)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello</p>"_s), GetParam() };
    TextFinderTester find { page.webView() };

    EXPECT_EQ([find.findMatches(@"hello", caseInsensitive, 1).matches count], 1u);
    EXPECT_EQ([find.findMatches(@"hello", caseInsensitive, 1).matches count], 0u);
}

TEST_P(FindTextFinder, StartsWith)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello othello</p>"_s), GetParam() };
    TextFinderTester find { page.webView() };

    EXPECT_EQ([find.findMatches(@"hello", caseInsensitive).matches count], 2u);
    auto startsWith = static_cast<NSTextFinderAsynchronousDocumentFindOptions>(caseInsensitive | NSTextFinderAsynchronousDocumentFindOptionsStartsWith);
    EXPECT_EQ([find.findMatches(@"hello", startsWith).matches count], 1u);
}

TEST_P(FindTextFinder, SelectMatch)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>one hello two hello</p>"_s), GetParam() };
    TextFinderTester find { page.webView() };

    auto result = find.findMatches(@"hello", caseInsensitive);
    find.selectMatch([result.matches objectAtIndex:1]);
    EXPECT_EQ([[page.webView() objectByEvaluatingJavaScript:@"getSelection().anchorOffset"] intValue], 14);
}

TEST_P(FindTextFinder, ReplaceMatches)
{
    FindTestPage page { FindTestFixtures::contentEditable(), GetParam() };
    TextFinderTester find { page.webView() };

    auto result = find.findMatches(@"hello", caseInsensitive);
    EXPECT_EQ(find.replaceMatches(result.matches.get(), @"goodbye"), 1u);
    EXPECT_WK_STREQ([page.webView() objectByEvaluatingJavaScript:@"document.querySelector('[contenteditable]').textContent"], "goodbye");
}

TEST_P(FindTextFinder, FindAllAcrossCrossOriginFrames)
{
    FindTestPage page { FindTestFixtures::matchesInEveryFrame(), GetParam() };
    TextFinderTester find { page.webView() };

    EXPECT_EQ([find.findMatches(@"hello", caseInsensitive).matches count], 4u);
}

TEST_P(FindTextFinder, FindAllInSameOriginChild)
{
    FindTestPage page { FindTestFixtures::sameOriginChild(), GetParam() };
    TextFinderTester find { page.webView() };

    EXPECT_EQ([find.findMatches(@"hello", caseInsensitive).matches count], 2u);
}

TEST_P(FindTextFinder, StepAcrossCrossOriginFrames)
{
    FindTestPage page { FindTestFixtures::matchesInEveryFrame(), GetParam() };
    TextFinderTester find { page.webView() };

    Vector<Vector<unsigned>> expectedFrames { { }, { 0 }, { 1 }, { 2 }, { } };
    for (size_t i = 0; i < expectedFrames.size(); ++i) {
        find.findMatches(@"hello", caseInsensitiveWrap, 1);
        EXPECT_EQ(FindStateSnapshot::capture(page.webView()).framesWithSelection(), Vector<Vector<unsigned>> { expectedFrames[i] }) << "step " << i;
    }
}

TEST_P(FindTextFinder, ShadowDOM)
{
    FindTestPage page { FindTestFixtures::shadowDOM(), GetParam() };
    TextFinderTester find { page.webView() };

    EXPECT_EQ([find.findMatches(@"hello", caseInsensitive).matches count], 1u);
}

TEST_P(FindTextFinder, TextControls)
{
    FindTestPage page { FindTestFixtures::textControls(), GetParam() };
    TextFinderTester find { page.webView() };

    EXPECT_EQ([find.findMatches(@"hello", caseInsensitive).matches count], 2u);
}

TEST_P(FindTextFinder, MatchRemovedAfterFindAll)
{
    FindTestPage page { FindTestFixtures::singleFrame("<div contenteditable><span id='a'>hello</span> <span>hello</span></div>"_s), GetParam() };
    TextFinderTester find { page.webView() };

    auto result = find.findMatches(@"hello", caseInsensitive);
    [page.webView() objectByEvaluatingJavaScript:@"a.remove()"];
    // FIXME(326400): Replacing a stale match removes the neighboring text and reports a replacement for it.
    EXPECT_EQ(find.replaceMatches(result.matches.get(), @"goodbye"), 2u);
    EXPECT_WK_STREQ([page.webView() objectByEvaluatingJavaScript:@"document.querySelector('[contenteditable]').innerHTML"], "goodbye<br>");
}

TEST_P(FindTextFinder, SelectMatchInCrossOriginFrame)
{
    FindTestPage page { FindTestFixtures::crossOriginChild(), GetParam() };
    TextFinderTester find { page.webView() };

    auto result = find.findMatches(@"hello", caseInsensitive);
    find.selectMatch([result.matches lastObject]);

    auto childFrame = FindStateSnapshot::capture(page.webView()).frame({ 0 });
    ASSERT_TRUE(childFrame);
    EXPECT_EQ([result.matches count], 2u);
    EXPECT_WK_STREQ("hello", childFrame->selectedText);
}

// scrollFindMatchToVisible calls indicateFindMatch, but only when WebKit draws the find UI.
TEST_P(FindTextFinder, ScrollMatchToVisibleInCrossOriginFrame)
{
    FindTestPage page { FindTestFixtures::crossOriginChild(), GetParam() };
    [page.webView() _setUsePlatformFindUI:NO];
    TextFinderTester find { page.webView() };

    auto result = find.findMatches(@"hello", caseInsensitive);
    find.scrollMatchToVisible([result.matches lastObject]);

    auto childFrame = FindStateSnapshot::capture(page.webView()).frame({ 0 });
    ASSERT_TRUE(childFrame);
    EXPECT_EQ([result.matches count], 2u);
    EXPECT_WK_STREQ("hello", childFrame->selectedText);
}

TEST_P(FindTextFinder, ImageForMatchInCrossOriginFrame)
{
    FindTestPage page { FindTestFixtures::crossOriginChild(), GetParam() };
    TextFinderTester find { page.webView() };

    auto result = find.findMatches(@"hello", caseInsensitive);
    EXPECT_EQ([result.matches count], 2u);
    EXPECT_NOT_NULL(find.imageForMatch([result.matches lastObject]).get());
}

TEST_P(FindTextFinder, ReplaceMatchesInCrossOriginFrame)
{
    FindTestPage page { { .body = "<div contenteditable>hello</div>"_s, .children = { { .host = "b.com"_s, .body = "<div contenteditable>hello</div>"_s } } }, GetParam() };
    TextFinderTester find { page.webView() };

    auto result = find.findMatches(@"hello", caseInsensitive);
    auto replacementCount = find.replaceMatches(result.matches.get(), @"goodbye");

    NSString *childText = [page.webView() objectByEvaluatingJavaScript:@"document.body.textContent" inFrame:[page.webView() firstChildFrame]];
    EXPECT_EQ([result.matches count], 2u);
    EXPECT_EQ(replacementCount, 2u);
    EXPECT_WK_STREQ(childText, "goodbye");
}

TEST_P(FindTextFinder, ReplaceSelectionInCrossOriginFrame)
{
    FindTestPage page { { .body = "<p>nothing here</p>"_s, .children = { { .host = "b.com"_s, .body = "<div contenteditable>hello</div>"_s } } }, GetParam() };
    TextFinderTester find { page.webView() };

    find.findMatches(@"hello", caseInsensitiveWrap, 1);
    EXPECT_EQ(FindStateSnapshot::capture(page.webView()).framesWithSelection(), Vector<Vector<unsigned>> { { 0 } });
    auto replacementCount = find.replaceMatches(@[ ], @"goodbye");

    NSString *childText = [page.webView() objectByEvaluatingJavaScript:@"document.body.textContent" inFrame:[page.webView() firstChildFrame]];
    EXPECT_EQ(replacementCount, 1u);
    EXPECT_WK_STREQ(childText, "goodbye");
}

#if ENABLE(UNIFIED_PDF)

TEST_P(FindTextFinder, FindAllInPDF)
{
    FindTestPage page { "/test.pdf"_s, GetParam(), configurationForWebViewTestingUnifiedPDF(), FindTestFixtures::pdfResources() };
    TextFinderTester find { page.webView() };

    // FIXME(326398): findStringMatches only searches DOM text, never a PDF plugin.
    EXPECT_EQ([find.findMatches(@"555", caseInsensitive).matches count], 0u);
}

#endif // ENABLE(UNIFIED_PDF)

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
