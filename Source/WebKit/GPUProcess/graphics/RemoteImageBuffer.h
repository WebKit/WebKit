/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "GPUConnectionToWebProcess.h"
#include <WebCore/ConcreteImageBuffer.h>
#include <WebCore/DisplayListReplayer.h>

namespace WebKit {

template<typename BackendType>
class RemoteImageBuffer : public WebCore::ConcreteImageBuffer<BackendType>, public WebCore::DisplayList::Replayer::Delegate {
    using BaseConcreteImageBuffer = WebCore::ConcreteImageBuffer<BackendType>;
    using BaseConcreteImageBuffer::context;
    using BaseConcreteImageBuffer::m_backend;
    using BaseConcreteImageBuffer::putImageData;

public:
    static auto create(const WebCore::FloatSize& size, float resolutionScale, WebCore::ColorSpace colorSpace, RemoteRenderingBackend& remoteRenderingBackend, WebCore::RemoteResourceIdentifier remoteResourceIdentifier)
    {
        return BaseConcreteImageBuffer::template create<RemoteImageBuffer>(size, resolutionScale, colorSpace, nullptr, remoteRenderingBackend, remoteResourceIdentifier);
    }

    RemoteImageBuffer(std::unique_ptr<BackendType>&& backend, RemoteRenderingBackend& remoteRenderingBackend, WebCore::RemoteResourceIdentifier remoteResourceIdentifier)
        : BaseConcreteImageBuffer(WTFMove(backend))
        , m_remoteRenderingBackend(remoteRenderingBackend)
    {
        m_remoteRenderingBackend.imageBufferBackendWasCreated(m_backend->logicalSize(), m_backend->backendSize(), m_backend->resolutionScale(), m_backend->colorSpace(), m_backend->createImageBufferBackendHandle(), remoteResourceIdentifier);
    }

    ~RemoteImageBuffer()
    {
        // Unwind the context's state stack before destruction, since calls to restore may not have
        // been flushed yet, or the web process may have terminated.
        while (context().stackSize())
            context().restore();
    }

private:
    void flushDisplayList(const WebCore::DisplayList::DisplayList& displayList) override
    {
        if (displayList.itemCount()) {
            WebCore::DisplayList::Replayer replayer(BaseConcreteImageBuffer::context(), displayList, this);
            replayer.replay();
        }
    }

    bool apply(WebCore::DisplayList::Item& item, WebCore::GraphicsContext& context) override
    {
        if (item.type() == WebCore::DisplayList::ItemType::PutImageData) {
            auto& putImageDataItem = static_cast<WebCore::DisplayList::PutImageData&>(item);
            putImageData(putImageDataItem.inputFormat(), putImageDataItem.imageData(), putImageDataItem.srcRect(), putImageDataItem.destPoint(), putImageDataItem.destFormat());
            return true;
        }

        if (item.type() == WebCore::DisplayList::ItemType::PaintFrameForMedia) {
            apply(static_cast<WebCore::DisplayList::PaintFrameForMedia&>(item), context);
            return true;
        }

        return false;
    }

    void apply(WebCore::DisplayList::PaintFrameForMedia& item, WebCore::GraphicsContext& context)
    {
        auto process = m_remoteRenderingBackend.gpuConnectionToWebProcess();
        if (!process)
            return;

        auto playerProxy = process->remoteMediaPlayerManagerProxy().getProxy(item.identifier());
        if (!playerProxy)
            return;

        auto player = playerProxy->mediaPlayer();
        if (!player)
            return;

        context.paintFrameForMedia(*player, item.destination());
    }
    
    RemoteRenderingBackend& m_remoteRenderingBackend;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
