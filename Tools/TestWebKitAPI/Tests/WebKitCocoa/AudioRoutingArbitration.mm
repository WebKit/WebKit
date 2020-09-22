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

#if ENABLE(ROUTING_ARBITRATION)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>

class AudioRoutingArbitration : public testing::Test {
public:
    RetainPtr<TestWKWebView> webView;

    void SetUp() final
    {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        configuration.get()._mediaDataLoadsAutomatically = YES;
        configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
        webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:configuration.get() addToWindow:YES]);

        bool isPlaying = false;
        [webView performAfterReceivingMessage:@"playing" action:[&] { isPlaying = true; }];
        [webView synchronouslyLoadTestPageNamed:@"video-with-audio"];
        TestWebKitAPI::Util::run(&isPlaying);
    }

    void TearDown() final
    {
        [webView _close];
    }

    void statusShouldBecomeEqualTo(WKWebViewAudioRoutingArbitrationStatus status)
    {
        int tries = 0;
        do {
            if ([webView _audioRoutingArbitrationStatus] == status)
                break;

            TestWebKitAPI::Util::sleep(0.1);
        } while (++tries <= 100);

        EXPECT_EQ(status, [webView _audioRoutingArbitrationStatus]);
    }
};

TEST_F(AudioRoutingArbitration, Basic)
{
    statusShouldBecomeEqualTo(WKWebViewAudioRoutingArbitrationStatusActive);
}

TEST_F(AudioRoutingArbitration, Mute)
{
    statusShouldBecomeEqualTo(WKWebViewAudioRoutingArbitrationStatusActive);

    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.querySelector('video').muted = true"];

    statusShouldBecomeEqualTo(WKWebViewAudioRoutingArbitrationStatusNone);

    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.querySelector('video').muted = false"];

    statusShouldBecomeEqualTo(WKWebViewAudioRoutingArbitrationStatusActive);
}

TEST_F(AudioRoutingArbitration, Navigation)
{
    statusShouldBecomeEqualTo(WKWebViewAudioRoutingArbitrationStatusActive);

    [webView synchronouslyLoadHTMLString:@"<html>no contents</html>"];

    statusShouldBecomeEqualTo(WKWebViewAudioRoutingArbitrationStatusNone);
}

TEST_F(AudioRoutingArbitration, Deletion)
{
    statusShouldBecomeEqualTo(WKWebViewAudioRoutingArbitrationStatusActive);

    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.querySelector('video').parentNode.innerHTML = ''"];

    statusShouldBecomeEqualTo(WKWebViewAudioRoutingArbitrationStatusNone);
}

TEST_F(AudioRoutingArbitration, Close)
{
    statusShouldBecomeEqualTo(WKWebViewAudioRoutingArbitrationStatusActive);

    [webView _close];

    statusShouldBecomeEqualTo(WKWebViewAudioRoutingArbitrationStatusNone);
}


#endif
