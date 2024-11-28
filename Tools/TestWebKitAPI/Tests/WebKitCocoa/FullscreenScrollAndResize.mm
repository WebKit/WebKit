/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
// FIXME: Fullscreen tests do not work when run on iOS because the test binary is not a real "app".
// Enable this test on iOS once that issue is resolved.

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(Fullscreen, ScrollAndResize)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences].elementFullscreenEnabled = YES;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get() addToWindow:YES]);

    [webView synchronouslyLoadHTMLString:
        @"<html><head><style>"
        @"#spacer { height: 150px; }"
        @"#target { height: 50px; background-color: green; }"
        @"</style><script>"
        @"function logEvent(event) { "
        @"    document.getElementById('target').appendChild(document.createElement('div')).innerText = event.type;"
        @"    window.webkit.messageHandlers.testHandler.postMessage(event.type);"
        @"};"
        @"window.addEventListener('resize', logEvent);"
        @"window.addEventListener('scroll', logEvent);"
        @"window.addEventListener('fullscreenchange', logEvent);"
        @"</script><body><div id=spacer><div id=target></body></html>"];

    ASSERT_FALSE([webView _isInFullscreen]);

    __block bool didScroll = false;
    __block bool didResize = false;
    __block bool didChangeFullscreen = false;

    [webView performAfterReceivingMessage:@"resize" action:^{ didResize = true; }];
    [webView performAfterReceivingMessage:@"scroll" action:^{ didScroll = true; }];
    [webView performAfterReceivingMessage:@"fullscreenchange" action:^{ didChangeFullscreen = true; }];

    [webView evaluateJavaScript:@"document.getElementById('target').scrollIntoView()" completionHandler:nil];
    TestWebKitAPI::Util::run(&didScroll);

    didScroll = false;
    [webView evaluateJavaScript:@"document.getElementById('target').requestFullscreen()" completionHandler:nil];

    TestWebKitAPI::Util::run(&didChangeFullscreen);
    TestWebKitAPI::Util::run(&didResize);
    TestWebKitAPI::Util::run(&didScroll);

    auto logContents = [webView stringByEvaluatingJavaScript:@"document.getElementById('target').innerText"];
    EXPECT_WK_STREQ("scroll\nfullscreenchange\nresize\nscroll", logContents);
}

} // namespace TestWebKitAPI

#endif
