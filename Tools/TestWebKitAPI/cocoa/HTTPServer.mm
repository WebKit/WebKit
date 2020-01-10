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

#import <wtf/text/StringBuilder.h>
#import <wtf/text/WTFString.h>

namespace TestWebKitAPI {

HTTPServer::HTTPServer(std::initializer_list<std::pair<String, HTTPResponse>> responses)
    : m_requestResponseMap([](std::initializer_list<std::pair<String, HTTPServer::HTTPResponse>> list) {
        HashMap<String, HTTPServer::HTTPResponse> map;
        for (auto& pair : list)
            map.add(pair.first, pair.second);
        return map;
    }(responses))
{
    auto parameters = adoptNS(nw_parameters_create_secure_tcp(NW_PARAMETERS_DISABLE_PROTOCOL, NW_PARAMETERS_DEFAULT_CONFIGURATION));
    m_listener = adoptNS(nw_listener_create(parameters.get()));
    nw_listener_set_queue(m_listener.get(), dispatch_get_main_queue());
    nw_listener_set_new_connection_handler(m_listener.get(), ^(nw_connection_t connection) {
        nw_connection_set_queue(connection, dispatch_get_main_queue());
        nw_connection_start(connection);
        respondToRequests(connection);
    });
    nw_listener_start(m_listener.get());
}

static String statusText(unsigned statusCode)
{
    switch (statusCode) {
    case 200:
        return "OK"_s;
    case 301:
        return "Moved Permanently"_s;
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
        request.append(0);

        const char* getPathPrefix = "GET ";
        const char* postPathPrefix = "POST ";
        const char* pathSuffix = " HTTP/1.1\r\n";
        const char* pathEnd = strstr(request.data(), pathSuffix);
        ASSERT_WITH_MESSAGE(pathEnd, "HTTPServer assumes request is HTTP 1.1");
        ASSERT_WITH_MESSAGE(strstr(request.data(), "\r\n\r\n"), "HTTPServer assumes entire HTTP request is received at once");
        size_t pathPrefixLength = 0;
        if (!memcmp(request.data(), getPathPrefix, strlen(getPathPrefix)))
            pathPrefixLength = strlen(getPathPrefix);
        else if (!memcmp(request.data(), postPathPrefix, strlen(postPathPrefix)))
            pathPrefixLength = strlen(postPathPrefix);
        ASSERT_WITH_MESSAGE(pathPrefixLength, "HTTPServer assumes request is GET or POST");
        size_t pathLength = pathEnd - request.data() - pathPrefixLength;
        String path(request.data() + pathPrefixLength, pathLength);
        ASSERT_WITH_MESSAGE(m_requestResponseMap.contains(path), "This HTTPServer does not know how to respond to a request for %s", path.utf8().data());
        
        auto response = m_requestResponseMap.get(path);
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
        nw_connection_send(connection, responseData.get(), context, true, ^(nw_error_t error) {
            ASSERT(!error);
            respondToRequests(connection);
        });
    });
}

uint16_t HTTPServer::port() const
{
    return nw_listener_get_port(m_listener.get());
}

NSURLRequest *HTTPServer::request() const
{
    return [NSURLRequest requestWithURL:[NSURL URLWithString:[NSString stringWithFormat:@"http://127.0.0.1:%d/", port()]]];
}

} // namespace TestWebKitAPI

#endif // HAVE(NETWORK_FRAMEWORK)
