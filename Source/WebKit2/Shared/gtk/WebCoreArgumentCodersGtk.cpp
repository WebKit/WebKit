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

#include <WebCore/NotImplemented.h>

namespace CoreIPC {

void encodeResourceRequest(ArgumentEncoder* encoder, const WebCore::ResourceRequest& resourceRequest)
{
    notImplemented();
}

bool decodeResourceRequest(ArgumentDecoder* decoder, WebCore::ResourceRequest& resourceRequest)
{
    notImplemented();

    // FIXME: Add real implementation when we want to implement something that
    // depends on this like the policy client.
    // https://bugs.webkit.org/show_bug.cgi?id=55934
    resourceRequest = WebCore::ResourceRequest();
    return true;
}

void encodeResourceResponse(ArgumentEncoder* encoder, const WebCore::ResourceResponse& resourceResponse)
{
    notImplemented();
}

bool decodeResourceResponse(ArgumentDecoder* decoder, WebCore::ResourceResponse& resourceResponse)
{
    notImplemented();

    // FIXME: Ditto.
    resourceResponse = WebCore::ResourceResponse();
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
