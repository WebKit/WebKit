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

#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestUIDelegate.h"
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/ParsingUtilities.h>

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

#if HAVE(PRECONNECT_PING)

// Hand-rolled HTTP/2 framing. Protocol::Http2Raw only negotiates the "h2" ALPN; the framing
// is this test's business, so it lives here rather than in HTTPServer.
namespace H2 {

// https://http2.github.io/http2-spec/#rfc.section.4.1
class Frame {
public:

    // https://http2.github.io/http2-spec/#rfc.section.6
    enum class Type : uint8_t {
        Data = 0x0,
        Headers = 0x1,
        Priority = 0x2,
        RSTStream = 0x3,
        Settings = 0x4,
        PushPromise = 0x5,
        Ping = 0x6,
        GoAway = 0x7,
        WindowUpdate = 0x8,
        Continuation = 0x9,
    };

    Frame(Type type, uint8_t flags, uint32_t streamID, Vector<uint8_t> payload)
        : m_type(type)
        , m_flags(flags)
        , m_streamID(streamID)
        , m_payload(WTF::move(payload)) { }

    Type type() const { return m_type; }
    uint8_t flags() const { return m_flags; }
    uint32_t streamID() const { return m_streamID; }
    const Vector<uint8_t>& payload() const { return m_payload; }

private:
    Type m_type;
    uint8_t m_flags;
    uint32_t m_streamID;
    Vector<uint8_t> m_payload;
};

class Connection : public RefCounted<Connection> {
public:
    static Ref<Connection> create(TestWebKitAPI::Connection tlsConnection) { return adoptRef(*new Connection(tlsConnection)); }
    void send(Frame&&, CompletionHandler<void()>&& = nullptr) const;
    void receive(CompletionHandler<void(Frame&&)>&&) const;
private:
    Connection(TestWebKitAPI::Connection tlsConnection)
    : m_tlsConnection(tlsConnection) { }

    TestWebKitAPI::Connection m_tlsConnection;
    mutable bool m_expectClientConnectionPreface { true };
    mutable bool m_sendServerConnectionPreface { true };
    mutable Vector<uint8_t> m_receiveBuffer;
};

void Connection::send(Frame&& frame, CompletionHandler<void()>&& completionHandler) const
{
    auto frameType = frame.type();
    auto sendFrame = [tlsConnection = m_tlsConnection, frame = WTF::move(frame), completionHandler = WTF::move(completionHandler)] () mutable {
        // https://http2.github.io/http2-spec/#rfc.section.4.1
        Vector<uint8_t> bytes;
        constexpr size_t frameHeaderLength = 9;
        bytes.reserveInitialCapacity(frameHeaderLength + frame.payload().size());
        bytes.append(frame.payload().size() >> 16);
        bytes.append(frame.payload().size() >> 8);
        bytes.append(frame.payload().size() >> 0);
        bytes.append(static_cast<uint8_t>(frame.type()));
        bytes.append(frame.flags());
        bytes.append(frame.streamID() >> 24);
        bytes.append(frame.streamID() >> 16);
        bytes.append(frame.streamID() >> 8);
        bytes.append(frame.streamID() >> 0);
        bytes.appendVector(frame.payload());
        tlsConnection.send(WTF::move(bytes), WTF::move(completionHandler));
    };

    if (m_sendServerConnectionPreface && frameType != Frame::Type::Settings) {
        // https://http2.github.io/http2-spec/#rfc.section.3.5
        m_sendServerConnectionPreface = false;
        send(Frame(Frame::Type::Settings, 0, 0, { }), WTF::move(sendFrame));
    } else
        sendFrame();
}

void Connection::receive(CompletionHandler<void(Frame&&)>&& completionHandler) const
{
    if (m_expectClientConnectionPreface) {
        // https://http2.github.io/http2-spec/#rfc.section.3.5
        constexpr size_t clientConnectionPrefaceLength = 24;
        if (m_receiveBuffer.size() < clientConnectionPrefaceLength) {
            m_tlsConnection.receiveBytes([this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)] (Vector<uint8_t>&& bytes) mutable {
                m_receiveBuffer.appendVector(bytes);
                receive(WTF::move(completionHandler));
            });
            return;
        }
        ASSERT(spanHasPrefix(m_receiveBuffer.span(), "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"_span));
        m_receiveBuffer.removeAt(0, clientConnectionPrefaceLength);
        m_expectClientConnectionPreface = false;
        return receive(WTF::move(completionHandler));
    }

    // https://http2.github.io/http2-spec/#rfc.section.4.1
    constexpr size_t frameHeaderLength = 9;
    if (m_receiveBuffer.size() >= frameHeaderLength) {
        uint32_t payloadLength = (static_cast<uint32_t>(m_receiveBuffer[0]) << 16)
        + (static_cast<uint32_t>(m_receiveBuffer[1]) << 8)
        + (static_cast<uint32_t>(m_receiveBuffer[2]) << 0);
        if (m_receiveBuffer.size() >= frameHeaderLength + payloadLength) {
            auto type = static_cast<Frame::Type>(m_receiveBuffer[3]);
            auto flags = m_receiveBuffer[4];
            auto streamID = (static_cast<uint32_t>(m_receiveBuffer[5]) << 24)
            + (static_cast<uint32_t>(m_receiveBuffer[6]) << 16)
            + (static_cast<uint32_t>(m_receiveBuffer[7]) << 8)
            + (static_cast<uint32_t>(m_receiveBuffer[8]) << 0);
            Vector<uint8_t> payload;
            payload.append(m_receiveBuffer.subspan(frameHeaderLength, payloadLength));
            m_receiveBuffer.removeAt(0, frameHeaderLength + payloadLength);
            return completionHandler(Frame(type, flags, streamID, WTF::move(payload)));
        }
    }

    m_tlsConnection.receiveBytes([this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)] (Vector<uint8_t>&& bytes) mutable {
        m_receiveBuffer.appendVector(bytes);
        receive(WTF::move(completionHandler));
    });
}

} // namespace H2

#endif

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
    RetainPtr webView = adoptNS([WKWebView new]);
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
    RetainPtr webView = adoptNS([WKWebView new]);

    [webView _preconnectToServer:server.request().URL];
    Util::run(&anyConnections);
    Util::spinRunLoop(10);
    EXPECT_FALSE(requested);

    webView.get()._canUseCredentialStorage = NO;
    [webView loadRequest:server.request()];
    Util::run(&requested);

    EXPECT_EQ(connectionCount, 1u);
}

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
    RetainPtr webView = adoptNS([WKWebView new]);
    RetainPtr delegate = adoptNS([TestNavigationDelegate new]);
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
        pingPong(WTF::move(connection), headersCount);
    });
}

TEST(Preconnect, H2Ping)
{
    size_t headersCount = 0;
    HTTPServer server([headersCount = &headersCount] (Connection tlsConnection) {
        pingPong(H2::Connection::create(tlsConnection), headersCount);
    }, HTTPServer::Protocol::Http2Raw);
    
    RetainPtr delegate = adoptNS([SessionDelegate new]);
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
    }, HTTPServer::Protocol::Http2Raw);

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration]);

    RetainPtr delegate = adoptNS([TestNavigationDelegate new]);
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

static void verifyPreconnectDisabled(void(*disabler)(WKWebViewConfiguration *))
{
    size_t connectionCount { 0 };
    HTTPServer server([&](Connection) {
        connectionCount++;
    });
    NSString *html = [NSString stringWithFormat:@"<link rel='preconnect' href='http://127.0.0.1:%d'>", server.port()];

    {
        RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
        disabler(configuration.get());
        RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
        [webView loadHTMLString:html baseURL:nil];
        [webView _test_waitForDidFinishNavigation];
        Util::spinRunLoop(10);
        usleep(10000);
        Util::spinRunLoop(10);
        EXPECT_EQ(connectionCount, 0u);
    }

    {
        RetainPtr webView = adoptNS([WKWebView new]);
        [webView loadHTMLString:html baseURL:nil];
        [webView _test_waitForDidFinishNavigation];
        while (connectionCount != 1)
            Util::spinRunLoop();
    }
}

TEST(Preconnect, DisablePreconnect)
{
    verifyPreconnectDisabled([] (WKWebViewConfiguration *configuration) {
        configuration._allowedNetworkHosts = [NSSet set];
    });
    verifyPreconnectDisabled([] (WKWebViewConfiguration *configuration) {
        configuration._loadsSubresources = NO;
    });
}

#if HAVE(SYSTEM_SUPPORT_FOR_ADVANCED_PRIVACY_PROTECTIONS)

TEST(Preconnect, PrivacyProxyRequestFlags)
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

    constexpr auto policies = _WKWebsiteNetworkConnectionIntegrityPolicyEnabled
        | _WKWebsiteNetworkConnectionIntegrityPolicyFailClosed
        | _WKWebsiteNetworkConnectionIntegrityPolicyRequestValidation;

    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration defaultWebpagePreferences]._networkConnectionIntegrityPolicy = policies;

    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    RetainPtr request = adoptNS(server.request().mutableCopy);
    [request _setPrivacyProxyFailClosedForUnreachableHosts:YES];
    [request _setUseEnhancedPrivacyMode:YES];
    [webView loadRequest:request.get()];

    Util::run(&requested);
    EXPECT_EQ(connectionCount, 1U);
}

#endif // HAVE(SYSTEM_SUPPORT_FOR_ADVANCED_PRIVACY_PROTECTIONS)

}
