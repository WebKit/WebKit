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

#import "config.h"

#if PLATFORM(MAC)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

static void *audioStateObserverChangeKVOContext = &audioStateObserverChangeKVOContext;
static bool stateDidChange;

@interface AudioStateObserver : NSObject
- (instancetype)initWithWebView:(TestWKWebView *)webView;
@property (nonatomic, readonly) BOOL isPlayingAudio;
@end

@implementation AudioStateObserver {
    RetainPtr<TestWKWebView> _webView;
}

- (instancetype)initWithWebView:(TestWKWebView *)webView
{
    if (!(self = [super init]))
        return nil;

    _webView = webView;
    [webView addObserver:self forKeyPath:@"_isPlayingAudio" options:NSKeyValueObservingOptionNew context:audioStateObserverChangeKVOContext];
    return self;
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if (context == audioStateObserverChangeKVOContext) {
        stateDidChange = true;
        return;
    }

    [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
}
@end

@interface AudioStateTestView : TestWKWebView
- (BOOL)setMuted:(_WKMediaMutedState)expected;
@end

@implementation AudioStateTestView
- (BOOL)setMuted:(_WKMediaMutedState)state
{
    [self _setPageMuted:state];

    int retryCount = 100;
    while (retryCount--) {
        if ([self _mediaMutedState] == state)
            return YES;

        TestWebKitAPI::Util::spinRunLoop(10);
    }

    return NO;
}
@end

namespace TestWebKitAPI {

TEST(WKWebView, MediaMuted)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[AudioStateTestView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:configuration.get() addToWindow:YES]);
    auto observer = adoptNS([[AudioStateObserver alloc] initWithWebView:webView.get()]);

    [webView synchronouslyLoadHTMLString:@"<video src=\"video-with-audio.mp4\" webkit-playsinline loop></video>"];

    EXPECT_EQ([webView _mediaMutedState], _WKMediaNoneMuted);

    stateDidChange = false;
    [webView evaluateJavaScript:@"document.querySelector('video').play()" completionHandler:nil];
    TestWebKitAPI::Util::run(&stateDidChange);

    EXPECT_EQ([webView _mediaMutedState], _WKMediaNoneMuted);

    [webView setMuted:_WKMediaAudioMuted];
    EXPECT_EQ([webView _mediaMutedState], _WKMediaAudioMuted);

    [webView setMuted:_WKMediaNoneMuted];
    EXPECT_EQ([webView _mediaMutedState], _WKMediaNoneMuted);
}

} // namespace TestWebKitAPI

#endif
