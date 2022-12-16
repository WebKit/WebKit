/*
 * Copyright (C) 2019-2022 Apple Inc. All rights reserved.
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

#import "Utilities.h"
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/BlockPtr.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/CompletionHandler.h>
#import <wtf/RetainPtr.h>
#import <wtf/SHA1.h>
#import <wtf/ThreadSafeRefCounted.h>
#import <wtf/text/Base64.h>
#import <wtf/text/StringBuilder.h>
#import <wtf/text/WTFString.h>

namespace TestWebKitAPI {

struct HTTPServer::RequestData : public ThreadSafeRefCounted<RequestData, WTF::DestructionThread::MainRunLoop> {
    RequestData(std::initializer_list<std::pair<String, HTTPResponse>> responses)
    : requestMap([](std::initializer_list<std::pair<String, HTTPResponse>> list) {
        HashMap<String, HTTPResponse> map;
        for (auto& pair : list)
            map.add(pair.first, pair.second);
        return map;
    }(responses)) { }

    size_t requestCount { 0 };
    HashMap<String, HTTPResponse> requestMap;
    Vector<Connection> connections;
    Vector<CoroutineHandle<Task::promise_type>> coroutineHandles;
};

static RetainPtr<nw_protocol_definition_t> proxyDefinition(HTTPServer::Protocol protocol)
{
    return adoptNS(nw_framer_create_definition("HttpsProxy", NW_FRAMER_CREATE_FLAGS_DEFAULT, [protocol] (nw_framer_t framer) -> nw_framer_start_result_t {

        __block enum class State {
            WillRequestCredentials,
            DidRequestCredentials,
            WillNotRequestCredentials,
            PassThrough
        } state { protocol == HTTPServer::Protocol::HttpsProxyWithAuthentication ? State::WillRequestCredentials : State::WillNotRequestCredentials };

        nw_framer_set_input_handler(framer, ^size_t(nw_framer_t framer) {
            __block RetainPtr<nw_framer_t> retainedFramer = framer;
            nw_framer_pass_through_output(framer);
            nw_framer_parse_input(framer, 1, std::numeric_limits<uint32_t>::max(), nullptr, ^size_t(uint8_t* buffer, size_t bufferLength, bool isComplete) {
                switch (state) {
                case State::WillRequestCredentials: {
                    const char* challengeResponse =
                        "HTTP/1.1 407 Proxy Authentication Required\r\n"
                        "Proxy-Authenticate: Basic realm=\"testrealm\"\r\n"
                        "Content-Length: 0\r\n"
                        "\r\n";
                    auto response = adoptNS(dispatch_data_create(challengeResponse, strlen(challengeResponse), nullptr, nullptr));
                    nw_framer_write_output_data(retainedFramer.get(), response.get());
                    state = State::DidRequestCredentials;
                    break;
                }
                case State::DidRequestCredentials:
                    EXPECT_TRUE(strnstr(reinterpret_cast<const char*>(buffer), "Proxy-Authorization: Basic dGVzdHVzZXI6dGVzdHBhc3N3b3Jk\r\n", bufferLength));
                    FALLTHROUGH;
                case State::WillNotRequestCredentials: {
                    const char* negotiationResponse = ""
                        "HTTP/1.1 200 Connection Established\r\n"
                        "Connection: close\r\n\r\n";
                    auto response = adoptNS(dispatch_data_create(negotiationResponse, strlen(negotiationResponse), nullptr, nullptr));
                    nw_framer_write_output_data(retainedFramer.get(), response.get());
                    nw_framer_mark_ready(retainedFramer.get());
                    state = State::PassThrough;
                    break;
                }
                case State::PassThrough:
                    nw_framer_deliver_input_no_copy(retainedFramer.get(), bufferLength, adoptNS(nw_framer_message_create(retainedFramer.get())).get(), isComplete);
                    return 0;
                }
                return bufferLength;
            });
            return 0;
        });
        return nw_framer_start_result_will_mark_ready;
    }));
}

static bool shouldDisableTLS(HTTPServer::Protocol protocol)
{
    switch (protocol) {
    case HTTPServer::Protocol::Http:
    case HTTPServer::Protocol::HttpsProxy:
    case HTTPServer::Protocol::HttpsProxyWithAuthentication:
        return true;
    case HTTPServer::Protocol::Https:
    case HTTPServer::Protocol::HttpsWithLegacyTLS:
    case HTTPServer::Protocol::Http2:
        return false;
    }
}

RetainPtr<nw_parameters_t> HTTPServer::listenerParameters(Protocol protocol, CertificateVerifier&& verifier, RetainPtr<SecIdentityRef>&& customTestIdentity, std::optional<uint16_t> port)
{
    if (protocol != Protocol::Http && !customTestIdentity)
        customTestIdentity = testIdentity();

    auto configureTLS = [protocol, verifier = WTFMove(verifier), testIdentity = WTFMove(customTestIdentity)] (nw_protocol_options_t protocolOptions) mutable {
        auto options = adoptNS(nw_tls_copy_sec_protocol_options(protocolOptions));
        auto identity = adoptNS(sec_identity_create(testIdentity.get()));
        sec_protocol_options_set_local_identity(options.get(), identity.get());
        if (protocol == Protocol::HttpsWithLegacyTLS)
            sec_protocol_options_set_max_tls_protocol_version(options.get(), tls_protocol_version_TLSv10);
        if (verifier) {
            sec_protocol_options_set_peer_authentication_required(options.get(), true);
            sec_protocol_options_set_verify_block(options.get(), makeBlockPtr([verifier = WTFMove(verifier)](sec_protocol_metadata_t metadata, sec_trust_t trust, sec_protocol_verify_complete_t completion) {
                verifier(metadata, trust, completion);
            }).get(), dispatch_get_main_queue());
        }
        if (protocol == Protocol::Http2)
            sec_protocol_options_add_tls_application_protocol(options.get(), "h2");
    };

    auto configureTLSBlock = shouldDisableTLS(protocol) ? makeBlockPtr(NW_PARAMETERS_DISABLE_PROTOCOL) : makeBlockPtr(WTFMove(configureTLS));
    auto parameters = adoptNS(nw_parameters_create_secure_tcp(configureTLSBlock.get(), NW_PARAMETERS_DEFAULT_CONFIGURATION));
    if (port)
        nw_parameters_set_local_endpoint(parameters.get(), nw_endpoint_create_host("::", makeString(*port).utf8().data()));

    if (protocol == Protocol::HttpsProxy || protocol == Protocol::HttpsProxyWithAuthentication) {
        auto stack = adoptNS(nw_parameters_copy_default_protocol_stack(parameters.get()));
        auto options = adoptNS(nw_framer_create_options(proxyDefinition(protocol).get()));
        nw_protocol_stack_prepend_application_protocol(stack.get(), options.get());

        auto tlsOptions = adoptNS(nw_tls_create_options());
        configureTLS(tlsOptions.get());
        nw_protocol_stack_prepend_application_protocol(stack.get(), tlsOptions.get());
    }

    return parameters;
}

static void startListening(nw_listener_t listener)
{
    __block bool ready = false;
    nw_listener_set_state_changed_handler(listener, ^(nw_listener_state_t state, nw_error_t error) {
        ASSERT_UNUSED(error, !error);
        if (state == nw_listener_state_ready)
            ready = true;
    });
    nw_listener_start(listener);
    Util::run(&ready);
}

void HTTPServer::cancel()
{
    __block bool cancelled = false;
    nw_listener_set_state_changed_handler(m_listener.get(), ^(nw_listener_state_t state, nw_error_t error) {
        ASSERT_UNUSED(error, !error);
        if (state == nw_listener_state_cancelled)
            cancelled = true;
    });
    nw_listener_cancel(m_listener.get());
    Util::run(&cancelled);
    m_listener = nullptr;

    bool done { false };
    terminateAllConnections([&] {
        done = true;
    });
    Util::run(&done);
}

void HTTPServer::terminateAllConnections(CompletionHandler<void()>&& completionHandler)
{
    auto aggregator = CallbackAggregator::create(WTFMove(completionHandler));
    for (auto& connection : std::exchange(m_requestData->connections, { }))
        connection.terminate([aggregator] { });
}

HTTPServer::HTTPServer(std::initializer_list<std::pair<String, HTTPResponse>> responses, Protocol protocol, CertificateVerifier&& verifier, RetainPtr<SecIdentityRef>&& identity, std::optional<uint16_t> port)
    : m_requestData(adoptRef(*new RequestData(responses)))
    , m_listener(adoptNS(nw_listener_create(listenerParameters(protocol, WTFMove(verifier), WTFMove(identity), port).get())))
    , m_protocol(protocol)
{
    nw_listener_set_queue(m_listener.get(), dispatch_get_main_queue());
    nw_listener_set_new_connection_handler(m_listener.get(), makeBlockPtr([requestData = m_requestData](nw_connection_t connection) {
        requestData->connections.append(Connection(connection));
        nw_connection_set_queue(connection, dispatch_get_main_queue());
        nw_connection_start(connection);
        respondToRequests(Connection(connection), requestData);
    }).get());
    startListening(m_listener.get());
}

HTTPServer::HTTPServer(Function<void(Connection)>&& connectionHandler, Protocol protocol)
    : m_requestData(adoptRef(*new RequestData({ })))
    , m_listener(adoptNS(nw_listener_create(listenerParameters(protocol, nullptr, nullptr, { }).get())))
    , m_protocol(protocol)
{
    nw_listener_set_queue(m_listener.get(), dispatch_get_main_queue());
    nw_listener_set_new_connection_handler(m_listener.get(), makeBlockPtr([requestData = m_requestData, connectionHandler = WTFMove(connectionHandler)] (nw_connection_t connection) {
        requestData->connections.append(Connection(connection));
        nw_connection_set_queue(connection, dispatch_get_main_queue());
        nw_connection_start(connection);
        connectionHandler(Connection(connection));
    }).get());
    startListening(m_listener.get());
}

HTTPServer::HTTPServer(UseCoroutines, WTF::Function<Task(Connection)>&& connectionHandler, Protocol protocol)
    : m_requestData(adoptRef(*new RequestData({ })))
    , m_listener(adoptNS(nw_listener_create(listenerParameters(protocol, nullptr, nullptr, { }).get())))
    , m_protocol(protocol)
{
    nw_listener_set_queue(m_listener.get(), dispatch_get_main_queue());
    nw_listener_set_new_connection_handler(m_listener.get(), makeBlockPtr([requestData = m_requestData, connectionHandler = WTFMove(connectionHandler)] (nw_connection_t connection) {
        requestData->connections.append(Connection(connection));
        nw_connection_set_queue(connection, dispatch_get_main_queue());
        nw_connection_start(connection);
        requestData->coroutineHandles.append(connectionHandler(Connection(connection)).handle);
    }).get());
    startListening(m_listener.get());
}

HTTPServer::~HTTPServer() = default;

void HTTPServer::addResponse(String&& path, HTTPResponse&& response)
{
    RELEASE_ASSERT(!m_requestData->requestMap.contains(path));
    m_requestData->requestMap.add(WTFMove(path), WTFMove(response));
}

void HTTPServer::setResponse(String&& path, HTTPResponse&& response)
{
    ASSERT(m_requestData->requestMap.contains(path));
    m_requestData->requestMap.set(WTFMove(path), WTFMove(response));
}

void HTTPServer::respondWithChallengeThenOK(Connection connection)
{
    connection.receiveHTTPRequest([connection] (Vector<char>&&) {
        constexpr auto challengeHeader =
        "HTTP/1.1 401 Unauthorized\r\n"
        "Date: Sat, 23 Mar 2019 06:29:01 GMT\r\n"
        "Content-Length: 0\r\n"
        "WWW-Authenticate: Basic realm=\"testrealm\"\r\n\r\n"_s;
        connection.send(challengeHeader, [connection] {
            connection.receiveHTTPRequest([connection] (Vector<char>&&) {
                connection.send(
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: 34\r\n\r\n"
                    "<script>alert('success!')</script>"_s, [connection] {
                        respondWithChallengeThenOK(connection);
                    }
                );
            });
        });
    });
}

void HTTPServer::respondWithOK(Connection connection)
{
    connection.receiveHTTPRequest([connection] (Vector<char>&&) {
        connection.send(
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 34\r\n\r\n"
            "<script>alert('success!')</script>"_s
        );
    });
}

size_t HTTPServer::totalRequests() const
{
    return m_requestData->requestCount;
}

static ASCIILiteral statusText(unsigned statusCode)
{
    switch (statusCode) {
    case 101:
        return "Switching Protocols"_s;
    case 200:
        return "OK"_s;
    case 204:
        return "No Content"_s;
    case 301:
        return "Moved Permanently"_s;
    case 302:
        return "Found"_s;
    case 404:
        return "Not Found"_s;
    case 503:
        return "Service Unavailable"_s;
    }
    ASSERT_NOT_REACHED();
    return "Unknown Status Code"_s;
}

static RetainPtr<dispatch_data_t> dataFromString(String&& s)
{
    auto impl = s.releaseImpl();
    ASSERT(impl->is8Bit());
    return adoptNS(dispatch_data_create(impl->characters8(), impl->length(), dispatch_get_main_queue(), ^{
        (void)impl;
    }));
}

static RetainPtr<dispatch_data_t> dataFromVector(Vector<uint8_t>&& v)
{
    auto bufferSize = v.size();
    auto rawPointer = v.releaseBuffer().leakPtr();
    return adoptNS(dispatch_data_create(rawPointer, bufferSize, dispatch_get_main_queue(), ^{
        fastFree(rawPointer);
    }));
}

static Vector<uint8_t> vectorFromData(dispatch_data_t content)
{
    ASSERT(content);
    __block Vector<uint8_t> request;
    dispatch_data_apply(content, ^bool(dispatch_data_t, size_t, const void* buffer, size_t size) {
        request.append(static_cast<const char*>(buffer), size);
        return true;
    });
    return request;
}

static void appendUTF8ToVector(Vector<uint8_t>& vector, const String& string)
{
    auto utf8 = string.utf8();
    vector.append(reinterpret_cast<const uint8_t*>(utf8.data()), utf8.length());
}

String HTTPServer::parsePath(const Vector<char>& request)
{
    if (!request.size())
        return { };
    const char* getPathPrefix = "GET ";
    const char* postPathPrefix = "POST ";
    const char* pathSuffix = " HTTP/1.1\r\n";
    const char* pathEnd = strnstr(request.data(), pathSuffix, request.size());
    ASSERT_WITH_MESSAGE(pathEnd, "HTTPServer assumes request is HTTP 1.1");
    size_t pathPrefixLength = 0;
    if (!memcmp(request.data(), getPathPrefix, strlen(getPathPrefix)))
        pathPrefixLength = strlen(getPathPrefix);
    else if (!memcmp(request.data(), postPathPrefix, strlen(postPathPrefix)))
        pathPrefixLength = strlen(postPathPrefix);
    ASSERT_WITH_MESSAGE(pathPrefixLength, "HTTPServer assumes request is GET or POST");
    size_t pathLength = pathEnd - request.data() - pathPrefixLength;
    return String(request.data() + pathPrefixLength, pathLength);
}

String HTTPServer::parseBody(const Vector<char>& request)
{
    const char* headerEndBytes = "\r\n\r\n";
    const char* headerEnd = strnstr(request.data(), headerEndBytes, request.size()) + strlen(headerEndBytes);
    size_t headerLength = headerEnd - request.data();
    return String(headerEnd, request.size() - headerLength);
}

void HTTPServer::respondToRequests(Connection connection, Ref<RequestData> requestData)
{
    connection.receiveHTTPRequest([connection, requestData] (Vector<char>&& request) mutable {
        if (!request.size())
            return;
        requestData->requestCount++;

        auto path = parsePath(request);
        ASSERT_WITH_MESSAGE(requestData->requestMap.contains(path), "This HTTPServer does not know how to respond to a request for %s", path.utf8().data());

        auto response = requestData->requestMap.get(path);
        if (response.terminateConnection == HTTPResponse::TerminateConnection::Yes)
            return connection.terminate();

        connection.send(response.serialize(), [connection, requestData] {
            respondToRequests(connection, requestData);
        });
    });
}

uint16_t HTTPServer::port() const
{
    return nw_listener_get_port(m_listener.get());
}

const char* HTTPServer::scheme() const
{
    const char* scheme = nullptr;
    switch (m_protocol) {
    case Protocol::Http:
        scheme = "http";
        break;
    case Protocol::Https:
    case Protocol::HttpsWithLegacyTLS:
    case Protocol::Http2:
        scheme = "https";
        break;
    case Protocol::HttpsProxy:
    case Protocol::HttpsProxyWithAuthentication:
        RELEASE_ASSERT_NOT_REACHED();
    }
    return scheme;
}

String HTTPServer::origin() const
{
    return [NSString stringWithFormat:@"%s://127.0.0.1:%d", scheme(), port()];
}

NSURLRequest *HTTPServer::request(StringView path) const
{
    return [NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"%s://127.0.0.1:%d%@", scheme(), port(), path.createNSString().get()]]];
}

NSURLRequest *HTTPServer::requestWithLocalhost(StringView path) const
{
    return [NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"%s://localhost:%d%@", scheme(), port(), path.createNSString().get()]]];
}

WKWebViewConfiguration *HTTPServer::httpsProxyConfiguration() const
{
    auto storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", port()]]];
    auto viewConfiguration = adoptNS([WKWebViewConfiguration new]);
    [viewConfiguration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    return viewConfiguration.autorelease();
}

void Connection::receiveBytes(CompletionHandler<void(Vector<uint8_t>&&)>&& completionHandler, size_t minimumSize) const
{
    nw_connection_receive(m_connection.get(), minimumSize, std::numeric_limits<uint32_t>::max(), makeBlockPtr([connection = *this, completionHandler = WTFMove(completionHandler)](dispatch_data_t content, nw_content_context_t, bool, nw_error_t error) mutable {
        if (error || !content)
            return completionHandler({ });
        completionHandler(vectorFromData(content));
    }).get());
}

void Connection::receiveHTTPRequest(CompletionHandler<void(Vector<char>&&)>&& completionHandler, Vector<char>&& buffer) const
{
    receiveBytes([connection = *this, completionHandler = WTFMove(completionHandler), buffer = WTFMove(buffer)](Vector<uint8_t>&& bytes) mutable {
        buffer.appendVector(WTFMove(bytes));
        if (auto* doubleNewline = strnstr(buffer.data(), "\r\n\r\n", buffer.size())) {
            if (auto* contentLengthBegin = strnstr(buffer.data(), "Content-Length", buffer.size())) {
                size_t contentLength = atoi(contentLengthBegin + strlen("Content-Length: "));
                size_t headerLength = doubleNewline - buffer.data() + strlen("\r\n\r\n");
                if (buffer.size() - headerLength < contentLength)
                    return connection.receiveHTTPRequest(WTFMove(completionHandler), WTFMove(buffer));
            }
            completionHandler(WTFMove(buffer));
        } else
            connection.receiveHTTPRequest(WTFMove(completionHandler), WTFMove(buffer));
    });
}

ReceiveOperation Connection::awaitableReceiveHTTPRequest() const
{
    return { *this };
}

void ReceiveOperation::await_suspend(std::coroutine_handle<> handle)
{
    m_connection.receiveHTTPRequest([this, handle](Vector<char>&& result) mutable {
        m_result = WTFMove(result);
        handle();
    });
}

void SendOperation::await_suspend(std::coroutine_handle<> handle)
{
    m_connection.send(WTFMove(m_data), [handle] (bool) mutable {
        handle();
    });
}

SendOperation Connection::awaitableSend(Vector<uint8_t>&& message)
{
    return { dataFromVector(WTFMove(message)), *this };
}

SendOperation Connection::awaitableSend(String&& message)
{
    return { dataFromString(WTFMove(message)), *this };
}

void Connection::send(String&& message, CompletionHandler<void()>&& completionHandler) const
{
    send(dataFromString(WTFMove(message)), [completionHandler = WTFMove(completionHandler)] (bool) mutable {
        if (completionHandler)
            completionHandler();
    });
}

void Connection::send(Vector<uint8_t>&& message, CompletionHandler<void()>&& completionHandler) const
{
    send(dataFromVector(WTFMove(message)), [completionHandler = WTFMove(completionHandler)] (bool) mutable {
        if (completionHandler)
            completionHandler();
    });
}

void Connection::sendAndReportError(Vector<uint8_t>&& message, CompletionHandler<void(bool)>&& completionHandler) const
{
    send(dataFromVector(WTFMove(message)), WTFMove(completionHandler));
}

void Connection::send(RetainPtr<dispatch_data_t>&& message, CompletionHandler<void(bool)>&& completionHandler) const
{
    nw_connection_send(m_connection.get(), message.get(), NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, true, makeBlockPtr([completionHandler = WTFMove(completionHandler)](nw_error_t error) mutable {
        if (completionHandler)
            completionHandler(!!error);
    }).get());
}

void Connection::webSocketHandshake(CompletionHandler<void()>&& connectionHandler)
{
    receiveHTTPRequest([connection = Connection(*this), connectionHandler = WTFMove(connectionHandler)] (Vector<char>&& request) mutable {

        auto webSocketAcceptValue = [] (const Vector<char>& request) {
            constexpr auto* keyHeaderField = "Sec-WebSocket-Key: ";
            const char* keyBegin = strnstr(request.data(), keyHeaderField, request.size()) + strlen(keyHeaderField);
            ASSERT(keyBegin);
            const char* keyEnd = strnstr(keyBegin, "\r\n", request.size() + (keyBegin - request.data()));
            ASSERT(keyEnd);

            constexpr auto* webSocketKeyGUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
            SHA1 sha1;
            sha1.addBytes(reinterpret_cast<const uint8_t*>(keyBegin), keyEnd - keyBegin);
            sha1.addBytes(reinterpret_cast<const uint8_t*>(webSocketKeyGUID), strlen(webSocketKeyGUID));
            SHA1::Digest hash;
            sha1.computeHash(hash);
            return base64EncodeToString(hash.data(), SHA1::hashSize);
        };

        connection.send(HTTPResponse(101, {
            { "Upgrade"_s, "websocket"_s },
            { "Connection"_s, "Upgrade"_s },
            { "Sec-WebSocket-Accept"_s, webSocketAcceptValue(request) }
        }).serialize(HTTPResponse::IncludeContentLength::No), WTFMove(connectionHandler));
    });
}

void Connection::terminate(CompletionHandler<void()>&& completionHandler)
{
    nw_connection_set_state_changed_handler(m_connection.get(), makeBlockPtr([completionHandler = WTFMove(completionHandler)] (nw_connection_state_t state, nw_error_t error) mutable {
        ASSERT_UNUSED(error, !error);
        if (state == nw_connection_state_cancelled && completionHandler)
            completionHandler();
    }).get());
    nw_connection_cancel(m_connection.get());
}

Vector<uint8_t> HTTPResponse::bodyFromString(const String& string)
{
    Vector<uint8_t> vector;
    appendUTF8ToVector(vector, string);
    return vector;
}

Vector<uint8_t> HTTPResponse::serialize(IncludeContentLength includeContentLength) const
{
    StringBuilder responseBuilder;
    responseBuilder.append("HTTP/1.1 ", statusCode, ' ', statusText(statusCode), "\r\n");
    if (includeContentLength == IncludeContentLength::Yes)
        responseBuilder.append("Content-Length: ", body.size(), "\r\n");
    for (auto& pair : headerFields)
        responseBuilder.append(pair.key, ": ", pair.value, "\r\n");
    responseBuilder.append("\r\n");
    
    Vector<uint8_t> bytesToSend;
    appendUTF8ToVector(bytesToSend, responseBuilder.toString());
    bytesToSend.appendVector(body);
    return bytesToSend;
}

void H2::Connection::send(Frame&& frame, CompletionHandler<void()>&& completionHandler) const
{
    auto frameType = frame.type();
    auto sendFrame = [tlsConnection = m_tlsConnection, frame = WTFMove(frame), completionHandler = WTFMove(completionHandler)] () mutable {
        // https://http2.github.io/http2-spec/#rfc.section.4.1
        Vector<uint8_t> bytes;
        constexpr size_t frameHeaderLength = 9;
        bytes.reserveInitialCapacity(frameHeaderLength + frame.payload().size());
        bytes.uncheckedAppend(frame.payload().size() >> 16);
        bytes.uncheckedAppend(frame.payload().size() >> 8);
        bytes.uncheckedAppend(frame.payload().size() >> 0);
        bytes.uncheckedAppend(static_cast<uint8_t>(frame.type()));
        bytes.uncheckedAppend(frame.flags());
        bytes.uncheckedAppend(frame.streamID() >> 24);
        bytes.uncheckedAppend(frame.streamID() >> 16);
        bytes.uncheckedAppend(frame.streamID() >> 8);
        bytes.uncheckedAppend(frame.streamID() >> 0);
        bytes.appendVector(frame.payload());
        tlsConnection.send(WTFMove(bytes), WTFMove(completionHandler));
    };

    if (m_sendServerConnectionPreface && frameType != Frame::Type::Settings) {
        // https://http2.github.io/http2-spec/#rfc.section.3.5
        m_sendServerConnectionPreface = false;
        send(Frame(Frame::Type::Settings, 0, 0, { }), WTFMove(sendFrame));
    } else
        sendFrame();
}

void H2::Connection::receive(CompletionHandler<void(Frame&&)>&& completionHandler) const
{
    if (m_expectClientConnectionPreface) {
        // https://http2.github.io/http2-spec/#rfc.section.3.5
        constexpr size_t clientConnectionPrefaceLength = 24;
        if (m_receiveBuffer.size() < clientConnectionPrefaceLength) {
            m_tlsConnection.receiveBytes([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] (Vector<uint8_t>&& bytes) mutable {
                m_receiveBuffer.appendVector(bytes);
                receive(WTFMove(completionHandler));
            });
            return;
        }
        ASSERT(!memcmp(m_receiveBuffer.data(), "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n", clientConnectionPrefaceLength));
        m_receiveBuffer.remove(0, clientConnectionPrefaceLength);
        m_expectClientConnectionPreface = false;
        return receive(WTFMove(completionHandler));
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
            payload.append(m_receiveBuffer.data() + frameHeaderLength, payloadLength);
            m_receiveBuffer.remove(0, frameHeaderLength + payloadLength);
            return completionHandler(Frame(type, flags, streamID, WTFMove(payload)));
        }
    }
    
    m_tlsConnection.receiveBytes([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] (Vector<uint8_t>&& bytes) mutable {
        m_receiveBuffer.appendVector(bytes);
        receive(WTFMove(completionHandler));
    });
}

Vector<uint8_t> HTTPServer::testCertificate()
{
    // Certificate and private key were generated by running this command:
    // openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -days 365 -nodes
    // and entering this information:
    /*
     Country Name (2 letter code) []:US
     State or Province Name (full name) []:New Mexico
     Locality Name (eg, city) []:Santa Fe
     Organization Name (eg, company) []:Self
     Organizational Unit Name (eg, section) []:Myself
     Common Name (eg, fully qualified host name) []:Me
     Email Address []:me@example.com
     */
    
    String pemEncodedCertificate(""
    "MIIFgDCCA2gCCQCKHiPRU5MQuDANBgkqhkiG9w0BAQsFADCBgTELMAkGA1UEBhMC"
    "VVMxEzARBgNVBAgMCk5ldyBNZXhpY28xETAPBgNVBAcMCFNhbnRhIEZlMQ0wCwYD"
    "VQQKDARTZWxmMQ8wDQYDVQQLDAZNeXNlbGYxCzAJBgNVBAMMAk1lMR0wGwYJKoZI"
    "hvcNAQkBFg5tZUBleGFtcGxlLmNvbTAeFw0xOTAzMjMwNTUwMTRaFw0yMDAzMjIw"
    "NTUwMTRaMIGBMQswCQYDVQQGEwJVUzETMBEGA1UECAwKTmV3IE1leGljbzERMA8G"
    "A1UEBwwIU2FudGEgRmUxDTALBgNVBAoMBFNlbGYxDzANBgNVBAsMBk15c2VsZjEL"
    "MAkGA1UEAwwCTWUxHTAbBgkqhkiG9w0BCQEWDm1lQGV4YW1wbGUuY29tMIICIjAN"
    "BgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEA3rhN4SPg8VY/PtGDNKY3T9JISgby"
    "8YGMJx0vO+YZFZm3G3fsTUsyvDyEHwqp5abCZRB/By1PwWkNrfxn/XP8P034JPlE"
    "6irViuAYQrqUh6k7ZR8CpOM5GEcRZgAUJGGQwNlOkEwaHnMGc8SsHurgDPh5XBpg"
    "bDytd7BJuB1NoI/KJmhcajkAuV3varS+uPLofPHNqe+cL8hNnjZQwHWarP45ks4e"
    "BcOD7twqxuHnVm/FWErpY8Ws5s1MrPThUdDahjEMf+YfDJ9KL8y304yS8J8feCxY"
    "fcH4BvgLtJmBNHJgj3eND/EMZjJgz2FsBjrJk8kKD31cw+4Wp8UF4skWXCf46+mN"
    "OHp13PeSCZLyF4ZAHazUVknDPcc2YNrWVV1i6n3T15kI0T5Z7bstdmALuSkE2cuJ"
    "SVNO6gR+ZsVRTneuQxwWTU0MNEhAPFOX2BhGP5eisgEUzknxMJddFDn9Wxklu1Jh"
    "gkzASA/+3AmlrFZMPhOhjEul0zjgNR5RBl1G8Hz92LAx5UEDBtdLg71I+I8AzQOh"
    "d6LtBekECxA16pSappg5vcW9Z/8N6ZlsHnZ2FztA0nCOflkoO9iejOpcuFN4EVYD"
    "xItwctKw1LCeND/s4kmoRRnXbX7k9O6cI1UUWM595Gsu5tPa33M5AZFCav2gOVuY"
    "djppS0HOfo5hv6cCAwEAATANBgkqhkiG9w0BAQsFAAOCAgEAY8EWaAFEfw7OV+oD"
    "XUZSIYXq3EH2E5p3q38AhIOLRjBuB+utyu7Q6rxMMHuw2TtsN+zbAR7yrjfsseA3"
    "4TM1xe4Nk7NVNHRoZQ+C0Iqf9fvcioMvT1tTrma0MhKSjFQpx+PvyLVbD7YdP86L"
    "meehKqU7h1pLGAiGwjoaZ9Ybh6Kuq/MTAHy3D8+wk7B36VBxF6diVlUPZJZQWKJy"
    "MKy9G3sze1ZGt9WeE0AMvkN2HIef0HTKCUZ3eBvecOMijxL0WhWo5Qyf5k6ylCaU"
    "2fx+M8DfDcwFo7tSgLxSK3GCFpxPfiDt6Qk8c9tQn5S1gY3t6LJuwVCFwUIXlNkB"
    "JD7+cZ1Z/tCrEhzj3YCk0uUU8CifoU+4FG+HGFP+SPztsYE055mSj3+Esh+oyoVB"
    "gBH90sE2T1i0eNI8f61oSgwYFeHsf7fC71XEXLFR+GwNdmwqlmwlDZEpTu7BoNN+"
    "q7+Tfk1MRkJlL1PH6Yu/IPhZiNh4tyIqDOtlYfzp577A+OUU+q5PPRFRIsqheOxt"
    "mNlHx4Uzd4U3ITfmogJazjqwYO2viBZY4jUQmyZs75eH/jiUFHWRsha3AdnW5LWa"
    "G3PFnYbW8urH0NSJG/W+/9DA+Y7Aa0cs4TPpuBGZ0NU1W94OoCMo4lkO6H/y6Leu"
    "3vjZD3y9kZk7mre9XHwkI8MdK5s="_s);
    
    auto decodedCertificate = base64Decode(pemEncodedCertificate);
    return WTFMove(*decodedCertificate);
}

Vector<uint8_t> HTTPServer::testPrivateKey()
{
    String pemEncodedPrivateKey(""
    "MIIJQgIBADANBgkqhkiG9w0BAQEFAASCCSwwggkoAgEAAoICAQDeuE3hI+DxVj8+"
    "0YM0pjdP0khKBvLxgYwnHS875hkVmbcbd+xNSzK8PIQfCqnlpsJlEH8HLU/BaQ2t"
    "/Gf9c/w/Tfgk+UTqKtWK4BhCupSHqTtlHwKk4zkYRxFmABQkYZDA2U6QTBoecwZz"
    "xKwe6uAM+HlcGmBsPK13sEm4HU2gj8omaFxqOQC5Xe9qtL648uh88c2p75wvyE2e"
    "NlDAdZqs/jmSzh4Fw4Pu3CrG4edWb8VYSuljxazmzUys9OFR0NqGMQx/5h8Mn0ov"
    "zLfTjJLwnx94LFh9wfgG+Au0mYE0cmCPd40P8QxmMmDPYWwGOsmTyQoPfVzD7han"
    "xQXiyRZcJ/jr6Y04enXc95IJkvIXhkAdrNRWScM9xzZg2tZVXWLqfdPXmQjRPlnt"
    "uy12YAu5KQTZy4lJU07qBH5mxVFOd65DHBZNTQw0SEA8U5fYGEY/l6KyARTOSfEw"
    "l10UOf1bGSW7UmGCTMBID/7cCaWsVkw+E6GMS6XTOOA1HlEGXUbwfP3YsDHlQQMG"
    "10uDvUj4jwDNA6F3ou0F6QQLEDXqlJqmmDm9xb1n/w3pmWwednYXO0DScI5+WSg7"
    "2J6M6ly4U3gRVgPEi3By0rDUsJ40P+ziSahFGddtfuT07pwjVRRYzn3kay7m09rf"
    "czkBkUJq/aA5W5h2OmlLQc5+jmG/pwIDAQABAoICAGra/Cp/f0Xqvk9ST+Prt2/p"
    "kNtLeDXclLSTcP0JCZHufQaFw+7VnFLpqe4GvLq9Bllcz8VOvQwrbe/CwNW+VxC8"
    "RMjge2rqACgwGhOx1t87l46NkUQw7Ey0lCle8kr+MGgGGoZqrMFdKIRUoMv4nmQ6"
    "tmc1FHv5pLRe9Q+Lp5nYQwGoYmZoUOueoOaOL08m49pGXQkiN8pJDMxSfO3Jvtsu"
    "4cqIb6kOQ/dO1Is1CTvURld1IYLH7YuShi4ZEx2g2ac2Uyvt6YmxxvMmAjBSKpGd"
    "loiepho3/NrDGUKdv3q9QYyzrA8w9GT32LDGqgBXJi1scBI8cExkp6P4iDllhv7s"
    "vZsspvobRJa3O1zk863LHXa24JCnyuzimqezZ2Olh7l4olHoYD6UFC9jfd4KcHRg"
    "1c4syqt/n8AK/1s1eBfS9dzb5Cfjt9MtKYslxvLzq1WwOINwz8rIYuRi0PcLm9hs"
    "l+U0u/zB37eMgv6+iwDXk1fSjbuYsE/bETWYknKGNFFL5JSiKV7WCpmgNTTrrE4K"
    "S8E6hR9uPOAaow7vPCCt4xLX/48l2EI6Zeq6qOpq1lJ2qcy8r4tyuQgNRLQMkZg1"
    "AxQl6vnQ8Cu4iu+NIhef0y9Z7qkfNvZeCj5GlFB9c2YjV8Y2mdWfJB4qWK3Z/+MJ"
    "QOTCKRz7/LxLNBUepRjJAoIBAQD3ZsV5tWU9ZSKcVJ9DC7TZk0P+lhcisZr0nL0t"
    "PQuQO+pHvPI1MqRnNskHJhyPnqVCi+dp89tK/It590ULl8os6UC1FhytBPoT1YPd"
    "WGWep2pOc7bVpi4ip31y+ImfgeZyJtMATdme3kBPAOe5NGE9Gig/l5nqLyb02sd1"
    "QW7O0GdqLx3DpLw4SLlhMf6aE0uGRS8sfB085e4DGn54O2wEVuSZqZl5NNEf35Rz"
    "Xgim3h+RWF1ZFSQzjB/smN0Zh+v3Iz7vEJ1h0ywV6o+GzvHkP9HE6gLIhtyV8OEw"
    "vlyYk1Ga7pUVGRh8o8OMe6RR9DQi7JqC4eI7GckmBzaqzJcDAoIBAQDmde6ATew3"
    "H9bQK6xnbMIncz/COpIISdlcFb23AHGEb4b4VhJFBNwxrNL6tHKSFLeYZFLhTdhx"
    "PfXyULHNf5ozdEkl0WrleroDdogbCyWg5uJp9/Q68sbwbGr8CAlO7ZHYTrjuQf1K"
    "AS9pCm77KP3k2d3UlG+pelDjXLoBziXq0NjxJpMz45vrIx8rSWzFNjMGjXT3fXaS"
    "962k/0AXei5/bfuhBxlm7Pni0bQJIWFkeaUuGlrOaHDRxUiX1r9IZS9wv5lk1Ptg"
    "idpbcWyw18cFGTvjdKhRbZH8EsbmzmNNsCGdgCMqFkKYsW16QKoCj/NAovI3n0qn"
    "6VoRa0sGmTGNAoIBACl/mqZEsBuxSDHy29gSMZ7BXglpQa43HmfjlrPs5nCmLDEm"
    "V3Zm7T7G6MeDNA0/LjdQYlvaZLFaVUb7HCDKsEYCRjFZ6St4hz4mdXz+Y+VN7b4F"
    "GOkTe++iKp/LYsJXtsD1FDWb2WIVo7Hc1AGz8I+gQJoSIuYuTJmLzSM0+5JDUOV1"
    "y8dSbaP/RuEv0qYjkGqQVk5e70SUyOzKV+ZxCThdHvFLiovTOTTgevUzE75xydfG"
    "e7oCmtTurzgvl/69Vu5Ygij1n4CWPHHcq4CQW/DOZ7BhFGBwhrW79voHJF8PbwPO"
    "+0DTudDGY3nAD5sTnF8zUuObYihJtfzj/t59fOMCggEBAIYuuBUASb62zQ4bv5/g"
    "VRM/KSpfi9NDnEjfZ7x7h5zCiuVgx/ZjpAlQRO8vzV18roEOOKtx9cnJd8AEd+Hc"
    "n93BoS1hx0mhsVh+1TRZwyjyBXYJpqwD2wz1Mz1XOIQ6EqbM/yPKTD2gfwg7yO53"
    "qYxrxZsWagVVcG9Q+ARBERatTwLpoN+fcJLxuh4r/Ca/LepsxmOrKzTa/MGK1LhW"
    "rWgIk2/ogEPLSptj2d1PEDO+GAzFz4VKjhW1NlUh9fGi6IJPLHLnBw3odbi0S8KT"
    "gA9Z5+LBc5clotAP5rtQA8Wh/ZCEoPTKTTA2bjW2HMatJcbGmR0FpCQr3AM0Y1SO"
    "MakCggEALru6QZ6YUwJJG45H1eq/rPdDY8tqqjJVViKoBVvzKj/XfJZYEVQiIw5p"
    "uoGhDoyFuFUeIh/d1Jc2Iruy2WjoOkiQYtIugDHHxRrkLdQcjPhlCTCE/mmySJt+"
    "bkUbiHIbQ8dJ5yj8SKr0bHzqEtOy9/JeRjkYGHC6bVWpq5FA2MBhf4dNjJ4UDlnT"
    "vuePcTjr7nnfY1sztvfVl9D8dmgT+TBnOOV6yWj1gm5bS1DxQSLgNmtKxJ8tAh2u"
    "dEObvcpShP22ItOVjSampRuAuRG26ZemEbGCI3J6Mqx3y6m+6HwultsgtdzDgrFe"
    "qJfU8bbdbu2pi47Y4FdJK0HLffl5Rw=="_s);

    auto decodedPrivateKey = base64Decode(pemEncodedPrivateKey);
    return WTFMove(*decodedPrivateKey);
}
    
} // namespace TestWebKitAPI
