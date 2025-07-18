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

#pragma once

#import "NetworkConnection.h"
#import <wtf/CompletionHandler.h>
#import <wtf/Forward.h>
#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/StringHash.h>

OBJC_CLASS NSURLRequest;

namespace TestWebKitAPI {

class WebTransportServer;
struct HTTPResponse;

class HTTPServer {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(HTTPServer);
public:
    struct RequestData;
    enum class Protocol : uint8_t { Http, Https, HttpsWithLegacyTLS, Http2, HttpsProxy, HttpsProxyWithAuthentication };
    using CertificateVerifier = Function<void(sec_protocol_metadata_t, sec_trust_t, sec_protocol_verify_complete_t)>;

    HTTPServer(std::initializer_list<std::pair<String, HTTPResponse>>, Protocol = Protocol::Http, CertificateVerifier&& = nullptr, SecIdentityRef = nullptr, std::optional<uint16_t> port = { });
    HTTPServer(Function<void(Connection)>&&, Protocol = Protocol::Http);
    enum class UseCoroutines : bool { Yes };
    HTTPServer(UseCoroutines, Function<ConnectionTask(Connection)>&&, Protocol = Protocol::Http);
    ~HTTPServer();
    uint16_t port() const;
    String origin() const;
    NSURLRequest *request(StringView path = "/"_s) const;
    NSURLRequest *requestWithLocalhost(StringView path = "/"_s) const;
    WKWebViewConfiguration *httpsProxyConfiguration() const;
    size_t totalConnections() const;
    size_t totalRequests() const;
    String lastRequestCookies() const;
    void cancel();
    void terminateAllConnections(CompletionHandler<void()>&&);

    void addResponse(String&& path, HTTPResponse&&);
    void setResponse(String&& path, HTTPResponse&&);

    static void respondWithOK(Connection);
    static void respondWithChallengeThenOK(Connection);
    static String parseCookies(const Vector<char>& request);
    static String parsePath(const Vector<char>& request);
    static String parseBody(const Vector<char>&);
    static Vector<uint8_t> testPrivateKey();
    static Vector<uint8_t> testCertificate();

private:
    static RetainPtr<nw_parameters_t> listenerParameters(Protocol, CertificateVerifier&&, RetainPtr<SecIdentityRef>&&, std::optional<uint16_t> port);
    static void respondToRequests(Connection, Ref<RequestData>);
    const char* scheme() const;

    Ref<RequestData> m_requestData;
    RetainPtr<nw_listener_t> m_listener;
    Protocol m_protocol { Protocol::Http };
};

struct HTTPResponse {
    enum class Behavior : uint8_t {
        SendResponseNormally,
        TerminateConnectionAfterReceivingRequest,
        NeverSendResponse
    };

    HTTPResponse(Vector<uint8_t>&& body)
        : body(WTFMove(body)) { }
    HTTPResponse(const String& body)
        : body(bodyFromString(body)) { }
    HTTPResponse(HashMap<String, String>&& headerFields, const String& body)
        : headerFields(WTFMove(headerFields))
        , body(bodyFromString(body)) { }
    HTTPResponse(unsigned statusCode, HashMap<String, String>&& headerFields = { }, const String& body = { })
        : statusCode(statusCode)
        , headerFields(WTFMove(headerFields))
        , body(bodyFromString(body)) { }
    HTTPResponse(Behavior behavior)
        : behavior(behavior) { }
    HTTPResponse(NSData *data)
        : body(makeVector(data)) { }

    HTTPResponse(const HTTPResponse&) = default;
    HTTPResponse(HTTPResponse&&) = default;
    HTTPResponse() = default;
    HTTPResponse& operator=(const HTTPResponse&) = default;
    HTTPResponse& operator=(HTTPResponse&&) = default;

    void setShouldRespondWith304ToConditionalRequests(HashMap<String, String>&& headerFields = { })
    {
        shouldRespondWith304ToConditionalRequests = true;
        headerFieldsFor304 = WTFMove(headerFields);
    }

    enum class IncludeContentLength : bool { No, Yes };
    Vector<uint8_t> serialize(IncludeContentLength = IncludeContentLength::Yes) const;
    static Vector<uint8_t> bodyFromString(const String&);

    unsigned statusCode { 200 };
    HashMap<String, String> headerFields;
    Vector<uint8_t> body;
    Behavior behavior { Behavior::SendResponseNormally };
    bool shouldRespondWith304ToConditionalRequests { false };
    HashMap<String, String> headerFieldsFor304;
};

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
        , m_payload(WTFMove(payload)) { }

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

} // namespace H2

} // namespace TestWebKitAPI

RetainPtr<SecCertificateRef> testCertificate();
RetainPtr<SecIdentityRef> testIdentity();
RetainPtr<SecIdentityRef> testIdentity2();
void verifyCertificateAndPublicKey(SecTrustRef);
