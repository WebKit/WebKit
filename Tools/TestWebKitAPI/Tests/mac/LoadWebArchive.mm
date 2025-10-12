/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "DragAndDropSimulator.h"
#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestProtocol.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKDragDestinationAction.h>
#import <WebKit/WKNavigationPrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

static bool navigationComplete = false;
static bool navigationFail = false;
static String finalURL;
static id<WKNavigationDelegate> gDelegate;
static RetainPtr<WKWebView> newWebView;

@interface TestLoadWebArchiveNavigationDelegate : NSObject <WKNavigationDelegate, WKUIDelegate>
@end

@implementation TestLoadWebArchiveNavigationDelegate

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    navigationComplete = true;
    finalURL = navigation._request.URL.lastPathComponent;
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    navigationFail = true;
    finalURL = navigation._request.URL.lastPathComponent;
}

- (WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures
{
    newWebView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
    [newWebView setNavigationDelegate:gDelegate];

    return newWebView.get();
}

@end

namespace TestWebKitAPI {

TEST(LoadWebArchive, FailNavigation1)
{
    // Using `document.location.href = 'helloworld.webarchive';`.
    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"load-web-archive-1" withExtension:@"html"];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"LoadWebArchiveWithEphemeralStorageEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestLoadWebArchiveNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    navigationFail = false;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&navigationFail);

    EXPECT_WK_STREQ(finalURL, "helloworld.webarchive");
}

TEST(LoadWebArchive, FailNavigation2)
{
    // Using `window.open('helloworld.webarchive');`.
    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"load-web-archive-2" withExtension:@"html"];

    auto delegate = adoptNS([[TestLoadWebArchiveNavigationDelegate alloc] init]);
    gDelegate = delegate.get();

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"LoadWebArchiveWithEphemeralStorageEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    navigationFail = false;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&navigationFail);

    EXPECT_WK_STREQ(finalURL, "helloworld.webarchive");
}

TEST(LoadWebArchive, ClientNavigationSucceed)
{
    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"helloworld" withExtension:@"webarchive"];

    auto webView = adoptNS([[WKWebView alloc] init]);
    auto delegate = adoptNS([[TestLoadWebArchiveNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    navigationComplete = false;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&navigationComplete);

    EXPECT_WK_STREQ(finalURL, "helloworld.webarchive");
}

TEST(LoadWebArchive, ClientNavigationReload)
{
    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"helloworld" withExtension:@"webarchive"];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"LoadWebArchiveWithEphemeralStorageEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);

    auto delegate = adoptNS([[TestLoadWebArchiveNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    navigationComplete = false;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&navigationComplete);
    EXPECT_WK_STREQ(finalURL, "helloworld.webarchive");

    navigationComplete = false;
    [webView reload];
    Util::run(&navigationComplete);
    EXPECT_WK_STREQ(finalURL, "");
}

TEST(LoadWebArchive, ClientNavigationWithStorageReload)
{
    RetainPtr<NSURL> testURL = [NSBundle.test_resourcesBundle URLForResource:@"helloworld" withExtension:@"webarchive"];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"LoadWebArchiveWithEphemeralStorageEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    auto delegate = adoptNS([[TestLoadWebArchiveNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    navigationComplete = false;
    [webView loadRequest:[NSURLRequest requestWithURL:testURL.get()]];
    Util::run(&navigationComplete);
    EXPECT_WK_STREQ(finalURL, "helloworld.webarchive");

    __block bool doneEvaluatingJavaScript { false };
    [webView evaluateJavaScript:@"localStorage.setItem(\"key\", \"value\");" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);

    [webView evaluateJavaScript:@"localStorage.getItem(\"key\");" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"value", value);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);

    navigationComplete = false;
    [webView reload];
    Util::run(&navigationComplete);
    EXPECT_WK_STREQ(finalURL, "");

    [webView evaluateJavaScript:@"localStorage.getItem(\"key\");" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"value", value);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
}

TEST(LoadWebArchive, DragNavigationSucceed)
{
    RetainPtr<NSURL> webArchiveURL = [NSBundle.test_resourcesBundle URLForResource:@"helloworld" withExtension:@"webarchive"];
    RetainPtr<NSURL> simpleURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard declareTypes:@[NSFilenamesPboardType] owner:nil];
    [pasteboard setPropertyList:@[webArchiveURL.get().path] forType:NSFilenamesPboardType];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"LoadWebArchiveWithEphemeralStorageEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    [simulator setExternalDragPasteboard:pasteboard];
    [simulator setDragDestinationAction:WKDragDestinationActionAny];

    auto webView = [simulator webView];
    auto delegate = adoptNS([[TestLoadWebArchiveNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    navigationComplete = false;
    [webView loadRequest:[NSURLRequest requestWithURL:simpleURL.get()]];
    Util::run(&navigationComplete);
    EXPECT_WK_STREQ(finalURL, "simple.html");

    navigationComplete = false;
    [simulator runFrom:CGPointMake(0, 0) to:CGPointMake(50, 50)];
    Util::run(&navigationComplete);
    EXPECT_WK_STREQ(finalURL, "helloworld.webarchive");
}

TEST(LoadWebArchive, DragNavigationReload)
{
    RetainPtr<NSURL> webArchiveURL = [NSBundle.test_resourcesBundle URLForResource:@"helloworld" withExtension:@"webarchive"];
    RetainPtr<NSURL> simpleURL = [NSBundle.test_resourcesBundle URLForResource:@"simple" withExtension:@"html"];

    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard declareTypes:@[NSFilenamesPboardType] owner:nil];
    [pasteboard setPropertyList:@[webArchiveURL.get().path] forType:NSFilenamesPboardType];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"LoadWebArchiveWithEphemeralStorageEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    [simulator setExternalDragPasteboard:pasteboard];
    [simulator setDragDestinationAction:WKDragDestinationActionAny];

    auto webView = [simulator webView];
    auto delegate = adoptNS([[TestLoadWebArchiveNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    navigationComplete = false;
    [webView loadRequest:[NSURLRequest requestWithURL:simpleURL.get()]];
    Util::run(&navigationComplete);
    EXPECT_WK_STREQ(finalURL, "simple.html");

    navigationComplete = false;
    [simulator runFrom:CGPointMake(0, 0) to:CGPointMake(50, 50)];
    Util::run(&navigationComplete);
    EXPECT_WK_STREQ(finalURL, "helloworld.webarchive");

    navigationComplete = false;
    [webView reload];
    Util::run(&navigationComplete);
    EXPECT_WK_STREQ(finalURL, "");
}

static NSData* constructArchive()
{
    NSString *js = @"alert('loaded http subresource successfully')";
    auto response = adoptNS([[NSURLResponse alloc] initWithURL:[NSURL URLWithString:@"http://download.example/script.js"] MIMEType:@"application/javascript" expectedContentLength:js.length textEncodingName:@"utf-8"]);
    auto responseArchiver = adoptNS([[NSKeyedArchiver alloc] initRequiringSecureCoding:YES]);
    [responseArchiver encodeObject:response.get() forKey:@"WebResourceResponse"];
    NSDictionary *archive = @{
        @"WebMainResource": @{
            @"WebResourceData": [@"<script src='script.js'></script>" dataUsingEncoding:NSUTF8StringEncoding],
            @"WebResourceFrameName": @"",
            @"WebResourceMIMEType": @"text/html",
            @"WebResourceTextEncodingName": @"UTF-8",
            @"WebResourceURL": @"http://download.example/",
        },
        @"WebSubresources": @[@{
            @"WebResourceData": [js dataUsingEncoding:NSUTF8StringEncoding],
            @"WebResourceMIMEType": @"application/javascript",
            @"WebResourceResponse": responseArchiver.get().encodedData,
            @"WebResourceTextEncodingName": @"utf-8",
            @"WebResourceURL": @"http://download.example/script.js",
        }]
    };
    return [NSPropertyListSerialization dataFromPropertyList:archive format:NSPropertyListBinaryFormat_v1_0 errorDescription:nil];
}

TEST(LoadWebArchive, HTTPSUpgrade)
{
    NSData *data = constructArchive();

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"LoadWebArchiveWithEphemeralStorageEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadData:data MIMEType:@"application/x-webarchive" characterEncodingName:@"utf-8" baseURL:[NSURL URLWithString:@"http://download.example/"]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded http subresource successfully");
}

TEST(LoadWebArchive, DisallowedNetworkHosts)
{
    NSData *data = constructArchive();

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get()._allowedNetworkHosts = [NSSet set];

    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"LoadWebArchiveWithEphemeralStorageEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView loadData:data MIMEType:@"application/x-webarchive" characterEncodingName:@"utf-8" baseURL:[NSURL URLWithString:@"http://download.example/"]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "loaded http subresource successfully");
}

TEST(LoadWebArchive, SiteToWebArchiveAndBack)
{
    using namespace TestWebKitAPI;
    HTTPServer server({
        { "http://download.example/"_s, { "<body>/</body>"_s } },
        { "http://download.example/page1.html"_s, { "<body>page1</body>"_s } },
    }, HTTPServer::Protocol::Http);

    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPProxyPort: @(server.port())
    }];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:dataStore.get()];

    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"LoadWebArchiveWithEphemeralStorageEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);

    auto delegate = adoptNS([[TestLoadWebArchiveNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    navigationComplete = false;

    RetainPtr<NSURL> webArchiveURL = [NSBundle.test_resourcesBundle URLForResource:@"download.example" withExtension:@"webarchive"];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://download.example/page1.html"]]];
    Util::run(&navigationComplete);
    EXPECT_WK_STREQ([webView URL].absoluteString, @"http://download.example/page1.html");
    navigationComplete = false;

    __block bool doneEvaluatingJavaScript { false };
    [webView evaluateJavaScript:@"localStorage.setItem(\"key\", \"value\");" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    doneEvaluatingJavaScript = false;

    [webView evaluateJavaScript:@"localStorage.getItem(\"key\");" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"value", value);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    doneEvaluatingJavaScript = false;

    [webView loadFileURL:webArchiveURL.get() allowingReadAccessToURL:webArchiveURL.get()];
    Util::run(&navigationComplete);
    EXPECT_WK_STREQ(finalURL, @"download.example.webarchive");
    navigationComplete = false;

    [webView evaluateJavaScript:@"location.href" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"http://download.example/", value);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    doneEvaluatingJavaScript = false;

    EXPECT_TRUE([webView canGoBack]);

    [webView evaluateJavaScript:@"localStorage.getItem(\"key\");" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSNull class]]);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    doneEvaluatingJavaScript = false;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://download.example/page1.html"]]];
    Util::run(&navigationComplete);
    EXPECT_WK_STREQ([webView URL].absoluteString, @"http://download.example/page1.html");
    navigationComplete = false;

    [webView evaluateJavaScript:@"localStorage.getItem(\"key\");" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_FALSE([value isKindOfClass:[NSNull class]]);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"value", value);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    doneEvaluatingJavaScript = false;
}

TEST(LoadWebArchive, WebArchiveToSiteAndBack)
{
    using namespace TestWebKitAPI;
    HTTPServer server({
        { "http://download.example/"_s, { "<body>/</body>"_s } },
        { "http://download.example/page1.html"_s, { "<body>page1</body>"_s } },
    }, HTTPServer::Protocol::Http);

    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPProxyPort: @(server.port())
    }];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:dataStore.get()];

    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"LoadWebArchiveWithEphemeralStorageEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);

    auto delegate = adoptNS([[TestLoadWebArchiveNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    navigationComplete = false;

    RetainPtr<NSURL> webArchiveURL = [NSBundle.test_resourcesBundle URLForResource:@"download.example" withExtension:@"webarchive"];
    [webView loadFileURL:webArchiveURL.get() allowingReadAccessToURL:webArchiveURL.get()];
    Util::run(&navigationComplete);
    EXPECT_WK_STREQ(finalURL, @"download.example.webarchive");
    navigationComplete = false;

    __block bool doneEvaluatingJavaScript { false };
    [webView evaluateJavaScript:@"location.href" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"http://download.example/", value);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    doneEvaluatingJavaScript = false;

    [webView evaluateJavaScript:@"localStorage.setItem(\"key\", \"value\");" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    doneEvaluatingJavaScript = false;

    [webView evaluateJavaScript:@"localStorage.getItem(\"key\");" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"value", value);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    doneEvaluatingJavaScript = false;

    [webView evaluateJavaScript:@"let anchor = document.createElement('a'); anchor.href = \"http://download.example/page1.html\"; anchor.click();" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    doneEvaluatingJavaScript = false;
    Util::run(&navigationComplete);
    EXPECT_WK_STREQ([webView URL].absoluteString, @"http://download.example/page1.html");
    navigationComplete = false;

    [webView evaluateJavaScript:@"localStorage.getItem(\"key\");" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSNull class]]);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    doneEvaluatingJavaScript = false;

    WKBackForwardList *list = [webView backForwardList];
    EXPECT_WK_STREQ(@"http://download.example/page1.html", [list.currentItem.URL absoluteString]);
    EXPECT_EQ((NSUInteger)1, list.backList.count);
    EXPECT_EQ((NSUInteger)0, list.forwardList.count);
    EXPECT_TRUE([webView canGoBack]);

    [webView goBack];
    Util::run(&navigationComplete);
    EXPECT_WK_STREQ(finalURL, @"download.example.webarchive");
    navigationComplete = false;

    [webView evaluateJavaScript:@"location.href" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"http://download.example/", value);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    doneEvaluatingJavaScript = false;

    [webView evaluateJavaScript:@"localStorage.getItem(\"key\");" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_FALSE([value isKindOfClass:[NSNull class]]);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"value", value);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    doneEvaluatingJavaScript = false;
}

TEST(LoadWebArchive, FileWebArchiveToDataWebArchiveAndBack)
{
    auto configuration = adoptNS([WKWebViewConfiguration new]);
    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"LoadWebArchiveWithEphemeralStorageEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);

    auto delegate = adoptNS([[TestLoadWebArchiveNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:delegate.get()];

    navigationComplete = false;

    RetainPtr<NSURL> webArchiveURL = [NSBundle.test_resourcesBundle URLForResource:@"download.example" withExtension:@"webarchive"];
    [webView loadFileURL:webArchiveURL.get() allowingReadAccessToURL:webArchiveURL.get()];
    Util::run(&navigationComplete);
    EXPECT_WK_STREQ(finalURL, @"download.example.webarchive");
    navigationComplete = false;

    __block bool doneEvaluatingJavaScript { false };
    [webView evaluateJavaScript:@"location.href" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"http://download.example/", value);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    doneEvaluatingJavaScript = false;

    [webView evaluateJavaScript:@"localStorage.setItem(\"key\", \"value\");" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    doneEvaluatingJavaScript = false;

    [webView evaluateJavaScript:@"localStorage.getItem(\"key\");" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"value", value);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    doneEvaluatingJavaScript = false;

    NSData *data = constructArchive();
    [webView loadData:data MIMEType:@"application/x-webarchive" characterEncodingName:@"utf-8" baseURL:[NSURL URLWithString:@"http://download.example/"]];
    Util::run(&navigationComplete);
    EXPECT_WK_STREQ([webView URL].absoluteString, @"http://download.example/");
    navigationComplete = false;

    [webView evaluateJavaScript:@"localStorage.getItem(\"key\");" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSNull class]]);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    doneEvaluatingJavaScript = false;

    [webView loadFileURL:webArchiveURL.get() allowingReadAccessToURL:webArchiveURL.get()];
    Util::run(&navigationComplete);
    EXPECT_WK_STREQ(finalURL, @"download.example.webarchive");
    navigationComplete = false;

    [webView evaluateJavaScript:@"location.href" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSString class]]);
        EXPECT_WK_STREQ(@"http://download.example/", value);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    doneEvaluatingJavaScript = false;

    [webView evaluateJavaScript:@"localStorage.getItem(\"key\");" completionHandler:^(id value, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([value isKindOfClass:[NSNull class]]);
        doneEvaluatingJavaScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingJavaScript);
    doneEvaluatingJavaScript = false;
}

TEST(LoadWebArchive, PreferCachedImageSourceOverBrokenImage)
{
    [TestProtocol registerWithScheme:@"http"];

    RetainPtr archiveData = [] {
        RetainPtr sourceView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600)]);
        [sourceView _setOverrideDeviceScaleFactor:1];
        [sourceView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
            "<html>"
            "  <meta name='viewport' content='width=device-width, initial-scale=1'>"
            "  <body>"
            "    <picture>"
            "      <img src='http://bundle-file/400x400-green.png'"
            "        srcset='http://bundle-file/400x400-green.png 1x,http://bundle-file/large-red-square.png 2x' />"
            "    </picture>"
            "  </body>"
            "</html>"];
        RetainPtr result = [sourceView contentsAsWebArchive];
        [sourceView _killWebContentProcessAndResetState];
        return result;
    }();

    [TestProtocol unregister];

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:WKWebsiteDataStore.nonPersistentDataStore];
    [configuration _setAllowedNetworkHosts:NSSet.set];

    RetainPtr delegate = adoptNS([TestNavigationDelegate new]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:delegate.get()];
    [webView _setOverrideDeviceScaleFactor:2];
    [webView loadData:archiveData.get() MIMEType:@"application/x-webarchive" characterEncodingName:@"" baseURL:[NSURL URLWithString:@""]];
    [delegate waitForDidFinishNavigation];

    NSArray<NSNumber *> *dimensions = [webView objectByEvaluatingJavaScript:@"const image = document.images[0]; [image.naturalWidth, image.naturalHeight];"];
    auto naturalWidth = dimensions.firstObject.intValue;
    auto naturalHeight = dimensions.lastObject.intValue;
    EXPECT_EQ(naturalWidth, 400);
    EXPECT_EQ(naturalHeight, 400);
}

TEST(LoadWebArchive, FailNavigationFromNonClientOrUserInitiatedWindowOpen)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration preferences] setJavaScriptCanOpenWindowsAutomatically:YES];

    for (_WKFeature *feature in WKPreferences._features) {
        NSString *key = feature.key;
        if ([key isEqualToString:@"LoadWebArchiveWithEphemeralStorageEnabled"])
            [[configuration preferences] _setEnabled:YES forFeature:feature];
    }

    RetainPtr navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);

    __block bool failed = false;
    __block bool finished = false;

    navigationDelegate.get().didFailProvisionalNavigation = ^(WKWebView *, WKNavigation *, NSError *error) {
        failed = true;
        finished = true;
    };

    navigationDelegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *) {
        finished = true;
    };

    RetainPtr uiDelegate = adoptNS([[TestUIDelegate alloc] init]);

    __block RetainPtr<WKWebView> openedWebView;
    uiDelegate.get().createWebViewWithConfiguration = ^(WKWebViewConfiguration *configuration, WKNavigationAction *action, WKWindowFeatures *windowFeatures) {
        openedWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);
        openedWebView.get().navigationDelegate = navigationDelegate.get();
        openedWebView.get().UIDelegate = uiDelegate.get();
        return openedWebView.get();
    };

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView setUIDelegate:uiDelegate.get()];

    [webView evaluateJavaScript:@"window.open('file:///usr/local/lib/WebKitAdditions.resources/iframe.webarchive', '_blank', 'noopener');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];

    Util::run(&finished);

    EXPECT_TRUE(failed);
}

} // namespace TestWebKitAPI
