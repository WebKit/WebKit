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

#import "DeprecatedGlobalValues.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WKWebsiteDataStoreRef.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <WebKit/_WKUserStyleSheet.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

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
    didFinishNavigationBoolean = true;
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

    readyToContinue = false;
    WKWebsiteDataStoreSyncLocalStorage((WKWebsiteDataStoreRef)configuration.get().websiteDataStore, nullptr, [](void*) {
        readyToContinue = true;
    });
    TestWebKitAPI::Util::run(&readyToContinue);

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
    
    TestWebKitAPI::Util::run(&didFinishNavigationBoolean);
    didFinishNavigationBoolean = false;
    
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
    TestWebKitAPI::Util::run(&didFinishNavigationBoolean);
    didFinishNavigationBoolean = false;
    
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
    TestWebKitAPI::Util::run(&didFinishNavigationBoolean);
    didFinishNavigationBoolean = false;
    
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
    
    TestWebKitAPI::Util::run(&didFinishNavigationBoolean);
    didFinishNavigationBoolean = false;
    
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
        
    TestWebKitAPI::Util::run(&didFinishNavigationBoolean);
    didFinishNavigationBoolean = false;
    
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

TEST(WKWebView, LocalStorageGroup)
{
    auto runTest = [] (bool setGroupIdentifier) {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [configuration setWebsiteDataStore:[WKWebsiteDataStore nonPersistentDataStore]];
        if (setGroupIdentifier)
            [configuration _setGroupIdentifier:@"testgroupidentifier"];
        auto webView1 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        NSString *html1 = @""
        "<script>"
        "localStorage.setItem('testkey', 'testvalue1');"
        "window.onstorage = function(e) { alert('storage changed key ' + e.key + ' value from ' + e.oldValue + ' to ' + e.newValue) };"
        "</script>";
        [webView1 loadHTMLString:html1 baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
        [webView1 _test_waitForDidFinishNavigation];

        auto webView2 = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        NSString *html2 = @""
        "<script>"
        "alert('second web view got value ' + localStorage.getItem('testkey'));"
        "localStorage.setItem('testkey', 'testvalue2');"
        "</script>";
        [webView2 loadHTMLString:html2 baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
        EXPECT_WK_STREQ([webView2 _test_waitForAlert], "second web view got value testvalue1");

        EXPECT_WK_STREQ([webView1 _test_waitForAlert], "storage changed key testkey value from testvalue1 to testvalue2");
    };
    runTest(true);
    runTest(false);
}

TEST(WKWebView, LocalStorageNoSizeOverflow)
{
    readyToContinue = false;
    [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:[WKWebsiteDataStore allWebsiteDataTypes] modifiedSince:[NSDate distantPast] completionHandler:^() {
        readyToContinue = true;
    }];
    TestWebKitAPI::Util::run(&readyToContinue);

    NSString *htmlString = @"<script> \
        const key = '\u00FF\u00FF'; \
        function onStorage() { window.webkit.messageHandlers.testHandler.postMessage('storage'); } \
        function setItem() { localStorage.setItem(key, 'value'); } \
        function removeItem() { localStorage.removeItem(localStorage.key(0)); } \
        function getItem() { return localStorage.getItem(key) ? localStorage.getItem(key) : '[null]'; } \
        window.addEventListener('storage', onStorage);\
        window.webkit.messageHandlers.testHandler.postMessage(getItem()); \
        </script>";
    auto handler = adoptNS([[LocalStorageMessageHandler alloc] init]);
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"[null]", [lastScriptMessage body]);
    receivedScriptMessage = false;

    [webView evaluateJavaScript:@"setItem()" completionHandler:^(NSNumber *result, NSError *error) {
        receivedScriptMessage = true;
    }];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    receivedScriptMessage = false;
    
    auto secondWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [secondWebView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"http://webkit.org"]];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"value", [lastScriptMessage body]);
    receivedScriptMessage = false;

    [secondWebView evaluateJavaScript:@"removeItem()" completionHandler:nil];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"storage", [lastScriptMessage body]);
    receivedScriptMessage = false;

    [secondWebView evaluateJavaScript:@"setItem()" completionHandler:nil];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
    EXPECT_WK_STREQ(@"storage", [lastScriptMessage body]);
    receivedScriptMessage = false;

    [webView evaluateJavaScript:@"getItem()" completionHandler:^(NSString* result, NSError *error) {
        EXPECT_WK_STREQ("value", [result UTF8String]);
        receivedScriptMessage = true;
    }];
    TestWebKitAPI::Util::run(&receivedScriptMessage);
}

#if PLATFORM(IOS_FAMILY)

TEST(WKWebView, LocalStorageDirectoryExcludedFromBackup)
{
    auto websiteDataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] init]);
    websiteDataStoreConfiguration.get().unifiedOriginStorageLevel = _WKUnifiedOriginStorageLevelNone;
    RetainPtr<NSURL> webStorageDirectory = [websiteDataStoreConfiguration _webStorageDirectory];
    auto websiteDataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:websiteDataStoreConfiguration.get()]);
    
    // Create WebStorage directory and make it not excluded.
    [[NSFileManager defaultManager] createDirectoryAtURL:webStorageDirectory.get() withIntermediateDirectories:YES attributes:nil error:nullptr];
    [webStorageDirectory.get() setResourceValue:@NO forKey:NSURLIsExcludedFromBackupKey error:nil];
    NSNumber *isDirectoryExcluded = nil;
    EXPECT_TRUE([webStorageDirectory.get() getResourceValue:&isDirectoryExcluded forKey:NSURLIsExcludedFromBackupKey error:nil]);
    EXPECT_FALSE(isDirectoryExcluded.boolValue);

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:websiteDataStore.get()];
    auto delegate = adoptNS([[LocalStorageNavigationDelegate alloc] init]);
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:delegate.get()];
    didFinishNavigationBoolean = false;
    NSURLRequest *request = [NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"simple" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]];
    [webView loadRequest:request];
    TestWebKitAPI::Util::run(&didFinishNavigationBoolean);

    done = false;
    [webView evaluateJavaScript:@"localStorage.getItem('key');" completionHandler: [&] (id result, NSError *) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    // Create new url that has updated value.
    webStorageDirectory = [websiteDataStoreConfiguration _webStorageDirectory];
    EXPECT_TRUE([webStorageDirectory.get() getResourceValue:&isDirectoryExcluded forKey:NSURLIsExcludedFromBackupKey error:nil]);
    EXPECT_TRUE(isDirectoryExcluded.boolValue);
}

#endif
