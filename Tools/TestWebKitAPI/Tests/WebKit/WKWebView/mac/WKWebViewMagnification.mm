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

#import "Helpers/Test.h"
#import "Helpers/cocoa/TestNSBundleExtras.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "SyntheticNSEvent.h"
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

static void magnify(TestWKWebView *webView, NSEventPhase phase, CGFloat delta)
{
    static NSInteger eventNumber = 0;

    NSPoint locationInWindow = NSMakePoint(NSMidX(webView.frame), NSMidY(webView.frame));
    RetainPtr<NSWindow> window = webView.hostWindow;
    NSPoint globalLocation = [window convertRectToScreen:NSMakeRect(locationInWindow.x, locationInWindow.y, 1, 1)].origin;

    RetainPtr event = adoptNS([[SyntheticNSEvent alloc] initMagnifyEventAtLocation:locationInWindow
        globalLocation:globalLocation
        magnification:delta
        phase:phase
        time:NSProcessInfo.processInfo.systemUptime
        eventNumber:++eventNumber
        window:window]);

    dispatchSyntheticEvent(event, webView, @"magnify", ^(NSView *targetView, NSEvent *syntheticEvent) {
        [targetView magnifyWithEvent:syntheticEvent];
    });

    [webView waitForNextPresentationUpdate];
}

static double magnifyPastMaximum(TestWKWebView *webView)
{
    double maximum = [webView _maxMagnification];

    magnify(webView, NSEventPhaseBegan, 0);

    for (auto i = 0; i < 60 && [webView magnification] <= maximum; ++i)
        magnify(webView, NSEventPhaseChanged, 0.2);

    return [webView magnification];
}

static double lowestMagnificationReportedAfterGesture(TestWKWebView *webView)
{
    double lowest = [webView magnification];

    // Some presentation updates to outlast the transient zoom snap-back.
    for (unsigned i = 0; i < 30; ++i) {
        [webView waitForNextPresentationUpdate];
        double reported = [webView magnification];
        if (reported < lowest)
            lowest = reported;
    }

    return lowest;
}

static RetainPtr<TestWKWebView> webViewWithMagnificationAllowed()
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 600, 400)]);
    [webView setAllowsMagnification:YES];
    return webView;
}

static void runAccurateAtGestureEndTest(TestWKWebView *webView)
{
    double maximum = [webView _maxMagnification];
    EXPECT_GT(magnifyPastMaximum(webView), maximum);

    // We intentionally want to end magnification _past_ the maximum, since
    // that forces the commit to animate.
    magnify(webView, NSEventPhaseEnded, 0);

    EXPECT_NEAR(maximum, lowestMagnificationReportedAfterGesture(webView), 0.01);
    EXPECT_NEAR(maximum, [webView _pageScale], 0.01);
}

TEST(WKWebViewMagnification, AccurateAtGestureEnd)
{
    RetainPtr webView = webViewWithMagnificationAllowed();
    [webView synchronouslyLoadHTMLString:@"<body style='margin:0;height:3000px'>magnification</body>"];
    [webView waitForNextPresentationUpdate];
    runAccurateAtGestureEndTest(webView);
}

TEST(WKWebViewMagnification, AccurateAtGestureEndPDF)
{
    RetainPtr webView = webViewWithMagnificationAllowed();
    RetainPtr request = adoptNS([[NSURLRequest alloc] initWithURL:[NSBundle.test_resourcesBundle URLForResource:@"multiple-pages" withExtension:@"pdf"]]);
    [webView synchronouslyLoadRequest:request];
    [webView waitForNextPresentationUpdate];
    runAccurateAtGestureEndTest(webView);
}

} // namespace TestWebKitAPI
