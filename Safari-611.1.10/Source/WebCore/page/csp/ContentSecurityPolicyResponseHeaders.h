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

enum class ContentSecurityPolicyHeaderType : bool {
    Report,
    Enforce,
};

class ContentSecurityPolicyResponseHeaders {
public:
    ContentSecurityPolicyResponseHeaders() = default;
    WEBCORE_EXPORT explicit ContentSecurityPolicyResponseHeaders(const ResourceResponse&);

    ContentSecurityPolicyResponseHeaders isolatedCopy() const;

    template <class Encoder> void encode(Encoder&) const;
    template <class Decoder> static Optional<ContentSecurityPolicyResponseHeaders> decode(Decoder&);

    enum EmptyTag { Empty };
    struct MarkableTraits {
        static bool isEmptyValue(const ContentSecurityPolicyResponseHeaders& identifier)
        {
            return identifier.m_emptyForMarkable;
        }

        static ContentSecurityPolicyResponseHeaders emptyValue()
        {
            return ContentSecurityPolicyResponseHeaders(Empty);
        }
    };

private:
    friend class ContentSecurityPolicy;
    ContentSecurityPolicyResponseHeaders(EmptyTag)
        : m_emptyForMarkable(true)
    { }

    Vector<std::pair<String, ContentSecurityPolicyHeaderType>> m_headers;
    int m_httpStatusCode { 0 };
    bool m_emptyForMarkable { false };
};

template <class Encoder>
void ContentSecurityPolicyResponseHeaders::encode(Encoder& encoder) const
{
    encoder << static_cast<uint64_t>(m_headers.size());
    for (auto& pair : m_headers) {
        encoder << pair.first;
        encoder << pair.second;
    }
    encoder << m_httpStatusCode;
}

template <class Decoder>
Optional<ContentSecurityPolicyResponseHeaders> ContentSecurityPolicyResponseHeaders::decode(Decoder& decoder)
{
    ContentSecurityPolicyResponseHeaders headers;

    Optional<uint64_t> headersSize;
    decoder >> headersSize;
    if (!headersSize)
        return WTF::nullopt;
    for (size_t i = 0; i < *headersSize; ++i) {
        Optional<String> header;
        decoder >> header;
        if (!header)
            return WTF::nullopt;
        Optional<ContentSecurityPolicyHeaderType> headerType;
        decoder >> headerType;
        if (!headerType)
            return WTF::nullopt;
        headers.m_headers.append(std::make_pair(WTFMove(*header), WTFMove(*headerType)));
    }
    headers.m_headers.shrinkToFit();

    Optional<int> httpStatusCode;
    decoder >> httpStatusCode;
    if (!httpStatusCode)
        return WTF::nullopt;
    headers.m_httpStatusCode = *httpStatusCode;

    return headers;
}

} // namespace WebCore
