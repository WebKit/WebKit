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

#if (_LIBCPP_VERSION >= 14000) && !defined(_LIBCPP_HAS_NO_CXX20_COROUTINES)
#import <coroutine>
#else
#import <experimental/coroutine>
namespace std {
using std::experimental::coroutine_handle;
using std::experimental::suspend_never;
using std::experimental::suspend_always;
}
#endif

OBJC_CLASS NSURLRequest;

namespace TestWebKitAPI {

class Connection;
struct HTTPResponse;

template<typename PromiseType>
struct CoroutineHandle {
    CoroutineHandle(std::coroutine_handle<PromiseType>&& handle)
        : handle(WTFMove(handle)) { }
    CoroutineHandle(const CoroutineHandle&) = delete;
    CoroutineHandle(CoroutineHandle&& other)
        : handle(std::exchange(other.handle, nullptr)) { }
    ~CoroutineHandle()
    {
        if (handle)
            handle.destroy();
    }
    std::coroutine_handle<PromiseType> handle;
};

struct Task {
    struct promise_type {
        Task get_return_object() { return { std::coroutine_handle<promise_type>::from_promise(*this) }; }
        std::suspend_never initial_suspend() { return { }; }
        std::suspend_always final_suspend() noexcept { return { }; }
        void unhandled_exception() { }
        void return_void() { }
    };
    CoroutineHandle<promise_type> handle;
};

class ReceiveOperation;
class SendOperation;

class HTTPServer {
    WTF_MAKE_FAST_ALLOCATED;
public:
    struct RequestData;
    enum class Protocol : uint8_t { Http, Https, HttpsWithLegacyTLS, Http2, HttpsProxy, HttpsProxyWithAuthentication };
    using CertificateVerifier = Function<void(sec_protocol_metadata_t, sec_trust_t, sec_protocol_verify_complete_t)>;

    HTTPServer(std::initializer_list<std::pair<String, HTTPResponse>>, Protocol = Protocol::Http, CertificateVerifier&& = nullptr, RetainPtr<SecIdentityRef>&& = nullptr, std::optional<uint16_t> port = { });
    HTTPServer(Function<void(Connection)>&&, Protocol = Protocol::Http);
    enum class UseCoroutines : bool { Yes };
    HTTPServer(UseCoroutines, Function<Task(Connection)>&&, Protocol = Protocol::Http);
    ~HTTPServer();
    uint16_t port() const;
    String origin() const;
    NSURLRequest *request(StringView path = "/"_s) const;
    NSURLRequest *requestWithLocalhost(StringView path = "/"_s) const;
    WKWebViewConfiguration *httpsProxyConfiguration() const;
    size_t totalRequests() const;
    void cancel();
    void terminateAllConnections(CompletionHandler<void()>&&);

    void addResponse(String&& path, HTTPResponse&&);
    void setResponse(String&& path, HTTPResponse&&);

    static void respondWithOK(Connection);
    static void respondWithChallengeThenOK(Connection);
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

class Connection {
public:
    void send(String&&, CompletionHandler<void()>&& = nullptr) const;
    void send(Vector<uint8_t>&&, CompletionHandler<void()>&& = nullptr) const;
    void send(RetainPtr<dispatch_data_t>&&, CompletionHandler<void(bool)>&& = nullptr) const;
    SendOperation awaitableSend(Vector<uint8_t>&&);
    SendOperation awaitableSend(String&&);
    void sendAndReportError(Vector<uint8_t>&&, CompletionHandler<void(bool)>&&) const;
    void receiveBytes(CompletionHandler<void(Vector<uint8_t>&&)>&&, size_t minimumSize = 1) const;
    void receiveHTTPRequest(CompletionHandler<void(Vector<char>&&)>&&, Vector<char>&& buffer = { }) const;
    ReceiveOperation awaitableReceiveHTTPRequest() const;
    void webSocketHandshake(CompletionHandler<void()>&& = { });
    void terminate(CompletionHandler<void()>&& = { });
    void cancel();

private:
    friend class HTTPServer;
    Connection(nw_connection_t connection)
        : m_connection(connection) { }

    RetainPtr<nw_connection_t> m_connection;
};

class ReceiveOperation {
public:
    ReceiveOperation(const Connection& connection)
        : m_connection(connection) { }
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<>);
    Vector<char> await_resume() { return WTFMove(m_result); }
private:
    Connection m_connection;
    Vector<char> m_result;
};

class SendOperation {
public:
    SendOperation(RetainPtr<dispatch_data_t>&& data, const Connection& connection)
        : m_data(WTFMove(data))
        , m_connection(connection) { }
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<>);
    void await_resume() { }
private:
    RetainPtr<dispatch_data_t> m_data;
    Connection m_connection;
};

struct HTTPResponse {
    enum class TerminateConnection { No, Yes };

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
    HTTPResponse(TerminateConnection terminateConnection)
        : terminateConnection(terminateConnection) { }

    HTTPResponse(const HTTPResponse&) = default;
    HTTPResponse(HTTPResponse&&) = default;
    HTTPResponse() = default;
    HTTPResponse& operator=(const HTTPResponse&) = default;
    HTTPResponse& operator=(HTTPResponse&&) = default;

    enum class IncludeContentLength : bool { No, Yes };
    Vector<uint8_t> serialize(IncludeContentLength = IncludeContentLength::Yes) const;
    static Vector<uint8_t> bodyFromString(const String&);

    unsigned statusCode { 200 };
    HashMap<String, String> headerFields;
    Vector<uint8_t> body;
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

RetainPtr<SecCertificateRef> testCertificate();
RetainPtr<SecIdentityRef> testIdentity();
RetainPtr<SecIdentityRef> testIdentity2();
