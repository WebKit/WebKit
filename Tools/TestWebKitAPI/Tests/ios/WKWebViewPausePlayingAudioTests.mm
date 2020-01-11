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

#if PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>

namespace TestWebKitAPI {

static RetainPtr<WKWebViewConfiguration> autoplayingConfiguration()
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    configuration.get().allowsInlineMediaPlayback = YES;
    configuration.get()._inlineMediaPlaybackRequiresPlaysInlineAttribute = NO;
    return configuration;
}

TEST(WKWebViewPausePlayingAudioTests, InWindow)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:autoplayingConfiguration().get() addToWindow:YES]);

    bool isPlaying = false;
    [webView performAfterReceivingMessage:@"playing" action:[&] { isPlaying = true; }];

    [webView synchronouslyLoadTestPageNamed:@"video-with-audio"];
    [webView objectByEvaluatingJavaScript:@"document.querySelector('video').addEventListener('pause', paused)"];
    Util::run(&isPlaying);

    bool isPaused = false;
    [webView performAfterReceivingMessage:@"paused" action:[&] { isPaused = true; }];

    [NSNotificationCenter.defaultCenter postNotificationName:UIApplicationDidEnterBackgroundNotification object:UIApplication.sharedApplication userInfo:@{@"isSuspendedUnderLock": @NO }];
    [NSNotificationCenter.defaultCenter postNotificationName:UISceneDidEnterBackgroundNotification object:webView.get().window.windowScene];
    Util::run(&isPaused);

    [webView objectByEvaluatingJavaScript:@"document.querySelector('video').addEventListener('play', playing)"];

    isPlaying = false;
    [NSNotificationCenter.defaultCenter postNotificationName:UIApplicationWillEnterForegroundNotification object:UIApplication.sharedApplication userInfo:@{@"isSuspendedUnderLock": @NO }];
    [NSNotificationCenter.defaultCenter postNotificationName:UISceneWillEnterForegroundNotification object:webView.get().window.windowScene];
    Util::run(&isPlaying);
}

TEST(WKWebViewPausePlayingAudioTests, OutOfWindow)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:autoplayingConfiguration().get() addToWindow:YES]);

    bool isPlaying = false;
    [webView performAfterReceivingMessage:@"playing" action:[&] { isPlaying = true; }];

    [webView synchronouslyLoadTestPageNamed:@"video-with-audio"];
    Util::run(&isPlaying);

    bool isHidden = false;
    [webView performAfterReceivingMessage:@"hidden" action:[&] { isHidden = true; }];
    [webView objectByEvaluatingJavaScript:@"document.addEventListener('visibilitychange', event => { if (document.hidden) window.webkit.messageHandlers.testHandler.postMessage('hidden') })"];

    [webView removeFromSuperview];
    Util::run(&isHidden);

    bool isPaused = false;
    [webView objectByEvaluatingJavaScript:@"document.querySelector('video').addEventListener('pause', paused)"];
    [webView performAfterReceivingMessage:@"paused" action:[&] { isPaused = true; }];

    [NSNotificationCenter.defaultCenter postNotificationName:UIApplicationDidEnterBackgroundNotification object:UIApplication.sharedApplication userInfo:@{@"isSuspendedUnderLock": @NO }];
    [NSNotificationCenter.defaultCenter postNotificationName:UISceneDidEnterBackgroundNotification object:webView.get().window.windowScene];
    Util::run(&isPaused);

    [webView objectByEvaluatingJavaScript:@"document.querySelector('video').addEventListener('play', playing)"];

    isPlaying = false;
    [NSNotificationCenter.defaultCenter postNotificationName:UIApplicationWillEnterForegroundNotification object:UIApplication.sharedApplication userInfo:@{@"isSuspendedUnderLock": @NO }];
    [NSNotificationCenter.defaultCenter postNotificationName:UISceneWillEnterForegroundNotification object:webView.get().window.windowScene];

    Util::run(&isPlaying);
}

}

#endif
