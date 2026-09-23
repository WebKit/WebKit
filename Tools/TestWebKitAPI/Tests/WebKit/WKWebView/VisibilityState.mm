/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfiguration.h>

#if !PLATFORM(IOS_SIMULATOR)
#import "Helpers/DeprecatedGlobalValues.h"
#endif

TEST(VisibilityState, InitialHiddenState)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get() addToWindow:NO]);

    [webView synchronouslyLoadHTMLString:@"foo"];

    __block bool done = false;
    [webView evaluateJavaScript:@"document.visibilityState" completionHandler:^(NSString *visibilityState, NSError *error) {
        EXPECT_WK_STREQ("hidden", visibilityState);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(VisibilityState, InitialVisibleState)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get() addToWindow:YES]);

    [webView synchronouslyLoadHTMLString:@"foo"];

    __block bool done = false;
    [webView evaluateJavaScript:@"document.visibilityState" completionHandler:^(NSString *visibilityState, NSError *error) {
        EXPECT_WK_STREQ("visible", visibilityState);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

#if !PLATFORM(IOS_SIMULATOR)

@interface PiPUIDelegate : NSObject <WKUIDelegate>
@end

@implementation PiPUIDelegate

- (void)_webView:(WKWebView *)webView hasVideoInPictureInPictureDidChange:(BOOL)value
{
    if (value)
        didEnterPiP = true;
    else
        didExitPiP = true;
}

@end

TEST(VisibilityState, PIPKeepsDocumentVisibleQuirk)
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration preferences]._allowsPictureInPictureMediaPlayback = YES;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get() addToWindow:YES]);

    RetainPtr<PiPUIDelegate> handler = adoptNS([[PiPUIDelegate alloc] init]);
    [webView setUIDelegate:handler.get()];

    [webView loadTestPageNamed:@"twitch_quirk"];
    [webView waitForMessage:@"playing"];

    didEnterPiP = false;
    [webView evaluateJavaScript:@"document.querySelector('video').webkitSetPresentationMode('picture-in-picture')" completionHandler:nil];
    ASSERT_TRUE(TestWebKitAPI::Util::runFor(&didEnterPiP, 10_s));

    __block bool done = false;
    [webView evaluateJavaScript:@"document.visibilityState" completionHandler:^(NSString *visibilityState, NSError *error) {
        EXPECT_WK_STREQ("visible", visibilityState);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    // Hide the primary window (leaving the PIP window active)
#if PLATFORM(MAC)
    [webView.get().window setIsVisible:NO];
#else
    webView.get().window.hidden = YES;
#endif
    [webView.get().window resignKeyWindow];

    [webView waitUntilActivityStateUpdateDone];

    done = false;
    [webView evaluateJavaScript:@"document.visibilityState" completionHandler:^(NSString *visibilityState, NSError *error) {
        EXPECT_WK_STREQ("visible", visibilityState);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

#endif // !PLATFORM(IOS_SIMULATOR)
