/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if HAVE(UIWEBVIEW)

#import "DeprecatedGlobalValues.h"
#import "PlatformUtilities.h"
#import <WebKit/DOMHTMLMediaElement.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKitLegacy/WebPreferencesPrivate.h>
#import <wtf/MainThread.h>
#import <wtf/RetainPtr.h>

@interface ScrollingDoesNotPauseMediaDelegate : NSObject <UIWebViewDelegate, DOMEventListener> {
}
@end

static bool gotMainFrame = false;
static RetainPtr<WebFrame> mainFrame;

static bool readyToTest = false;
static bool didReceivePause  = false;
static bool didReceivePlaying = false;

@implementation ScrollingDoesNotPauseMediaDelegate

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)webViewDidFinishLoad:(UIWebView *)webView
IGNORE_WARNINGS_END
{
    didFinishLoad = true;
}

- (void)uiWebView:(UIWebView *)sender didCommitLoadForFrame:(WebFrame *)frame
{
    mainFrame = frame;
    gotMainFrame = true;
}

- (void)handleEvent:(DOMEvent *)event
{
    if ([event.type isEqualToString:@"canplaythrough"])
        readyToTest = true;
    else if ([event.type isEqualToString:@"pause"])
        didReceivePause = true;
    else if ([event.type isEqualToString:@"playing"])
        didReceivePlaying = true;
}
@end

namespace TestWebKitAPI {

// FIXME Re-enable when https://bugs.webkit.org/show_bug.cgi?id=237125 is resovled 
#if PLATFORM(IOS) || PLATFORM(VISION)
TEST(WebKitLegacy, DISABLED_ScrollingDoesNotPauseMedia)
#else
TEST(WebKitLegacy, ScrollingDoesNotPauseMedia)
#endif
{
    RetainPtr<WebPreferences> preferences = [WebPreferences standardPreferences];
    preferences.get().mediaDataLoadsAutomatically = YES;
    preferences.get().mediaPlaybackAllowsInline = YES;
    preferences.get().mediaPlaybackRequiresUserGesture = NO;

    RetainPtr<UIWindow> uiWindow = adoptNS([[UIWindow alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    RetainPtr<UIWebView> uiWebView = adoptNS([[UIWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [uiWindow addSubview:uiWebView.get()];

    RetainPtr<ScrollingDoesNotPauseMediaDelegate> testController = adoptNS([ScrollingDoesNotPauseMediaDelegate new]);
    uiWebView.get().delegate = testController.get();

    [uiWebView loadRequest:[NSURLRequest requestWithURL:[NSBundle.mainBundle URLForResource:@"one-video" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];

    Util::run(&didFinishLoad);
    Util::run(&gotMainFrame);

    callOnMainThread([&] () mutable {
        [mainFrame setTimeoutsPaused:YES];

        DOMHTMLMediaElement* video = (DOMHTMLMediaElement*)[[mainFrame DOMDocument] querySelector:@"video"];
        ASSERT_TRUE([video isKindOfClass:[DOMHTMLMediaElement class]]);

        [video addEventListener:@"canplaythrough" listener:testController.get() useCapture:NO];
        [video addEventListener:@"playing" listener:testController.get() useCapture:NO];
        [video addEventListener:@"pause" listener:testController.get() useCapture:NO];
        [video setSrc:@"video-with-audio.mp4"];

        [mainFrame setTimeoutsPaused:NO];
    });

    Util::run(&readyToTest);

    callOnMainThread([&] () mutable {
        DOMHTMLMediaElement* video = (DOMHTMLMediaElement*)[[mainFrame DOMDocument] querySelector:@"video"];
        ASSERT_TRUE([video isKindOfClass:[DOMHTMLMediaElement class]]);

        [video play];
    });

    Util::run(&didReceivePlaying);

    callOnMainThread([&] () mutable {
        DOMHTMLMediaElement* video = (DOMHTMLMediaElement*)[[mainFrame DOMDocument] querySelector:@"video"];
        ASSERT_TRUE([video isKindOfClass:[DOMHTMLMediaElement class]]);

        [video pause];
    });

    Util::run(&didReceivePause);
}

} // namespace TestWebKitAPI

#endif
