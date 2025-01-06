/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import <WebKit/WKFoundation.h>
#import <WebKit/WKPreferencesPrivate.h>

@interface WKPreferencesMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation WKPreferencesMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;
    scriptMessages.append(message);
}

@end

static const char* simpleHTML = R"TESTRESOURCE(
<script>
function log(text)
{
    window.webkit.messageHandlers.testHandler.postMessage(text);
}
log("Loaded");
</script>
)TESTRESOURCE";

TEST(WKPreferencesPrivate, DisableRichJavaScriptFeatures)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration.get().preferences _disableRichJavaScriptFeatures];
    RetainPtr handler = adoptNS([[WKPreferencesMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadHTMLString:[NSString stringWithUTF8String:simpleHTML] baseURL:[NSURL URLWithString:@"https://webkit.org"]];
    RetainPtr result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Loaded", result.get());

    [webView evaluateJavaScript:@"canvas = document.createElement('canvas'); log(canvas.getContext('webgl') ? 'WebGL Enabled' : 'WebGL Disabled');" completionHandler:nil];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"WebGL Disabled", result.get());

    [webView evaluateJavaScript:@"log(navigator.gpu ? 'WebGPU Enabled' : 'WebGPU Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"WebGPU Disabled", result.get());

    [webView evaluateJavaScript:@"log(navigator.xr ? 'WebXR Enabled' : 'WebXR Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"WebXR Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.AudioContext ? 'Web Audio Enabled' : 'Web Audio Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Web Audio Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.RTCPeerConnection ? 'Web RTC Enabled' : 'Web RTC Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Web RTC Disabled", result.get());

    [webView evaluateJavaScript:@"log(navigator.getGamepads ? 'Gamepad Enabled' : 'Gamepad Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Gamepad Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.webkitSpeechRecognition ? 'SpeechRecognition Enabled' : 'SpeechRecognition Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"SpeechRecognition Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.SpeechSynthesis ? 'SpeechSynthesis Enabled' : 'SpeechSynthesis Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"SpeechSynthesis Disabled", result.get());

    [webView evaluateJavaScript:@"log(navigator.geolocation ? 'Geolocation Enabled' : 'Geolocation Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Geolocation Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.ApplePaySession ? 'ApplePay Enabled' : 'ApplePay Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"ApplePay Disabled", result.get());

    [webView evaluateJavaScript:@"log(navigator.credentials ? 'Web Authentication Enabled' : 'Web Authentication Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Web Authentication Disabled", result.get());

    [webView evaluateJavaScript:@"log(navigator.setAppBadge ? 'Badging Enabled' : 'Badging Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Badging Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.BarcodeDetector ? 'Shape Detection Enabled' : 'Shape Detection Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Shape Detection Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.screen && screen.orientation ? 'Screen Orientation Enabled' : 'Screen Orientation Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Screen Orientation Disabled", result.get());

    [webView evaluateJavaScript:@"log(navigator.identity && window.DigitalCredential ? 'Digital Credentials Enabled' : 'Digital Credentials Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Digital Credentials Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.Notification ? 'Notifications Enabled' : 'Notifications Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Notifications Disabled", result.get());
}
