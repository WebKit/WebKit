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

#if PLATFORM(MAC)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFullscreenDelegate.h>
#import <wtf/RetainPtr.h>
#import <wtf/Seconds.h>

static bool didEnterFullscreen;
static bool didExitFullscreen;

@interface CloseWebViewAfterEnterFullscreenUIDelegate : NSObject <_WKFullscreenDelegate>
@end

@implementation CloseWebViewAfterEnterFullscreenUIDelegate

- (void)_webViewDidEnterFullscreen:(WKWebView *)webView
{
    didEnterFullscreen = true;
}

- (void)_webViewDidExitFullscreen:(WKWebView *)webView
{
    didExitFullscreen = true;
}
@end

namespace TestWebKitAPI {

TEST(CloseWebViewAfterEnterFullscreen, VideoFullscreen)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences]._fullScreenEnabled = YES;
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    RetainPtr<CloseWebViewAfterEnterFullscreenUIDelegate> handler = adoptNS([[CloseWebViewAfterEnterFullscreenUIDelegate alloc] init]);
    [webView _setFullscreenDelegate:handler.get()];

    [webView synchronouslyLoadHTMLString:@"<video src=\"video-with-audio.mp4\" controls></video>"];

    didEnterFullscreen = false;
    [webView evaluateJavaScript:@"document.querySelector('video').webkitEnterFullscreen()" completionHandler: nil];
    TestWebKitAPI::Util::run(&didEnterFullscreen);
    ASSERT_TRUE(didEnterFullscreen);

    // Should not crash:
    [webView _close];
}


TEST(CloseWebViewAfterEnterFullscreen, ElementFullscreen)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences]._fullScreenEnabled = YES;
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    RetainPtr<CloseWebViewAfterEnterFullscreenUIDelegate> handler = adoptNS([[CloseWebViewAfterEnterFullscreenUIDelegate alloc] init]);
    [webView _setFullscreenDelegate:handler.get()];

    [webView synchronouslyLoadHTMLString:@"<div style=\"width:100px;height:100px;background-color:red;\"></div>"];

    didEnterFullscreen = false;
    [webView evaluateJavaScript:@"document.querySelector('div').webkitRequestFullscreen()" completionHandler: nil];
    TestWebKitAPI::Util::run(&didEnterFullscreen);
    ASSERT_TRUE(didEnterFullscreen);

    // Should not crash:
    [webView _close];
}

} // namespace TestWebKitAPI

#endif
