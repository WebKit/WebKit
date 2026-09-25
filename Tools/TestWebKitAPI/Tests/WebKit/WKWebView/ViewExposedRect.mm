/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKPage.h>
#import <WebKit/WKPagePrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTestingMac.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(MAC)

@interface WKWebView ()
- (WKPageRef)_pageForTesting;
@end

static bool viewExposedRectDidForceRepaint;

static void viewExposedRectForceRepaintCallback(WKErrorRef error, void*)
{
    EXPECT_NULL(error);
    viewExposedRectDidForceRepaint = true;
}

static void expectLiveResizePresentationGeometry(TestWKWebView *webView, NSSize committedViewSize, NSSize resizeDelta, NSPoint visibleContentOrigin, bool expectScrollbarGutters)
{
    EXPECT_TRUE([webView _hasLiveResizePresentationOverrideForTesting]);

    NSSize committedClipSize = [webView _liveResizePresentationCommittedClipSizeForTesting];
    NSSize targetClipSize = [webView _liveResizePresentationTargetClipSizeForTesting];
    NSPoint actualVisibleContentOrigin = [webView _liveResizePresentationVisibleContentOriginForTesting];
    NSRect mappedContentRect = [webView _liveResizePresentationMappedContentRectForTesting];
    EXPECT_NEAR(resizeDelta.width, targetClipSize.width - committedClipSize.width, 0.01);
    EXPECT_NEAR(resizeDelta.height, targetClipSize.height - committedClipSize.height, 0.01);
    EXPECT_NEAR(visibleContentOrigin.x, actualVisibleContentOrigin.x, 0.01);
    EXPECT_NEAR(visibleContentOrigin.y, actualVisibleContentOrigin.y, 0.01);
    EXPECT_NEAR(visibleContentOrigin.x, NSMinX(mappedContentRect), 0.01);
    EXPECT_NEAR(visibleContentOrigin.y, NSMinY(mappedContentRect), 0.01);
    EXPECT_NEAR(targetClipSize.width, NSWidth(mappedContentRect), 0.01);
    EXPECT_NEAR(targetClipSize.height, NSHeight(mappedContentRect), 0.01);

    if (expectScrollbarGutters) {
        EXPECT_LT(committedClipSize.width, committedViewSize.width);
        EXPECT_LT(committedClipSize.height, committedViewSize.height);
    } else
        EXPECT_TRUE(NSEqualSizes(committedClipSize, committedViewSize));
}

static void expectNoLiveResizePresentationOverride(TestWKWebView *webView)
{
    EXPECT_FALSE([webView _hasLiveResizePresentationOverrideForTesting]);
    EXPECT_TRUE(NSEqualSizes(NSZeroSize, [webView _liveResizePresentationCommittedClipSizeForTesting]));
    EXPECT_TRUE(NSEqualSizes(NSZeroSize, [webView _liveResizePresentationTargetClipSizeForTesting]));
    EXPECT_TRUE(NSEqualPoints(NSZeroPoint, [webView _liveResizePresentationVisibleContentOriginForTesting]));
    EXPECT_TRUE(NSEqualRects(NSZeroRect, [webView _liveResizePresentationMappedContentRectForTesting]));
}

static void waitForNoLiveResizePresentationOverride(TestWKWebView *webView)
{
    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([webView] {
        return ![webView _hasLiveResizePresentationOverrideForTesting];
    }));
    expectNoLiveResizePresentationOverride(webView);
}

TEST(WebKit, InitialTileCoverageUsesViewExposedRect)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 5000000, 5000000)]);
    [webView _setClipsToVisibleRect:YES];
    [[webView hostWindow] setFrame:NSMakeRect(0, 0, 800, 600) display:NO];
    [webView addToTestWindow];
    [webView synchronouslyLoadHTMLString:@""];

    viewExposedRectDidForceRepaint = false;
    WKPageForceRepaint([webView _pageForTesting], 0, viewExposedRectForceRepaintCallback);
    TestWebKitAPI::Util::run(&viewExposedRectDidForceRepaint);
}

TEST(WebKit, LiveResizeKeepsCommittedContentCoveringViewWithScrollbarGutters)
{
    EXPECT_EQ(NSScrollerStyleLegacy, NSScroller.preferredScrollerStyle);

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 300)]);
    [webView addToTestWindow];
    [webView synchronouslyLoadHTMLString:@"<style>html { overflow: scroll; scrollbar-gutter: stable both-edges; } body { width: 2000px; height: 2000px; margin: 0; background: green; }</style>"];
    [webView waitForNextPresentationUpdate];

    NSWindow *window = [webView hostWindow];
    [NSNotificationCenter.defaultCenter postNotificationName:NSWindowWillStartLiveResizeNotification object:window];
    expectNoLiveResizePresentationOverride(webView.get());

    [webView setFrameSize:NSMakeSize(600, 450)];
    expectLiveResizePresentationGeometry(webView.get(), NSMakeSize(400, 300), NSMakeSize(200, 150), NSZeroPoint, true);

    waitForNoLiveResizePresentationOverride(webView.get());

    [webView objectByEvaluatingJavaScript:@"scrollTo(300, 200)"];
    [webView waitForNextPresentationUpdate];
    [webView setFrameSize:NSMakeSize(550, 400)];
    expectLiveResizePresentationGeometry(webView.get(), NSMakeSize(600, 450), NSMakeSize(-50, -50), NSMakePoint(300, 200), true);

    waitForNoLiveResizePresentationOverride(webView.get());

    [webView objectByEvaluatingJavaScript:@"document.documentElement.style.direction = 'rtl'; scrollTo(-300, 200)"];
    [webView waitForNextPresentationUpdate];
    [webView setFrameSize:NSMakeSize(500, 350)];
    expectLiveResizePresentationGeometry(webView.get(), NSMakeSize(550, 400), NSMakeSize(-50, -50), NSMakePoint(-300, 200), true);

    waitForNoLiveResizePresentationOverride(webView.get());

    [webView objectByEvaluatingJavaScript:@"document.documentElement.style.overflow = 'hidden'; document.documentElement.style.scrollbarGutter = 'auto'; document.documentElement.style.direction = 'ltr'; scrollTo(0, 0)"];
    [webView waitForNextPresentationUpdate];
    [webView setFrameSize:NSMakeSize(450, 300)];
    expectLiveResizePresentationGeometry(webView.get(), NSMakeSize(500, 350), NSMakeSize(-50, -50), NSZeroPoint, false);

    waitForNoLiveResizePresentationOverride(webView.get());

    [NSNotificationCenter.defaultCenter postNotificationName:NSWindowDidEndLiveResizeNotification object:window];
    expectNoLiveResizePresentationOverride(webView.get());
}

#endif // PLATFORM(MAC)
