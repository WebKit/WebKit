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

RemoteImageBufferProxy::RemoteImageBufferProxy(Parameters parameters, const ImageBufferBackend::Info& info, RemoteRenderingBackendProxy& renderingBackend)
    : ImageBuffer(parameters, info, { }, nullptr)
    , m_context(ImageBuffer::colorSpace(), ImageBuffer::renderingMode() , { { }, ImageBuffer::logicalSize() }, ImageBuffer::baseTransform(), renderingBackend)
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
ALWAYS_INLINE void RemoteImageBufferProxy::send(T&& message)
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
ALWAYS_INLINE auto RemoteImageBufferProxy::sendSync(T&& message)
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

void RemoteImageBufferProxy::didCreateBackend(std::optional<ImageBufferBackendHandle> backendHandle)
{
    ASSERT(!m_backend);
    // This should match RemoteImageBufferProxy::create<>() call site and RemoteImageBuffer::create<>() call site.
    // FIXME: this will be removed and backend be constructed in the contructor.
    std::unique_ptr<ImageBufferBackend> backend;
    auto backendParameters = this->backendParameters(parameters());

    switch (renderingMode()) {
    case RenderingMode::Accelerated:
#if HAVE(IOSURFACE)
        if (backendHandle && std::holds_alternative<MachSendRight>(*backendHandle)) {
            if (RemoteRenderingBackendProxy::canMapRemoteImageBufferBackendBackingStore())
                backend = ImageBufferShareableMappedIOSurfaceBackend::create(backendParameters, WTFMove(*backendHandle));
            else
                backend = ImageBufferRemoteIOSurfaceBackend::create(backendParameters, WTFMove(*backendHandle));
        }
#endif
        [[fallthrough]];

    case RenderingMode::Unaccelerated:
        if (backendHandle && std::holds_alternative<ShareableBitmap::Handle>(*backendHandle)) {
            m_backendInfo = ImageBuffer::populateBackendInfo<ImageBufferShareableBitmapBackend>(backendParameters);
            auto handle = std::get<ShareableBitmap::Handle>(WTFMove(*backendHandle));
            handle.takeOwnershipOfMemory(MemoryLedger::Graphics);
            backend = ImageBufferShareableBitmapBackend::create(backendParameters, WTFMove(handle));
        }
        break;

    case RenderingMode::PDFDocument:
        backend = ImageBufferRemotePDFDocumentBackend::create(backendParameters);
        break;

    case RenderingMode::DisplayList:
        ASSERT(renderingPurpose() == RenderingPurpose::Snapshot);
        backend = ImageBufferRemoteDisplayListBackend::create(backendParameters);
        break;
    }

    if (!backend) {
        m_context.abandon();
        if (RefPtr renderingBackend = m_renderingBackend.get()) {
            m_renderingBackend = nullptr;
            renderingBackend->releaseImageBuffer(*this);
        }
        return;
    }
    setBackend(WTFMove(backend));
}

ImageBufferBackend* RemoteImageBufferProxy::ensureBackend() const
{
    if (m_backend)
        return m_backend.get();

    RefPtr connection = this->connection();
    if (!connection)
        return nullptr;

    auto error = connection->waitForAndDispatchImmediately<Messages::RemoteImageBufferProxy::DidCreateBackend>(m_renderingResourceIdentifier);
    if (error == IPC::Error::NoError)
        return m_backend.get();

    RefPtr renderingBackend = m_renderingBackend.get();
    if (!renderingBackend) [[unlikely]] {
        RELEASE_LOG(RemoteLayerBuffers, "[renderingBackend was deleted] RemoteImageBufferProxy::ensureBackendCreated - waitForAndDispatchImmediately returned error: %" PUBLIC_LOG_STRING,
            IPC::errorAsString(error).characters());
        return nullptr;
    }

    RELEASE_LOG(RemoteLayerBuffers, "[renderingBackend=%" PRIu64 "] RemoteImageBufferProxy::ensureBackendCreated - waitForAndDispatchImmediately returned error: %" PUBLIC_LOG_STRING,
        renderingBackend->renderingBackendIdentifier().toUInt64(), IPC::errorAsString(error).characters());
    didBecomeUnresponsive();
    return nullptr;
}

RefPtr<NativeImage> RemoteImageBufferProxy::copyNativeImage() const
{
    auto* backend = ensureBackend();
    if (!backend)
        return { };
    if (backend->canMapBackingStore()) {
        const_cast<RemoteImageBufferProxy*>(this)->flushDrawingContext();
        return ImageBuffer::copyNativeImage();
    }
    RefPtr renderingBackend = m_renderingBackend.get();
    if (!renderingBackend) [[unlikely]]
        return { };

    auto bitmap = renderingBackend->getShareableBitmap(m_renderingResourceIdentifier, PreserveResolution::Yes);
    if (!bitmap)
        return { };
    return NativeImage::create(bitmap->createPlatformImage(DontCopyBackingStore));
}

RefPtr<NativeImage> RemoteImageBufferProxy::createNativeImageReference() const
{
    auto* backend = ensureBackend();
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
    RefPtr renderingBackend = m_renderingBackend.get();
    if (!renderingBackend) [[unlikely]]
        return nullptr;
    // FIXME: The fallback is incorrect.
    auto filterData = renderingBackend->remoteResourceCacheProxy().recordFilterUse(filter, DestinationColorSpace::SRGB());
    if (!filterData) [[unlikely]]
        return nullptr;
    auto sendResult = sendSync(Messages::RemoteImageBuffer::FilteredNativeImage(*filterData));
    if (!sendResult.succeeded()) [[unlikely]]
        return nullptr;
    auto [handle] = sendResult.takeReply();
    if (!handle) [[unlikely]]
        return nullptr;
    handle->takeOwnershipOfMemory(MemoryLedger::Graphics);
    auto bitmap = ShareableBitmap::create(WTFMove(*handle));
    if (!bitmap) [[unlikely]]
        return nullptr;
    return NativeImage::create(bitmap->createPlatformImage(DontCopyBackingStore, ShouldInterpolate::No));
}

RefPtr<PixelBuffer> RemoteImageBufferProxy::getPixelBuffer(const PixelBufferFormat& destinationFormat, const IntRect& sourceRect, const ImageBufferAllocator& allocator) const
{
    auto* backend = ensureBackend();
    if (!backend)
        return { };
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
    if (m_backend)
        prepareForBackingStoreChange();
    m_backend = nullptr;
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
    auto* backend = ensureBackend();
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
    send(Messages::RemoteImageBuffer::ConvertToLuminanceMask());
}

void RemoteImageBufferProxy::transformToColorSpace(const DestinationColorSpace& colorSpace)
{
    send(Messages::RemoteImageBuffer::TransformToColorSpace(colorSpace));
}

void RemoteImageBufferProxy::flushDrawingContext()
{
    if (!m_renderingBackend) [[unlikely]]
        return;
    if (m_context.consumeHasDrawn()) {
        TraceScope tracingScope(FlushRemoteImageBufferStart, FlushRemoteImageBufferEnd);
        sendSync(Messages::RemoteImageBuffer::FlushContextSync());
        return;
    }
}

bool RemoteImageBufferProxy::flushDrawingContextAsync()
{
    if (!m_renderingBackend) [[unlikely]]
        return false;

    if (!m_context.consumeHasDrawn())
        return false;

    send(Messages::RemoteImageBuffer::FlushContext());
    return true;
}

void RemoteImageBufferProxy::prepareForBackingStoreChange()
{
    // If the backing store is mapped in the process and the changes happen in the other
    // process, we need to prepare for the backing store change before we let the change happen.
    if (auto* backend = ensureBackend())
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

    if (!ensureBackend())
        return nullptr;

    std::unique_ptr result = renderingBackend->moveToSerializedBuffer(*this);

    disconnect();
    m_renderingBackend = nullptr;

    std::unique_ptr<SerializedImageBuffer> ret = WTFMove(result);
    return ret;
}

RemoteSerializedImageBufferProxy::RemoteSerializedImageBufferProxy(WebCore::ImageBuffer::Parameters parameters, const WebCore::ImageBufferBackend::Info& info, RemoteRenderingBackendProxy& backend)
    : m_parameters(parameters)
    , m_info(info)
    , m_connection(nullptr/*backend.connection()*/)
{
}

RefPtr<ImageBuffer> RemoteSerializedImageBufferProxy::sinkIntoImageBuffer(std::unique_ptr<RemoteSerializedImageBufferProxy> buffer, RemoteRenderingBackendProxy& renderingBackend)
{
    Ref result = renderingBackend.moveToImageBuffer(*buffer);
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
