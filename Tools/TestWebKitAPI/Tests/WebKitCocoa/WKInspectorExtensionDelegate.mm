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

#import "Test.h"
#import "Utilities.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKInspector.h>
#import <WebKit/_WKInspectorExtension.h>
#import <WebKit/_WKInspectorExtensionDelegate.h>
#import <WebKit/_WKInspectorPrivateForTesting.h>
#import <wtf/RetainPtr.h>

#if ENABLE(INSPECTOR_EXTENSIONS)

static bool didAttachLocalInspectorCalled = false;
static bool didShowExtensionTabWasCalled = false;
static bool didHideExtensionTabWasCalled = false;
static bool pendingCallbackWasCalled = false;
static RetainPtr<_WKInspectorExtension> sharedInspectorExtension;
static RetainPtr<NSString> sharedExtensionTabIdentifier;

static void resetGlobalState()
{
    didAttachLocalInspectorCalled = false;
    didShowExtensionTabWasCalled = false;
    didHideExtensionTabWasCalled = false;
    pendingCallbackWasCalled = false;
}

@interface UIDelegateForTestingInspectorExtensionDelegate : NSObject <WKUIDelegate>
@end

@implementation UIDelegateForTestingInspectorExtensionDelegate

- (void)_webView:(WKWebView *)webView didAttachLocalInspector:(_WKInspector *)inspector
{
    EXPECT_EQ(webView._inspector, inspector);
    didAttachLocalInspectorCalled = true;
}

@end


@interface InspectorExtensionDelegateForTesting : NSObject <_WKInspectorExtensionDelegate>
@end

@implementation InspectorExtensionDelegateForTesting {
}

- (void)inspectorExtension:(_WKInspectorExtension *)extension didShowTabWithIdentifier:(NSString *)tabIdentifier
{
    didShowExtensionTabWasCalled = true;
}

- (void)inspectorExtension:(_WKInspectorExtension *)extension didHideTabWithIdentifier:(NSString *)tabIdentifier
{
    didHideExtensionTabWasCalled = true;
}

@end

TEST(WKInspectorExtensionDelegate, ShowAndHideTabCallbacks)
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
    auto extensionDisplayName = @"FirstExtension";

    // Register the test extension.
    pendingCallbackWasCalled = false;
    [[webView _inspector] registerExtensionWithID:extensionID displayName:extensionDisplayName completionHandler:^(NSError * _Nullable error, _WKInspectorExtension * _Nullable extension) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(extension);
        sharedInspectorExtension = extension;

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    auto extensionDelegate = adoptNS([InspectorExtensionDelegateForTesting new]);
    [sharedInspectorExtension setDelegate:extensionDelegate.get()];

    // Create an extension tab.
    auto iconURL = [[NSBundle mainBundle] URLForResource:@"InspectorExtension-TabIcon-30x30" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"];
    auto sourceURL = [[NSBundle mainBundle] URLForResource:@"InspectorExtension-basic-tab" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];

    pendingCallbackWasCalled = false;
    [sharedInspectorExtension createTabWithName:@"FirstExtension-Tab" tabIconURL:iconURL sourceURL:sourceURL completionHandler:^(NSError * _Nullable error, NSString * _Nullable extensionTabIdentifier) {
        EXPECT_NULL(error);
        EXPECT_NOT_NULL(extensionTabIdentifier);
        sharedExtensionTabIdentifier = extensionTabIdentifier;

        pendingCallbackWasCalled = true;
    }];
    TestWebKitAPI::Util::run(&pendingCallbackWasCalled);

    // Force a known non-extension tab to be shown before showing the extension tab. Otherwise,
    // if the extension tab was already open, then this test would hang waiting for a didShow callback.
    [[webView _inspector] showConsole];

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

#endif // ENABLE(INSPECTOR_EXTENSIONS)
