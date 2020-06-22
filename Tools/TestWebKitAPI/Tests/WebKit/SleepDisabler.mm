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
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/Threading.h>

#if WK_HAVE_C_SPI

TEST(WebKit, SleepDisabler)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
    configuration.get().processPool = (WKProcessPool *)context.get();
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);

    ASSERT_FALSE([webView _hasSleepDisabler]);

    auto createSleepDisabler = [&] {
        return [webView stringByEvaluatingJavaScript:@"window.internals.createSleepDisabler(\"TestDisabler\", true)"].longLongValue;
    };

    auto identifier = createSleepDisabler();

    ASSERT_NE(identifier, 0);

    ASSERT_TRUE([webView _hasSleepDisabler]);

    auto destroySleepDisabler = [&] {
        NSString *js = [NSString stringWithFormat:@"window.internals.destroySleepDisabler(%lld)", identifier];
        return [webView stringByEvaluatingJavaScript:js];
    };

    ASSERT_TRUE(destroySleepDisabler());

    ASSERT_FALSE([webView _hasSleepDisabler]);
}

#endif // WK_HAVE_C_SPI

class SleepDisabler : public testing::Test {
public:
    RetainPtr<TestWKWebView> webView;

    void SetUp() final
    {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        configuration.get()._mediaDataLoadsAutomatically = YES;
        configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
        webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 240) configuration:configuration.get() addToWindow:YES]);
    }

    void TearDown() final
    {
        [webView _close];
    }

    void runAndWaitUntilPlaying(Function<void()>&& function)
    {
        bool isPlaying = false;
        [webView performAfterReceivingMessage:@"playing" action:[&] { isPlaying = true; }];

        function();

        TestWebKitAPI::Util::run(&isPlaying);
        [webView clearMessageHandlers:@[@"playing"]];
    }

    void loadPlayingPage(NSString* pageName)
    {
        runAndWaitUntilPlaying([&] {
            [webView synchronouslyLoadTestPageNamed:pageName];
        });
    }

    void hasSleepDisablerShouldBecomeEqualTo(bool shouldHaveSleepDisabler)
    {
        int tries = 0;
        do {
            if ([webView _hasSleepDisabler] == shouldHaveSleepDisabler)
                break;

            TestWebKitAPI::Util::sleep(0.1);
        } while (++tries <= 100);

        EXPECT_EQ(shouldHaveSleepDisabler, [webView _hasSleepDisabler]);
    }
};

TEST_F(SleepDisabler, Basic)
{
    loadPlayingPage(@"video-with-audio");
    hasSleepDisablerShouldBecomeEqualTo(true);
}

TEST_F(SleepDisabler, Pause)
{
    loadPlayingPage(@"video-with-audio");
    hasSleepDisablerShouldBecomeEqualTo(true);

    [webView evaluateJavaScript:@"document.querySelector('video').pause()" completionHandler:nil];
    hasSleepDisablerShouldBecomeEqualTo(false);
}

TEST_F(SleepDisabler, Mute)
{
    loadPlayingPage(@"video-with-audio");
    hasSleepDisablerShouldBecomeEqualTo(true);

    [webView evaluateJavaScript:@"document.querySelector('video').muted = true" completionHandler:nil];
    hasSleepDisablerShouldBecomeEqualTo(false);
}

TEST_F(SleepDisabler, Unmute)
{
    loadPlayingPage(@"video-with-audio");
    hasSleepDisablerShouldBecomeEqualTo(true);

    loadPlayingPage(@"video-with-muted-audio");
    hasSleepDisablerShouldBecomeEqualTo(false);

    [webView evaluateJavaScript:@"document.querySelector('video').muted = false" completionHandler:nil];
    hasSleepDisablerShouldBecomeEqualTo(true);
}

TEST_F(SleepDisabler, DisableAudioTrack)
{
    loadPlayingPage(@"video-with-audio");
    hasSleepDisablerShouldBecomeEqualTo(true);

    [webView evaluateJavaScript:@"document.querySelector('video').audioTracks[0].enabled = false" completionHandler:nil];
    hasSleepDisablerShouldBecomeEqualTo(false);
}

TEST_F(SleepDisabler, Loop)
{
    loadPlayingPage(@"video-with-audio");
    hasSleepDisablerShouldBecomeEqualTo(true);

    [webView evaluateJavaScript:@"document.querySelector('video').loop = true" completionHandler:nil];
    hasSleepDisablerShouldBecomeEqualTo(false);
}

TEST_F(SleepDisabler, ChangeSrc)
{
    loadPlayingPage(@"video-with-audio");
    hasSleepDisablerShouldBecomeEqualTo(true);

    [webView evaluateJavaScript:@"video = document.querySelector('video').src = 'video-without-audio.mp4'" completionHandler:nil];
    hasSleepDisablerShouldBecomeEqualTo(false);
}

TEST_F(SleepDisabler, Load)
{
    loadPlayingPage(@"video-with-audio");
    hasSleepDisablerShouldBecomeEqualTo(true);

    runAndWaitUntilPlaying([&] {
        [webView evaluateJavaScript:@"video = document.querySelector('video'); video.load(); go()" completionHandler:nil];
    });
    hasSleepDisablerShouldBecomeEqualTo(true);
}

TEST_F(SleepDisabler, Unload)
{
    loadPlayingPage(@"video-with-audio");
    hasSleepDisablerShouldBecomeEqualTo(true);

    [webView evaluateJavaScript:@"video = document.querySelector('video'); video.src = ''; video.load()" completionHandler:nil];
    hasSleepDisablerShouldBecomeEqualTo(false);
}

TEST_F(SleepDisabler, Navigate)
{
    loadPlayingPage(@"video-with-audio");
    hasSleepDisablerShouldBecomeEqualTo(true);

    [webView synchronouslyLoadHTMLString:@"<html>no contents</html>"];
    hasSleepDisablerShouldBecomeEqualTo(false);
}

TEST_F(SleepDisabler, NavigateBack)
{
    loadPlayingPage(@"video-with-audio");
    hasSleepDisablerShouldBecomeEqualTo(true);

    loadPlayingPage(@"video-without-audio");
    hasSleepDisablerShouldBecomeEqualTo(false);

    [webView goBack];
    [webView evaluateJavaScript:@"document.querySelector('video').play()" completionHandler:nil];
    hasSleepDisablerShouldBecomeEqualTo(true);
}

TEST_F(SleepDisabler, Reload)
{
    loadPlayingPage(@"video-with-audio");
    hasSleepDisablerShouldBecomeEqualTo(true);

    runAndWaitUntilPlaying([&] {
        [webView reload];
    });
    hasSleepDisablerShouldBecomeEqualTo(true);
}

TEST_F(SleepDisabler, Close)
{
    loadPlayingPage(@"video-with-audio");
    hasSleepDisablerShouldBecomeEqualTo(true);

    [webView _close];
    hasSleepDisablerShouldBecomeEqualTo(false);
}

TEST_F(SleepDisabler, Crash)
{
    loadPlayingPage(@"video-with-audio");
    hasSleepDisablerShouldBecomeEqualTo(true);

    [webView _killWebContentProcess];
    hasSleepDisablerShouldBecomeEqualTo(false);
}
