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

#if HAVE(SSL)

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "TCPServer.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
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

- (void)webView:(WKWebView *)webView didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential * _Nullable credential))completionHandler
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
    TCPServer server(TCPServer::Protocol::HTTPSProxy, TCPServer::respondWithOK);

    auto storeConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [storeConfiguration setAllowsServerPreconnect:NO];
    auto viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:[[[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()] autorelease]];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get()]);
    auto delegate = adoptNS([ProxyDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/"]]];
    EXPECT_WK_STREQ([delegate waitForAlert], "success!");
}

TEST(WebKit, HTTPProxyAuthentication)
{
    TCPServer server([] (int socket) {
        auto requestShouldContain = [] (const auto& request, const char* str) {
            EXPECT_TRUE(strnstr(reinterpret_cast<const char*>(request.data()), str, request.size()));
        };

        auto connectRequest = TCPServer::read(socket);
        requestShouldContain(connectRequest, "CONNECT example.com:443");
        const char* response1 =
            "HTTP/1.1 407 Proxy Authentication Required\r\n"
            "Proxy-Authenticate: Basic realm=\"testrealm\"\r\n"
            "Content-Length: 0\r\n"
            "\r\n";
        TCPServer::write(socket, response1, strlen(response1));

        auto connectRequestWithCredentials = TCPServer::read(socket);
        requestShouldContain(connectRequestWithCredentials, "CONNECT example.com:443");
        requestShouldContain(connectRequestWithCredentials, "Proxy-Authorization: Basic dGVzdHVzZXI6dGVzdHBhc3N3b3Jk");
        const char* response2 =
            "HTTP/1.1 200 Connection Established\r\n"
            "Connection: close\r\n"
            "\r\n";
        TCPServer::write(socket, response2, strlen(response2));

        TCPServer::startSecureConnection(socket, TCPServer::respondWithOK);
    });
    
    auto storeConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPSProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPSProxyPort: @(server.port())
    }];
    [storeConfiguration setAllowsServerPreconnect:NO];
    [storeConfiguration setPreventsSystemHTTPProxyAuthentication:YES];

    auto viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:[[[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()] autorelease]];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get()]);
    auto delegate = adoptNS([ProxyDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    [webView setUIDelegate:delegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/"]]];
    EXPECT_WK_STREQ([delegate waitForAlert], "success!");
}

TEST(WebKit, SecureProxyConnection)
{
    std::atomic<bool> receivedValidClientHello = false;
    TCPServer server([&] (int socket) {
        // Check that the client sends what looks like the beginning of a TLS handshake.
        // We can't test more than this because CFNetwork requires a certificate chain signed by a trusted CA,
        // and we wouldn't want to include such a certificate's private key in this repository.
        auto clientHelloBytes = TCPServer::read(socket);

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
    
    auto storeConfiguration = adoptNS([_WKWebsiteDataStoreConfiguration new]);
    [storeConfiguration setProxyConfiguration:@{
        (NSString *)kCFStreamPropertyHTTPSProxyHost: @"127.0.0.1",
        (NSString *)kCFStreamPropertyHTTPSProxyPort: @(server.port())
    }];
    [storeConfiguration setAllowsServerPreconnect:NO];
    [storeConfiguration setRequiresSecureHTTPSProxyConnection:YES];

    auto viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:[[[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()] autorelease]];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100) configuration:viewConfiguration.get()]);
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/"]]];
    while (!receivedValidClientHello)
        TestWebKitAPI::Util::spinRunLoop();
}

} // namespace TestWebKitAPI

#endif // HAVE(SSL)
