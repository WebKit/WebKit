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

#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "TestNSBundleExtras.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import "TestWKWebViewController.h"
#import "Utilities.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WebKit.h>
#import <objc/runtime.h>
#import <wtf/BlockPtr.h>
#import <wtf/text/WTFString.h>
#import <pal/ios/QuickLookSoftLink.h>

#if PLATFORM(VISION)
SOFT_LINK_PRIVATE_FRAMEWORK(AssetViewer);
SOFT_LINK_CLASS(AssetViewer, ASVLaunchPreview);
#endif

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
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration _setSystemPreviewEnabled:YES];

    RetainPtr viewController = adoptNS([[TestWKWebViewController alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    RetainPtr webView = [viewController webView];
    RetainPtr uiDelegate = adoptNS([[TestSystemPreviewUIDelegate alloc] init]);
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

    NSURL *modelURL = [NSBundle.test_resourcesBundle URLForResource:@"UnitBox" withExtension:@"usdz"];
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
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration _setSystemPreviewEnabled:YES];

    RetainPtr viewController = adoptNS([[TestWKWebViewController alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    RetainPtr webView = [viewController webView];
    RetainPtr uiDelegate = adoptNS([[TestSystemPreviewUIDelegate alloc] init]);
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

    NSURL *modelURL = [NSBundle.test_resourcesBundle URLForResource:@"UnitBox" withExtension:@"usdz"];
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

    [webView _triggerSystemPreviewActionOnElement:elementID document:documentID.createNSString().get() page:pageID];
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

    [webView _triggerSystemPreviewActionOnElement:elementID document:documentID.createNSString().get() page:pageID];
    Util::run(&wasTriggeredByDetachedElement);
}

static void testModelPreviewPrompt(void(^loadModel)(TestWKWebView *)) {
    bool arqlInvoked = false;
    bool alertPresented = false;

    auto swizzledQLItemBlock = makeBlockPtr([&](id self, SEL _cmd, id dataProvider, id contentType, id previewTitle) {
        arqlInvoked = true;
        return nil;
    });

    Class qlItemClass = PAL::getQLItemClassSingleton();
    SEL targetSelector = @selector(initWithDataProvider:contentType:previewTitle:);

    InstanceMethodSwizzler qlItemSwizzler { qlItemClass, targetSelector, imp_implementationWithBlock(swizzledQLItemBlock.get()) };

    auto swizzledPresentBlock = makeBlockPtr([&](id self, UIViewController *controllerToPresent, BOOL animated, void (^completion)()) {
        EXPECT_TRUE([controllerToPresent isKindOfClass:[UIAlertController class]]);
        UIAlertController *alert = (UIAlertController *)controllerToPresent;
        alertPresented = true;

        EXPECT_WK_STREQ(@"View 3D Object?", alert.title);
        EXPECT_EQ(2U, alert.actions.count);

        UIAlertAction *cancelAction = alert.actions[0];
        EXPECT_EQ(UIAlertActionStyleCancel, cancelAction.style);
        id cancelHandler = [cancelAction valueForKey:@"handler"];
        EXPECT_TRUE(cancelHandler != nil);
        void (^cancelHandlerBlock)(UIAlertAction *) = cancelHandler;
        cancelHandlerBlock(cancelAction);
        EXPECT_FALSE(arqlInvoked);

        UIAlertAction *allowAction = alert.actions[1];
        EXPECT_EQ(UIAlertActionStyleDefault, allowAction.style);
        id allowHandler = [allowAction valueForKey:@"handler"];
        EXPECT_TRUE(allowHandler != nil);
        void (^allowHandlerBlock)(UIAlertAction *) = allowHandler;
        allowHandlerBlock(allowAction);
        EXPECT_TRUE(arqlInvoked);
    });

    InstanceMethodSwizzler presentSwizzler { [UIViewController class], @selector(presentViewController:animated:completion:), imp_implementationWithBlock(swizzledPresentBlock.get()) };

    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration _setSystemPreviewEnabled:YES];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);

    RetainPtr viewController = adoptNS([[UIViewController alloc] init]);
    RetainPtr uiDelegate = adoptNS([[TestSystemPreviewUIDelegate alloc] init]);
    [uiDelegate setViewController:viewController.get()];
    [webView setUIDelegate:uiDelegate.get()];
    [viewController setView:webView.get()];

    loadModel(webView.get());

    TestWebKitAPI::Util::run(&alertPresented);
}

static void testRelARPrompt(void(^loadModel)(TestWKWebView *)) {
    bool arInvoked = false;
    bool alertPresented = false;
    bool testingAllowPath = false;

#if PLATFORM(VISION)
    auto swizzledASVBeginBlock = makeBlockPtr([&](id self, SEL _cmd, NSArray *urls, BOOL is3DContent, NSURL *websiteURL, void (^completion)(NSError *)) {
        arInvoked = true;
        if (completion)
            completion(nil);
    });

    Class asvLaunchPreviewClass = getASVLaunchPreviewClassSingleton();
    SEL beginSelector = NSSelectorFromString(@"beginPreviewApplicationWithURLs:is3DContent:websiteURL:completion:");
    InstanceMethodSwizzler asvSwizzler { asvLaunchPreviewClass, beginSelector, imp_implementationWithBlock(swizzledASVBeginBlock.get()) };
#else
    auto swizzledSetDataSourceBlock = makeBlockPtr([&](id self, SEL _cmd, id dataSource) {
        arInvoked = true;
    });

    Class qlPreviewControllerClass = PAL::getQLPreviewControllerClassSingleton();
    InstanceMethodSwizzler methodSwizzler { qlPreviewControllerClass, @selector(setDataSource:), imp_implementationWithBlock(swizzledSetDataSourceBlock.get()) };
#endif

    auto swizzledPresentBlock = makeBlockPtr([&](id self, UIViewController *controllerToPresent, BOOL animated, void (^completion)()) {
#if !PLATFORM(VISION)
        if ([controllerToPresent isKindOfClass:PAL::getQLPreviewControllerClassSingleton()])
            return;
#endif

        EXPECT_TRUE([controllerToPresent isKindOfClass:[UIAlertController class]]);
        UIAlertController *alert = (UIAlertController *)controllerToPresent;
        alertPresented = true;

        EXPECT_WK_STREQ(@"View in AR?", alert.title);
        EXPECT_EQ(2U, alert.actions.count);

        if (testingAllowPath) {
            UIAlertAction *allowAction = alert.actions[1];
            EXPECT_EQ(UIAlertActionStyleDefault, allowAction.style);
            id handler = [allowAction valueForKey:@"handler"];
            EXPECT_TRUE(handler != nil);
            void (^handlerBlock)(UIAlertAction *) = handler;
            handlerBlock(allowAction);
            EXPECT_TRUE(arInvoked);
        } else {
            UIAlertAction *cancelAction = alert.actions[0];
            EXPECT_EQ(UIAlertActionStyleCancel, cancelAction.style);
            id handler = [cancelAction valueForKey:@"handler"];
            EXPECT_TRUE(handler != nil);
            void (^handlerBlock)(UIAlertAction *) = handler;
            handlerBlock(cancelAction);
            EXPECT_FALSE(arInvoked);
        }
    });

    InstanceMethodSwizzler presentSwizzler { [UIViewController class], @selector(presentViewController:animated:completion:), imp_implementationWithBlock(swizzledPresentBlock.get()) };

    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration _setSystemPreviewEnabled:YES];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);

    RetainPtr viewController = adoptNS([[UIViewController alloc] init]);
    RetainPtr uiDelegate = adoptNS([[TestSystemPreviewUIDelegate alloc] init]);
    [uiDelegate setViewController:viewController.get()];
    [webView setUIDelegate:uiDelegate.get()];
    [viewController setView:webView.get()];

    loadModel(webView.get());
    TestWebKitAPI::Util::run(&alertPresented);

    testingAllowPath = true;
    alertPresented = false;

    loadModel(webView.get());
    TestWebKitAPI::Util::run(&alertPresented);
}

TEST(WebKit, PromptUSDZTopLevelNavigation)
{
    testModelPreviewPrompt(^(TestWKWebView *webView) {
        RetainPtr usdzURL = [NSBundle.test_resourcesBundle URLForResource:@"UnitBox" withExtension:@"usdz"];
        [webView loadRequest:[NSURLRequest requestWithURL:usdzURL.get()]];
    });
}

TEST(WebKit, PromptRealityTopLevelNavigation)
{
    testModelPreviewPrompt(^(TestWKWebView *webView) {
        RetainPtr realityURL = [NSBundle.test_resourcesBundle URLForResource:@"hab" withExtension:@"reality"];
        [webView loadRequest:[NSURLRequest requestWithURL:realityURL.get()]];
    });
}

TEST(WebKit, PromptUSDZLinkWithoutRelAR)
{
    testModelPreviewPrompt(^(TestWKWebView *webView) {
        [webView synchronouslyLoadTestPageNamed:@"system-preview"];
        [webView evaluateJavaScript:@"document.getElementById('usdz-link').click()" completionHandler:nil];
    });
}

TEST(WebKit, PromptRealityLinkWithoutRelAR)
{
    testModelPreviewPrompt(^(TestWKWebView *webView) {
        [webView synchronouslyLoadTestPageNamed:@"system-preview"];
        [webView evaluateJavaScript:@"document.getElementById('reality-link').click()" completionHandler:nil];
    });
}

TEST(WebKit, PromptUSDZLinkWithRelAR)
{
    testRelARPrompt(^(TestWKWebView *webView) {
        [webView synchronouslyLoadTestPageNamed:@"system-preview"];
        [webView evaluateJavaScript:@"document.getElementById('arlink').click()" completionHandler:nil];
    });
}

TEST(WebKit, PromptRealityLinkWithRelAR)
{
    testRelARPrompt(^(TestWKWebView *webView) {
        [webView synchronouslyLoadTestPageNamed:@"system-preview"];
        [webView evaluateJavaScript:@"document.getElementById('reality-with-relar-link').click()" completionHandler:nil];
    });
}

}

#endif // (PLATFORM(IOS) || PLATFORM(VISION)) && USE(SYSTEM_PREVIEW)
