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

#include "Encoder.h"
#include "RemoteRenderingBackendProxy.h"
#include "SharedMemory.h"
#include <WebCore/DisplayList.h>
#include <WebCore/DisplayListImageBuffer.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/DisplayListRecorder.h>
#include <wtf/SystemTracing.h>

namespace WebKit {

class RemoteRenderingBackend;

class ThreadSafeRemoteImageBufferFlusher : public WebCore::ThreadSafeImageBufferFlusher {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ThreadSafeRemoteImageBufferFlusher(WebCore::ImageBuffer& imageBuffer)
    {
        // FIXME: We shouldn't synchronously wait on the flush until flush() is called, but have to invent
        // a thread-safe way to wait on the incoming message.
        imageBuffer.flushDrawingContext();
    }

    void flush() override
    {
    }

private:
};

template<typename BackendType>
class RemoteImageBufferProxy : public WebCore::DisplayList::ImageBuffer<BackendType>, public WebCore::DisplayList::Recorder::Delegate, public WebCore::DisplayList::ItemBufferWritingClient {
    using BaseDisplayListImageBuffer = WebCore::DisplayList::ImageBuffer<BackendType>;
    using BaseDisplayListImageBuffer::m_backend;
    using BaseDisplayListImageBuffer::m_drawingContext;
    using BaseDisplayListImageBuffer::m_renderingResourceIdentifier;

public:
    static RefPtr<RemoteImageBufferProxy> create(const WebCore::FloatSize& size, WebCore::RenderingMode renderingMode, float resolutionScale, WebCore::ColorSpace colorSpace, WebCore::PixelFormat pixelFormat, RemoteRenderingBackendProxy& remoteRenderingBackendProxy)
    {
        if (BackendType::calculateBackendSize(size, resolutionScale).isEmpty())
            return nullptr;

        return adoptRef(new RemoteImageBufferProxy(size, renderingMode, resolutionScale, colorSpace, pixelFormat, remoteRenderingBackendProxy));
    }

    ~RemoteImageBufferProxy()
    {
        if (!m_remoteRenderingBackendProxy)
            return;
        flushDrawingContext();
        m_remoteRenderingBackendProxy->remoteResourceCacheProxy().releaseImageBuffer(m_renderingResourceIdentifier);
        m_remoteRenderingBackendProxy->releaseRemoteResource(m_renderingResourceIdentifier);
    }

    void clearBackend() { m_backend = nullptr; }

    void createBackend(const WebCore::FloatSize& logicalSize, const WebCore::IntSize& backendSize, float resolutionScale, WebCore::ColorSpace colorSpace, WebCore::PixelFormat pixelFormat, ImageBufferBackendHandle handle)
    {
        ASSERT(!m_backend);
        m_backend = BackendType::create(logicalSize, backendSize, resolutionScale, colorSpace, pixelFormat, WTFMove(handle));
    }

    void commitFlushDisplayList(WebCore::DisplayList::FlushIdentifier flushIdentifier)
    {
        m_receivedFlushIdentifier = flushIdentifier;
    }

    const WebCore::FloatSize& size() const { return m_size; }
    WebCore::RenderingMode renderingMode() const { return m_renderingMode; }
    float resolutionScale() const final { return m_resolutionScale; }
    WebCore::ColorSpace colorSpace() const { return m_colorSpace; }
    WebCore::PixelFormat pixelFormat() const { return m_pixelFormat; }

    ImageBufferBackendHandle createImageBufferBackendHandle()
    {
        ensureBackendCreated();
        return m_backend->createImageBufferBackendHandle();
    }

protected:
    RemoteImageBufferProxy(const WebCore::FloatSize& size, WebCore::RenderingMode renderingMode, float resolutionScale, WebCore::ColorSpace colorSpace, WebCore::PixelFormat pixelFormat, RemoteRenderingBackendProxy& remoteRenderingBackendProxy)
        : BaseDisplayListImageBuffer(size, this)
        , m_remoteRenderingBackendProxy(makeWeakPtr(remoteRenderingBackendProxy))
        , m_size(size)
        , m_renderingMode(renderingMode)
        , m_resolutionScale(resolutionScale)
        , m_colorSpace(colorSpace)
        , m_pixelFormat(pixelFormat)
    {
        ASSERT(m_remoteRenderingBackendProxy);
        m_remoteRenderingBackendProxy->remoteResourceCacheProxy().cacheImageBuffer(*this);

        m_drawingContext.displayList().setItemBufferClient(this);
        m_drawingContext.displayList().setTracksDrawingItemExtents(false);
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
            mutableThis.submitDisplayList(displayList);
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
        TraceScope tracingScope(FlushRemoteImageBufferStart, FlushRemoteImageBufferEnd);
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

        m_sentFlushIdentifier = WebCore::DisplayList::FlushIdentifier::generate();
        displayList.template append<WebCore::DisplayList::FlushContext>(m_sentFlushIdentifier);
        m_remoteRenderingBackendProxy->submitDisplayList(displayList, m_renderingResourceIdentifier);
        m_itemCountInCurrentDisplayList = 0;
        displayList.clear();
    }

    void submitDisplayList(const WebCore::DisplayList::DisplayList& displayList) override
    {
        if (!m_remoteRenderingBackendProxy || displayList.isEmpty())
            return;

        m_remoteRenderingBackendProxy->submitDisplayList(displayList, m_renderingResourceIdentifier);
    }

    void willAppendItemOfType(WebCore::DisplayList::ItemType) override
    {
        constexpr size_t DisplayListBatchSize = 512;
        auto& displayList = m_drawingContext.displayList();
        if (++m_itemCountInCurrentDisplayList < DisplayListBatchSize)
            return;

        m_itemCountInCurrentDisplayList = 0;
        submitDisplayList(displayList);
        displayList.clear();
    }

    void cacheNativeImage(WebCore::NativeImage& image) override
    {
        if (m_remoteRenderingBackendProxy)
            m_remoteRenderingBackendProxy->remoteResourceCacheProxy().cacheNativeImage(image);
    }

    WebCore::DisplayList::ItemBufferHandle createItemBuffer(size_t capacity) override
    {
        if (m_remoteRenderingBackendProxy)
            return m_remoteRenderingBackendProxy->createItemBuffer(capacity, m_renderingResourceIdentifier);

        ASSERT_NOT_REACHED();
        return { };
    }

    RefPtr<WebCore::SharedBuffer> encodeItem(WebCore::DisplayList::ItemHandle item) const override
    {
        switch (item.type()) {
        case WebCore::DisplayList::ItemType::ClipOutToPath:
            return IPC::Encoder::encodeSingleObject<WebCore::DisplayList::ClipOutToPath>(item.get<WebCore::DisplayList::ClipOutToPath>());
        case WebCore::DisplayList::ItemType::ClipPath:
            return IPC::Encoder::encodeSingleObject<WebCore::DisplayList::ClipPath>(item.get<WebCore::DisplayList::ClipPath>());
        case WebCore::DisplayList::ItemType::ClipToDrawingCommands:
            return IPC::Encoder::encodeSingleObject<WebCore::DisplayList::ClipToDrawingCommands>(item.get<WebCore::DisplayList::ClipToDrawingCommands>());
        case WebCore::DisplayList::ItemType::DrawFocusRingPath:
            return IPC::Encoder::encodeSingleObject<WebCore::DisplayList::DrawFocusRingPath>(item.get<WebCore::DisplayList::DrawFocusRingPath>());
        case WebCore::DisplayList::ItemType::DrawFocusRingRects:
            return IPC::Encoder::encodeSingleObject<WebCore::DisplayList::DrawFocusRingRects>(item.get<WebCore::DisplayList::DrawFocusRingRects>());
        case WebCore::DisplayList::ItemType::DrawGlyphs:
            return IPC::Encoder::encodeSingleObject<WebCore::DisplayList::DrawGlyphs>(item.get<WebCore::DisplayList::DrawGlyphs>());
        case WebCore::DisplayList::ItemType::DrawLinesForText:
            return IPC::Encoder::encodeSingleObject<WebCore::DisplayList::DrawLinesForText>(item.get<WebCore::DisplayList::DrawLinesForText>());
        case WebCore::DisplayList::ItemType::DrawPath:
            return IPC::Encoder::encodeSingleObject<WebCore::DisplayList::DrawPath>(item.get<WebCore::DisplayList::DrawPath>());
        case WebCore::DisplayList::ItemType::FillCompositedRect:
            return IPC::Encoder::encodeSingleObject<WebCore::DisplayList::FillCompositedRect>(item.get<WebCore::DisplayList::FillCompositedRect>());
        case WebCore::DisplayList::ItemType::FillPath:
            return IPC::Encoder::encodeSingleObject<WebCore::DisplayList::FillPath>(item.get<WebCore::DisplayList::FillPath>());
        case WebCore::DisplayList::ItemType::FillRectWithColor:
            return IPC::Encoder::encodeSingleObject<WebCore::DisplayList::FillRectWithColor>(item.get<WebCore::DisplayList::FillRectWithColor>());
        case WebCore::DisplayList::ItemType::FillRectWithGradient:
            return IPC::Encoder::encodeSingleObject<WebCore::DisplayList::FillRectWithGradient>(item.get<WebCore::DisplayList::FillRectWithGradient>());
        case WebCore::DisplayList::ItemType::FillRectWithRoundedHole:
            return IPC::Encoder::encodeSingleObject<WebCore::DisplayList::FillRectWithRoundedHole>(item.get<WebCore::DisplayList::FillRectWithRoundedHole>());
        case WebCore::DisplayList::ItemType::FillRoundedRect:
            return IPC::Encoder::encodeSingleObject<WebCore::DisplayList::FillRoundedRect>(item.get<WebCore::DisplayList::FillRoundedRect>());
        case WebCore::DisplayList::ItemType::PutImageData:
            return IPC::Encoder::encodeSingleObject<WebCore::DisplayList::PutImageData>(item.get<WebCore::DisplayList::PutImageData>());
        case WebCore::DisplayList::ItemType::SetLineDash:
            return IPC::Encoder::encodeSingleObject<WebCore::DisplayList::SetLineDash>(item.get<WebCore::DisplayList::SetLineDash>());
        case WebCore::DisplayList::ItemType::SetState:
            return IPC::Encoder::encodeSingleObject<WebCore::DisplayList::SetState>(item.get<WebCore::DisplayList::SetState>());
        case WebCore::DisplayList::ItemType::StrokePath:
            return IPC::Encoder::encodeSingleObject<WebCore::DisplayList::StrokePath>(item.get<WebCore::DisplayList::StrokePath>());
        case WebCore::DisplayList::ItemType::ApplyDeviceScaleFactor:
#if USE(CG)
        case WebCore::DisplayList::ItemType::ApplyFillPattern:
        case WebCore::DisplayList::ItemType::ApplyStrokePattern:
#endif
        case WebCore::DisplayList::ItemType::BeginTransparencyLayer:
        case WebCore::DisplayList::ItemType::ClearRect:
        case WebCore::DisplayList::ItemType::ClearShadow:
        case WebCore::DisplayList::ItemType::Clip:
        case WebCore::DisplayList::ItemType::ClipOut:
        case WebCore::DisplayList::ItemType::ClipToImageBuffer:
        case WebCore::DisplayList::ItemType::ConcatenateCTM:
        case WebCore::DisplayList::ItemType::DrawDotsForDocumentMarker:
        case WebCore::DisplayList::ItemType::DrawEllipse:
        case WebCore::DisplayList::ItemType::DrawImageBuffer:
        case WebCore::DisplayList::ItemType::DrawNativeImage:
        case WebCore::DisplayList::ItemType::DrawPattern:
        case WebCore::DisplayList::ItemType::DrawLine:
        case WebCore::DisplayList::ItemType::DrawRect:
        case WebCore::DisplayList::ItemType::EndTransparencyLayer:
        case WebCore::DisplayList::ItemType::FillEllipse:
#if ENABLE(INLINE_PATH_DATA)
        case WebCore::DisplayList::ItemType::FillInlinePath:
#endif
        case WebCore::DisplayList::ItemType::FillRect:
        case WebCore::DisplayList::ItemType::FlushContext:
        case WebCore::DisplayList::ItemType::MetaCommandSwitchTo:
        case WebCore::DisplayList::ItemType::PaintFrameForMedia:
        case WebCore::DisplayList::ItemType::Restore:
        case WebCore::DisplayList::ItemType::Rotate:
        case WebCore::DisplayList::ItemType::Save:
        case WebCore::DisplayList::ItemType::Scale:
        case WebCore::DisplayList::ItemType::SetCTM:
        case WebCore::DisplayList::ItemType::SetInlineFillColor:
        case WebCore::DisplayList::ItemType::SetInlineFillGradient:
        case WebCore::DisplayList::ItemType::SetInlineStrokeColor:
        case WebCore::DisplayList::ItemType::SetLineCap:
        case WebCore::DisplayList::ItemType::SetLineJoin:
        case WebCore::DisplayList::ItemType::SetMiterLimit:
        case WebCore::DisplayList::ItemType::SetStrokeThickness:
        case WebCore::DisplayList::ItemType::StrokeEllipse:
#if ENABLE(INLINE_PATH_DATA)
        case WebCore::DisplayList::ItemType::StrokeInlinePath:
#endif
        case WebCore::DisplayList::ItemType::StrokeRect:
        case WebCore::DisplayList::ItemType::StrokeLine:
        case WebCore::DisplayList::ItemType::Translate:
            RELEASE_ASSERT_NOT_REACHED();
            return nullptr;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return nullptr;
        }
    }

    void didAppendItemOfType(WebCore::DisplayList::ItemType type) override
    {
        if (type == WebCore::DisplayList::ItemType::DrawImageBuffer)
            flushDrawingContext();
    }

    std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher> createFlusher() override
    {
        return WTF::makeUnique<ThreadSafeRemoteImageBufferFlusher>(*this);
    }

    WebCore::DisplayList::FlushIdentifier m_sentFlushIdentifier;
    WebCore::DisplayList::FlushIdentifier m_receivedFlushIdentifier;
    WeakPtr<RemoteRenderingBackendProxy> m_remoteRenderingBackendProxy;
    size_t m_itemCountInCurrentDisplayList { 0 };
    WebCore::FloatSize m_size;
    WebCore::RenderingMode m_renderingMode;
    float m_resolutionScale;
    WebCore::ColorSpace m_colorSpace;
    WebCore::PixelFormat m_pixelFormat;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
