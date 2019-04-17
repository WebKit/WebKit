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

#include "config.h"

#if WK_HAVE_C_SPI

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import "WebCoreTestSupport.h"
#import <JavaScriptCore/JSContext.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/_WKFullscreenDelegate.h>
#import <wtf/RetainPtr.h>

class PreferredAudioBufferSize : public testing::Test {
public:
    void SetUp() override
    {
        configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        WKRetainPtr<WKContextRef> context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));
        configuration.get().processPool = (WKProcessPool *)context.get();
        configuration.get().preferences._lowPowerVideoAudioBufferSizeEnabled = YES;
    }

    void createView()
    {
        webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    }

    double preferredAudioBufferSize() const
    {
        return [webView stringByEvaluatingJavaScript:@"window.internals.preferredAudioBufferSize()"].doubleValue;
    }

    void runPlayingTestWithPageNamed(NSString* name, double expectedAudioBufferSize)
    {
        createView();
        __block bool isPlaying = false;
        [webView performAfterReceivingMessage:@"playing" action:^() { isPlaying = true; }];
        [webView synchronouslyLoadTestPageNamed:name];
        TestWebKitAPI::Util::run(&isPlaying);
        EXPECT_EQ(expectedAudioBufferSize, preferredAudioBufferSize());
    }

    RetainPtr<WKWebViewConfiguration> configuration;
    RetainPtr<TestWKWebView> webView;
    bool isPlaying { false };
};

TEST_F(PreferredAudioBufferSize, Empty)
{
    createView();
    [webView synchronouslyLoadHTMLString:@""];
    EXPECT_EQ(512, preferredAudioBufferSize());
}

TEST_F(PreferredAudioBufferSize, AudioElement)
{
    runPlayingTestWithPageNamed(@"audio-only", 4096);
}

TEST_F(PreferredAudioBufferSize, WebAudio)
{
    runPlayingTestWithPageNamed(@"web-audio-only", 128);
}

TEST_F(PreferredAudioBufferSize, VideoOnly)
{
    runPlayingTestWithPageNamed(@"video-without-audio", 512);
}

TEST_F(PreferredAudioBufferSize, VideoWithAudio)
{
    runPlayingTestWithPageNamed(@"video-with-audio", 4096);
}

TEST_F(PreferredAudioBufferSize, AudioWithWebAudio)
{
    runPlayingTestWithPageNamed(@"audio-with-web-audio", 128);
}

TEST_F(PreferredAudioBufferSize, VideoWithAudioAndWebAudio)
{
    runPlayingTestWithPageNamed(@"video-with-audio-and-web-audio", 128);
}

#endif // WK_HAVE_C_SPI
