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

#include "config.h"

#if PLATFORM(COCOA)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

static bool isInFullscreen = false;
static bool isOutOfFullscreen = false;

TEST(Fullscreen, Alert)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
    [configuration preferences]._fullScreenEnabled = YES;

    auto checkFullscreen = [&] {
        isInFullscreen = [webView _isInFullscreen];
        isOutOfFullscreen = !isInFullscreen;
    };

    [webView performAfterReceivingMessage:@"fullscreenchange" action:checkFullscreen];

    [webView synchronouslyLoadHTMLString:
        @"<html><head><script>"
        @"document.addEventListener('webkitfullscreenchange', e => { window.webkit.messageHandlers.testHandler.postMessage(\"fullscreenchange\") });"
        @"function enterFullscreen() { document.body.webkitRequestFullscreen(); }"
        @"</script></head><body>some text</body></html>"];

    ASSERT_FALSE([webView _isInFullscreen]);

    [webView evaluateJavaScript:@"enterFullscreen()" completionHandler:nil];

    TestWebKitAPI::Util::run(&isInFullscreen);

    [webView evaluateJavaScript:@"alert()" completionHandler:nil];

    TestWebKitAPI::Util::run(&isOutOfFullscreen);
}

} // namespace TestWebKitAPI

#endif
