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

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import <WebKit/WKHTTPCookieStorePrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/StringConcatenateNumbers.h>

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

    auto storeConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [storeConfiguration setAllowsServerPreconnect:NO];
    auto viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get()]);
    auto delegate = adoptNS([ProxyDelegate new]);
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
                    connection.send(WTFMove(response), [=] {
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

    auto storeConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    [storeConfiguration setProxyConfiguration:@{
        @"SOCKSProxy": @"127.0.0.1",
        @"SOCKSPort": @(server.port())
    }];
    [storeConfiguration setAllowsServerPreconnect:NO];
    auto viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://example.com/"]]];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "success!");
}

static HTTPServer proxyAuthenticationServer()
{
    return HTTPServer(HTTPServer::respondWithOK, HTTPServer::Protocol::HttpsProxyWithAuthentication);
}

static std::pair<RetainPtr<WKWebView>, RetainPtr<ProxyDelegate>> webViewAndDelegate(const HTTPServer& server)
{
    auto storeConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPSProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPSProxyPort: @(server.port())
    }];
    [storeConfiguration setAllowsServerPreconnect:NO];
    [storeConfiguration setPreventsSystemHTTPProxyAuthentication:YES];

    auto viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get()]);
    auto delegate = adoptNS([ProxyDelegate new]);
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
    
    auto storeConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPSProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPSProxyPort: @(server.port())
    }];
    [storeConfiguration setAllowsServerPreconnect:NO];
    [storeConfiguration setRequiresSecureHTTPSProxyConnection:YES];

    auto viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get()]);
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
                const char* body =
                "<script>"
                    "fetch("
                        "'http://www.webkit.org/path3',"
                        "{credentials:'include'}"
                    ").then(()=>{"
                        "alert('fetched')"
                    "}).catch((e)=>{"
                        "alert(e)"
                    "})"
                "</script>";
                switch (connectionCount) {
                case 1: {
                    EXPECT_TRUE(strstr(request.data(), "GET http://www.webkit.org/path1 HTTP/1.1\r\n"));
                    reply = makeString(
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Length: ", strlen(body), "\r\n"
                        "Set-Cookie: a=b\r\n"
                        "Connection: close\r\n"
                        "\r\n", body
                    );
                    break;
                }
                case 3: {
                    EXPECT_TRUE(strstr(request.data(), "GET http://example.com/path2 HTTP/1.1\r\n"));
                    reply = makeString(
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/html\r\n"
                        "Content-Length: ", strlen(body), "\r\n"
                        "Connection: close\r\n"
                        "\r\n", body
                    );
                    break;
                }
                case 2:
                case 4:
                    if (connectionCount == 2 || shouldRelaxThirdPartyCookieBlocking)
                        EXPECT_TRUE(strstr(request.data(), "Cookie: a=b\r\n"));
                    else
                        EXPECT_FALSE(strstr(request.data(), "Cookie: a=b\r\n"));
                    EXPECT_TRUE(strstr(request.data(), "GET http://www.webkit.org/path3 HTTP/1.1\r\n"));
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
                connection.send(WTFMove(reply));
            });
        });

        auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
        [storeConfiguration setProxyConfiguration:@{
            (NSString *)kCFStreamPropertyHTTPProxyHost: @"127.0.0.1",
            (NSString *)kCFStreamPropertyHTTPProxyPort: @(server.port())
        }];
        [storeConfiguration setAllowsServerPreconnect:NO];
        auto dataStore = adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]);
        [dataStore _setResourceLoadStatisticsEnabled:YES];
        auto viewConfiguration = adoptNS([WKWebViewConfiguration new]);
        [viewConfiguration _setShouldRelaxThirdPartyCookieBlocking:shouldRelaxThirdPartyCookieBlocking];
        [viewConfiguration setWebsiteDataStore:dataStore.get()];
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get()]);

        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://www.webkit.org/path1"]]];
        EXPECT_WK_STREQ([webView _test_waitForAlert], "fetched");

        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://example.com/path2"]]];
        EXPECT_WK_STREQ([webView _test_waitForAlert], "fetched");
    };
    runTest(true);
    runTest(false);
}

} // namespace TestWebKitAPI
