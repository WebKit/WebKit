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

#import <Network/Network.h>
#import <wtf/CompletionHandler.h>
#import <wtf/Forward.h>
#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/StringHash.h>

namespace TestWebKitAPI {

class Connection;

class HTTPServer {
public:
    struct HTTPResponse;
    struct RequestData;
    enum class Protocol : uint8_t { Http, Https, HttpsWithLegacyTLS, Http2 };
    using CertificateVerifier = Function<void(sec_protocol_metadata_t, sec_trust_t, sec_protocol_verify_complete_t)>;

    HTTPServer(std::initializer_list<std::pair<String, HTTPResponse>>, Protocol = Protocol::Http, CertificateVerifier&& = nullptr, RetainPtr<SecIdentityRef>&& = nullptr, Optional<uint16_t> port = { });
    HTTPServer(Function<void(Connection)>&&, Protocol = Protocol::Http);
    ~HTTPServer();
    uint16_t port() const;
    NSURLRequest *request(const String& path = "/"_str) const;
    size_t totalRequests() const;
    void cancel();

    static void respondWithOK(Connection);
    static void respondWithChallengeThenOK(Connection);

private:
    static RetainPtr<nw_parameters_t> listenerParameters(Protocol, CertificateVerifier&&, RetainPtr<SecIdentityRef>&&, Optional<uint16_t> port);
    static void respondToRequests(Connection, Ref<RequestData>);

    Ref<RequestData> m_requestData;
    RetainPtr<nw_listener_t> m_listener;
    Protocol m_protocol { Protocol::Http };
};

class Connection {
public:
    void send(String&&, CompletionHandler<void()>&& = nullptr) const;
    void send(Vector<uint8_t>&&, CompletionHandler<void()>&& = nullptr) const;
    void send(RetainPtr<dispatch_data_t>&&, CompletionHandler<void()>&& = nullptr) const;
    void receiveBytes(CompletionHandler<void(Vector<uint8_t>&&)>&&) const;
    void receiveHTTPRequest(CompletionHandler<void(Vector<char>&&)>&&, Vector<char>&& buffer = { }) const;
    void terminate();
    void cancel();

private:
    friend class HTTPServer;
    Connection(nw_connection_t connection)
        : m_connection(connection) { }

    RetainPtr<nw_connection_t> m_connection;
};

struct HTTPServer::HTTPResponse {
    enum class TerminateConnection { No, Yes };
    
    HTTPResponse(const String& body)
        : body(body) { }
    HTTPResponse(HashMap<String, String>&& headerFields, String&& body)
        : headerFields(WTFMove(headerFields))
        , body(WTFMove(body)) { }
    HTTPResponse(unsigned statusCode, HashMap<String, String>&& headerFields = { }, String&& body = { })
        : statusCode(statusCode)
        , headerFields(WTFMove(headerFields))
        , body(WTFMove(body)) { }
    HTTPResponse(TerminateConnection terminateConnection)
        : terminateConnection(terminateConnection) { }

    HTTPResponse(const HTTPResponse&) = default;
    HTTPResponse(HTTPResponse&&) = default;
    HTTPResponse() = default;
    HTTPResponse& operator=(const HTTPResponse&) = default;
    HTTPResponse& operator=(HTTPResponse&&) = default;
    
    unsigned statusCode { 200 };
    HashMap<String, String> headerFields;
    String body;
    TerminateConnection terminateConnection { TerminateConnection::No };
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

RetainPtr<SecIdentityRef> testIdentity();
RetainPtr<SecIdentityRef> testIdentity2();
