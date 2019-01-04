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
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(WKWebView, StopAllMediaPlayback)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:configuration.get() addToWindow:YES]);

    [webView synchronouslyLoadHTMLString:@"<video src=\"video-with-audio.mp4\" webkit-playsinline></video>"];

    [webView objectByEvaluatingJavaScript:@"function eventToMessage(event){window.webkit.messageHandlers.testHandler.postMessage(event.type);} var video = document.querySelector('video'); video.addEventListener('playing', eventToMessage); video.addEventListener('pause', eventToMessage);"];

    __block bool didBeginPlaying = false;
    [webView performAfterReceivingMessage:@"playing" action:^{ didBeginPlaying = true; }];
    [webView evaluateJavaScript:@"document.querySelector('video').play()" completionHandler:nil];
    TestWebKitAPI::Util::run(&didBeginPlaying);

    __block bool didPause = false;
    [webView performAfterReceivingMessage:@"pause" action:^{ didPause = true; }];
    [webView _stopAllMediaPlayback];
    TestWebKitAPI::Util::run(&didPause);
}

TEST(WKWebView, SuspendResumeAllMediaPlayback)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:configuration.get() addToWindow:YES]);

    [webView synchronouslyLoadHTMLString:@"<video src=\"video-with-audio.mp4\" webkit-playsinline></video>"];

    [webView objectByEvaluatingJavaScript:@"function eventToMessage(event){window.webkit.messageHandlers.testHandler.postMessage(event.type);} var video = document.querySelector('video'); video.addEventListener('playing', eventToMessage); video.addEventListener('pause', eventToMessage);"];

    __block bool didBeginPlaying = false;
    [webView performAfterReceivingMessage:@"playing" action:^{ didBeginPlaying = true; }];
    [webView evaluateJavaScript:@"document.querySelector('video').play()" completionHandler:nil];
    TestWebKitAPI::Util::run(&didBeginPlaying);

    __block bool didPause = false;
    [webView performAfterReceivingMessage:@"pause" action:^{ didPause = true; }];
    [webView _suspendAllMediaPlayback];
    TestWebKitAPI::Util::run(&didPause);

    __block bool didReject = false;
    [webView performAfterReceivingMessage:@"rejected" action:^{ didReject = true; }];
    [webView evaluateJavaScript:@"document.querySelector('video').play().catch(() = { window.webkit.messageHandlers.testHandler.postMessage('rejected'); })" completionHandler:nil];

    didBeginPlaying = false;
    [webView performAfterReceivingMessage:@"playing" action:^{ didBeginPlaying = true; }];
    [webView _resumeAllMediaPlayback];
    TestWebKitAPI::Util::run(&didBeginPlaying);
}

} // namespace TestWebKitAPI

#endif
