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
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/WallTime.h>

class AudioRoutingArbitration : public testing::Test {
public:
    RetainPtr<TestWKWebView> webView;

    void SetUp() final
    {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
        configuration.get().processPool = (WKProcessPool *)context.get();
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

    void statusShouldBecomeEqualTo(WKWebViewAudioRoutingArbitrationStatus status, const char* message, bool forceGC = false)
    {
        int tries = 0;
        do {
            if ([webView _audioRoutingArbitrationStatus] == status)
                break;

            if (forceGC)
                [webView.get().configuration.processPool _garbageCollectJavaScriptObjectsForTesting];
            TestWebKitAPI::Util::sleep(0.1);
        } while (++tries <= 100);

        EXPECT_EQ(status, [webView _audioRoutingArbitrationStatus]) << message;
    }
};

TEST_F(AudioRoutingArbitration, Basic)
{
    statusShouldBecomeEqualTo(WKWebViewAudioRoutingArbitrationStatusActive, "Basic");
}

TEST_F(AudioRoutingArbitration, Mute)
{
    statusShouldBecomeEqualTo(WKWebViewAudioRoutingArbitrationStatusActive, "Mute 1");

    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.querySelector('video').muted = true"];

    statusShouldBecomeEqualTo(WKWebViewAudioRoutingArbitrationStatusNone, "Mute 2");

    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.querySelector('video').muted = false"];

    statusShouldBecomeEqualTo(WKWebViewAudioRoutingArbitrationStatusActive, "Mute 3");
}

TEST_F(AudioRoutingArbitration, Navigation)
{
    statusShouldBecomeEqualTo(WKWebViewAudioRoutingArbitrationStatusActive, "Navigation 1");

    [webView synchronouslyLoadHTMLString:@"<html>no contents</html>"];

    statusShouldBecomeEqualTo(WKWebViewAudioRoutingArbitrationStatusNone, "Navigation 2");
}

TEST_F(AudioRoutingArbitration, Deletion)
{
    statusShouldBecomeEqualTo(WKWebViewAudioRoutingArbitrationStatusActive, "Deletion 1");

    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.querySelector('video').parentNode.innerHTML = ''"];

    statusShouldBecomeEqualTo(WKWebViewAudioRoutingArbitrationStatusNone, "Deletion 2", true);
}

TEST_F(AudioRoutingArbitration, Close)
{
    statusShouldBecomeEqualTo(WKWebViewAudioRoutingArbitrationStatusActive, "Close 1");

    [webView _close];

    statusShouldBecomeEqualTo(WKWebViewAudioRoutingArbitrationStatusNone, "Close 2");
}

TEST_F(AudioRoutingArbitration, Updating)
{
    statusShouldBecomeEqualTo(WKWebViewAudioRoutingArbitrationStatusActive, "Updating 1");

    [webView evaluateJavaScript:@"document.querySelector('video').pause()" completionHandler:nil];
    statusShouldBecomeEqualTo(WKWebViewAudioRoutingArbitrationStatusActive, "Updating 2");

    auto start = WallTime::now().secondsSinceEpoch().seconds();
    auto arbitrationUpdateTime = [webView _audioRoutingArbitrationUpdateTime];
    ASSERT_TRUE(arbitrationUpdateTime < start);

    [webView evaluateJavaScript:@"document.querySelector('video').play()" completionHandler:nil];
    EXPECT_EQ(arbitrationUpdateTime, [webView _audioRoutingArbitrationUpdateTime]) << "Arbitration was unexpectedly updated";
    statusShouldBecomeEqualTo(WKWebViewAudioRoutingArbitrationStatusActive, "Updating 3");

    [webView evaluateJavaScript:@"document.querySelector('video').pause()" completionHandler:nil];
    [webView stringByEvaluatingJavaScript:@"window.internals.setIsPlayingToBluetoothOverride(true)"];

    [webView evaluateJavaScript:@"document.querySelector('video').play()" completionHandler:nil];

    int tries = 0;
    do {
        if ([webView _audioRoutingArbitrationUpdateTime] > arbitrationUpdateTime)
            break;

        TestWebKitAPI::Util::sleep(0.1);
    } while (++tries <= 100);

    EXPECT_LT(arbitrationUpdateTime, [webView _audioRoutingArbitrationUpdateTime]) << "Arbitration was not updated";

    [webView stringByEvaluatingJavaScript:@"window.internals.setIsPlayingToBluetoothOverride()"];
}

#endif
