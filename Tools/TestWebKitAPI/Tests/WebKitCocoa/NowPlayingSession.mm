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

#import "config.h"

#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>

static NSString * const nowPlayingSessionKeyPath = @"_hasActiveNowPlayingSession";
static bool hasActiveNowPlayingSessionChanged;

@interface NowPlayingSessionObserver : NSObject
@end

@implementation NowPlayingSessionObserver

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSString *, id> *)change context:(void *)context
{
    ASSERT([keyPath isEqualToString:nowPlayingSessionKeyPath]);
    ASSERT([object isKindOfClass:WKWebView.class]);
    hasActiveNowPlayingSessionChanged = true;
}

TEST(NowPlayingSession, NoSession)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 480, 320)]);
    ASSERT_FALSE([webView _hasActiveNowPlayingSession]);
}

TEST(NowPlayingSession, HasSession)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeNone];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 480, 320) configuration:configuration.get()]);
    ASSERT_FALSE([webView _hasActiveNowPlayingSession]);

    RetainPtr observer = adoptNS([[NowPlayingSessionObserver alloc] init]);
    [webView addObserver:observer.get() forKeyPath:nowPlayingSessionKeyPath options:NSKeyValueObservingOptionNew context:nil];

    [webView loadTestPageNamed:@"large-video-test-now-playing"];
    [webView waitForMessage:@"playing"];

    if (!hasActiveNowPlayingSessionChanged)
        TestWebKitAPI::Util::run(&hasActiveNowPlayingSessionChanged);

    ASSERT_TRUE([webView _hasActiveNowPlayingSession]);
}

TEST(NowPlayingSession, NavigateAfterHasSession)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeNone];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 480, 320) configuration:configuration.get()]);
    ASSERT_FALSE([webView _hasActiveNowPlayingSession]);

    RetainPtr observer = adoptNS([[NowPlayingSessionObserver alloc] init]);
    [webView addObserver:observer.get() forKeyPath:nowPlayingSessionKeyPath options:NSKeyValueObservingOptionNew context:nil];

    [webView loadTestPageNamed:@"large-video-test-now-playing"];
    [webView waitForMessage:@"playing"];

    if (!hasActiveNowPlayingSessionChanged)
        TestWebKitAPI::Util::run(&hasActiveNowPlayingSessionChanged);

    ASSERT_TRUE([webView _hasActiveNowPlayingSession]);

    hasActiveNowPlayingSessionChanged = false;
    [webView loadTestPageNamed:@"simple"];

    if (!hasActiveNowPlayingSessionChanged)
        TestWebKitAPI::Util::run(&hasActiveNowPlayingSessionChanged);

    ASSERT_FALSE([webView _hasActiveNowPlayingSession]);
}

TEST(NowPlayingSession, NavigateToFragmentAfterHasSession)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeNone];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 480, 320) configuration:configuration.get()]);
    ASSERT_FALSE([webView _hasActiveNowPlayingSession]);

    RetainPtr observer = adoptNS([[NowPlayingSessionObserver alloc] init]);
    [webView addObserver:observer.get() forKeyPath:nowPlayingSessionKeyPath options:NSKeyValueObservingOptionNew context:nil];

    [webView loadTestPageNamed:@"large-video-test-now-playing"];
    [webView waitForMessage:@"playing"];

    if (!hasActiveNowPlayingSessionChanged)
        TestWebKitAPI::Util::run(&hasActiveNowPlayingSessionChanged);

    ASSERT_TRUE([webView _hasActiveNowPlayingSession]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"#a" relativeToURL:[webView URL]]]];
    [webView _test_waitForDidSameDocumentNavigation];

    ASSERT_TRUE([webView _hasActiveNowPlayingSession]);
}

TEST(NowPlayingSession, LoadSubframeAfterHasSession)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeNone];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 480, 320) configuration:configuration.get()]);
    ASSERT_FALSE([webView _hasActiveNowPlayingSession]);

    RetainPtr observer = adoptNS([[NowPlayingSessionObserver alloc] init]);
    [webView addObserver:observer.get() forKeyPath:nowPlayingSessionKeyPath options:NSKeyValueObservingOptionNew context:nil];

    [webView loadTestPageNamed:@"large-video-test-now-playing"];
    [webView waitForMessage:@"playing"];

    if (!hasActiveNowPlayingSessionChanged)
        TestWebKitAPI::Util::run(&hasActiveNowPlayingSessionChanged);

    ASSERT_TRUE([webView _hasActiveNowPlayingSession]);

    [webView evaluateJavaScript:@"loadSubframe()" completionHandler:nil];
    [webView waitForMessage:@"subframeLoaded"];

    ASSERT_TRUE([webView _hasActiveNowPlayingSession]);
}

@end
