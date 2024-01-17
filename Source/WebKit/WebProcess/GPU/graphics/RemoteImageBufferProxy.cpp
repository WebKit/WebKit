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

RemoteImageBufferProxy::RemoteImageBufferProxy(Parameters parameters, const ImageBufferBackend::Info& info, RemoteRenderingBackendProxy& remoteRenderingBackendProxy, std::unique_ptr<ImageBufferBackend>&& backend, RenderingResourceIdentifier identifier)
    : ImageBuffer(parameters, info, { }, WTFMove(backend), identifier)
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

    auto result = m_remoteRenderingBackendProxy->streamConnection().send(std::forward<T>(message), renderingResourceIdentifier(), RemoteRenderingBackendProxy::defaultTimeout);
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

    auto result = m_remoteRenderingBackendProxy->streamConnection().sendSync(std::forward<T>(message), renderingResourceIdentifier(), RemoteRenderingBackendProxy::defaultTimeout);
#if !RELEASE_LOG_DISABLED
    if (UNLIKELY(!result.succeeded())) {
        auto& parameters = m_remoteRenderingBackendProxy->parameters();
        RELEASE_LOG(RemoteLayerBuffers, "[pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", renderingBackend=%" PRIu64 "] RemoteDisplayListRecorderProxy::sendSync - failed, name:%" PUBLIC_LOG_STRING ", error:%" PUBLIC_LOG_STRING,
            parameters.pageProxyID.toUInt64(), parameters.pageID.toUInt64(), parameters.identifier.toUInt64(), IPC::description(T::name()), IPC::errorAsString(result.error()));
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
    prepareForBackingStoreChange();
}

void RemoteImageBufferProxy::didCreateBackend(std::optional<ImageBufferBackendHandle> backendHandle)
{
    ASSERT(!m_backend);
    // This should match RemoteImageBufferProxy::create<>() call site and RemoteImageBuffer::create<>() call site.
    // FIXME: this will be removed and backend be constructed in the contructor.
    std::unique_ptr<ImageBufferBackend> backend;
    if (backendHandle) {
        auto backendParameters = this->backendParameters(parameters());
#if HAVE(IOSURFACE)
        if (std::holds_alternative<MachSendRight>(*backendHandle)) {
            if (canMapBackingStore())
                backend = ImageBufferShareableMappedIOSurfaceBackend::create(backendParameters, WTFMove(*backendHandle));
            else
                backend = ImageBufferRemoteIOSurfaceBackend::create(backendParameters, WTFMove(*backendHandle));
        }
#endif
        if (std::holds_alternative<ShareableBitmap::Handle>(*backendHandle)) {
            m_backendInfo = ImageBuffer::populateBackendInfo<ImageBufferShareableBitmapBackend>(backendParameters);
            auto handle = std::get<ShareableBitmap::Handle>(WTFMove(*backendHandle));
            handle.takeOwnershipOfMemory(MemoryLedger::Graphics);
            backend = ImageBufferShareableBitmapBackend::create(backendParameters, WTFMove(handle));
        }
    }
    if (!backend) {
        m_remoteDisplayList.disconnect();
        m_remoteRenderingBackendProxy->remoteResourceCacheProxy().forgetImageBuffer(renderingResourceIdentifier());
        m_remoteRenderingBackendProxy->releaseImageBuffer(renderingResourceIdentifier());
        m_remoteRenderingBackendProxy = nullptr;
        return;
    }

    setBackend(WTFMove(backend));
}

ImageBufferBackend* RemoteImageBufferProxy::ensureBackend() const
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

    copyBuffer->context().drawImageBuffer(*this, FloatPoint { }, { CompositeOperator::Copy });
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
        m_needsFlush = false;
        return;
    }
}

bool RemoteImageBufferProxy::flushDrawingContextAsync()
{
    if (UNLIKELY(!m_remoteRenderingBackendProxy))
        return false;

    if (!m_needsFlush)
        return false;

    send(Messages::RemoteImageBuffer::FlushContext());
    return true;
}

void RemoteImageBufferProxy::prepareForBackingStoreChange()
{
    // If the backing store is mapped in the process and the changes happen in the other
    // process, we need to prepare for the backing store change before we let the change happen.
    if (!canMapBackingStore())
        return;
    if (auto* backend = ensureBackend())
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

    if (!ensureBackend())
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
