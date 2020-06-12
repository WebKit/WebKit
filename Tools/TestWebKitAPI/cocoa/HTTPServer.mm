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

#if HAVE(NETWORK_FRAMEWORK)

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
};

RetainPtr<nw_parameters_t> HTTPServer::listenerParameters(Protocol protocol, CertificateVerifier&& verifier)
{
    auto configureTLS = protocol == Protocol::Http ? makeBlockPtr(NW_PARAMETERS_DISABLE_PROTOCOL) : makeBlockPtr([protocol, verifier = WTFMove(verifier)] (nw_protocol_options_t protocolOptions) mutable {
#if HAVE(TLS_PROTOCOL_VERSION_T)
        auto options = adoptNS(nw_tls_copy_sec_protocol_options(protocolOptions));
        auto identity = adoptNS(sec_identity_create(testIdentity().get()));
        sec_protocol_options_set_local_identity(options.get(), identity.get());
        if (protocol == Protocol::HttpsWithLegacyTLS)
            sec_protocol_options_set_max_tls_protocol_version(options.get(), tls_protocol_version_TLSv10);
        if (verifier) {
            sec_protocol_options_set_peer_authentication_required(options.get(), true);
            sec_protocol_options_set_verify_block(options.get(), makeBlockPtr([verifier = WTFMove(verifier)](sec_protocol_metadata_t metadata, sec_trust_t trust, sec_protocol_verify_complete_t completion) {
                verifier(metadata, trust, completion);
            }).get(), dispatch_get_main_queue());
        }
#else
        UNUSED_PARAM(protocolOptions);
        ASSERT_UNUSED(protocol, protocol != Protocol::HttpsWithLegacyTLS);
#endif
    });
    return adoptNS(nw_parameters_create_secure_tcp(configureTLS.get(), NW_PARAMETERS_DEFAULT_CONFIGURATION));
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

HTTPServer::HTTPServer(std::initializer_list<std::pair<String, HTTPResponse>> responses, Protocol protocol, CertificateVerifier&& verifier)
    : m_requestData(adoptRef(new RequestData(responses)))
    , m_listener(adoptNS(nw_listener_create(listenerParameters(protocol, WTFMove(verifier)).get())))
    , m_protocol(protocol)
{
    nw_listener_set_queue(m_listener.get(), dispatch_get_main_queue());
    nw_listener_set_new_connection_handler(m_listener.get(), makeBlockPtr([requestData = m_requestData](nw_connection_t connection) {
        nw_connection_set_queue(connection, dispatch_get_main_queue());
        nw_connection_start(connection);
        respondToRequests(Connection(connection), requestData);
    }).get());
    startListening(m_listener.get());
}

HTTPServer::HTTPServer(Function<void(Connection)>&& connectionHandler, Protocol protocol)
    : m_listener(adoptNS(nw_listener_create(listenerParameters(protocol, nullptr).get())))
    , m_protocol(protocol)
{
    nw_listener_set_queue(m_listener.get(), dispatch_get_main_queue());
    nw_listener_set_new_connection_handler(m_listener.get(), makeBlockPtr([connectionHandler = WTFMove(connectionHandler)] (nw_connection_t connection) {
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
            connection.receiveHTTPRequest([connection] (Vector<char>&&) {
                connection.send(
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Length: 13\r\n\r\n"
                    "Hello, World!"
                );
            });
        });
    });
}

size_t HTTPServer::totalRequests() const
{
    if (!m_requestData)
        return 0;
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

static Vector<char> vectorFromData(dispatch_data_t content)
{
    __block Vector<char> request;
    dispatch_data_apply(content, ^bool(dispatch_data_t, size_t, const void* buffer, size_t size) {
        request.append(static_cast<const char*>(buffer), size);
        return true;
    });
    return request;
}

void HTTPServer::respondToRequests(Connection connection, RefPtr<RequestData> requestData)
{
    connection.receiveHTTPRequest([connection, requestData] (Vector<char>&& request) {
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
        format = @"https://127.0.0.1:%d%s";
        break;
    }
    return [NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:format, port(), path.utf8().data()]]];
}

void Connection::receiveHTTPRequest(CompletionHandler<void(Vector<char>&&)>&& completionHandler, Vector<char>&& buffer) const
{
    nw_connection_receive(m_connection.get(), 1, std::numeric_limits<uint32_t>::max(), makeBlockPtr([connection = *this, completionHandler = WTFMove(completionHandler), buffer = WTFMove(buffer)](dispatch_data_t content, nw_content_context_t, bool, nw_error_t error) mutable {
        if (error || !content)
            return completionHandler({ });
        buffer.appendVector(vectorFromData(content));
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
    }).get());
}

void Connection::send(String&& message, CompletionHandler<void()>&& completionHandler) const
{
    nw_connection_send(m_connection.get(), dataFromString(WTFMove(message)).get(), NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, true, makeBlockPtr([completionHandler = WTFMove(completionHandler)](nw_error_t error) mutable {
        ASSERT(!error);
        if (completionHandler)
            completionHandler();
    }).get());
}

void Connection::terminate() const
{
    nw_connection_cancel(m_connection.get());
}

} // namespace TestWebKitAPI

#endif // HAVE(NETWORK_FRAMEWORK)
