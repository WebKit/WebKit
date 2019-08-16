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

#include "config.h"
#include "FrameInfoData.h"

#include "WebCoreArgumentCoders.h"

namespace WebKit {

void FrameInfoData::encode(IPC::Encoder& encoder) const
{
    encoder << isMainFrame;
    encoder << request;
    encoder << securityOrigin;
    encoder << frameID;
}

bool FrameInfoData::decode(IPC::Decoder& decoder, FrameInfoData& result)
{
    if (!decoder.decode(result.isMainFrame))
        return false;
    if (!decoder.decode(result.request))
        return false;
    Optional<WebCore::SecurityOriginData> securityOrigin;
    decoder >> securityOrigin;
    if (!securityOrigin)
        return false;
    result.securityOrigin = WTFMove(*securityOrigin);
    
    Optional<Optional<WebCore::FrameIdentifier>> frameID;
    decoder >> frameID;
    if (!frameID)
        return false;
    result.frameID = WTFMove(*frameID);

    return true;
}

}
