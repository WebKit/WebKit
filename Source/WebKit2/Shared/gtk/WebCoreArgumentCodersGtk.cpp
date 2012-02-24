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
#include "WebCoreArgumentCoders.h"

#include <wtf/text/CString.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>

using namespace WebCore;

namespace CoreIPC {

void ArgumentCoder<ResourceRequest>::encode(ArgumentEncoder* encoder, const ResourceRequest& resourceRequest)
{
    encoder->encode(resourceRequest.url().string());
    encoder->encode(resourceRequest.httpMethod());

    const HTTPHeaderMap& headers = resourceRequest.httpHeaderFields();
    encoder->encode(headers);

    FormData* httpBody = resourceRequest.httpBody();
    encoder->encode(static_cast<bool>(httpBody));
    if (httpBody)
        encoder->encode(httpBody->flattenToString());

    encoder->encode(resourceRequest.firstPartyForCookies().string());
    encoder->encode(static_cast<uint32_t>(resourceRequest.soupMessageFlags()));
}

bool ArgumentCoder<ResourceRequest>::decode(ArgumentDecoder* decoder, ResourceRequest& resourceRequest)
{
    ResourceRequest request;

    String url;
    if (!decoder->decode(url))
        return false;
    request.setURL(KURL(KURL(), url));

    String httpMethod;
    if (!decoder->decode(httpMethod))
        return false;
    request.setHTTPMethod(httpMethod);

    HTTPHeaderMap headers;
    if (!decoder->decode(headers))
        return false;
    request.addHTTPHeaderFields(headers);

    bool hasHTTPBody;
    if (!decoder->decode(hasHTTPBody))
        return false;
    if (hasHTTPBody) {
        String httpBody;
        if (!decoder->decode(httpBody))
            return false;
        request.setHTTPBody(FormData::create(httpBody.utf8()));
    }

    String firstPartyForCookies;
    if (!decoder->decode(firstPartyForCookies))
        return false;
    request.setFirstPartyForCookies(KURL(KURL(), firstPartyForCookies));

    uint32_t soupMessageFlags;
    if (!decoder->decode(soupMessageFlags))
        return false;
    request.setSoupMessageFlags(static_cast<SoupMessageFlags>(soupMessageFlags));

    resourceRequest = request;
    return true;
}


void ArgumentCoder<ResourceResponse>::encode(ArgumentEncoder* encoder, const ResourceResponse& resourceResponse)
{
    bool responseIsNull = resourceResponse.isNull();
    encoder->encode(responseIsNull);
    if (responseIsNull)
        return;

    encoder->encode(resourceResponse.url().string());
    encoder->encode(static_cast<int32_t>(resourceResponse.httpStatusCode()));

    const HTTPHeaderMap& headers = resourceResponse.httpHeaderFields();
    encoder->encode(headers);

    encoder->encode(static_cast<uint32_t>(resourceResponse.soupMessageFlags()));
    encoder->encode(resourceResponse.mimeType());
    encoder->encode(resourceResponse.textEncodingName());
    encoder->encode(static_cast<int64_t>(resourceResponse.expectedContentLength()));
    encoder->encode(resourceResponse.httpStatusText());
    encoder->encode(resourceResponse.suggestedFilename());
}

bool ArgumentCoder<ResourceResponse>::decode(ArgumentDecoder* decoder, ResourceResponse& resourceResponse)
{
    bool responseIsNull;
    if (!decoder->decode(responseIsNull))
        return false;
    if (responseIsNull) {
        resourceResponse = ResourceResponse();
        return true;
    }

    ResourceResponse response;

    String url;
    if (!decoder->decode(url))
        return false;
    response.setURL(KURL(KURL(), url));

    int32_t httpStatusCode;
    if (!decoder->decode(httpStatusCode))
        return false;
    response.setHTTPStatusCode(httpStatusCode);

    HTTPHeaderMap headers;
    if (!decoder->decode(headers))
        return false;
    for (HTTPHeaderMap::const_iterator it = headers.begin(), end = headers.end(); it != end; ++it)
        response.setHTTPHeaderField(it->first, it->second);

    uint32_t soupMessageFlags;
    if (!decoder->decode(soupMessageFlags))
        return false;
    response.setSoupMessageFlags(static_cast<SoupMessageFlags>(soupMessageFlags));

    String mimeType;
    if (!decoder->decode(mimeType))
        return false;
    response.setMimeType(mimeType);

    String textEncodingName;
    if (!decoder->decode(textEncodingName))
        return false;
    response.setTextEncodingName(textEncodingName);

    int64_t contentLength;
    if (!decoder->decode(contentLength))
        return false;
    response.setExpectedContentLength(contentLength);

    String httpStatusText;
    if (!decoder->decode(httpStatusText))
        return false;
    response.setHTTPStatusText(httpStatusText);

    String suggestedFilename;
    if (!decoder->decode(suggestedFilename))
        return false;
    response.setSuggestedFilename(suggestedFilename);

    resourceResponse = response;
    return true;
}


void ArgumentCoder<ResourceError>::encode(ArgumentEncoder* encoder, const ResourceError& resourceError)
{
    bool errorIsNull = resourceError.isNull();
    encoder->encode(errorIsNull);
    if (errorIsNull)
        return;

    encoder->encode(resourceError.domain());
    encoder->encode(resourceError.errorCode());
    encoder->encode(resourceError.failingURL()); 
    encoder->encode(resourceError.localizedDescription());
}

bool ArgumentCoder<ResourceError>::decode(ArgumentDecoder* decoder, ResourceError& resourceError)
{
    bool errorIsNull;
    if (!decoder->decode(errorIsNull))
        return false;
    if (errorIsNull) {
        resourceError = ResourceError();
        return true;
    }

    String domain;
    if (!decoder->decode(domain))
        return false;

    int errorCode;
    if (!decoder->decode(errorCode))
        return false;

    String failingURL;
    if (!decoder->decode(failingURL))
        return false;

    String localizedDescription;
    if (!decoder->decode(localizedDescription))
        return false;
    
    resourceError = ResourceError(domain, errorCode, failingURL, localizedDescription);
    return true;
}

}
