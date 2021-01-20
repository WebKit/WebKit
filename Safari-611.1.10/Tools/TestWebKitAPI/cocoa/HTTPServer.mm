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

#import "Utilities.h"
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>
#import <wtf/RetainPtr.h>
#import <wtf/ThreadSafeRefCounted.h>
#import <wtf/text/StringBuilder.h>
#import <wtf/text/WTFString.h>

namespace TestWebKitAPI {

struct HTTPServer::RequestData : public ThreadSafeRefCounted<RequestData, WTF::DestructionThread::MainRunLoop> {
    RequestData(std::initializer_list<std::pair<String, HTTPResponse>> responses)
    : requestMap([](std::initializer_list<std::pair<String, HTTPServer::HTTPResponse>> list) {
        HashMap<String, HTTPServer::HTTPResponse> map;
        for (auto& pair : list)
            map.add(pair.first, pair.second);
        return map;
    }(responses)) { }
    
    size_t requestCount { 0 };
    const HashMap<String, HTTPResponse> requestMap;
    Vector<Connection> connections;
};

RetainPtr<nw_parameters_t> HTTPServer::listenerParameters(Protocol protocol, CertificateVerifier&& verifier, RetainPtr<SecIdentityRef>&& customTestIdentity, Optional<uint16_t> port)
{
    if (protocol != Protocol::Http && !customTestIdentity)
        customTestIdentity = testIdentity();
    auto configureTLS = protocol == Protocol::Http ? makeBlockPtr(NW_PARAMETERS_DISABLE_PROTOCOL) : makeBlockPtr([protocol, verifier = WTFMove(verifier), testIdentity = WTFMove(customTestIdentity)] (nw_protocol_options_t protocolOptions) mutable {
#if HAVE(TLS_PROTOCOL_VERSION_T)
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
#else
        UNUSED_PARAM(protocolOptions);
        ASSERT_UNUSED(protocol, protocol != Protocol::HttpsWithLegacyTLS);
#endif
    });
    auto parameters = adoptNS(nw_parameters_create_secure_tcp(configureTLS.get(), NW_PARAMETERS_DEFAULT_CONFIGURATION));
    if (port)
        nw_parameters_set_local_endpoint(parameters.get(), nw_endpoint_create_host("::", makeString(*port).utf8().data()));
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
    for (auto& connection : std::exchange(m_requestData->connections, { }))
        connection.cancel();
}

HTTPServer::HTTPServer(std::initializer_list<std::pair<String, HTTPResponse>> responses, Protocol protocol, CertificateVerifier&& verifier, RetainPtr<SecIdentityRef>&& identity, Optional<uint16_t> port)
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

HTTPServer::~HTTPServer() = default;

void HTTPServer::respondWithChallengeThenOK(Connection connection)
{
    connection.receiveHTTPRequest([connection] (Vector<char>&&) {
        const char* challengeHeader =
        "HTTP/1.1 401 Unauthorized\r\n"
        "Date: Sat, 23 Mar 2019 06:29:01 GMT\r\n"
        "Content-Length: 0\r\n"
        "WWW-Authenticate: Basic realm=\"testrealm\"\r\n\r\n";
        connection.send(challengeHeader, [connection] {
            respondWithOK(connection);
        });
    });
}

void HTTPServer::respondWithOK(Connection connection)
{
    connection.receiveHTTPRequest([connection] (Vector<char>&&) {
        connection.send(
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 13\r\n\r\n"
            "Hello, World!"
        );
    });
}

size_t HTTPServer::totalRequests() const
{
    return m_requestData->requestCount;
}

static String statusText(unsigned statusCode)
{
    switch (statusCode) {
    case 200:
        return "OK"_s;
    case 301:
        return "Moved Permanently"_s;
    case 404:
        return "Not Found"_s;
    }
    ASSERT_NOT_REACHED();
    return { };
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

void HTTPServer::respondToRequests(Connection connection, Ref<RequestData> requestData)
{
    connection.receiveHTTPRequest([connection, requestData] (Vector<char>&& request) mutable {
        if (!request.size())
            return;
        requestData->requestCount++;

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
        String path(request.data() + pathPrefixLength, pathLength);
        ASSERT_WITH_MESSAGE(requestData->requestMap.contains(path), "This HTTPServer does not know how to respond to a request for %s", path.utf8().data());

        auto response = requestData->requestMap.get(path);
        if (response.terminateConnection == HTTPResponse::TerminateConnection::Yes)
            return connection.terminate();
        StringBuilder responseBuilder;
        responseBuilder.append("HTTP/1.1 ", response.statusCode, ' ', statusText(response.statusCode), "\r\n");
        responseBuilder.append("Content-Length: ", response.body.length(), "\r\n");
        for (auto& pair : response.headerFields)
            responseBuilder.append(pair.key, ": ", pair.value, "\r\n");
        responseBuilder.append("\r\n");
        responseBuilder.append(response.body);
        connection.send(responseBuilder.toString(), [connection, requestData] {
            respondToRequests(connection, requestData);
        });
    });
}

uint16_t HTTPServer::port() const
{
    return nw_listener_get_port(m_listener.get());
}

NSURLRequest *HTTPServer::request(const String& path) const
{
    NSString *format;
    switch (m_protocol) {
    case Protocol::Http:
        format = @"http://127.0.0.1:%d%s";
        break;
    case Protocol::Https:
    case Protocol::HttpsWithLegacyTLS:
    case Protocol::Http2:
        format = @"https://127.0.0.1:%d%s";
        break;
    }
    return [NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:format, port(), path.utf8().data()]]];
}

void Connection::receiveBytes(CompletionHandler<void(Vector<uint8_t>&&)>&& completionHandler) const
{
    nw_connection_receive(m_connection.get(), 1, std::numeric_limits<uint32_t>::max(), makeBlockPtr([connection = *this, completionHandler = WTFMove(completionHandler)](dispatch_data_t content, nw_content_context_t, bool, nw_error_t error) mutable {
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

void Connection::send(String&& message, CompletionHandler<void()>&& completionHandler) const
{
    send(dataFromString(WTFMove(message)), WTFMove(completionHandler));
}

void Connection::send(Vector<uint8_t>&& message, CompletionHandler<void()>&& completionHandler) const
{
    send(dataFromVector(WTFMove(message)), WTFMove(completionHandler));
}

void Connection::send(RetainPtr<dispatch_data_t>&& message, CompletionHandler<void()>&& completionHandler) const
{
    nw_connection_send(m_connection.get(), message.get(), NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, true, makeBlockPtr([completionHandler = WTFMove(completionHandler)](nw_error_t error) mutable {
        ASSERT(!error);
        if (completionHandler)
            completionHandler();
    }).get());
}

void Connection::terminate()
{
    nw_connection_cancel(m_connection.get());
}

void Connection::cancel()
{
    __block bool cancelled = false;
    nw_connection_set_state_changed_handler(m_connection.get(), ^(nw_connection_state_t state, nw_error_t error) {
        ASSERT_UNUSED(error, !error);
        if (state == nw_connection_state_cancelled)
            cancelled = true;
    });
    nw_connection_cancel(m_connection.get());
    Util::run(&cancelled);
    m_connection = nullptr;
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
            m_tlsConnection.receiveBytes([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)] (Vector<uint8_t>&& bytes) mutable {
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
    
    m_tlsConnection.receiveBytes([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)] (Vector<uint8_t>&& bytes) mutable {
        m_receiveBuffer.appendVector(bytes);
        receive(WTFMove(completionHandler));
    });
}

} // namespace TestWebKitAPI
