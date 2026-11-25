/*
 * Copyright (C) 2017 Igalia S.L.
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

#include "config.h"
#include "HTTPServer.h"

#include "CommandResult.h"
#include <wtf/JSONValues.h>

namespace WebDriver {

static StringView hostnameFromHostHeader(StringView hostHeader)
{
    // RFC3986 ipv6 addresses
    if (hostHeader.startsWith('[')) {
        auto end = hostHeader.find(']');
        if (end == notFound)
            return { };
        return hostHeader.substring(1, end - 1);
    }

    auto colon = hostHeader.find(':');
    return colon == notFound ? hostHeader : hostHeader.left(colon);
}

std::expected<void, RefPtr<StringImpl>> HTTPRequestHandler::validateRequestHeaders(StringImpl* hostHeader, StringImpl* originHeader, StringImpl* method, StringImpl* contentTypeHeader) const
{
    // Browsers always send a Host header, so a request without one can't come from a web page.
    // Accepting only localhost and IP addresses by default protects against DNS rebinding attacks.
    if (!!hostHeader) {
        auto hostname = hostnameFromHostHeader(hostHeader);
        bool isAllowedHost = equalLettersIgnoringASCIICase(hostname, "localhost"_s) || URL::hostIsIPAddress(hostname) || std::ranges::any_of(m_allowedHosts, [&](auto& allowedHost) {
            return equalIgnoringASCIICase(allowedHost, hostname);
        });
        if (!isAllowedHost)
            return makeUnexpected("Host header not allowed, use --allow-hosts to accept it"_s);
    }

    // WebDriver clients don't send an Origin header, but browsers do for any cross-site POST request.
    if (!!originHeader) {
        URL origin { originHeader };
        bool isAllowedOrigin = origin.isValid() && std::ranges::any_of(m_allowedOrigins, [&](auto& allowedOrigin) {
            return protocolHostAndPortAreEqual(allowedOrigin, origin);
        });
        if (!isAllowedOrigin)
            return makeUnexpected("Origin header not allowed, use --allow-origins to accept it"_s);
    }

    // These are the only content types a web page can POST cross-site without a CORS preflight.
    if (equal(method, "POST"_s) && !!contentTypeHeader) {
        StringView contentType = contentTypeHeader;
        auto semicolon = contentType.find(';');
        auto mimeType = (semicolon == notFound ? contentType : contentType.left(semicolon)).trim(isASCIIWhitespace<char16_t>);
        if (equalLettersIgnoringASCIICase(mimeType, "application/x-www-form-urlencoded"_s)
            || equalLettersIgnoringASCIICase(mimeType, "multipart/form-data"_s)
            || equalLettersIgnoringASCIICase(mimeType, "text/plain"_s))
            return makeUnexpected("Content-Type header not allowed, use application/json"_s);
    }

    return { };
}

HTTPRequestHandler::Response HTTPRequestHandler::rejectedRequestResponse(StringImpl* reason)
{
    auto result = CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, reason);
    auto errorObject = JSON::Object::create();
    errorObject->setString("error"_si, result.errorString());
    errorObject->setString("message"_si, reason);
    errorObject->setString("stacktrace"_si, emptyString());
    auto responseObject = JSON::Object::create();
    responseObject->setObject("value"_si, WTF::move(errorObject));
    return { result.httpStatusCode(), utf8(responseObject->toJSONString()), "application/json; charset=utf-8"_s };
}

HTTPServer::HTTPServer(HTTPRequestHandler& requestHandler)
    : m_requestHandler(requestHandler)
{
}

} // namespace WebDriver
