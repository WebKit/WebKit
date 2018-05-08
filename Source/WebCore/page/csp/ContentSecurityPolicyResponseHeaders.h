/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ContentSecurityPolicy;
class ResourceResponse;

enum class ContentSecurityPolicyHeaderType {
    Report,
    Enforce,
    PrefixedReport,
    PrefixedEnforce,
};

class ContentSecurityPolicyResponseHeaders {
public:
    ContentSecurityPolicyResponseHeaders() = default;
    WEBCORE_EXPORT explicit ContentSecurityPolicyResponseHeaders(const ResourceResponse&);

    ContentSecurityPolicyResponseHeaders isolatedCopy() const;

    template <class Encoder> void encode(Encoder&) const;
    template <class Decoder> static bool decode(Decoder&, ContentSecurityPolicyResponseHeaders&);

private:
    friend class ContentSecurityPolicy;

    Vector<std::pair<String, ContentSecurityPolicyHeaderType>> m_headers;
    int m_httpStatusCode { 0 };
};

template <class Encoder>
void ContentSecurityPolicyResponseHeaders::encode(Encoder& encoder) const
{
    encoder << static_cast<uint64_t>(m_headers.size());
    for (auto& pair : m_headers) {
        encoder << pair.first;
        encoder.encodeEnum(pair.second);
    }
    encoder << m_httpStatusCode;
}

template <class Decoder>
bool ContentSecurityPolicyResponseHeaders::decode(Decoder& decoder, ContentSecurityPolicyResponseHeaders& headers)
{
    uint64_t headersSize;
    if (!decoder.decode(headersSize))
        return false;
    headers.m_headers.reserveCapacity(static_cast<size_t>(headersSize));
    for (size_t i = 0; i < headersSize; ++i) {
        String header;
        if (!decoder.decode(header))
            return false;
        ContentSecurityPolicyHeaderType headerType;
        if (!decoder.decodeEnum(headerType))
            return false;
        headers.m_headers.append(std::make_pair(header, headerType));
    }

    if (!decoder.decode(headers.m_httpStatusCode))
        return false;

    return true;
}

} // namespace WebCore
