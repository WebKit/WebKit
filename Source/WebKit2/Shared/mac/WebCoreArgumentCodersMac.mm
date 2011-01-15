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

#include "WebCoreArgumentCoders.h"

#include "ArgumentCodersCF.h"
#include "WebKitSystemInterface.h"

namespace CoreIPC {

void encodeResourceRequest(ArgumentEncoder* encoder, const WebCore::ResourceRequest& resourceRequest)
{
    RetainPtr<CFDictionaryRef> dictionary(AdoptCF, WKNSURLRequestCreateSerializableRepresentation(resourceRequest.nsURLRequest(), CoreIPC::tokenNullTypeRef()));
    encode(encoder, dictionary.get());
}

bool decodeResourceRequest(ArgumentDecoder* decoder, WebCore::ResourceRequest& resourceRequest)
{
    RetainPtr<CFDictionaryRef> dictionary;
    if (!decode(decoder, dictionary))
        return false;

    NSURLRequest *nsURLRequest = WKNSURLRequestFromSerializableRepresentation(dictionary.get(), CoreIPC::tokenNullTypeRef());
    if (!nsURLRequest)
        return false;

    resourceRequest = WebCore::ResourceRequest(nsURLRequest);
    return true;
}

void encodeResourceResponse(ArgumentEncoder* encoder, const WebCore::ResourceResponse& resourceResponse)
{
    bool responseIsPresent = resourceResponse.nsURLResponse();
    encoder->encode(responseIsPresent);

    if (!responseIsPresent)
        return;

    RetainPtr<CFDictionaryRef> dictionary(AdoptCF, WKNSURLResponseCreateSerializableRepresentation(resourceResponse.nsURLResponse(), CoreIPC::tokenNullTypeRef()));
    encode(encoder, dictionary.get());
}

bool decodeResourceResponse(ArgumentDecoder* decoder, WebCore::ResourceResponse& resourceResponse)
{
    bool responseIsPresent;
    decoder->decode(responseIsPresent);

    if (!responseIsPresent) {
        resourceResponse = WebCore::ResourceResponse();
        return true;
    }

    RetainPtr<CFDictionaryRef> dictionary;
    if (!decode(decoder, dictionary))
        return false;

    NSURLResponse* nsURLResponse = WKNSURLResponseFromSerializableRepresentation(dictionary.get(), CoreIPC::tokenNullTypeRef());
    if (!nsURLResponse)
        return false;

    resourceResponse = WebCore::ResourceResponse(nsURLResponse);
    return true;
}


} // namespace CoreIPC
