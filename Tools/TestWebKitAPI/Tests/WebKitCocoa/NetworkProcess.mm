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
#import "Test.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKScriptMessageHandler.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKDataTask.h>
#import <WebKit/_WKDataTaskDelegate.h>
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
    RetainPtr<WKWebsiteDataStore> websiteDataStore;

    @autoreleasepool {
        auto webView = adoptNS([WKWebView new]);
        websiteDataStore = [webView configuration].websiteDataStore;
        [websiteDataStore _setResourceLoadStatisticsEnabled:YES];
        [[webView configuration].processPool _registerURLSchemeAsSecure:@"test"];
        [[webView configuration].processPool _registerURLSchemeAsBypassingContentSecurityPolicy:@"test"];
    }

    TestWebKitAPI::Util::spinRunLoop(10);
    EXPECT_FALSE([websiteDataStore _networkProcessExists]);
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

TEST(NetworkProcess, TerminateWhenNoWebsiteDataStore)
{
    pid_t networkProcessIdentifier = 0;
    @autoreleasepool {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        auto nonPersistentStore = [WKWebsiteDataStore nonPersistentDataStore];
        configuration.get().websiteDataStore = nonPersistentStore;
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 0, 0) configuration:configuration.get()]);
        [webView synchronouslyLoadTestPageNamed:@"simple"];
        EXPECT_TRUE([WKWebsiteDataStore _defaultNetworkProcessExists]);

        networkProcessIdentifier = [nonPersistentStore _networkProcessIdentifier];
        EXPECT_NE(networkProcessIdentifier, 0);
    }

    while (!kill(networkProcessIdentifier, 0))
        TestWebKitAPI::Util::spinRunLoop();
    EXPECT_TRUE(errno == ESRCH);
    EXPECT_FALSE([WKWebsiteDataStore _defaultNetworkProcessExists]);
}

TEST(NetworkProcess, TerminateWhenNoDefaultWebsiteDataStore)
{
    pid_t networkProcessIdentifier = 0;
    @autoreleasepool {
        auto webView = adoptNS([WKWebView new]);
        [webView synchronouslyLoadTestPageNamed:@"simple"];
        EXPECT_TRUE([WKWebsiteDataStore _defaultNetworkProcessExists]);

        networkProcessIdentifier = [webView.get().configuration.websiteDataStore _networkProcessIdentifier];
        EXPECT_NE(networkProcessIdentifier, 0);
    }

    [WKWebsiteDataStore _deleteDefaultDataStoreForTesting];

    while (!kill(networkProcessIdentifier, 0))
        TestWebKitAPI::Util::spinRunLoop();
    EXPECT_TRUE(errno == ESRCH);
    EXPECT_FALSE([WKWebsiteDataStore _defaultNetworkProcessExists]);
}

TEST(NetworkProcess, DoNotLaunchOnDataStoreDestruction)
{
    auto storeConfiguration1 = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    auto websiteDataStore1 = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration: storeConfiguration1.get()]);

    EXPECT_FALSE([WKWebsiteDataStore _defaultNetworkProcessExists]);
    @autoreleasepool {
        auto storeConfiguration2 = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
        auto websiteDataStore2 = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration: storeConfiguration2.get()]);
    }

    TestWebKitAPI::Util::spinRunLoop(10);
    EXPECT_FALSE([WKWebsiteDataStore _defaultNetworkProcessExists]);
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
            
            constexpr auto response =
            "HTTP/1.1 204 No Content\r\n"
            "Access-Control-Allow-Origin: http://example.com\r\n"
            "Access-Control-Allow-Methods: OPTIONS, GET, POST, DELETE\r\n"
            "Cache-Control: max-age=604800\r\n\r\n"_s;
            connection.send(response, [&, connection] {
                connection.receiveHTTPRequest([&, connection] (Vector<char>&& request) {
                    const char* expectedRequestBegin = "DELETE / HTTP/1.1\r\n";
                    EXPECT_TRUE(!memcmp(request.data(), expectedRequestBegin, strlen(expectedRequestBegin)));
                    EXPECT_TRUE(strnstr(request.data(), "Origin: http://example.com\r\n", request.size()));
                    constexpr auto response =
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: 2\r\n\r\n"
                    "hi"_s;
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


static Vector<RetainPtr<WKScriptMessage>> receivedMessagesVector;
static bool receivedMessage = false;
static RetainPtr<WKScriptMessage> waitAndGetNextMessage()
{
    if (receivedMessagesVector.isEmpty()) {
        receivedMessage = false;
        TestWebKitAPI::Util::run(&receivedMessage);
    }

    EXPECT_EQ(receivedMessagesVector.size(), 1U);
    return receivedMessagesVector.takeLast();
}

@interface NetworkProcessTestMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation NetworkProcessTestMessageHandler
- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedMessagesVector.append(message);
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
    auto expectedCookieString = makeString("TEST=", createVersion4UUIDString());
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
    auto messageHandler = adoptNS([[NetworkProcessTestMessageHandler alloc] init]);
    [[webViewConfiguration userContentController] addScriptMessageHandler:messageHandler.get() name:@"test"];

    auto webView1 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);
    auto webView2 = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:webViewConfiguration.get()]);

    receivedMessage = false;
    receivedMessagesVector.clear();

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
    EXPECT_TRUE(receivedMessagesVector.isEmpty());

    // Test that initial communication from webView1 to webView2 works.
    receivedMessage = false;
    receivedMessagesVector.clear();
    bool finishedRunningScript = false;
    [webView1 evaluateJavaScript:@"bc.postMessage('foo')" completionHandler: [&] (id result, NSError *error) {
        EXPECT_TRUE(!error);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);

    TestWebKitAPI::Util::run(&receivedMessage);
    TestWebKitAPI::Util::spinRunLoop(10);

    EXPECT_EQ(receivedMessagesVector.size(), 1U);
    EXPECT_EQ([receivedMessagesVector[0] webView], webView2);
    EXPECT_WK_STREQ([receivedMessagesVector[0] body], @"foo");

    // Test that initial communication from webView2 to webView1 works.
    receivedMessage = false;
    receivedMessagesVector.clear();
    finishedRunningScript = false;
    [webView2 evaluateJavaScript:@"bc.postMessage('bar')" completionHandler: [&] (id result, NSError *error) {
        EXPECT_TRUE(!error);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);

    TestWebKitAPI::Util::run(&receivedMessage);
    TestWebKitAPI::Util::spinRunLoop(10);

    EXPECT_EQ(receivedMessagesVector.size(), 1U);
    EXPECT_EQ([receivedMessagesVector[0] webView], webView1);
    EXPECT_WK_STREQ([receivedMessagesVector[0] body], @"bar");

    // Kill the network process.
    kill(networkPID, 9);
    while ([[WKWebsiteDataStore defaultDataStore] _networkProcessIdentifier] == networkPID)
        TestWebKitAPI::Util::spinRunLoop(10);

    waitUntilNetworkProcessIsResponsive(webView1.get(), webView2.get());

    // Test that initial communication from webView1 to webView2 works.
    receivedMessage = false;
    receivedMessagesVector.clear();
    finishedRunningScript = false;
    [webView1 evaluateJavaScript:@"bc.postMessage('foo2')" completionHandler: [&] (id result, NSError *error) {
        EXPECT_TRUE(!error);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);

    TestWebKitAPI::Util::run(&receivedMessage);
    TestWebKitAPI::Util::spinRunLoop(10);

    EXPECT_EQ(receivedMessagesVector.size(), 1U);
    EXPECT_EQ([receivedMessagesVector[0] webView], webView2);
    EXPECT_WK_STREQ([receivedMessagesVector[0] body], @"foo2");

    // Test that initial communication from webView2 to webView1 works.
    receivedMessage = false;
    receivedMessagesVector.clear();
    finishedRunningScript = false;
    [webView2 evaluateJavaScript:@"bc.postMessage('bar2')" completionHandler: [&] (id result, NSError *error) {
        EXPECT_TRUE(!error);
        finishedRunningScript = true;
    }];
    TestWebKitAPI::Util::run(&finishedRunningScript);

    TestWebKitAPI::Util::run(&receivedMessage);
    TestWebKitAPI::Util::spinRunLoop(10);

    EXPECT_EQ(receivedMessagesVector.size(), 1U);
    EXPECT_EQ([receivedMessagesVector[0] webView], webView1);
    EXPECT_WK_STREQ([receivedMessagesVector[0] body], @"bar2");

    auto networkPID2 = [[WKWebsiteDataStore defaultDataStore] _networkProcessIdentifier];
    EXPECT_NE(networkPID2, 0);
    EXPECT_NE(networkPID, networkPID2);

    EXPECT_EQ(webPID1, [webView1 _webProcessIdentifier]);
    EXPECT_EQ(webPID2, [webView2 _webProcessIdentifier]);
}

#if HAVE(NSURLSESSION_TASK_DELEGATE)

@interface TestDataTaskDelegate : NSObject<_WKDataTaskDelegate>

@property (nonatomic, copy) void(^didReceiveAuthenticationChallenge)(_WKDataTask *, NSURLAuthenticationChallenge *, void(^)(NSURLSessionAuthChallengeDisposition, NSURLCredential *));
@property (nonatomic, copy) void(^willPerformHTTPRedirection)(_WKDataTask *, NSHTTPURLResponse *, NSURLRequest *, void(^)(_WKDataTaskRedirectPolicy));
@property (nonatomic, copy) void (^didReceiveResponse)(_WKDataTask *, NSURLResponse *, void (^)(_WKDataTaskResponsePolicy));
@property (nonatomic, copy) void (^didReceiveData)(_WKDataTask *, NSData *);
@property (nonatomic, copy) void (^didCompleteWithError)(_WKDataTask *, NSError *);

@end

@implementation TestDataTaskDelegate

- (void)dataTask:(_WKDataTask *)dataTask didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential *))completionHandler
{
    if (_didReceiveAuthenticationChallenge)
        _didReceiveAuthenticationChallenge(dataTask, challenge, completionHandler);
    else
        completionHandler(NSURLSessionAuthChallengeRejectProtectionSpace, nil);
}

- (void)dataTask:(_WKDataTask *)dataTask willPerformHTTPRedirection:(NSHTTPURLResponse *)response newRequest:(NSURLRequest *)request decisionHandler:(void (^)(_WKDataTaskRedirectPolicy))decisionHandler
{
    if (_willPerformHTTPRedirection)
        _willPerformHTTPRedirection(dataTask, response, request, decisionHandler);
    else
        decisionHandler(_WKDataTaskRedirectPolicyAllow);
}

- (void)dataTask:(_WKDataTask *)dataTask didReceiveResponse:(NSURLResponse *)response decisionHandler:(void (^)(_WKDataTaskResponsePolicy))decisionHandler
{
    if (_didReceiveResponse)
        _didReceiveResponse(dataTask, response, decisionHandler);
    else
        decisionHandler(_WKDataTaskResponsePolicyAllow);
}

- (void)dataTask:(_WKDataTask *)dataTask didReceiveData:(NSData *)data
{
    if (_didReceiveData)
        _didReceiveData(dataTask, data);
}

- (void)dataTask:(_WKDataTask *)dataTask didCompleteWithError:(NSError *)error
{
    if (_didCompleteWithError)
        _didCompleteWithError(dataTask, error);
}

@end

TEST(_WKDataTask, Basic)
{
    using namespace TestWebKitAPI;
    constexpr auto html = "<script>document.cookie='testkey=value'</script>"_s;
    constexpr auto secondResponse = "second response"_s;
    Vector<char> secondRequest;
    auto server = HTTPServer(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> Task {
        while (1) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "/initial_request"_s) {
                co_await connection.awaitableSend(HTTPResponse(html).serialize());
                continue;
            }
            if (path == "/second_request"_s) {
                secondRequest = WTFMove(request);
                co_await connection.awaitableSend(HTTPResponse(secondResponse).serialize());
                continue;
            }
            EXPECT_FALSE(true);
        }
    });
    auto webView = adoptNS([TestWKWebView new]);
    [webView synchronouslyLoadRequest:server.request("/initial_request"_s)];

    __block bool done = false;
    RetainPtr<NSMutableURLRequest> postRequest = adoptNS([server.request("/second_request"_s) mutableCopy]);
    [postRequest setMainDocumentURL:postRequest.get().URL];
    [postRequest setHTTPMethod:@"POST"];
    auto requestBody = "request body";
    [postRequest setHTTPBody:[NSData dataWithBytes:requestBody length:strlen(requestBody)]];
    [webView _dataTaskWithRequest:postRequest.get() completionHandler:^(_WKDataTask *task) {
        auto delegate = adoptNS([TestDataTaskDelegate new]);
        task.delegate = delegate.get();
        __block bool receivedResponse = false;
        delegate.get().didReceiveResponse = ^(_WKDataTask *, NSURLResponse *response, void (^decisionHandler)(_WKDataTaskResponsePolicy)) {
            EXPECT_WK_STREQ(response.URL.absoluteString, postRequest.get().URL.absoluteString);
            receivedResponse = true;
            decisionHandler(_WKDataTaskResponsePolicyAllow);
        };
        __block bool receivedData = false;
        delegate.get().didReceiveData = ^(_WKDataTask *, NSData *data) {
            EXPECT_TRUE(receivedResponse);
            EXPECT_EQ(data.length, strlen(secondResponse));
            EXPECT_TRUE(!memcmp(data.bytes, secondResponse, data.length));
            receivedData = true;
        };
        delegate.get().didCompleteWithError = ^(_WKDataTask *, NSError *error) {
            EXPECT_TRUE(receivedData);
            EXPECT_NULL(error);
            done = true;
        };
    }];
    Util::run(&done);
    EXPECT_TRUE(strnstr(secondRequest.data(), "Cookie: testkey=value\r\n", secondRequest.size()));
    EXPECT_WK_STREQ(HTTPServer::parseBody(secondRequest), requestBody);

    done = false;
    __block RetainPtr<_WKDataTask> retainedTask;
    [webView _dataTaskWithRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"blob:blank"]] completionHandler:^(_WKDataTask *task) {
        retainedTask = task;
        auto delegate = adoptNS([TestDataTaskDelegate new]);
        task.delegate = delegate.get();
        __block bool receivedResponse = false;
        delegate.get().didReceiveResponse = ^(_WKDataTask *, NSURLResponse *response, void (^decisionHandler)(_WKDataTaskResponsePolicy)) {
            receivedResponse = true;
        };
        __block bool receivedData = false;
        delegate.get().didReceiveData = ^(_WKDataTask *, NSData *data) {
            receivedData = true;
        };
        delegate.get().didCompleteWithError = ^(_WKDataTask *task, NSError *error) {
            EXPECT_FALSE(receivedResponse);
            EXPECT_FALSE(receivedData);
            EXPECT_WK_STREQ(error.domain, NSURLErrorDomain);
            EXPECT_EQ(error.code, NSURLErrorUnsupportedURL);
            EXPECT_NOT_NULL(task.delegate);
            done = true;
        };
    }];
    Util::run(&done);
    EXPECT_NOT_NULL(retainedTask.get());
    EXPECT_NULL(retainedTask.get().delegate);
}

TEST(_WKDataTask, Challenge)
{
    using namespace TestWebKitAPI;
    HTTPServer server(HTTPServer::respondWithChallengeThenOK, HTTPServer::Protocol::Https);
    auto webView = adoptNS([TestWKWebView new]);

    __block bool done = false;
    [webView _dataTaskWithRequest:server.request() completionHandler:^(_WKDataTask *task) {
        auto delegate = adoptNS([TestDataTaskDelegate new]);
        task.delegate = delegate.get();
        __block bool receivedServerTrustChallenge = false;
        __block bool receivedBasicAuthChallenge = false;
        delegate.get().didReceiveAuthenticationChallenge = ^(_WKDataTask *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
            if ([challenge.protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodServerTrust]) {
                EXPECT_FALSE(receivedBasicAuthChallenge);
                EXPECT_FALSE(receivedServerTrustChallenge);
                receivedServerTrustChallenge = true;
                completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
                return;
            }
            EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodHTTPBasic);
            EXPECT_FALSE(receivedBasicAuthChallenge);
            EXPECT_TRUE(receivedServerTrustChallenge);
            receivedBasicAuthChallenge = true;
            completionHandler(NSURLSessionAuthChallengeUseCredential, nil);
        };
        __block bool receivedData = false;
        delegate.get().didReceiveData = ^(_WKDataTask *, NSData *data) {
            const char* expectedResponse = "<script>alert('success!')</script>";
            EXPECT_EQ(data.length, strlen(expectedResponse));
            EXPECT_TRUE(!memcmp(data.bytes, expectedResponse, data.length));
            receivedData = true;
        };
        delegate.get().didCompleteWithError = ^(_WKDataTask *, NSError *error) {
            EXPECT_TRUE(receivedData);
            EXPECT_TRUE(receivedBasicAuthChallenge);
            EXPECT_TRUE(receivedServerTrustChallenge);
            EXPECT_NULL(error);
            done = true;
        };
    }];
    Util::run(&done);
}

void sendLoop(TestWebKitAPI::Connection connection, bool& sentWithError)
{
    Vector<uint8_t> bytes(1000, 0);
    connection.sendAndReportError(WTFMove(bytes), [&, connection] (bool sawError) {
        if (sawError)
            sentWithError = true;
        else
            sendLoop(connection, sentWithError);
    });
}

TEST(_WKDataTask, Cancel)
{
    using namespace TestWebKitAPI;
    bool sentWithError { false };
    HTTPServer server([&] (Connection connection) {
        connection.receiveHTTPRequest([&, connection] (Vector<char>&&) {
            constexpr auto header = "HTTP/1.1 200 OK\r\n\r\n"_s;
            connection.send(header, [&, connection] {
                sendLoop(connection, sentWithError);
            });
        });
    });

    auto webView = adoptNS([WKWebView new]);
    [webView _dataTaskWithRequest:server.request() completionHandler:^(_WKDataTask *task) {
        auto delegate = adoptNS([TestDataTaskDelegate new]);
        task.delegate = delegate.get();
        delegate.get().didReceiveResponse = ^(_WKDataTask *task, NSURLResponse *response, void (^decisionHandler)(_WKDataTaskResponsePolicy)) {
            decisionHandler(_WKDataTaskResponsePolicyAllow);
            dispatch_async(dispatch_get_main_queue(), ^{
                EXPECT_NOT_NULL(task.delegate);
                [task cancel];
                EXPECT_NULL(task.delegate);
            });
        };
    }];
    Util::run(&sentWithError);

    __block bool completed { false };
    [webView _dataTaskWithRequest:server.request() completionHandler:^(_WKDataTask *task) {
        auto delegate = adoptNS([TestDataTaskDelegate new]);
        task.delegate = delegate.get();
        delegate.get().didReceiveResponse = ^(_WKDataTask *task, NSURLResponse *response, void (^decisionHandler)(_WKDataTaskResponsePolicy)) {
            decisionHandler(_WKDataTaskResponsePolicyCancel);
        };
        delegate.get().didCompleteWithError = ^(_WKDataTask *, NSError *error) {
            EXPECT_WK_STREQ(error.domain, NSURLErrorDomain);
            EXPECT_EQ(error.code, NSURLErrorCancelled);
            completed = true;
        };
    }];
    Util::run(&completed);
}

TEST(_WKDataTask, Redirect)
{
    using namespace TestWebKitAPI;
    HTTPServer server { {
        { "/"_s, { 301, { { "Location"_s, "/redirectTarget"_s }, { "Custom-Name"_s, "Custom-Value"_s } } } },
        { "/redirectTarget"_s, { "hi"_s } },
    } };
    auto webView = adoptNS([WKWebView new]);
    RetainPtr<NSURLRequest> serverRequest = server.request();
    __block bool receivedData { false };
    [webView _dataTaskWithRequest:serverRequest.get() completionHandler:^(_WKDataTask *task) {
        auto delegate = adoptNS([TestDataTaskDelegate new]);
        task.delegate = delegate.get();
        delegate.get().willPerformHTTPRedirection = ^(_WKDataTask *task, NSHTTPURLResponse *response, NSURLRequest *request, void(^decisionHandler)(_WKDataTaskRedirectPolicy)) {
            EXPECT_WK_STREQ(serverRequest.get().URL.absoluteString, response.URL.absoluteString);
            EXPECT_WK_STREQ([serverRequest.get().URL.absoluteString stringByAppendingString:@"redirectTarget"], request.URL.absoluteString);
            EXPECT_WK_STREQ([response valueForHTTPHeaderField:@"Custom-Name"], "Custom-Value");
            decisionHandler(_WKDataTaskRedirectPolicyAllow);
        };
        delegate.get().didReceiveData = ^(_WKDataTask *, NSData *data) {
            EXPECT_EQ(data.length, strlen("hi"));
            EXPECT_TRUE(!memcmp(data.bytes, "hi", data.length));
            receivedData = true;
        };
    }];
    Util::run(&receivedData);
    
    __block bool completed { false };
    __block bool receivedResponse { false };
    [webView _dataTaskWithRequest:serverRequest.get() completionHandler:^(_WKDataTask *task) {
        auto delegate = adoptNS([TestDataTaskDelegate new]);
        task.delegate = delegate.get();
        delegate.get().willPerformHTTPRedirection = ^(_WKDataTask *task, NSHTTPURLResponse *response, NSURLRequest *request, void(^decisionHandler)(_WKDataTaskRedirectPolicy)) {
            decisionHandler(_WKDataTaskRedirectPolicyCancel);
        };
        delegate.get().didReceiveResponse = ^(_WKDataTask *task, NSURLResponse *response, void (^decisionHandler)(_WKDataTaskResponsePolicy)) {
            EXPECT_WK_STREQ(response.URL.absoluteString, serverRequest.get().URL.absoluteString);
            EXPECT_EQ(((NSHTTPURLResponse *)response).statusCode, 301);
            receivedResponse = true;
            decisionHandler(_WKDataTaskResponsePolicyAllow);
        };
        delegate.get().didCompleteWithError = ^(_WKDataTask *, NSError *error) {
            EXPECT_TRUE(receivedResponse);
            EXPECT_NULL(error);
            completed = true;
        };
    }];
    Util::run(&completed);
}

TEST(_WKDataTask, Crash)
{
    using namespace TestWebKitAPI;
    HTTPServer server(HTTPServer::respondWithOK);
    auto webView = adoptNS([WKWebView new]);

    __block bool done = false;
    [webView _dataTaskWithRequest:server.request() completionHandler:^(_WKDataTask *task) {
        auto delegate = adoptNS([TestDataTaskDelegate new]);
        task.delegate = delegate.get();
        delegate.get().didReceiveResponse = ^(_WKDataTask *task, NSURLResponse *response, void (^decisionHandler)(_WKDataTaskResponsePolicy)) {
            kill(webView.get().configuration.websiteDataStore._networkProcessIdentifier, SIGKILL);
            decisionHandler(_WKDataTaskResponsePolicyAllow);
        };
        delegate.get().didCompleteWithError = ^(_WKDataTask *, NSError *error) {
            EXPECT_WK_STREQ(error.domain, WebKitErrorDomain);
            EXPECT_EQ(error.code, 300);
            done = true;
        };
    }];
    Util::run(&done);
}

#endif // HAVE(NSURLSESSION_TASK_DELEGATE)

TEST(WKWebView, CrossOriginDoubleRedirectAuthentication)
{
    using namespace TestWebKitAPI;
    bool done { false };

    auto hasAuthorizationHeaderField = [] (const Vector<char>& request) {
        const char* field = "Authorization: TestValue\r\n";
        return memmem(request.data(), request.size(), field, strlen(field));
    };

    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> Task {
        while (true) {
            auto request = co_await connection.awaitableReceiveHTTPRequest();
            auto path = HTTPServer::parsePath(request);
            if (path == "http://example.com/original-target"_s) {
                EXPECT_TRUE(hasAuthorizationHeaderField(request));
                co_await connection.awaitableSend(HTTPResponse(302, { { "Location"_s, "http://not-example.com/first-redirect"_s } }, { }).serialize());
            } else if (path == "http://not-example.com/first-redirect"_s) {
                EXPECT_FALSE(hasAuthorizationHeaderField(request));
                co_await connection.awaitableSend(HTTPResponse(302, { { "Location"_s, "http://not-example.com/second-redirect"_s } }, { }).serialize());
            } else if (path == "http://not-example.com/second-redirect"_s) {
                EXPECT_FALSE(hasAuthorizationHeaderField(request));
                done = true;
            } else
                EXPECT_FALSE(true);
        }
    });

    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPProxyPort: @(server.port())
    }];
    auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
    auto viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:dataStore.get()];

    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:@"http://example.com/original-target"]];
    [request setValue:@"TestValue" forHTTPHeaderField:@"Authorization"];

    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect configuration:viewConfiguration.get()]);
    [webView loadRequest:request];
    Util::run(&done);
}

static constexpr auto mainBytes = R"TESTRESOURCE(
<script>
function terminateWorker()
{
    worker.terminate();
    worker = null;
}

var worker = new Worker('worker.js');
worker.onerror = (event) => {
    window.webkit.messageHandlers.testHandler.postMessage(event);
};
worker.onmessage = (event) => {
    window.webkit.messageHandlers.testHandler.postMessage(event.data);
};
</script>
)TESTRESOURCE"_s;

static constexpr auto workerBytes = R"TESTRESOURCE(
caches.open("test").then(() => {
    self.postMessage('cache is opened');
}, () => {
    self.postMessage('cache cannot be opened');
});
)TESTRESOURCE"_s;

TEST(NetworkProcess, DoNotLaunchForDOMCacheDestruction)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { mainBytes } },
        { "/worker.js"_s, { { { "Content-Type"_s, "text/javascript"_s } }, workerBytes } }
    });
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration.get().websiteDataStore _setResourceLoadStatisticsEnabled:NO];
    auto messageHandler = adoptNS([[NetworkProcessTestMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:messageHandler.get() name:@"testHandler"];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get() addToWindow:YES]);
    [webView loadRequest:server.request()];
    EXPECT_WK_STREQ(@"cache is opened", (NSString *)[waitAndGetNextMessage() body]);
    EXPECT_TRUE([configuration.get().websiteDataStore _networkProcessExists]);

    kill(configuration.get().websiteDataStore._networkProcessIdentifier, SIGKILL);
    while ([configuration.get().websiteDataStore _networkProcessExists])
        TestWebKitAPI::Util::spinRunLoop();

    // Unregister service worker client.
    bool done = false;
    [webView evaluateJavaScript:@"terminateWorker()" completionHandler:[&] (id result, NSError *error) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    [configuration.get().processPool _garbageCollectJavaScriptObjectsForTesting];

    TestWebKitAPI::Util::spinRunLoop(50);
    EXPECT_FALSE([configuration.get().websiteDataStore _networkProcessExists]);
}
