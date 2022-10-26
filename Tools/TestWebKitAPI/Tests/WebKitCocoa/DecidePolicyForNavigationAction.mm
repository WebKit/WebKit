/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "DeprecatedGlobalValues.h"
#import "PlatformUtilities.h"
#import "PlatformWebView.h"
#import "TestProtocol.h"
#import <WebKit/WKNavigationActionPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/_WKHitTestResult.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>

static bool shouldCancelNavigation;
static bool shouldDelayDecision;
static bool decidedPolicy;
static bool finishedNavigation;
static RetainPtr<WKNavigationAction> action;
static RetainPtr<WKWebView> newWebView;
static BlockPtr<void(WKNavigationActionPolicy)> delayedDecision;

static NSString *firstURL = @"data:text/html,First";
static NSString *secondURL = @"data:text/html,Second";

@interface DecidePolicyForNavigationActionController : NSObject <WKNavigationDelegate, WKUIDelegate>
@end

@implementation DecidePolicyForNavigationActionController

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    if (shouldCancelNavigation) {
        int64_t deferredWaitTime = 100 * NSEC_PER_MSEC;
        dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, deferredWaitTime);
        dispatch_after(when, dispatch_get_main_queue(), ^{
            decisionHandler(WKNavigationActionPolicyCancel);
            decidedPolicy = true;
        });
        return;
    }

    if (shouldDelayDecision) {
        if (delayedDecision)
            delayedDecision(WKNavigationActionPolicyCancel);
        delayedDecision = makeBlockPtr(decisionHandler);
        decidedPolicy = true;
        return;
    }

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

    didCreateWebView = true;
    return newWebView.get();
}

@end

TEST(WebKit, DecidePolicyForNavigationActionReload)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES]);
    [[window contentView] addSubview:webView.get()];

    auto controller = adoptNS([[DecidePolicyForNavigationActionController alloc] init]);
    [webView setNavigationDelegate:controller.get()];
    [webView setUIDelegate:controller.get()];

    finishedNavigation = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:firstURL]]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    decidedPolicy = false;
    [webView reload];
    TestWebKitAPI::Util::run(&decidedPolicy);

    EXPECT_EQ(WKNavigationTypeReload, [action navigationType]);
    EXPECT_TRUE([action sourceFrame] != [action targetFrame]);
    EXPECT_EQ(nil, [action sourceFrame]);
    EXPECT_WK_STREQ(firstURL, [[[action request] URL] absoluteString]);
    EXPECT_WK_STREQ(firstURL, [[[[action targetFrame] request] URL] absoluteString]);
    EXPECT_EQ(webView.get(), [[action targetFrame] webView]);

    newWebView = nullptr;
    action = nullptr;
}

TEST(WebKit, DecidePolicyForNavigationActionReloadFromOrigin)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES]);
    [[window contentView] addSubview:webView.get()];

    auto controller = adoptNS([[DecidePolicyForNavigationActionController alloc] init]);
    [webView setNavigationDelegate:controller.get()];
    [webView setUIDelegate:controller.get()];

    finishedNavigation = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:firstURL]]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    decidedPolicy = false;
    [webView reloadFromOrigin];
    TestWebKitAPI::Util::run(&decidedPolicy);

    EXPECT_EQ(WKNavigationTypeReload, [action navigationType]);
    EXPECT_TRUE([action sourceFrame] != [action targetFrame]);
    EXPECT_EQ(nil, [action sourceFrame]);
    EXPECT_WK_STREQ(firstURL, [[[action request] URL] absoluteString]);
    EXPECT_WK_STREQ(firstURL, [[[[action targetFrame] request] URL] absoluteString]);
    EXPECT_EQ(webView.get(), [[action targetFrame] webView]);

    newWebView = nullptr;
    action = nullptr;
}

TEST(WebKit, DecidePolicyForNavigationActionGoBack)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES]);
    [[window contentView] addSubview:webView.get()];

    auto controller = adoptNS([[DecidePolicyForNavigationActionController alloc] init]);
    [webView setNavigationDelegate:controller.get()];
    [webView setUIDelegate:controller.get()];

    finishedNavigation = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:firstURL]]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    finishedNavigation = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:secondURL]]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    decidedPolicy = false;
    [webView goBack];
    TestWebKitAPI::Util::run(&decidedPolicy);

    EXPECT_EQ(WKNavigationTypeBackForward, [action navigationType]);
    EXPECT_TRUE([action sourceFrame] != [action targetFrame]);
    EXPECT_EQ(nil, [action sourceFrame]);
    EXPECT_WK_STREQ(firstURL, [[[action request] URL] absoluteString]);
    EXPECT_WK_STREQ(secondURL, [[[[action targetFrame] request] URL] absoluteString]);
    EXPECT_EQ(webView.get(), [[action targetFrame] webView]);

    newWebView = nullptr;
    action = nullptr;
}

TEST(WebKit, DecidePolicyForNavigationActionGoForward)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES]);
    [[window contentView] addSubview:webView.get()];

    auto controller = adoptNS([[DecidePolicyForNavigationActionController alloc] init]);
    [webView setNavigationDelegate:controller.get()];
    [webView setUIDelegate:controller.get()];

    finishedNavigation = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:firstURL]]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    finishedNavigation = false;
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:secondURL]]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    finishedNavigation = false;
    [webView goBack];
    TestWebKitAPI::Util::run(&finishedNavigation);

    decidedPolicy = false;
    [webView goForward];
    TestWebKitAPI::Util::run(&decidedPolicy);

    EXPECT_EQ(WKNavigationTypeBackForward, [action navigationType]);
    EXPECT_TRUE([action sourceFrame] != [action targetFrame]);
    EXPECT_EQ(nil, [action sourceFrame]);
    EXPECT_WK_STREQ(secondURL, [[[action request] URL] absoluteString]);
    EXPECT_WK_STREQ(firstURL, [[[[action targetFrame] request] URL] absoluteString]);
    EXPECT_EQ(webView.get(), [[action targetFrame] webView]);

    newWebView = nullptr;
    action = nullptr;
}

TEST(WebKit, DecidePolicyForNavigationActionOpenNewWindowAndDeallocSourceWebView)
{
    auto controller = adoptNS([[DecidePolicyForNavigationActionController alloc] init]);

    @autoreleasepool {
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

        auto window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES]);
        [[window contentView] addSubview:webView.get()];

        [webView setNavigationDelegate:controller.get()];
        [webView setUIDelegate:controller.get()];

        didCreateWebView = false;
        [webView loadHTMLString:@"<script>window.open('http://webkit.org/destination.html')</script>" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
        TestWebKitAPI::Util::run(&didCreateWebView);
    }

    decidedPolicy = false;
    [newWebView setNavigationDelegate:controller.get()];
    TestWebKitAPI::Util::run(&decidedPolicy);

    EXPECT_EQ(WKNavigationTypeOther, [action navigationType]);
    EXPECT_TRUE([action sourceFrame] != [action targetFrame]);
    EXPECT_EQ(nil, [[action sourceFrame] webView]);
    EXPECT_EQ(newWebView.get(), [[action targetFrame] webView]);

    newWebView = nullptr;
    action = nullptr;
}

#if WK_HAVE_C_SPI
TEST(WebKit, DecidePolicyForNewWindowAction)
{
    auto context = adoptWK(WKContextCreateWithConfiguration(nullptr));

    TestWebKitAPI::PlatformWebView webView(context.get());

    WKPagePolicyClientV1 policyClient;
    memset(&policyClient, 0, sizeof(policyClient));
    policyClient.base.version = 1;
    policyClient.decidePolicyForNewWindowAction = [] (WKPageRef page, WKFrameRef frame, WKFrameNavigationType navigationType, WKEventModifiers modifiers, WKEventMouseButton mouseButton, WKURLRequestRef request, WKStringRef frameName, WKFramePolicyListenerRef listener, WKTypeRef userData, const void* clientInfo) {
        EXPECT_TRUE(WKStringIsEqualToUTF8CString(adoptWK(WKURLCopyString(adoptWK(WKURLRequestCopyURL(request)).get())).get(), "https://webkit.org/"));
        WKFramePolicyListenerIgnore(listener);
        decidedPolicy = true;
    };
    WKPageSetPagePolicyClient(webView.page(), &policyClient.base);

    WKPageLoadHTMLString(webView.page(), adoptWK(WKStringCreateWithUTF8CString("<body onload='anchorTag.click()'><a href='https://webkit.org/' id='anchorTag' target=_blank>link</a></body>")).get(), nullptr);

    TestWebKitAPI::Util::run(&decidedPolicy);
}
#endif // WK_HAVE_C_SPI

TEST(WebKit, DecidePolicyForNavigationActionForTargetedHyperlink)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES]);
    [[window contentView] addSubview:webView.get()];

    auto controller = adoptNS([[DecidePolicyForNavigationActionController alloc] init]);
    [webView setNavigationDelegate:controller.get()];
    [webView setUIDelegate:controller.get()];

    finishedNavigation = false;
    [webView loadHTMLString:@"<a style=\"display: block; height: 100%\" href=\"https://webkit.org/destination2.html\" target=\"B\">" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    didCreateWebView = false;
    [webView evaluateJavaScript:@"window.open(\"https://webkit.org/destination1.html\", \"B\")" completionHandler:nil];
    TestWebKitAPI::Util::run(&didCreateWebView);

    EXPECT_EQ(WKNavigationTypeOther, [action navigationType]);
    EXPECT_TRUE([action sourceFrame] != [action targetFrame]);
    EXPECT_EQ(nil, [action targetFrame]);
    EXPECT_EQ(webView.get(), [[action sourceFrame] webView]);
    EXPECT_WK_STREQ("http", [[[action sourceFrame] securityOrigin] protocol]);
    EXPECT_WK_STREQ("webkit.org", [[[action sourceFrame] securityOrigin] host]);
    EXPECT_NULL([action _hitTestResult]);

    // Wait for newWebView to ask to load its initial document.
    decidedPolicy = false;
    [newWebView setNavigationDelegate:controller.get()];
    TestWebKitAPI::Util::run(&decidedPolicy);

    decidedPolicy = false;
    [newWebView setNavigationDelegate:controller.get()];
    NSPoint clickPoint = NSMakePoint(100, 100);
    [[webView hitTest:clickPoint] mouseDown:[NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:clickPoint modifierFlags:0 timestamp:0 windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1]];
    [[webView hitTest:clickPoint] mouseUp:[NSEvent mouseEventWithType:NSEventTypeLeftMouseUp location:clickPoint modifierFlags:0 timestamp:0 windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1]];
    TestWebKitAPI::Util::run(&decidedPolicy);

    EXPECT_EQ(WKNavigationTypeLinkActivated, [action navigationType]);
    EXPECT_TRUE([action sourceFrame] != [action targetFrame]);
    EXPECT_EQ(webView.get(), [[action sourceFrame] webView]);
    EXPECT_EQ(newWebView.get(), [[action targetFrame] webView]);
    EXPECT_WK_STREQ("http", [[[action sourceFrame] securityOrigin] protocol]);
    EXPECT_WK_STREQ("webkit.org", [[[action sourceFrame] securityOrigin] host]);

    _WKHitTestResult *hitTestResult = [action _hitTestResult];
    EXPECT_NOT_NULL(hitTestResult);

    CGRect elementBoundingBox = hitTestResult.elementBoundingBox;
    EXPECT_FALSE(CGSizeEqualToSize(elementBoundingBox.size, CGSizeZero));

    newWebView = nullptr;
    action = nullptr;
}

TEST(WebKit, DecidePolicyForNavigationActionForLoadHTMLStringAllow)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES]);
    [[window contentView] addSubview:webView.get()];

    auto controller = adoptNS([[DecidePolicyForNavigationActionController alloc] init]);
    [webView setNavigationDelegate:controller.get()];
    [webView setUIDelegate:controller.get()];

    finishedNavigation = false;
    decidedPolicy = false;
    [webView loadHTMLString:@"TEST" baseURL:[NSURL URLWithString:@"about:blank"]];
    TestWebKitAPI::Util::run(&finishedNavigation);
    EXPECT_TRUE(decidedPolicy);
}

TEST(WebKit, DecidePolicyForNavigationActionForLoadHTMLStringDeny)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES]);
    [[window contentView] addSubview:webView.get()];

    auto controller = adoptNS([[DecidePolicyForNavigationActionController alloc] init]);
    [webView setNavigationDelegate:controller.get()];
    [webView setUIDelegate:controller.get()];

    shouldCancelNavigation = true;
    finishedNavigation = false;
    decidedPolicy = false;
    [webView loadHTMLString:@"TEST" baseURL:[NSURL URLWithString:@"about:blank"]];
    TestWebKitAPI::Util::runFor(0.5_s);
    EXPECT_FALSE(finishedNavigation);
    shouldCancelNavigation = false;
}

TEST(WebKit, DecidePolicyForNavigationActionForTargetedWindowOpen)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES]);
    [[window contentView] addSubview:webView.get()];

    auto controller = adoptNS([[DecidePolicyForNavigationActionController alloc] init]);
    [webView setNavigationDelegate:controller.get()];
    [webView setUIDelegate:controller.get()];

    finishedNavigation = false;
    [webView loadHTMLString:@"<a style=\"display: block; height: 100%\" href=\"javascript:window.open('https://webkit.org/destination2.html', 'B')\">" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    didCreateWebView = false;
    [webView evaluateJavaScript:@"window.open(\"https://webkit.org/destination1.html\", \"B\")" completionHandler:nil];
    TestWebKitAPI::Util::run(&didCreateWebView);

    EXPECT_EQ(WKNavigationTypeOther, [action navigationType]);
    EXPECT_TRUE([action sourceFrame] != [action targetFrame]);
    EXPECT_EQ(nil, [action targetFrame]);
    EXPECT_EQ(webView.get(), [[action sourceFrame] webView]);
    EXPECT_WK_STREQ("http", [[[action sourceFrame] securityOrigin] protocol]);
    EXPECT_WK_STREQ("webkit.org", [[[action sourceFrame] securityOrigin] host]);
    EXPECT_NULL([action _hitTestResult]);

    // Wait for newWebView to ask to load its initial document.
    decidedPolicy = false;
    [newWebView setNavigationDelegate:controller.get()];
    TestWebKitAPI::Util::run(&decidedPolicy);

    decidedPolicy = false;
    [newWebView setNavigationDelegate:controller.get()];
    NSPoint clickPoint = NSMakePoint(100, 100);
    [[webView hitTest:clickPoint] mouseDown:[NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:clickPoint modifierFlags:0 timestamp:0 windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1]];
    [[webView hitTest:clickPoint] mouseUp:[NSEvent mouseEventWithType:NSEventTypeLeftMouseUp location:clickPoint modifierFlags:0 timestamp:0 windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1]];
    TestWebKitAPI::Util::run(&decidedPolicy);

    EXPECT_EQ(WKNavigationTypeOther, [action navigationType]);
    EXPECT_TRUE([action sourceFrame] != [action targetFrame]);
    EXPECT_EQ(webView.get(), [[action sourceFrame] webView]);
    EXPECT_EQ(newWebView.get(), [[action targetFrame] webView]);
    EXPECT_WK_STREQ("http", [[[action sourceFrame] securityOrigin] protocol]);
    EXPECT_WK_STREQ("webkit.org", [[[action sourceFrame] securityOrigin] host]);
    EXPECT_NULL([action _hitTestResult]);

    newWebView = nullptr;
    action = nullptr;
}

TEST(WebKit, DecidePolicyForNavigationActionForTargetedFormSubmission)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES]);
    [[window contentView] addSubview:webView.get()];

    auto controller = adoptNS([[DecidePolicyForNavigationActionController alloc] init]);
    [webView setNavigationDelegate:controller.get()];
    [webView setUIDelegate:controller.get()];

    finishedNavigation = false;
    [webView loadHTMLString:@"<form action=\"https://webkit.org/destination1.html\" target=\"B\"><input type=\"submit\" name=\"submit\" value=\"Submit\" style=\"-webkit-appearance: none; height: 100%; width: 100%\"></form>" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    didCreateWebView = false;
    [webView evaluateJavaScript:@"window.open(\"https://webkit.org/destination2.html\", \"B\")" completionHandler:nil];
    TestWebKitAPI::Util::run(&didCreateWebView);

    EXPECT_EQ(WKNavigationTypeOther, [action navigationType]);
    EXPECT_TRUE([action sourceFrame] != [action targetFrame]);
    EXPECT_EQ(nil, [action targetFrame]);
    EXPECT_EQ(webView.get(), [[action sourceFrame] webView]);
    EXPECT_WK_STREQ("http", [[[action sourceFrame] securityOrigin] protocol]);
    EXPECT_WK_STREQ("webkit.org", [[[action sourceFrame] securityOrigin] host]);
    EXPECT_NULL([action _hitTestResult]);

    // Wait for newWebView to ask to load its initial document.
    decidedPolicy = false;
    [newWebView setNavigationDelegate:controller.get()];
    TestWebKitAPI::Util::run(&decidedPolicy);

    decidedPolicy = false;
    NSPoint clickPoint = NSMakePoint(100, 100);
    [[webView hitTest:clickPoint] mouseDown:[NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:clickPoint modifierFlags:0 timestamp:0 windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1]];
    [[webView hitTest:clickPoint] mouseUp:[NSEvent mouseEventWithType:NSEventTypeLeftMouseUp location:clickPoint modifierFlags:0 timestamp:0 windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1]];
    TestWebKitAPI::Util::run(&decidedPolicy);

    EXPECT_EQ(WKNavigationTypeFormSubmitted, [action navigationType]);
    EXPECT_TRUE([action sourceFrame] != [action targetFrame]);
    EXPECT_EQ(webView.get(), [[action sourceFrame] webView]);
    EXPECT_EQ(newWebView.get(), [[action targetFrame] webView]);
    EXPECT_WK_STREQ("http", [[[action sourceFrame] securityOrigin] protocol]);
    EXPECT_WK_STREQ("webkit.org", [[[action sourceFrame] securityOrigin] host]);

    _WKHitTestResult *hitTestResult = [action _hitTestResult];
    EXPECT_NOT_NULL(hitTestResult);

    CGRect elementBoundingBox = hitTestResult.elementBoundingBox;
    EXPECT_FALSE(CGSizeEqualToSize(elementBoundingBox.size, CGSizeZero));

    newWebView = nullptr;
    action = nullptr;
}

enum class ShouldEnableProcessSwap { No, Yes };
static void runDecidePolicyForNavigationActionForHyperlinkThatRedirects(ShouldEnableProcessSwap shouldEnableProcessSwap)
{
    auto processPoolConfiguration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);
    processPoolConfiguration.get().processSwapsOnNavigation = shouldEnableProcessSwap == ShouldEnableProcessSwap::Yes ? YES : NO;
    auto processPool = adoptNS([[WKProcessPool alloc] _initWithConfiguration:processPoolConfiguration.get()]);

    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [webViewConfiguration setProcessPool:processPool.get()];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);

    auto window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES]);
    [[window contentView] addSubview:webView.get()];

    auto controller = adoptNS([[DecidePolicyForNavigationActionController alloc] init]);
    [webView setNavigationDelegate:controller.get()];
    [webView setUIDelegate:controller.get()];

    [TestProtocol registerWithScheme:@"http"];
    finishedNavigation = false;
    [webView loadHTMLString:@"<a style=\"display: block; height: 100%\" href=\"http://redirect/?result\">" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    decidedPolicy = false;
    [newWebView setNavigationDelegate:controller.get()];
    NSPoint clickPoint = NSMakePoint(100, 100);
    [[webView hitTest:clickPoint] mouseDown:[NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:clickPoint modifierFlags:0 timestamp:0 windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1]];
    [[webView hitTest:clickPoint] mouseUp:[NSEvent mouseEventWithType:NSEventTypeLeftMouseUp location:clickPoint modifierFlags:0 timestamp:0 windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:1 pressure:1]];
    TestWebKitAPI::Util::run(&decidedPolicy);

    EXPECT_EQ(WKNavigationTypeLinkActivated, [action navigationType]);
    EXPECT_TRUE([action sourceFrame] == [action targetFrame]);
    EXPECT_WK_STREQ("GET", [[action request] HTTPMethod]);
    EXPECT_WK_STREQ("http://redirect/?result", [[[action request] URL] absoluteString]);
    EXPECT_EQ(webView.get(), [[action sourceFrame] webView]);
    EXPECT_WK_STREQ("http", [[[action sourceFrame] securityOrigin] protocol]);
    EXPECT_WK_STREQ("webkit.org", [[[action sourceFrame] securityOrigin] host]);
    EXPECT_FALSE([action _isRedirect]);

    _WKHitTestResult *hitTestResult = [action _hitTestResult];
    EXPECT_NOT_NULL(hitTestResult);

    CGRect elementBoundingBox = hitTestResult.elementBoundingBox;
    EXPECT_FALSE(CGSizeEqualToSize(elementBoundingBox.size, CGSizeZero));

    // Wait to decide policy for redirect.
    decidedPolicy = false;
    TestWebKitAPI::Util::run(&decidedPolicy);

    EXPECT_EQ(WKNavigationTypeLinkActivated, [action navigationType]);
    EXPECT_TRUE([action sourceFrame] == [action targetFrame]);
    EXPECT_WK_STREQ("GET", [[action request] HTTPMethod]);
    EXPECT_WK_STREQ("http://result/", [[[action request] URL] absoluteString]);
    EXPECT_EQ(webView.get(), [[action sourceFrame] webView]);
    EXPECT_WK_STREQ("http", [[[action sourceFrame] securityOrigin] protocol]);
    EXPECT_WK_STREQ("webkit.org", [[[action sourceFrame] securityOrigin] host]);
    EXPECT_TRUE([action _isRedirect]);

    [TestProtocol unregister];
    newWebView = nullptr;
    action = nullptr;
}

TEST(WebKit, DecidePolicyForNavigationActionForHyperlinkThatRedirectsWithoutPSON)
{
    runDecidePolicyForNavigationActionForHyperlinkThatRedirects(ShouldEnableProcessSwap::No);
}

TEST(WebKit, DecidePolicyForNavigationActionForHyperlinkThatRedirectsWithPSON)
{
    runDecidePolicyForNavigationActionForHyperlinkThatRedirects(ShouldEnableProcessSwap::Yes);
}

TEST(WebKit, DecidePolicyForNavigationActionForPOSTFormSubmissionThatRedirectsToGET)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES]);
    [[window contentView] addSubview:webView.get()];

    auto controller = adoptNS([[DecidePolicyForNavigationActionController alloc] init]);
    [webView setNavigationDelegate:controller.get()];
    [webView setUIDelegate:controller.get()];

    finishedNavigation = false;
    [webView loadHTMLString:@"<form action=\"http://redirect/?result\" method=\"POST\"><input type=\"submit\" name=\"submitButton\" value=\"Submit\"></form>" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    [TestProtocol registerWithScheme:@"http"];
    decidedPolicy = false;
    [webView evaluateJavaScript:@"document.forms[0].submit()" completionHandler:nil];
    TestWebKitAPI::Util::run(&decidedPolicy);

    EXPECT_EQ(WKNavigationTypeFormSubmitted, [action navigationType]);
    EXPECT_TRUE([action sourceFrame] == [action targetFrame]);
    EXPECT_WK_STREQ("POST", [[action request] HTTPMethod]);
    EXPECT_WK_STREQ("http://redirect/?result", [[[action request] URL] absoluteString]);
    EXPECT_EQ(webView.get(), [[action sourceFrame] webView]);
    EXPECT_WK_STREQ("http", [[[action sourceFrame] securityOrigin] protocol]);
    EXPECT_WK_STREQ("webkit.org", [[[action sourceFrame] securityOrigin] host]);
    EXPECT_FALSE([action _isRedirect]);

    // Wait to decide policy for redirect.
    decidedPolicy = false;
    TestWebKitAPI::Util::run(&decidedPolicy);

    EXPECT_EQ(WKNavigationTypeFormSubmitted, [action navigationType]);
    EXPECT_TRUE([action sourceFrame] == [action targetFrame]);
    EXPECT_WK_STREQ("GET", [[action request] HTTPMethod]);
    EXPECT_WK_STREQ("http://result/", [[[action request] URL] absoluteString]);
    EXPECT_EQ(webView.get(), [[action sourceFrame] webView]);
    EXPECT_WK_STREQ("http", [[[action sourceFrame] securityOrigin] protocol]);
    EXPECT_WK_STREQ("webkit.org", [[[action sourceFrame] securityOrigin] host]);
    EXPECT_TRUE([action _isRedirect]);

    [TestProtocol unregister];
    newWebView = nullptr;
    action = nullptr;
}

TEST(WebKit, DecidePolicyForNavigationActionForPOSTFormSubmissionThatRedirectsToPOST)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    auto window = adoptNS([[NSWindow alloc] initWithContentRect:[webView frame] styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES]);
    [[window contentView] addSubview:webView.get()];

    auto controller = adoptNS([[DecidePolicyForNavigationActionController alloc] init]);
    [webView setNavigationDelegate:controller.get()];
    [webView setUIDelegate:controller.get()];

    finishedNavigation = false;
    [webView loadHTMLString:@"<form action=\"http://307-redirect/?result\" method=\"POST\"><input type=\"submit\" name=\"submitButton\" value=\"Submit\"></form>" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    TestWebKitAPI::Util::run(&finishedNavigation);

    [TestProtocol registerWithScheme:@"http"];
    decidedPolicy = false;
    [webView evaluateJavaScript:@"document.forms[0].submit()" completionHandler:nil];
    TestWebKitAPI::Util::run(&decidedPolicy);

    EXPECT_EQ(WKNavigationTypeFormSubmitted, [action navigationType]);
    EXPECT_TRUE([action sourceFrame] == [action targetFrame]);
    EXPECT_WK_STREQ("POST", [[action request] HTTPMethod]);
    EXPECT_WK_STREQ("http://307-redirect/?result", [[[action request] URL] absoluteString]);
    EXPECT_EQ(webView.get(), [[action sourceFrame] webView]);
    EXPECT_WK_STREQ("http", [[[action sourceFrame] securityOrigin] protocol]);
    EXPECT_WK_STREQ("webkit.org", [[[action sourceFrame] securityOrigin] host]);
    EXPECT_FALSE([action _isRedirect]);

    // Wait to decide policy for redirect.
    decidedPolicy = false;
    TestWebKitAPI::Util::run(&decidedPolicy);

    EXPECT_EQ(WKNavigationTypeFormSubmitted, [action navigationType]);
    EXPECT_TRUE([action sourceFrame] == [action targetFrame]);
    EXPECT_WK_STREQ("POST", [[action request] HTTPMethod]);
    EXPECT_WK_STREQ("http://result/", [[[action request] URL] absoluteString]);
    EXPECT_EQ(webView.get(), [[action sourceFrame] webView]);
    EXPECT_WK_STREQ("http", [[[action sourceFrame] securityOrigin] protocol]);
    EXPECT_WK_STREQ("webkit.org", [[[action sourceFrame] securityOrigin] host]);
    EXPECT_TRUE([action _isRedirect]);

    [TestProtocol unregister];
    newWebView = nullptr;
    action = nullptr;
}

TEST(WebKit, DelayDecidePolicyForNavigationAction)
{
    shouldDelayDecision = true;

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    auto controller = adoptNS([[DecidePolicyForNavigationActionController alloc] init]);
    [webView setNavigationDelegate:controller.get()];

    RetainPtr<NSURL> testURL = [[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    TestWebKitAPI::Util::run(&decidedPolicy);
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    TestWebKitAPI::Util::runFor(0.5_s); // Wait until the pending api request gets clear.
    EXPECT_TRUE([[webView URL] isEqual:testURL.get()]);

    shouldDelayDecision = false;
    delayedDecision(WKNavigationActionPolicyCancel);
}

static size_t calls;

@interface DecidePolicyForNavigationActionFragmentDelegate : NSObject <WKNavigationDelegate>
@end

@implementation DecidePolicyForNavigationActionFragmentDelegate

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    decisionHandler(WKNavigationActionPolicyAllow);
    const char* url = navigationAction.request.URL.absoluteString.UTF8String;
    switch (calls++) {
    case 0:
        EXPECT_STREQ(url, "http://webkit.org/");
        return;
    case 1:
        EXPECT_STREQ(url, "http://webkit.org/#fragment");
        done = true;
        return;
    }
    ASSERT_NOT_REACHED();
}

@end

TEST(WebKit, DecidePolicyForNavigationActionFragment)
{
    auto webView = adoptNS([[WKWebView alloc] init]);
    auto delegate = adoptNS([[DecidePolicyForNavigationActionFragmentDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    [webView loadHTMLString:@"<script>window.location.href='#fragment';</script>" baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    TestWebKitAPI::Util::run(&done);
}

#endif
