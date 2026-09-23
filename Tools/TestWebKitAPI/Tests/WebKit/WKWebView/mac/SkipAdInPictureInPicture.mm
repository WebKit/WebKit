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

#if HAVE(PIP_SKIP_PREROLL)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <pal/spi/mac/PIPSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

SOFT_LINK_PRIVATE_FRAMEWORK(PIP)
SOFT_LINK_CLASS(PIP, PIPPanel)
SOFT_LINK_CLASS(PIP, PIPPrerollAttributes)
SOFT_LINK_CLASS(PIP, PIPViewController)

static bool didEnterPiP = false;

@interface SkipAdInPiPFullscreenDelegate : NSObject <WKUIDelegate>
@end

@implementation SkipAdInPiPFullscreenDelegate

- (void)_webView:(WKWebView *)webView hasVideoInPictureInPictureDidChange:(BOOL)value
{
    if (value)
        didEnterPiP = true;
}

@end

namespace TestWebKitAPI {

static RetainPtr<TestWKWebView> createWebView()
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration preferences]._allowsPictureInPictureMediaPlayback = YES;
    [configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeAudio];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:configuration.get() addToWindow:YES]);
    return webView;
}

TEST(SkipAdInPictureInPicture, VideoHasAd)
{
    RetainPtr webView = createWebView();
    RetainPtr<SkipAdInPiPFullscreenDelegate> handler = adoptNS([[SkipAdInPiPFullscreenDelegate alloc] init]);
    [webView setUIDelegate:handler.get()];

    [webView synchronouslyLoadTestPageNamed:@"SkipAdInPictureInPicture"];

    didEnterPiP = false;
    [webView evaluateJavaScript:@"document.getElementById('enter-pip').click()" completionHandler:nil];
    TestWebKitAPI::Util::waitFor([&] {
        return didEnterPiP;
    });
    EXPECT_TRUE(didEnterPiP);

    __block NSWindow *pipPanel = nil;
    [[NSApplication sharedApplication] enumerateWindowsWithOptions:0 usingBlock:^(NSWindow *window, BOOL *stop) {
        if ([window isKindOfClass:getPIPPanelClassSingleton()]) {
            pipPanel = window;
            *stop = YES;
        }
    }];
    ASSERT_TRUE(pipPanel);
    RetainPtr contentViewController = [pipPanel contentViewController];
    ASSERT_TRUE([contentViewController isKindOfClass:getPIPViewControllerClassSingleton()]);
    RetainPtr pipViewController = (PIPViewController *)contentViewController;
    ASSERT_TRUE(pipViewController);

    RetainPtr pipPlaybackState = [pipViewController playbackState];

    TestWebKitAPI::Util::waitFor([&] {
        return ([pipPlaybackState prerollAttributes] != nil);
    });
    EXPECT_NOT_NULL([pipPlaybackState prerollAttributes]);
}

TEST(SkipAdInPictureInPicture, VideoHasNoAd)
{
    RetainPtr webView = createWebView();
    RetainPtr<SkipAdInPiPFullscreenDelegate> handler = adoptNS([[SkipAdInPiPFullscreenDelegate alloc] init]);
    [webView setUIDelegate:handler.get()];

    [webView synchronouslyLoadTestPageNamed:@"video-with-audio"];

    didEnterPiP = false;
    [webView evaluateJavaScript:@"document.getElementsByTagName('video')[0].webkitSetPresentationMode('picture-in-picture')" completionHandler:nil];
    TestWebKitAPI::Util::waitFor([&] {
        return didEnterPiP;
    });
    EXPECT_TRUE(didEnterPiP);

    __block NSWindow *pipPanel = nil;
    [[NSApplication sharedApplication] enumerateWindowsWithOptions:0 usingBlock:^(NSWindow *window, BOOL *stop) {
        if ([window isKindOfClass:getPIPPanelClassSingleton()]) {
            pipPanel = window;
            *stop = YES;
        }
    }];
    ASSERT_TRUE(pipPanel);
    RetainPtr contentViewController = [pipPanel contentViewController];
    ASSERT_TRUE([contentViewController isKindOfClass:getPIPViewControllerClassSingleton()]);
    RetainPtr pipViewController = (PIPViewController*)contentViewController;
    ASSERT_TRUE(pipViewController);

    RetainPtr pipPlaybackState = [pipViewController playbackState];

    TestWebKitAPI::Util::runFor(1_s);
    EXPECT_NULL([pipPlaybackState prerollAttributes]);
}

}
#endif
