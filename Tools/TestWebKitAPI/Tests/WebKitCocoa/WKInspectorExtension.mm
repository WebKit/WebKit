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

static RetainPtr<NSObject> evaluationResult;

@interface UIDelegateForTestingInspectorExtension : NSObject <WKUIDelegate>
@end

@implementation UIDelegateForTestingInspectorExtension

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


@interface InspectorExtensionDelegateForTestingInspectorExtension : NSObject <_WKInspectorExtensionDelegate>
@end

@implementation InspectorExtensionDelegateForTestingInspectorExtension {
}

- (void)inspectorExtension:(_WKInspectorExtension *)extension didShowTabWithIdentifier:(NSString *)tabIdentifier withFrameHandle:(_WKFrameHandle *)frameHandle
{
    didShowExtensionTabWasCalled = true;
}

- (void)inspectorExtension:(_WKInspectorExtension *)extension didHideTabWithIdentifier:(NSString *)tabIdentifier
{
    didHideExtensionTabWasCalled = true;
}

@end

TEST(WKInspectorExtension, CanEvaluateScriptInExtensionTab)
{
    resetGlobalState();

    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().preferences._developerExtrasEnabled = YES;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto uiDelegate = adoptNS([UIDelegateForTestingInspectorExtension new]);

    [webView setUIDelegate:uiDelegate.get()];
    [webView loadHTMLString:@"<head><title>Test page to be inspected</title></head><body><p>Filler content</p></body>" baseURL:[NSURL URLWithString:@"http://example.com/"]];

    [[webView _inspector] show];
    TestWebKitAPI::Util::run(&didAttachLocalInspectorCalled);

    auto extensionID = [NSUUID UUID].UUIDString;
    auto extensionBundleIdentifier = @"org.webkit.TestWebKitAPI.FirstExtension";
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

    auto extensionDelegate = adoptNS([InspectorExtensionDelegateForTestingInspectorExtension new]);
    [sharedInspectorExtension setDelegate:extensionDelegate.get()];

    // Create and show an extension tab.
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
    didShowExtensionTabWasCalled = false;
    [[webView _inspector] showExtensionTabWithIdentifier:sharedExtensionTabIdentifier.get() completionHandler:^(NSError * _Nullable error) {
        EXPECT_NULL(error);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);
    TestWebKitAPI::Util::run(&didShowExtensionTabWasCalled);

    // Read back a value that is set in the <iframe>'s script context.
    pendingCallbackWasCalled = false;
    auto scriptSource2 = @"window._secretValue";
    [sharedInspectorExtension evaluateScript:scriptSource2 inTabWithIdentifier:sharedExtensionTabIdentifier.get() completionHandler:^(NSError * _Nullable error, NSDictionary * _Nullable result) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(result);
        EXPECT_NS_EQUAL(result[@"answer"], @42);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    // Check to see that script is actually being evaluated in the <iframe>'s script context.
    pendingCallbackWasCalled = false;
    auto scriptSource3 = @"window.top !== window";
    [sharedInspectorExtension evaluateScript:scriptSource3 inTabWithIdentifier:sharedExtensionTabIdentifier.get() completionHandler:^(NSError * _Nullable error, NSDictionary * _Nullable result) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(result);
        EXPECT_NS_EQUAL(result, @YES);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    // Unregister the test extension.
    pendingCallbackWasCalled = false;
    [[webView _inspector] unregisterExtension:sharedInspectorExtension.get() completionHandler:^(NSError * _Nullable error) {
        EXPECT_NULL(error);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);
}

// FIXME: rdar://137088246
#if PLATFORM(MAC) && !defined(NDEBUG)
TEST(WKInspectorExtension, DISABLED_ExtensionTabIsPersistent)
#else
TEST(WKInspectorExtension, ExtensionTabIsPersistent)
#endif
{
    resetGlobalState();

    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().preferences._developerExtrasEnabled = YES;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto uiDelegate = adoptNS([UIDelegateForTestingInspectorExtension new]);

    [webView setUIDelegate:uiDelegate.get()];
    [webView loadHTMLString:@"<head><title>Test page to be inspected</title></head><body><p>Filler content</p></body>" baseURL:[NSURL URLWithString:@"http://example.com/"]];

    [[webView _inspector] show];
    TestWebKitAPI::Util::run(&didAttachLocalInspectorCalled);

    auto extensionID = [NSUUID UUID].UUIDString;
    auto extensionBundleIdentifier = @"org.webkit.TestWebKitAPI.SecondExtension";
    auto extensionDisplayName = @"SecondExtension";

    // Register the test extension.
    pendingCallbackWasCalled = false;
    [[webView _inspector] registerExtensionWithID:extensionID extensionBundleIdentifier:extensionBundleIdentifier displayName:extensionDisplayName completionHandler:^(NSError * _Nullable error, _WKInspectorExtension * _Nullable extension) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(extension);
        sharedInspectorExtension = extension;

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    auto extensionDelegate = adoptNS([InspectorExtensionDelegateForTestingInspectorExtension new]);
    [sharedInspectorExtension setDelegate:extensionDelegate.get()];

    // Create and show an extension tab.
    auto iconURL = [NSURL URLWithString:@"test-resource://SecondExtension/InspectorExtension-TabIcon-30x30.png"];
    auto sourceURL = [NSURL URLWithString:@"test-resource://SecondExtension/InspectorExtension-basic-tab.html"];

    pendingCallbackWasCalled = false;
    [sharedInspectorExtension createTabWithName:@"SecondExtension-Tab" tabIconURL:iconURL sourceURL:sourceURL completionHandler:^(NSError * _Nullable error, NSString * _Nullable extensionTabIdentifier) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(extensionTabIdentifier);
        sharedExtensionTabIdentifier = extensionTabIdentifier;

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    pendingCallbackWasCalled = false;
    didShowExtensionTabWasCalled = false;
    [[webView _inspector] showExtensionTabWithIdentifier:sharedExtensionTabIdentifier.get() completionHandler:^(NSError * _Nullable error) {
        EXPECT_NULL(error);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);
    TestWebKitAPI::Util::run(&didShowExtensionTabWasCalled);

    auto scriptSource = @"!!window.getUniqueValue && window.getUniqueValue()";

    // Read back a value that is unique to the <iframe>'s script context.
    pendingCallbackWasCalled = false;
    [sharedInspectorExtension evaluateScript:scriptSource inTabWithIdentifier:sharedExtensionTabIdentifier.get() completionHandler:^(NSError * _Nullable error, NSDictionary * _Nullable result) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(result);
        evaluationResult = result;

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    // Cause the extension tab to be hidden. Its <iframe> should not detach from the DOM.
    [[webView _inspector] showConsole];
    TestWebKitAPI::Util::run(&didHideExtensionTabWasCalled);

    // Check the unique value again, while the <iframe> is being hidden. If the <iframe> is
    // detached from the DOM, then this evaluation will fail or hang.
    pendingCallbackWasCalled = false;
    [sharedInspectorExtension evaluateScript:scriptSource inTabWithIdentifier:sharedExtensionTabIdentifier.get() completionHandler:^(NSError * _Nullable error, NSDictionary * _Nullable result) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(result);
        EXPECT_NS_EQUAL(result, evaluationResult.get());

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    // Reselect the extension tab.
    pendingCallbackWasCalled = false;
    didShowExtensionTabWasCalled = false;
    [[webView _inspector] showExtensionTabWithIdentifier:sharedExtensionTabIdentifier.get() completionHandler:^(NSError * _Nullable error) {
        EXPECT_NULL(error);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);
    TestWebKitAPI::Util::run(&didShowExtensionTabWasCalled);

    // Check the unique value again after reselecting the extension tab.
    pendingCallbackWasCalled = false;
    [sharedInspectorExtension evaluateScript:scriptSource inTabWithIdentifier:sharedExtensionTabIdentifier.get() completionHandler:^(NSError * _Nullable error, NSDictionary * _Nullable result) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(result);
        EXPECT_NS_EQUAL(result, evaluationResult.get());

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    // Unregister the test extension.
    pendingCallbackWasCalled = false;
    [[webView _inspector] unregisterExtension:sharedInspectorExtension.get() completionHandler:^(NSError * _Nullable error) {
        EXPECT_NULL(error);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);
}

TEST(WKInspectorExtension, EvaluateScriptInExtensionTabCanReturnPromises)
{
    resetGlobalState();

    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().preferences._developerExtrasEnabled = YES;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto uiDelegate = adoptNS([UIDelegateForTestingInspectorExtension new]);

    [webView setUIDelegate:uiDelegate.get()];
    [webView loadHTMLString:@"<head><title>Test page to be inspected</title></head><body><p>Filler content</p></body>" baseURL:[NSURL URLWithString:@"http://example.com/"]];

    [[webView _inspector] show];
    TestWebKitAPI::Util::run(&didAttachLocalInspectorCalled);

    auto extensionID = [NSUUID UUID].UUIDString;
    auto extensionBundleIdentifier = @"org.webkit.TestWebKitAPI.ThirdExtension";
    auto extensionDisplayName = @"ThirdExtension";

    // Register the test extension.
    pendingCallbackWasCalled = false;
    [[webView _inspector] registerExtensionWithID:extensionID extensionBundleIdentifier:extensionBundleIdentifier displayName:extensionDisplayName completionHandler:^(NSError * _Nullable error, _WKInspectorExtension * _Nullable extension) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(extension);
        sharedInspectorExtension = extension;

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    auto extensionDelegate = adoptNS([InspectorExtensionDelegateForTestingInspectorExtension new]);
    [sharedInspectorExtension setDelegate:extensionDelegate.get()];

    // Create and show an extension tab.
    auto iconURL = [NSURL URLWithString:@"test-resource://ThirdExtension/InspectorExtension-TabIcon-30x30.png"];
    auto sourceURL = [NSURL URLWithString:@"test-resource://ThirdExtension/InspectorExtension-basic-tab.html"];

    pendingCallbackWasCalled = false;
    [sharedInspectorExtension createTabWithName:@"ThirdExtension-Tab" tabIconURL:iconURL sourceURL:sourceURL completionHandler:^(NSError * _Nullable error, NSString * _Nullable extensionTabIdentifier) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(extensionTabIdentifier);
        sharedExtensionTabIdentifier = extensionTabIdentifier;

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    pendingCallbackWasCalled = false;
    didShowExtensionTabWasCalled = false;
    [[webView _inspector] showExtensionTabWithIdentifier:sharedExtensionTabIdentifier.get() completionHandler:^(NSError * _Nullable error) {
        EXPECT_NULL(error);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);
    TestWebKitAPI::Util::run(&didShowExtensionTabWasCalled);

    auto secretString = @"Open Sesame";
    auto scriptSource = [NSString stringWithFormat:@"Promise.resolve(\"%@\")", secretString];

    pendingCallbackWasCalled = false;
    [sharedInspectorExtension evaluateScript:scriptSource inTabWithIdentifier:sharedExtensionTabIdentifier.get() completionHandler:^(NSError * _Nullable error, NSDictionary * _Nullable result) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(result);
        EXPECT_NS_EQUAL(result, secretString);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    pendingCallbackWasCalled = false;
    [sharedInspectorExtension evaluateScript:@"(((" inTabWithIdentifier:sharedExtensionTabIdentifier.get() completionHandler:^(NSError * _Nullable error, NSDictionary * _Nullable result) {
        EXPECT_NOT_NULL(error);
        EXPECT_NULL(result);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    // Unregister the test extension.
    pendingCallbackWasCalled = false;
    [[webView _inspector] unregisterExtension:sharedInspectorExtension.get() completionHandler:^(NSError * _Nullable error) {
        EXPECT_NULL(error);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);
}

// FIXME: rdar://137088246
#if PLATFORM(MAC) && !defined(NDEBUG)
TEST(WKInspectorExtension, DISABLED_EvaluateScriptOnPage)
#else
TEST(WKInspectorExtension, EvaluateScriptOnPage)
#endif
{
    resetGlobalState();

    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().preferences._developerExtrasEnabled = YES;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto uiDelegate = adoptNS([UIDelegateForTestingInspectorExtension new]);
    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);

    auto *testPageFileURL = [NSBundle.test_resourcesBundle URLForResource:@"WKInspectorExtensionEvaluateScriptOnPage" withExtension:@"html"];

    [webView setUIDelegate:uiDelegate.get()];
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView loadFileURL:testPageFileURL allowingReadAccessToURL:testPageFileURL.URLByDeletingLastPathComponent];
    [navigationDelegate waitForDidFinishNavigation];

    [[webView _inspector] show];
    TestWebKitAPI::Util::run(&didAttachLocalInspectorCalled);

    auto extensionID = [NSUUID UUID].UUIDString;
    auto extensionBundleIdentifier = @"org.webkit.TestWebKitAPI.FourthExtension";
    auto extensionDisplayName = @"FourthExtension";

    // Register the test extension.
    pendingCallbackWasCalled = false;
    [[webView _inspector] registerExtensionWithID:extensionID extensionBundleIdentifier:extensionBundleIdentifier displayName:extensionDisplayName completionHandler:^(NSError *error, _WKInspectorExtension *extension) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(extension);
        sharedInspectorExtension = extension;

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    auto extensionDelegate = adoptNS([InspectorExtensionDelegateForTestingInspectorExtension new]);
    [sharedInspectorExtension setDelegate:extensionDelegate.get()];

    // Create and show an extension tab.
    auto iconURL = [NSURL URLWithString:@"test-resource://FourthExtension/InspectorExtension-TabIcon-30x30.png"];
    auto sourceURL = [NSURL URLWithString:@"test-resource://FourthExtension/InspectorExtension-basic-tab.html"];

    pendingCallbackWasCalled = false;
    [sharedInspectorExtension createTabWithName:@"FourthExtension-Tab" tabIconURL:iconURL sourceURL:sourceURL completionHandler:^(NSError *error, NSString *extensionTabIdentifier) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(extensionTabIdentifier);
        sharedExtensionTabIdentifier = extensionTabIdentifier;

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    pendingCallbackWasCalled = false;
    didShowExtensionTabWasCalled = false;
    [[webView _inspector] showExtensionTabWithIdentifier:sharedExtensionTabIdentifier.get() completionHandler:^(NSError *error) {
        EXPECT_NULL(error);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);
    TestWebKitAPI::Util::run(&didShowExtensionTabWasCalled);

    auto mainFrameSecretString = @"42-mainFrame";
    auto innerFrameSecretString = @"42-innerFrame";
    auto scriptSource = @"document.getElementById('secret').innerText";

    // Test main frame evaluation.
    pendingCallbackWasCalled = false;
    [sharedInspectorExtension evaluateScript:scriptSource frameURL:nil contextSecurityOrigin:nil useContentScriptContext:false completionHandler:^(NSError *error, NSDictionary *result) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(result);
        EXPECT_NS_EQUAL(result, mainFrameSecretString);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    // Test main frame evaluation failure.
    pendingCallbackWasCalled = false;
    [sharedInspectorExtension evaluateScript:@"[].x.x" frameURL:nil contextSecurityOrigin:nil useContentScriptContext:false completionHandler:^(NSError *error, NSDictionary *result) {
        EXPECT_NULL(result);
        EXPECT_NOT_NULL(error);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    // Test loose frameURL evaluation.
    auto *testPageInnerFrameFileURL = [NSURL URLWithString:[NSString stringWithFormat:@"%@WKInspectorExtensionEvaluateScriptOnPageInnerFrame.html", [[testPageFileURL URLByDeletingLastPathComponent] absoluteString]]];

    pendingCallbackWasCalled = false;
    [sharedInspectorExtension evaluateScript:scriptSource frameURL:testPageInnerFrameFileURL contextSecurityOrigin:nil useContentScriptContext:false completionHandler:^(NSError *error, NSDictionary *result) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(result);
        EXPECT_NS_EQUAL(result, innerFrameSecretString);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    // Test strict frameURL evaluation.
    auto *testPageInnerFrameStrictFileURL = [NSURL URLWithString:[NSString stringWithFormat:@"%@?query=param#fragment", [testPageInnerFrameFileURL absoluteString]]];

    pendingCallbackWasCalled = false;
    [sharedInspectorExtension evaluateScript:scriptSource frameURL:testPageInnerFrameStrictFileURL contextSecurityOrigin:nil useContentScriptContext:false completionHandler:^(NSError *error, NSDictionary *result) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(result);
        EXPECT_NS_EQUAL(result, innerFrameSecretString);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    // Unregister the test extension.
    pendingCallbackWasCalled = false;
    [[webView _inspector] unregisterExtension:sharedInspectorExtension.get() completionHandler:^(NSError * error) {
        EXPECT_NULL(error);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);
}

#endif // ENABLE(INSPECTOR_EXTENSIONS)
