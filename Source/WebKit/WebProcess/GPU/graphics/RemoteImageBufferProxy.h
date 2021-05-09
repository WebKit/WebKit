/*
 * Copyright (C) 2020-2021 Apple Inc.  All rights reserved.
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
#include <WebCore/MIMETypeRegistry.h>
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
    static RefPtr<RemoteImageBufferProxy> create(const WebCore::FloatSize& size, float resolutionScale, WebCore::DestinationColorSpace colorSpace, WebCore::PixelFormat pixelFormat, RemoteRenderingBackendProxy& remoteRenderingBackendProxy)
    {
        auto parameters = WebCore::ImageBufferBackend::Parameters { size, resolutionScale, colorSpace, pixelFormat };
        if (BackendType::calculateSafeBackendSize(parameters).isEmpty())
            return nullptr;
        return adoptRef(new RemoteImageBufferProxy(parameters, remoteRenderingBackendProxy));
    }

    ~RemoteImageBufferProxy()
    {
        if (!m_remoteRenderingBackendProxy) {
            clearDisplayList();
            return;
        }

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
        ASSERT(!isMainRunLoop());
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

        m_drawingContext.displayList().setItemBufferWritingClient(this);
        m_drawingContext.displayList().setItemBufferReadingClient(nullptr);
        m_drawingContext.displayList().setTracksDrawingItemExtents(false);
    }

    WebCore::RenderingMode renderingMode() const override { return BaseDisplayListImageBuffer::renderingMode(); }

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

    String toDataURL(const String& mimeType, Optional<double> quality, WebCore::PreserveResolution preserveResolution) const override
    {
        if (UNLIKELY(!m_remoteRenderingBackendProxy))
            return { };

        ASSERT(WebCore::MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));
        return m_remoteRenderingBackendProxy->getDataURLForImageBuffer(mimeType, quality, preserveResolution, m_renderingResourceIdentifier);
    }

    Vector<uint8_t> toData(const String& mimeType, Optional<double> quality = WTF::nullopt) const override
    {
        if (UNLIKELY(!m_remoteRenderingBackendProxy))
            return { };

        ASSERT(WebCore::MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));
        return m_remoteRenderingBackendProxy->getDataForImageBuffer(mimeType, quality, m_renderingResourceIdentifier);
    }

    Vector<uint8_t> toBGRAData() const override
    {
        if (UNLIKELY(!m_remoteRenderingBackendProxy))
            return { };

        return m_remoteRenderingBackendProxy->getBGRADataForImageBuffer(m_renderingResourceIdentifier);
    }
    RefPtr<WebCore::NativeImage> copyNativeImage(WebCore::BackingStoreCopy = WebCore::BackingStoreCopy::CopyBackingStore) const override
    {
        if (UNLIKELY(!m_remoteRenderingBackendProxy))
            return { };
        const_cast<RemoteImageBufferProxy*>(this)->flushDrawingContext();
        auto bitmap = m_remoteRenderingBackendProxy->getShareableBitmap(m_renderingResourceIdentifier, WebCore::PreserveResolution::Yes);
        if (!bitmap)
            return { };
        return WebCore::NativeImage::create(bitmap->createPlatformImage());
    }

    RefPtr<WebCore::Image> copyImage(WebCore::BackingStoreCopy = WebCore::BackingStoreCopy::CopyBackingStore, WebCore::PreserveResolution preserveResolution = WebCore::PreserveResolution::No) const override
    {
        if (UNLIKELY(!m_remoteRenderingBackendProxy))
            return { };
        const_cast<RemoteImageBufferProxy*>(this)->flushDrawingContext();
        auto bitmap = m_remoteRenderingBackendProxy->getShareableBitmap(m_renderingResourceIdentifier, preserveResolution);
        if (!bitmap)
            return { };
        return bitmap->createImage();
    }

    RefPtr<WebCore::ImageData> getImageData(WebCore::AlphaPremultiplication outputFormat, const WebCore::IntRect& srcRect) const override
    {
        if (UNLIKELY(!m_remoteRenderingBackendProxy))
            return nullptr;

        auto imageData = WebCore::ImageData::create(srcRect.size());
        if (!imageData)
            return nullptr;
        size_t dataSize = imageData->data().byteLength();

        IPC::Timeout timeout = 5_s;
        SharedMemory* sharedMemory = m_remoteRenderingBackendProxy->sharedMemoryForGetImageData(dataSize, timeout);
        if (!sharedMemory)
            return nullptr;

        auto& mutableThis = const_cast<RemoteImageBufferProxy&>(*this);
        mutableThis.m_drawingContext.recorder().getImageData(outputFormat, srcRect);
        mutableThis.flushDrawingContextAsync();

        if (m_remoteRenderingBackendProxy->waitForGetImageDataToComplete(timeout))
            memcpy(imageData->data().data(), sharedMemory->data(), dataSize);
        else
            memset(imageData->data().data(), 0, dataSize);
        return imageData;
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

    bool canAppendItemOfType(WebCore::DisplayList::ItemType) override
    {
        if (UNLIKELY(!m_remoteRenderingBackendProxy))
            return false;
        m_remoteRenderingBackendProxy->willAppendItem(m_renderingResourceIdentifier);
        return true;
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

    WebCore::DisplayList::ItemBufferHandle createItemBuffer(size_t capacity) final
    {
        if (LIKELY(m_remoteRenderingBackendProxy))
            return m_remoteRenderingBackendProxy->createItemBuffer(capacity, m_renderingResourceIdentifier);

        ASSERT_NOT_REACHED();
        return { };
    }

    RefPtr<WebCore::SharedBuffer> encodeItemOutOfLine(const WebCore::DisplayList::DisplayListItem& item) const final
    {
        return WTF::visit([](const auto& displayListItem) -> RefPtr<WebCore::SharedBuffer> {
            using DisplayListItemType = typename WTF::RemoveCVAndReference<decltype(displayListItem)>::type;
            if constexpr (!DisplayListItemType::isInlineItem)
                return IPC::Encoder::encodeSingleObject<DisplayListItemType>(displayListItem);
            RELEASE_ASSERT_NOT_REACHED();
            return nullptr;
        }, item);
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
