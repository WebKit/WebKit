/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "URLSchemeTaskParameters.h"

#include "Decoder.h"
#include "Encoder.h"
#include "WebCoreArgumentCoders.h"

namespace WebKit {

void URLSchemeTaskParameters::encode(IPC::Encoder& encoder) const
{
    encoder << handlerIdentifier;
    encoder << taskIdentifier;
    encoder << request;
    if (request.httpBody()) {
        encoder << true;
        encoder << request.httpBody();
    } else
        encoder << false;
    encoder << frameInfo;
}

std::optional<URLSchemeTaskParameters> URLSchemeTaskParameters::decode(IPC::Decoder& decoder)
{
    std::optional<WebURLSchemeHandlerIdentifier> handlerIdentifier;
    decoder >> handlerIdentifier;
    if (!handlerIdentifier)
        return std::nullopt;
    
    std::optional<WebCore::ResourceLoaderIdentifier> taskIdentifier;
    decoder >> taskIdentifier;
    if (!taskIdentifier)
        return std::nullopt;

    WebCore::ResourceRequest request;
    if (!decoder.decode(request))
        return std::nullopt;

    std::optional<bool> hasHTTPBody;
    decoder >> hasHTTPBody;
    if (!hasHTTPBody)
        return std::nullopt;
    if (*hasHTTPBody) {
        std::optional<RefPtr<WebCore::FormData>> formData;
        decoder >> formData;
        if (!formData)
            return std::nullopt;
        request.setHTTPBody(WTFMove(*formData));
    }
    
    std::optional<FrameInfoData> frameInfo;
    decoder >> frameInfo;
    if (!frameInfo)
        return std::nullopt;

    return {{
        WTFMove(*handlerIdentifier),
        WTFMove(*taskIdentifier),
        WTFMove(request),
        WTFMove(*frameInfo),
    }};
}
    
} // namespace WebKit
