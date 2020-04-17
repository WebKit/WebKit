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
#import <wtf/text/StringBuilder.h>
#import <wtf/text/WTFString.h>

namespace TestWebKitAPI {

HTTPServer::HTTPServer(std::initializer_list<std::pair<String, HTTPResponse>> responses, Protocol protocol, Function<void(sec_protocol_metadata_t, sec_trust_t, sec_protocol_verify_complete_t)>&& verify)
    : m_protocol(protocol)
    , m_certVerifier(WTFMove(verify))
    , m_requestResponseMap([](std::initializer_list<std::pair<String, HTTPServer::HTTPResponse>> list) {
        HashMap<String, HTTPServer::HTTPResponse> map;
        for (auto& pair : list)
            map.add(pair.first, pair.second);
        return map;
    }(responses))
{
    auto configureTLS = protocol == Protocol::Http ? NW_PARAMETERS_DISABLE_PROTOCOL : ^(nw_protocol_options_t protocolOptions) {
#if HAVE(TLS_PROTOCOL_VERSION_T)
        auto options = adoptNS(nw_tls_copy_sec_protocol_options(protocolOptions));
        auto identity = adoptNS(sec_identity_create(testIdentity().get()));
        sec_protocol_options_set_local_identity(options.get(), identity.get());
        if (protocol == Protocol::HttpsWithLegacyTLS)
            sec_protocol_options_set_max_tls_protocol_version(options.get(), tls_protocol_version_TLSv10);
        if (m_certVerifier) {
            sec_protocol_options_set_peer_authentication_required(options.get(), true);
            sec_protocol_options_set_verify_block(options.get(), ^(sec_protocol_metadata_t metadata, sec_trust_t trust, sec_protocol_verify_complete_t completion) {
                m_certVerifier(metadata, trust, completion);
            }, dispatch_get_main_queue());
        }
#else
        UNUSED_PARAM(protocolOptions);
        ASSERT_UNUSED(protocol, protocol != Protocol::HttpsWithLegacyTLS);
#endif
    };
    auto parameters = adoptNS(nw_parameters_create_secure_tcp(configureTLS, NW_PARAMETERS_DEFAULT_CONFIGURATION));
    m_listener = adoptNS(nw_listener_create(parameters.get()));
    nw_listener_set_queue(m_listener.get(), dispatch_get_main_queue());
    nw_listener_set_new_connection_handler(m_listener.get(), ^(nw_connection_t connection) {
        nw_connection_set_queue(connection, dispatch_get_main_queue());
        nw_connection_start(connection);
        respondToRequests(connection);
    });
    __block bool ready = false;
    nw_listener_set_state_changed_handler(m_listener.get(), ^(nw_listener_state_t state, nw_error_t error) {
        ASSERT_UNUSED(error, !error);
        if (state == nw_listener_state_ready)
            ready = true;
    });
    nw_listener_start(m_listener.get());
    Util::run(&ready);
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

void HTTPServer::respondToRequests(nw_connection_t connection)
{
    nw_connection_receive(connection, 1, std::numeric_limits<uint32_t>::max(), ^(dispatch_data_t content, nw_content_context_t context, bool complete, nw_error_t error) {
        if (error || !content)
            return;
        __block Vector<char> request;
        dispatch_data_apply(content, ^bool(dispatch_data_t, size_t, const void* buffer, size_t size) {
            request.append(static_cast<const char*>(buffer), size);
            return true;
        });
        request.append('\0');

        m_totalRequests++;

        const char* getPathPrefix = "GET ";
        const char* postPathPrefix = "POST ";
        const char* pathSuffix = " HTTP/1.1\r\n";
        const char* pathEnd = strstr(request.data(), pathSuffix);
        ASSERT_WITH_MESSAGE(pathEnd, "HTTPServer assumes request is HTTP 1.1");
        const char* doubleNewline = strstr(request.data(), "\r\n\r\n");
        ASSERT_WITH_MESSAGE(doubleNewline, "HTTPServer assumes entire HTTP request is received at once");
        size_t pathPrefixLength = 0;
        if (!memcmp(request.data(), getPathPrefix, strlen(getPathPrefix)))
            pathPrefixLength = strlen(getPathPrefix);
        else if (!memcmp(request.data(), postPathPrefix, strlen(postPathPrefix)))
            pathPrefixLength = strlen(postPathPrefix);
        ASSERT_WITH_MESSAGE(pathPrefixLength, "HTTPServer assumes request is GET or POST");
        size_t pathLength = pathEnd - request.data() - pathPrefixLength;
        String path(request.data() + pathPrefixLength, pathLength);
        ASSERT_WITH_MESSAGE(m_requestResponseMap.contains(path), "This HTTPServer does not know how to respond to a request for %s", path.utf8().data());

        CompletionHandler<void()> sendResponse = [this, connection = retainPtr(connection), context = retainPtr(context), path = WTFMove(path)] {
            auto response = m_requestResponseMap.get(path);
            if (response.terminateConnection == HTTPResponse::TerminateConnection::Yes) {
                nw_connection_cancel(connection.get());
                return;
            }
            StringBuilder responseBuilder;
            responseBuilder.append("HTTP/1.1 ");
            responseBuilder.appendNumber(response.statusCode);
            responseBuilder.append(' ');
            responseBuilder.append(statusText(response.statusCode));
            responseBuilder.append("\r\nContent-Length: ");
            responseBuilder.appendNumber(response.body.length());
            responseBuilder.append("\r\n");
            for (auto& pair : response.headerFields) {
                responseBuilder.append(pair.key);
                responseBuilder.append(": ");
                responseBuilder.append(pair.value);
                responseBuilder.append("\r\n");
            }
            responseBuilder.append("\r\n");
            responseBuilder.append(response.body);
            auto responseBodyAndHeader = responseBuilder.toString().releaseImpl();
            auto responseData = adoptNS(dispatch_data_create(responseBodyAndHeader->characters8(), responseBodyAndHeader->length(), dispatch_get_main_queue(), ^{
                (void)responseBodyAndHeader;
            }));
            nw_connection_send(connection.get(), responseData.get(), context.get(), true, ^(nw_error_t error) {
                ASSERT(!error);
                respondToRequests(connection.get());
            });
        };

        if (auto* contentLengthBegin = strstr(request.data(), "Content-Length")) {
            size_t contentLength = atoi(contentLengthBegin + strlen("Content-Length: "));
            size_t headerLength = doubleNewline - request.data() + strlen("\r\n\r\n");
            constexpr size_t nullTerminationLength = 1;
            if (request.size() - nullTerminationLength - headerLength < contentLength) {
                nw_connection_receive(connection, 1, std::numeric_limits<uint32_t>::max(), makeBlockPtr([sendResponse = WTFMove(sendResponse)] (dispatch_data_t content, nw_content_context_t context, bool complete, nw_error_t error) mutable {
                    if (error || !content)
                        return;
                    sendResponse();
                }).get());
                return;
            }
        }
        sendResponse();
    });
}

uint16_t HTTPServer::port() const
{
    return nw_listener_get_port(m_listener.get());
}

NSURLRequest *HTTPServer::request() const
{
    NSString *format;
    switch (m_protocol) {
    case Protocol::Http:
        format = @"http://127.0.0.1:%d/";
        break;
    case Protocol::Https:
    case Protocol::HttpsWithLegacyTLS:
        format = @"https://127.0.0.1:%d/";
        break;
    }
    return [NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:format, port()]]];
}

} // namespace TestWebKitAPI

#endif // HAVE(NETWORK_FRAMEWORK)
