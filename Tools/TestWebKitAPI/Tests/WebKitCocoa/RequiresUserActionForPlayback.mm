/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#import "DeprecatedGlobalValues.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>

#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#endif

@interface RequiresUserActionForPlaybackMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation RequiresUserActionForPlaybackMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;
    lastScriptMessage = message;
}
@end


class RequiresUserActionForPlaybackTest : public testing::Test {
public:
    virtual void SetUp()
    {
        handler = adoptNS([[RequiresUserActionForPlaybackMessageHandler alloc] init]);
        configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
        configuration.get()._mediaDataLoadsAutomatically = YES;
#if TARGET_OS_IPHONE
        configuration.get().allowsInlineMediaPlayback = YES;
        configuration.get()._inlineMediaPlaybackRequiresPlaysInlineAttribute = NO;
#endif
    }

    void createWebView()
    {
        webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
        [webView setVisibility:YES];
    }

    void testVideoWithAudio()
    {
        [webView synchronouslyLoadTestPageNamed:@"video-with-audio"];

        TestWebKitAPI::Util::run(&receivedScriptMessage);
        receivedScriptMessage = false;
    }

    void testVideoWithoutAudio()
    {
        [webView synchronouslyLoadTestPageNamed:@"video-without-audio"];

        TestWebKitAPI::Util::run(&receivedScriptMessage);
        receivedScriptMessage = false;
    }

    void testAudioOnly()
    {
        [webView synchronouslyLoadTestPageNamed:@"audio-only"];

        TestWebKitAPI::Util::run(&receivedScriptMessage);
        receivedScriptMessage = false;
    }

    RetainPtr<RequiresUserActionForPlaybackMessageHandler> handler;
    RetainPtr<WKWebViewConfiguration> configuration;
    RetainPtr<TestWKWebView> webView;
};

#if TARGET_OS_IPHONE
TEST_F(RequiresUserActionForPlaybackTest, RequiresUserActionForMediaPlayback)
{
    configuration.get().requiresUserActionForMediaPlayback = YES;
    createWebView();

    testVideoWithAudio();
    EXPECT_WK_STREQ(@"not playing", (NSString *)[lastScriptMessage body]);
    testVideoWithoutAudio();
    EXPECT_WK_STREQ(@"not playing", (NSString *)[lastScriptMessage body]);
    testAudioOnly();
    EXPECT_WK_STREQ(@"not playing", (NSString *)[lastScriptMessage body]);
}

TEST_F(RequiresUserActionForPlaybackTest, DoesNotRequireUserActionForMediaPlayback)
{
    configuration.get().requiresUserActionForMediaPlayback = NO;
    createWebView();

    testVideoWithAudio();
    EXPECT_WK_STREQ(@"playing", (NSString *)[lastScriptMessage body]);
    testVideoWithoutAudio();
    EXPECT_WK_STREQ(@"playing", (NSString *)[lastScriptMessage body]);
    testAudioOnly();
    EXPECT_WK_STREQ(@"playing", (NSString *)[lastScriptMessage body]);
}
#endif

TEST_F(RequiresUserActionForPlaybackTest, RequiresUserActionForAudioAndVideoPlayback)
{
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeAudio | WKAudiovisualMediaTypeVideo;
    createWebView();

    testVideoWithAudio();
    EXPECT_WK_STREQ(@"not playing", (NSString *)[lastScriptMessage body]);
    testVideoWithoutAudio();
    EXPECT_WK_STREQ(@"not playing", (NSString *)[lastScriptMessage body]);
    testAudioOnly();
    EXPECT_WK_STREQ(@"not playing", (NSString *)[lastScriptMessage body]);
}

TEST_F(RequiresUserActionForPlaybackTest, RequiresUserActionForAudioButNotVideoPlayback)
{
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeAudio;
    createWebView();

    testVideoWithAudio();
    EXPECT_WK_STREQ(@"not playing", (NSString *)[lastScriptMessage body]);
    testVideoWithoutAudio();
    EXPECT_WK_STREQ(@"playing", (NSString *)[lastScriptMessage body]);
    testAudioOnly();
    EXPECT_WK_STREQ(@"not playing", (NSString *)[lastScriptMessage body]);
}

TEST_F(RequiresUserActionForPlaybackTest, RequiresUserActionForVideoButNotAudioPlayback)
{
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeVideo;
    createWebView();

    testVideoWithAudio();
    EXPECT_WK_STREQ(@"not playing", (NSString *)[lastScriptMessage body]);
    testVideoWithoutAudio();
    EXPECT_WK_STREQ(@"not playing", (NSString *)[lastScriptMessage body]);
    testAudioOnly();
    EXPECT_WK_STREQ(@"playing", (NSString *)[lastScriptMessage body]);
}

TEST_F(RequiresUserActionForPlaybackTest, DeprecatedRequiresUserActionForAudioAndVideoPlayback)
{
    configuration.get()._requiresUserActionForAudioPlayback = YES;
    configuration.get()._requiresUserActionForVideoPlayback = YES;
    createWebView();

    testVideoWithAudio();
    EXPECT_WK_STREQ(@"not playing", (NSString *)[lastScriptMessage body]);
    testVideoWithoutAudio();
    EXPECT_WK_STREQ(@"not playing", (NSString *)[lastScriptMessage body]);
    testAudioOnly();
    EXPECT_WK_STREQ(@"not playing", (NSString *)[lastScriptMessage body]);
}

TEST_F(RequiresUserActionForPlaybackTest, DeprecatedRequiresUserActionForAudioButNotVideoPlayback)
{
    configuration.get()._requiresUserActionForAudioPlayback = YES;
    configuration.get()._requiresUserActionForVideoPlayback = NO;
    createWebView();

    testVideoWithAudio();
    EXPECT_WK_STREQ(@"not playing", (NSString *)[lastScriptMessage body]);
    testVideoWithoutAudio();
    EXPECT_WK_STREQ(@"playing", (NSString *)[lastScriptMessage body]);
    testAudioOnly();
    EXPECT_WK_STREQ(@"not playing", (NSString *)[lastScriptMessage body]);
}

TEST_F(RequiresUserActionForPlaybackTest, DeprecatedRequiresUserActionForVideoButNotAudioPlayback)
{
    configuration.get()._requiresUserActionForAudioPlayback = NO;
    configuration.get()._requiresUserActionForVideoPlayback = YES;
    createWebView();

    testVideoWithAudio();
    EXPECT_WK_STREQ(@"not playing", (NSString *)[lastScriptMessage body]);
    testVideoWithoutAudio();
    EXPECT_WK_STREQ(@"not playing", (NSString *)[lastScriptMessage body]);
    testAudioOnly();
    EXPECT_WK_STREQ(@"playing", (NSString *)[lastScriptMessage body]);
}
