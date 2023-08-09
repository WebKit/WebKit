/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RemoteRenderingBackendProxy.h"

#if ENABLE(GPU_PROCESS)

#include "BufferIdentifierSet.h"
#include "GPUConnectionToWebProcess.h"
#include "Logging.h"
#include "PlatformImageBufferShareableBackend.h"
#include "RemoteDisplayListRecorderProxy.h"
#include "RemoteImageBufferProxy.h"
#include "RemoteImageBufferProxyMessages.h"
#include "RemoteRenderingBackendMessages.h"
#include "RemoteRenderingBackendProxyMessages.h"
#include "SwapBuffersDisplayRequirement.h"
#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <JavaScriptCore/TypedArrayInlines.h>
#include <WebCore/FontCustomPlatformData.h>
#include <wtf/text/TextStream.h>

namespace WebKit {

using namespace WebCore;

std::unique_ptr<RemoteRenderingBackendProxy> RemoteRenderingBackendProxy::create(WebPage& webPage)
{
    return std::unique_ptr<RemoteRenderingBackendProxy>(new RemoteRenderingBackendProxy({ RenderingBackendIdentifier::generate(), webPage.webPageProxyIdentifier(), webPage.identifier() }, RunLoop::main()));
}

std::unique_ptr<RemoteRenderingBackendProxy> RemoteRenderingBackendProxy::create(const RemoteRenderingBackendCreationParameters& parameters, SerialFunctionDispatcher& dispatcher)
{
    return std::unique_ptr<RemoteRenderingBackendProxy>(new RemoteRenderingBackendProxy(parameters, dispatcher));
}

RemoteRenderingBackendProxy::RemoteRenderingBackendProxy(const RemoteRenderingBackendCreationParameters& parameters, SerialFunctionDispatcher& dispatcher)
    : m_parameters(parameters)
    , m_dispatcher(dispatcher)
{
}

RemoteRenderingBackendProxy::~RemoteRenderingBackendProxy()
{
    for (auto& markAsVolatileHandlers : m_markAsVolatileRequests.values())
        markAsVolatileHandlers(false);

    if (!m_streamConnection)
        return;

    ensureOnMainRunLoop([ident = renderingBackendIdentifier()]() {
        WebProcess::singleton().ensureGPUProcessConnection().connection().send(Messages::GPUConnectionToWebProcess::ReleaseRenderingBackend(ident), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
    });
    m_remoteResourceCacheProxy.clear();
    disconnectGPUProcess();
}

void RemoteRenderingBackendProxy::ensureGPUProcessConnection()
{
    if (!m_streamConnection) {
        static constexpr auto connectionBufferSizeLog2 = 21;
        auto [streamConnection, serverHandle] = IPC::StreamClientConnection::create(connectionBufferSizeLog2);
        if (!streamConnection)
            CRASH();
        m_streamConnection = WTFMove(streamConnection);
        // RemoteRenderingBackendProxy behaves as the dispatcher for the connection to obtain isolated state for its
        // connection. This prevents waits on RemoteRenderingBackendProxy to process messages from other connections.
        m_streamConnection->open(*this, *this);

        callOnMainRunLoopAndWait([this, serverHandle = WTFMove(serverHandle)]() mutable {
            m_connection = &WebProcess::singleton().ensureGPUProcessConnection().connection();
            m_connection->send(Messages::GPUConnectionToWebProcess::CreateRenderingBackend(m_parameters, WTFMove(serverHandle)), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
        });
    }
}
template<typename T>
auto RemoteRenderingBackendProxy::send(T&& message)
{
    auto result = streamConnection().send(WTFMove(message), renderingBackendIdentifier(), defaultTimeout);
    if (UNLIKELY(result != IPC::Error::NoError)) {
        RELEASE_LOG(RemoteLayerBuffers, "[pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", renderingBackend=%" PRIu64 "] RemoteRenderingBackendProxy::send - failed, name:%" PUBLIC_LOG_STRING ", error:%" PUBLIC_LOG_STRING,
            m_parameters.pageProxyID.toUInt64(), m_parameters.pageID.toUInt64(), m_parameters.identifier.toUInt64(), IPC::description(T::name()), IPC::errorAsString(result));
    }
    return result;
}

template<typename T>
auto RemoteRenderingBackendProxy::sendSync(T&& message)
{
    auto result = streamConnection().sendSync(WTFMove(message), renderingBackendIdentifier(), defaultTimeout);
    if (UNLIKELY(!result.succeeded())) {
        RELEASE_LOG(RemoteLayerBuffers, "[pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", renderingBackend=%" PRIu64 "] RemoteRenderingBackendProxy::sendSync - failed, name:%" PUBLIC_LOG_STRING ", error:%" PUBLIC_LOG_STRING,
            m_parameters.pageProxyID.toUInt64(), m_parameters.pageID.toUInt64(), m_parameters.identifier.toUInt64(), IPC::description(T::name()), IPC::errorAsString(result.error));
    }
    return result;
}

void RemoteRenderingBackendProxy::didClose(IPC::Connection&)
{
    if (!m_streamConnection)
        return;
    disconnectGPUProcess();
    // Note: The cache will call back to this to setup a new connection.
    m_remoteResourceCacheProxy.remoteResourceCacheWasDestroyed();
}

void RemoteRenderingBackendProxy::disconnectGPUProcess()
{
    if (m_destroyGetPixelBufferSharedMemoryTimer.isActive())
        m_destroyGetPixelBufferSharedMemoryTimer.stop();
    m_getPixelBufferSharedMemory = nullptr;
    m_renderingUpdateID = { };
    m_didRenderingUpdateID = { };
    m_streamConnection->invalidate();
    m_streamConnection = nullptr;
}

void RemoteRenderingBackendProxy::createRemoteImageBuffer(ImageBuffer& imageBuffer)
{
    send(Messages::RemoteRenderingBackend::CreateImageBuffer(imageBuffer.logicalSize(), imageBuffer.renderingMode(), imageBuffer.renderingPurpose(), imageBuffer.resolutionScale(), imageBuffer.colorSpace(), imageBuffer.pixelFormat(), imageBuffer.renderingResourceIdentifier()));
}

RefPtr<ImageBuffer> RemoteRenderingBackendProxy::createImageBuffer(const FloatSize& size, RenderingMode renderingMode, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, bool avoidBackendSizeCheck)
{
    RefPtr<ImageBuffer> imageBuffer;

    if (renderingMode == RenderingMode::Accelerated) {
        // Unless DOM rendering is always enabled when any GPU process rendering is enabled,
        // we need to create ImageBuffers for e.g. Canvas that are actually mapped into the
        // Web Content process, so they can be painted into the tiles.
        if (!WebProcess::singleton().shouldUseRemoteRenderingFor(RenderingPurpose::DOM))
            imageBuffer = RemoteImageBufferProxy::create<AcceleratedImageBufferShareableMappedBackend>(size, resolutionScale, colorSpace, pixelFormat, purpose, *this, avoidBackendSizeCheck);
        else
            imageBuffer = RemoteImageBufferProxy::create<AcceleratedImageBufferRemoteBackend>(size, resolutionScale, colorSpace, pixelFormat, purpose, *this, avoidBackendSizeCheck);
    }

    if (!imageBuffer)
        imageBuffer = RemoteImageBufferProxy::create<UnacceleratedImageBufferShareableBackend>(size, resolutionScale, colorSpace, pixelFormat, purpose, *this, avoidBackendSizeCheck);

    if (imageBuffer) {
        createRemoteImageBuffer(*imageBuffer);
        return imageBuffer;
    }

    return nullptr;
}

void RemoteRenderingBackendProxy::moveToSerializedBuffer(WebCore::RenderingResourceIdentifier identifier)
{
    send(Messages::RemoteRenderingBackend::MoveToSerializedBuffer(identifier));
}

void RemoteRenderingBackendProxy::moveToImageBuffer(WebCore::RenderingResourceIdentifier identifier)
{
    send(Messages::RemoteRenderingBackend::MoveToImageBuffer(identifier));
}

bool RemoteRenderingBackendProxy::getPixelBufferForImageBuffer(RenderingResourceIdentifier imageBuffer, const PixelBufferFormat& destinationFormat, const IntRect& srcRect, std::span<uint8_t> result)
{
    if (auto handle = updateSharedMemoryForGetPixelBuffer(result.size())) {
        auto sendResult = sendSync(Messages::RemoteRenderingBackend::GetPixelBufferForImageBufferWithNewMemory(imageBuffer, WTFMove(*handle), destinationFormat, srcRect));
        if (!sendResult.succeeded())
            return false;
    } else {
        if (!m_getPixelBufferSharedMemory)
            return false;
        auto sendResult = sendSync(Messages::RemoteRenderingBackend::GetPixelBufferForImageBuffer(imageBuffer, destinationFormat, srcRect));
        if (!sendResult.succeeded())
            return false;
    }
    memcpy(result.data(), m_getPixelBufferSharedMemory->data(), result.size());
    return true;
}

void RemoteRenderingBackendProxy::putPixelBufferForImageBuffer(RenderingResourceIdentifier imageBuffer, const PixelBuffer& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    send(Messages::RemoteRenderingBackend::PutPixelBufferForImageBuffer(imageBuffer, Ref { const_cast<PixelBuffer&>(pixelBuffer) }, srcRect, destPoint, destFormat));
}

std::optional<SharedMemory::Handle> RemoteRenderingBackendProxy::updateSharedMemoryForGetPixelBuffer(size_t dataSize)
{
    if (m_destroyGetPixelBufferSharedMemoryTimer.isActive())
        m_destroyGetPixelBufferSharedMemoryTimer.stop();

    if (m_getPixelBufferSharedMemory && dataSize <= m_getPixelBufferSharedMemory->size()) {
        m_destroyGetPixelBufferSharedMemoryTimer.startOneShot(5_s);
        return std::nullopt;
    }
    destroyGetPixelBufferSharedMemory();
    auto memory = SharedMemory::allocate(dataSize);
    if (!memory)
        return std::nullopt;
    auto handle = memory->createHandle(SharedMemory::Protection::ReadWrite);
    if (!handle)
        return std::nullopt;

    m_getPixelBufferSharedMemory = WTFMove(memory);
    handle->takeOwnershipOfMemory(MemoryLedger::Graphics);
    m_destroyGetPixelBufferSharedMemoryTimer.startOneShot(5_s);
    return handle;
}

void RemoteRenderingBackendProxy::destroyGetPixelBufferSharedMemory()
{
    if (!m_getPixelBufferSharedMemory)
        return;
    m_getPixelBufferSharedMemory = nullptr;
    send(Messages::RemoteRenderingBackend::DestroyGetPixelBufferSharedMemory());
}

RefPtr<ShareableBitmap> RemoteRenderingBackendProxy::getShareableBitmap(RenderingResourceIdentifier imageBuffer, PreserveResolution preserveResolution)
{
    auto sendResult = sendSync(Messages::RemoteRenderingBackend::GetShareableBitmapForImageBuffer(imageBuffer, preserveResolution));
    auto [handle] = sendResult.takeReplyOr(ShareableBitmap::Handle { });
    if (handle.isNull())
        return { };
    handle.takeOwnershipOfMemory(MemoryLedger::Graphics);
    return ShareableBitmap::create(WTFMove(handle));
}

RefPtr<Image> RemoteRenderingBackendProxy::getFilteredImage(RenderingResourceIdentifier imageBuffer, Filter& filter)
{
    auto sendResult = sendSync(Messages::RemoteRenderingBackend::GetFilteredImageForImageBuffer(imageBuffer, filter));
    auto [handle] = sendResult.takeReplyOr(ShareableBitmap::Handle { });
    if (handle.isNull())
        return { };

    handle.takeOwnershipOfMemory(MemoryLedger::Graphics);
    auto bitmap = ShareableBitmap::create(WTFMove(handle));
    if (!bitmap)
        return { };

    return bitmap->createImage();
}

void RemoteRenderingBackendProxy::cacheNativeImage(ShareableBitmap::Handle&& handle, RenderingResourceIdentifier renderingResourceIdentifier)
{
    send(Messages::RemoteRenderingBackend::CacheNativeImage(WTFMove(handle), renderingResourceIdentifier));
}

void RemoteRenderingBackendProxy::cacheFont(const WebCore::Font::Attributes& fontAttributes, const WebCore::FontPlatformData::Attributes& platformData, std::optional<WebCore::RenderingResourceIdentifier> ident)
{
    send(Messages::RemoteRenderingBackend::CacheFont(fontAttributes, platformData, ident));
}

void RemoteRenderingBackendProxy::cacheFontCustomPlatformData(Ref<const FontCustomPlatformData>&& customPlatformData)
{
    Ref<FontCustomPlatformData> data = adoptRef(const_cast<FontCustomPlatformData&>(customPlatformData.leakRef()));
    send(Messages::RemoteRenderingBackend::CacheFontCustomPlatformData(WTFMove(data)));
}

void RemoteRenderingBackendProxy::cacheDecomposedGlyphs(Ref<DecomposedGlyphs>&& decomposedGlyphs)
{
    send(Messages::RemoteRenderingBackend::CacheDecomposedGlyphs(WTFMove(decomposedGlyphs)));
}

void RemoteRenderingBackendProxy::cacheGradient(Ref<Gradient>&& gradient)
{
    auto renderingResourceIdentifier = gradient->renderingResourceIdentifier();
    send(Messages::RemoteRenderingBackend::CacheGradient(WTFMove(gradient), renderingResourceIdentifier));
}

void RemoteRenderingBackendProxy::cacheFilter(Ref<Filter>&& filter)
{
    auto renderingResourceIdentifier = filter->renderingResourceIdentifier();
    send(Messages::RemoteRenderingBackend::CacheFilter(WTFMove(filter), renderingResourceIdentifier));
}

void RemoteRenderingBackendProxy::releaseAllRemoteResources()
{
    if (!m_streamConnection)
        return;
    send(Messages::RemoteRenderingBackend::ReleaseAllResources());
}

void RemoteRenderingBackendProxy::releaseRenderingResource(RenderingResourceIdentifier renderingResourceIdentifier)
{
    if (!m_streamConnection)
        return;
    send(Messages::RemoteRenderingBackend::ReleaseRenderingResource(renderingResourceIdentifier));
}

void RemoteRenderingBackendProxy::releaseAllImageResources()
{
    if (!m_streamConnection)
        return;
    send(Messages::RemoteRenderingBackend::ReleaseAllImageResources());
}

#if PLATFORM(COCOA)
auto RemoteRenderingBackendProxy::prepareBuffersForDisplay(const Vector<LayerPrepareBuffersData>& prepareBuffersInput) -> Vector<SwapBuffersResult>
{
    if (prepareBuffersInput.isEmpty())
        return { };

    auto bufferIdentifier = [](ImageBuffer* buffer) -> std::optional<RenderingResourceIdentifier> {
        if (!buffer)
            return std::nullopt;
        return buffer->renderingResourceIdentifier();
    };

    auto clearBackendHandle = [](ImageBuffer* buffer) {
        if (!buffer)
            return;

        if (auto* backend = buffer->ensureBackendCreated()) {
            auto* sharing = backend->toBackendSharing();
            if (is<ImageBufferBackendHandleSharing>(sharing))
                downcast<ImageBufferBackendHandleSharing>(*sharing).clearBackendHandle();
        }
    };

    Vector<PrepareBackingStoreBuffersInputData> inputData;
    inputData.reserveInitialCapacity(prepareBuffersInput.size());

    for (const auto& perLayerData : prepareBuffersInput) {
        // Clear all the buffer's MachSendRights to avoid all the surfaces appearing to be in-use.
        // We get back the new front buffer's MachSendRight in the reply.
        clearBackendHandle(perLayerData.buffers.front.get());
        clearBackendHandle(perLayerData.buffers.back.get());
        clearBackendHandle(perLayerData.buffers.secondaryBack.get());

        inputData.uncheckedAppend({
            {
                bufferIdentifier(perLayerData.buffers.front.get()),
                bufferIdentifier(perLayerData.buffers.back.get()),
                bufferIdentifier(perLayerData.buffers.secondaryBack.get())
            },
            perLayerData.supportsPartialRepaint,
            perLayerData.hasEmptyDirtyRegion
        });
    }

    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteRenderingBackendProxy::prepareBuffersForDisplay - input buffers  " << inputData);

    Vector<PrepareBackingStoreBuffersOutputData> outputData;
    auto sendResult = sendSync(Messages::RemoteRenderingBackend::PrepareBuffersForDisplay(inputData));
    if (!sendResult.succeeded()) {
        // GPU Process crashed. Set the output data to all null buffers, requiring a full display.
        RELEASE_LOG(RemoteLayerBuffers, "[pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", renderingBackend=%" PRIu64 "] RemoteRenderingBackendProxy::prepareBuffersForDisplay - prepareBuffersForDisplay returned error: %" PUBLIC_LOG_STRING,
            m_parameters.pageProxyID.toUInt64(), m_parameters.pageID.toUInt64(), m_parameters.identifier.toUInt64(), IPC::errorAsString(sendResult.error));
        outputData.resize(inputData.size());
        for (auto& perLayerOutputData : outputData)
            perLayerOutputData.displayRequirement = SwapBuffersDisplayRequirement::NeedsFullDisplay;
    } else
        std::tie(outputData) = sendResult.takeReply();

    RELEASE_ASSERT_WITH_MESSAGE(inputData.size() == outputData.size(), "PrepareBuffersForDisplay: mismatched buffer vector sizes");

    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteRenderingBackendProxy::prepareBuffersForDisplay - output buffers " << outputData);

    auto fetchBufferWithIdentifier = [&](std::optional<RenderingResourceIdentifier> identifier, std::optional<ImageBufferBackendHandle>&& handle = std::nullopt, bool isFrontBuffer = false) -> RefPtr<ImageBuffer> {
        if (!identifier)
            return nullptr;

        auto buffer = m_remoteResourceCacheProxy.cachedImageBuffer(*identifier);
        if (!buffer)
            return nullptr;

        if (handle) {
            if (auto* backend = buffer->ensureBackendCreated()) {
                auto* sharing = backend->toBackendSharing();
                if (is<ImageBufferBackendHandleSharing>(sharing))
                    downcast<ImageBufferBackendHandleSharing>(*sharing).setBackendHandle(WTFMove(*handle));
            }
        }

        if (isFrontBuffer) {
            // We know the GPU Process always sets the new front buffer to be non-volatile.
            buffer->setVolatilityState(VolatilityState::NonVolatile);
        }

        return buffer;
    };

    Vector<SwapBuffersResult> result;
    result.reserveInitialCapacity(outputData.size());

    for (auto& perLayerOutputData : outputData) {
        result.uncheckedAppend({
            {
                fetchBufferWithIdentifier(perLayerOutputData.bufferSet.front, WTFMove(perLayerOutputData.frontBufferHandle), true),
                fetchBufferWithIdentifier(perLayerOutputData.bufferSet.back),
                fetchBufferWithIdentifier(perLayerOutputData.bufferSet.secondaryBack)
            },
            perLayerOutputData.displayRequirement
        });
    }

    return result;
}
#endif

void RemoteRenderingBackendProxy::markSurfacesVolatile(Vector<WebCore::RenderingResourceIdentifier>&& identifiers, CompletionHandler<void(bool)>&& completionHandler)
{
    auto requestIdentifier = MarkSurfacesAsVolatileRequestIdentifier::generate();
    m_markAsVolatileRequests.add(requestIdentifier, WTFMove(completionHandler));

    send(Messages::RemoteRenderingBackend::MarkSurfacesVolatile(requestIdentifier, identifiers));
}

void RemoteRenderingBackendProxy::didMarkLayersAsVolatile(MarkSurfacesAsVolatileRequestIdentifier requestIdentifier, const Vector<WebCore::RenderingResourceIdentifier>& markedVolatileBufferIdentifiers, bool didMarkAllLayersAsVolatile)
{
    ASSERT(requestIdentifier);
    auto completionHandler = m_markAsVolatileRequests.take(requestIdentifier);
    if (!completionHandler)
        return;

    for (auto identifier : markedVolatileBufferIdentifiers) {
        auto imageBuffer = m_remoteResourceCacheProxy.cachedImageBuffer(identifier);
        if (imageBuffer)
            imageBuffer->setVolatilityState(WebCore::VolatilityState::Volatile);
    }
    completionHandler(didMarkAllLayersAsVolatile);
}

void RemoteRenderingBackendProxy::finalizeRenderingUpdate()
{
    if (!m_streamConnection)
        return;
    send(Messages::RemoteRenderingBackend::FinalizeRenderingUpdate(m_renderingUpdateID));
    m_renderingUpdateID.increment();
}

void RemoteRenderingBackendProxy::didPaintLayers()
{
    if (!m_streamConnection)
        return;
    m_remoteResourceCacheProxy.didPaintLayers();
}

bool RemoteRenderingBackendProxy::dispatchMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (decoder.messageReceiverName() == Messages::RemoteImageBufferProxy::messageReceiverName()) {
        auto imageBuffer = m_remoteResourceCacheProxy.cachedImageBuffer(RenderingResourceIdentifier { decoder.destinationID() });
        if (imageBuffer)
            imageBuffer->didReceiveMessage(connection, decoder);
        // Messages to already removed instances are ok.
        return true;
    }
    return false;
}

bool RemoteRenderingBackendProxy::dispatchSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&)
{
    return false;
}

void RemoteRenderingBackendProxy::didFinalizeRenderingUpdate(RenderingUpdateID didRenderingUpdateID)
{
    ASSERT(didRenderingUpdateID <= m_renderingUpdateID);
    m_didRenderingUpdateID = std::min(didRenderingUpdateID, m_renderingUpdateID);
}

RenderingBackendIdentifier RemoteRenderingBackendProxy::renderingBackendIdentifier() const
{
    return m_parameters.identifier;
}

RenderingBackendIdentifier RemoteRenderingBackendProxy::ensureBackendCreated()
{
    ensureGPUProcessConnection();
    return renderingBackendIdentifier();
}

IPC::StreamClientConnection& RemoteRenderingBackendProxy::streamConnection()
{
    ensureGPUProcessConnection();
    if (UNLIKELY(!m_streamConnection->hasSemaphores()))
        m_streamConnection->waitForAndDispatchImmediately<Messages::RemoteRenderingBackendProxy::DidInitialize>(renderingBackendIdentifier(), defaultTimeout);
    return *m_streamConnection;
}

void RemoteRenderingBackendProxy::didInitialize(IPC::Semaphore&& wakeUp, IPC::Semaphore&& clientWait)
{
    if (!m_streamConnection) {
        ASSERT_NOT_REACHED();
        return;
    }
    m_streamConnection->setSemaphores(WTFMove(wakeUp), WTFMove(clientWait));
}

bool RemoteRenderingBackendProxy::isCached(const ImageBuffer& imageBuffer) const
{
    if (auto cachedImageBuffer = m_remoteResourceCacheProxy.cachedImageBuffer(imageBuffer.renderingResourceIdentifier())) {
        ASSERT_UNUSED(cachedImageBuffer, cachedImageBuffer == &imageBuffer);
        return true;
    }
    return false;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
