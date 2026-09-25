/*
 * Copyright (C) 2020-2024 Apple Inc. All rights reserved.
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
#include "ImageBufferRemoteDisplayListBackend.h"
#include "ImageBufferRemotePDFDocumentBackend.h"
#include "ImageBufferShareableBitmapBackend.h"
#include "Logging.h"
#include "RemoteImageBufferMessages.h"
#include "RemoteImageBufferProxyMessages.h"
#include "RemoteNativeImageProxy.h"
#include "RemoteRenderingBackendProxy.h"
#include "RemoteSharedResourceCacheMessages.h"
#include "WebPage.h"
#include "WebWorkerClient.h"
#include <WebCore/Document.h>
#include <WebCore/WorkerGlobalScope.h>
#include <wtf/SystemTracing.h>
#include <wtf/TZoneMallocInlines.h>

#if HAVE(IOSURFACE)
#include "ImageBufferRemoteIOSurfaceBackend.h"
#include "ImageBufferShareableMappedIOSurfaceBackend.h"
#endif

namespace WebKit {
using namespace WebCore;

// putPixelBuffer calls are marked as batched if they are smaller than this. Speeds up multiple small pixel buffer sends
// while minimizing the risk of large memory areas being kept unused in IPC buffers.
// See also CanvasRenderingContext2DBase putImageDataCacheAreaLimit.
constexpr uint64_t putPixelBufferBatchedAreaLimit = 60 * 60;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteImageBufferProxy);
WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteSerializedImageBufferProxy);

class RemoteImageBufferProxyFlushFence : public ThreadSafeRefCounted<RemoteImageBufferProxyFlushFence> {
    WTF_MAKE_NONCOPYABLE(RemoteImageBufferProxyFlushFence);
    WTF_MAKE_TZONE_ALLOCATED(RemoteImageBufferProxyFlushFence);
public:
    static Ref<RemoteImageBufferProxyFlushFence> create(IPC::Event event)
    {
        return adoptRef(*new RemoteImageBufferProxyFlushFence { WTF::move(event) });
    }

    bool waitFor(Seconds timeout)
    {
        Locker locker { m_lock };
        if (m_signaled)
            return true;
        m_signaled = m_event.waitFor(timeout);
        return m_signaled;
    }

    std::optional<IPC::Event> tryTakeEvent()
    {
        if (!m_signaled)
            return std::nullopt;
        Locker locker { m_lock };
        return WTF::move(m_event);
    }

private:
    RemoteImageBufferProxyFlushFence(IPC::Event event)
        : m_event(WTF::move(event))
    {
    }
    Lock m_lock;
    std::atomic<bool> m_signaled { false };
    IPC::Event WTF_GUARDED_BY_LOCK(m_lock) m_event;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteImageBufferProxyFlushFence);

namespace {

class RemoteImageBufferProxyFlusher final : public ThreadSafeImageBufferFlusher {
    WTF_MAKE_TZONE_ALLOCATED(RemoteImageBufferProxyFlusher);
public:
    RemoteImageBufferProxyFlusher(Ref<RemoteImageBufferProxyFlushFence> flushState)
        : m_flushState(WTF::move(flushState))
    {
    }

    void flush() final
    {
        Ref { m_flushState }->waitFor(RemoteRenderingBackendProxy::defaultTimeout);
    }

private:
    Ref<RemoteImageBufferProxyFlushFence> m_flushState;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteImageBufferProxyFlusher);

}

Ref<RemoteImageBufferProxy> RemoteImageBufferProxy::createForSerializedBuffer(const ImageBuffer::Parameters& parameters, std::unique_ptr<ImageBufferBackend>&& backend, RemoteRenderingBackendProxy& renderingBackend)
{
    return adoptRef(*new RemoteImageBufferProxy(parameters, WTF::move(backend), renderingBackend));
}

RemoteImageBufferProxy::RemoteImageBufferProxy(Parameters parameters, std::unique_ptr<ImageBufferBackend>&& backend, RemoteRenderingBackendProxy& renderingBackend)
    : ImageBuffer(parameters, { }, WTF::move(backend))
    , m_context({ { }, ImageBuffer::logicalSize() }, ImageBuffer::baseTransform(), ImageBuffer::colorSpace(), ImageBuffer::renderingMode(), renderingBackend)
    , m_renderingBackend(renderingBackend)
{
    m_context.setClient(*this);
}

RemoteImageBufferProxy::~RemoteImageBufferProxy()
{
    RefPtr renderingBackend = m_renderingBackend.get();
    if (!renderingBackend)
        return;
    if (!renderingBackend->isGPUProcessConnectionClosed())
        flushDrawingContextAsync();
    renderingBackend->releaseImageBuffer(*this);
}

void RemoteImageBufferProxy::assertDispatcherIsCurrent() const
{
    ASSERT(!m_renderingBackend || m_renderingBackend->isCurrent());
}

template<typename T>
ALWAYS_INLINE void RemoteImageBufferProxy::send(T&& message) const
{
    RefPtr connection = this->connection();
    if (!connection) [[unlikely]]
        return;

    auto result = connection->send(std::forward<T>(message), renderingResourceIdentifier());
    if (result != IPC::Error::NoError) [[unlikely]] {
        RELEASE_LOG(RemoteLayerBuffers, "RemoteImageBufferProxy::send - failed, name:%" PUBLIC_LOG_STRING ", error:%" PUBLIC_LOG_STRING, IPC::description(T::name()).characters(), IPC::errorAsString(result).characters());
        didBecomeUnresponsive();
    }
}

template<typename T>
ALWAYS_INLINE auto RemoteImageBufferProxy::sendSync(T&& message) const
{
    RefPtr connection = this->connection();
    if (!connection) [[unlikely]]
        return IPC::StreamClientConnection::SendSyncResult<T> { IPC::Error::InvalidConnection };

    auto result = connection->sendSync(std::forward<T>(message), renderingResourceIdentifier());
    if (!result.succeeded()) [[unlikely]] {
        RELEASE_LOG(RemoteLayerBuffers, "RemoteGraphicsContextProxy::sendSync - failed, name:%" PUBLIC_LOG_STRING ", error:%" PUBLIC_LOG_STRING, IPC::description(T::name()).characters(), IPC::errorAsString(result.error()).characters());
        didBecomeUnresponsive();
    }
    return result;
}

ALWAYS_INLINE RefPtr<IPC::StreamClientConnection> RemoteImageBufferProxy::connection() const
{
    RefPtr backend = m_renderingBackend.get();
    if (!backend) [[unlikely]]
        return nullptr;
    return backend->connection();
}

void RemoteImageBufferProxy::didBecomeUnresponsive() const
{
    RefPtr backend = m_renderingBackend.get();
    if (!backend) [[unlikely]]
        return;
    backend->didBecomeUnresponsive();
}

void RemoteImageBufferProxy::backingStoreWillChange()
{
    prepareForBackingStoreChange();
}

void RemoteImageBufferProxy::didFailToCreateBackend()
{
    m_context.abandon();
    if (RefPtr renderingBackend = m_renderingBackend.get()) {
        m_renderingBackend = nullptr;
        renderingBackend->releaseImageBuffer(*this);
    }
}

// The backing store of a backend this process cannot map lives in the GPU process,
// which hands out its handle on request. Everything else was built in the constructor.
void RemoteImageBufferProxy::ensureBackendHandle() const
{
    auto* backend = ImageBuffer::backend();
    if (!backend || backend->canMapBackingStore())
        return;
    auto* sharing = dynamicDowncast<ImageBufferBackendHandleSharing>(backend->toBackendSharing());
    if (!sharing || sharing->hasBackendHandle())
        return;
    // The handle describes the backing store the GPU process is drawing into, so let
    // it see the buffered draws before it answers.
    sendPendingDrawsIfNecessary();
    auto sendResult = sendSync(Messages::RemoteImageBuffer::GetBackendHandle());
    if (!sendResult.succeeded())
        return;
    auto [handle] = sendResult.takeReply();
    if (!handle)
        return;
    sharing->setBackendHandle(WTF::move(*handle));
}

std::optional<ImageBufferBackendHandle> RemoteImageBufferProxy::takeBackendHandleForSerialization()
{
    auto* backend = ImageBuffer::backend();
    if (!backend || !backend->canMapBackingStore())
        return std::nullopt;
    auto* sharing = dynamicDowncast<ImageBufferBackendHandleSharing>(backend->toBackendSharing());
    if (!sharing)
        return std::nullopt;
    return sharing->takeBackendHandle();
}

ImageBufferBackendSharing* RemoteImageBufferProxy::toBackendSharing()
{
    ensureBackendHandle();
    return ImageBuffer::toBackendSharing();
}

std::optional<ImageBufferBackendHandle> RemoteImageBufferProxy::createBackingStoreHandleForGPUProcess() const
{
    // Only backing store this process allocated travels to the GPU process.
    auto* backend = ImageBuffer::backend();
    if (!backend || !backend->canMapBackingStore())
        return std::nullopt;
    auto* sharing = dynamicDowncast<ImageBufferBackendHandleSharing>(backend->toBackendSharing());
    if (!sharing)
        return std::nullopt;
    return sharing->createBackendHandle();
}

std::optional<RenderingMode> RemoteImageBufferProxy::getEffectiveRenderingModeForTesting() const
{
    auto sendResult = sendSync(Messages::RemoteImageBuffer::GetEffectiveRenderingModeForTesting());
    if (!sendResult.succeeded())
        return std::nullopt;
    auto [renderingMode] = sendResult.takeReply();
    return renderingMode;
}

RefPtr<NativeImage> RemoteImageBufferProxy::copyNativeImage() const
{
    auto* backend = ImageBuffer::backend();
    if (!backend)
        return nullptr;
    if (backend->canMapBackingStore()) {
        const_cast<RemoteImageBufferProxy*>(this)->flushDrawingContext();
        return ImageBuffer::copyNativeImage();
    }
    RefPtr renderingBackend = m_renderingBackend.get();
    if (!renderingBackend) [[unlikely]]
        return nullptr;
    bool hasAlpha = !pixelFormatIsOpaque(pixelFormat());
    Ref nativeImage = renderingBackend->remoteResourceCacheProxy().createNativeImage(backendSize(), colorSpace().platformColorSpace(), hasAlpha);
    // This read-back bypasses flushDrawingContext(), so send any buffered line
    // strokes into the stream before the (in-order) CopyNativeImage message.
    sendPendingDrawsIfNecessary();
    const_cast<RemoteImageBufferProxy*>(this)->send(Messages::RemoteImageBuffer::CopyNativeImage(nativeImage->renderingResourceIdentifier()));
    return nativeImage;
}

RefPtr<NativeImage> RemoteImageBufferProxy::createNativeImageReference() const
{
    auto* backend = ImageBuffer::backend();
    if (!backend)
        return { };
    if (backend->canMapBackingStore()) {
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
    auto copyBuffer = ImageBuffer::create(logicalSize(), RenderingMode::Unaccelerated, renderingPurpose(), resolutionScale(), colorSpace(), pixelFormat());
    if (!copyBuffer)
        return nullptr;

    copyBuffer->context().drawImageBuffer(*this, FloatPoint { }, { CompositeOperator::Copy });
    return copyBuffer;
}

RefPtr<NativeImage> RemoteImageBufferProxy::filteredNativeImage(Filter& filter)
{
    if (!m_renderingBackend) [[unlikely]]
        return nullptr;
    // FilteredNativeImage reads the buffer in the GPU process, so send any
    // buffered line strokes into the stream first.
    sendPendingDrawsIfNecessary();
    auto sendResult = sendSync(Messages::RemoteImageBuffer::FilteredNativeImage(filter));
    if (!sendResult.succeeded())
        return nullptr;
    auto [handle] = sendResult.takeReply();
    if (!handle)
        return nullptr;
    handle->takeOwnershipOfMemory(MemoryLedger::Graphics);
    auto bitmap = ShareableBitmap::create(WTF::move(*handle));
    if (!bitmap)
        return nullptr;
    return NativeImage::create(bitmap->createPlatformImage(DontCopyBackingStore, ShouldInterpolate::No));
}

RefPtr<PixelBuffer> RemoteImageBufferProxy::getPixelBuffer(const PixelBufferFormat& destinationFormat, const IntRect& sourceRect, const ImageBufferAllocator& allocator) const
{
    auto* backend = ImageBuffer::backend();
    if (!backend)
        return { };
    // getPixelBuffer observes the buffer's pixels (the non-mappable branch
    // bypasses flushDrawingContext()), so send any buffered line strokes first.
    sendPendingDrawsIfNecessary();
    if (backend->canMapBackingStore()) {
        const_cast<RemoteImageBufferProxy&>(*this).flushDrawingContext();
        return ImageBuffer::getPixelBuffer(destinationFormat, sourceRect, allocator);
    }
    auto pixelBuffer = allocator.createPixelBuffer(destinationFormat, sourceRect.size());
    if (!pixelBuffer) [[unlikely]]
        return nullptr;
    if (RefPtr renderingBackend = m_renderingBackend.get()) {
        if (renderingBackend->getPixelBufferForImageBuffer(m_renderingResourceIdentifier, destinationFormat, sourceRect, pixelBuffer->bytes()))
            return pixelBuffer;
    }
    pixelBuffer->zeroFill();
    return pixelBuffer;
}

void RemoteImageBufferProxy::disconnect()
{
    m_context.consumeHasDrawn();
    m_context.disconnect();
    prepareForBackingStoreChange();
    m_pendingFlush = nullptr;
    // The backend stays: it is where this buffer's properties come from, and a backing
    // store this process allocated outlives the GPU process. A handle to a GPU process
    // backing store does not, so drop it and ask again if it is needed.
    if (auto* backend = ImageBuffer::backend(); backend && !backend->canMapBackingStore()) {
        if (auto* sharing = dynamicDowncast<ImageBufferBackendHandleSharing>(backend->toBackendSharing()))
            sharing->clearBackendHandle();
    }
}

bool RemoteImageBufferProxy::isValid() const
{
    return m_renderingBackend.get();
}

GraphicsContext& RemoteImageBufferProxy::context() const
{
    return m_context;
}

void RemoteImageBufferProxy::putPixelBuffer(const PixelBufferSourceView& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    auto* backend = ImageBuffer::backend();
    if (!backend)
        return;
    if (backend->canMapBackingStore()) {
        // Simulate a write so that pending reads migrate the data off of the mapped buffer.
        context().fillRect({ });
        const_cast<RemoteImageBufferProxy&>(*this).flushDrawingContext();
        ImageBuffer::putPixelBuffer(pixelBuffer, srcRect, destPoint, destFormat);
        return;
    }

    RefPtr renderingBackend = m_renderingBackend.get();
    if (!renderingBackend) [[unlikely]]
        return;
    // The math inside PixelBuffer::create() doesn't agree with the math inside ImageBufferBackend::putPixelBuffer() about how m_resolutionScale interacts with the data in the ImageBuffer.
    // This means that putPixelBuffer() is only called when resolutionScale() == 1.
    ASSERT(resolutionScale() == 1);
    // This put is ordered after prior draws in the stream; send any buffered
    // line strokes first so they are not reordered behind it.
    sendPendingDrawsIfNecessary();
    backingStoreWillChange();
    send(Messages::RemoteImageBuffer::PutPixelBuffer(pixelBuffer, srcRect.location(), srcRect.size(), destPoint, destFormat));
    // Small putPixelBuffers are batched, large ones are not.
    if (pixelBuffer.size().unclampedArea() > putPixelBufferBatchedAreaLimit) {
        if (RefPtr connection = this->connection())
            connection->flushBatch();
    }
}

void RemoteImageBufferProxy::convertToLuminanceMask()
{
    sendPendingDrawsIfNecessary();
    send(Messages::RemoteImageBuffer::ConvertToLuminanceMask());
}

void RemoteImageBufferProxy::transformToColorSpace(const ColorSpace& colorSpace)
{
    sendPendingDrawsIfNecessary();
    send(Messages::RemoteImageBuffer::TransformToColorSpace(colorSpace));
}

void RemoteImageBufferProxy::flushDrawingContext()
{
    if (!m_renderingBackend) [[unlikely]]
        return;
    if (m_context.consumeHasDrawn()) {
        m_pendingFlush = nullptr;
        TraceScope tracingScope(FlushRemoteImageBufferStart, FlushRemoteImageBufferEnd);
        sendSync(Messages::RemoteImageBuffer::FlushContextSync());
        return;
    }
    if (RefPtr pendingFlush = std::exchange(m_pendingFlush, nullptr)) {
        bool success = pendingFlush->waitFor(RemoteRenderingBackendProxy::defaultTimeout);
        ASSERT_UNUSED(success, success); // Currently there is nothing to be done on a timeout.
    }
}

bool RemoteImageBufferProxy::flushDrawingContextAsync()
{
    if (!m_renderingBackend) [[unlikely]]
        return false;

    if (!m_context.consumeHasDrawn())
        return m_pendingFlush;

    std::optional<IPC::Event> event;
    // FIXME: This only recycles the event if the previous flush has been
    // waited on successfully. It should be possible to have the same semaphore
    // being used in multiple still-pending flushes, though if one times out,
    // then the others will be waiting on the wrong signal.
    if (RefPtr pendingFlush = m_pendingFlush)
        event = pendingFlush->tryTakeEvent();
    if (!event) {
        auto pair = IPC::createEventSignalPair();
        if (!pair) {
            flushDrawingContext();
            return false;
        }

        event = WTF::move(pair->event);
        send(Messages::RemoteImageBuffer::SetFlushSignal(WTF::move(pair->signal)));
    }

    send(Messages::RemoteImageBuffer::FlushContext());
    m_pendingFlush = RemoteImageBufferProxyFlushFence::create(WTF::move(*event));
    return true;
}

std::unique_ptr<ThreadSafeImageBufferFlusher> RemoteImageBufferProxy::createFlusher()
{
    if (!m_renderingBackend) [[unlikely]]
        return nullptr;
    if (!flushDrawingContextAsync())
        return nullptr;
    return makeUnique<RemoteImageBufferProxyFlusher>(Ref<RemoteImageBufferProxyFlushFence> { *m_pendingFlush });
}

void RemoteImageBufferProxy::prepareForBackingStoreChange()
{
    // If the backing store is mapped in the process and the changes happen in the other
    // process, we need to prepare for the backing store change before we let the change happen.
    if (auto* backend = ImageBuffer::backend())
        backend->ensureNativeImagesHaveCopiedBackingStore();
}

std::unique_ptr<SerializedImageBuffer> RemoteImageBufferProxy::sinkIntoSerializedImageBuffer()
{
    ASSERT(hasOneRef());

    flushDrawingContext();
    m_context.abandon();

    RefPtr renderingBackend = m_renderingBackend.get();
    if (!renderingBackend)
        return nullptr;

    prepareForBackingStoreChange();

    std::unique_ptr result = renderingBackend->moveToSerializedBuffer(*this);

    disconnect();
    m_renderingBackend = nullptr;

    std::unique_ptr<SerializedImageBuffer> ret = WTF::move(result);
    return ret;
}

RemoteSerializedImageBufferProxy::RemoteSerializedImageBufferProxy(WebCore::ImageBuffer::Parameters parameters, WebCore::RenderingMode renderingMode, size_t memoryCost, std::optional<ImageBufferBackendHandle>&& backendHandle, RemoteRenderingBackendProxy& backend)
    : m_parameters(parameters)
    , m_renderingMode(renderingMode)
    , m_memoryCost(memoryCost)
    , m_backendHandle(WTF::move(backendHandle))
    , m_connection(nullptr/*backend.connection()*/)
    , m_renderingBackend(backend)
{
}

static std::optional<ImageBufferBackendHandle> copyBackendHandle(const std::optional<ImageBufferBackendHandle>& handle)
{
    if (!handle)
        return std::nullopt;
    return WTF::switchOn(*handle,
        [](const ShareableBitmap::Handle& bitmapHandle) -> std::optional<ImageBufferBackendHandle> {
            return ImageBufferBackendHandle { ShareableBitmap::Handle { bitmapHandle } };
        }
#if PLATFORM(COCOA)
        , [](const MachSendRight& sendRight) -> std::optional<ImageBufferBackendHandle> {
            return ImageBufferBackendHandle { MachSendRight { sendRight } };
        }
#endif
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
        , [](const WebCore::DynamicContentScalingDisplayList&) -> std::optional<ImageBufferBackendHandle> {
            return std::nullopt;
        }
#endif
    );
}

std::unique_ptr<WebCore::SerializedImageBuffer> RemoteSerializedImageBufferProxy::clone() const
{
    return std::unique_ptr<WebCore::SerializedImageBuffer>(new RemoteSerializedImageBufferProxy(m_parameters, m_renderingMode, m_memoryCost, copyBackendHandle(m_backendHandle), m_connection));
}

std::optional<WebCore::ImageBufferTransferHandle> RemoteSerializedImageBufferProxy::sinkIntoTransferHandle()
{
    RefPtr renderingBackend = m_renderingBackend.get();
    if (!renderingBackend)
        return std::nullopt;

    auto transferIdentifier = renderingBackend->moveSerializedBufferToTransferHeap(*this);
    // The pixels stay in the GPU process, so any backing store this process allocated is of no use
    // to the recipient; it asks the GPU process for a handle if it ever needs one.
    m_backendHandle = std::nullopt;
    // Must not be released back into this process's rendering backend when the proxy goes away.
    m_connection = nullptr;
    m_renderingBackend = nullptr;
    if (!transferIdentifier)
        return std::nullopt;
    return WebCore::ImageBufferTransferHandle { *transferIdentifier, m_parameters, m_renderingMode, m_memoryCost };
}

RefPtr<ImageBuffer> RemoteSerializedImageBufferProxy::sinkIntoImageBuffer(std::unique_ptr<RemoteSerializedImageBufferProxy> buffer, RemoteRenderingBackendProxy& renderingBackend)
{
    RefPtr result = renderingBackend.moveToImageBuffer(*buffer);
    if (!result)
        return nullptr;
    buffer->m_connection = nullptr;
    return result;
}

RemoteSerializedImageBufferProxy::~RemoteSerializedImageBufferProxy()
{
    if (RefPtr connection = m_connection)
        connection->send(Messages::RemoteSharedResourceCache::ReleaseSerializedImageBuffer(identifier()), 0);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
