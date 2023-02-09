/*
 * Copyright (C) 2020-2022 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "RemoteImageBufferProxy.h"

#if ENABLE(GPU_PROCESS)

#include "GPUConnectionToWebProcessMessages.h"
#include "Logging.h"
#include "PlatformImageBufferShareableBackend.h"
#include "RemoteRenderingBackendProxy.h"
#include "WebPage.h"
#include "WebWorkerClient.h"
#include <WebCore/Document.h>
#include <WebCore/WorkerGlobalScope.h>
#include <wtf/SystemTracing.h>

namespace WebKit {
using namespace WebCore;

namespace {

class RemoteImageBufferProxyFlusher final : public ThreadSafeImageBufferFlusher {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RemoteImageBufferProxyFlusher(RemoteImageBufferProxyFlushState& flushState, DisplayListRecorderFlushIdentifier identifier)
        : m_flushState(flushState)
        , m_targetFlushIdentifier(identifier)
    {
    }

    void flush() final
    {
        m_flushState->waitForDidFlushOnSecondaryThread(m_targetFlushIdentifier);
    }

private:
    Ref<RemoteImageBufferProxyFlushState> m_flushState;
    DisplayListRecorderFlushIdentifier m_targetFlushIdentifier;
};

}

RemoteImageBufferProxy::RemoteImageBufferProxy(const ImageBufferBackend::Parameters& parameters, const ImageBufferBackend::Info& info, RemoteRenderingBackendProxy& remoteRenderingBackendProxy, std::unique_ptr<ImageBufferBackend>&& backend, RenderingResourceIdentifier identifier)
    : ImageBuffer(parameters, info, WTFMove(backend), identifier)
    , m_flushState(adoptRef(*new RemoteImageBufferProxyFlushState))
    , m_remoteRenderingBackendProxy(remoteRenderingBackendProxy)
    , m_remoteDisplayList(*this, remoteRenderingBackendProxy, { { }, ImageBuffer::logicalSize() }, ImageBuffer::baseTransform())
{
    ASSERT(m_remoteRenderingBackendProxy);
    m_remoteRenderingBackendProxy->remoteResourceCacheProxy().cacheImageBuffer(*this);
}

RemoteImageBufferProxy::~RemoteImageBufferProxy()
{
    if (!m_remoteRenderingBackendProxy || m_remoteRenderingBackendProxy->isGPUProcessConnectionClosed()) {
        m_needsFlush = false;
        return;
    }

    flushDrawingContextAsync();
    m_remoteRenderingBackendProxy->remoteResourceCacheProxy().releaseImageBuffer(*this);
}

void RemoteImageBufferProxy::assertDispatcherIsCurrent() const
{
    if (m_remoteRenderingBackendProxy)
        assertIsCurrent(m_remoteRenderingBackendProxy->dispatcher());
}

bool RemoteImageBufferProxy::hasPendingFlush() const
{
    assertDispatcherIsCurrent();
    return m_sentFlushIdentifier != m_flushState->identifierForCompletedFlush();
}

void RemoteImageBufferProxy::backingStoreWillChange()
{
    if (m_needsFlush)
        return;
    m_needsFlush = true;

    // Prepare for the backing store change the first time this notification comes after flush has
    // completed.

    // If we already need a flush, this cannot be the first notification for change,
    // handled by the m_needsFlush case above.

    // If we already have a pending flush, this cannot be the first notification for change.
    if (hasPendingFlush())
        return;

    prepareForBackingStoreChange();
}

void RemoteImageBufferProxy::didCreateImageBufferBackend(ImageBufferBackendHandle&& handle)
{
    ASSERT(!m_backend);
    if (renderingMode() == RenderingMode::Accelerated && std::holds_alternative<ShareableBitmapHandle>(handle))
        m_backendInfo = ImageBuffer::populateBackendInfo<UnacceleratedImageBufferShareableBackend>(parameters());
    
    if (renderingMode() == RenderingMode::Unaccelerated)
        m_backend = UnacceleratedImageBufferShareableBackend::create(parameters(), WTFMove(handle));
    else if (canMapBackingStore())
        m_backend = AcceleratedImageBufferShareableMappedBackend::create(parameters(), WTFMove(handle));
    else
        m_backend = AcceleratedImageBufferRemoteBackend::create(parameters(), WTFMove(handle));
}

void RemoteImageBufferProxy::waitForDidFlushWithTimeout()
{
    if (!m_remoteRenderingBackendProxy)
        return;

    // Wait for our DisplayList to be flushed but do not hang.
    static constexpr unsigned maximumNumberOfTimeouts = 3;
    unsigned numberOfTimeouts = 0;
#if !LOG_DISABLED
    auto startTime = MonotonicTime::now();
#endif
    LOG_WITH_STREAM(SharedDisplayLists, stream << "RemoteImageBufferProxy " << m_renderingResourceIdentifier << " waitForDidFlushWithTimeout: waiting for flush {" << m_sentFlushIdentifier);
    while (numberOfTimeouts < maximumNumberOfTimeouts && hasPendingFlush()) {
        if (!m_remoteRenderingBackendProxy->waitForDidFlush())
            ++numberOfTimeouts;
    }

    LOG_WITH_STREAM(SharedDisplayLists, stream << "RemoteImageBufferProxy " << m_renderingResourceIdentifier << " waitForDidFlushWithTimeout: done waiting " << (MonotonicTime::now() - startTime).milliseconds() << "ms; " << numberOfTimeouts << " timeout(s)");

    if (UNLIKELY(numberOfTimeouts >= maximumNumberOfTimeouts))
        RELEASE_LOG_FAULT(SharedDisplayLists, "Exceeded timeout while waiting for flush in remote rendering backend: %" PRIu64 ".", m_remoteRenderingBackendProxy->renderingBackendIdentifier().toUInt64());
}

ImageBufferBackend* RemoteImageBufferProxy::ensureBackendCreated() const
{
    if (!m_remoteRenderingBackendProxy)
        return m_backend.get();

    static constexpr unsigned maximumTimeoutOrFailureCount = 3;
    unsigned numberOfTimeoutsOrFailures = 0;
    while (!m_backend && numberOfTimeoutsOrFailures < maximumTimeoutOrFailureCount) {
        if (m_remoteRenderingBackendProxy->waitForDidCreateImageBufferBackend() == RemoteRenderingBackendProxy::DidReceiveBackendCreationResult::TimeoutOrIPCFailure)
            ++numberOfTimeoutsOrFailures;
    }
    if (numberOfTimeoutsOrFailures == maximumTimeoutOrFailureCount) {
        LOG_WITH_STREAM(SharedDisplayLists, stream << "RemoteImageBufferProxy " << m_renderingResourceIdentifier << " ensureBackendCreated: exceeded max number of timeouts");
        RELEASE_LOG_FAULT(SharedDisplayLists, "Exceeded max number of timeouts waiting for image buffer backend creation in remote rendering backend %" PRIu64 ".", m_remoteRenderingBackendProxy->renderingBackendIdentifier().toUInt64());
    }
    return m_backend.get();
}

RefPtr<NativeImage> RemoteImageBufferProxy::copyNativeImage(BackingStoreCopy copyBehavior) const
{
    if (canMapBackingStore())
        return ImageBuffer::copyNativeImage(copyBehavior);

    if (UNLIKELY(!m_remoteRenderingBackendProxy))
        return { };

    const_cast<RemoteImageBufferProxy*>(this)->flushDrawingContext();
    auto bitmap = m_remoteRenderingBackendProxy->getShareableBitmap(m_renderingResourceIdentifier, PreserveResolution::Yes);
    if (!bitmap)
        return { };
    return NativeImage::create(bitmap->createPlatformImage(DontCopyBackingStore));
}

RefPtr<NativeImage> RemoteImageBufferProxy::copyNativeImageForDrawing(BackingStoreCopy copyBehavior) const
{
    if (canMapBackingStore())
        return ImageBuffer::copyNativeImageForDrawing(copyBehavior);
    return copyNativeImage(copyBehavior);
}

void RemoteImageBufferProxy::drawConsuming(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    ASSERT(&destContext != &context());
    destContext.drawImageBuffer(*this, destRect, srcRect, options);
}

RefPtr<NativeImage> RemoteImageBufferProxy::sinkIntoNativeImage()
{
    return copyNativeImage();
}

RefPtr<ImageBuffer> RemoteImageBufferProxy::sinkIntoBufferForDifferentThread()
{
    ASSERT(hasOneRef());
    // We can't use these on a different thread, so make a local clone instead.
    return cloneForDifferentThread();
}

RefPtr<ImageBuffer> RemoteImageBufferProxy::cloneForDifferentThread()
{
    auto copyBuffer = ImageBuffer::create(logicalSize(), renderingPurpose(), resolutionScale(), colorSpace(), pixelFormat());
    if (!copyBuffer)
        return nullptr;

    copyBuffer->context().drawImageBuffer(*this, FloatPoint { }, CompositeOperator::Copy);
    return copyBuffer;
}

RefPtr<Image> RemoteImageBufferProxy::filteredImage(Filter& filter)
{
    if (UNLIKELY(!m_remoteRenderingBackendProxy))
        return { };
    flushDrawingContext();
    return m_remoteRenderingBackendProxy->getFilteredImage(m_renderingResourceIdentifier, filter);
}

RefPtr<PixelBuffer> RemoteImageBufferProxy::getPixelBuffer(const PixelBufferFormat& destinationFormat, const IntRect& srcRect, const ImageBufferAllocator& allocator) const
{
    if (UNLIKELY(!m_remoteRenderingBackendProxy))
        return nullptr;
    auto& mutableThis = const_cast<RemoteImageBufferProxy&>(*this);
    mutableThis.flushDrawingContextAsync();
    IntRect sourceRectScaled = srcRect;
    sourceRectScaled.scale(resolutionScale());
    auto pixelBuffer = allocator.createPixelBuffer(destinationFormat, sourceRectScaled.size());
    if (!pixelBuffer)
        return nullptr;
    if (!m_remoteRenderingBackendProxy->getPixelBufferForImageBuffer(m_renderingResourceIdentifier, destinationFormat, srcRect, { pixelBuffer->bytes(), pixelBuffer->sizeInBytes() }))
        return nullptr;
    return pixelBuffer;
}

void RemoteImageBufferProxy::clearBackend()
{
    m_needsFlush = false;
    m_sentFlushIdentifier = { };
    m_flushState->cancel();
    prepareForBackingStoreChange();
    m_backend = nullptr;
}

GraphicsContext& RemoteImageBufferProxy::context() const
{
    return const_cast<RemoteImageBufferProxy*>(this)->m_remoteDisplayList;
}

void RemoteImageBufferProxy::putPixelBuffer(const PixelBuffer& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    if (UNLIKELY(!m_remoteRenderingBackendProxy))
        return;
    // The math inside PixelBuffer::create() doesn't agree with the math inside ImageBufferBackend::putPixelBuffer() about how m_resolutionScale interacts with the data in the ImageBuffer.
    // This means that putPixelBuffer() is only called when resolutionScale() == 1.
    ASSERT(resolutionScale() == 1);
    auto& mutableThis = const_cast<RemoteImageBufferProxy&>(*this);
    mutableThis.flushDrawingContextAsync();
    backingStoreWillChange();
    m_remoteRenderingBackendProxy->putPixelBufferForImageBuffer(m_renderingResourceIdentifier, pixelBuffer, srcRect, destPoint, destFormat);
}

void RemoteImageBufferProxy::convertToLuminanceMask()
{
    m_remoteDisplayList.convertToLuminanceMask();
}

void RemoteImageBufferProxy::transformToColorSpace(const DestinationColorSpace& colorSpace)
{
    m_remoteDisplayList.transformToColorSpace(colorSpace);
}

void RemoteImageBufferProxy::flushContext()
{
    flushDrawingContext();
    m_backend->flushContext();
}

void RemoteImageBufferProxy::flushDrawingContext()
{
    if (UNLIKELY(!m_remoteRenderingBackendProxy))
        return;

    TraceScope tracingScope(FlushRemoteImageBufferStart, FlushRemoteImageBufferEnd);

    bool shouldWait = flushDrawingContextAsync();
    LOG_WITH_STREAM(SharedDisplayLists, stream << "RemoteImageBufferProxy " << m_renderingResourceIdentifier << " flushDrawingContext: shouldWait " << shouldWait);
    if (shouldWait)
        waitForDidFlushWithTimeout();
}

bool RemoteImageBufferProxy::flushDrawingContextAsync()
{
    if (UNLIKELY(!m_remoteRenderingBackendProxy))
        return false;

    if (!m_needsFlush)
        return hasPendingFlush();

    m_sentFlushIdentifier = DisplayListRecorderFlushIdentifier::generate();
    LOG_WITH_STREAM(SharedDisplayLists, stream << "RemoteImageBufferProxy " << m_renderingResourceIdentifier << " flushDrawingContextAsync - flush " << m_sentFlushIdentifier);
    m_remoteDisplayList.flushContext(m_sentFlushIdentifier);
    m_remoteRenderingBackendProxy->addPendingFlush(m_flushState, m_sentFlushIdentifier);
    m_needsFlush = false;
    return true;
}

std::unique_ptr<ThreadSafeImageBufferFlusher> RemoteImageBufferProxy::createFlusher()
{
    if (UNLIKELY(!m_remoteRenderingBackendProxy))
        return nullptr;
    return makeUnique<RemoteImageBufferProxyFlusher>(m_flushState, lastSentFlushIdentifier());
}

void RemoteImageBufferProxy::prepareForBackingStoreChange()
{
    ASSERT(!hasPendingFlush());
    // If the backing store is mapped in the process and the changes happen in the other
    // process, we need to prepare for the backing store change before we let the change happen.
    if (!canMapBackingStore())
        return;
    if (auto* backend = ensureBackendCreated())
        backend->ensureNativeImagesHaveCopiedBackingStore();
}

void RemoteImageBufferProxyFlushState::waitForDidFlushOnSecondaryThread(DisplayListRecorderFlushIdentifier targetFlushIdentifier)
{
    Locker locker { m_lock };
    if (m_identifier >= targetFlushIdentifier)
        return;
    m_condition.wait(m_lock, [&] {
        assertIsHeld(m_lock);
        return m_identifier >= targetFlushIdentifier;
    });
}

void RemoteImageBufferProxyFlushState::markCompletedFlush(DisplayListRecorderFlushIdentifier flushIdentifier)
{
    Locker locker { m_lock };
    if (m_identifier >= flushIdentifier && flushIdentifier)
        return;
    m_identifier = flushIdentifier;
    m_condition.notifyAll();
}

DisplayListRecorderFlushIdentifier RemoteImageBufferProxyFlushState::identifierForCompletedFlush() const
{
    Locker locker { m_lock };
    return m_identifier;
}

void RemoteImageBufferProxyFlushState::cancel()
{
    markCompletedFlush({ });
}

std::unique_ptr<SerializedImageBuffer> RemoteImageBufferProxy::sinkIntoSerializedImageBuffer()
{
    ASSERT(hasOneRef());

    flushDrawingContext();
    m_remoteDisplayList.disconnect();

    if (!m_remoteRenderingBackendProxy)
        return nullptr;

    prepareForBackingStoreChange();

    if (!ensureBackendCreated())
        return nullptr;

    auto result = makeUnique<RemoteSerializedImageBufferProxy>(backend()->parameters(), backendInfo(), m_renderingResourceIdentifier, *m_remoteRenderingBackendProxy);

    clearBackend();
    m_remoteRenderingBackendProxy = nullptr;

    std::unique_ptr<SerializedImageBuffer> ret = WTFMove(result);
    return ret;
}

RemoteSerializedImageBufferProxy::RemoteSerializedImageBufferProxy(const WebCore::ImageBufferBackend::Parameters& parameters, const WebCore::ImageBufferBackend::Info& info, const WebCore::RenderingResourceIdentifier& renderingResourceIdentifier, RemoteRenderingBackendProxy& backend)
    : m_referenceTracker(RemoteSerializedImageBufferIdentifier::generateThreadSafe())
    , m_parameters(parameters)
    , m_info(info)
    , m_renderingResourceIdentifier(renderingResourceIdentifier)
    , m_connection(backend.connection())
{
    backend.remoteResourceCacheProxy().forgetImageBuffer(m_renderingResourceIdentifier);
    backend.moveToSerializedBuffer(m_renderingResourceIdentifier, m_referenceTracker.write());

    // Record an implicit read, since we always do a read+write (to get+remove) as a single
    // operation.
    m_referenceTracker.read();
}

RefPtr<ImageBuffer> RemoteSerializedImageBufferProxy::sinkIntoImageBuffer(std::unique_ptr<RemoteSerializedImageBufferProxy> buffer, RemoteRenderingBackendProxy& backend)
{
    auto result = adoptRef(new RemoteImageBufferProxy(buffer->m_parameters, buffer->m_info, backend, nullptr, buffer->m_renderingResourceIdentifier));
    backend.moveToImageBuffer(buffer->m_referenceTracker.write(), result->renderingResourceIdentifier());
    buffer->m_connection = nullptr;
    return result;
}

RemoteSerializedImageBufferProxy::~RemoteSerializedImageBufferProxy()
{
    if (m_connection)
        m_connection->send(Messages::GPUConnectionToWebProcess::ReleaseSerializedImageBuffer(m_referenceTracker.write()), 0);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
