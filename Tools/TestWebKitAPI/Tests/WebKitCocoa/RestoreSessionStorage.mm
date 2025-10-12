/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import <WebKit/WKScriptMessageHandler.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebsiteDataStore.h>
#import <wtf/RetainPtr.h>

static void testRestoreSessionStorage(RetainPtr<WKWebsiteDataStore> websiteDataStore)
{
    // Fetch Session Storage.

    // Clear the data store.
    __block bool done = false;
    [websiteDataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Create and load the WebView.
    RetainPtr viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:websiteDataStore.get()];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get()]);
    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadHTMLString:@"<body>webkit.org</body>" baseURL:[NSURL URLWithString:@"https://webkit.org"]];
    [navigationDelegate waitForDidFinishNavigation];

    // Put data into session storage.
    [webView evaluateJavaScript:@"sessionStorage.setItem('key', 'value');" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Fetch the session storage data.
    __block RetainPtr<NSData> sessionStorageData;
    [webView fetchDataOfTypes:WKWebViewDataTypeSessionStorage completionHandler:^(NSData *data, NSError *error) {
        EXPECT_NOT_NULL(data);
        EXPECT_NULL(error);
        sessionStorageData = data;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Restore Session Storage.

    // Clear the data store.
    [websiteDataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Create a new WebView.
    RetainPtr newWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get()]);
    [newWebView setNavigationDelegate:navigationDelegate.get()];

    // Restore the session storage data.
    [newWebView restoreData:sessionStorageData.get() completionHandler:^(NSError *error) {
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Load the same page as before.
    [newWebView loadHTMLString:@"<body>webkit.org</body>" baseURL:[NSURL URLWithString:@"https://webkit.org"]];
    [navigationDelegate waitForDidFinishNavigation];

    // Check that the session storage data was correctly restored.
    [newWebView evaluateJavaScript:@"sessionStorage.getItem('key');" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"value", value);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;
}

TEST(WebKit, RestoreSessionStorageFromPersistentDataStore)
{
    testRestoreSessionStorage([WKWebsiteDataStore defaultDataStore]);
}

TEST(WebKit, RestoreSessionStorageFromEphemeralDataStore)
{
    testRestoreSessionStorage([WKWebsiteDataStore nonPersistentDataStore]);
}

@interface RestoreSessionStorageMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation RestoreSessionStorageMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;
    lastScriptMessage = message;
}

@end

static void testRestoreSessionStorageThirdPartyIFrame(RetainPtr<WKWebsiteDataStore> websiteDataStore)
{
    static NSString *mainFrameString = @"<script> \
        function postResult(event) \
        { \
            window.webkit.messageHandlers.testHandler.postMessage(event.data); \
        } \
        addEventListener('message', postResult, false); \
        </script> \
        <iframe src='https://127.0.0.1:9091/'>";

    static constexpr auto frameBytes = R"TESTRESOURCE(
    <script>
    function postMessage(message)
    {
        parent.postMessage(message, '*');
    }
    function item()
    {
        result = sessionStorage.getItem('key');
        if (!result) {
            sessionStorage.setItem('key', 'value');
            postMessage('Item is set');
        } else
            postMessage(result);
    }
    item();
    </script>
    )TESTRESOURCE"_s;

    // Fetch Session Storage.

    // Clear the data store.
    __block bool done = false;
    [websiteDataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Create and load the WebView.
    RetainPtr viewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr handler = adoptNS([[RestoreSessionStorageMessageHandler alloc] init]);
    [[viewConfiguration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    [viewConfiguration setWebsiteDataStore:websiteDataStore.get()];

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get()]);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    }];
    [navigationDelegate setDecidePolicyForNavigationAction:[&](WKNavigationAction *action, void (^decisionHandler)(WKNavigationActionPolicy)) {
        decisionHandler(WKNavigationActionPolicyAllow);
    }];
    [webView setNavigationDelegate:navigationDelegate.get()];

    TestWebKitAPI::HTTPServer server({
        { "/"_s, { frameBytes } },
    }, TestWebKitAPI::HTTPServer::Protocol::Https, nullptr, nullptr, 9091);

    // The third party iframe will put the data into session storage.
    [webView loadHTMLString:mainFrameString baseURL:[NSURL URLWithString:@"https://webkit.org"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"Item is set", [lastScriptMessage body]);

    // Fetch the session storage data.
    __block RetainPtr<NSData> sessionStorageData;
    [webView fetchDataOfTypes:WKWebViewDataTypeSessionStorage completionHandler:^(NSData *data, NSError *error) {
        EXPECT_NOT_NULL(data);
        EXPECT_NULL(error);
        sessionStorageData = data;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Restore Session Storage.

    // Clear the data store.
    [websiteDataStore removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // Create a new WebView.
    RetainPtr newWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get()]);
    [newWebView setNavigationDelegate:navigationDelegate.get()];

    // Restore the local storage data.
    [newWebView restoreData:sessionStorageData.get() completionHandler:^(NSError *error) {
        EXPECT_NULL(error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    // The third party iframe will check that the session storage data was correctly restored.
    [newWebView loadHTMLString:mainFrameString baseURL:[NSURL URLWithString:@"https://webkit.org"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    EXPECT_WK_STREQ(@"value", [lastScriptMessage body]);
}

TEST(WebKit, RestoreSessionStorageFromPersistentDataStoreThirdPartyIFrame)
{
    testRestoreSessionStorageThirdPartyIFrame([WKWebsiteDataStore defaultDataStore]);
}

TEST(WebKit, RestoreSessionStorageFromEphemeralDataStoreThirdPartyIFrame)
{
    testRestoreSessionStorageThirdPartyIFrame([WKWebsiteDataStore nonPersistentDataStore]);
}
