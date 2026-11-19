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

#import "Helpers/Test.h"
#import "Helpers/cocoa/TestCocoa.h"
#import "Helpers/cocoa/TestInspectorURLSchemeHandler.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/Utilities.h"
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
- (void)waitForDidAttachLocalInspector;
@end

@implementation UIDelegateForTestingInspectorExtensionDelegate {
    bool _didAttachLocalInspector;
    RetainPtr<TestInspectorURLSchemeHandler> _urlSchemeHandler;
}

- (void)_webView:(WKWebView *)webView didAttachLocalInspector:(_WKInspector *)inspector
{
    EXPECT_EQ(webView._inspector, inspector);
    _didAttachLocalInspector = true;
}

- (_WKInspectorConfiguration *)_webView:(WKWebView *)webView configurationForLocalInspector:(_WKInspector *)inspector
{
    if (!_urlSchemeHandler)
        _urlSchemeHandler = adoptNS([[TestInspectorURLSchemeHandler alloc] init]);

    RetainPtr inspectorConfiguration = adoptNS([[_WKInspectorConfiguration alloc] init]);
    [inspectorConfiguration setURLSchemeHandler:_urlSchemeHandler.get() forURLScheme:@"test-resource"];
    return inspectorConfiguration.autorelease();
}

- (void)waitForDidAttachLocalInspector
{
    TestWebKitAPI::Util::run(&_didAttachLocalInspector);
}

@end


@interface InspectorExtensionDelegateForTesting : NSObject <_WKInspectorExtensionDelegate>
@property (nonatomic) bool inspectedPageDidNavigate;
- (void)waitForDidShowExtensionTab;
- (void)waitForDidHideExtensionTab;
- (void)waitForInspectedPageDidNavigate;
@end

@implementation InspectorExtensionDelegateForTesting {
    bool _didShowExtensionTab;
    bool _didHideExtensionTab;
}

- (void)inspectorExtension:(_WKInspectorExtension *)extension didShowTabWithIdentifier:(NSString *)tabIdentifier withFrameHandle:(_WKFrameHandle *)frameHandle
{
    _didShowExtensionTab = true;
}

- (void)inspectorExtension:(_WKInspectorExtension *)extension didHideTabWithIdentifier:(NSString *)tabIdentifier
{
    _didHideExtensionTab = true;
}

- (void)inspectorExtension:(_WKInspectorExtension *)extension didNavigateTabWithIdentifier:(NSString *)tabIdentifier newURL:(NSURL *)newURL
{
    extensionTabDidNavigateWasCalled = true;
}

- (void)inspectorExtension:(_WKInspectorExtension *)extension inspectedPageDidNavigate:(NSURL *)newURL
{
    _inspectedPageDidNavigate = true;
    sharedNewURLAfterNavigation = newURL;
}

- (void)waitForDidShowExtensionTab
{
    TestWebKitAPI::Util::run(&_didShowExtensionTab);
}

- (void)waitForDidHideExtensionTab
{
    TestWebKitAPI::Util::run(&_didHideExtensionTab);
}

- (void)waitForInspectedPageDidNavigate
{
    TestWebKitAPI::Util::run(&_inspectedPageDidNavigate);
}

@end

// FIXME: Re-enable this test once webkit.org/b/231847 is fixed.
TEST(WKInspectorExtensionDelegate, DISABLED_ShowAndHideTabCallbacks)
{
    RetainPtr webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().preferences._developerExtrasEnabled = YES;
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    RetainPtr uiDelegate = adoptNS([UIDelegateForTestingInspectorExtensionDelegate new]);

    [webView setUIDelegate:uiDelegate.get()];
    [webView loadHTMLString:@"<head><title>Test page to be inspected</title></head><body><p>Filler content</p></body>" baseURL:[NSURL URLWithString:@"http://example.com/"]];

    [[webView _inspector] show];
    [uiDelegate waitForDidAttachLocalInspector];

    auto extensionID = [NSUUID UUID].UUIDString;
    auto extensionBundleIdentifier = @"com.apple.webkit.FirstExtension";
    auto extensionDisplayName = @"FirstExtension";
    __block RetainPtr<_WKInspectorExtension> inspectorExtension;
    __block RetainPtr<NSString> tabIdentifier;

    // Register the test extension.
    __block bool pendingCallbackWasCalled = false;
    [[webView _inspector] registerExtensionWithID:extensionID extensionBundleIdentifier:extensionBundleIdentifier displayName:extensionDisplayName completionHandler:^(NSError * _Nullable error, _WKInspectorExtension * _Nullable extension) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(extension);
        inspectorExtension = extension;

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    RetainPtr extensionDelegate = adoptNS([InspectorExtensionDelegateForTesting new]);
    [inspectorExtension setDelegate:extensionDelegate.get()];

    // Create an extension tab.
    auto iconURL = [NSURL URLWithString:@"test-resource://FirstExtension/InspectorExtension-TabIcon-30x30.png"];
    auto sourceURL = [NSURL URLWithString:@"test-resource://FirstExtension/InspectorExtension-basic-tab.html"];

    pendingCallbackWasCalled = false;
    [inspectorExtension createTabWithName:@"FirstExtension-Tab" tabIconURL:iconURL sourceURL:sourceURL completionHandler:^(NSError * _Nullable error, NSString * _Nullable extensionTabIdentifier) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(extensionTabIdentifier);
        tabIdentifier = extensionTabIdentifier;

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    pendingCallbackWasCalled = false;
    [[webView _inspector] showExtensionTabWithIdentifier:tabIdentifier.get() completionHandler:^(NSError * _Nullable error) {
        EXPECT_NULL(error);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);
    [extensionDelegate waitForDidShowExtensionTab];

    [[webView _inspector] showConsole];
    [extensionDelegate waitForDidHideExtensionTab];

    // Unregister the test extension.
    pendingCallbackWasCalled = false;
    [[webView _inspector] unregisterExtension:inspectorExtension.get() completionHandler:^(NSError * _Nullable error) {
        EXPECT_NULL(error);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);
}

// FIXME: Re-enable this test once webkit.org/b/257332 is fixed.
TEST(WKInspectorExtensionDelegate, DISABLED_InspectedPageNavigatedCallbacks)
{
    // Hook up the test-resource: handler so that we can navigate to a different test file.
    RetainPtr urlSchemeHandler = adoptNS([[TestInspectorURLSchemeHandler alloc] init]);

    RetainPtr webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().preferences._developerExtrasEnabled = YES;
    [webViewConfiguration setURLSchemeHandler:urlSchemeHandler.get() forURLScheme:@"test-resource"];

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    RetainPtr uiDelegate = adoptNS([UIDelegateForTestingInspectorExtensionDelegate new]);

    [webView setUIDelegate:uiDelegate.get()];

    [[webView _inspector] show];
    [uiDelegate waitForDidAttachLocalInspector];

    // Register the test extension.
    auto extensionID = [NSUUID UUID].UUIDString;
    auto extensionBundleIdentifier = @"com.apple.webkit.SecondExtension";
    auto extensionDisplayName = @"SecondExtension";
    __block RetainPtr<_WKInspectorExtension> inspectorExtension;
    __block bool pendingCallbackWasCalled = false;
    [[webView _inspector] registerExtensionWithID:extensionID extensionBundleIdentifier:extensionBundleIdentifier displayName:extensionDisplayName completionHandler:^(NSError * _Nullable error, _WKInspectorExtension * _Nullable extension) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(extension);
        inspectorExtension = extension;

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    RetainPtr extensionDelegate = adoptNS([InspectorExtensionDelegateForTesting new]);
    [inspectorExtension setDelegate:extensionDelegate.get()];

    [webView loadHTMLString:@"<head><title>Test page to be inspected</title></head><body><p>Filler content</p></body>" baseURL:[NSURL URLWithString:@"http://example.com/"]];
    [webView _test_waitForDidFinishNavigation];

    extensionDelegate.get().inspectedPageDidNavigate = false;
    [extensionDelegate waitForInspectedPageDidNavigate];
    EXPECT_NS_EQUAL(sharedNewURLAfterNavigation.get().absoluteString, @"http://example.com/");
    extensionDelegate.get().inspectedPageDidNavigate = false;

    // Initiate a navigation in the inspected WKWebView to a test-resource:// URL.
    auto newURL = [NSURL URLWithString:@"test-resource://SecondExtension/InspectorExtension-basic-page.html"];
    auto result = [webView loadRequest:[NSURLRequest requestWithURL:newURL]];
    EXPECT_NOT_NULL(result);

    extensionDelegate.get().inspectedPageDidNavigate = false;
    [extensionDelegate waitForInspectedPageDidNavigate];
    EXPECT_NS_EQUAL(sharedNewURLAfterNavigation.get().absoluteString, @"about:blank");

    extensionDelegate.get().inspectedPageDidNavigate = false;
    [extensionDelegate waitForInspectedPageDidNavigate];
    EXPECT_NS_EQUAL(sharedNewURLAfterNavigation.get().absoluteString, newURL.absoluteString);

    // Unregister the test extension.
    pendingCallbackWasCalled = false;
    [[webView _inspector] unregisterExtension:inspectorExtension.get() completionHandler:^(NSError * _Nullable error) {
        EXPECT_NULL(error);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);
}

// FIXME: Re-enable this test once webkit.org/b/257332 is fixed.
TEST(WKInspectorExtensionDelegate, DISABLED_ExtensionTabNavigatedCallbacks)
{
    // Hook up the test-resource: handler so that we can navigate to a different test file.
    RetainPtr urlSchemeHandler = adoptNS([[TestInspectorURLSchemeHandler alloc] init]);

    RetainPtr webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().preferences._developerExtrasEnabled = YES;
    [webViewConfiguration setURLSchemeHandler:urlSchemeHandler.get() forURLScheme:@"test-resource"];

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    RetainPtr uiDelegate = adoptNS([UIDelegateForTestingInspectorExtensionDelegate new]);

    [webView setUIDelegate:uiDelegate.get()];

    [[webView _inspector] show];
    [uiDelegate waitForDidAttachLocalInspector];

    // Register the test extension.
    auto extensionID = [NSUUID UUID].UUIDString;
    auto extensionBundleIdentifier = @"com.apple.webkit.ThirdExtension";
    auto extensionDisplayName = @"ThirdExtension";
    __block RetainPtr<_WKInspectorExtension> inspectorExtension;
    __block RetainPtr<NSString> tabIdentifier;
    __block bool pendingCallbackWasCalled = false;
    [[webView _inspector] registerExtensionWithID:extensionID extensionBundleIdentifier:extensionBundleIdentifier displayName:extensionDisplayName completionHandler:^(NSError *error, _WKInspectorExtension *extension) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(extension);
        inspectorExtension = extension;

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    RetainPtr extensionDelegate = adoptNS([InspectorExtensionDelegateForTesting new]);
    [inspectorExtension setDelegate:extensionDelegate.get()];

    auto baseURL = [NSURL URLWithString:@"http://example.com/"];
    [webView loadHTMLString:@"<head><title>Test page to be inspected</title></head><body><p>Filler content</p></body>" baseURL:baseURL];
    [webView _test_waitForDidFinishNavigation];

    extensionDelegate.get().inspectedPageDidNavigate = false;
    [extensionDelegate waitForInspectedPageDidNavigate];
    EXPECT_NS_EQUAL(sharedNewURLAfterNavigation.get().absoluteString, baseURL.absoluteString);
    extensionDelegate.get().inspectedPageDidNavigate = false;

    // Create an extension tab.
    auto iconURL = [NSURL URLWithString:@"test-resource://ThirdExtension/InspectorExtension-TabIcon-30x30.png"];
    auto sourceURL = [NSURL URLWithString:@"test-resource://ThirdExtension/InspectorExtension-basic-tab.html"];

    pendingCallbackWasCalled = false;
    [inspectorExtension createTabWithName:@"ThirdExtension-Tab" tabIconURL:iconURL sourceURL:sourceURL completionHandler:^(NSError *error, NSString *extensionTabIdentifier) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(extensionTabIdentifier);
        tabIdentifier = extensionTabIdentifier;

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    pendingCallbackWasCalled = false;
    [[webView _inspector] showExtensionTabWithIdentifier:tabIdentifier.get() completionHandler:^(NSError *error) {
        EXPECT_NULL(error);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    pendingCallbackWasCalled = false;
    extensionTabDidNavigateWasCalled = false;
    auto newURL = [NSURL URLWithString:@"test-resource://ThirdExtension/InspectorExtension-basic-page.html"];
    [inspectorExtension evaluateScript:[NSString stringWithFormat:@"window.location.replace(\"%@\")", newURL] inTabWithIdentifier:tabIdentifier.get() completionHandler:^(NSError *error, id result) {
        EXPECT_NULL(error);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);
    TestWebKitAPI::Util::run(&extensionTabDidNavigateWasCalled);

    // Unregister the test extension.
    pendingCallbackWasCalled = false;
    [[webView _inspector] unregisterExtension:inspectorExtension.get() completionHandler:^(NSError * _Nullable error) {
        EXPECT_NULL(error);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);
}

#endif // ENABLE(INSPECTOR_EXTENSIONS)
