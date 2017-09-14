/*
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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

#include "HTTPParsers.h"

namespace WebCore {

bool ResourceResponse::isAppendableHeader(const String &key)
{
    static const char* appendableHeaders[] = {
        "access-control-allow-headers",
        "access-control-allow-methods",
        "access-control-allow-origin",
        "access-control-expose-headers",
        "allow",
        "cache-control",
        "connection",
        "content-encoding",
        "content-language",
        "if-match",
        "if-none-match",
        "keep-alive",
        "pragma",
        "proxy-authenticate",
        "public",
        "server",
        "set-cookie",
        "te",
        "trailer",
        "transfer-encoding",
        "upgrade",
        "user-agent",
        "vary",
        "via",
        "warning",
        "www-authenticate"
    };

    // Custom headers start with 'X-', and need no further checking.
    if (key.startsWith("x-", /* caseSensitive */ false))
        return true;

    for (auto& header : appendableHeaders) {
        if (equalIgnoringASCIICase(key, header))
            return true;
    }

    return false;
}

void ResourceResponse::appendHTTPHeaderField(const String& header)
{
    int splitPosistion = header.find(":");
    if (splitPosistion != notFound) {
        String key = header.left(splitPosistion).stripWhiteSpace();
        String value = header.substring(splitPosistion + 1).stripWhiteSpace();

        if (isAppendableHeader(key))
            addHTTPHeaderField(key, value);
        else
            setHTTPHeaderField(key, value);
    } else if (header.startsWith("HTTP", false)) {
        // This is the first line of the response.
        setStatusLine(header);
    }
}

void ResourceResponse::setStatusLine(const String& header)
{
    String statusLine = header.stripWhiteSpace();

    int httpVersionEndPosition = statusLine.find(" ");
    int statusCodeEndPosition = notFound;

    // Extract the http version
    if (httpVersionEndPosition != notFound) {
        String httpVersion = statusLine.left(httpVersionEndPosition);
        setHTTPVersion(httpVersion.stripWhiteSpace());

        statusLine = statusLine.substring(httpVersionEndPosition + 1).stripWhiteSpace();
        statusCodeEndPosition = statusLine.find(" ");
    }

    // Extract the http status text
    if (statusCodeEndPosition != notFound) {
        String statusText = statusLine.substring(statusCodeEndPosition + 1);
        setHTTPStatusText(statusText.stripWhiteSpace());
    }
}

String ResourceResponse::platformSuggestedFilename() const
{
    return filenameFromHTTPContentDisposition(httpHeaderField(HTTPHeaderName::ContentDisposition));
}

bool ResourceResponse::isRedirection() const
{
    auto statusCode = httpStatusCode();
    return (300 <= statusCode) && (statusCode < 400) && (statusCode != 304);
}

bool ResourceResponse::isNotModified() const
{
    return (httpStatusCode() == 304);
}

bool ResourceResponse::isUnauthorized() const
{
    return (httpStatusCode() == 401);
}

}

#endif
