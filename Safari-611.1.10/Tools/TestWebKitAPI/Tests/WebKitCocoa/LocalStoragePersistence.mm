/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#import "PlatformUtilities.h"
#import "Test.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKUserStyleSheet.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

static bool readyToContinue;
static bool receivedScriptMessage;
static bool didFinishNavigation;
static RetainPtr<WKScriptMessage> lastScriptMessage;
static RetainPtr<WKWebView> createdWebView;

@interface LocalStorageMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation LocalStorageMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;
    lastScriptMessage = message;
}

@end

@interface LocalStorageNavigationDelegate : NSObject <WKNavigationDelegate, WKUIDelegate> {
    @public void (^decidePolicyForNavigationActionHandler)(WKNavigationAction *, WKWebpagePreferences *, void (^)(WKNavigationActionPolicy, WKWebpagePreferences *));
}
@end

@implementation LocalStorageNavigationDelegate

- (WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures
{
    createdWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
    [createdWebView setNavigationDelegate:self];
    return createdWebView.get();
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction preferences:(WKWebpagePreferences *)preferences decisionHandler:(void (^)(WKNavigationActionPolicy, WKWebpagePreferences *))decisionHandler
{
    if (decidePolicyForNavigationActionHandler)
        decidePolicyForNavigationActionHandler(navigationAction, preferences, decisionHandler);
    else
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    didFinishNavigation = true;
}

@end

TEST(WKWebView, LocalStorageProcessCrashes)
{
    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    RetainPtr<LocalStorageMessageHandler> handler = adoptNS([[LocalStorageMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    [configuration _setAllowUniversalAccessFromFileURLs:YES];
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"local-storage-process-crashes" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];

    receivedScriptMessage = false;
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"local:storage", [lastScriptMessage body]);

    receivedScriptMessage = false;
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"session:storage", [lastScriptMessage body]);

    [configuration.get().websiteDataStore _terminateNetworkProcess];

    receivedScriptMessage = false;
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"Network Process Crashed", [lastScriptMessage body]);

    readyToContinue = false;
    [webView evaluateJavaScript:@"window.localStorage.getItem('local')" completionHandler:^(id result, NSError *) {
        EXPECT_TRUE([@"storage" isEqualToString:result]);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    // If network process crashes, sessionStorage would be lost.
    readyToContinue = false;
    [webView evaluateJavaScript:@"window.sessionStorage.getItem('session')" completionHandler:^(id result, NSError *) {
        EXPECT_TRUE([result isEqual:NSNull.null]);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
}

TEST(WKWebView, LocalStorageProcessSuspends)
{
    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    RetainPtr<LocalStorageMessageHandler> handler = adoptNS([[LocalStorageMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    RetainPtr<WKProcessPool> processPool = adoptNS([[WKProcessPool alloc] init]);
    [configuration setProcessPool:processPool.get()];
    [configuration _setAllowUniversalAccessFromFileURLs:YES];

    RetainPtr<WKWebView> webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"local-storage-process-suspends-1" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView1 loadRequest:request];

    receivedScriptMessage = false;
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"value", [lastScriptMessage body]);
    
    RetainPtr<WKWebView> webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"local-storage-process-suspends-2" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView2 loadRequest:request];

    receivedScriptMessage = false;
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"value", [lastScriptMessage body]);

    [configuration.get().websiteDataStore _sendNetworkProcessWillSuspendImminently];

    readyToContinue = false;
    [webView1 evaluateJavaScript:@"window.localStorage.setItem('key', 'newValue')" completionHandler:^(id, NSError *) {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
    
    readyToContinue = false;
    [webView2 evaluateJavaScript:@"window.localStorage.getItem('key')" completionHandler:^(id result, NSError *) {
        EXPECT_WK_STREQ(@"value", result);
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);
    
    [configuration.get().websiteDataStore _sendNetworkProcessDidResume];

    receivedScriptMessage = false;
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"newValue", [lastScriptMessage body]);
}

TEST(WKWebView, LocalStorageEmptyString)
{
    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    @autoreleasepool {
        RetainPtr<LocalStorageMessageHandler> handler = adoptNS([[LocalStorageMessageHandler alloc] init]);
        RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
        [configuration _setAllowUniversalAccessFromFileURLs:YES];
    
        RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    
        NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"localstorage-empty-string-value" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
        [webView loadRequest:request];
    
        TestWebKitAPI::Util::run(&receivedScriptMessage);
        receivedScriptMessage = false;
        RetainPtr<NSString> string1 = (NSString *)[lastScriptMessage body];
        EXPECT_WK_STREQ(@"setItem EmptyString", string1.get());
    
        // Ditch this web view (ditch the in-memory local storage).
        webView = nil;
        configuration = nil;
        handler = nil;
        request = nil;
    }
    
    RetainPtr<LocalStorageMessageHandler> handler = adoptNS([[LocalStorageMessageHandler alloc] init]);
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    [configuration _setAllowUniversalAccessFromFileURLs:YES];
    
    // Make a new web view to finish the test.
    RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"localstorage-empty-string-value" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    RetainPtr<NSString> string2 = (NSString *)[lastScriptMessage body];
    EXPECT_WK_STREQ(@"", string2.get());
}

TEST(WKWebView, LocalStorageOpenWindowPrivate)
{
    auto handler = adoptNS([[LocalStorageMessageHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    [configuration _setAllowUniversalAccessFromFileURLs:YES];
    [configuration setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
    auto delegate = adoptNS([[LocalStorageNavigationDelegate alloc] init]);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];
    [webView configuration].preferences.javaScriptCanOpenWindowsAutomatically = YES;

    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"localstorage-open-window-private" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    
    receivedScriptMessage = false;
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"local:storage", [lastScriptMessage body]);
}

TEST(WKWebView, PrivateBrowsingAffectsLocalStorage)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAllowUniversalAccessFromFileURLs:YES];
    [configuration setWebsiteDataStore:[WKWebsiteDataStore defaultDataStore]];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    
    auto delegate = adoptNS([[LocalStorageNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    
    TestWebKitAPI::Util::run(&didFinishNavigation);
    didFinishNavigation = false;
    
    bool finishedRunningScript = false;
    [webView evaluateJavaScript:@"localStorage.setItem('testItem', 'Persistent item!');" completionHandler: [&] (id result, NSError *error) {
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);
    finishedRunningScript = false;
    
    [webView evaluateJavaScript:@"localStorage.getItem('testItem');" completionHandler: [&] (id result, NSError *error) {
        NSString *value = (NSString *)result;
        EXPECT_WK_STREQ(@"Persistent item!", value);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);
    finishedRunningScript = false;
    
    delegate->decidePolicyForNavigationActionHandler = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        // Switch to an ephemeral session.
        preferences._websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    
    [webView reload];
    TestWebKitAPI::Util::run(&didFinishNavigation);
    didFinishNavigation = false;
    
    [webView evaluateJavaScript:@"localStorage.getItem('testItem');" completionHandler: [&] (id result, NSError *error) {
        EXPECT_TRUE([result isEqual:NSNull.null]);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);
    finishedRunningScript = false;
    
    [webView evaluateJavaScript:@"localStorage.setItem('testItem', 'FirstValue');" completionHandler: [&] (id result, NSError *error) {
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);
    finishedRunningScript = false;
    
    [webView evaluateJavaScript:@"localStorage.getItem('testItem');" completionHandler: [&] (id result, NSError *error) {
        NSString *value = (NSString *)result;
        EXPECT_WK_STREQ(@"FirstValue", value);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);
    finishedRunningScript = false;
    
    [webView evaluateJavaScript:@"localStorage.setItem('testItem', 'ChangedValue');" completionHandler: [&] (id result, NSError *error) {
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);
    finishedRunningScript = false;
    
    [webView evaluateJavaScript:@"localStorage.getItem('testItem');" completionHandler: [&] (id result, NSError *error) {
        NSString *value = (NSString *)result;
        EXPECT_WK_STREQ(@"ChangedValue", value);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);
    finishedRunningScript = false;
    
    delegate->decidePolicyForNavigationActionHandler = ^(WKNavigationAction *navigationAction, WKWebpagePreferences *preferences, void (^decisionHandler)(WKNavigationActionPolicy, WKWebpagePreferences *)) {
        // Switch back to the default session.
        preferences._websiteDataStore = [WKWebsiteDataStore defaultDataStore];
        decisionHandler(WKNavigationActionPolicyAllow, preferences);
    };
    
    [webView reload];
    TestWebKitAPI::Util::run(&didFinishNavigation);
    didFinishNavigation = false;
    
    [webView evaluateJavaScript:@"localStorage.getItem('testItem');" completionHandler: [&] (id result, NSError *error) {
        NSString *value = (NSString *)result;
        EXPECT_WK_STREQ(@"Persistent item!", value);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);
    finishedRunningScript = false;
}

TEST(WKWebView, AuxiliaryWindowsShareLocalStorage)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAllowUniversalAccessFromFileURLs:YES];
    [configuration setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    
    auto delegate = adoptNS([[LocalStorageNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];
    
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    
    TestWebKitAPI::Util::run(&didFinishNavigation);
    didFinishNavigation = false;
    
    bool finishedRunningScript = false;
    [webView evaluateJavaScript:@"localStorage.setItem('testItem', 'Persistent item!');" completionHandler: [&] (id result, NSError *error) {
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);
    finishedRunningScript = false;
    
    createdWebView = nullptr;
    [webView evaluateJavaScript:@"open(window.location)" completionHandler: [&] (id result, NSError *error) {
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);
    finishedRunningScript = false;
        
    TestWebKitAPI::Util::run(&didFinishNavigation);
    didFinishNavigation = false;
    
    EXPECT_TRUE(!!createdWebView);
    
    [createdWebView evaluateJavaScript:@"localStorage.getItem('testItem');" completionHandler: [&] (id result, NSError *error) {
        NSString *value = (NSString *)result;
        EXPECT_WK_STREQ(@"Persistent item!", value);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);
    finishedRunningScript = false;
    
    [createdWebView evaluateJavaScript:@"localStorage.setItem('testItem', 'ChangedValue');" completionHandler: [&] (id result, NSError *error) {
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);
    finishedRunningScript = false;
    
    [createdWebView evaluateJavaScript:@"localStorage.getItem('testItem');" completionHandler: [&] (id result, NSError *error) {
        NSString *value = (NSString *)result;
        EXPECT_WK_STREQ(@"ChangedValue", value);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);
    finishedRunningScript = false;
    
    [webView evaluateJavaScript:@"localStorage.getItem('testItem');" completionHandler: [&] (id result, NSError *error) {
        NSString *value = (NSString *)result;
        EXPECT_WK_STREQ(@"ChangedValue", value);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);
    finishedRunningScript = false;
}
