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
#include "Logging.h"
#include "RemoteDisplayListRecorderProxy.h"
#include "RemoteRenderingBackendMessages.h"
#include "RemoteRenderingBackendProxy.h"
#include "SharedMemory.h"
#include <WebCore/ConcreteImageBuffer.h>
#include <WebCore/DisplayList.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/MIMETypeRegistry.h>
#include <wtf/Condition.h>
#include <wtf/Lock.h>
#include <wtf/SystemTracing.h>

namespace WebKit {

class RemoteRenderingBackend;
template<typename BackendType> class ThreadSafeRemoteImageBufferFlusher;

template<typename BackendType>
class RemoteImageBufferProxy : public WebCore::ConcreteImageBuffer<BackendType> {
    using BaseConcreteImageBuffer = WebCore::ConcreteImageBuffer<BackendType>;
    using BaseConcreteImageBuffer::m_backend;
    using BaseConcreteImageBuffer::m_renderingResourceIdentifier;
    using BaseConcreteImageBuffer::resolutionScale;

public:
    static RefPtr<RemoteImageBufferProxy> create(const WebCore::FloatSize& size, float resolutionScale, const WebCore::DestinationColorSpace& colorSpace, WebCore::PixelFormat pixelFormat, RemoteRenderingBackendProxy& remoteRenderingBackendProxy)
    {
        auto parameters = WebCore::ImageBufferBackend::Parameters { size, resolutionScale, colorSpace, pixelFormat };
        if (BackendType::calculateSafeBackendSize(parameters).isEmpty())
            return nullptr;
        return adoptRef(new RemoteImageBufferProxy(parameters, remoteRenderingBackendProxy));
    }

    ~RemoteImageBufferProxy()
    {
        if (!m_remoteRenderingBackendProxy || m_remoteRenderingBackendProxy->isGPUProcessConnectionClosed()) {
            m_remoteDisplayList.resetNeedsFlush();
            return;
        }

        flushDrawingContext();
        m_remoteRenderingBackendProxy->remoteResourceCacheProxy().releaseImageBuffer(m_renderingResourceIdentifier);
    }

    WebCore::GraphicsContextFlushIdentifier lastSentFlushIdentifier() const { return m_sentFlushIdentifier; }

    void waitForDidFlushOnSecondaryThread(WebCore::GraphicsContextFlushIdentifier targetFlushIdentifier)
    {
        ASSERT(!isMainRunLoop());
        Locker locker { m_receivedFlushIdentifierLock };
        m_receivedFlushIdentifierChangedCondition.wait(m_receivedFlushIdentifierLock, [&] {
            assertIsHeld(m_receivedFlushIdentifierLock);
            return m_receivedFlushIdentifier == targetFlushIdentifier;
        });

        // Nothing should have sent more drawing commands to the GPU process
        // while waiting for this ImageBuffer to be flushed.
        ASSERT(m_sentFlushIdentifier == targetFlushIdentifier);
    }

protected:
    RemoteImageBufferProxy(const WebCore::ImageBufferBackend::Parameters& parameters, RemoteRenderingBackendProxy& remoteRenderingBackendProxy)
        : BaseConcreteImageBuffer(parameters)
        , m_remoteRenderingBackendProxy(remoteRenderingBackendProxy)
        , m_remoteDisplayList(*this, remoteRenderingBackendProxy, { { }, BaseConcreteImageBuffer::logicalSize() }, BaseConcreteImageBuffer::baseTransform())
    {
        ASSERT(m_remoteRenderingBackendProxy);
        m_remoteRenderingBackendProxy->remoteResourceCacheProxy().cacheImageBuffer(*this);
    }

    // It is safe to access m_receivedFlushIdentifier from the main thread without locking since it
    // only gets modified on the main thread.
    bool hasPendingFlush() const WTF_IGNORES_THREAD_SAFETY_ANALYSIS
    {
        ASSERT(isMainRunLoop());
        return m_sentFlushIdentifier != m_receivedFlushIdentifier;
    }

    void didFlush(WebCore::GraphicsContextFlushIdentifier flushIdentifier) final
    {
        ASSERT(isMainRunLoop());
        Locker locker { m_receivedFlushIdentifierLock };
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
#if !LOG_DISABLED
        auto startTime = MonotonicTime::now();
#endif
        LOG_WITH_STREAM(SharedDisplayLists, stream << "Waiting for Flush{" << m_sentFlushIdentifier << "} in Image(" << m_renderingResourceIdentifier << ")");
        while (numberOfTimeouts < maximumNumberOfTimeouts && hasPendingFlush()) {
            if (!m_remoteRenderingBackendProxy->waitForDidFlush())
                ++numberOfTimeouts;
        }
        LOG_WITH_STREAM(SharedDisplayLists, stream << "Done waiting: " << MonotonicTime::now() - startTime << "; " << numberOfTimeouts << " timeout(s)");

        if (UNLIKELY(numberOfTimeouts >= maximumNumberOfTimeouts))
            RELEASE_LOG_FAULT(SharedDisplayLists, "Exceeded timeout while waiting for flush in remote rendering backend: %" PRIu64 ".", m_remoteRenderingBackendProxy->renderingBackendIdentifier().toUInt64());
    }

    WebCore::ImageBufferBackend* ensureBackendCreated() const final
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

    String toDataURL(const String& mimeType, std::optional<double> quality, WebCore::PreserveResolution preserveResolution) const final
    {
        if (UNLIKELY(!m_remoteRenderingBackendProxy))
            return { };

        ASSERT(WebCore::MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));
        return m_remoteRenderingBackendProxy->getDataURLForImageBuffer(mimeType, quality, preserveResolution, m_renderingResourceIdentifier);
    }

    Vector<uint8_t> toData(const String& mimeType, std::optional<double> quality = std::nullopt) const final
    {
        if (UNLIKELY(!m_remoteRenderingBackendProxy))
            return { };

        ASSERT(WebCore::MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));
        return m_remoteRenderingBackendProxy->getDataForImageBuffer(mimeType, quality, m_renderingResourceIdentifier);
    }

    RefPtr<WebCore::NativeImage> copyNativeImage(WebCore::BackingStoreCopy = WebCore::BackingStoreCopy::CopyBackingStore) const final
    {
        if (UNLIKELY(!m_remoteRenderingBackendProxy))
            return { };
        const_cast<RemoteImageBufferProxy*>(this)->flushDrawingContext();
        auto bitmap = m_remoteRenderingBackendProxy->getShareableBitmap(m_renderingResourceIdentifier, WebCore::PreserveResolution::Yes);
        if (!bitmap)
            return { };
        return WebCore::NativeImage::create(bitmap->createPlatformImage());
    }

    RefPtr<WebCore::Image> copyImage(WebCore::BackingStoreCopy = WebCore::BackingStoreCopy::CopyBackingStore, WebCore::PreserveResolution preserveResolution = WebCore::PreserveResolution::No) const final
    {
        if (UNLIKELY(!m_remoteRenderingBackendProxy))
            return { };
        const_cast<RemoteImageBufferProxy*>(this)->flushDrawingContext();
        auto bitmap = m_remoteRenderingBackendProxy->getShareableBitmap(m_renderingResourceIdentifier, preserveResolution);
        if (!bitmap)
            return { };
        return bitmap->createImage();
    }

    RefPtr<WebCore::NativeImage> sinkIntoNativeImage() final
    {
        return copyNativeImage();
    }

    RefPtr<WebCore::Image> sinkIntoImage(WebCore::PreserveResolution preserveResolution = WebCore::PreserveResolution::No) final
    {
        return copyImage(WebCore::BackingStoreCopy::CopyBackingStore, preserveResolution);
    }

    RefPtr<WebCore::Image> filteredImage(WebCore::Filter& filter) final
    {
        if (UNLIKELY(!m_remoteRenderingBackendProxy))
            return { };
        flushDrawingContext();
        return m_remoteRenderingBackendProxy->getFilteredImage(m_renderingResourceIdentifier, filter);
    }

    std::optional<WebCore::PixelBuffer> getPixelBuffer(const WebCore::PixelBufferFormat& destinationFormat, const WebCore::IntRect& srcRect) const final
    {
        if (UNLIKELY(!m_remoteRenderingBackendProxy))
            return std::nullopt;
        auto& mutableThis = const_cast<RemoteImageBufferProxy&>(*this);
        mutableThis.flushDrawingContextAsync();
        auto pixelBuffer = WebCore::PixelBuffer::tryCreate(destinationFormat, srcRect.size());
        if (!pixelBuffer)
            return std::nullopt;
        auto& data = pixelBuffer->data();
        if (!m_remoteRenderingBackendProxy->getPixelBufferForImageBuffer(m_renderingResourceIdentifier, destinationFormat, srcRect, { data.data(), data.byteLength() }))
            return std::nullopt;
        return pixelBuffer;
    }

    void clearBackend() final
    {
        m_remoteDisplayList.resetNeedsFlush();
        BaseConcreteImageBuffer::clearBackend();
    }

    WebCore::GraphicsContext& context() const final
    {
        return const_cast<RemoteImageBufferProxy*>(this)->m_remoteDisplayList;
    }

    WebCore::GraphicsContext* drawingContext() final
    {
        return &m_remoteDisplayList;
    }

    void putPixelBuffer(const WebCore::PixelBuffer& pixelBuffer, const WebCore::IntRect& srcRect, const WebCore::IntPoint& destPoint = { }, WebCore::AlphaPremultiplication destFormat = WebCore::AlphaPremultiplication::Premultiplied) final
    {
        if (UNLIKELY(!m_remoteRenderingBackendProxy))
            return;
        // The math inside PixelBuffer::create() doesn't agree with the math inside ImageBufferBackend::putPixelBuffer() about how m_resolutionScale interacts with the data in the ImageBuffer.
        // This means that putPixelBuffer() is only called when resolutionScale() == 1.
        ASSERT(resolutionScale() == 1);
        auto& mutableThis = const_cast<RemoteImageBufferProxy&>(*this);
        mutableThis.flushDrawingContextAsync();
        m_remoteRenderingBackendProxy->putPixelBufferForImageBuffer(m_renderingResourceIdentifier, pixelBuffer, srcRect, destPoint, destFormat);
    }

    void convertToLuminanceMask() final
    {
        m_remoteDisplayList.convertToLuminanceMask();
    }

    void transformToColorSpace(const WebCore::DestinationColorSpace& colorSpace) final
    {
        m_remoteDisplayList.transformToColorSpace(colorSpace);
    }

    bool prefersPreparationForDisplay() final { return true; }

    void flushContext() final
    {
        flushDrawingContext();
        m_backend->flushContext();
    }

    void flushDrawingContext() final
    {
        if (UNLIKELY(!m_remoteRenderingBackendProxy))
            return;

        TraceScope tracingScope(FlushRemoteImageBufferStart, FlushRemoteImageBufferEnd);
        flushDrawingContextAsync();
        waitForDidFlushWithTimeout();
    }

    void flushDrawingContextAsync() final
    {
        if (UNLIKELY(!m_remoteRenderingBackendProxy))
            return;

        if (m_remoteDisplayList.needsFlush() || !hasPendingFlush()) {
            m_sentFlushIdentifier = WebCore::GraphicsContextFlushIdentifier::generate();
            m_remoteDisplayList.flushContext(m_sentFlushIdentifier);
        }

        m_remoteDisplayList.resetNeedsFlush();
    }

    void recordNativeImageUse(WebCore::NativeImage& image)
    {
        if (m_remoteRenderingBackendProxy)
            m_remoteRenderingBackendProxy->remoteResourceCacheProxy().recordNativeImageUse(image);
    }

    void recordFontUse(WebCore::Font& font)
    {
        if (m_remoteRenderingBackendProxy)
            m_remoteRenderingBackendProxy->remoteResourceCacheProxy().recordFontUse(font);
    }

    void recordImageBufferUse(WebCore::ImageBuffer& imageBuffer)
    {
        if (m_remoteRenderingBackendProxy)
            m_remoteRenderingBackendProxy->remoteResourceCacheProxy().recordImageBufferUse(imageBuffer);
    }

    std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher> createFlusher() final
    {
        return WTF::makeUnique<ThreadSafeRemoteImageBufferFlusher<BackendType>>(*this);
    }

    WebCore::GraphicsContextFlushIdentifier m_sentFlushIdentifier;
    Lock m_receivedFlushIdentifierLock;
    Condition m_receivedFlushIdentifierChangedCondition;
    WebCore::GraphicsContextFlushIdentifier m_receivedFlushIdentifier WTF_GUARDED_BY_LOCK(m_receivedFlushIdentifierLock); // Only modified on the main thread but may get queried on a secondary thread.
    WeakPtr<RemoteRenderingBackendProxy> m_remoteRenderingBackendProxy;
    RemoteDisplayListRecorderProxy m_remoteDisplayList;
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
    WebCore::GraphicsContextFlushIdentifier m_targetFlushIdentifier;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
