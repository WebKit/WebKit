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

#include <WebCore/DisplayListImageBuffer.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/DisplayListRecorder.h>

namespace WebKit {

class RemoteRenderingBackend;

template<typename BackendType>
class RemoteImageBufferProxy : public WebCore::DisplayList::ImageBuffer<BackendType>, public WebCore::DisplayList::Recorder::Delegate {
    using BaseDisplayListImageBuffer = WebCore::DisplayList::ImageBuffer<BackendType>;
    using BaseDisplayListImageBuffer::m_backend;
    using BaseDisplayListImageBuffer::m_drawingContext;
    using BaseDisplayListImageBuffer::m_renderingResourceIdentifier;

public:
    static RefPtr<RemoteImageBufferProxy> create(const WebCore::FloatSize& size, float resolutionScale, RemoteRenderingBackendProxy& remoteRenderingBackendProxy)
    {
        if (BackendType::calculateBackendSize(size, resolutionScale).isEmpty())
            return nullptr;

        return adoptRef(new RemoteImageBufferProxy(size, remoteRenderingBackendProxy));
    }

    ~RemoteImageBufferProxy()
    {
        if (!m_remoteRenderingBackendProxy)
            return;
        flushDrawingContext();
        m_remoteRenderingBackendProxy->releaseRemoteResource(m_renderingResourceIdentifier);
    }

    void createBackend(const WebCore::FloatSize& logicalSize, const WebCore::IntSize& backendSize, float resolutionScale, WebCore::ColorSpace colorSpace, ImageBufferBackendHandle handle)
    {
        ASSERT(!m_backend);
        m_backend = BackendType::create(logicalSize, backendSize, resolutionScale, colorSpace, WTFMove(handle));
    }

    void commitFlushDisplayList(DisplayListFlushIdentifier flushIdentifier)
    {
        m_receivedFlushIdentifier = flushIdentifier;
    }

protected:
    RemoteImageBufferProxy(const WebCore::FloatSize& size, RemoteRenderingBackendProxy& remoteRenderingBackendProxy)
        : BaseDisplayListImageBuffer(size, this)
        , m_remoteRenderingBackendProxy(makeWeakPtr(remoteRenderingBackendProxy))
    {
        ASSERT(m_remoteRenderingBackendProxy);
    }

    bool isPendingFlush() const { return m_sentFlushIdentifier != m_receivedFlushIdentifier; }

    void timeoutWaitForFlushDisplayListWasCommitted()
    {
        if (!m_remoteRenderingBackendProxy)
            return;

        // Wait for our DisplayList to be flushed but do not hang.
        static constexpr unsigned maxWaitingFlush = 3;
        for (unsigned numWaitingFlush = 0; numWaitingFlush < maxWaitingFlush && isPendingFlush(); ++numWaitingFlush)
            m_remoteRenderingBackendProxy->waitForFlushDisplayListWasCommitted();
    }

    BackendType* ensureBackendCreated() const override
    {
        if (!m_backend && m_remoteRenderingBackendProxy)
            m_remoteRenderingBackendProxy->waitForImageBufferBackendWasCreated();
        return m_backend.get();
    }

    RefPtr<WebCore::ImageData> getImageData(WebCore::AlphaPremultiplication outputFormat, const WebCore::IntRect& srcRect) const override
    {
        if (!m_remoteRenderingBackendProxy)
            return nullptr;

        auto& mutableThis = const_cast<RemoteImageBufferProxy&>(*this);
        auto& displayList = mutableThis.m_drawingContext.displayList();
        if (!displayList.isEmpty()) {
            mutableThis.flushDisplayList(displayList);
            mutableThis.m_itemCountInCurrentDisplayList = 0;
            displayList.clear();
        }

        // getImageData is synchronous, which means we've already received the CommitImageBufferFlushContext message.
        return m_remoteRenderingBackendProxy->getImageData(outputFormat, srcRect, m_renderingResourceIdentifier);
    }

    void putImageData(WebCore::AlphaPremultiplication inputFormat, const WebCore::ImageData& imageData, const WebCore::IntRect& srcRect, const WebCore::IntPoint& destPoint = { }, WebCore::AlphaPremultiplication destFormat = WebCore::AlphaPremultiplication::Premultiplied) override
    {
        // The math inside ImageData::create() doesn't agree with the math inside ImageBufferBackend::putImageData() about how m_resolutionScale interacts with the data in the ImageBuffer.
        // This means that putImageData() is only called when m_resolutionScale == 1.
        ASSERT_IMPLIES(m_backend, m_backend->resolutionScale() == 1);
        m_drawingContext.recorder().putImageData(inputFormat, imageData, srcRect, destPoint, destFormat);
    }

    bool prefersPreparationForDisplay() override { return true; }

    void flushContext() override
    {
        flushDrawingContext();
        m_backend->flushContext();
    }

    void flushDrawingContext() override
    {
        flushDrawingContextAndCommit();
        timeoutWaitForFlushDisplayListWasCommitted();
    }

    void flushDrawingContextAndCommit() override
    {
        if (!m_remoteRenderingBackendProxy)
            return;

        auto& displayList = m_drawingContext.displayList();
        if (displayList.isEmpty())
            return;

        TraceScope tracingScope(FlushRemoteImageBufferStart, FlushRemoteImageBufferEnd, 1);
        m_sentFlushIdentifier = m_remoteRenderingBackendProxy->flushDisplayListAndCommit(displayList, m_renderingResourceIdentifier);
        m_itemCountInCurrentDisplayList = 0;
        displayList.clear();
    }

    void flushDisplayList(const WebCore::DisplayList::DisplayList& displayList) override
    {
        if (!m_remoteRenderingBackendProxy || displayList.isEmpty())
            return;

        TraceScope tracingScope(FlushRemoteImageBufferStart, FlushRemoteImageBufferEnd);
        m_remoteRenderingBackendProxy->flushDisplayList(displayList, m_renderingResourceIdentifier);
    }

    void willAppendItemOfType(WebCore::DisplayList::ItemType) override
    {
        constexpr size_t DisplayListBatchSize = 512;
        auto& displayList = m_drawingContext.displayList();
        if (++m_itemCountInCurrentDisplayList < DisplayListBatchSize)
            return;

        m_itemCountInCurrentDisplayList = 0;
        flushDisplayList(displayList);
        displayList.clear();
    }

    void didAppendItemOfType(WebCore::DisplayList::ItemType type) override
    {
        if (type == WebCore::DisplayList::ItemType::DrawImageBuffer)
            flushDrawingContext();
    }

    DisplayListFlushIdentifier m_sentFlushIdentifier;
    DisplayListFlushIdentifier m_receivedFlushIdentifier;
    WeakPtr<RemoteRenderingBackendProxy> m_remoteRenderingBackendProxy;
    size_t m_itemCountInCurrentDisplayList { 0 };
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
