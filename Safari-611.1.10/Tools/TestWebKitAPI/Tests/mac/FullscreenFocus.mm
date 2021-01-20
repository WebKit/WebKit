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

#if PLATFORM(MAC)

#import "TestCocoa.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFullscreenDelegate.h>
#import <wtf/RetainPtr.h>

static bool didEnterFullscreen;
static bool didExitFullscreen;

@interface FullscreenFocusUIDelegate : NSObject <_WKFullscreenDelegate>
@end

@implementation FullscreenFocusUIDelegate

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

TEST(Fullscreen, Focus)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences]._fullScreenEnabled = YES;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    auto handler = adoptNS([[FullscreenFocusUIDelegate alloc] init]);
    [webView _setFullscreenDelegate:handler.get()];

    bool canplaythrough = false;
    [webView performAfterReceivingMessage:@"canplaythrough" action:[&] { canplaythrough = true; }];
    [webView synchronouslyLoadHTMLString:
        @"<video id=\"one\" controls></video><video id=\"two\" controls></video>"
        @"<script>"
        @"one.addEventListener('canplaythrough', event => { window.webkit.messageHandlers.testHandler.postMessage('canplaythrough'); }, {once:true});"
        @"one.src = two.src = 'video-with-audio.mp4';"
        @"</script>"];

    Util::run(&canplaythrough);

    didEnterFullscreen = false;
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"one.webkitEnterFullscreen()"];
    Util::run(&didEnterFullscreen);

    didExitFullscreen = false;
    [webView objectByEvaluatingJavaScript:@"one.webkitExitFullscreen()"];
    Util::run(&didExitFullscreen);

    EXPECT_WK_STREQ("[object HTMLBodyElement]", [webView stringByEvaluatingJavaScript:@"String(document.activeElement)"]);
}

}

#endif
