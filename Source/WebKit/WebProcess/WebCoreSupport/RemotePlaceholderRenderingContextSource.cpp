/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "RemotePlaceholderRenderingContextSource.h"

#if ENABLE(OFFSCREEN_CANVAS) && ENABLE(GPU_PROCESS)

#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/ImageBuffer.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemotePlaceholderRenderingContextSource);

Ref<RemotePlaceholderRenderingContextSource> RemotePlaceholderRenderingContextSource::create(PlaceholderRenderingContextIdentifier identifier)
{
    return adoptRef(*new RemotePlaceholderRenderingContextSource(identifier));
}

RemotePlaceholderRenderingContextSource::RemotePlaceholderRenderingContextSource(PlaceholderRenderingContextIdentifier identifier)
    : PlaceholderRenderingContextSource(identifier)
{
}

void RemotePlaceholderRenderingContextSource::setPlaceholderBuffer(ImageBuffer& buffer, bool originClean, bool opaque)
{
    if (m_placeholderUnreachable)
        return;

    RefPtr clone = buffer.clone();
    if (!clone)
        return;

    auto transferHandle = SerializedImageBuffer::sinkIntoTransferHandle(ImageBuffer::sinkIntoSerializedImageBuffer(WTF::move(clone)));
    if (!transferHandle)
        return;

    RefPtr connection = WebProcess::singleton().parentProcessConnection();
    if (!connection)
        return;
    auto transferIdentifier = transferHandle->identifier;
    connection->sendWithAsyncReply(Messages::WebProcessProxy::CommitOffscreenCanvasPlaceholderFrame(identifier(), WTF::move(*transferHandle), originClean, opaque), [protectedThis = Ref { *this }, transferIdentifier](bool accepted) {
        if (accepted)
            return;
        protectedThis->m_placeholderUnreachable = true;
        WebProcess::singleton().releaseTransferredImageBuffer(transferIdentifier);
    });
}

} // namespace WebKit

#endif // ENABLE(OFFSCREEN_CANVAS) && ENABLE(GPU_PROCESS)
