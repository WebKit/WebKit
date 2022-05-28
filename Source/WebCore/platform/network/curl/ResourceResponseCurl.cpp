/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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

#if USE(CURL)
#include "ResourceResponse.h"

#include "CurlContext.h"
#include "CurlResponse.h"
#include "HTTPParsers.h"

namespace WebCore {

bool ResourceResponse::isAppendableHeader(const String &key)
{
    static constexpr ASCIILiteral appendableHeaders[] = {
        "access-control-allow-headers"_s,
        "access-control-allow-methods"_s,
        "access-control-allow-origin"_s,
        "access-control-expose-headers"_s,
        "allow"_s,
        "cache-control"_s,
        "connection"_s,
        "content-encoding"_s,
        "content-language"_s,
        "if-match"_s,
        "if-none-match"_s,
        "keep-alive"_s,
        "pragma"_s,
        "proxy-authenticate"_s,
        "public"_s,
        "server"_s,
        "set-cookie"_s,
        "te"_s,
        "trailer"_s,
        "transfer-encoding"_s,
        "upgrade"_s,
        "user-agent"_s,
        "vary"_s,
        "via"_s,
        "warning"_s,
        "www-authenticate"_s
    };

    // Custom headers start with 'X-', and need no further checking.
    if (startsWithLettersIgnoringASCIICase(key, "x-"_s))
        return true;

    for (const auto& header : appendableHeaders) {
        if (equalIgnoringASCIICase(key, header))
            return true;
    }

    return false;
}

ResourceResponse::ResourceResponse(const CurlResponse& response)
    : ResourceResponseBase()
{
    setURL(response.url);
    setExpectedContentLength(response.expectedContentLength);
    setHTTPStatusCode(response.statusCode ? response.statusCode : response.httpConnectCode);

    for (const auto& header : response.headers)
        appendHTTPHeaderField(header);

    switch (response.httpVersion) {
    case CURL_HTTP_VERSION_1_0:
        setHTTPVersion("HTTP/1.0"_s);
        break;
    case CURL_HTTP_VERSION_1_1:
        setHTTPVersion("HTTP/1.1"_s);
        break;
    case CURL_HTTP_VERSION_2_0:
        setHTTPVersion("HTTP/2"_s);
        break;
    case CURL_HTTP_VERSION_NONE:
    default:
        break;
    }

    setMimeType(extractMIMETypeFromMediaType(httpHeaderField(HTTPHeaderName::ContentType)).convertToASCIILowercase());
    setTextEncodingName(extractCharsetFromMediaType(httpHeaderField(HTTPHeaderName::ContentType)).toString());
    setSource(ResourceResponse::Source::Network);
}

void ResourceResponse::appendHTTPHeaderField(const String& header)
{
    auto splitPosition = header.find(':');
    if (splitPosition != notFound) {
        auto key = header.left(splitPosition).stripWhiteSpace();
        auto value = header.substring(splitPosition + 1).stripWhiteSpace();

        if (isAppendableHeader(key))
            addHTTPHeaderField(key, value);
        else
            setHTTPHeaderField(key, value);
    } else if (startsWithLettersIgnoringASCIICase(header, "http"_s)) {
        // This is the first line of the response.
        setStatusLine(header);
    }
}

void ResourceResponse::setStatusLine(StringView header)
{
    auto statusLine = header.stripLeadingAndTrailingMatchedCharacters(isSpaceOrNewline);

    auto httpVersionEndPosition = statusLine.find(' ');
    auto statusCodeEndPosition = notFound;

    // Extract the http version
    if (httpVersionEndPosition != notFound) {
        statusLine = statusLine.substring(httpVersionEndPosition + 1).stripLeadingAndTrailingMatchedCharacters(isSpaceOrNewline);
        statusCodeEndPosition = statusLine.find(' ');
    }

    // Extract the http status text
    if (statusCodeEndPosition != notFound) {
        auto statusText = statusLine.substring(statusCodeEndPosition + 1).stripLeadingAndTrailingMatchedCharacters(isSpaceOrNewline);
        setHTTPStatusText(statusText.toString());
    }
}

void ResourceResponse::setCertificateInfo(CertificateInfo&& certificateInfo)
{
    m_certificateInfo = WTFMove(certificateInfo);
}

String ResourceResponse::platformSuggestedFilename() const
{
    StringView contentDisposition = filenameFromHTTPContentDisposition(httpHeaderField(HTTPHeaderName::ContentDisposition));
    if (contentDisposition.is8Bit())
        return String::fromUTF8WithLatin1Fallback(contentDisposition.characters8(), contentDisposition.length());
    return contentDisposition.toString();
}

bool ResourceResponse::shouldRedirect()
{
    auto statusCode = httpStatusCode();
    if (statusCode < 300 || 400 <= statusCode)
        return false;

    // Some 3xx status codes aren't actually redirects.
    if (statusCode == 300 || statusCode == 304 || statusCode == 305 || statusCode == 306)
        return false;

    if (httpHeaderField(HTTPHeaderName::Location).isEmpty())
        return false;

    return true;
}

bool ResourceResponse::isMovedPermanently() const
{
    return httpStatusCode() == 301;
}

bool ResourceResponse::isFound() const
{
    return httpStatusCode() == 302;
}

bool ResourceResponse::isSeeOther() const
{
    return httpStatusCode() == 303;
}

bool ResourceResponse::isNotModified() const
{
    return httpStatusCode() == 304;
}

bool ResourceResponse::isUnauthorized() const
{
    return httpStatusCode() == 401;
}

bool ResourceResponse::isProxyAuthenticationRequired() const
{
    return httpStatusCode() == 407;
}

}

#endif
