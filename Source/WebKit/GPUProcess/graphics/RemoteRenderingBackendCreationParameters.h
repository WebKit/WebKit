/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include "IPCSemaphore.h"
#include "RenderingBackendIdentifier.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/PageIdentifier.h>

namespace WebKit {

struct RemoteRenderingBackendCreationParameters {
    RenderingBackendIdentifier identifier;
    IPC::Semaphore resumeDisplayListSemaphore;
    WebPageProxyIdentifier pageProxyID;
    WebCore::PageIdentifier pageID;

    void encode(IPC::Encoder& encoder) const
    {
        encoder << identifier << resumeDisplayListSemaphore << pageProxyID << pageID;
    }

    static std::optional<RemoteRenderingBackendCreationParameters> decode(IPC::Decoder& decoder)
    {
        std::optional<RenderingBackendIdentifier> identifier;
        decoder >> identifier;
        if (!identifier)
            return std::nullopt;

        std::optional<IPC::Semaphore> resumeDisplayListSemaphore;
        decoder >> resumeDisplayListSemaphore;
        if (!resumeDisplayListSemaphore)
            return std::nullopt;

        std::optional<WebPageProxyIdentifier> pageProxyID;
        decoder >> pageProxyID;
        if (!pageProxyID)
            return std::nullopt;

        std::optional<WebCore::PageIdentifier> pageID;
        decoder >> pageID;
        if (!pageID)
            return std::nullopt;

        return {{ *identifier, WTFMove(*resumeDisplayListSemaphore), *pageProxyID, *pageID }};
    }
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
