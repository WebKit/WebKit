/*
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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
#include "RemoteImageBufferProxy.h"
#include "RemoteRenderingBackendMessages.h"
#include "RemoteRenderingBackendProxyMessages.h"
#include "SwapBuffersDisplayRequirement.h"
#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <JavaScriptCore/TypedArrayInlines.h>
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
        m_streamConnection = WTFMove(streamConnection);
        m_streamConnection->open(*this, m_dispatcher);

        callOnMainRunLoopAndWait([this, serverHandle = WTFMove(serverHandle)]() mutable {
            m_connection = &WebProcess::singleton().ensureGPUProcessConnection().connection();
            m_connection->send(Messages::GPUConnectionToWebProcess::CreateRenderingBackend(m_parameters, WTFMove(serverHandle)), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
        });
    }
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
    auto pendingFlushes = std::exchange(m_pendingFlushes, { });
    for (auto& flushState : pendingFlushes.values())
        flushState->cancel();
}

RemoteRenderingBackendProxy::DidReceiveBackendCreationResult RemoteRenderingBackendProxy::waitForDidCreateImageBufferBackend()
{
    if (!streamConnection().waitForAndDispatchImmediately<Messages::RemoteRenderingBackendProxy::DidCreateImageBufferBackend>(renderingBackendIdentifier(), 1_s, IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives))
        return DidReceiveBackendCreationResult::TimeoutOrIPCFailure;
    return DidReceiveBackendCreationResult::ReceivedAnyResponse;
}

bool RemoteRenderingBackendProxy::waitForDidFlush()
{
    return streamConnection().waitForAndDispatchImmediately<Messages::RemoteRenderingBackendProxy::DidFlush>(renderingBackendIdentifier(), 1_s, IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives);
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

void RemoteRenderingBackendProxy::moveToSerializedBuffer(WebCore::RenderingResourceIdentifier identifier, RemoteSerializedImageBufferWriteReference&& reference)
{
    send(Messages::RemoteRenderingBackend::MoveToSerializedBuffer(identifier, reference));
}

void RemoteRenderingBackendProxy::moveToImageBuffer(RemoteSerializedImageBufferWriteReference&& reference, WebCore::RenderingResourceIdentifier identifier)
{
    send(Messages::RemoteRenderingBackend::MoveToImageBuffer(reference, identifier));
}

bool RemoteRenderingBackendProxy::getPixelBufferForImageBuffer(RenderingResourceIdentifier imageBuffer, const PixelBufferFormat& destinationFormat, const IntRect& srcRect, Span<uint8_t> result)
{
    if (auto handle = updateSharedMemoryForGetPixelBuffer(result.size())) {
        auto sendResult = sendSync(Messages::RemoteRenderingBackend::GetPixelBufferForImageBufferWithNewMemory(imageBuffer, WTFMove(*handle), destinationFormat, srcRect));
        if (!sendResult)
            return false;
    } else {
        if (!m_getPixelBufferSharedMemory)
            return false;
        auto sendResult = sendSync(Messages::RemoteRenderingBackend::GetPixelBufferForImageBuffer(imageBuffer, destinationFormat, srcRect));
        if (!sendResult)
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
    auto [handle] = sendResult.takeReplyOr(ShareableBitmapHandle { });
    if (handle.isNull())
        return { };
    handle.takeOwnershipOfMemory(MemoryLedger::Graphics);
    return ShareableBitmap::create(handle);
}

RefPtr<Image> RemoteRenderingBackendProxy::getFilteredImage(RenderingResourceIdentifier imageBuffer, Filter& filter)
{
    auto sendResult = sendSync(Messages::RemoteRenderingBackend::GetFilteredImageForImageBuffer(imageBuffer, filter));
    auto [handle] = sendResult.takeReplyOr(ShareableBitmapHandle { });
    if (handle.isNull())
        return { };

    handle.takeOwnershipOfMemory(MemoryLedger::Graphics);
    auto bitmap = ShareableBitmap::create(handle);
    if (!bitmap)
        return { };

    return bitmap->createImage();
}

void RemoteRenderingBackendProxy::cacheNativeImage(const ShareableBitmapHandle& handle, RenderingResourceIdentifier renderingResourceIdentifier)
{
    send(Messages::RemoteRenderingBackend::CacheNativeImage(handle, renderingResourceIdentifier));
}

void RemoteRenderingBackendProxy::cacheFont(Ref<Font>&& font)
{
    send(Messages::RemoteRenderingBackend::CacheFont(WTFMove(font)));
}

void RemoteRenderingBackendProxy::cacheDecomposedGlyphs(Ref<DecomposedGlyphs>&& decomposedGlyphs)
{
    send(Messages::RemoteRenderingBackend::CacheDecomposedGlyphs(WTFMove(decomposedGlyphs)));
}

void RemoteRenderingBackendProxy::releaseAllRemoteResources()
{
    send(Messages::RemoteRenderingBackend::ReleaseAllResources());
}

void RemoteRenderingBackendProxy::releaseRemoteResource(RenderingResourceIdentifier renderingResourceIdentifier)
{
    send(Messages::RemoteRenderingBackend::ReleaseResource(renderingResourceIdentifier));
}

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

    Vector<PrepareBackingStoreBuffersOutputData> outputData;
    auto sendResult = sendSync(Messages::RemoteRenderingBackend::PrepareBuffersForDisplay(inputData));
    if (!sendResult) {
        // GPU Process crashed. Set the output data to all null buffers, requiring a full display.
        outputData.resize(inputData.size());
        for (auto& perLayerOutputData : outputData)
            perLayerOutputData.displayRequirement = SwapBuffersDisplayRequirement::NeedsFullDisplay;
    } else
        std::tie(outputData) = sendResult.takeReply();

    RELEASE_ASSERT_WITH_MESSAGE(inputData.size() == outputData.size(), "PrepareBuffersForDisplay: mismatched buffer vector sizes");

    auto fetchBufferWithIdentifier = [&](std::optional<RenderingResourceIdentifier> identifier, std::optional<ImageBufferBackendHandle>&& handle = std::nullopt, bool isFrontBuffer = false) -> RefPtr<ImageBuffer> {
        if (!identifier)
            return nullptr;

        auto* buffer = m_remoteResourceCacheProxy.cachedImageBuffer(*identifier);
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

void RemoteRenderingBackendProxy::didCreateImageBufferBackend(ImageBufferBackendHandle&& handle, RenderingResourceIdentifier renderingResourceIdentifier)
{
    auto imageBuffer = m_remoteResourceCacheProxy.cachedImageBuffer(renderingResourceIdentifier);
    if (!imageBuffer)
        return;
    imageBuffer->didCreateImageBufferBackend(WTFMove(handle));
}

void RemoteRenderingBackendProxy::didFlush(DisplayListRecorderFlushIdentifier flushIdentifier)
{
    auto flush = m_pendingFlushes.take(flushIdentifier);
    ASSERT(flush);
    flush->markCompletedFlush(flushIdentifier);
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
        m_streamConnection->waitForAndDispatchImmediately<Messages::RemoteRenderingBackendProxy::DidInitialize>(renderingBackendIdentifier(), 3_s, IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives);
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

void RemoteRenderingBackendProxy::addPendingFlush(RemoteImageBufferProxyFlushState& flushState, DisplayListRecorderFlushIdentifier identifier)
{
    auto result = m_pendingFlushes.add(identifier, flushState);
    ASSERT_UNUSED(result, result.isNewEntry);
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
