/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestURLSchemeHandler.h"
#import <WebKit/WKNavigationActionPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/mac/AppKitCompatibilityDeclarations.h>

static bool createdWebView;
static bool decidedPolicy;
static bool finishedNavigation;
static RetainPtr<WKNavigationAction> action;
static RetainPtr<WKWebView> newWebView;

@interface ShouldOpenExternalURLsInNewWindowActionsController : NSObject <WKNavigationDelegate, WKUIDelegate>
@end

@implementation ShouldOpenExternalURLsInNewWindowActionsController

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    decisionHandler(webView == newWebView.get() ? WKNavigationActionPolicyCancel : WKNavigationActionPolicyAllow);

    action = navigationAction;
    decidedPolicy = true;
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    finishedNavigation = true;
}

- (WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures
{
    action = navigationAction;
    newWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);

    createdWebView = true;
    return newWebView.get();
}

@end

TEST(WebKit, ShouldOpenExternalURLsInWindowOpen)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES]);
    [[window contentView] addSubview:webView.get()];

    auto controller = adoptNS([[ShouldOpenExternalURLsInNewWindowActionsController alloc] init]);
    [webView setNavigationDelegate:controller.get()];
    [webView setUIDelegate:controller.get()];

    [webView loadHTMLString:@"<body onclick=\"window.open('https://webkit.org/destination')\">" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    TestWebKitAPI::Util::run(&finishedNavigation);
    finishedNavigation = false;

    NSPoint clickPoint = NSMakePoint(100, 100);

    [[webView hitTest:clickPoint] mouseDown:[NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:clickPoint modifierFlags:0 timestamp:0 windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1]];
    [[webView hitTest:clickPoint] mouseUp:[NSEvent mouseEventWithType:NSEventTypeLeftMouseUp location:clickPoint modifierFlags:0 timestamp:0 windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1]];
    TestWebKitAPI::Util::run(&createdWebView);
    createdWebView = false;

    // User-initiated window.open to the same host should allow external schemes but not App Links.
    ASSERT_TRUE([action _shouldOpenExternalSchemes]);
    ASSERT_FALSE([action _shouldOpenAppLinks]);

    decidedPolicy = false;
    [newWebView setNavigationDelegate:controller.get()];
    TestWebKitAPI::Util::run(&decidedPolicy);
    decidedPolicy = false;

    // User-initiated window.open to the same host should allow external schemes but not App Links.
    ASSERT_TRUE([action _shouldOpenExternalSchemes]);
    ASSERT_FALSE([action _shouldOpenAppLinks]);

    [webView loadHTMLString:@"<body onclick=\"window.open('http://apple.com/destination')\">" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    TestWebKitAPI::Util::run(&finishedNavigation);
    finishedNavigation = false;

    [[webView hitTest:clickPoint] mouseDown:[NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:clickPoint modifierFlags:0 timestamp:0 windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1]];
    [[webView hitTest:clickPoint] mouseUp:[NSEvent mouseEventWithType:NSEventTypeLeftMouseUp location:clickPoint modifierFlags:0 timestamp:0 windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1]];
    TestWebKitAPI::Util::run(&createdWebView);
    createdWebView = false;

    // User-initiated window.open to different host should allow external schemes and App Links.
    ASSERT_TRUE([action _shouldOpenExternalSchemes]);
    ASSERT_TRUE([action _shouldOpenAppLinks]);

    decidedPolicy = false;
    [newWebView setNavigationDelegate:controller.get()];
    TestWebKitAPI::Util::run(&decidedPolicy);
    decidedPolicy = false;

    // User-initiated window.open to different host should allow external schemes and App Links.
    ASSERT_TRUE([action _shouldOpenExternalSchemes]);
    ASSERT_TRUE([action _shouldOpenAppLinks]);

    newWebView = nullptr;
    action = nullptr;
}

TEST(WebKit, ShouldOpenExternalURLsInTargetedLink)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES]);
    [[window contentView] addSubview:webView.get()];

    auto controller = adoptNS([[ShouldOpenExternalURLsInNewWindowActionsController alloc] init]);
    [webView setNavigationDelegate:controller.get()];
    [webView setUIDelegate:controller.get()];

    [webView loadHTMLString:@"<a style=\"display: block; height: 100%\" href=\"https://webkit.org/destination.html\" target=\"_blank\">" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    TestWebKitAPI::Util::run(&finishedNavigation);
    finishedNavigation = false;

    NSPoint clickPoint = NSMakePoint(100, 100);

    decidedPolicy = false;
    [[webView hitTest:clickPoint] mouseDown:[NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:clickPoint modifierFlags:0 timestamp:0 windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1]];
    [[webView hitTest:clickPoint] mouseUp:[NSEvent mouseEventWithType:NSEventTypeLeftMouseUp location:clickPoint modifierFlags:0 timestamp:0 windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1]];
    TestWebKitAPI::Util::run(&decidedPolicy);
    decidedPolicy = false;

    // User-initiated targeted navigation to the same host should allow external schemes but not App Links.
    ASSERT_TRUE([action _shouldOpenExternalSchemes]);
    ASSERT_FALSE([action _shouldOpenAppLinks]);

    TestWebKitAPI::Util::run(&createdWebView);
    createdWebView = false;

    // User-initiated targeted navigation to the same host should allow external schemes but not App Links.
    ASSERT_TRUE([action _shouldOpenExternalSchemes]);
    ASSERT_FALSE([action _shouldOpenAppLinks]);

    decidedPolicy = false;
    [newWebView setNavigationDelegate:controller.get()];
    TestWebKitAPI::Util::run(&decidedPolicy);
    decidedPolicy = false;

    // User-initiated targeted navigation to the same host should allow external schemes but not App Links.
    ASSERT_TRUE([action _shouldOpenExternalSchemes]);
    ASSERT_FALSE([action _shouldOpenAppLinks]);

    [webView loadHTMLString:@"<a style=\"display: block; height: 100%\" href=\"http://apple.com/destination.html\" target=\"_blank\">" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    TestWebKitAPI::Util::run(&finishedNavigation);
    finishedNavigation = false;

    decidedPolicy = false;
    [[webView hitTest:clickPoint] mouseDown:[NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:clickPoint modifierFlags:0 timestamp:0 windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1]];
    [[webView hitTest:clickPoint] mouseUp:[NSEvent mouseEventWithType:NSEventTypeLeftMouseUp location:clickPoint modifierFlags:0 timestamp:0 windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1]];
    TestWebKitAPI::Util::run(&decidedPolicy);
    decidedPolicy = false;

    // User-initiated targeted navigation to different host should allow external schemes and App Links.
    ASSERT_TRUE([action _shouldOpenExternalSchemes]);
    ASSERT_TRUE([action _shouldOpenAppLinks]);

    TestWebKitAPI::Util::run(&createdWebView);
    createdWebView = false;

    // User-initiated targeted navigation to different host should allow external schemes and App Links.
    ASSERT_TRUE([action _shouldOpenExternalSchemes]);
    ASSERT_TRUE([action _shouldOpenAppLinks]);

    decidedPolicy = false;
    [newWebView setNavigationDelegate:controller.get()];
    TestWebKitAPI::Util::run(&decidedPolicy);
    decidedPolicy = false;

    // User-initiated targeted navigation to different host should allow external schemes and App Links.
    ASSERT_TRUE([action _shouldOpenExternalSchemes]);
    ASSERT_TRUE([action _shouldOpenAppLinks]);

    newWebView = nullptr;
    action = nullptr;
}

TEST(WebKit, RestoreShouldOpenExternalURLsPolicyAfterCrash)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    auto window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES]);
    [[window contentView] addSubview:webView.get()];
    auto controller = adoptNS([[ShouldOpenExternalURLsInNewWindowActionsController alloc] init]);
    [webView setNavigationDelegate:controller.get()];
    [webView setUIDelegate:controller.get()];

    finishedNavigation = false;
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"should-open-external-schemes" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&finishedNavigation);
    finishedNavigation = false;

    // Before crash
    decidedPolicy = false;
    [webView evaluateJavaScript:@"navigateToTelURLInNestedZeroTimer()" completionHandler:nil]; // Non-user initiated navigation because it is performed in a nested timer callback.
    TestWebKitAPI::Util::run(&decidedPolicy);
    decidedPolicy = false;

    ASSERT_TRUE([action _shouldOpenExternalSchemes]);
    ASSERT_FALSE([action _shouldOpenAppLinks]);

    // Crash
    [webView _killWebContentProcessAndResetState];
    [webView reload];

    finishedNavigation = false;
    TestWebKitAPI::Util::run(&finishedNavigation);
    finishedNavigation = false;

    // After crash
    decidedPolicy = false;
    [webView evaluateJavaScript:@"navigateToTelURLInNestedZeroTimer()" completionHandler:nil]; // Non-user initiated navigation because it is performed in a nested timer callback.
    TestWebKitAPI::Util::run(&decidedPolicy);
    decidedPolicy = false;

    ASSERT_TRUE([action _shouldOpenExternalSchemes]);
    ASSERT_FALSE([action _shouldOpenAppLinks]);
};



static const char* iframeBytes = R"schemeresource(
<script>
top.location.href = "externalScheme://someotherhost/foo";
</script>
)schemeresource";

static const char* mainFrameBytes = R"schemeresource(
<script>
function clicked() {
    var iframe = document.createElement('iframe');
    iframe.src = "custom://firsthost/iframe.html";
    document.body.appendChild(iframe);
}
</script>

<a style="display: block; height: 100%" onclick="clicked();">Click to start iframe dance</a>
)schemeresource";

TEST(WebKit, IFrameWithSameOriginAsMainFramePropagates)
{
    auto schemeHandler = adoptNS([[TestURLSchemeHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setURLSchemeHandler:schemeHandler.get() forURLScheme:@"custom"];

    [schemeHandler setStartURLSchemeTaskHandler:^(WKWebView *, id<WKURLSchemeTask> task) {
        NSURL *requestURL = [task request].URL;
        
        NSString *responseText = nil;
        if ([[task request].URL.absoluteString containsString:@"iframe.html"])
            responseText = [NSString stringWithUTF8String:iframeBytes];
        else if ([[task request].URL.absoluteString containsString:@"mainframe.html"])
            responseText = [NSString stringWithUTF8String:mainFrameBytes];

        auto response = adoptNS([[NSURLResponse alloc] initWithURL:requestURL MIMEType:@"text/html" expectedContentLength:[responseText length] textEncodingName:nil]);
        [task didReceiveResponse:response.get()];
        [task didReceiveData:[responseText dataUsingEncoding:NSUTF8StringEncoding]];
        [task didFinish];
    }];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"custom://firsthost/mainframe.html"]]];
    [navigationDelegate waitForDidFinishNavigation];
    
    // Install the decidePolicyListener
    static bool openAppLinks;
    static bool externalSchemes;
    static bool finished = false;
    navigationDelegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *action, void (^decisionHandler)(WKNavigationActionPolicy)) {
        if (!action.targetFrame.mainFrame) {
            decisionHandler(WKNavigationActionPolicyAllow);
            return;
        }

        openAppLinks = [action _shouldOpenAppLinks];
        externalSchemes = [action _shouldOpenExternalSchemes];
        
        decisionHandler(WKNavigationActionPolicyCancel);
        finished = true;
    };
    
    // Click the link
    NSPoint clickPoint = NSMakePoint(100, 100);
    [[webView hitTest:clickPoint] mouseDown:[NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:clickPoint modifierFlags:0 timestamp:0 windowNumber:[webView.get().window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1]];
    [[webView hitTest:clickPoint] mouseUp:[NSEvent mouseEventWithType:NSEventTypeLeftMouseUp location:clickPoint modifierFlags:0 timestamp:0 windowNumber:[webView.get().window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1]];

    TestWebKitAPI::Util::run(&finished);
    
    ASSERT_TRUE(openAppLinks);
    ASSERT_TRUE(externalSchemes);
};

#endif
