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
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/FindStateSnapshot.h"
#import "Helpers/cocoa/FindTestFixtures.h"
#import "Helpers/cocoa/PublicFindAPITester.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKFindConfiguration.h>

namespace TestWebKitAPI {

class FindPublicAPI : public ::testing::TestWithParam<SiteIsolation> {
protected:
    static RetainPtr<WKFindConfiguration> configuration(bool backwards, bool wraps, bool caseSensitive)
    {
        RetainPtr configuration = adoptNS([WKFindConfiguration new]);
        [configuration setBackwards:backwards];
        [configuration setWraps:wraps];
        [configuration setCaseSensitive:caseSensitive];
        return configuration;
    }
};

INSTANTIATE_TEST_SUITE_P(FindInPage, FindPublicAPI, testing::Values(SiteIsolation::Off, SiteIsolation::On), [](auto& info) {
    return std::string { info.param == SiteIsolation::On ? "SiteIsolation" : "NoSiteIsolation" };
});

TEST_P(FindPublicAPI, FoundAndNotFound)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello</p>"_s), GetParam() };
    PublicFindAPITester find { page.webView() };

    EXPECT_TRUE(find.findString(@"hello"));
    EXPECT_WK_STREQ("hello", FindStateSnapshot::capture(page.webView()).frame({ }).selectedText);
    EXPECT_FALSE(find.findString(@"goodbye"));
}

TEST_P(FindPublicAPI, WrapAndNoWrap)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello hello</p>"_s), GetParam() };
    PublicFindAPITester find { page.webView() };

    RetainPtr noWrap = configuration(false, false, false);
    EXPECT_TRUE(find.findString(@"hello", noWrap.get()));
    EXPECT_TRUE(find.findString(@"hello", noWrap.get()));
    EXPECT_FALSE(find.findString(@"hello", noWrap.get()));
    EXPECT_WK_STREQ("", FindStateSnapshot::capture(page.webView()).frame({ }).selectedText);

    RetainPtr wrap = configuration(false, true, false);
    EXPECT_TRUE(find.findString(@"hello", wrap.get()));
}

TEST_P(FindPublicAPI, Backwards)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>hello hello</p>"_s), GetParam() };
    PublicFindAPITester find { page.webView() };

    EXPECT_TRUE(find.findString(@"hello", configuration(true, true, false).get()));
    EXPECT_TRUE([page.webView() selectionRangeHasStartOffset:6 endOffset:11]);
}

TEST_P(FindPublicAPI, CaseSensitive)
{
    FindTestPage page { FindTestFixtures::singleFrame("<p>Hello</p>"_s), GetParam() };
    PublicFindAPITester find { page.webView() };

    EXPECT_FALSE(find.findString(@"hello", configuration(false, true, true).get()));
    EXPECT_TRUE(find.findString(@"hello", configuration(false, true, false).get()));
}

TEST_P(FindPublicAPI, StepAcrossCrossOriginFrames)
{
    FindTestPage page { FindTestFixtures::matchesInEveryFrame(), GetParam() };
    PublicFindAPITester find { page.webView() };

    Vector<Vector<unsigned>> expectedFrames { { }, { 0 }, { 1 }, { 2 }, { } };
    for (size_t i = 0; i < expectedFrames.size(); ++i) {
        EXPECT_TRUE(find.findString(@"hello"));
        EXPECT_EQ(FindStateSnapshot::capture(page.webView()).framesWithSelection(), Vector<Vector<unsigned>> { expectedFrames[i] }) << "step " << i;
    }
}

TEST_P(FindPublicAPI, MatchOnlyInChild)
{
    FindTestPage page { FindTestFixtures::matchOnlyInChild(), GetParam() };
    PublicFindAPITester find { page.webView() };

    EXPECT_TRUE(find.findString(@"hello"));
    EXPECT_WK_STREQ("hello", FindStateSnapshot::capture(page.webView()).frame({ 0 }).selectedText);
}

TEST_P(FindPublicAPI, HiddenChildFrame)
{
    FindTestPage page { FindTestFixtures::hiddenChildFrame(), GetParam() };
    PublicFindAPITester find { page.webView() };

    EXPECT_TRUE(find.findString(@"hello"));
    EXPECT_TRUE(find.findString(@"hello"));
    // FIXME: With site isolation, find selects a match inside a display: none cross-origin frame.
    Vector<unsigned> expectedFrame = GetParam() == SiteIsolation::On ? Vector<unsigned> { 0 } : Vector<unsigned> { };
    EXPECT_EQ(FindStateSnapshot::capture(page.webView()).framesWithSelection(), Vector<Vector<unsigned>> { expectedFrame });
}

TEST_P(FindPublicAPI, ShadowDOM)
{
    FindTestPage page { FindTestFixtures::shadowDOM(), GetParam() };
    PublicFindAPITester find { page.webView() };

    EXPECT_TRUE(find.findString(@"hello"));
}

TEST_P(FindPublicAPI, TextControlMatchIsNotFocused)
{
    FindTestPage page { FindTestFixtures::textControls(), GetParam() };
    PublicFindAPITester find { page.webView() };

    EXPECT_TRUE(find.findString(@"hello"));
    EXPECT_WK_STREQ([page.webView() objectByEvaluatingJavaScript:@"document.activeElement.tagName"], "BODY");
}

TEST_P(FindPublicAPI, ClosedDetailsOpens)
{
    FindTestPage page { FindTestFixtures::closedDetails(), GetParam() };
    PublicFindAPITester find { page.webView() };

    EXPECT_TRUE(find.findString(@"hello"));
    EXPECT_TRUE([[page.webView() objectByEvaluatingJavaScript:@"document.querySelector('details').open"] boolValue]);
}

TEST_P(FindPublicAPI, HiddenUntilFoundReveals)
{
    FindTestPage page { FindTestFixtures::hiddenUntilFound(), GetParam() };
    PublicFindAPITester find { page.webView() };

    EXPECT_TRUE(find.findString(@"hello"));
    EXPECT_FALSE([[page.webView() objectByEvaluatingJavaScript:@"document.querySelector('div').hasAttribute('hidden')"] boolValue]);
}

TEST_P(FindPublicAPI, UserSelectNone)
{
    FindTestPage page { FindTestFixtures::userSelectNone(), GetParam() };
    PublicFindAPITester find { page.webView() };

    EXPECT_TRUE(find.findString(@"hello"));
}

TEST_P(FindPublicAPI, ChildFrameRemovedBetweenSteps)
{
    FindTestPage page { FindTestFixtures::crossOriginChild(), GetParam() };
    PublicFindAPITester find { page.webView() };

    find.findString(@"hello");
    find.findString(@"hello");
    EXPECT_EQ(FindStateSnapshot::capture(page.webView()).framesWithSelection(), Vector<Vector<unsigned>> { { 0 } });
    [page.webView() objectByEvaluatingJavaScript:@"document.querySelector('iframe').remove()"];
    EXPECT_TRUE(find.findString(@"hello"));
    EXPECT_EQ(FindStateSnapshot::capture(page.webView()).framesWithSelection(), Vector<Vector<unsigned>> { { } });
}

TEST_P(FindPublicAPI, StepIntoCaptionCueInCrossOriginFrame)
{
    FindTestPage page { FindTestFixtures::crossOriginChildWithCaptionedVideo(), GetParam(), FindTestFixtures::findInVideoConfiguration(), FindTestFixtures::captionedVideoResources() };
    PublicFindAPITester find { page.webView() };

    EXPECT_TRUE(find.findString(@"hello"));
    EXPECT_TRUE(find.findString(@"hello"));

    RetainPtr childFrame = [page.webView() firstChildFrame];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[page.webView() objectByEvaluatingJavaScript:@"document.querySelector('video').currentTime" inFrame:childFrame.get()] doubleValue] == 2;
    }));
}

} // namespace TestWebKitAPI

