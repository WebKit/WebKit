/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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

@interface NowPlayingSessionObserver : NSObject
- (void)waitForHasActiveNowPlayingSessionChanged;
@end

@implementation NowPlayingSessionObserver {
    bool _hasActiveNowPlayingSessionChanged;
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSString *, id> *)change context:(void *)context
{
    ASSERT([keyPath isEqualToString:nowPlayingSessionKeyPath]);
    ASSERT([object isKindOfClass:WKWebView.class]);
    _hasActiveNowPlayingSessionChanged = true;
}

- (void)waitForHasActiveNowPlayingSessionChanged
{
    _hasActiveNowPlayingSessionChanged = false;
    TestWebKitAPI::Util::run(&_hasActiveNowPlayingSessionChanged);
}

static RetainPtr<TestWKWebView> createWebView()
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeNone];
#if PLATFORM(IOS_FAMILY)
    [configuration setAllowsInlineMediaPlayback:YES];
#endif
    return adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 480, 320) configuration:configuration.get()]);
}

TEST(NowPlayingSession, NoSession)
{
    RetainPtr webView = createWebView();
    EXPECT_FALSE([webView _hasActiveNowPlayingSession]);
}

TEST(NowPlayingSession, HasSession)
{
    RetainPtr webView = createWebView();
    EXPECT_FALSE([webView _hasActiveNowPlayingSession]);

    RetainPtr observer = adoptNS([[NowPlayingSessionObserver alloc] init]);
    [webView addObserver:observer.get() forKeyPath:nowPlayingSessionKeyPath options:NSKeyValueObservingOptionNew context:nil];

    [webView synchronouslyLoadTestPageNamed:@"now-playing-session-test"];
    EXPECT_FALSE([webView _hasActiveNowPlayingSession]);

    [webView evaluateJavaScript:@"playInMainFrame()" completionHandler:nil];
    [observer waitForHasActiveNowPlayingSessionChanged];
    EXPECT_TRUE([webView _hasActiveNowPlayingSession]);

    [webView removeObserver:observer.get() forKeyPath:nowPlayingSessionKeyPath];
}

TEST(NowPlayingSession, NavigateAfterHasSession)
{
    RetainPtr webView = createWebView();
    EXPECT_FALSE([webView _hasActiveNowPlayingSession]);

    RetainPtr observer = adoptNS([[NowPlayingSessionObserver alloc] init]);
    [webView addObserver:observer.get() forKeyPath:nowPlayingSessionKeyPath options:NSKeyValueObservingOptionNew context:nil];

    [webView synchronouslyLoadTestPageNamed:@"now-playing-session-test"];
    EXPECT_FALSE([webView _hasActiveNowPlayingSession]);

    [webView evaluateJavaScript:@"playInMainFrame()" completionHandler:nil];
    [observer waitForHasActiveNowPlayingSessionChanged];
    EXPECT_TRUE([webView _hasActiveNowPlayingSession]);

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    EXPECT_FALSE([webView _hasActiveNowPlayingSession]);

    [webView removeObserver:observer.get() forKeyPath:nowPlayingSessionKeyPath];
}

TEST(NowPlayingSession, RemoveVideoElementAfterHasSession)
{
    RetainPtr webView = createWebView();
    EXPECT_FALSE([webView _hasActiveNowPlayingSession]);

    RetainPtr observer = adoptNS([[NowPlayingSessionObserver alloc] init]);
    [webView addObserver:observer.get() forKeyPath:nowPlayingSessionKeyPath options:NSKeyValueObservingOptionNew context:nil];

    [webView synchronouslyLoadTestPageNamed:@"now-playing-session-test"];
    EXPECT_FALSE([webView _hasActiveNowPlayingSession]);

    [webView evaluateJavaScript:@"playInMainFrame()" completionHandler:nil];
    [observer waitForHasActiveNowPlayingSessionChanged];
    EXPECT_TRUE([webView _hasActiveNowPlayingSession]);

    [webView evaluateJavaScript:@"document.querySelector('video').remove()" completionHandler:nil];
    [observer waitForHasActiveNowPlayingSessionChanged];
    EXPECT_FALSE([webView _hasActiveNowPlayingSession]);

    [webView removeObserver:observer.get() forKeyPath:nowPlayingSessionKeyPath];
}

TEST(NowPlayingSession, NavigateAfterHasSessionAndPlayAgain)
{
    RetainPtr webView = createWebView();
    EXPECT_FALSE([webView _hasActiveNowPlayingSession]);

    RetainPtr observer = adoptNS([[NowPlayingSessionObserver alloc] init]);
    [webView addObserver:observer.get() forKeyPath:nowPlayingSessionKeyPath options:NSKeyValueObservingOptionNew context:nil];

    [webView synchronouslyLoadTestPageNamed:@"now-playing-session-test"];
    EXPECT_FALSE([webView _hasActiveNowPlayingSession]);

    [webView evaluateJavaScript:@"playInMainFrame()" completionHandler:nil];
    [observer waitForHasActiveNowPlayingSessionChanged];
    EXPECT_TRUE([webView _hasActiveNowPlayingSession]);

    [webView synchronouslyLoadTestPageNamed:@"now-playing-session-test"];
    EXPECT_FALSE([webView _hasActiveNowPlayingSession]);

    [webView evaluateJavaScript:@"playInMainFrame()" completionHandler:nil];
    [observer waitForHasActiveNowPlayingSessionChanged];
    EXPECT_TRUE([webView _hasActiveNowPlayingSession]);

    [webView removeObserver:observer.get() forKeyPath:nowPlayingSessionKeyPath];
}

TEST(NowPlayingSession, NavigateToFragmentAfterHasSession)
{
    RetainPtr webView = createWebView();
    EXPECT_FALSE([webView _hasActiveNowPlayingSession]);

    RetainPtr observer = adoptNS([[NowPlayingSessionObserver alloc] init]);
    [webView addObserver:observer.get() forKeyPath:nowPlayingSessionKeyPath options:NSKeyValueObservingOptionNew context:nil];

    [webView synchronouslyLoadTestPageNamed:@"now-playing-session-test"];
    EXPECT_FALSE([webView _hasActiveNowPlayingSession]);

    [webView evaluateJavaScript:@"playInMainFrame()" completionHandler:nil];
    [observer waitForHasActiveNowPlayingSessionChanged];
    EXPECT_TRUE([webView _hasActiveNowPlayingSession]);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"#a" relativeToURL:[webView URL]]]];
    [webView _test_waitForDidSameDocumentNavigation];
    EXPECT_TRUE([webView _hasActiveNowPlayingSession]);

    [webView removeObserver:observer.get() forKeyPath:nowPlayingSessionKeyPath];
}

TEST(NowPlayingSession, HasSessionInSubframe)
{
    RetainPtr webView = createWebView();
    EXPECT_FALSE([webView _hasActiveNowPlayingSession]);

    RetainPtr observer = adoptNS([[NowPlayingSessionObserver alloc] init]);
    [webView addObserver:observer.get() forKeyPath:nowPlayingSessionKeyPath options:NSKeyValueObservingOptionNew context:nil];

    [webView synchronouslyLoadTestPageNamed:@"now-playing-session-test"];
    EXPECT_FALSE([webView _hasActiveNowPlayingSession]);

    [webView evaluateJavaScript:@"playInSubframe()" completionHandler:nil];
    [observer waitForHasActiveNowPlayingSessionChanged];
    EXPECT_TRUE([webView _hasActiveNowPlayingSession]);

    [webView removeObserver:observer.get() forKeyPath:nowPlayingSessionKeyPath];
}

TEST(NowPlayingSession, RemoveSubframeAfterHasSessionInSubframe)
{
    RetainPtr webView = createWebView();
    EXPECT_FALSE([webView _hasActiveNowPlayingSession]);

    RetainPtr observer = adoptNS([[NowPlayingSessionObserver alloc] init]);
    [webView addObserver:observer.get() forKeyPath:nowPlayingSessionKeyPath options:NSKeyValueObservingOptionNew context:nil];

    [webView synchronouslyLoadTestPageNamed:@"now-playing-session-test"];
    EXPECT_FALSE([webView _hasActiveNowPlayingSession]);

    [webView evaluateJavaScript:@"playInSubframe()" completionHandler:nil];
    [observer waitForHasActiveNowPlayingSessionChanged];
    EXPECT_TRUE([webView _hasActiveNowPlayingSession]);

    [webView evaluateJavaScript:@"document.querySelector('iframe').remove()" completionHandler:nil];
    [observer waitForHasActiveNowPlayingSessionChanged];
    EXPECT_FALSE([webView _hasActiveNowPlayingSession]);

    [webView removeObserver:observer.get() forKeyPath:nowPlayingSessionKeyPath];
}

TEST(NowPlayingSession, NavigateSubframeAfterHasSessionInSubframe)
{
    RetainPtr webView = createWebView();
    EXPECT_FALSE([webView _hasActiveNowPlayingSession]);

    RetainPtr observer = adoptNS([[NowPlayingSessionObserver alloc] init]);
    [webView addObserver:observer.get() forKeyPath:nowPlayingSessionKeyPath options:NSKeyValueObservingOptionNew context:nil];

    [webView synchronouslyLoadTestPageNamed:@"now-playing-session-test"];
    EXPECT_FALSE([webView _hasActiveNowPlayingSession]);

    [webView evaluateJavaScript:@"playInSubframe()" completionHandler:nil];
    [observer waitForHasActiveNowPlayingSessionChanged];
    EXPECT_TRUE([webView _hasActiveNowPlayingSession]);

    [webView evaluateJavaScript:@"document.querySelector('iframe').srcdoc = ''" completionHandler:nil];
    [observer waitForHasActiveNowPlayingSessionChanged];
    EXPECT_FALSE([webView _hasActiveNowPlayingSession]);

    [webView removeObserver:observer.get() forKeyPath:nowPlayingSessionKeyPath];
}

TEST(NowPlayingSession, KillWebContentProcessAfterHasSession)
{
    RetainPtr webView = createWebView();
    EXPECT_FALSE([webView _hasActiveNowPlayingSession]);

    RetainPtr observer = adoptNS([[NowPlayingSessionObserver alloc] init]);
    [webView addObserver:observer.get() forKeyPath:nowPlayingSessionKeyPath options:NSKeyValueObservingOptionNew context:nil];

    [webView synchronouslyLoadTestPageNamed:@"now-playing-session-test"];
    EXPECT_FALSE([webView _hasActiveNowPlayingSession]);

    [webView evaluateJavaScript:@"playInMainFrame()" completionHandler:nil];
    [observer waitForHasActiveNowPlayingSessionChanged];
    EXPECT_TRUE([webView _hasActiveNowPlayingSession]);

    [webView _killWebContentProcessAndResetState];
    EXPECT_FALSE([webView _hasActiveNowPlayingSession]);

    [webView removeObserver:observer.get() forKeyPath:nowPlayingSessionKeyPath];
}

@end
