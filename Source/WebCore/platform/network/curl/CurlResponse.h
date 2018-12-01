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

#pragma once

#include <wtf/URL.h>

namespace WebCore {

class CurlResponse {
public:
    CurlResponse() = default;

    CurlResponse isolatedCopy() const
    {
        CurlResponse copy;

        copy.url = url.isolatedCopy();
        copy.statusCode = statusCode;
        copy.httpConnectCode = httpConnectCode;
        copy.expectedContentLength = expectedContentLength;

        for (const auto& header : headers)
            copy.headers.append(header.isolatedCopy());

        copy.proxyUrl = proxyUrl.isolatedCopy();
        copy.availableHttpAuth = availableHttpAuth;
        copy.availableProxyAuth = availableProxyAuth;
        copy.httpVersion = httpVersion;

        return copy;
    }

    URL url;
    long statusCode { 0 };
    long httpConnectCode { 0 };
    long long expectedContentLength { 0 };
    Vector<String> headers;

    URL proxyUrl;
    long availableHttpAuth { 0 };
    long availableProxyAuth { 0 };
    long httpVersion { 0 };
};

} // namespace WebCore
