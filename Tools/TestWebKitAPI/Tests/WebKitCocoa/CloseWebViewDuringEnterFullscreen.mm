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

#include "config.h"

#if WK_API_ENABLED && PLATFORM(MAC)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFullscreenDelegate.h>
#import <wtf/RetainPtr.h>
#import <wtf/Seconds.h>

static bool willEnterFullscreen;
static bool willExitFullscreen;

@interface CloseWebViewDuringEnterFullscreenUIDelegate : NSObject <_WKFullscreenDelegate>
@end

@implementation CloseWebViewDuringEnterFullscreenUIDelegate

- (void)_webViewWillEnterFullscreen:(WKWebView *)webView
{
    willEnterFullscreen = true;
}

- (void)_webViewWillExitFullscreen:(WKWebView *)webView
{
    willExitFullscreen = true;
}
@end

namespace TestWebKitAPI {

TEST(CloseWebViewDuringEnterFullscreen, VideoFullscreen)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences]._fullScreenEnabled = YES;
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    RetainPtr<CloseWebViewDuringEnterFullscreenUIDelegate> handler = adoptNS([[CloseWebViewDuringEnterFullscreenUIDelegate alloc] init]);
    [webView _setFullscreenDelegate:handler.get()];

    [webView synchronouslyLoadHTMLString:@"<video src=\"video-with-audio.mp4\" controls></video>"];

    willEnterFullscreen = false;
    [webView evaluateJavaScript:@"document.querySelector('video').webkitEnterFullscreen()" completionHandler: nil];
    TestWebKitAPI::Util::run(&willEnterFullscreen);
    TestWebKitAPI::Util::sleep(0.2);

    // Should not crash:
    [webView _close];
}


TEST(CloseWebViewDuringEnterFullscreen, ElementFullscreen)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences]._fullScreenEnabled = YES;
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    RetainPtr<CloseWebViewDuringEnterFullscreenUIDelegate> handler = adoptNS([[CloseWebViewDuringEnterFullscreenUIDelegate alloc] init]);
    [webView _setFullscreenDelegate:handler.get()];

    [webView synchronouslyLoadHTMLString:@"<div style=\"width:100px;height:100px;background-color:red;\"></div>"];

    willEnterFullscreen = false;
    [webView evaluateJavaScript:@"document.querySelector('div').webkitRequestFullscreen()" completionHandler: nil];
    TestWebKitAPI::Util::run(&willEnterFullscreen);
    TestWebKitAPI::Util::sleep(0.2);

    // Should not crash:
    [webView _close];
}

} // namespace TestWebKitAPI

#endif
