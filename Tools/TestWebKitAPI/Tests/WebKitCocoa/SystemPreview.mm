/*
 * Copyright (C) 2019-2022 Apple Inc. All rights reserved.
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

#if (PLATFORM(IOS) || PLATFORM(VISION)) && USE(SYSTEM_PREVIEW)

#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WebKit.h>
#import <wtf/text/WTFString.h>

static bool hasTriggerInfo;
static bool wasTriggered;
static bool wasTriggeredByDetachedElement;
static uint64_t elementID;
static uint64_t pageID;
static String documentID;

@interface TestSystemPreviewTriggeredHandler : NSObject <WKScriptMessageHandler>
@end

@implementation TestSystemPreviewTriggeredHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    if ([message.body[@"message"] isEqualToString:@"loaded_triggered"]) {
        elementID = [message.body[@"elementID"] unsignedIntValue];
        documentID = message.body[@"documentID"];
        pageID = [message.body[@"pageID"] unsignedIntValue];
        hasTriggerInfo = true;
    } else if ([message.body[@"message"] isEqualToString:@"triggered"]) {
        EXPECT_TRUE([message.body[@"action"] isEqualToString:@"_apple_ar_quicklook_button_tapped"]);
        wasTriggered = true;
    }
}

@end

@interface TestSystemPreviewTriggeredOnDetachedElementHandler : NSObject <WKScriptMessageHandler>
@end

@implementation TestSystemPreviewTriggeredOnDetachedElementHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    if ([message.body[@"message"] isEqualToString:@"loaded_triggered_detached"]) {
        elementID = [message.body[@"elementID"] unsignedIntValue];
        documentID = message.body[@"documentID"];
        pageID = [message.body[@"pageID"] unsignedIntValue];
        hasTriggerInfo = true;
    } else if ([message.body[@"message"] isEqualToString:@"triggered_detached"]) {
        EXPECT_TRUE([message.body[@"action"] isEqualToString:@"_apple_ar_quicklook_button_tapped"]);
        wasTriggeredByDetachedElement = true;
    }
}

@end

@interface TestSystemPreviewUIDelegate : NSObject <WKUIDelegate>
@property (nonatomic, weak) UIViewController *viewController;
@end

@implementation TestSystemPreviewUIDelegate
- (UIViewController *)_presentingViewControllerForWebView:(WKWebView *)webView
{
    return _viewController;
}
@end

namespace TestWebKitAPI {

TEST(WebKit, SystemPreviewLoad)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration _setSystemPreviewEnabled:YES];

    auto viewController = adoptNS([[UIViewController alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    auto uiDelegate = adoptNS([[TestSystemPreviewUIDelegate alloc] init]);
    uiDelegate.get().viewController = viewController.get();
    [webView setUIDelegate:uiDelegate.get()];
    [viewController setView:webView.get()];

    [webView synchronouslyLoadTestPageNamed:@"system-preview"];

    [webView _setSystemPreviewCompletionHandlerForLoadTesting:^(bool success) {
        EXPECT_TRUE(success);
        wasTriggered = true;
    }];

    [webView evaluateJavaScript:@"arlink.click()" completionHandler:nil];

    Util::run(&wasTriggered);
}

TEST(WebKit, SystemPreviewFail)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration _setSystemPreviewEnabled:YES];

    auto viewController = adoptNS([[UIViewController alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    auto uiDelegate = adoptNS([[TestSystemPreviewUIDelegate alloc] init]);
    uiDelegate.get().viewController = viewController.get();
    [webView setUIDelegate:uiDelegate.get()];
    [viewController setView:webView.get()];

    [webView synchronouslyLoadTestPageNamed:@"system-preview"];

    [webView _setSystemPreviewCompletionHandlerForLoadTesting:^(bool success) {
        EXPECT_FALSE(success);
        wasTriggered = true;
    }];

    [webView evaluateJavaScript:@"badlink.click()" completionHandler:nil];

    Util::run(&wasTriggered);
}

TEST(WebKit, SystemPreviewBlobRevokedImmediately)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration _setSystemPreviewEnabled:YES];

    auto viewController = adoptNS([[UIViewController alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    auto uiDelegate = adoptNS([[TestSystemPreviewUIDelegate alloc] init]);
    uiDelegate.get().viewController = viewController.get();
    [webView setUIDelegate:uiDelegate.get()];
    [viewController setView:webView.get()];

    NSURL *modelURL = [[NSBundle mainBundle] URLForResource:@"UnitBox" withExtension:@"usdz" subdirectory:@"TestWebKitAPI.resources"];
    NSData *modelData = [NSData dataWithContentsOfURL:modelURL];
    NSString *modelBase64 = [modelData base64EncodedStringWithOptions:0];
    NSString *html = [NSString stringWithFormat:@"<script>let base64URL = 'data:model/vnd.usdz+zip;base64,%@';"
        "fetch(base64URL)"
        "    .then((response) => response.blob())"
        "    .then((blob) => {"
        "        const blobURL = URL.createObjectURL(blob);"
        "        var a = document.createElement('a');"
        "        a.href = blobURL;"
        "        a.rel = 'ar';"
        "        document.body.appendChild(a);"
        "        var i = document.createElement('img');"
        "        a.appendChild(i);"
        "        a.click();"
        "        URL.revokeObjectURL(blobURL);"
        "    });</script>", modelBase64];

    [webView loadHTMLString:html baseURL:nil];

    [webView _setSystemPreviewCompletionHandlerForLoadTesting:^(bool success) {
        EXPECT_TRUE(success);
        wasTriggered = true;
    }];

    Util::run(&wasTriggered);
}

TEST(WebKit, SystemPreviewBlob)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration _setSystemPreviewEnabled:YES];

    auto viewController = adoptNS([[UIViewController alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    auto uiDelegate = adoptNS([[TestSystemPreviewUIDelegate alloc] init]);
    uiDelegate.get().viewController = viewController.get();
    [webView setUIDelegate:uiDelegate.get()];
    [viewController setView:webView.get()];

    [webView synchronouslyLoadTestPageNamed:@"system-preview"];

    [webView _setSystemPreviewCompletionHandlerForLoadTesting:^(bool success) {
        EXPECT_FALSE(success);
        wasTriggered = true;
    }];

    [webView evaluateJavaScript:@"bloblink.click()" completionHandler:nil];

    Util::run(&wasTriggered);
}

TEST(WebKit, SystemPreviewUnknownMIMEType)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration _setSystemPreviewEnabled:YES];

    auto viewController = adoptNS([[UIViewController alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    auto uiDelegate = adoptNS([[TestSystemPreviewUIDelegate alloc] init]);
    uiDelegate.get().viewController = viewController.get();
    [webView setUIDelegate:uiDelegate.get()];
    [viewController setView:webView.get()];

    NSURL *modelURL = [[NSBundle mainBundle] URLForResource:@"UnitBox" withExtension:@"usdz" subdirectory:@"TestWebKitAPI.resources"];
    NSData *modelData = [NSData dataWithContentsOfURL:modelURL];
    NSString *modelBase64 = [modelData base64EncodedStringWithOptions:0];
    NSString *html = [NSString stringWithFormat:@"<script>let base64URL = 'data:application/octet-stream;base64,%@';"
        "fetch(base64URL)"
        "    .then((response) => response.blob())"
        "    .then((blob) => {"
        "        const blobURL = URL.createObjectURL(blob);"
        "        var a = document.createElement('a');"
        "        a.href = blobURL;"
        "        a.rel = 'ar';"
        "        document.body.appendChild(a);"
        "        var i = document.createElement('img');"
        "        a.appendChild(i);"
        "        a.click();"
        "    });</script>", modelBase64];

    [webView loadHTMLString:html baseURL:nil];

    [webView _setSystemPreviewCompletionHandlerForLoadTesting:^(bool success) {
        EXPECT_TRUE(success);
        wasTriggered = true;
    }];

    Util::run(&wasTriggered);
}

TEST(WebKit, SystemPreviewTriggered)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto messageHandler = adoptNS([[TestSystemPreviewTriggeredHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testSystemPreview"];
    [configuration _setSystemPreviewEnabled:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    [webView synchronouslyLoadTestPageNamed:@"system-preview-trigger"];
    Util::run(&hasTriggerInfo);

    [webView _triggerSystemPreviewActionOnElement:elementID document:documentID page:pageID];
    Util::run(&wasTriggered);
}

TEST(WebKit, SystemPreviewTriggeredOnDetachedElement)
{
    auto *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto messageHandler = adoptNS([[TestSystemPreviewTriggeredOnDetachedElementHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testSystemPreview"];
    [configuration _setSystemPreviewEnabled:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
    [webView synchronouslyLoadTestPageNamed:@"system-preview-trigger"];
    Util::run(&hasTriggerInfo);

    [webView _triggerSystemPreviewActionOnElement:elementID document:documentID page:pageID];
    Util::run(&wasTriggeredByDetachedElement);
}

}

#endif // (PLATFORM(IOS) || PLATFORM(VISION)) && USE(SYSTEM_PREVIEW)
