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

#include "Decoder.h"
#include "GPUConnectionToWebProcess.h"
#include <WebCore/ConcreteImageBuffer.h>
#include <WebCore/DisplayList.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/DisplayListReplayer.h>

namespace WebKit {

template<typename BackendType>
class RemoteImageBuffer : public WebCore::ConcreteImageBuffer<BackendType>, public WebCore::DisplayList::Replayer::Delegate {
    using BaseConcreteImageBuffer = WebCore::ConcreteImageBuffer<BackendType>;
    using BaseConcreteImageBuffer::context;
    using BaseConcreteImageBuffer::m_backend;
    using BaseConcreteImageBuffer::putImageData;

public:
    static auto create(const WebCore::FloatSize& size, float resolutionScale, WebCore::DestinationColorSpace colorSpace, WebCore::PixelFormat pixelFormat, RemoteRenderingBackend& remoteRenderingBackend, WebCore::RenderingResourceIdentifier renderingResourceIdentifier)
    {
        return BaseConcreteImageBuffer::template create<RemoteImageBuffer>(size, resolutionScale, colorSpace, pixelFormat, nullptr, remoteRenderingBackend, renderingResourceIdentifier);
    }

    RemoteImageBuffer(const WebCore::ImageBufferBackend::Parameters& parameters, std::unique_ptr<BackendType>&& backend, RemoteRenderingBackend& remoteRenderingBackend, WebCore::RenderingResourceIdentifier renderingResourceIdentifier)
        : BaseConcreteImageBuffer(parameters, WTFMove(backend), renderingResourceIdentifier)
        , m_remoteRenderingBackend(remoteRenderingBackend)
        , m_renderingResourceIdentifier(renderingResourceIdentifier)
    {
        m_remoteRenderingBackend.didCreateImageBufferBackend(m_backend->createImageBufferBackendHandle(), renderingResourceIdentifier);
    }

    ~RemoteImageBuffer()
    {
        // Unwind the context's state stack before destruction, since calls to restore may not have
        // been flushed yet, or the web process may have terminated.
        while (context().stackSize())
            context().restore();
    }

private:
    bool apply(WebCore::DisplayList::ItemHandle item, WebCore::GraphicsContext& context) override
    {
        if (item.is<WebCore::DisplayList::PutImageData>()) {
            auto& putImageDataItem = item.get<WebCore::DisplayList::PutImageData>();
            putImageData(putImageDataItem.inputFormat(), putImageDataItem.imageData(), putImageDataItem.srcRect(), putImageDataItem.destPoint(), putImageDataItem.destFormat());
            return true;
        }

        if (item.is<WebCore::DisplayList::FlushContext>()) {
            BaseConcreteImageBuffer::flushContext();
            auto identifier = item.get<WebCore::DisplayList::FlushContext>().identifier();
            m_remoteRenderingBackend.didFlush(identifier, m_renderingResourceIdentifier);
            return true;
        }

        if (item.is<WebCore::DisplayList::MetaCommandChangeItemBuffer>()) {
            auto nextBufferIdentifier = item.get<WebCore::DisplayList::MetaCommandChangeItemBuffer>().identifier();
            m_remoteRenderingBackend.setNextItemBufferToRead(nextBufferIdentifier, m_renderingResourceIdentifier);
            return true;
        }

        return m_remoteRenderingBackend.applyMediaItem(item, context);
    }

    RemoteRenderingBackend& m_remoteRenderingBackend;
    WebCore::RenderingResourceIdentifier m_renderingResourceIdentifier;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
