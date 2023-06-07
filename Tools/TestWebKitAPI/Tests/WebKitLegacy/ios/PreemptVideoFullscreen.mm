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

#if HAVE(UIWEBVIEW)

#import "DeprecatedGlobalValues.h"
#import "PlatformUtilities.h"
#import <UIKit/UIKit.h>
#import <WebKit/DOMHTMLMediaElement.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKitLegacy/WebPreferencesPrivate.h>
#import <wtf/MainThread.h>
#import <wtf/RetainPtr.h>

static bool gotMainFrame = false;
static RetainPtr<WebFrame> mainFrame;
static int countOfVideoElementsCanPlay = 0;
static bool readyToTest = false;
static int fullscreenBegin = 0;
static int fullscreenEnd = 0;
static bool doneTest = false;

@interface PreemptVideoFullscreenUIWebViewDelegate : NSObject <UIWebViewDelegate, DOMEventListener>
@end

@implementation PreemptVideoFullscreenUIWebViewDelegate
IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)webViewDidFinishLoad:(UIWebView *)webView
{
    didFinishLoad = true;
}
IGNORE_WARNINGS_END

- (void)uiWebView:(UIWebView *)sender didCommitLoadForFrame:(WebFrame *)frame
{
    gotMainFrame = true;
    mainFrame = frame;
}

- (void)handleEvent:(DOMEvent *)event
{
    if ([event.type isEqualToString:@"canplaythrough"])
        countOfVideoElementsCanPlay++;

    if (countOfVideoElementsCanPlay == 2)
        readyToTest = true;

    if ([event.type isEqualToString:@"webkitbeginfullscreen"])
        fullscreenBegin++;

    if ([event.type isEqualToString:@"webkitendfullscreen"])
        fullscreenEnd++;

    if (fullscreenBegin == 2 && fullscreenEnd == 1)
        doneTest = true;
}
@end

namespace TestWebKitAPI {

// FIXME Re-enable when https://bugs.webkit.org/show_bug.cgi?id=237125 is resovled 
#if PLATFORM(IOS) || PLATFORM(VISION)
TEST(WebKitLegacy, DISABLED_PreemptVideoFullscreen)
#else
TEST(WebKitLegacy, PreemptVideoFullscreen)
#endif
{
    RetainPtr<WebPreferences> preferences = [WebPreferences standardPreferences];
    preferences.get().mediaDataLoadsAutomatically = YES;
    preferences.get().mediaPlaybackAllowsInline = NO;
    preferences.get().mediaPlaybackRequiresUserGesture = NO;

    RetainPtr<UIWindow> uiWindow = adoptNS([[UIWindow alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    RetainPtr<UIWebView> uiWebView = adoptNS([[UIWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [uiWindow addSubview:uiWebView.get()];

    RetainPtr<PreemptVideoFullscreenUIWebViewDelegate> uiDelegate = adoptNS([[PreemptVideoFullscreenUIWebViewDelegate alloc] init]);
    uiWebView.get().delegate = uiDelegate.get();

    [uiWebView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"two-videos" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];

    Util::run(&didFinishLoad);
    Util::run(&gotMainFrame);

    callOnMainThread([&] () mutable {
        [mainFrame setTimeoutsPaused:YES];
        DOMHTMLMediaElement* video1 = (DOMHTMLMediaElement*)[[mainFrame DOMDocument] getElementById:@"video1"];
        [video1 addEventListener:@"canplaythrough" listener:uiDelegate.get() useCapture:NO];
        [video1 addEventListener:@"webkitbeginfullscreen" listener:uiDelegate.get() useCapture:NO];
        [video1 addEventListener:@"webkitendfullscreen" listener:uiDelegate.get() useCapture:NO];

        DOMHTMLMediaElement* video2 = (DOMHTMLMediaElement*)[[mainFrame DOMDocument] getElementById:@"video2"];
        [video2 addEventListener:@"canplaythrough" listener:uiDelegate.get() useCapture:NO];
        [video2 addEventListener:@"webkitbeginfullscreen" listener:uiDelegate.get() useCapture:NO];

        [video1 setSrc:@"video-with-audio.mp4"];
        [video2 setSrc:@"video-without-audio.mp4"];
        [mainFrame setTimeoutsPaused:NO];
    });

    // Wait until both video elements are ready to play.
    Util::run(&readyToTest);

    callOnMainThread([&] () mutable {
        DOMHTMLMediaElement* video1 = (DOMHTMLMediaElement*)[[mainFrame DOMDocument] getElementById:@"video1"];
        DOMHTMLMediaElement* video2 = (DOMHTMLMediaElement*)[[mainFrame DOMDocument] getElementById:@"video2"];
        [video1 play];
        [video2 play];
    });

    // Eventually, only one video will be in fullscreen.
    Util::run(&doneTest);
}

}

#endif
