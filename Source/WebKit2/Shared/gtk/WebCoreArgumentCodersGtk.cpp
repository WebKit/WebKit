/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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
#include "ResourceRequest.h"
#include "WebCoreArgumentCoders.h"

namespace CoreIPC {

void encodeResourceRequest(ArgumentEncoder* encoder, const WebCore::ResourceRequest& resourceRequest)
{
    encoder->encode(CoreIPC::In(resourceRequest.url().string()));
    encoder->encode(CoreIPC::In(resourceRequest.httpMethod()));

    const WebCore::HTTPHeaderMap& headers = resourceRequest.httpHeaderFields();
    encoder->encode(CoreIPC::In(static_cast<uint32_t>(headers.size())));
    if (!headers.isEmpty()) {
        WebCore::HTTPHeaderMap::const_iterator end = headers.end();
        for (WebCore::HTTPHeaderMap::const_iterator it = headers.begin(); it != end; ++it)
            encoder->encode(CoreIPC::In(it->first, it->second));
    }

    WebCore::FormData* httpBody = resourceRequest.httpBody();
    encoder->encode(CoreIPC::In(static_cast<bool>(httpBody)));
    if (httpBody)
        encoder->encode(CoreIPC::In(httpBody->flattenToString()));

    encoder->encode(CoreIPC::In(resourceRequest.firstPartyForCookies().string()));
    encoder->encode(CoreIPC::In(static_cast<uint32_t>(resourceRequest.soupMessageFlags())));
}

bool decodeResourceRequest(ArgumentDecoder* decoder, WebCore::ResourceRequest& resourceRequest)
{
    WebCore::ResourceRequest request;

    String url;
    if (!decoder->decode(CoreIPC::Out(url)))
        return false;
    request.setURL(WebCore::KURL(WebCore::KURL(), url));

    String httpMethod;
    if (!decoder->decode(CoreIPC::Out(httpMethod)))
        return false;
    request.setHTTPMethod(httpMethod);

    uint32_t size;
    if (!decoder->decode(CoreIPC::Out(size)))
        return false;
    if (size) {
        AtomicString name;
        String value;
        for (uint32_t i = 0; i < size; ++i) {
            if (!decoder->decode(CoreIPC::Out(name, value)))
                return false;
            request.setHTTPHeaderField(name, value);
        }
    }

    bool hasHTTPBody;
    if (!decoder->decode(CoreIPC::Out(hasHTTPBody)))
        return false;
    if (hasHTTPBody) {
        String httpBody;
        if (!decoder->decode(CoreIPC::Out(httpBody)))
            return false;
        request.setHTTPBody(WebCore::FormData::create(httpBody.utf8()));
    }

    String firstPartyForCookies;
    if (!decoder->decode(CoreIPC::Out(firstPartyForCookies)))
        return false;
    request.setFirstPartyForCookies(WebCore::KURL(WebCore::KURL(), firstPartyForCookies));

    uint32_t soupMessageFlags;
    if (!decoder->decode(CoreIPC::Out(soupMessageFlags)))
        return false;
    request.setSoupMessageFlags(static_cast<SoupMessageFlags>(soupMessageFlags));

    resourceRequest = request;
    return true;
}

void encodeResourceResponse(ArgumentEncoder* encoder, const WebCore::ResourceResponse& resourceResponse)
{
    encoder->encode(CoreIPC::In(resourceResponse.url().string()));
    encoder->encode(CoreIPC::In(static_cast<int32_t>(resourceResponse.httpStatusCode())));

    const WebCore::HTTPHeaderMap& headers = resourceResponse.httpHeaderFields();
    encoder->encode(CoreIPC::In(static_cast<uint32_t>(headers.size())));
    if (!headers.isEmpty()) {
        WebCore::HTTPHeaderMap::const_iterator end = headers.end();
        for (WebCore::HTTPHeaderMap::const_iterator it = headers.begin(); it != end; ++it)
            encoder->encode(CoreIPC::In(it->first, it->second));
    }

    encoder->encode(CoreIPC::In(static_cast<uint32_t>(resourceResponse.soupMessageFlags())));
    encoder->encode(CoreIPC::In(resourceResponse.mimeType()));
    encoder->encode(CoreIPC::In(resourceResponse.textEncodingName()));
    encoder->encode(CoreIPC::In(static_cast<int64_t>(resourceResponse.expectedContentLength())));
    encoder->encode(CoreIPC::In(resourceResponse.httpStatusText()));
    encoder->encode(CoreIPC::In(resourceResponse.suggestedFilename()));
}

bool decodeResourceResponse(ArgumentDecoder* decoder, WebCore::ResourceResponse& resourceResponse)
{
    WebCore::ResourceResponse response;

    String url;
    if (!decoder->decode(CoreIPC::Out(url)))
        return false;
    response.setURL(WebCore::KURL(WebCore::KURL(), url));

    int32_t httpStatusCode;
    if (!decoder->decode(CoreIPC::Out(httpStatusCode)))
        return false;
    response.setHTTPStatusCode(httpStatusCode);

    uint32_t size;
    if (!decoder->decode(CoreIPC::Out(size)))
        return false;
    if (size) {
        AtomicString name;
        String value;
        for (uint32_t i = 0; i < size; ++i) {
            if (!decoder->decode(CoreIPC::Out(name, value)))
                return false;
            response.setHTTPHeaderField(name, value);
        }
    }

    uint32_t soupMessageFlags;
    if (!decoder->decode(CoreIPC::Out(soupMessageFlags)))
        return false;
    response.setSoupMessageFlags(static_cast<SoupMessageFlags>(soupMessageFlags));

    String mimeType;
    if (!decoder->decode(CoreIPC::Out(mimeType)))
        return false;
    response.setMimeType(mimeType);

    String textEncodingName;
    if (!decoder->decode(CoreIPC::Out(textEncodingName)))
        return false;
    response.setTextEncodingName(textEncodingName);

    int64_t contentLength;
    if (!decoder->decode(CoreIPC::Out(contentLength)))
        return false;
    response.setExpectedContentLength(contentLength);

    String httpStatusText;
    if (!decoder->decode(CoreIPC::Out(httpStatusText)))
        return false;
    response.setHTTPStatusText(httpStatusText);

    String suggestedFilename;
    if (!decoder->decode(CoreIPC::Out(suggestedFilename)))
        return false;
    response.setSuggestedFilename(suggestedFilename);

    resourceResponse = response;
    return true;
}

void encodeResourceError(ArgumentEncoder* encoder, const WebCore::ResourceError& resourceError)
{
    encoder->encode(CoreIPC::In(resourceError.domain(), resourceError.errorCode(), resourceError.failingURL(), resourceError.localizedDescription()));
}

bool decodeResourceError(ArgumentDecoder* decoder, WebCore::ResourceError& resourceError)
{
    String domain;
    int errorCode;
    String failingURL;
    String localizedDescription;
    if (!decoder->decode(CoreIPC::Out(domain, errorCode, failingURL, localizedDescription)))
        return false;
    resourceError = WebCore::ResourceError(domain, errorCode, failingURL, localizedDescription);
    return true;
}

}
