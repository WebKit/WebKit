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
#import <WebKit/WebViewPrivate.h>
#import <wtf/RetainPtr.h>

// These tests use WebKitLegacy because it handles a wheel event synchronously, with scrolling on
// the main thread. The page can make a change and leave it pending, and the next wheel event sees
// it before anything else updates layout.
//
// EventHandler hit tests wheel events that may scroll on the main thread. These tests check
// behavior that must stay the same if it uses the latched state for those events instead.

static bool legacyWheelEventLatchingDidFinishLoad;

@interface LegacyWheelEventLatchingLoadDelegate : NSObject <WebFrameLoadDelegate>
@end

@implementation LegacyWheelEventLatchingLoadDelegate

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    legacyWheelEventLatchingDidFinishLoad = true;
}

@end

namespace TestWebKitAPI {

static constexpr NSSize legacyWheelEventLatchingWindowSize { 800, 600 };

// The pointer is over #inner, which is inside #outer.
static constexpr NSPoint legacyWheelEventLatchingPointInPage { 150, 150 };

static NSString *const legacyWheelEventLatchingMarkup = @""
    "<style>"
    "body { margin: 0; }"
    "#outer { width: 400px; height: 400px; overflow: scroll; }"
    "#inner { width: 300px; height: 300px; overflow: scroll; }"
    ".tall { height: 2000px; }"
    "#cover { position: absolute; top: 0; left: 0; width: 400px; height: 400px; }"
    "</style>"
    "<div id='outer'><div id='inner'><div id='innerContent' class='tall'></div></div><div class='tall'></div></div>"
    "<script>"
    "var lastWheelEventTarget = '';"
    "document.addEventListener('wheel', event => lastWheelEventTarget = event.target.id);"
    "</script>";

static RetainPtr<WebView> loadNestedScrollers(NSWindow *window)
{
    RetainPtr webView = adoptNS([[WebView alloc] initWithFrame:window.contentView.bounds frameName:nil groupName:nil]);
    [window.contentView addSubview:webView.get()];

    RetainPtr delegate = adoptNS([[LegacyWheelEventLatchingLoadDelegate alloc] init]);
    [webView setFrameLoadDelegate:delegate.get()];
    legacyWheelEventLatchingDidFinishLoad = false;
    [[webView mainFrame] loadHTMLString:legacyWheelEventLatchingMarkup baseURL:nil];
    Util::run(&legacyWheelEventLatchingDidFinishLoad);
    [webView setFrameLoadDelegate:nil];

    return webView;
}

static RetainPtr<NSWindow> createWindowAtScreenOrigin()
{
    auto contentRect = NSMakeRect(0, 0, legacyWheelEventLatchingWindowSize.width, legacyWheelEventLatchingWindowSize.height);
    return adoptNS([[NSWindow alloc] initWithContentRect:contentRect styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES]);
}

// Gives the event to the view directly, so that nothing updates layout before WebKit handles it.
static void sendWheelEvent(WebView *webView, int32_t deltaY, CGScrollPhase phase)
{
    RetainPtr cgEvent = adoptCF(CGEventCreateScrollWheelEvent(nullptr, kCGScrollEventUnitPixel, 1, deltaY));
    CGEventSetIntegerValueField(cgEvent.get(), kCGScrollWheelEventIsContinuous, 1);
    CGEventSetIntegerValueField(cgEvent.get(), kCGScrollWheelEventScrollPhase, phase);
    CGEventSetIntegerValueField(cgEvent.get(), kCGScrollWheelEventMomentumPhase, kCGMomentumScrollPhaseNone);

    // The window is at the screen origin, so window and screen points are the same. CGEvent uses
    // flipped coordinates relative to the first screen.
    CGFloat screenHeight = NSScreen.screens.firstObject.frame.size.height;
    CGFloat yInWindow = legacyWheelEventLatchingWindowSize.height - legacyWheelEventLatchingPointInPage.y;
    CGEventSetLocation(cgEvent.get(), CGPointMake(legacyWheelEventLatchingPointInPage.x, screenHeight - yInWindow));

    NSEvent *event = [NSEvent eventWithCGEvent:cgEvent.get()];
    [[webView hitTest:event.locationInWindow] scrollWheel:event];
}

static int scrollTop(WebView *webView, NSString *elementID)
{
    return [webView stringByEvaluatingJavaScriptFromString:[NSString stringWithFormat:@"document.getElementById('%@').scrollTop", elementID]].intValue;
}

TEST(WebKitLegacy, WheelGestureRelatchesWhenLatchedScrollerStopsScrolling)
{
    RetainPtr window = createWindowAtScreenOrigin();
    RetainPtr webView = loadNestedScrollers(window.get());

    sendWheelEvent(webView.get(), -10, kCGScrollPhaseBegan);
    sendWheelEvent(webView.get(), -10, kCGScrollPhaseChanged);
    EXPECT_GT(scrollTop(webView.get(), @"inner"), 0);
    EXPECT_EQ(scrollTop(webView.get(), @"outer"), 0);

    // The style change destroys the latched scroller's scrollable area, but only when layout runs.
    // The element stays in the document, so the latching state stays.
    [webView stringByEvaluatingJavaScriptFromString:@"document.getElementById('inner').style.overflow = 'visible'"];
    sendWheelEvent(webView.get(), -10, kCGScrollPhaseChanged);
    sendWheelEvent(webView.get(), 0, kCGScrollPhaseEnded);

    // The rest of the gesture latches to the enclosing scroller.
    EXPECT_GT(scrollTop(webView.get(), @"outer"), 0);
}

TEST(WebKitLegacy, LatchedWheelGestureIgnoresElementCoveringPointer)
{
    RetainPtr window = createWindowAtScreenOrigin();
    RetainPtr webView = loadNestedScrollers(window.get());

    sendWheelEvent(webView.get(), -10, kCGScrollPhaseBegan);
    sendWheelEvent(webView.get(), -10, kCGScrollPhaseChanged);
    int innerScrollTop = scrollTop(webView.get(), @"inner");
    EXPECT_GT(innerScrollTop, 0);

    // A hit test after layout finds the cover. The latched element and scroller still get the rest
    // of the gesture.
    [webView stringByEvaluatingJavaScriptFromString:@""
        "lastWheelEventTarget = '';"
        "let cover = document.createElement('div');"
        "cover.id = 'cover';"
        "document.body.appendChild(cover);"];
    sendWheelEvent(webView.get(), -10, kCGScrollPhaseChanged);
    sendWheelEvent(webView.get(), 0, kCGScrollPhaseEnded);

    EXPECT_WK_STREQ(@"innerContent", [webView stringByEvaluatingJavaScriptFromString:@"lastWheelEventTarget"]);
    EXPECT_GT(scrollTop(webView.get(), @"inner"), innerScrollTop);
    EXPECT_EQ(scrollTop(webView.get(), @"outer"), 0);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
