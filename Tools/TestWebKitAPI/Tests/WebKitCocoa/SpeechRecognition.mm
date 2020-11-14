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
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <wtf/RetainPtr.h>

static bool shouldGrantPermissionRequest = true;
static bool permissionRequested = false;
static bool receivedScriptMessage;
static RetainPtr<WKScriptMessage> lastScriptMessage;

@interface SpeechRecognitionPermissionUIDelegate : NSObject<WKUIDelegatePrivate>
- (void)_webView:(WKWebView *)webView requestSpeechRecognitionPermissionForOrigin:(WKSecurityOrigin *)origin decisionHandler:(void (^)(BOOL))decisionHandler;
@end

@implementation SpeechRecognitionPermissionUIDelegate
- (void)_webView:(WKWebView *)webView requestSpeechRecognitionPermissionForOrigin:(WKSecurityOrigin *)origin decisionHandler:(void (^)(BOOL))decisionHandler
{
    permissionRequested = true;
    decisionHandler(shouldGrantPermissionRequest);
}
@end

@interface SpeechRecognitionMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation SpeechRecognitionMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;
    lastScriptMessage = message;
}
@end

namespace TestWebKitAPI {

TEST(WebKit2, SpeechRecognitionUserPermissionPersistence)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto handler = adoptNS([[SpeechRecognitionMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    auto preferences = [configuration preferences];
    preferences._mockCaptureDevicesEnabled = YES;
    preferences._speechRecognitionEnabled = YES;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    auto delegate = adoptNS([[SpeechRecognitionPermissionUIDelegate alloc] init]);
    [webView setUIDelegate:delegate.get()];

    shouldGrantPermissionRequest = false;
    permissionRequested = false;
    receivedScriptMessage = false;
    [webView loadTestPageNamed:@"speechrecognition-user-permission-persistence"];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"Error: not-allowed - Permission check failed", [lastScriptMessage body]);
    EXPECT_TRUE(permissionRequested);

    // Permission result is remembered.
    permissionRequested = false;
    receivedScriptMessage = false;
    [webView stringByEvaluatingJavaScript:@"start()"];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"Error: not-allowed - Permission check failed", [lastScriptMessage body]);
    EXPECT_FALSE(permissionRequested);

    // Permission result will be cleared after document changes.
    [webView synchronouslyLoadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"about:blank"]]];

    shouldGrantPermissionRequest = true;
    permissionRequested = false;
    receivedScriptMessage = false;
    [webView loadTestPageNamed:@"speechrecognition-user-permission-persistence"];
    TestWebKitAPI::Util::run(&permissionRequested);
    // Should not get error message as permission is granted.
    EXPECT_FALSE(receivedScriptMessage);
}

} // namespace TestWebKitAPI
