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
#include "IPCEvent.h"
#include "ImageBufferShareableBitmapBackend.h"
#include "Logging.h"
#include "RemoteImageBufferMessages.h"
#include "RemoteImageBufferProxyMessages.h"
#include "RemoteRenderingBackendProxy.h"
#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebWorkerClient.h"
#include <WebCore/Document.h>
#include <WebCore/WorkerGlobalScope.h>
#include <wtf/SystemTracing.h>

#if HAVE(IOSURFACE)
#include "ImageBufferRemoteIOSurfaceBackend.h"
#include "ImageBufferShareableMappedIOSurfaceBackend.h"
#endif

namespace WebKit {
using namespace WebCore;


class RemoteImageBufferProxyFlushFence : public ThreadSafeRefCounted<RemoteImageBufferProxyFlushFence> {
    WTF_MAKE_NONCOPYABLE(RemoteImageBufferProxyFlushFence);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteImageBufferProxyFlushFence> create(IPC::Event event)
    {
        return adoptRef(*new RemoteImageBufferProxyFlushFence { WTFMove(event) });
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
        m_signaled = m_event.waitFor(timeout);
        if (m_signaled)
            tracePoint(FlushRemoteImageBufferEnd, reinterpret_cast<uintptr_t>(this), 0u);
        return m_signaled;
    }

    std::optional<IPC::Event> tryTakeEvent()
    {
        if (!m_signaled)
            return std::nullopt;
        Locker locker { m_lock };
        return WTFMove(m_event);
    }

private:
    RemoteImageBufferProxyFlushFence(IPC::Event event)
        : m_event(WTFMove(event))
    {
        tracePoint(FlushRemoteImageBufferStart, reinterpret_cast<uintptr_t>(this));
    }
    Lock m_lock;
    std::atomic<bool> m_signaled { false };
    IPC::Event WTF_GUARDED_BY_LOCK(m_lock) m_event;
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

RemoteImageBufferProxy::RemoteImageBufferProxy(Parameters parameters, const ImageBufferBackend::Info& info, RemoteRenderingBackendProxy& remoteRenderingBackendProxy, std::unique_ptr<ImageBufferBackend>&& backend, RenderingResourceIdentifier identifier)
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
    m_remoteRenderingBackendProxy->remoteResourceCacheProxy().forgetImageBuffer(renderingResourceIdentifier());
    m_remoteRenderingBackendProxy->releaseImageBuffer(renderingResourceIdentifier());
}

void RemoteImageBufferProxy::assertDispatcherIsCurrent() const
{
    if (m_remoteRenderingBackendProxy)
        assertIsCurrent(m_remoteRenderingBackendProxy->dispatcher());
}

template<typename T>
ALWAYS_INLINE void RemoteImageBufferProxy::send(T&& message)
{
    if (UNLIKELY(!m_remoteRenderingBackendProxy))
        return;

    auto result = m_remoteRenderingBackendProxy->streamConnection().send(std::forward<T>(message), renderingResourceIdentifier());
#if !RELEASE_LOG_DISABLED
    if (UNLIKELY(result != IPC::Error::NoError)) {
        auto& parameters = m_remoteRenderingBackendProxy->parameters();
        RELEASE_LOG(RemoteLayerBuffers, "[pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", renderingBackend=%" PRIu64 "] RemoteImageBufferProxy::send - failed, name:%" PUBLIC_LOG_STRING ", error:%" PUBLIC_LOG_STRING,
            parameters.pageProxyID.toUInt64(), parameters.pageID.toUInt64(), parameters.identifier.toUInt64(), IPC::description(T::name()), IPC::errorAsString(result));
    }
#else
    UNUSED_VARIABLE(result);
#endif
}

template<typename T>
ALWAYS_INLINE auto RemoteImageBufferProxy::sendSync(T&& message)
{
    if (UNLIKELY(!m_remoteRenderingBackendProxy))
        return IPC::StreamClientConnection::SendSyncResult<T> { IPC::Error::InvalidConnection };

    auto result = m_remoteRenderingBackendProxy->streamConnection().sendSync(std::forward<T>(message), renderingResourceIdentifier());
#if !RELEASE_LOG_DISABLED
    if (UNLIKELY(!result.succeeded())) {
        auto& parameters = m_remoteRenderingBackendProxy->parameters();
        RELEASE_LOG(RemoteLayerBuffers, "[pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", renderingBackend=%" PRIu64 "] RemoteDisplayListRecorderProxy::sendSync - failed, name:%" PUBLIC_LOG_STRING ", error:%" PUBLIC_LOG_STRING,
            parameters.pageProxyID.toUInt64(), parameters.pageID.toUInt64(), parameters.identifier.toUInt64(), IPC::description(T::name()), IPC::errorAsString(result.error));
    }
#endif
    return result;
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

void RemoteImageBufferProxy::didCreateBackend(std::optional<ImageBufferBackendHandle> handle)
{
    ASSERT(!m_backend);
    if (!handle) {
        m_remoteDisplayList.disconnect();
        m_remoteRenderingBackendProxy->remoteResourceCacheProxy().forgetImageBuffer(renderingResourceIdentifier());
        m_remoteRenderingBackendProxy->releaseImageBuffer(renderingResourceIdentifier());
        m_remoteRenderingBackendProxy = nullptr;
        return;
    }
    // This should match RemoteImageBufferProxy::create<>() call site and RemoteImageBuffer::create<>() call site.
    // FIXME: this will be removed and backend be constructed in the contructor.
    std::unique_ptr<ImageBufferBackend> backend;
    auto backendParameters = this->backendParameters(parameters());
    if (renderingMode() == RenderingMode::Accelerated) {
#if HAVE(IOSURFACE)
        if (canMapBackingStore())
            backend = ImageBufferShareableMappedIOSurfaceBackend::create(backendParameters, WTFMove(*handle));
        else
            backend = ImageBufferRemoteIOSurfaceBackend::create(backendParameters, WTFMove(*handle));
#endif
    } else
        backend = ImageBufferShareableBitmapBackend::create(backendParameters, WTFMove(*handle));

    setBackend(WTFMove(backend));
}

ImageBufferBackend* RemoteImageBufferProxy::ensureBackendCreated() const
{
    if (!m_backend && m_remoteRenderingBackendProxy) {
        auto error = streamConnection().waitForAndDispatchImmediately<Messages::RemoteImageBufferProxy::DidCreateBackend>(m_renderingResourceIdentifier);
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

RefPtr<NativeImage> RemoteImageBufferProxy::copyNativeImage() const
{
    if (canMapBackingStore()) {
        const_cast<RemoteImageBufferProxy*>(this)->flushDrawingContext();
        return ImageBuffer::copyNativeImage();
    }
    if (UNLIKELY(!m_remoteRenderingBackendProxy))
        return { };

    auto bitmap = m_remoteRenderingBackendProxy->getShareableBitmap(m_renderingResourceIdentifier, PreserveResolution::Yes);
    if (!bitmap)
        return { };
    return NativeImage::create(bitmap->createPlatformImage(DontCopyBackingStore));
}

RefPtr<NativeImage> RemoteImageBufferProxy::createNativeImageReference() const
{
    if (canMapBackingStore()) {
        const_cast<RemoteImageBufferProxy*>(this)->flushDrawingContext();
        return ImageBuffer::createNativeImageReference();
    }
    return copyNativeImage();
}

RefPtr<NativeImage> RemoteImageBufferProxy::sinkIntoNativeImage()
{
    return copyNativeImage();
}

RefPtr<ImageBuffer> RemoteImageBufferProxy::sinkIntoBufferForDifferentThread()
{
    ASSERT(hasOneRef());
    // We can't use these on a different thread, so make a local clone instead.
    auto copyBuffer = ImageBuffer::create(logicalSize(), renderingPurpose(), resolutionScale(), colorSpace(), pixelFormat());
    if (!copyBuffer)
        return nullptr;

    copyBuffer->context().drawImageBuffer(*this, FloatPoint { }, CompositeOperator::Copy);
    return copyBuffer;
}

RefPtr<NativeImage> RemoteImageBufferProxy::filteredNativeImage(Filter& filter)
{
    if (UNLIKELY(!m_remoteRenderingBackendProxy))
        return nullptr;
    auto sendResult = sendSync(Messages::RemoteImageBuffer::FilteredNativeImage(filter));
    if (!sendResult.succeeded())
        return nullptr;
    auto [handle] = sendResult.takeReply();
    if (!handle)
        return nullptr;
    handle->takeOwnershipOfMemory(MemoryLedger::Graphics);
    auto bitmap = ShareableBitmap::create(WTFMove(*handle));
    if (!bitmap)
        return nullptr;
    return NativeImage::create(bitmap->createPlatformImage(DontCopyBackingStore, ShouldInterpolate::No));
}

RefPtr<PixelBuffer> RemoteImageBufferProxy::getPixelBuffer(const PixelBufferFormat& destinationFormat, const IntRect& sourceRect, const ImageBufferAllocator& allocator) const
{
    if (canMapBackingStore()) {
        const_cast<RemoteImageBufferProxy&>(*this).flushDrawingContext();
        return ImageBuffer::getPixelBuffer(destinationFormat, sourceRect, allocator);
    }
    auto pixelBuffer = allocator.createPixelBuffer(destinationFormat, sourceRect.size());
    if (UNLIKELY(!pixelBuffer))
        return nullptr;
    if (LIKELY(m_remoteRenderingBackendProxy)) {
        if (m_remoteRenderingBackendProxy->getPixelBufferForImageBuffer(m_renderingResourceIdentifier, destinationFormat, sourceRect, { pixelBuffer->bytes(), pixelBuffer->sizeInBytes() }))
            return pixelBuffer;
    }
    pixelBuffer->zeroFill();
    return pixelBuffer;
}

void RemoteImageBufferProxy::clearBackend()
{
    m_needsFlush = false;
    m_pendingFlush = nullptr;
    if (m_backend)
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
    send(Messages::RemoteImageBuffer::ConvertToLuminanceMask());
}

void RemoteImageBufferProxy::transformToColorSpace(const DestinationColorSpace& colorSpace)
{
    send(Messages::RemoteImageBuffer::TransformToColorSpace(colorSpace));
}

void RemoteImageBufferProxy::flushDrawingContext()
{
    if (UNLIKELY(!m_remoteRenderingBackendProxy))
        return;
    if (m_needsFlush) {
        TraceScope tracingScope(FlushRemoteImageBufferStart, FlushRemoteImageBufferEnd);
        sendSync(Messages::RemoteImageBuffer::FlushContextSync());
        m_pendingFlush = nullptr;
        m_needsFlush = false;
        return;
    }
    if (m_pendingFlush) {
        bool success = m_pendingFlush->waitFor(m_remoteRenderingBackendProxy->streamConnection().defaultTimeout());
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
    std::optional<IPC::Event> event;
    // FIXME: This only recycles the event if the previous flush has been
    // waited on successfully. It should be possible to have the same semaphore
    // being used in multiple still-pending flushes, though if one times out,
    // then the others will be waiting on the wrong signal.
    if (m_pendingFlush)
        event = m_pendingFlush->tryTakeEvent();
    if (!event) {
        auto pair = IPC::createEventSignalPair();
        if (!pair) {
            flushDrawingContext();
            return false;
        }

        event = WTFMove(pair->event);
        send(Messages::RemoteImageBuffer::SetFlushSignal(WTFMove(pair->signal)));
    }

    send(Messages::RemoteImageBuffer::FlushContext());
    m_pendingFlush = RemoteImageBufferProxyFlushFence::create(WTFMove(*event));
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

    m_remoteRenderingBackendProxy->remoteResourceCacheProxy().forgetImageBuffer(m_renderingResourceIdentifier);

    auto result = makeUnique<RemoteSerializedImageBufferProxy>(parameters(), backendInfo(), m_renderingResourceIdentifier, *m_remoteRenderingBackendProxy);

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

RemoteSerializedImageBufferProxy::RemoteSerializedImageBufferProxy(WebCore::ImageBuffer::Parameters parameters, const WebCore::ImageBufferBackend::Info& info, const WebCore::RenderingResourceIdentifier& renderingResourceIdentifier, RemoteRenderingBackendProxy& backend)
    : m_parameters(parameters)
    , m_info(info)
    , m_renderingResourceIdentifier(renderingResourceIdentifier)
    , m_connection(backend.connection())
{
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
