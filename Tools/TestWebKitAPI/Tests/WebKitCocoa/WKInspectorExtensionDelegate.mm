/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(INSPECTOR_EXTENSIONS)

#import "DeprecatedGlobalValues.h"
#import "Test.h"
#import "TestCocoa.h"
#import "TestInspectorURLSchemeHandler.h"
#import "TestNavigationDelegate.h"
#import "Utilities.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKInspector.h>
#import <WebKit/_WKInspectorConfiguration.h>
#import <WebKit/_WKInspectorExtension.h>
#import <WebKit/_WKInspectorExtensionDelegate.h>
#import <WebKit/_WKInspectorPrivateForTesting.h>
#import <wtf/RetainPtr.h>

static RetainPtr<NSURL> sharedNewURLAfterNavigation;
static bool extensionTabDidNavigateWasCalled;

@interface UIDelegateForTestingInspectorExtensionDelegate : NSObject <WKUIDelegate>
@end

@implementation UIDelegateForTestingInspectorExtensionDelegate

- (void)_webView:(WKWebView *)webView didAttachLocalInspector:(_WKInspector *)inspector
{
    EXPECT_EQ(webView._inspector, inspector);
    didAttachLocalInspectorCalled = true;
}

- (_WKInspectorConfiguration *)_webView:(WKWebView *)webView configurationForLocalInspector:(_WKInspector *)inspector
{
    if (!sharedURLSchemeHandler)
        sharedURLSchemeHandler = adoptNS([[TestInspectorURLSchemeHandler alloc] init]);

    auto inspectorConfiguration = adoptNS([[_WKInspectorConfiguration alloc] init]);
    [inspectorConfiguration setURLSchemeHandler:sharedURLSchemeHandler.get() forURLScheme:@"test-resource"];
    return inspectorConfiguration.autorelease();
}

@end


@interface InspectorExtensionDelegateForTesting : NSObject <_WKInspectorExtensionDelegate>
@end

@implementation InspectorExtensionDelegateForTesting {
}

- (void)inspectorExtension:(_WKInspectorExtension *)extension didShowTabWithIdentifier:(NSString *)tabIdentifier withFrameHandle:(_WKFrameHandle *)frameHandle
{
    didShowExtensionTabWasCalled = true;
}

- (void)inspectorExtension:(_WKInspectorExtension *)extension didHideTabWithIdentifier:(NSString *)tabIdentifier
{
    didHideExtensionTabWasCalled = true;
}

- (void)inspectorExtension:(_WKInspectorExtension *)extension didNavigateTabWithIdentifier:(NSString *)tabIdentifier newURL:(NSURL *)newURL
{
    extensionTabDidNavigateWasCalled = true;
}

- (void)inspectorExtension:(_WKInspectorExtension *)extension inspectedPageDidNavigate:(NSURL *)newURL
{
    inspectedPageDidNavigateWasCalled = true;
    sharedNewURLAfterNavigation = newURL;
}

@end

// FIXME: Re-enable this test once webkit.org/b/231847 is fixed.
TEST(WKInspectorExtensionDelegate, DISABLED_ShowAndHideTabCallbacks)
{
    resetGlobalState();

    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().preferences._developerExtrasEnabled = YES;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto uiDelegate = adoptNS([UIDelegateForTestingInspectorExtensionDelegate new]);

    [webView setUIDelegate:uiDelegate.get()];
    [webView loadHTMLString:@"<head><title>Test page to be inspected</title></head><body><p>Filler content</p></body>" baseURL:[NSURL URLWithString:@"http://example.com/"]];

    [[webView _inspector] show];
    TestWebKitAPI::Util::run(&didAttachLocalInspectorCalled);

    auto extensionID = [NSUUID UUID].UUIDString;
    auto extensionBundleIdentifier = @"com.apple.webkit.FirstExtension";
    auto extensionDisplayName = @"FirstExtension";

    // Register the test extension.
    pendingCallbackWasCalled = false;
    [[webView _inspector] registerExtensionWithID:extensionID extensionBundleIdentifier:extensionBundleIdentifier displayName:extensionDisplayName completionHandler:^(NSError * _Nullable error, _WKInspectorExtension * _Nullable extension) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(extension);
        sharedInspectorExtension = extension;

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    auto extensionDelegate = adoptNS([InspectorExtensionDelegateForTesting new]);
    [sharedInspectorExtension setDelegate:extensionDelegate.get()];

    // Create an extension tab.
    auto iconURL = [NSURL URLWithString:@"test-resource://FirstExtension/InspectorExtension-TabIcon-30x30.png"];
    auto sourceURL = [NSURL URLWithString:@"test-resource://FirstExtension/InspectorExtension-basic-tab.html"];

    pendingCallbackWasCalled = false;
    [sharedInspectorExtension createTabWithName:@"FirstExtension-Tab" tabIconURL:iconURL sourceURL:sourceURL completionHandler:^(NSError * _Nullable error, NSString * _Nullable extensionTabIdentifier) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(extensionTabIdentifier);
        sharedExtensionTabIdentifier = extensionTabIdentifier;

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    pendingCallbackWasCalled = false;
    [[webView _inspector] showExtensionTabWithIdentifier:sharedExtensionTabIdentifier.get() completionHandler:^(NSError * _Nullable error) {
        EXPECT_NULL(error);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);
    TestWebKitAPI::Util::run(&didShowExtensionTabWasCalled);

    [[webView _inspector] showConsole];
    TestWebKitAPI::Util::run(&didHideExtensionTabWasCalled);

    // Unregister the test extension.
    pendingCallbackWasCalled = false;
    [[webView _inspector] unregisterExtension:sharedInspectorExtension.get() completionHandler:^(NSError * _Nullable error) {
        EXPECT_NULL(error);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);
}

// FIXME: Re-enable this test once webkit.org/b/257332 is fixed.
TEST(WKInspectorExtensionDelegate, DISABLED_InspectedPageNavigatedCallbacks)
{
    resetGlobalState();

    // Hook up the test-resource: handler so that we can navigate to a different test file.
    if (!sharedURLSchemeHandler)
        sharedURLSchemeHandler = adoptNS([[TestInspectorURLSchemeHandler alloc] init]);

    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().preferences._developerExtrasEnabled = YES;
    [webViewConfiguration setURLSchemeHandler:sharedURLSchemeHandler.get() forURLScheme:@"test-resource"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto uiDelegate = adoptNS([UIDelegateForTestingInspectorExtensionDelegate new]);

    [webView setUIDelegate:uiDelegate.get()];

    [[webView _inspector] show];
    TestWebKitAPI::Util::run(&didAttachLocalInspectorCalled);

    // Register the test extension.
    auto extensionID = [NSUUID UUID].UUIDString;
    auto extensionBundleIdentifier = @"com.apple.webkit.SecondExtension";
    auto extensionDisplayName = @"SecondExtension";
    pendingCallbackWasCalled = false;
    [[webView _inspector] registerExtensionWithID:extensionID extensionBundleIdentifier:extensionBundleIdentifier displayName:extensionDisplayName completionHandler:^(NSError * _Nullable error, _WKInspectorExtension * _Nullable extension) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(extension);
        sharedInspectorExtension = extension;

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    auto extensionDelegate = adoptNS([InspectorExtensionDelegateForTesting new]);
    [sharedInspectorExtension setDelegate:extensionDelegate.get()];

    [webView loadHTMLString:@"<head><title>Test page to be inspected</title></head><body><p>Filler content</p></body>" baseURL:[NSURL URLWithString:@"http://example.com/"]];
    [webView _test_waitForDidFinishNavigation];

    inspectedPageDidNavigateWasCalled = false;
    TestWebKitAPI::Util::run(&inspectedPageDidNavigateWasCalled);
    EXPECT_NS_EQUAL(sharedNewURLAfterNavigation.get().absoluteString, @"http://example.com/");
    inspectedPageDidNavigateWasCalled = false;

    // Initiate a navigation in the inspected WKWebView to a test-resource:// URL.
    auto newURL = [NSURL URLWithString:@"test-resource://SecondExtension/InspectorExtension-basic-page.html"];
    auto result = [webView loadRequest:[NSURLRequest requestWithURL:newURL]];
    EXPECT_NOT_NULL(result);

    inspectedPageDidNavigateWasCalled = false;
    TestWebKitAPI::Util::run(&inspectedPageDidNavigateWasCalled);
    EXPECT_NS_EQUAL(sharedNewURLAfterNavigation.get().absoluteString, @"about:blank");

    inspectedPageDidNavigateWasCalled = false;
    TestWebKitAPI::Util::run(&inspectedPageDidNavigateWasCalled);
    EXPECT_NS_EQUAL(sharedNewURLAfterNavigation.get().absoluteString, newURL.absoluteString);

    // Unregister the test extension.
    pendingCallbackWasCalled = false;
    [[webView _inspector] unregisterExtension:sharedInspectorExtension.get() completionHandler:^(NSError * _Nullable error) {
        EXPECT_NULL(error);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);
}

// FIXME: Re-enable this test once webkit.org/b/257332 is fixed.
TEST(WKInspectorExtensionDelegate, DISABLED_ExtensionTabNavigatedCallbacks)
{
    resetGlobalState();

    // Hook up the test-resource: handler so that we can navigate to a different test file.
    if (!sharedURLSchemeHandler)
        sharedURLSchemeHandler = adoptNS([[TestInspectorURLSchemeHandler alloc] init]);

    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().preferences._developerExtrasEnabled = YES;
    [webViewConfiguration setURLSchemeHandler:sharedURLSchemeHandler.get() forURLScheme:@"test-resource"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto uiDelegate = adoptNS([UIDelegateForTestingInspectorExtensionDelegate new]);

    [webView setUIDelegate:uiDelegate.get()];

    [[webView _inspector] show];
    TestWebKitAPI::Util::run(&didAttachLocalInspectorCalled);

    // Register the test extension.
    auto extensionID = [NSUUID UUID].UUIDString;
    auto extensionBundleIdentifier = @"com.apple.webkit.ThirdExtension";
    auto extensionDisplayName = @"ThirdExtension";
    pendingCallbackWasCalled = false;
    [[webView _inspector] registerExtensionWithID:extensionID extensionBundleIdentifier:extensionBundleIdentifier displayName:extensionDisplayName completionHandler:^(NSError *error, _WKInspectorExtension *extension) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(extension);
        sharedInspectorExtension = extension;

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    auto extensionDelegate = adoptNS([InspectorExtensionDelegateForTesting new]);
    [sharedInspectorExtension setDelegate:extensionDelegate.get()];

    auto baseURL = [NSURL URLWithString:@"http://example.com/"];
    [webView loadHTMLString:@"<head><title>Test page to be inspected</title></head><body><p>Filler content</p></body>" baseURL:baseURL];
    [webView _test_waitForDidFinishNavigation];

    inspectedPageDidNavigateWasCalled = false;
    TestWebKitAPI::Util::run(&inspectedPageDidNavigateWasCalled);
    EXPECT_NS_EQUAL(sharedNewURLAfterNavigation.get().absoluteString, baseURL.absoluteString);
    inspectedPageDidNavigateWasCalled = false;

    // Create an extension tab.
    auto iconURL = [NSURL URLWithString:@"test-resource://ThirdExtension/InspectorExtension-TabIcon-30x30.png"];
    auto sourceURL = [NSURL URLWithString:@"test-resource://ThirdExtension/InspectorExtension-basic-tab.html"];

    pendingCallbackWasCalled = false;
    [sharedInspectorExtension createTabWithName:@"ThirdExtension-Tab" tabIconURL:iconURL sourceURL:sourceURL completionHandler:^(NSError *error, NSString *extensionTabIdentifier) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(extensionTabIdentifier);
        sharedExtensionTabIdentifier = extensionTabIdentifier;

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    pendingCallbackWasCalled = false;
    [[webView _inspector] showExtensionTabWithIdentifier:sharedExtensionTabIdentifier.get() completionHandler:^(NSError *error) {
        EXPECT_NULL(error);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    pendingCallbackWasCalled = false;
    extensionTabDidNavigateWasCalled = false;
    auto newURL = [NSURL URLWithString:@"test-resource://ThirdExtension/InspectorExtension-basic-page.html"];
    [sharedInspectorExtension evaluateScript:[NSString stringWithFormat:@"window.location.replace(\"%@\")", newURL] inTabWithIdentifier:sharedExtensionTabIdentifier.get() completionHandler:^(NSError *error, id result) {
        EXPECT_NULL(error);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);
    TestWebKitAPI::Util::run(&extensionTabDidNavigateWasCalled);

    // Unregister the test extension.
    pendingCallbackWasCalled = false;
    [[webView _inspector] unregisterExtension:sharedInspectorExtension.get() completionHandler:^(NSError * _Nullable error) {
        EXPECT_NULL(error);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);
}

#endif // ENABLE(INSPECTOR_EXTENSIONS)
