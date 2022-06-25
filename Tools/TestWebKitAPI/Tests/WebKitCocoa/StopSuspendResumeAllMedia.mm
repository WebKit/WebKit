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

#if PLATFORM(MAC)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
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
    [webView pauseAllMediaPlaybackWithCompletionHandler:nil];
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
    [webView setAllMediaPlaybackSuspended:YES completionHandler:nil];
    TestWebKitAPI::Util::run(&didPause);

    __block bool didReject = false;
    [webView performAfterReceivingMessage:@"rejected" action:^{ didReject = true; }];
    [webView evaluateJavaScript:@"document.querySelector('video').play().catch(() => { window.webkit.messageHandlers.testHandler.postMessage('rejected'); })" completionHandler:nil];
    TestWebKitAPI::Util::run(&didReject);

    didBeginPlaying = false;
    [webView performAfterReceivingMessage:@"playing" action:^{ didBeginPlaying = true; }];
    [webView setAllMediaPlaybackSuspended:NO completionHandler:nil];
    TestWebKitAPI::Util::run(&didBeginPlaying);
}

TEST(WKWebView, SuspendResumeAllMediaPlaybackMultipleTimes)
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
    [webView setAllMediaPlaybackSuspended:YES completionHandler:nil];
    TestWebKitAPI::Util::run(&didPause);

    __block bool didReject = false;
    [webView performAfterReceivingMessage:@"rejected" action:^{ didReject = true; }];
    [webView evaluateJavaScript:@"document.querySelector('video').play().catch(() => { window.webkit.messageHandlers.testHandler.postMessage('rejected'); })" completionHandler:nil];
    TestWebKitAPI::Util::run(&didReject);

    // Suspend again to increment the counter.
    __block bool isDone = false;
    [webView setAllMediaPlaybackSuspended:YES completionHandler:^{
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);

    isDone = false;
    [webView setAllMediaPlaybackSuspended:NO completionHandler:^{
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);

    // Make sure the media is still suspended.
    didReject = false;
    [webView performAfterReceivingMessage:@"rejected" action:^{ didReject = true; }];
    [webView evaluateJavaScript:@"document.querySelector('video').play().catch(() => { window.webkit.messageHandlers.testHandler.postMessage('rejected'); })" completionHandler:nil];
    TestWebKitAPI::Util::run(&didReject);

    didBeginPlaying = false;
    [webView performAfterReceivingMessage:@"playing" action:^{ didBeginPlaying = true; }];
    [webView setAllMediaPlaybackSuspended:NO completionHandler:nil];
    TestWebKitAPI::Util::run(&didBeginPlaying);
}

TEST(WKWebView, CheckMediaPlaybackSuspended)
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
    [webView setAllMediaPlaybackSuspended:YES completionHandler:nil];
    TestWebKitAPI::Util::run(&didPause);

    __block bool isDone = false;
    [webView requestMediaPlaybackStateWithCompletionHandler:^(WKMediaPlaybackState state) {
        EXPECT_EQ(state, WKMediaPlaybackStateSuspended);
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);

    didBeginPlaying = false;
    [webView performAfterReceivingMessage:@"playing" action:^{ didBeginPlaying = true; }];
    [webView setAllMediaPlaybackSuspended:NO completionHandler:nil];
    TestWebKitAPI::Util::run(&didBeginPlaying);
    
    isDone = false;
    [webView requestMediaPlaybackStateWithCompletionHandler:^(WKMediaPlaybackState state) {
        EXPECT_EQ(state, WKMediaPlaybackStatePlaying);
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
}

TEST(WKWebView, CheckMediaPlaybackExists)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:configuration.get() addToWindow:YES]);

    [webView synchronouslyLoadHTMLString:@"start network process"];

    __block bool isDone = false;
    [webView requestMediaPlaybackStateWithCompletionHandler:^(WKMediaPlaybackState state) {
        EXPECT_EQ(state, WKMediaPlaybackStateNone);
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
    
    [webView synchronouslyLoadHTMLString:@"<video src=\"video-with-audio.mp4\" webkit-playsinline></video>"];

    isDone = false;
    [webView requestMediaPlaybackStateWithCompletionHandler:^(WKMediaPlaybackState state) {
        EXPECT_EQ(state, WKMediaPlaybackStatePlaying);
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
    
    [webView synchronouslyLoadHTMLString:@"<body></body>"];
    isDone = false;
    [webView requestMediaPlaybackStateWithCompletionHandler:^(WKMediaPlaybackState state) {
        EXPECT_EQ(state, WKMediaPlaybackStateNone);
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
}

TEST(WKWebView, CheckMediaPlaybackPaused)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:configuration.get() addToWindow:YES]);

    [webView synchronouslyLoadHTMLString:@"<video src=\"video-with-audio.mp4\" webkit-playsinline></video>"];

    [webView objectByEvaluatingJavaScript:@"function eventToMessage(event){window.webkit.messageHandlers.testHandler.postMessage(event.type);} var video = document.querySelector('video'); video.addEventListener('playing', eventToMessage); video.addEventListener('pause', eventToMessage);"];

    __block bool didBeginPlaying = false;
    [webView performAfterReceivingMessage:@"playing" action:^{ didBeginPlaying = true; }];
    [webView evaluateJavaScript:@"document.querySelector('video').play()" completionHandler:nil];
    TestWebKitAPI::Util::run(&didBeginPlaying);

    __block bool isDone = false;
    [webView requestMediaPlaybackStateWithCompletionHandler:^(WKMediaPlaybackState state) {
        EXPECT_EQ(state, WKMediaPlaybackStatePlaying);
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
    
    __block bool didPause = false;
    [webView performAfterReceivingMessage:@"pause" action:^{ didPause = true; }];
    [webView pauseAllMediaPlaybackWithCompletionHandler:nil];
    TestWebKitAPI::Util::run(&didPause);
    
    isDone = false;
    [webView requestMediaPlaybackStateWithCompletionHandler:^(WKMediaPlaybackState state) {
        EXPECT_TRUE(WKMediaPlaybackStatePaused);
        isDone = true;
    }];
    TestWebKitAPI::Util::run(&isDone);
}

} // namespace TestWebKitAPI

#endif
