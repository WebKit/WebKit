/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"

#if PLATFORM(IOS)

#import "PlatformUtilities.h"
#import <AVFoundation/AVAudioSession.h>
#import <UIKit/UIKit.h>
#import <WebCore/DeprecatedGlobalSettings.h>
#import <WebKit/WebKitLegacy.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK(AVFoundation)
SOFT_LINK_CLASS(AVFoundation, AVAudioSession)
SOFT_LINK_CONSTANT(AVFoundation, AVAudioSessionCategoryAmbient, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVAudioSessionCategoryPlayback, NSString *)

static bool didBeginPlaying = false;

@interface AudioSessionCategoryUIWebViewDelegate : NSObject <UIWebViewDelegate>
@end

@implementation AudioSessionCategoryUIWebViewDelegate
IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (BOOL)webView:(UIWebView *)webView shouldStartLoadWithRequest:(NSURLRequest *)request navigationType:(UIWebViewNavigationType)navigationType
IGNORE_WARNINGS_END
{
    if ([request.URL.scheme isEqualToString:@"callback"] && [request.URL.resourceSpecifier isEqualToString:@"playing"]) {
        didBeginPlaying = true;
        return NO;
    }

    return YES;
}
@end

namespace TestWebKitAPI {

static void waitUntilAudioSessionCategoryIsEqualTo(NSString *expectedValue)
{
    int tries = 0;
    do {
        if ([[[getAVAudioSessionClass() sharedInstance] category] isEqualToString:expectedValue])
            return;
        Util::sleep(0.1);
    } while (++tries <= 100);
}

TEST(WebKitLegacy, AudioSessionCategoryIOS)
{
    WebCore::DeprecatedGlobalSettings::setShouldManageAudioSessionCategory(true);
    RetainPtr<UIWindow> uiWindow = adoptNS([[UIWindow alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    RetainPtr<UIWebView> uiWebView = adoptNS([[UIWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [uiWindow addSubview:uiWebView.get()];

    uiWebView.get().mediaPlaybackRequiresUserAction = NO;
    uiWebView.get().allowsInlineMediaPlayback = YES;

    RetainPtr<AudioSessionCategoryUIWebViewDelegate> uiDelegate = adoptNS([[AudioSessionCategoryUIWebViewDelegate alloc] init]);
    uiWebView.get().delegate = uiDelegate.get();

    [uiWebView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"video-with-audio" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];

    Util::run(&didBeginPlaying);

    waitUntilAudioSessionCategoryIsEqualTo(getAVAudioSessionCategoryPlayback());
    EXPECT_WK_STREQ(getAVAudioSessionCategoryPlayback(), [[getAVAudioSessionClass() sharedInstance] category]);

    didBeginPlaying = false;

    [uiWebView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"video-without-audio" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];

    Util::run(&didBeginPlaying);

    waitUntilAudioSessionCategoryIsEqualTo(getAVAudioSessionCategoryAmbient());
    EXPECT_WK_STREQ(getAVAudioSessionCategoryAmbient(), [[getAVAudioSessionClass() sharedInstance] category]);

    didBeginPlaying = false;

    [uiWebView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"video-with-muted-audio" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];

    Util::run(&didBeginPlaying);

    waitUntilAudioSessionCategoryIsEqualTo(getAVAudioSessionCategoryAmbient());
    EXPECT_WK_STREQ(getAVAudioSessionCategoryAmbient(), [[getAVAudioSessionClass() sharedInstance] category]);

    didBeginPlaying = false;

    [uiWebView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"video-with-muted-audio-and-webaudio" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];

    Util::run(&didBeginPlaying);

    waitUntilAudioSessionCategoryIsEqualTo(getAVAudioSessionCategoryAmbient());
    EXPECT_WK_STREQ(getAVAudioSessionCategoryAmbient(), [[getAVAudioSessionClass() sharedInstance] category]);

    didBeginPlaying = false;

    [uiWebView loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"video-with-paused-audio-and-playing-muted" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];

    Util::run(&didBeginPlaying);

    waitUntilAudioSessionCategoryIsEqualTo(getAVAudioSessionCategoryPlayback());
    EXPECT_WK_STREQ(getAVAudioSessionCategoryPlayback(), [[getAVAudioSessionClass() sharedInstance] category]);
}

}

#endif
