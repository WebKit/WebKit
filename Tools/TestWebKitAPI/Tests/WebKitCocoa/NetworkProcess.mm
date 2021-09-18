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
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKScriptMessageHandler.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/UUID.h>
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
        [webView loadHTMLString:[NSString stringWithFormat:@"<meta name='referrer' content='unsafe-url'><body onload='document.getElementById(\"formID\").submit()'><form id='formID' method='post' action='http://127.0.0.1:%d/'></form></body>", server.port()] baseURL:baseURL];
        Util::run(&done);
    };
    
    Vector<char> a5k(5000, 'a');
    Vector<char> a3k(3000, 'a');
    NSString *longPath = [NSString stringWithFormat:@"http://webkit.org/%s?asdf", a5k.data()];
    NSString *shorterPath = [NSString stringWithFormat:@"http://webkit.org/%s?asdf", a3k.data()];
    NSString *longHost = [NSString stringWithFormat:@"http://webkit.org%s/path", a5k.data()];
    NSString *shorterHost = [NSString stringWithFormat:@"http://webkit.org%s/path", a3k.data()];
    checkReferer([NSURL URLWithString:longPath], "http://webkit.org/");
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

TEST(NetworkProcess, CORSPreflightCachePartitioned)
{
    using namespace TestWebKitAPI;
    size_t preflightRequestsReceived { 0 };
    HTTPServer server([&] (Connection connection) {
        connection.receiveHTTPRequest([&, connection] (Vector<char>&& request) {
            const char* expectedRequestBegin = "OPTIONS / HTTP/1.1\r\n";
            EXPECT_TRUE(!memcmp(request.data(), expectedRequestBegin, strlen(expectedRequestBegin)));
            EXPECT_TRUE(strnstr(request.data(), "Origin: http://example.com\r\n", request.size()));
            EXPECT_TRUE(strnstr(request.data(), "Access-Control-Request-Method: DELETE\r\n", request.size()));
            
            const char* response =
            "HTTP/1.1 204 No Content\r\n"
            "Access-Control-Allow-Origin: http://example.com\r\n"
            "Access-Control-Allow-Methods: OPTIONS, GET, POST, DELETE\r\n"
            "Cache-Control: max-age=604800\r\n\r\n";
            connection.send(response, [&, connection] {
                connection.receiveHTTPRequest([&, connection] (Vector<char>&& request) {
                    const char* expectedRequestBegin = "DELETE / HTTP/1.1\r\n";
                    EXPECT_TRUE(!memcmp(request.data(), expectedRequestBegin, strlen(expectedRequestBegin)));
                    EXPECT_TRUE(strnstr(request.data(), "Origin: http://example.com\r\n", request.size()));
                    const char* response =
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: 2\r\n\r\n"
                    "hi";
                    connection.send(response, [&, connection] {
                        preflightRequestsReceived++;
                    });
                });
            });
        });
    });
    NSString *html = [NSString stringWithFormat:@"<script>var xhr = new XMLHttpRequest();xhr.open('DELETE', 'http://localhost:%d/');xhr.send()</script>", server.port()];
    NSURL *baseURL = [NSURL URLWithString:@"http://example.com/"];
    auto firstWebView = adoptNS([WKWebView new]);
    [firstWebView loadHTMLString:html baseURL:baseURL];
    while (preflightRequestsReceived != 1)
        TestWebKitAPI::Util::spinRunLoop();

    auto configuration = adoptNS([WKWebViewConfiguration new]);
    configuration.get().websiteDataStore = [WKWebsiteDataStore nonPersistentDataStore];
    auto secondWebView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [secondWebView loadHTMLString:html baseURL:baseURL];
    while (preflightRequestsReceived != 2)
        TestWebKitAPI::Util::spinRunLoop();
}


static Vector<RetainPtr<WKScriptMessage>> receivedMessages;
static bool receivedMessage = false;

@interface BroadcastChannelMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation BroadcastChannelMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedMessages.append(message);
    receivedMessage = true;
}
@end

static void waitUntilNetworkProcessIsResponsive(WKWebView *webView1, WKWebView *webView2)
{
    // We've just terminated and relaunched the network process. However, there is no easy well to tell if the webviews'
    // WebProcesses have been notified of the network process crash or not (we only know that the UIProcess was). Because
    // we don't want the test to go on with the WebProcesses using stale NetworkProcessConnections, we use the following
    // trick to wait until both WebProcesses are able to communicate with the new NetworkProcess:
    // The first WebProcess tries setting a cookie until the second Webview is able to see it.
    auto expectedCookieString = makeString("TEST=", createCanonicalUUIDString());
    auto setTestCookieString = makeString("setInterval(() => { document.cookie='", expectedCookieString, "'; }, 100);");
    [webView1 evaluateJavaScript:(NSString *)setTestCookieString completionHandler: [&] (id result, NSError *error) {
        EXPECT_TRUE(!error);
    }];

    bool canSecondWebViewSeeNewCookie = false;
    do {
        TestWebKitAPI::Util::spinRunLoop(10);
        String cookieString = (NSString *)[webView2 objectByEvaluatingJavaScript:@"document.cookie"];
        canSecondWebViewSeeNewCookie = cookieString.contains(expectedCookieString);
    } while (!canSecondWebViewSeeNewCookie);
}

TEST(NetworkProcess, BroadcastChannelCrashRecovery)
{
    auto webViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    auto messageHandler = adoptNS([[BroadcastChannelMessageHandler alloc] init]);
    [[webViewConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"test"];

    auto webView1 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);

    receivedMessage = false;
    receivedMessages.clear();

    NSString *html = [NSString stringWithFormat:@"<script>let bc = new BroadcastChannel('test'); bc.onmessage = (msg) => { webkit.messageHandlers.test.postMessage(msg.data); };</script>"];
    NSURL *baseURL = [NSURL URLWithString:@"http://example.com/"];

    [webView1 synchronouslyLoadHTMLString:html baseURL:baseURL];
    [webView2 synchronouslyLoadHTMLString:html baseURL:baseURL];

    auto webPID1 = [webView1 _webProcessIdentifier];
    auto webPID2 = [webView2 _webProcessIdentifier];
    EXPECT_NE(webPID1, 0);
    EXPECT_NE(webPID2, 0);
    EXPECT_NE(webPID1, webPID2);

    auto networkPID = [[WKWebsiteDataStore defaultDataStore] _networkProcessIdentifier];
    EXPECT_NE(networkPID, 0);

    EXPECT_FALSE(receivedMessage);
    EXPECT_TRUE(receivedMessages.isEmpty());

    // Test that initial communication from webView1 to webView2 works.
    receivedMessage = false;
    receivedMessages.clear();
    bool finishedRunningScript = false;
    [webView1 evaluateJavaScript:@"bc.postMessage('foo')" completionHandler: [&] (id result, NSError *error) {
        EXPECT_TRUE(!error);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);

    TestWebKitAPI::Util::run(&receivedMessage);
    TestWebKitAPI::Util::spinRunLoop(10);

    EXPECT_EQ(receivedMessages.size(), 1U);
    EXPECT_EQ([receivedMessages[0] webView], webView2);
    EXPECT_WK_STREQ([receivedMessages[0] body], @"foo");

    // Test that initial communication from webView2 to webView1 works.
    receivedMessage = false;
    receivedMessages.clear();
    finishedRunningScript = false;
    [webView2 evaluateJavaScript:@"bc.postMessage('bar')" completionHandler: [&] (id result, NSError *error) {
        EXPECT_TRUE(!error);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);

    TestWebKitAPI::Util::run(&receivedMessage);
    TestWebKitAPI::Util::spinRunLoop(10);

    EXPECT_EQ(receivedMessages.size(), 1U);
    EXPECT_EQ([receivedMessages[0] webView], webView1);
    EXPECT_WK_STREQ([receivedMessages[0] body], @"bar");

    // Kill the network process.
    kill(networkPID, 9);
    while ([[WKWebsiteDataStore defaultDataStore] _networkProcessIdentifier] == networkPID)
        TestWebKitAPI::Util::spinRunLoop(10);

    waitUntilNetworkProcessIsResponsive(webView1.get(), webView2.get());

    // Test that initial communication from webView1 to webView2 works.
    receivedMessage = false;
    receivedMessages.clear();
    finishedRunningScript = false;
    [webView1 evaluateJavaScript:@"bc.postMessage('foo2')" completionHandler: [&] (id result, NSError *error) {
        EXPECT_TRUE(!error);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);

    TestWebKitAPI::Util::run(&receivedMessage);
    TestWebKitAPI::Util::spinRunLoop(10);

    EXPECT_EQ(receivedMessages.size(), 1U);
    EXPECT_EQ([receivedMessages[0] webView], webView2);
    EXPECT_WK_STREQ([receivedMessages[0] body], @"foo2");

    // Test that initial communication from webView2 to webView1 works.
    receivedMessage = false;
    receivedMessages.clear();
    finishedRunningScript = false;
    [webView2 evaluateJavaScript:@"bc.postMessage('bar2')" completionHandler: [&] (id result, NSError *error) {
        EXPECT_TRUE(!error);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);

    TestWebKitAPI::Util::run(&receivedMessage);
    TestWebKitAPI::Util::spinRunLoop(10);

    EXPECT_EQ(receivedMessages.size(), 1U);
    EXPECT_EQ([receivedMessages[0] webView], webView1);
    EXPECT_WK_STREQ([receivedMessages[0] body], @"bar2");

    auto networkPID2 = [[WKWebsiteDataStore defaultDataStore] _networkProcessIdentifier];
    EXPECT_NE(networkPID2, 0);
    EXPECT_NE(networkPID, networkPID2);

    EXPECT_EQ(webPID1, [webView1 _webProcessIdentifier]);
    EXPECT_EQ(webPID2, [webView2 _webProcessIdentifier]);
}
