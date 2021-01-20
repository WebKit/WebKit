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
#include <WebCore/DisplayListReplayer.h>
#include <wtf/Condition.h>
#include <wtf/Lock.h>
#include <wtf/SystemTracing.h>

namespace WebKit {

class RemoteRenderingBackend;
template<typename BackendType> class ThreadSafeRemoteImageBufferFlusher;

template<typename BackendType>
class RemoteImageBufferProxy : public WebCore::DisplayList::ImageBuffer<BackendType>, public WebCore::DisplayList::Recorder::Delegate, public WebCore::DisplayList::ItemBufferWritingClient {
    using BaseDisplayListImageBuffer = WebCore::DisplayList::ImageBuffer<BackendType>;
    using BaseDisplayListImageBuffer::m_backend;
    using BaseDisplayListImageBuffer::m_drawingContext;
    using BaseDisplayListImageBuffer::m_renderingResourceIdentifier;
    using BaseDisplayListImageBuffer::resolutionScale;

public:
    static RefPtr<RemoteImageBufferProxy> create(const WebCore::FloatSize& size, float resolutionScale, WebCore::ColorSpace colorSpace, WebCore::PixelFormat pixelFormat, RemoteRenderingBackendProxy& remoteRenderingBackendProxy)
    {
        if (BackendType::calculateBackendSize(size, resolutionScale).isEmpty())
            return nullptr;

        auto parameters = WebCore::ImageBufferBackend::Parameters { size, resolutionScale, colorSpace, pixelFormat };
        return adoptRef(new RemoteImageBufferProxy(parameters, remoteRenderingBackendProxy));
    }

    ~RemoteImageBufferProxy()
    {
        if (!m_remoteRenderingBackendProxy)
            return;
        flushDrawingContext();
        m_remoteRenderingBackendProxy->remoteResourceCacheProxy().releaseImageBuffer(m_renderingResourceIdentifier);
        m_remoteRenderingBackendProxy->releaseRemoteResource(m_renderingResourceIdentifier);
    }

    ImageBufferBackendHandle createImageBufferBackendHandle()
    {
        ensureBackendCreated();
        return m_backend->createImageBufferBackendHandle();
    }

    WebCore::DisplayList::FlushIdentifier lastSentFlushIdentifier() const { return m_sentFlushIdentifier; }

    void waitForDidFlushOnSecondaryThread(WebCore::DisplayList::FlushIdentifier targetFlushIdentifier)
    {
        ASSERT(!isMainThread());
        auto locker = holdLock(m_receivedFlushIdentifierLock);
        m_receivedFlushIdentifierChangedCondition.wait(m_receivedFlushIdentifierLock, [&] {
            return m_receivedFlushIdentifier == targetFlushIdentifier;
        });

        // Nothing should have sent more drawing commands to the GPU process
        // while waiting for this ImageBuffer to be flushed.
        ASSERT(m_sentFlushIdentifier == targetFlushIdentifier);
    }

protected:
    RemoteImageBufferProxy(const WebCore::ImageBufferBackend::Parameters& parameters, RemoteRenderingBackendProxy& remoteRenderingBackendProxy)
        : BaseDisplayListImageBuffer(parameters, this)
        , m_remoteRenderingBackendProxy(makeWeakPtr(remoteRenderingBackendProxy))
    {
        ASSERT(m_remoteRenderingBackendProxy);
        m_remoteRenderingBackendProxy->remoteResourceCacheProxy().cacheImageBuffer(*this);

        m_drawingContext.displayList().setItemBufferClient(this);
        m_drawingContext.displayList().setTracksDrawingItemExtents(false);
    }

    bool hasPendingFlush() const { return m_sentFlushIdentifier != m_receivedFlushIdentifier; }

    void didFlush(WebCore::DisplayList::FlushIdentifier flushIdentifier) override
    {
        auto locker = holdLock(m_receivedFlushIdentifierLock);
        m_receivedFlushIdentifier = flushIdentifier;
        m_receivedFlushIdentifierChangedCondition.notifyAll();
    }

    void waitForDidFlushWithTimeout()
    {
        if (!m_remoteRenderingBackendProxy)
            return;

        // Wait for our DisplayList to be flushed but do not hang.
        static constexpr unsigned maximumNumberOfTimeouts = 3;
        unsigned numberOfTimeouts = 0;
        while (numberOfTimeouts < maximumNumberOfTimeouts && hasPendingFlush()) {
            if (!m_remoteRenderingBackendProxy->waitForDidFlush())
                ++numberOfTimeouts;
        }
    }

    WebCore::ImageBufferBackend* ensureBackendCreated() const override
    {
        if (!m_remoteRenderingBackendProxy)
            return m_backend.get();

        static constexpr unsigned maximumTimeoutOrFailureCount = 3;
        unsigned numberOfTimeoutsOrFailures = 0;
        while (!m_backend && numberOfTimeoutsOrFailures < maximumTimeoutOrFailureCount) {
            if (m_remoteRenderingBackendProxy->waitForDidCreateImageBufferBackend() == RemoteRenderingBackendProxy::DidReceiveBackendCreationResult::TimeoutOrIPCFailure)
                ++numberOfTimeoutsOrFailures;
        }
        return m_backend.get();
    }

    RefPtr<WebCore::ImageData> getImageData(WebCore::AlphaPremultiplication outputFormat, const WebCore::IntRect& srcRect) const override
    {
        if (UNLIKELY(!m_remoteRenderingBackendProxy))
            return nullptr;

        return m_remoteRenderingBackendProxy->getImageData(outputFormat, srcRect, m_renderingResourceIdentifier);
    }

    String toDataURL(const String& mimeType, Optional<double> quality, WebCore::PreserveResolution preserveResolution) const override
    {
        if (UNLIKELY(!m_remoteRenderingBackendProxy))
            return { };

        return m_remoteRenderingBackendProxy->getDataURLForImageBuffer(mimeType, quality, preserveResolution, m_renderingResourceIdentifier);
    }

    Vector<uint8_t> toData(const String& mimeType, Optional<double> quality = WTF::nullopt) const override
    {
        if (UNLIKELY(!m_remoteRenderingBackendProxy))
            return { };

        return m_remoteRenderingBackendProxy->getDataForImageBuffer(mimeType, quality, m_renderingResourceIdentifier);
    }

    Vector<uint8_t> toBGRAData() const override
    {
        if (UNLIKELY(!m_remoteRenderingBackendProxy))
            return { };

        return m_remoteRenderingBackendProxy->getBGRADataForImageBuffer(m_renderingResourceIdentifier);
    }

    void putImageData(WebCore::AlphaPremultiplication inputFormat, const WebCore::ImageData& imageData, const WebCore::IntRect& srcRect, const WebCore::IntPoint& destPoint = { }, WebCore::AlphaPremultiplication destFormat = WebCore::AlphaPremultiplication::Premultiplied) override
    {
        // The math inside ImageData::create() doesn't agree with the math inside ImageBufferBackend::putImageData() about how m_resolutionScale interacts with the data in the ImageBuffer.
        // This means that putImageData() is only called when resolutionScale() == 1.
        ASSERT(resolutionScale() == 1);
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
        if (UNLIKELY(!m_remoteRenderingBackendProxy))
            return;

        TraceScope tracingScope(FlushRemoteImageBufferStart, FlushRemoteImageBufferEnd);
        flushDrawingContextAsync();
        waitForDidFlushWithTimeout();
    }

    void flushDrawingContextAsync() override
    {
        if (UNLIKELY(!m_remoteRenderingBackendProxy))
            return;

        if (!m_drawingContext.displayList().isEmpty()) {
            m_sentFlushIdentifier = WebCore::DisplayList::FlushIdentifier::generate();
            m_drawingContext.recorder().flushContext(m_sentFlushIdentifier);
        }

        m_remoteRenderingBackendProxy->sendDeferredWakeupMessageIfNeeded();
        clearDisplayList();
    }

    void cacheNativeImage(WebCore::NativeImage& image) override
    {
        if (m_remoteRenderingBackendProxy)
            m_remoteRenderingBackendProxy->remoteResourceCacheProxy().cacheNativeImage(image);
    }

    bool isCachedImageBuffer(const WebCore::ImageBuffer& imageBuffer) const override
    {
        if (!m_remoteRenderingBackendProxy)
            return false;
        auto cachedImageBuffer = m_remoteRenderingBackendProxy->remoteResourceCacheProxy().cachedImageBuffer(imageBuffer.renderingResourceIdentifier());
        ASSERT(!cachedImageBuffer || cachedImageBuffer == &imageBuffer);
        return cachedImageBuffer;
    }

    void changeDestinationImageBuffer(WebCore::RenderingResourceIdentifier nextImageBuffer) final
    {
        bool wasEmpty = m_drawingContext.displayList().isEmpty();
        m_drawingContext.displayList().template append<WebCore::DisplayList::MetaCommandChangeDestinationImageBuffer>(nextImageBuffer);
        if (wasEmpty)
            clearDisplayList();
    }

    void prepareToAppendDisplayListItems(WebCore::DisplayList::ItemBufferHandle&& handle) final
    {
        m_drawingContext.displayList().prepareToAppend(WTFMove(handle));
    }

    void clearDisplayList()
    {
        m_drawingContext.displayList().clear();
    }

    void willAppendItemOfType(WebCore::DisplayList::ItemType) override
    {
        if (LIKELY(m_remoteRenderingBackendProxy))
            m_remoteRenderingBackendProxy->willAppendItem(m_renderingResourceIdentifier);
    }

    void didAppendData(const WebCore::DisplayList::ItemBufferHandle& handle, size_t numberOfBytes, WebCore::DisplayList::DidChangeItemBuffer didChangeItemBuffer) override
    {
        if (LIKELY(m_remoteRenderingBackendProxy))
            m_remoteRenderingBackendProxy->didAppendData(handle, numberOfBytes, didChangeItemBuffer, m_renderingResourceIdentifier);
    }

    void cacheFont(WebCore::Font& font) override
    {
        if (m_remoteRenderingBackendProxy)
            m_remoteRenderingBackendProxy->remoteResourceCacheProxy().cacheFont(font);
    }

    WebCore::DisplayList::ItemBufferHandle createItemBuffer(size_t capacity) override
    {
        if (LIKELY(m_remoteRenderingBackendProxy))
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
        case WebCore::DisplayList::ItemType::MetaCommandChangeDestinationImageBuffer:
        case WebCore::DisplayList::ItemType::MetaCommandChangeItemBuffer:
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

    std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher> createFlusher() override
    {
        return WTF::makeUnique<ThreadSafeRemoteImageBufferFlusher<BackendType>>(*this);
    }

    WebCore::DisplayList::FlushIdentifier m_sentFlushIdentifier;
    Lock m_receivedFlushIdentifierLock;
    Condition m_receivedFlushIdentifierChangedCondition;
    WebCore::DisplayList::FlushIdentifier m_receivedFlushIdentifier;
    WeakPtr<RemoteRenderingBackendProxy> m_remoteRenderingBackendProxy;
};

template<typename BackendType>
class ThreadSafeRemoteImageBufferFlusher final : public WebCore::ThreadSafeImageBufferFlusher {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ThreadSafeRemoteImageBufferFlusher(RemoteImageBufferProxy<BackendType>& imageBuffer)
        : m_imageBuffer(imageBuffer)
        , m_targetFlushIdentifier(imageBuffer.lastSentFlushIdentifier())
    {
    }

    void flush() final
    {
        m_imageBuffer->waitForDidFlushOnSecondaryThread(m_targetFlushIdentifier);
    }

private:
    Ref<RemoteImageBufferProxy<BackendType>> m_imageBuffer;
    WebCore::DisplayList::FlushIdentifier m_targetFlushIdentifier;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
