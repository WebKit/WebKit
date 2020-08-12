/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if HAVE(NETWORK_FRAMEWORK)

#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "Utilities.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewPrivate.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/RetainPtr.h>

#if HAVE(PRECONNECT_PING)
@interface SessionDelegate : NSObject <NSURLSessionDataDelegate>
@end

@implementation SessionDelegate
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition disposition, NSURLCredential *credential))completionHandler
{
    completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
}
@end
#endif

namespace TestWebKitAPI {

TEST(Preconnect, HTTP)
{
    size_t connectionCount = 0;
    bool connected = false;
    bool requested = false;
    HTTPServer server([&] (Connection connection) {
        ++connectionCount;
        connected = true;
        connection.receiveHTTPRequest([&](Vector<char>&&) {
            requested = true;
        });
    });
    auto webView = adoptNS([WKWebView new]);
    [webView _preconnectToServer:server.request().URL];
    Util::run(&connected);
    Util::spinRunLoop(10);
    EXPECT_FALSE(requested);
    [webView loadRequest:server.request()];
    Util::run(&requested);

    EXPECT_EQ(connectionCount, 1u);
}

TEST(Preconnect, ConnectionCount)
{
    size_t connectionCount = 0;
    bool anyConnections = false;
    bool requested = false;
    HTTPServer server([&] (Connection connection) {
        ++connectionCount;
        anyConnections = true;
        connection.receiveHTTPRequest([&](Vector<char>&&) {
            requested = true;
        });
    });
    auto webView = adoptNS([WKWebView new]);

    // The preconnect to the server will use the default setting of "use the credential store",
    // and therefore use the credential-store-blessed NSURLSession.
    [webView _preconnectToServer:server.request().URL];
    Util::run(&anyConnections);
    Util::spinRunLoop(10);
    EXPECT_FALSE(requested);

    // Then this request will *not* use the credential store, therefore using a different NSURLSession
    // that doesn't know about the above preconnect, triggering a second connection to the server.
    webView.get()._canUseCredentialStorage = NO;
    [webView loadRequest:server.request()];
    Util::run(&requested);

    EXPECT_EQ(connectionCount, 2u);
}

// Mojave CFNetwork _preconnect SPI seems to have a bug causing this to time out.
// That's no problem, because this is a test for SPI only to be used on later OS versions.
#if !PLATFORM(MAC) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500

TEST(Preconnect, HTTPS)
{
    bool connected = false;
    bool requested = false;
    __block bool receivedChallenge = false;
    HTTPServer server([&] (Connection connection) {
        connected = true;
        connection.receiveHTTPRequest([&](Vector<char>&&) {
            requested = true;
        });
    }, HTTPServer::Protocol::Https);
    auto webView = adoptNS([WKWebView new]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    [webView setNavigationDelegate:delegate.get()];
    [delegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        receivedChallenge = true;
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    }];
    [webView _preconnectToServer:server.request().URL];
    Util::run(&connected);
    Util::spinRunLoop(10);
    EXPECT_FALSE(receivedChallenge);
    EXPECT_FALSE(requested);
    [webView loadRequest:server.request()];
    Util::run(&requested);
    EXPECT_TRUE(receivedChallenge);
}

#endif

#if HAVE(PRECONNECT_PING)
static void pingPong(Ref<H2::Connection>&& connection, size_t* headersCount)
{
    connection->receive([connection, headersCount] (H2::Frame&& frame) mutable {
        switch (frame.type()) {
        case H2::Frame::Type::Headers:
            ++*headersCount;
            break;
        case H2::Frame::Type::Settings:
        case H2::Frame::Type::WindowUpdate:
            // These frame types are ok for a preconnect task.
            break;
        case H2::Frame::Type::Ping:
            {
                // https://http2.github.io/http2-spec/#rfc.section.6.7
                constexpr uint8_t ack = 0x1;
                connection->send(H2::Frame(H2::Frame::Type::Ping, ack, frame.streamID(), frame.payload()));
            }
            break;
        default:
            // If anything else is sent by the client, preconnect is doing too much.
            ASSERT_NOT_REACHED();
            break;
        }
        pingPong(WTFMove(connection), headersCount);
    });
}

TEST(Preconnect, H2Ping)
{
    size_t headersCount = 0;
    HTTPServer server([headersCount = &headersCount] (Connection tlsConnection) {
        pingPong(H2::Connection::create(tlsConnection), headersCount);
    }, HTTPServer::Protocol::Http2);
    
    auto delegate = adoptNS([SessionDelegate new]);
    NSURLSession *session = [NSURLSession sessionWithConfiguration:[NSURLSessionConfiguration ephemeralSessionConfiguration] delegate:delegate.get() delegateQueue:[NSOperationQueue mainQueue]];
    NSURLSessionDataTask *task = [session dataTaskWithRequest:server.request()];
    task._preconnect = YES;
    __block bool done = false;

    [task getUnderlyingHTTPConnectionInfoWithCompletionHandler:^(_NSHTTPConnectionInfo *connectionInfo) {
        EXPECT_TRUE(connectionInfo.isValid);
        [connectionInfo sendPingWithReceiveHandler:^(NSError *error, NSTimeInterval interval) {
            EXPECT_FALSE(error);
            EXPECT_GT(interval, 0.0);
            done = true;
        }];
    }];
    [task resume];
    Util::run(&done);
    
    // Make sure the client doesn't send anything except Settings, WindowUpdate, and Ping.
    // If Headers or Data were sent, then the preconnect wouldn't be preconnect.
    usleep(100000);
    Util::spinRunLoop(100);
    EXPECT_EQ(headersCount, 0u);

    NSURLSessionDataTask *task2 = [session dataTaskWithRequest:server.request()];
    [task2 resume];
    while (!headersCount)
        Util::spinRunLoop();
    EXPECT_EQ(headersCount, 1u);
    usleep(100000);
    Util::spinRunLoop(100);
    EXPECT_EQ(headersCount, 1u);
}

TEST(Preconnect, H2PingFromWebCoreNSURLSession)
{
    size_t headersCount = 0;
    HTTPServer server([headersCount = &headersCount] (Connection tlsConnection) {
        pingPong(H2::Connection::create(tlsConnection), headersCount);
    }, HTTPServer::Protocol::Http2);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);

    auto delegate = adoptNS([TestNavigationDelegate new]);
    __block bool receivedChallenge = false;
    [delegate setDidReceiveAuthenticationChallenge:^(WKWebView *, NSURLAuthenticationChallenge *challenge, void (^callback)(NSURLSessionAuthChallengeDisposition, NSURLCredential *)) {
        EXPECT_WK_STREQ(challenge.protectionSpace.authenticationMethod, NSURLAuthenticationMethodServerTrust);
        receivedChallenge = true;
        callback(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:challenge.protectionSpace.serverTrust]);
    }];
    [webView setNavigationDelegate:delegate.get()];
    [webView loadHTMLString:[NSString stringWithFormat:@"<script>internals.sendH2Ping('https://127.0.0.1:%d/').then(function(t){if(t>0){alert('pass')}else{alert('fail')}})</script>", server.port()] baseURL:nil];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "pass");
    EXPECT_FALSE(headersCount);
    EXPECT_TRUE(receivedChallenge);
}

#endif // HAVE(PRECONNECT_PING)

}

#endif
