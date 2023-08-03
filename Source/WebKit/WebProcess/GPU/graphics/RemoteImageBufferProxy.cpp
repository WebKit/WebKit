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
#include "RemoteImageBufferProxyMessages.h"
#include "RemoteRenderingBackendProxy.h"
#include "WebPage.h"
#include "WebWorkerClient.h"
#include <WebCore/Document.h>
#include <WebCore/WorkerGlobalScope.h>
#include <wtf/SystemTracing.h>

namespace WebKit {
using namespace WebCore;


class RemoteImageBufferProxyFlushFence : public ThreadSafeRefCounted<RemoteImageBufferProxyFlushFence> {
    WTF_MAKE_NONCOPYABLE(RemoteImageBufferProxyFlushFence);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteImageBufferProxyFlushFence> create(IPC::Semaphore semaphore)
    {
        return adoptRef(*new RemoteImageBufferProxyFlushFence { WTFMove(semaphore) });
    }

    ~RemoteImageBufferProxyFlushFence()
    {
        if (!m_signaled)
            tracePoint(FlushRemoteImageBufferEnd, reinterpret_cast<uintptr_t>(this), 1u);
    }

    bool waitFor(Seconds timeout)
    {
        Locker locker { m_lock };
        if (m_signaled)
            return true;
        m_signaled = m_semaphore.waitFor(timeout);
        if (m_signaled)
            tracePoint(FlushRemoteImageBufferEnd, reinterpret_cast<uintptr_t>(this), 0u);
        return m_signaled;
    }

    std::optional<IPC::Semaphore> tryTakeSemaphore()
    {
        if (!m_signaled)
            return std::nullopt;
        Locker locker { m_lock };
        return WTFMove(m_semaphore);
    }

private:
    RemoteImageBufferProxyFlushFence(IPC::Semaphore semaphore)
        : m_semaphore(WTFMove(semaphore))
    {
        tracePoint(FlushRemoteImageBufferStart, reinterpret_cast<uintptr_t>(this));
    }
    Lock m_lock;
    std::atomic<bool> m_signaled { false };
    IPC::Semaphore WTF_GUARDED_BY_LOCK(m_lock) m_semaphore;
};

namespace {

class RemoteImageBufferProxyFlusher final : public ThreadSafeImageBufferFlusher {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RemoteImageBufferProxyFlusher(Ref<RemoteImageBufferProxyFlushFence> flushState)
        : m_flushState(WTFMove(flushState))
    {
    }

    void flush() final
    {
        m_flushState->waitFor(RemoteRenderingBackendProxy::defaultTimeout);
    }

private:
    Ref<RemoteImageBufferProxyFlushFence> m_flushState;
};

}

RemoteImageBufferProxy::RemoteImageBufferProxy(const ImageBufferBackend::Parameters& parameters, const ImageBufferBackend::Info& info, RemoteRenderingBackendProxy& remoteRenderingBackendProxy, std::unique_ptr<ImageBufferBackend>&& backend, RenderingResourceIdentifier identifier)
    : ImageBuffer(parameters, info, WTFMove(backend), identifier)
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
    if (m_pendingFlush)
        return;

    prepareForBackingStoreChange();
}

void RemoteImageBufferProxy::didCreateBackend(ImageBufferBackendHandle&& handle)
{
    ASSERT(!m_backend);
    if (renderingMode() == RenderingMode::Accelerated && std::holds_alternative<ShareableBitmap::Handle>(handle))
        m_backendInfo = ImageBuffer::populateBackendInfo<UnacceleratedImageBufferShareableBackend>(parameters());

    std::unique_ptr<ImageBufferBackend> backend;
    if (renderingMode() == RenderingMode::Unaccelerated)
        backend = UnacceleratedImageBufferShareableBackend::create(parameters(), WTFMove(handle));
    else if (canMapBackingStore())
        backend = AcceleratedImageBufferShareableMappedBackend::create(parameters(), WTFMove(handle));
    else
        backend = AcceleratedImageBufferRemoteBackend::create(parameters(), WTFMove(handle));

    setBackend(WTFMove(backend));
}

ImageBufferBackend* RemoteImageBufferProxy::ensureBackendCreated() const
{
    if (!m_backend && m_remoteRenderingBackendProxy) {
        auto error = streamConnection().waitForAndDispatchImmediately<Messages::RemoteImageBufferProxy::DidCreateBackend>(m_renderingResourceIdentifier, RemoteRenderingBackendProxy::defaultTimeout);
        if (error != IPC::Error::NoError) {
#if !RELEASE_LOG_DISABLED
            auto& parameters = m_remoteRenderingBackendProxy->parameters();
#endif
            RELEASE_LOG(RemoteLayerBuffers, "[pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", renderingBackend=%" PRIu64 "] RemoteImageBufferProxy::ensureBackendCreated - waitForAndDispatchImmediately returned error: %" PUBLIC_LOG_STRING,
                parameters.pageProxyID.toUInt64(), parameters.pageID.toUInt64(), parameters.identifier.toUInt64(), IPC::errorAsString(error));
            return nullptr;
        }
    }
    return m_backend.get();
}

RefPtr<NativeImage> RemoteImageBufferProxy::copyNativeImage(BackingStoreCopy copyBehavior) const
{
    if (canMapBackingStore()) {
        const_cast<RemoteImageBufferProxy*>(this)->flushDrawingContext();
        return ImageBuffer::copyNativeImage(copyBehavior);
    }
    if (UNLIKELY(!m_remoteRenderingBackendProxy))
        return { };

    auto bitmap = m_remoteRenderingBackendProxy->getShareableBitmap(m_renderingResourceIdentifier, PreserveResolution::Yes);
    if (!bitmap)
        return { };
    return NativeImage::create(bitmap->createPlatformImage(DontCopyBackingStore));
}

RefPtr<NativeImage> RemoteImageBufferProxy::copyNativeImageForDrawing(GraphicsContext& destination) const
{
    if (canMapBackingStore()) {
        const_cast<RemoteImageBufferProxy*>(this)->flushDrawingContext();
        return ImageBuffer::copyNativeImageForDrawing(destination);
    }
    return copyNativeImage(DontCopyBackingStore);
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
    return m_remoteRenderingBackendProxy->getFilteredImage(m_renderingResourceIdentifier, filter);
}

RefPtr<PixelBuffer> RemoteImageBufferProxy::getPixelBuffer(const PixelBufferFormat& destinationFormat, const IntRect& srcRect, const ImageBufferAllocator& allocator) const
{
    if (canMapBackingStore()) {
        const_cast<RemoteImageBufferProxy&>(*this).flushDrawingContext();
        return ImageBuffer::getPixelBuffer(destinationFormat, srcRect, allocator);
    }

    if (UNLIKELY(!m_remoteRenderingBackendProxy))
        return nullptr;
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
    m_pendingFlush = nullptr;
    prepareForBackingStoreChange();
    m_backend = nullptr;
}

GraphicsContext& RemoteImageBufferProxy::context() const
{
    return const_cast<RemoteImageBufferProxy*>(this)->m_remoteDisplayList;
}

void RemoteImageBufferProxy::putPixelBuffer(const PixelBuffer& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    if (canMapBackingStore()) {
        // Simulate a write so that pending reads migrate the data off of the mapped buffer.
        context().fillRect({ });
        const_cast<RemoteImageBufferProxy&>(*this).flushDrawingContext();
        ImageBuffer::putPixelBuffer(pixelBuffer, srcRect, destPoint, destFormat);
        return;
    }

    if (UNLIKELY(!m_remoteRenderingBackendProxy))
        return;
    // The math inside PixelBuffer::create() doesn't agree with the math inside ImageBufferBackend::putPixelBuffer() about how m_resolutionScale interacts with the data in the ImageBuffer.
    // This means that putPixelBuffer() is only called when resolutionScale() == 1.
    ASSERT(resolutionScale() == 1);
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

void RemoteImageBufferProxy::flushDrawingContext()
{
    if (UNLIKELY(!m_remoteRenderingBackendProxy))
        return;
    if (m_needsFlush) {
        TraceScope tracingScope(FlushRemoteImageBufferStart, FlushRemoteImageBufferEnd);
        m_remoteDisplayList.flushContextSync();
        m_pendingFlush = nullptr;
        m_needsFlush = false;
        return;
    }
    if (m_pendingFlush) {
        bool success = m_pendingFlush->waitFor(RemoteRenderingBackendProxy::defaultTimeout);
        ASSERT_UNUSED(success, success); // Currently there is nothing to be done on a timeout.
        m_pendingFlush = nullptr;
    }
}

bool RemoteImageBufferProxy::flushDrawingContextAsync()
{
    if (UNLIKELY(!m_remoteRenderingBackendProxy))
        return false;

    if (!m_needsFlush)
        return m_pendingFlush;

    LOG_WITH_STREAM(SharedDisplayLists, stream << "RemoteImageBufferProxy " << m_renderingResourceIdentifier << " flushDrawingContextAsync");
    std::optional<IPC::Semaphore> flushSemaphore;
    if (m_pendingFlush)
        flushSemaphore = m_pendingFlush->tryTakeSemaphore();
    if (!flushSemaphore)
        flushSemaphore.emplace();
    m_remoteDisplayList.flushContext(*flushSemaphore);
    m_pendingFlush = RemoteImageBufferProxyFlushFence::create(WTFMove(*flushSemaphore));
    m_needsFlush = false;
    return true;
}

std::unique_ptr<ThreadSafeImageBufferFlusher> RemoteImageBufferProxy::createFlusher()
{
    if (UNLIKELY(!m_remoteRenderingBackendProxy))
        return nullptr;
    if (!flushDrawingContextAsync())
        return nullptr;
    return makeUnique<RemoteImageBufferProxyFlusher>(Ref<RemoteImageBufferProxyFlushFence> { *m_pendingFlush });
}

void RemoteImageBufferProxy::prepareForBackingStoreChange()
{
    ASSERT(!m_pendingFlush);
    // If the backing store is mapped in the process and the changes happen in the other
    // process, we need to prepare for the backing store change before we let the change happen.
    if (!canMapBackingStore())
        return;
    if (auto* backend = ensureBackendCreated())
        backend->ensureNativeImagesHaveCopiedBackingStore();
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

IPC::StreamClientConnection& RemoteImageBufferProxy::streamConnection() const
{
    ASSERT(m_remoteRenderingBackendProxy);
    return m_remoteRenderingBackendProxy->streamConnection();
}


RemoteSerializedImageBufferProxy::RemoteSerializedImageBufferProxy(const WebCore::ImageBufferBackend::Parameters& parameters, const WebCore::ImageBufferBackend::Info& info, const WebCore::RenderingResourceIdentifier& renderingResourceIdentifier, RemoteRenderingBackendProxy& backend)
    : m_parameters(parameters)
    , m_info(info)
    , m_renderingResourceIdentifier(renderingResourceIdentifier)
    , m_connection(backend.connection())
{
    backend.remoteResourceCacheProxy().forgetImageBuffer(m_renderingResourceIdentifier);
    backend.moveToSerializedBuffer(m_renderingResourceIdentifier);
}

RefPtr<ImageBuffer> RemoteSerializedImageBufferProxy::sinkIntoImageBuffer(std::unique_ptr<RemoteSerializedImageBufferProxy> buffer, RemoteRenderingBackendProxy& backend)
{
    auto result = adoptRef(new RemoteImageBufferProxy(buffer->m_parameters, buffer->m_info, backend, nullptr, buffer->m_renderingResourceIdentifier));
    backend.moveToImageBuffer(result->renderingResourceIdentifier());
    buffer->m_connection = nullptr;
    return result;
}

RemoteSerializedImageBufferProxy::~RemoteSerializedImageBufferProxy()
{
    if (m_connection)
        m_connection->send(Messages::GPUConnectionToWebProcess::ReleaseSerializedImageBuffer(m_renderingResourceIdentifier), 0);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
