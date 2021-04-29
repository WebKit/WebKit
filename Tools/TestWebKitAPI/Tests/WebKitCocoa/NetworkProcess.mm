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

#import "HTTPServer.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

TEST(NetworkProcess, Entitlements)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:adoptNS([[WKWebViewConfiguration alloc] init]).get()]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    WKWebsiteDataStore *store = [webView configuration].websiteDataStore;
    bool hasEntitlement = [store _networkProcessHasEntitlementForTesting:@"com.apple.rootless.storage.WebKitNetworkingSandbox"];
#if PLATFORM(MAC) && USE(APPLE_INTERNAL_SDK)
    EXPECT_TRUE(hasEntitlement);
#else
    EXPECT_FALSE(hasEntitlement);
#endif
    EXPECT_FALSE([store _networkProcessHasEntitlementForTesting:@"test failure case"]);
}

TEST(WebKit, HTTPReferer)
{
    auto checkReferer = [] (NSURL *baseURL, const char* expectedReferer) {
        using namespace TestWebKitAPI;
        bool done = false;
        HTTPServer server([&] (Connection connection) {
            connection.receiveHTTPRequest([connection, expectedReferer, &done] (Vector<char>&& request) {
                if (expectedReferer) {
                    auto expectedHeaderField = makeString("Referer: ", expectedReferer, "\r\n");
                    EXPECT_TRUE(strnstr(request.data(), expectedHeaderField.utf8().data(), request.size()));
                } else
                    EXPECT_FALSE(strnstr(request.data(), "Referer:", request.size()));
                done = true;
            });
        });
        auto webView = adoptNS([WKWebView new]);
        [webView loadHTMLString:[NSString stringWithFormat:@"<body onload='document.getElementById(\"formID\").submit()'><form id='formID' method='post' action='http://127.0.0.1:%d/'></form></body>", server.port()] baseURL:baseURL];
        Util::run(&done);
    };
    
    Vector<char> a5k(5000, 'a');
    Vector<char> a3k(3000, 'a');
    NSString *longPath = [NSString stringWithFormat:@"http://webkit.org/%s?asdf", a5k.data()];
    NSString *shorterPath = [NSString stringWithFormat:@"http://webkit.org/%s?asdf", a3k.data()];
    NSString *longHost = [NSString stringWithFormat:@"http://webkit.org%s/path", a5k.data()];
    NSString *shorterHost = [NSString stringWithFormat:@"http://webkit.org%s/path", a3k.data()];
    checkReferer([NSURL URLWithString:longPath], "http://webkit.org");
    checkReferer([NSURL URLWithString:shorterPath], shorterPath.UTF8String);
    checkReferer([NSURL URLWithString:longHost], nullptr);
    checkReferer([NSURL URLWithString:shorterHost], shorterHost.UTF8String);
}

TEST(NetworkProcess, LaunchOnlyWhenNecessary)
{
    auto webView = adoptNS([WKWebView new]);
    [webView configuration].websiteDataStore._resourceLoadStatisticsEnabled = YES;
    [[webView configuration].processPool _registerURLSchemeAsSecure:@"test"];
    [[webView configuration].processPool _registerURLSchemeAsBypassingContentSecurityPolicy:@"test"];
    EXPECT_FALSE([[webView configuration].websiteDataStore _networkProcessExists]);
}

TEST(NetworkProcess, CrashWhenNotAssociatedWithDataStore)
{
    pid_t networkProcessPID = 0;
    @autoreleasepool {
        auto viewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
        auto dataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
        auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);
        [viewConfiguration setWebsiteDataStore:dataStore.get()];
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:viewConfiguration.get()]);
        [webView loadHTMLString:@"foo" baseURL:[NSURL URLWithString:@"about:blank"]];
        while (![dataStore _networkProcessIdentifier])
            TestWebKitAPI::Util::spinRunLoop(10);
        networkProcessPID = [dataStore _networkProcessIdentifier];
        EXPECT_TRUE(!!networkProcessPID);
    }
    TestWebKitAPI::Util::spinRunLoop(10);

    // Kill the network process once it is no longer associated with any WebsiteDataStore.
    kill(networkProcessPID, 9);
    TestWebKitAPI::Util::spinRunLoop(10);

    auto viewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto dataStoreConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:dataStoreConfiguration.get()]);
    [viewConfiguration setWebsiteDataStore:dataStore.get()];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:viewConfiguration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    EXPECT_NE(networkProcessPID, [webView configuration].websiteDataStore._networkProcessIdentifier);
}

TEST(NetworkProcess, TerminateWhenUnused)
{
    RetainPtr<WKProcessPool> retainedPool;
    @autoreleasepool {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        configuration.get().websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
        retainedPool = configuration.get().processPool;
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 0, 0) configuration:configuration.get()]);
        [webView synchronouslyLoadTestPageNamed:@"simple"];
        EXPECT_TRUE([WKWebsiteDataStore _defaultNetworkProcessExists]);
    }
    while ([WKWebsiteDataStore _defaultNetworkProcessExists])
        TestWebKitAPI::Util::spinRunLoop();
    
    retainedPool = nil;
    
    @autoreleasepool {
        auto webView = adoptNS([WKWebView new]);
        [webView synchronouslyLoadTestPageNamed:@"simple"];
        EXPECT_TRUE([WKWebsiteDataStore _defaultNetworkProcessExists]);
    }
    while ([WKWebsiteDataStore _defaultNetworkProcessExists])
        TestWebKitAPI::Util::spinRunLoop();
}
