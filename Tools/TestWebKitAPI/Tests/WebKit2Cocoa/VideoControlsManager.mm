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

#include "config.h"

#import "PlatformUtilities.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

#if WK_API_ENABLED && PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200

static bool testedControlsManagerAfterPlaying;
static bool receivedScriptMessage;

@interface DidPlayMessageHandler : NSObject <WKScriptMessageHandler> {
    RetainPtr<WKWebView> _webView;
}

@property (nonatomic) BOOL expectedToHaveControlsManager;

- (instancetype)initWithWKWebView:(WKWebView*)webView;
@end

@implementation DidPlayMessageHandler

- (instancetype)initWithWKWebView:(WKWebView*)webView
{
    if (!(self = [super init]))
        return nil;

    _webView = webView;

    return self;
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;

    NSString *bodyString = (NSString *)[message body];
    if ([bodyString isEqualToString:@"playing"]) {
        BOOL hasControlsManager = [_webView _hasActiveVideoForControlsManager];
        if (self.expectedToHaveControlsManager)
            EXPECT_TRUE(hasControlsManager);
        else
            EXPECT_FALSE(hasControlsManager);
        testedControlsManagerAfterPlaying = true;
    }
}
@end

namespace TestWebKitAPI {

TEST(VideoControlsManager, VideoControlsManagerSingleLargeVideo)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
    RetainPtr<DidPlayMessageHandler> handler = adoptNS([[DidPlayMessageHandler alloc] initWithWKWebView:webView.get()]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"playingHandler"];

    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];

    // A large video with audio should have a controls manager even if it is played via script like this video.
    // So the expectation is YES.
    [handler setExpectedToHaveControlsManager:YES];
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"large-video-with-audio" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&testedControlsManagerAfterPlaying);
    TestWebKitAPI::Util::run(&receivedScriptMessage);
}

TEST(VideoControlsManager, VideoControlsManagerSingleSmallVideo)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
    RetainPtr<DidPlayMessageHandler> handler = adoptNS([[DidPlayMessageHandler alloc] initWithWKWebView:webView.get()]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"playingHandler"];

    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];

    // A small video will not have a controls manager unless it started playing because of a user gesture. Since this
    // video is started with a script, the expectation is NO.
    [handler setExpectedToHaveControlsManager:NO];
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"video-with-audio" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&testedControlsManagerAfterPlaying);
    TestWebKitAPI::Util::run(&receivedScriptMessage);
}

TEST(VideoControlsManager, VideoControlsManagerSingleLargeVideoWithoutAudio)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
    RetainPtr<DidPlayMessageHandler> handler = adoptNS([[DidPlayMessageHandler alloc] initWithWKWebView:webView.get()]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"playingHandler"];

    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];

    // A large video with no audio will not have a controls manager unless it started playing because of a user gesture. Since this
    // video is started with a script, the expectation is NO.
    [handler setExpectedToHaveControlsManager:NO];
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"large-video-without-audio" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&testedControlsManagerAfterPlaying);
    TestWebKitAPI::Util::run(&receivedScriptMessage);
}

TEST(VideoControlsManager, VideoControlsManagerAudioElementStartedWithScript)
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100) configuration:configuration.get()]);
    RetainPtr<DidPlayMessageHandler> handler = adoptNS([[DidPlayMessageHandler alloc] initWithWKWebView:webView.get()]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"playingHandler"];

    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [[window contentView] addSubview:webView.get()];

    // An audio element MUST be started with a user gesture in order to have a controls manager, so the expectation is NO.
    [handler setExpectedToHaveControlsManager:NO];
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"audio-only" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    TestWebKitAPI::Util::run(&testedControlsManagerAfterPlaying);
    TestWebKitAPI::Util::run(&receivedScriptMessage);
}

} // namespace TestWebKitAPI

#endif // WK_API_ENABLED && PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200
