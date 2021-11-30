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

#import "DeprecatedGlobalValues.h"
#import "Test.h"
#import "Utilities.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKInspector.h>
#import <WebKit/_WKInspectorPrivateForTesting.h>
#import <wtf/RetainPtr.h>

#if ENABLE(INSPECTOR_EXTENSIONS)

@interface UIDelegateForTestingInspectorExtensionHost : NSObject <WKUIDelegate>
@end

@implementation UIDelegateForTestingInspectorExtensionHost

- (void)_webView:(WKWebView *)webView didAttachLocalInspector:(_WKInspector *)inspector
{
    EXPECT_EQ(webView._inspector, inspector);
    didAttachLocalInspectorCalled = true;
}

@end

TEST(WKInspectorExtensionHost, RegisterExtension)
{
    resetGlobalState();

    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().preferences._developerExtrasEnabled = YES;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto uiDelegate = adoptNS([UIDelegateForTestingInspectorExtensionHost new]);

    [webView setUIDelegate:uiDelegate.get()];
    [webView loadHTMLString:@"<head><title>Test page to be inspected</title></head><body><p>Filler content</p></body>" baseURL:[NSURL URLWithString:@"http://example.com/"]];

    [[webView _inspector] show];
    TestWebKitAPI::Util::run(&didAttachLocalInspectorCalled);

    auto firstID = [NSUUID UUID].UUIDString;
    auto secondID = [NSUUID UUID].UUIDString;

    // Normal registration.
    pendingCallbackWasCalled = false;
    [[webView _inspector] registerExtensionWithID:firstID extensionBundleIdentifier:@"com.apple.webkit.FirstExtension" displayName:@"FirstExtension" completionHandler:^(NSError * _Nullable error, _WKInspectorExtension * _Nullable extension) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(extension);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    // Double registration.
    pendingCallbackWasCalled = false;
    [[webView _inspector] registerExtensionWithID:firstID extensionBundleIdentifier:@"com.apple.webkit.FirstExtension" displayName:@"FirstExtension" completionHandler:^(NSError * _Nullable error, _WKInspectorExtension * _Nullable extension) {
        EXPECT_NOT_NULL(error);
        EXPECT_NULL(extension);
        EXPECT_TRUE([error.localizedFailureReason containsString:@"RegistrationFailed"]);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    // Two registrations.
    pendingCallbackWasCalled = false;
    [[webView _inspector] registerExtensionWithID:secondID extensionBundleIdentifier:@"com.apple.webkit.SecondExtension" displayName:@"SecondExtension" completionHandler:^(NSError * _Nullable error, _WKInspectorExtension * _Nullable extension) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(extension);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);
}

TEST(WKInspectorExtensionHost, UnregisterExtension)
{
    resetGlobalState();

    auto webViewConfiguration = adoptNS([WKWebViewConfiguration new]);
    webViewConfiguration.get().preferences._developerExtrasEnabled = YES;
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto uiDelegate = adoptNS([UIDelegateForTestingInspectorExtensionHost new]);

    [webView setUIDelegate:uiDelegate.get()];
    [webView loadHTMLString:@"<head><title>Test page to be inspected</title></head><body><p>Filler content</p></body>" baseURL:[NSURL URLWithString:@"http://example.com/"]];

    [[webView _inspector] show];
    TestWebKitAPI::Util::run(&didAttachLocalInspectorCalled);

    auto firstID = [NSUUID UUID].UUIDString;
    __block RetainPtr<_WKInspectorExtension> foundExtension;

    // Unregister a known extension.
    pendingCallbackWasCalled = false;
    [[webView _inspector] registerExtensionWithID:firstID extensionBundleIdentifier:@"com.apple.webkit.FirstExtension" displayName:@"FirstExtension" completionHandler:^(NSError * _Nullable error, _WKInspectorExtension * _Nullable extension) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(extension);
        foundExtension = extension;

        [[webView _inspector] unregisterExtension:foundExtension.get() completionHandler:^(NSError * _Nullable error) {
            EXPECT_NULL(error);

            pendingCallbackWasCalled = true;
        }];
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);
    EXPECT_NOT_NULL(foundExtension.get());

    // Re-register an extension.
    pendingCallbackWasCalled = false;
    [[webView _inspector] registerExtensionWithID:firstID extensionBundleIdentifier:@"com.apple.webkit.FirstExtension" displayName:@"FirstExtension" completionHandler:^(NSError * _Nullable error, _WKInspectorExtension * _Nullable extension) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(extension);
        foundExtension = extension;

        [[webView _inspector] unregisterExtension:foundExtension.get() completionHandler:^(NSError * _Nullable error) {
            EXPECT_NULL(error);

            pendingCallbackWasCalled = true;
        }];
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);
    EXPECT_NOT_NULL(foundExtension.get());

    // Unregister an extension twice.
    pendingCallbackWasCalled = false;
    [[webView _inspector] unregisterExtension:foundExtension.get() completionHandler:^(NSError * _Nullable error) {
        EXPECT_NOT_NULL(error);
        EXPECT_TRUE([error.localizedFailureReason containsString:@"InvalidRequest"]);

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);
}

#endif // ENABLE(INSPECTOR_EXTENSIONS)
