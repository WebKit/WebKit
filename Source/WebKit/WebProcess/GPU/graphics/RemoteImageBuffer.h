/*
 * Copyright (C) 2020 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(GPU_PROCESS)

#include "RemoteImageBufferMessageHandler.h"
#include <WebCore/DisplayListImageBuffer.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/DisplayListRecorder.h>

namespace WebKit {

class RemoteRenderingBackend;

template<typename BackendType>
class RemoteImageBuffer : public WebCore::DisplayList::ImageBuffer<BackendType>, public RemoteImageBufferMessageHandler {
    using BaseDisplayListImageBuffer = WebCore::DisplayList::ImageBuffer<BackendType>;
    using BaseDisplayListImageBuffer::m_backend;
    using BaseDisplayListImageBuffer::m_drawingContext;

public:
    static std::unique_ptr<RemoteImageBuffer> create(const WebCore::FloatSize& size, WebCore::RenderingMode renderingMode, float resolutionScale, WebCore::ColorSpace colorSpace, RemoteRenderingBackend& remoteRenderingBackend)
    {
        if (BackendType::calculateBackendSize(size, resolutionScale).isEmpty())
            return nullptr;

        return std::unique_ptr<RemoteImageBuffer>(new RemoteImageBuffer(size, renderingMode, resolutionScale, colorSpace, remoteRenderingBackend));
    }

protected:
    RemoteImageBuffer(const WebCore::FloatSize& size, WebCore::RenderingMode renderingMode, float resolutionScale, WebCore::ColorSpace colorSpace, RemoteRenderingBackend& remoteRenderingBackend)
        : BaseDisplayListImageBuffer(size)
        , RemoteImageBufferMessageHandler(size, renderingMode, resolutionScale, colorSpace, remoteRenderingBackend)
    {
    }

    void createBackend(const WebCore::FloatSize& logicalSize, const WebCore::IntSize& backendSize, float resolutionScale, WebCore::ColorSpace colorSpace, ImageBufferBackendHandle handle) override
    {
        ASSERT(!m_backend);
        m_backend = BackendType::create(logicalSize, backendSize, resolutionScale, colorSpace, WTFMove(handle));
    }
    
    bool isBackendCreated() const override { return m_backend.get(); }

    BackendType* ensureBackendCreated() const override
    {
        if (!m_backend)
            const_cast<RemoteImageBuffer&>(*this).RemoteImageBufferMessageHandler::waitForCreateImageBufferBackend();
        return m_backend.get();
    }

    RefPtr<WebCore::ImageData> getImageData(WebCore::AlphaPremultiplication outputFormat, const WebCore::IntRect& srcRect) const override
    {
        auto& displayList = const_cast<RemoteImageBuffer*>(this)->m_drawingContext.displayList();
        const_cast<RemoteImageBuffer*>(this)->RemoteImageBufferMessageHandler::flushDrawingContext(displayList);
        auto result = const_cast<RemoteImageBuffer*>(this)->RemoteImageBufferMessageHandler::getImageData(outputFormat, srcRect);
        // getImageData is synchronous, which means we've already received the CommitImageBufferFlushContext message.
        return result;
    }

    void putImageData(WebCore::AlphaPremultiplication inputFormat, const WebCore::ImageData& imageData, const WebCore::IntRect& srcRect, const WebCore::IntPoint& destPoint = { }, WebCore::AlphaPremultiplication destFormat = WebCore::AlphaPremultiplication::Premultiplied) override
    {
        // The math inside ImageData::create() doesn't agree with the math inside ImageBufferBackend::putImageData() about how m_resolutionScale interacts with the data in the ImageBuffer.
        // This means that putImageData() is only called when m_resolutionScale == 1.
        ASSERT(m_backend->resolutionScale() == 1);
        m_drawingContext.recorder().putImageData(inputFormat, imageData, srcRect, destPoint, destFormat);
    }

    void flushContext() override
    {
        flushDrawingContext();
        m_backend->flushContext();
    }

    void flushDrawingContext() override
    {
        auto& displayList = m_drawingContext.displayList();
        if (displayList.itemCount())
            RemoteImageBufferMessageHandler::flushDrawingContextAndWaitCommit(displayList);
    }
    
    void willAppendItem(const WebCore::DisplayList::Item&) override
    {
        constexpr size_t DisplayListBatchSize = 512;
        auto& displayList = m_drawingContext.displayList();
        if (displayList.itemCount() >= DisplayListBatchSize)
            RemoteImageBufferMessageHandler::flushDrawingContext(displayList);
    }
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
