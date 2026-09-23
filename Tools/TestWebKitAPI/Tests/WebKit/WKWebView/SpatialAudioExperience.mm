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

#include "config.h"

#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>

#if HAVE(SPATIAL_AUDIO_EXPERIENCE)

namespace TestWebKitAPI {
static BOOL isDone;
}

@interface SpatialAudioExperienceMessageHandler : NSObject <WKScriptMessageHandler>
@property (nonatomic, retain) NSString *expectedBody;
@end

@implementation SpatialAudioExperienceMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    if (![_expectedBody isEqual:message.body])
        return;

    TestWebKitAPI::isDone = true;
}
@end

namespace TestWebKitAPI {

static RetainPtr<WKWebViewConfiguration> autoplayingInternalsConfiguration()
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    configuration.get().mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
    configuration.get().allowsInlineMediaPlayback = YES;
    configuration.get()._inlineMediaPlaybackRequiresPlaysInlineAttribute = NO;
    return configuration;
}

TEST(SpatialAudioExperience, NoWindow)
{
    isDone = false;

    RetainPtr configuration = autoplayingInternalsConfiguration();
    auto messageHandler = adoptNS([[SpatialAudioExperienceMessageHandler alloc] init]);

    [messageHandler setExpectedBody:@"{CAHeadTrackedSpatialAudio: soundStageSize(0), anchoringStrategy: {CAAutomaticAnchoringStrategy}}"];
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"media-player-spatial-experience-change"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get() addToWindow:NO]);

    [webView synchronouslyLoadTestPageNamed:@"spatial-audio-experience-with-video"];

    [webView stringByEvaluatingJavaScript:@"go()"];

    TestWebKitAPI::Util::run(&isDone);
}

TEST(SpatialAudioExperience, WindowWithWindowScene)
{
    // This test must run in TestWebKitAPI.app
    if (![NSBundle.mainBundle.bundleIdentifier isEqualToString:@"org.webkit.TestWebKitAPI"])
        return;

    isDone = false;

    RetainPtr configuration = autoplayingInternalsConfiguration();
    auto messageHandler = adoptNS([[SpatialAudioExperienceMessageHandler alloc] init]);

    [messageHandler setExpectedBody:@"{CAHeadTrackedSpatialAudio: soundStageSize(0), anchoringStrategy: {CAAutomaticAnchoringStrategy}}"];
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"media-player-spatial-experience-change"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);

    [webView synchronouslyLoadTestPageNamed:@"spatial-audio-experience-with-video"];

    [webView stringByEvaluatingJavaScript:@"go()"];

    TestWebKitAPI::Util::run(&isDone);

    [messageHandler setExpectedBody:[NSString stringWithFormat:@"{CAHeadTrackedSpatialAudio: soundStageSize(0), anchoringStrategy: {CASceneAnchoringStrategy: sceneId: %@}}", (NSString *)[webView window].windowScene.session.persistentIdentifier]];
    isDone = false;

    [webView stringByEvaluatingJavaScript:@"document.querySelector('video').style.display = 'none'"];
    TestWebKitAPI::Util::run(&isDone);
}

TEST(SpatialAudioExperience, AudioOnly)
{
    // This test must run in TestWebKitAPI.app
    if (![NSBundle.mainBundle.bundleIdentifier isEqualToString:@"org.webkit.TestWebKitAPI"])
        return;

    isDone = false;

    RetainPtr configuration = autoplayingInternalsConfiguration();
    auto messageHandler = adoptNS([[SpatialAudioExperienceMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"media-player-spatial-experience-change"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);

    [webView synchronouslyLoadTestPageNamed:@"spatial-audio-experience-with-audio"];

    [messageHandler setExpectedBody:[NSString stringWithFormat:@"{CAHeadTrackedSpatialAudio: soundStageSize(0), anchoringStrategy: {CASceneAnchoringStrategy: sceneId: %@}}", (NSString *)[webView window].windowScene.session.persistentIdentifier]];

    [webView stringByEvaluatingJavaScript:@"go()"];

    TestWebKitAPI::Util::run(&isDone);
}

}

#endif
