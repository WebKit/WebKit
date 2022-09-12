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

#import "config.h"

#if PLATFORM(MAC)

#import "TestCocoa.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFullscreenDelegate.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(Fullscreen, PointerLeave)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences]._fullScreenEnabled = YES;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    bool pointerenter = false;
    bool pointerleave = false;
    bool fullscreenchange = false;
    [webView performAfterReceivingMessage:@"pointerenter" action:[&] { pointerenter = true; }];
    [webView performAfterReceivingMessage:@"pointerleave" action:[&] { pointerleave = true; }];
    [webView performAfterReceivingMessage:@"webkitfullscreenchange" action:[&] { fullscreenchange = true; }];
    [webView synchronouslyLoadHTMLString:
        @"<style>#target { width:100px; height: 100px; }</style>"
        @"<div id=target></div>"
        @"<script>"
        @"let eventToMessage = event => { window.webkit.messageHandlers.testHandler.postMessage(event.type); }"
        @"target.addEventListener('pointerenter', eventToMessage, {once:true});"
        @"target.addEventListener('pointerleave', eventToMessage, {once:true});"
        @"document.addEventListener('webkitfullscreenchange', eventToMessage, {once:true});"
        @"</script>"];

    [webView mouseMoveToPoint:NSMakePoint(50, 50) withFlags:0];
    Util::runFor(&pointerenter, 5_s);

    [webView objectByEvaluatingJavaScriptWithUserGesture:@"target.webkitRequestFullscreen()"];
    Util::runFor(&fullscreenchange, 5_s);
    Util::runFor(&pointerleave, 5_s);
}

}

#endif
