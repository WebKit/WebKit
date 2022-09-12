/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

TEST(Fullscreen, RemoveNodeBeforeEnter)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences]._fullScreenEnabled = YES;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get() addToWindow:YES]);

    [webView synchronouslyLoadHTMLString:
        @"<html><head><script>"
        @"function enterFullscreenThenRemove() { "
        @"    let target = document.querySelector('div');"
        @"    target.webkitRequestFullscreen();"
        @"    setTimeout(() => { "
        @"        target.parentNode.removeChild(target);"
        @"        window.webkit.messageHandlers.testHandler.postMessage(\"noderemoved\");"
        @"    });"
        @"}"
        @"</script></head><body><div>some text</div></body></html>"];

    ASSERT_FALSE([webView _isInFullscreen]);

    __block bool nodeRemoved = false;
    [webView performAfterReceivingMessage:@"noderemoved" action:^{ nodeRemoved = true; }];

    [webView evaluateJavaScript:@"enterFullscreenThenRemove()" completionHandler:nil];

    TestWebKitAPI::Util::run(&nodeRemoved);

    // Allow the potential negative result time to occur.
    TestWebKitAPI::Util::runFor(0.5_s);

    // Fullscreen mode should eventually close.
    int tries = 0;
    do {
        if (![webView _isInFullscreen])
            break;
        TestWebKitAPI::Util::runFor(0.1_s);
    } while (++tries <= 100);

    ASSERT_FALSE([webView _isInFullscreen]);
}

} // namespace TestWebKitAPI

#endif
