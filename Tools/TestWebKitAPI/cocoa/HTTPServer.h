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

#import <wtf/RetainPtr.h>

#if HAVE(NETWORK_FRAMEWORK)

#import <Network/Network.h>
#import <wtf/Forward.h>
#import <wtf/HashMap.h>
#import <wtf/text/StringHash.h>

namespace TestWebKitAPI {

class HTTPServer {
public:
    struct HTTPResponse;
    enum class Protocol : uint8_t { Http, Https, HttpsWithLegacyTLS };
    HTTPServer(std::initializer_list<std::pair<String, HTTPResponse>>, Protocol = Protocol::Http, Function<void(sec_protocol_metadata_t, sec_trust_t, sec_protocol_verify_complete_t)>&& = nullptr);
    uint16_t port() const;
    NSURLRequest *request() const;
    
private:
    void respondToRequests(nw_connection_t);
    
    RetainPtr<nw_listener_t> m_listener;
    const Protocol m_protocol;
    const Function<void(sec_protocol_metadata_t, sec_trust_t, sec_protocol_verify_complete_t)> m_certVerifier;
    const HashMap<String, HTTPResponse> m_requestResponseMap;
};

struct HTTPServer::HTTPResponse {
    enum class TerminateConnection { No, Yes };

    HTTPResponse(const String& body)
        : body(body) { }
    HTTPResponse(String&& body, HashMap<String, String>&& headerFields)
        : body(WTFMove(body))
        , headerFields(WTFMove(headerFields)) { }
    HTTPResponse(unsigned statusCode, HashMap<String, String>&& headerFields)
        : headerFields(WTFMove(headerFields))
        , statusCode(statusCode) { }
    HTTPResponse(TerminateConnection terminateConnection)
        : terminateConnection(terminateConnection) { }

    HTTPResponse(const HTTPResponse&) = default;
    HTTPResponse(HTTPResponse&&) = default;
    HTTPResponse() = default;
    HTTPResponse& operator=(const HTTPResponse&) = default;
    HTTPResponse& operator=(HTTPResponse&&) = default;
    
    String body;
    HashMap<String, String> headerFields;
    unsigned statusCode { 200 };
    TerminateConnection terminateConnection { TerminateConnection::No };
};

} // namespace TestWebKitAPI

#endif // HAVE(NETWORK_FRAMEWORK)

RetainPtr<SecIdentityRef> testIdentity();
