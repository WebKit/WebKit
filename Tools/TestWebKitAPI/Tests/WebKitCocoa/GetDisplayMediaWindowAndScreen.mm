/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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

#if HAVE(SCREEN_CAPTURE_KIT)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import "UserMediaCaptureUIDelegate.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>

@interface WindowAndScreenCaptureTestView : TestWKWebView
- (BOOL)haveStream:(BOOL)expected;
@end

@implementation WindowAndScreenCaptureTestView
- (BOOL)haveStream:(BOOL)expected
{
    int retryCount = 1000;
    while (retryCount--) {
        auto result = [self stringByEvaluatingJavaScript:@"haveStream()"];
        if (result.boolValue == expected)
            return YES;

        TestWebKitAPI::Util::spinRunLoop(10);
    }

    return NO;
}
@end

namespace TestWebKitAPI {

TEST(WebKit2, GetDisplayMediaWindowAndScreenPrompt)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto context = adoptWK(TestWebKitAPI::Util::createContextForInjectedBundleTest("InternalsInjectedBundleTest"));

    configuration.get().processPool = (WKProcessPool *)context.get();
    configuration.get()._mediaCaptureEnabled = YES;

    auto preferences = [configuration preferences];
    preferences._mediaCaptureRequiresSecureConnection = NO;
    preferences._mockCaptureDevicesEnabled = YES;
    preferences._useScreenCaptureKit = YES;

    auto delegate = adoptNS([[UserMediaCaptureUIDelegate alloc] init]);
    auto webView = adoptNS([[WindowAndScreenCaptureTestView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    [webView setUIDelegate:delegate.get()];

    [webView synchronouslyLoadTestPageNamed:@"getDisplayMedia"];

    // Check "Don’t Allow"
    [delegate setGetDisplayMediaDecision:WKDisplayCapturePermissionDecisionDeny];
    [webView stringByEvaluatingJavaScript:@"promptForCapture({ video : true })"];
    [delegate waitUntilPrompted];
    EXPECT_TRUE([webView haveStream:NO]);

    // Check "Allow Screen"
    [webView stringByEvaluatingJavaScript:@"stop()"];
    [delegate resetWasPrompted];
    [webView _setIndexOfGetDisplayMediaDeviceSelectedForTesting:@0];
    [delegate setGetDisplayMediaDecision:WKDisplayCapturePermissionDecisionScreenPrompt];
    [webView stringByEvaluatingJavaScript:@"promptForCapture({ video : true })"];
    [delegate waitUntilPrompted];
    EXPECT_TRUE([webView haveStream:YES]);
    auto label = [webView stringByEvaluatingJavaScript:@"stream.getVideoTracks()[0].label"];
    EXPECT_WK_STREQ(label, @"Mock screen device 1");

    // Check "Allow Window"
    [webView stringByEvaluatingJavaScript:@"stop()"];
    [delegate resetWasPrompted];
    [webView _setIndexOfGetDisplayMediaDeviceSelectedForTesting:@0];
    [delegate setGetDisplayMediaDecision:WKDisplayCapturePermissionDecisionWindowPrompt];
    [webView stringByEvaluatingJavaScript:@"promptForCapture({ video : true })"];
    [delegate waitUntilPrompted];
    EXPECT_TRUE([webView haveStream:YES]);
    label = [webView stringByEvaluatingJavaScript:@"stream.getVideoTracks()[0].label"];
    EXPECT_WK_STREQ(label, @"Mock window device 1");

    [webView stringByEvaluatingJavaScript:@"stop()"];
    [webView _setIndexOfGetDisplayMediaDeviceSelectedForTesting:nil];
}

} // namespace TestWebKitAPI

#endif // HAVE(SCREEN_CAPTURE_KIT)
