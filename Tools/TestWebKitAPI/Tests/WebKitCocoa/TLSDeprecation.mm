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

#include "config.h"

#if HAVE(SSL)

#import "PlatformUtilities.h"
#import "TCPServer.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "WebCoreTestSupport.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WebKit.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS_FAMILY)
#import <WebKit/WebUIKitSupport.h>
#import <WebKit/WebCoreThread.h>
#endif

@interface WebSocketDelegate : NSObject <WKUIDelegate, WebUIDelegate>
- (NSString *)waitForMessage;
@end

@implementation WebSocketDelegate {
    RetainPtr<NSString> _message;
}

- (NSString *)waitForMessage
{
    while (!_message)
        TestWebKitAPI::Util::spinRunLoop();
    return _message.autorelease();
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    _message = message;
    completionHandler();
}

- (void)webView:(WebView *)sender runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WebFrame *)frame
{
    _message = message;
}

@end

namespace TestWebKitAPI {

const uint16_t tls1_1 = 0x0302;
static NSString *defaultsKey = @"WebKitEnableLegacyTLS";

TEST(WebKit, TLSVersionWebSocket)
{
    auto getWebSocketEvent = [] (bool clientAllowDeprecatedTLS, bool serverLimitTLS) {
        Optional<uint16_t> maxServerTLSVersion;
        if (serverLimitTLS)
            maxServerTLSVersion = tls1_1;
        TCPServer server(TCPServer::Protocol::HTTPS, [=](SSL *ssl) {
            EXPECT_TRUE(!ssl == (clientAllowDeprecatedTLS != serverLimitTLS));
        }, maxServerTLSVersion);

        if (clientAllowDeprecatedTLS)
            [[NSUserDefaults standardUserDefaults] setBool:YES forKey:defaultsKey];
        
        auto webView = adoptNS([TestWKWebView new]);
        auto delegate = adoptNS([WebSocketDelegate new]);
        [webView setUIDelegate:delegate.get()];
        [webView synchronouslyLoadHTMLString:@"start network process"];
        [[webView configuration].processPool _allowAnyTLSCertificateForWebSocketTesting];
        [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:
            @"<script>"
            "const socket = new WebSocket('wss://localhost:%d');"
            "socket.onclose = function(event){ alert('close'); };"
            "socket.onerror = function(event){ alert('error: ' + event.data); };"
            "</script>", server.port()]];
        NSString *message = [delegate waitForMessage];
        
        if (clientAllowDeprecatedTLS)
            [[NSUserDefaults standardUserDefaults] removeObjectForKey:defaultsKey];

        return message;
    };

    EXPECT_WK_STREQ(getWebSocketEvent(true, true), "close");
    NSString *message = getWebSocketEvent(false, true);
    EXPECT_TRUE([message isEqualToString:@"error: undefined"] || [message isEqualToString:@"close"]);
    EXPECT_WK_STREQ(getWebSocketEvent(false, false), "close");
}

NSString *getWebSocketEventWebKitLegacy(bool clientAllowDeprecatedTLS, bool serverLimitTLS)
{
#if PLATFORM(IOS_FAMILY)
    WebKitInitialize();
    WebThreadLock();
#endif
    Optional<uint16_t> maxServerTLSVersion;
    if (serverLimitTLS)
        maxServerTLSVersion = tls1_1;
    TCPServer server(TCPServer::Protocol::HTTPS, [=](SSL *ssl) {
        EXPECT_TRUE(!ssl == (clientAllowDeprecatedTLS != serverLimitTLS));
    }, maxServerTLSVersion);

    if (clientAllowDeprecatedTLS)
        [[NSUserDefaults standardUserDefaults] setBool:YES forKey:defaultsKey];
    
    auto webView = adoptNS([WebView new]);
    auto delegate = adoptNS([WebSocketDelegate new]);
    [webView setUIDelegate:delegate.get()];
    WebCoreTestSupport::setAllowsAnySSLCertificate(true);
    [[webView mainFrame] loadHTMLString:[NSString stringWithFormat:
        @"<script>"
        "const socket = new WebSocket('wss://localhost:%d');"
        "socket.onclose = function(event){ alert('close'); };"
        "socket.onerror = function(event){ alert('error: ' + event.data); };"
        "</script>", server.port()] baseURL:nil];
    NSString *message = [delegate waitForMessage];

    if (clientAllowDeprecatedTLS)
        [[NSUserDefaults standardUserDefaults] removeObjectForKey:defaultsKey];

    return message;
}

TEST(WebKit, TLSVersionWebSocketWebKitLegacy1)
{
    EXPECT_WK_STREQ(getWebSocketEventWebKitLegacy(true, true), "close");
}

TEST(WebKit, TLSVersionWebSocketWebKitLegacy2)
{
#if PLATFORM(IOS_FAMILY)
    const char* expected = "error: undefined";
#else
    const char* expected = "close";
#endif
    EXPECT_WK_STREQ(getWebSocketEventWebKitLegacy(false, true), expected);
}

TEST(WebKit, TLSVersionWebSocketWebKitLegacy3)
{
    EXPECT_WK_STREQ(getWebSocketEventWebKitLegacy(false, false), "close");
}

TEST(WebKit, TLSVersionNetworkSession)
{
    static auto delegate = adoptNS([TestNavigationDelegate new]);
    auto makeWebViewWith = [&] (WKWebsiteDataStore *store) {
        WKWebViewConfiguration *configuration = [[[WKWebViewConfiguration alloc] init] autorelease];
        configuration.websiteDataStore = store;
        auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]);
        [webView setNavigationDelegate:delegate.get()];
        [delegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
            callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
        }];
        return webView;
    };
    {
        TCPServer server(TCPServer::Protocol::HTTPS, [](SSL *ssl) {
            EXPECT_FALSE(ssl);
        }, tls1_1);
        auto webView = makeWebViewWith([WKWebsiteDataStore defaultDataStore]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]]];
        [delegate waitForDidFailProvisionalNavigation];
    }
    {
        TCPServer server(TCPServer::Protocol::HTTPS, [](SSL *ssl) {
            EXPECT_FALSE(ssl);
        }, tls1_1);
        auto webView = makeWebViewWith([WKWebsiteDataStore nonPersistentDataStore]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]]];
        [delegate waitForDidFailProvisionalNavigation];
    }
    {
        TCPServer server(TCPServer::Protocol::HTTPS, [](SSL *ssl) {
            TCPServer::respondWithOK(ssl);
        }, tls1_1);
        [[NSUserDefaults standardUserDefaults] setBool:YES forKey:defaultsKey];
        auto webView = makeWebViewWith([WKWebsiteDataStore defaultDataStore]);
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]]];
        [delegate waitForDidFinishNavigation];
        [[NSUserDefaults standardUserDefaults] removeObjectForKey:defaultsKey];
    }
}

}

#endif // HAVE(SSL)
