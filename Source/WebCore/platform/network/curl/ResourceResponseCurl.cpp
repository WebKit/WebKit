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

static bool isAppendableHeader(const String &key)
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
        "timing-allow-origin"_s,
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

ResourceResponse::ResourceResponse(CurlResponse& response)
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
    case CURL_HTTP_VERSION_3:
        setHTTPVersion("HTTP/3"_s);
        break;
    case CURL_HTTP_VERSION_NONE:
    default:
        break;
    }

    setMimeType(AtomString { extractMIMETypeFromMediaType(httpHeaderField(HTTPHeaderName::ContentType)).convertToASCIILowercase() });
    setTextEncodingName(extractCharsetFromMediaType(httpHeaderField(HTTPHeaderName::ContentType)).toAtomString());
    setCertificateInfo(WTFMove(response.certificateInfo));
    setSource(ResourceResponse::Source::Network);
}

void ResourceResponse::appendHTTPHeaderField(const String& header)
{
    if (startsWithLettersIgnoringASCIICase(header, "http/"_s)) {
        setHTTPStatusText(extractReasonPhraseFromHTTPStatusLine(header.trim(deprecatedIsSpaceOrNewline)));
        return;
    }

    if (auto splitPosition = header.find(':'); splitPosition != notFound) {
        auto key = header.left(splitPosition).trim(deprecatedIsSpaceOrNewline);
        auto value = header.substring(splitPosition + 1).trim(deprecatedIsSpaceOrNewline);

        if (isAppendableHeader(key))
            addHTTPHeaderField(key, value);
        else
            setHTTPHeaderField(key, value);
    }
}

String ResourceResponse::platformSuggestedFilename() const
{
    StringView contentDisposition = filenameFromHTTPContentDisposition(httpHeaderField(HTTPHeaderName::ContentDisposition));
    if (contentDisposition.is8Bit())
        return String::fromUTF8WithLatin1Fallback(contentDisposition.characters8(), contentDisposition.length());
    return contentDisposition.toString();
}

}

#endif
