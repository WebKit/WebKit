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

#if PLATFORM(MAC)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>

static NSString * const canEnterFullscreenKeyPath = @"_canEnterFullscreen";
static NSString * const fullscreenStateKeyPath = @"fullscreenState";
static bool canEnterFullscreenChanged;
static bool fullscreenStateChanged;

@interface FullscreenLifecycleObserver : NSObject
@end

@implementation FullscreenLifecycleObserver

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSString *, id> *)change context:(void *)context
{
    ASSERT([object isKindOfClass:WKWebView.class]);
    if ([keyPath isEqualToString:canEnterFullscreenKeyPath])
        canEnterFullscreenChanged = true;
    else if ([keyPath isEqualToString:fullscreenStateKeyPath])
        fullscreenStateChanged = true;
    else
        ASSERT_NOT_REACHED();
}

@end

TEST(Fullscreen, AudioLifecycle)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeNone];
    [configuration preferences].elementFullscreenEnabled = YES;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 480, 320) configuration:configuration.get()]);
    ASSERT_FALSE([webView _canEnterFullscreen]);
    ASSERT_EQ([webView fullscreenState], WKFullscreenStateNotInFullscreen);

    auto observer = adoptNS([[FullscreenLifecycleObserver alloc] init]);
    [webView addObserver:observer.get() forKeyPath:canEnterFullscreenKeyPath options:NSKeyValueObservingOptionNew context:nil];
    [webView addObserver:observer.get() forKeyPath:fullscreenStateKeyPath options:NSKeyValueObservingOptionNew context:nil];

    [webView synchronouslyLoadTestPageNamed:@"fullscreen-lifecycle-audio"];
    [webView evaluateJavaScript:@"go()" completionHandler:nil];
    [webView waitForMessage:@"playing"];

    ASSERT_FALSE([webView _canEnterFullscreen]);
}

TEST(Fullscreen, VideoLifecycle)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeNone];
    [configuration preferences].elementFullscreenEnabled = YES;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 480, 320) configuration:configuration.get()]);
    ASSERT_FALSE([webView _canEnterFullscreen]);
    ASSERT_EQ([webView fullscreenState], WKFullscreenStateNotInFullscreen);

    auto observer = adoptNS([[FullscreenLifecycleObserver alloc] init]);
    [webView addObserver:observer.get() forKeyPath:canEnterFullscreenKeyPath options:NSKeyValueObservingOptionNew context:nil];
    [webView addObserver:observer.get() forKeyPath:fullscreenStateKeyPath options:NSKeyValueObservingOptionNew context:nil];

    [webView loadTestPageNamed:@"large-video-test-now-playing"];
    [webView waitForMessage:@"playing"];

    TestWebKitAPI::Util::waitFor([&] {
        return [webView _canEnterFullscreen];
    });
    ASSERT_TRUE([webView _canEnterFullscreen]);
    ASSERT_TRUE(canEnterFullscreenChanged);

    [webView _enterFullscreen];

    TestWebKitAPI::Util::waitFor([&] {
        return [webView fullscreenState] == WKFullscreenStateInFullscreen;
    });
    ASSERT_EQ([webView fullscreenState], WKFullscreenStateInFullscreen);
    ASSERT_TRUE(fullscreenStateChanged);

    fullscreenStateChanged = false;
    [webView closeAllMediaPresentationsWithCompletionHandler:^{ }];

    TestWebKitAPI::Util::waitFor([&] {
        return [webView fullscreenState] == WKFullscreenStateNotInFullscreen;
    });
    ASSERT_EQ([webView fullscreenState], WKFullscreenStateNotInFullscreen);
    ASSERT_TRUE(fullscreenStateChanged);

    canEnterFullscreenChanged = false;
    [webView synchronouslyLoadHTMLString:@"<body></body>"];

    TestWebKitAPI::Util::waitFor([&] {
        return ![webView _canEnterFullscreen];
    });
    ASSERT_FALSE([webView _canEnterFullscreen]);
    ASSERT_TRUE(canEnterFullscreenChanged);
}

#endif // PLATFORM(MAC)
