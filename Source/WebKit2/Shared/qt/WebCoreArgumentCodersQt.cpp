/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>

using namespace WebCore;
 
namespace CoreIPC {

void ArgumentCoder<ResourceRequest>::encode(ArgumentEncoder* encoder, const ResourceRequest& resourceRequest)
{
    encoder->encode(resourceRequest.url().string());
}

bool ArgumentCoder<ResourceRequest>::decode(ArgumentDecoder* decoder, ResourceRequest& resourceRequest)
{
    // FIXME: Add *more* coding implementation when we want to implement something that
    // depends on this like the policy client.

    ResourceRequest request;
    String url;
    if (!decoder->decode(url))
        return false;
    request.setURL(KURL(WebCore::ParsedURLString, url));

    resourceRequest = request;
    return true;
}


void ArgumentCoder<ResourceResponse>::encode(ArgumentEncoder* encoder, const ResourceResponse& resourceResponse)
{
    encoder->encode(resourceResponse.url().string());
    encoder->encode(resourceResponse.mimeType());
    encoder->encode(static_cast<int64_t>(resourceResponse.expectedContentLength()));
    encoder->encode(resourceResponse.textEncodingName());
    encoder->encode(resourceResponse.suggestedFilename());
}

bool ArgumentCoder<ResourceResponse>::decode(ArgumentDecoder* decoder, ResourceResponse& resourceResponse)
{
    ResourceResponse response;

    String url;
    if (!decoder->decode(url))
        return false;
    response.setURL(KURL(WebCore::ParsedURLString, url));

    String mimeType;
    if (!decoder->decode(mimeType))
        return false;
    response.setMimeType(mimeType);

    int64_t contentLength;
    if (!decoder->decode(contentLength))
        return false;
    response.setExpectedContentLength(contentLength);

    String textEncodingName;
    if (!decoder->decode(textEncodingName))
        return false;
    response.setTextEncodingName(textEncodingName);

    String suggestedFilename;
    if (!decoder->decode(suggestedFilename))
        return false;
    response.setSuggestedFilename(suggestedFilename);

    resourceResponse = response;
    return true;
}


void ArgumentCoder<ResourceError>::encode(ArgumentEncoder* encoder, const ResourceError& resourceError)
{
    encoder->encode(resourceError.domain());
    encoder->encode(resourceError.errorCode());
    encoder->encode(resourceError.failingURL());
    encoder->encode(resourceError.localizedDescription());
    encoder->encode(resourceError.isCancellation());
}

bool ArgumentCoder<ResourceError>::decode(ArgumentDecoder* decoder, ResourceError& resourceError)
{
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

    bool isCancellation;
    if (!decoder->decode(isCancellation))
        return false;

    resourceError = ResourceError(domain, errorCode, failingURL, localizedDescription);
    resourceError.setIsCancellation(isCancellation);
    return true;
}

} // namespace CoreIPC
