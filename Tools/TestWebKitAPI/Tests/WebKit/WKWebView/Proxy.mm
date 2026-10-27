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

#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestUIDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/Utilities.h"
#import <WebKit/WKHTTPCookieStorePrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>
#import <wtf/StdLibExtras.h>
#import <wtf/text/MakeString.h>

@interface ProxyDelegate : NSObject <WKNavigationDelegate, WKUIDelegate>
- (NSString *)waitForAlert;
@end

@implementation ProxyDelegate {
    RetainPtr<NSString> _alert;
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    _alert = message;
    completionHandler();
}

- (NSString *)waitForAlert
{
    while (!_alert)
        TestWebKitAPI::Util::spinRunLoop();
    return _alert.autorelease();
}

- (void)webView:(WKWebView *)webView didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential))completionHandler
{
    if ([challenge.protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodServerTrust])
        return completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);

    EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodHTTPBasic);
    return completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialWithUser:@"testuser" password:@"testpassword" persistence:NSURLCredentialPersistenceNone]);
}

@end

namespace TestWebKitAPI {

TEST(WebKit, HTTPSProxy)
{
    HTTPServer server(HTTPServer::respondWithOK, HTTPServer::Protocol::HttpsProxy);

    RetainPtr storeConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [storeConfiguration setAllowsServerPreconnect:NO];
    RetainPtr viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get()]);
    RetainPtr delegate = adoptNS([ProxyDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/"]]];
    EXPECT_WK_STREQ([delegate waitForAlert], "success!");
}

TEST(WebKit, SOCKS5)
{
    constexpr uint8_t socks5Version = 0x5; // https://tools.ietf.org/html/rfc1928#section-3
    constexpr uint8_t noAuthenticationRequired = 0x00; // https://tools.ietf.org/html/rfc1928#section-3
    constexpr uint8_t connect = 0x01; // https://tools.ietf.org/html/rfc1928#section-4
    constexpr uint8_t reserved = 0x00; // https://tools.ietf.org/html/rfc1928#section-4
    constexpr uint8_t domainName = 0x03; // https://tools.ietf.org/html/rfc1928#section-4
    constexpr uint8_t requestSucceeded = 0x00; // https://tools.ietf.org/html/rfc1928#section-6

    using namespace TestWebKitAPI;
    HTTPServer server([](Connection connection) {
        connection.receiveBytes([=] (Vector<uint8_t>&& bytes) {
            constexpr uint8_t expectedAuthenticationMethodCount = 1;
            Vector<uint8_t> expectedClientGreeting { socks5Version, expectedAuthenticationMethodCount, noAuthenticationRequired };
            EXPECT_EQ(bytes, expectedClientGreeting);
            connection.send(Vector<uint8_t> { socks5Version, noAuthenticationRequired }, [=] {
                connection.receiveBytes([=] (Vector<uint8_t>&& bytes) {
                    constexpr uint8_t httpPortFirstByte = 0;
                    constexpr uint8_t httpPortSecondByte = 80;
                    Vector<uint8_t> expectedConnectRequest {
                        socks5Version,
                        connect,
                        reserved,
                        domainName,
                        static_cast<uint8_t>(strlen("example.com")),
                        'e', 'x', 'a', 'm', 'p', 'l', 'e', '.', 'c', 'o', 'm',
                        httpPortFirstByte, httpPortSecondByte
                    };
                    EXPECT_EQ(bytes, expectedConnectRequest);

                    Vector<uint8_t> response { socks5Version, requestSucceeded, reserved,
                        domainName,
                        static_cast<uint8_t>(strlen("example.com")),
                        'e', 'x', 'a', 'm', 'p', 'l', 'e', '.', 'c', 'o', 'm',
                        httpPortFirstByte, httpPortSecondByte
                    };
                    connection.send(WTF::move(response), [=] {
                        connection.receiveHTTPRequest([=] (Vector<char>&&) {
                            connection.send(
                                "HTTP/1.1 200 OK\r\n"
                                "Content-Length: 34\r\n"
                                "\r\n"
                                "<script>alert('success!')</script>"_s
                            );
                        });
                    });
                });
            });
        });
    });

    RetainPtr storeConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    [storeConfiguration setProxyConfiguration:@{
        @"SOCKSProxy": @"127.0.0.1",
        @"SOCKSPort": @(server.port())
    }];
    [storeConfiguration setAllowsServerPreconnect:NO];
    RetainPtr viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://example.com/"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "success!");
}

TEST(WebKit, HTTPSProxyAPI)
{
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)]);
    RetainPtr delegate = adoptNS([ProxyDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    TestWebKitAPI::HTTPServer nonProxyServer({
        { "/"_s, { "<script>alert('non proxy success!')</script>"_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Https);

    TestWebKitAPI::HTTPServer proxyServer1({
        { "/"_s, { "<script>alert('proxy success!')</script>"_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::HttpsProxy);

    TestWebKitAPI::HTTPServer proxyServer2({
        { "/"_s, { "<script>alert('other proxy success!')</script>"_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::HttpsProxy);

    // 127.0.0.1:1 will never be reachable and will immediately timeout, causing
    // us to fallback to 127.0.0.1:<real port>
    RetainPtr endpoint1 = adoptNS(nw_endpoint_create_host("127.0.0.1", "1"));
    RetainPtr proxyConfig1 = adoptNS(nw_proxy_config_create_http_connect(endpoint1.get(), nil));

    RetainPtr endpoint2 = adoptNS(nw_endpoint_create_host("127.0.0.1", std::to_string(proxyServer1.port()).c_str()));
    RetainPtr proxyConfig2 = adoptNS(nw_proxy_config_create_http_connect(endpoint2.get(), nil));

    RetainPtr endpoint3 = adoptNS(nw_endpoint_create_host("127.0.0.1", std::to_string(proxyServer2.port()).c_str()));
    RetainPtr proxyConfig3 = adoptNS(nw_proxy_config_create_http_connect(endpoint3.get(), nil));

    // Proxy 1 should be ignored as it is unreachable.
    // Proxy 2 should be used.
    // Proxy 3 should be ignored, since proxy 2 handled the request
    webView.get().configuration.websiteDataStore.proxyConfigurations = @[ proxyConfig1.get(), proxyConfig2.get(), proxyConfig3.get() ];

    EXPECT_EQ(webView.get().configuration.websiteDataStore.proxyConfigurations[0], proxyConfig1.get());
    EXPECT_EQ(webView.get().configuration.websiteDataStore.proxyConfigurations[1], proxyConfig2.get());
    EXPECT_EQ(webView.get().configuration.websiteDataStore.proxyConfigurations[2], proxyConfig3.get());

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/"]]];
    EXPECT_WK_STREQ([delegate waitForAlert], "proxy success!");

    // Proxy 3 should be used
    // Proxy 2 should now be ignored, since proxy 3 handled the request
    webView.get().configuration.websiteDataStore.proxyConfigurations = @[ proxyConfig3.get(), proxyConfig2.get() ];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://yetanotherexample.net/"]]];
    EXPECT_WK_STREQ([delegate waitForAlert], "other proxy success!");

    // Clear the proxies. The server should be hit directly.
    webView.get().configuration.websiteDataStore.proxyConfigurations = nil;
    EXPECT_EQ(webView.get().configuration.websiteDataStore.proxyConfigurations, nil);

    [webView loadRequest:nonProxyServer.request()];
    EXPECT_WK_STREQ([delegate waitForAlert], "non proxy success!");
}

TEST(WebKit, CanPrefetchDNSWithProxyConfigured)
{
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)]);
    WKWebsiteDataStore *dataStore = webView.get().configuration.websiteDataStore;

    __block bool canPrefetch = false;
    __block bool done = false;
    [dataStore _canPrefetchDNSForTesting:^(BOOL result) {
        canPrefetch = result;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_TRUE(canPrefetch);

    RetainPtr endpoint = adoptNS(nw_endpoint_create_host("127.0.0.1", "1"));
    RetainPtr proxyConfig = adoptNS(nw_proxy_config_create_http_connect(endpoint.get(), nil));
    dataStore.proxyConfigurations = @[ proxyConfig.get() ];

    done = false;
    [dataStore _canPrefetchDNSForTesting:^(BOOL result) {
        canPrefetch = result;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_FALSE(canPrefetch);

    dataStore.proxyConfigurations = nil;

    done = false;
    [dataStore _canPrefetchDNSForTesting:^(BOOL result) {
        canPrefetch = result;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_TRUE(canPrefetch);
}

TEST(WebKit, PrefetchDNSSkippedWhenProxyConfigured)
{
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)]);
    WKWebsiteDataStore *dataStore = webView.get().configuration.websiteDataStore;

    auto prefetchedHostnameCount = [&] {
        __block uint64_t count = 0;
        __block bool done = false;
        [dataStore _prefetchedDNSHostnameCountForTesting:^(NSUInteger result) {
            count = result;
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);
        return count;
    };

    RetainPtr endpoint = adoptNS(nw_endpoint_create_host("127.0.0.1", "1"));
    RetainPtr proxyConfig = adoptNS(nw_proxy_config_create_http_connect(endpoint.get(), nil));
    dataStore.proxyConfigurations = @[ proxyConfig.get() ];

    uint64_t countBeforeProxiedLoad = prefetchedHostnameCount();
    [webView loadHTMLString:@"<link rel='dns-prefetch' href='https://prefetch-dns-with-proxy.example/'>" baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    EXPECT_FALSE(TestWebKitAPI::Util::waitFor([&] {
        return prefetchedHostnameCount() > countBeforeProxiedLoad;
    }, 200));

    dataStore.proxyConfigurations = nil;

    uint64_t countBeforeUnproxiedLoad = prefetchedHostnameCount();
    [webView loadHTMLString:@"<link rel='dns-prefetch' href='https://prefetch-dns-without-proxy.example/'>" baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    EXPECT_TRUE(TestWebKitAPI::Util::waitFor([&] {
        return prefetchedHostnameCount() > countBeforeUnproxiedLoad;
    }, 200));
}

TEST(WebKit, ProxyAfterNetworkProcessCrash)
{
    TestWebKitAPI::HTTPServer proxyServer1({
        { "/"_s, { "<script>alert('proxy success!')</script>"_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::HttpsProxy);

    RetainPtr endpoint = adoptNS(nw_endpoint_create_host("127.0.0.1", std::to_string(proxyServer1.port()).c_str()));
    RetainPtr proxyConfig = adoptNS(nw_proxy_config_create_http_connect(endpoint.get(), nil));

    auto dataStore = WKWebsiteDataStore.nonPersistentDataStore;
    dataStore.proxyConfigurations = @[ proxyConfig.get() ];
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:dataStore];

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:configuration.get()]);
    RetainPtr delegate = adoptNS([ProxyDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    NSURLRequest *exampleRequest = [NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/"]];
    [webView loadRequest:exampleRequest];
    EXPECT_WK_STREQ([delegate waitForAlert], "proxy success!");

    pid_t originalNetworkProcessIdentifier = [dataStore _networkProcessIdentifier];
    EXPECT_NE(originalNetworkProcessIdentifier, 0);
    kill(originalNetworkProcessIdentifier, 9);
    while ([dataStore _networkProcessIdentifier])
        TestWebKitAPI::Util::spinRunLoop();
    [webView loadRequest:exampleRequest];
    EXPECT_WK_STREQ([delegate waitForAlert], "proxy success!");
    EXPECT_NE(originalNetworkProcessIdentifier, [dataStore _networkProcessIdentifier]);
}

TEST(WebKit, SOCKS5API)
{
    constexpr uint8_t socks5Version = 0x5; // https://tools.ietf.org/html/rfc1928#section-3
    constexpr uint8_t noAuthenticationRequired = 0x00; // https://tools.ietf.org/html/rfc1928#section-3
    constexpr uint8_t connect = 0x01; // https://tools.ietf.org/html/rfc1928#section-4
    constexpr uint8_t reserved = 0x00; // https://tools.ietf.org/html/rfc1928#section-4
    constexpr uint8_t domainName = 0x03; // https://tools.ietf.org/html/rfc1928#section-4
    constexpr uint8_t requestSucceeded = 0x00; // https://tools.ietf.org/html/rfc1928#section-6

    using namespace TestWebKitAPI;
    HTTPServer server([](Connection connection) {
        connection.receiveBytes([=] (Vector<uint8_t>&& bytes) {
            constexpr uint8_t expectedAuthenticationMethodCount = 1;
            Vector<uint8_t> expectedClientGreeting { socks5Version, expectedAuthenticationMethodCount, noAuthenticationRequired };
            EXPECT_EQ(bytes, expectedClientGreeting);
            connection.send(Vector<uint8_t> { socks5Version, noAuthenticationRequired }, [=] {
                connection.receiveBytes([=] (Vector<uint8_t>&& bytes) {
                    constexpr uint8_t httpPortFirstByte = 0;
                    constexpr uint8_t httpPortSecondByte = 80;
                    Vector<uint8_t> expectedConnectRequest {
                        socks5Version,
                        connect,
                        reserved,
                        domainName,
                        static_cast<uint8_t>(strlen("example.com")),
                        'e', 'x', 'a', 'm', 'p', 'l', 'e', '.', 'c', 'o', 'm',
                        httpPortFirstByte, httpPortSecondByte
                    };
                    EXPECT_EQ(bytes, expectedConnectRequest);

                    Vector<uint8_t> response { socks5Version, requestSucceeded, reserved,
                        domainName,
                        static_cast<uint8_t>(strlen("example.com")),
                        'e', 'x', 'a', 'm', 'p', 'l', 'e', '.', 'c', 'o', 'm',
                        httpPortFirstByte, httpPortSecondByte
                    };
                    connection.send(WTF::move(response), [=] {
                        connection.receiveHTTPRequest([=] (Vector<char>&&) {
                            connection.send(
                                "HTTP/1.1 200 OK\r\n"
                                "Content-Length: 34\r\n"
                                "\r\n"
                                "<script>alert('success!')</script>"_s
                            );
                        });
                    });
                });
            });
        });
    });

    RetainPtr endpoint = adoptNS(nw_endpoint_create_host("127.0.0.1", std::to_string(server.port()).c_str()));
    RetainPtr proxyConfig = adoptNS(nw_proxy_config_create_socksv5(endpoint.get()));

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)]);
    webView.get().configuration.websiteDataStore.proxyConfigurations = @[ proxyConfig.get() ];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://example.com/"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "success!");
}

// Answers the SOCKS5 greeting and CONNECT request, after which the connection is a tunnel to the
// destination that the client asked for, whatever it is. Returns false if the client closes it first.
static Awaitable<bool> acceptSOCKS5Connection(Connection connection)
{
    constexpr uint8_t socks5Version = 0x5; // https://tools.ietf.org/html/rfc1928#section-3
    constexpr uint8_t noAuthenticationRequired = 0x00; // https://tools.ietf.org/html/rfc1928#section-3
    constexpr uint8_t connect = 0x01; // https://tools.ietf.org/html/rfc1928#section-4
    constexpr uint8_t requestSucceeded = 0x00; // https://tools.ietf.org/html/rfc1928#section-6

    constexpr uint8_t expectedAuthenticationMethodCount = 1;
    auto clientGreeting = co_await connection.awaitableReceiveBytes();
    if (clientGreeting.isEmpty())
        co_return false;
    EXPECT_EQ(clientGreeting, (Vector<uint8_t> { socks5Version, expectedAuthenticationMethodCount, noAuthenticationRequired }));
    co_await connection.awaitableSend(Vector<uint8_t> { socks5Version, noAuthenticationRequired });

    // The reply has the layout of the request, with the status where the command was.
    auto reply = co_await connection.awaitableReceiveBytes();
    if (reply.isEmpty())
        co_return false;
    EXPECT_EQ(reply[1], connect);
    reply[1] = requestSucceeded;
    co_await connection.awaitableSend(WTF::move(reply));
    co_return true;
}

// Once the client closes the connection, awaitableReceiveHTTPRequest() retries the read forever
// instead of returning, so this reads the request itself, and returns false at the close.
static Awaitable<bool> receiveHTTPRequestUnlessClosed(Connection connection)
{
    Vector<char> request;
    while (!contains(request.span(), "\r\n\r\n"_span)) {
        auto bytes = co_await connection.awaitableReceiveBytes();
        if (bytes.isEmpty())
            co_return false;
        request.appendVector(WTF::move(bytes));
    }
    co_return true;
}

// Acts as a SOCKS5 proxy that answers every request it tunnels with a page that alerts `message`.
// Like a real proxy, it keeps the tunnel open between requests, so the client pools the connection
// through it and can reuse it.
static ConnectionTask respondThroughSOCKS5Tunnel(Connection connection, ASCIILiteral message)
{
    if (!co_await acceptSOCKS5Connection(connection))
        co_return;

    auto body = makeString("<script>alert('"_s, message, "')</script>"_s);
    while (co_await receiveHTTPRequestUnlessClosed(connection))
        co_await connection.awaitableSend(HTTPResponse(body).serialize());
}

static HTTPServer socks5ProxyServer(ASCIILiteral message)
{
    return HTTPServer(HTTPServer::UseCoroutines::Yes, [message](Connection connection) {
        return respondThroughSOCKS5Tunnel(connection, message);
    });
}

static RetainPtr<nw_proxy_config_t> socks5ProxyConfiguration(const HTTPServer& proxy)
{
    RetainPtr endpoint = adoptNS(nw_endpoint_create_host("127.0.0.1", std::to_string(proxy.port()).c_str()));
    return adoptNS(nw_proxy_config_create_socksv5(endpoint.get()));
}

static RetainPtr<WKWebView> webViewWithDataStore(WKWebsiteDataStore *dataStore)
{
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration setWebsiteDataStore:dataStore];
    return adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:configuration.get()]);
}

struct LoadResult {
    RetainPtr<NSString> alert;
    RetainPtr<NSError> error;
};

// Loads the URL and waits for that navigation to end, rather than for any navigation, because an
// earlier one can still finish after its page has alerted. Only pages from the test proxies alert.
static LoadResult loadAndWaitForNavigation(WKWebView *webView, NSString *url)
{
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    RetainPtr uiDelegate = adoptNS([TestUIDelegate new]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView setUIDelegate:uiDelegate.get()];

    __block LoadResult result;
    __block RetainPtr<WKNavigation> navigation;
    __block bool done = false;
    uiDelegate.get().runJavaScriptAlertPanelWithMessage = ^(WKWebView *, NSString *message, WKFrameInfo *, void (^completionHandler)(void)) {
        result.alert = message;
        completionHandler();
    };
    navigationDelegate.get().didFinishNavigation = ^(WKWebView *, WKNavigation *finishedNavigation) {
        if (finishedNavigation == navigation.get())
            done = true;
    };
    auto failed = ^(WKWebView *, WKNavigation *failedNavigation, NSError *error) {
        if (failedNavigation != navigation.get())
            return;
        result.error = error;
        done = true;
    };
    navigationDelegate.get().didFailProvisionalNavigation = failed;
    navigationDelegate.get().didFailNavigation = failed;

    navigation = [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:url]]];
    Util::run(&done);
    return result;
}

// Waits for a reply from the network process, which comes after it has handled everything the
// data store sent it earlier, such as a proxy change.
static void waitForNetworkProcessToHandleEarlierMessages(WKWebsiteDataStore *dataStore)
{
    __block bool done = false;
    [dataStore fetchDataRecordsOfTypes:[NSSet setWithObject:WKWebsiteDataTypeCookies] completionHandler:^(NSArray<WKWebsiteDataRecord *> *) {
        done = true;
    }];
    Util::run(&done);
}

TEST(WebKit, SOCKS5ProxyChangeAppliesToConnectedHost)
{
    auto proxyA = socks5ProxyServer("proxy A"_s);
    auto proxyB = socks5ProxyServer("proxy B"_s);

    RetainPtr dataStore = [WKWebsiteDataStore nonPersistentDataStore];
    dataStore.get().proxyConfigurations = @[ socks5ProxyConfiguration(proxyA).get() ];
    RetainPtr webView = webViewWithDataStore(dataStore.get());

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://example.com/first"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "proxy A");

    // The connection to example.com through proxy A is still open, and the next load must not reuse it.
    dataStore.get().proxyConfigurations = @[ socks5ProxyConfiguration(proxyB).get() ];
    waitForNetworkProcessToHandleEarlierMessages(dataStore.get());
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://example.com/second"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "proxy B");
}

TEST(WebKit, SOCKS5ProxyRemovalAppliesToConnectedHost)
{
    auto proxy = socks5ProxyServer("proxied"_s);

    RetainPtr dataStore = [WKWebsiteDataStore nonPersistentDataStore];
    dataStore.get().proxyConfigurations = @[ socks5ProxyConfiguration(proxy).get() ];
    RetainPtr webView = webViewWithDataStore(dataStore.get());

    // Only the proxy can reach this host, because .invalid never resolves (RFC 6761).
    EXPECT_WK_STREQ(loadAndWaitForNavigation(webView.get(), @"http://doesnotexist.invalid/first").alert.get(), "proxied");

    // Without the proxy, the load has to fail, rather than go through the connection that is still open.
    dataStore.get().proxyConfigurations = nil;
    waitForNetworkProcessToHandleEarlierMessages(dataStore.get());
    auto result = loadAndWaitForNavigation(webView.get(), @"http://doesnotexist.invalid/second");
    EXPECT_NULL(result.alert.get());
    EXPECT_EQ([result.error code], NSURLErrorCannotFindHost);
}

TEST(WebKit, ProxyRemovalAfterNetworkProcessCrash)
{
    auto proxy = socks5ProxyServer("proxied"_s);

    RetainPtr dataStore = [WKWebsiteDataStore nonPersistentDataStore];
    dataStore.get().proxyConfigurations = @[ socks5ProxyConfiguration(proxy).get() ];
    RetainPtr webView = webViewWithDataStore(dataStore.get());

    EXPECT_WK_STREQ(loadAndWaitForNavigation(webView.get(), @"http://doesnotexist.invalid/first").alert.get(), "proxied");

    dataStore.get().proxyConfigurations = nil;
    pid_t originalNetworkProcessIdentifier = [dataStore _networkProcessIdentifier];
    EXPECT_NE(originalNetworkProcessIdentifier, 0);
    kill(originalNetworkProcessIdentifier, 9);
    while ([dataStore _networkProcessIdentifier])
        Util::spinRunLoop();

    // The relaunched network process must not bring the removed proxy back.
    auto result = loadAndWaitForNavigation(webView.get(), @"http://doesnotexist.invalid/second");
    EXPECT_NULL(result.alert.get());
    EXPECT_EQ([result.error code], NSURLErrorCannotFindHost);
}

TEST(WebKit, SOCKS5ProxyChangeLetsLoadInFlightFinish)
{
    std::optional<Connection> connectionAwaitingResponse;
    HTTPServer proxyA(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        if (co_await acceptSOCKS5Connection(connection) && co_await receiveHTTPRequestUnlessClosed(connection))
            connectionAwaitingResponse = connection;
    });
    auto proxyB = socks5ProxyServer("proxy B"_s);

    RetainPtr dataStore = [WKWebsiteDataStore nonPersistentDataStore];
    dataStore.get().proxyConfigurations = @[ socks5ProxyConfiguration(proxyA).get() ];
    RetainPtr webView = webViewWithDataStore(dataStore.get());

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://example.com/first"]]];
    while (!connectionAwaitingResponse)
        Util::spinRunLoop();

    dataStore.get().proxyConfigurations = @[ socks5ProxyConfiguration(proxyB).get() ];
    waitForNetworkProcessToHandleEarlierMessages(dataStore.get());
    pid_t networkProcessIdentifier = [dataStore _networkProcessIdentifier];

    // The load that was in flight during the change finishes through proxy A. The response closes
    // the connection, so that this test is about that load, and not about reusing the connection.
    connectionAwaitingResponse->send(HTTPResponse({ { "Connection"_s, "close"_s } }, "<script>alert('proxy A')</script>"_s).serialize());
    EXPECT_WK_STREQ([webView _test_waitForAlert], "proxy A");

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://example.com/second"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "proxy B");
    EXPECT_EQ([dataStore _networkProcessIdentifier], networkProcessIdentifier);
}

TEST(WebKit, WebSocketOpenAcrossSOCKS5ProxyChange)
{
    std::optional<Connection> webSocketConnection;
    HTTPServer proxy(HTTPServer::UseCoroutines::Yes, [&](Connection connection) -> ConnectionTask {
        if (!co_await acceptSOCKS5Connection(connection))
            co_return;
        co_await AwaitableFromCompletionHandler<void> { [connection](auto completionHandler) mutable {
            connection.webSocketHandshake(WTF::move(completionHandler));
        } };
        webSocketConnection = connection;
    });

    RetainPtr dataStore = [WKWebsiteDataStore nonPersistentDataStore];
    dataStore.get().proxyConfigurations = @[ socks5ProxyConfiguration(proxy).get() ];
    RetainPtr webView = webViewWithDataStore(dataStore.get());

    // The close event comes before the web process tells the network process that the web socket
    // is gone, so alert after that.
    [webView loadHTMLString:@"<script>"
        "var webSocket = new WebSocket('ws://example.com/');"
        "webSocket.onopen = () => alert('open');"
        "webSocket.onclose = () => setTimeout(() => alert('closed'));"
        "</script>" baseURL:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "open");
    while (!webSocketConnection)
        Util::spinRunLoop();

    dataStore.get().proxyConfigurations = nil;
    waitForNetworkProcessToHandleEarlierMessages(dataStore.get());
    pid_t networkProcessIdentifier = [dataStore _networkProcessIdentifier];

    // The web socket is still on the session that the change replaced. Closing it from the server's end
    // finishes its task before the web process lets go of it, so the replaced session can become invalid
    // while its wrapper still has the web socket.
    constexpr uint8_t finalCloseFrame = 0x88; // https://www.rfc-editor.org/rfc/rfc6455#section-5.2
    webSocketConnection->send(Vector<uint8_t> { finalCloseFrame, 0 });
    EXPECT_WK_STREQ([webView _test_waitForAlert], "closed");

    waitForNetworkProcessToHandleEarlierMessages(dataStore.get());
    EXPECT_EQ([dataStore _networkProcessIdentifier], networkProcessIdentifier);
}

static HTTPServer proxyAuthenticationServer()
{
    return HTTPServer(HTTPServer::respondWithOK, HTTPServer::Protocol::HttpsProxyWithAuthentication);
}

static std::pair<RetainPtr<WKWebView>, RetainPtr<ProxyDelegate>> webViewAndDelegate(const HTTPServer& server)
{
    RetainPtr storeConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPSProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPSProxyPort: @(server.port())
    }];
    [storeConfiguration setAllowsServerPreconnect:NO];
    [storeConfiguration setPreventsSystemHTTPProxyAuthentication:YES];

    RetainPtr viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get()]);
    RetainPtr delegate = adoptNS([ProxyDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];
    return { webView, delegate };
}

TEST(WebKit, HTTPProxyAuthentication)
{
    auto server = proxyAuthenticationServer();
    auto [webView, delegate] = webViewAndDelegate(server);

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/"]]];
    EXPECT_WK_STREQ([delegate waitForAlert], "success!");
}

// rdar://136531022
#if USE(APPLE_INTERNAL_SDK)
TEST(WebKit, ProxyConfigurationAuthentication)
#else
TEST(WebKit, DISABLED_ProxyConfigurationAuthentication)
#endif
{
    auto server = proxyAuthenticationServer();
    RetainPtr webView = adoptNS([WKWebView new]);

    RetainPtr endpoint = adoptNS(nw_endpoint_create_host("127.0.0.1", std::to_string(server.port()).c_str()));
    RetainPtr proxyConfiguration = adoptNS(nw_proxy_config_create_http_connect(endpoint.get(), nil));
    webView.get().configuration.websiteDataStore.proxyConfigurations = @[ proxyConfiguration.get() ];

    RetainPtr delegate = adoptNS([TestNavigationDelegate new]);
    __block bool challenged { false };
    delegate.get().didReceiveAuthenticationChallenge = ^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^completionHandler)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        if ([challenge.protectionSpace.authenticationMethod isEqualToString:NSURLAuthenticationMethodServerTrust])
            return completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
        challenged = true;
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodHTTPBasic);
        return completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialWithUser:@"testuser" password:@"testpassword" persistence:NSURLCredentialPersistenceNone]);
    };
    webView.get().navigationDelegate = delegate.get();

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "success!");
    EXPECT_TRUE(challenged);
}

TEST(WebKit, HTTPProxyAuthenticationCrossOrigin)
{
    auto server = proxyAuthenticationServer();
    auto [webView, delegate] = webViewAndDelegate(server);

    [webView loadHTMLString:@"<script>fetch('https://example.com/',{mode:'no-cors'}).then(()=>{alert('fetched successfully!')}).catch(()=>{alert('failure!')})</script>" baseURL:[NSURL URLWithString:@"https://webkit.org/"]];
    EXPECT_WK_STREQ([delegate waitForAlert], "fetched successfully!");
}

TEST(WebKit, SecureProxyConnection)
{
    bool receivedValidClientHello = false;
    HTTPServer server([&] (const Connection& connection) {
        connection.receiveBytes([&](Vector<uint8_t>&& clientHelloBytes) {
            // Check that the client sends what looks like the beginning of a TLS handshake.
            // We can't test more than this because CFNetwork requires a certificate chain signed by a trusted CA,
            // and we wouldn't want to include such a certificate's private key in this repository.

            // https://tools.ietf.org/html/rfc5246#section-6.2.1
            enum class ContentType : uint8_t { Handshake = 22 };
            EXPECT_EQ(clientHelloBytes[0], static_cast<uint8_t>(ContentType::Handshake));
            uint16_t tlsPlaintextLength = clientHelloBytes[3] * 256u + clientHelloBytes[4];
            uint32_t clientHelloBytesLength = clientHelloBytes[6] * 65536u + clientHelloBytes[7] * 256u + clientHelloBytes[8];
            EXPECT_EQ(clientHelloBytes.size(), tlsPlaintextLength + sizeof(ContentType) + 2 * sizeof(uint8_t) + sizeof(uint16_t));
            
            // https://tools.ietf.org/html/rfc5246#section-7.4
            enum class HandshakeType : uint8_t { ClientHello = 1 };
            EXPECT_EQ(clientHelloBytes[5], static_cast<uint8_t>(HandshakeType::ClientHello));
            EXPECT_EQ(tlsPlaintextLength, clientHelloBytesLength + sizeof(HandshakeType) + 3);

            receivedValidClientHello = true;
        });
    });
    
    RetainPtr storeConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPSProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPSProxyPort: @(server.port())
    }];
    [storeConfiguration setAllowsServerPreconnect:NO];
    [storeConfiguration setRequiresSecureHTTPSProxyConnection:YES];

    RetainPtr viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/"]]];
    TestWebKitAPI::Util::run(&receivedValidClientHello);
}

TEST(WebKit, RelaxThirdPartyCookieBlocking)
{
    __block bool setDefaultCookieAcceptPolicy = false;
    [[WKWebsiteDataStore defaultDataStore].httpCookieStore _setCookieAcceptPolicy:NSHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain completionHandler:^{
        setDefaultCookieAcceptPolicy = true;
    }];
    Util::run(&setDefaultCookieAcceptPolicy);

    auto runTest = [] (bool shouldRelaxThirdPartyCookieBlocking) {
        HTTPServer server([connectionCount = 0, shouldRelaxThirdPartyCookieBlocking] (Connection connection) mutable {
            ++connectionCount;
            connection.receiveHTTPRequest([connection, connectionCount, shouldRelaxThirdPartyCookieBlocking] (Vector<char>&& request) {
                String reply;
                constexpr auto body =
                "<script>"
                    "fetch("
                        "'http://www.webkit.org/path3',"
                        "{credentials:'include'}"
                    ").then(()=>{"
                        "alert('fetched')"
                    "}).catch((e)=>{"
                        "alert(e)"
                    "})"
                "</script>"_s;
                switch (connectionCount) {
                case 1: {
                    EXPECT_TRUE(contains(request.span(), "GET http://www.webkit.org/path1 HTTP/1.1\r\n"_span));
                    reply = makeString(
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Length: "_s, strlen(body), "\r\n"
                        "Set-Cookie: a=b\r\n"
                        "Connection: close\r\n"
                        "\r\n"_s, body
                    );
                    break;
                }
                case 3: {
                    EXPECT_TRUE(contains(request.span(), "GET http://example.com/path2 HTTP/1.1\r\n"_span));
                    reply = makeString(
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/html\r\n"
                        "Content-Length: "_s, strlen(body), "\r\n"
                        "Connection: close\r\n"
                        "\r\n"_s, body
                    );
                    break;
                }
                case 2:
                case 4:
                    if (connectionCount == 2 || shouldRelaxThirdPartyCookieBlocking)
                        EXPECT_TRUE(contains(request.span(), "Cookie: a=b\r\n"_span));
                    else
                        EXPECT_FALSE(contains(request.span(), "Cookie: a=b\r\n"_span));
                    EXPECT_TRUE(contains(request.span(), "GET http://www.webkit.org/path3 HTTP/1.1\r\n"_span));
                    reply =
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Length: 0\r\n"
                        "Access-Control-Allow-Origin: http://example.com\r\n"
                        "Access-Control-Allow-Credentials: true\r\n"
                        "Connection: close\r\n"
                        "\r\n"_s;
                    break;
                default:
                    ASSERT_NOT_REACHED();
                }
                connection.send(WTF::move(reply));
            });
        });

        RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
        [storeConfiguration setProxyConfiguration:@{
            (NSString *)kCFStreamPropertyHTTPProxyHost: @"127.0.0.1",
            (NSString *)kCFStreamPropertyHTTPProxyPort: @(server.port())
        }];
        [storeConfiguration setAllowsServerPreconnect:NO];
        RetainPtr dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
        [dataStore _setResourceLoadStatisticsEnabled:YES];
        RetainPtr viewConfiguration = adoptNS([WKWebViewConfiguration new]);
        [viewConfiguration _setShouldRelaxThirdPartyCookieBlocking:shouldRelaxThirdPartyCookieBlocking];
        [viewConfiguration setWebsiteDataStore:dataStore.get()];
        RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get()]);

        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://www.webkit.org/path1"]]];
        EXPECT_WK_STREQ([webView _test_waitForAlert], "fetched");

        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://example.com/path2"]]];
        EXPECT_WK_STREQ([webView _test_waitForAlert], "fetched");
    };
    runTest(true);
    runTest(false);
}

} // namespace TestWebKitAPI
