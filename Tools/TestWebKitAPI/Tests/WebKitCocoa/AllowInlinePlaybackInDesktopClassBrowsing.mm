/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>

namespace TestWebKitAPI {

TEST(Fullscreen, AllowsInlinePlaybackInDesktopContentMode)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setAllowsInlineMediaPlayback:YES];
    [configuration _setInlineMediaPlaybackRequiresPlaysInlineAttribute:YES];

    RetainPtr preferences = adoptNS([[WKWebpagePreferences alloc] init]);
    [preferences setPreferredContentMode:WKContentModeDesktop];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 640, 480) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"large-video-no-playsinline-attr" preferences:preferences.get()];

    bool beganFullscreen = false;
    [webView performAfterReceivingMessage:@"beganFullscreen" action:[&] {
        beganFullscreen = true;
    }];

    [webView performAfterReceivingMessage:@"playing" action:[&] {
        [webView objectByEvaluatingJavaScriptWithUserGesture:@"pause()"];
    }];

    [webView objectByEvaluatingJavaScriptWithUserGesture:@"play()"];

    [webView waitForMessage:@"paused"];
    EXPECT_FALSE(beganFullscreen);
}

TEST(Fullscreen, DisallowsInlinePlaybackInMobileContentMode)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setAllowsInlineMediaPlayback:YES];
    [configuration _setInlineMediaPlaybackRequiresPlaysInlineAttribute:YES];

    RetainPtr preferences = adoptNS([[WKWebpagePreferences alloc] init]);
    [preferences setPreferredContentMode:WKContentModeMobile];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 640, 480) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"large-video-no-playsinline-attr" preferences:preferences.get()];

    bool beganFullscreen = false;
    [webView performAfterReceivingMessage:@"beganFullscreen" action:[&] {
        beganFullscreen = true;
    }];

    [webView performAfterReceivingMessage:@"playing" action:[&] {
        [webView objectByEvaluatingJavaScriptWithUserGesture:@"pause()"];
    }];

    [webView objectByEvaluatingJavaScriptWithUserGesture:@"play()"];

    [webView waitForMessage:@"paused"];
    EXPECT_TRUE(beganFullscreen);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY)
