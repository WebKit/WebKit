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
#include "ImageBufferShareableBitmapBackend.h"
#include "Logging.h"
#include "RemoteDisplayListRecorderProxy.h"
#include "RemoteImageBufferMessages.h"
#include "RemoteImageBufferProxy.h"
#include "RemoteImageBufferProxyMessages.h"
#include "RemoteImageBufferSetProxy.h"
#include "RemoteImageBufferSetProxyMessages.h"
#include "RemoteRenderingBackendMessages.h"
#include "RemoteRenderingBackendProxyMessages.h"
#include "SwapBuffersDisplayRequirement.h"
#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <JavaScriptCore/TypedArrayInlines.h>
#include <WebCore/FontCustomPlatformData.h>
#include <wtf/text/TextStream.h>

#if HAVE(IOSURFACE)
#include "ImageBufferRemoteIOSurfaceBackend.h"
#include "ImageBufferShareableMappedIOSurfaceBackend.h"
#endif

namespace WebKit {

using namespace WebCore;

std::unique_ptr<RemoteRenderingBackendProxy> RemoteRenderingBackendProxy::create(WebPage& webPage)
{
    return create({ RenderingBackendIdentifier::generate(), webPage.webPageProxyIdentifier(), webPage.identifier() }, RunLoop::main());
}

std::unique_ptr<RemoteRenderingBackendProxy> RemoteRenderingBackendProxy::create(const RemoteRenderingBackendCreationParameters& parameters, SerialFunctionDispatcher& dispatcher)
{
    return std::unique_ptr<RemoteRenderingBackendProxy>(new RemoteRenderingBackendProxy(parameters, dispatcher));
}

RemoteRenderingBackendProxy::RemoteRenderingBackendProxy(const RemoteRenderingBackendCreationParameters& parameters, SerialFunctionDispatcher& dispatcher)
    : m_parameters(parameters)
    , m_dispatcher(dispatcher)
    , m_queue(WorkQueue::create("RemoteRenderingBackendProxy", WorkQueue::QOS::UserInteractive))
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
        auto connectionPair = IPC::StreamClientConnection::create(connectionBufferSizeLog2);
        if (!connectionPair)
            CRASH();
        auto [streamConnection, serverHandle] = WTFMove(*connectionPair);
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
template<typename T, typename U, typename V>
auto RemoteRenderingBackendProxy::send(T&& message, ObjectIdentifierGeneric<U, V> destination)
{
    auto result = streamConnection().send(std::forward<T>(message), destination, defaultTimeout);
    if (UNLIKELY(result != IPC::Error::NoError)) {
        RELEASE_LOG(RemoteLayerBuffers, "[pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", renderingBackend=%" PRIu64 "] RemoteRenderingBackendProxy::send - failed, name:%" PUBLIC_LOG_STRING ", error:%" PUBLIC_LOG_STRING,
            m_parameters.pageProxyID.toUInt64(), m_parameters.pageID.toUInt64(), m_parameters.identifier.toUInt64(), IPC::description(T::name()), IPC::errorAsString(result));
    }
    return result;
}

template<typename T, typename U, typename V>
auto RemoteRenderingBackendProxy::sendSync(T&& message, ObjectIdentifierGeneric<U, V> destination)
{
    auto result = streamConnection().sendSync(std::forward<T>(message), destination, defaultTimeout);
    if (UNLIKELY(!result.succeeded())) {
        RELEASE_LOG(RemoteLayerBuffers, "[pageProxyID=%" PRIu64 ", webPageID=%" PRIu64 ", renderingBackend=%" PRIu64 "] RemoteRenderingBackendProxy::sendSync - failed, name:%" PUBLIC_LOG_STRING ", error:%" PUBLIC_LOG_STRING,
            m_parameters.pageProxyID.toUInt64(), m_parameters.pageID.toUInt64(), m_parameters.identifier.toUInt64(), IPC::description(T::name()), IPC::errorAsString(result.error()));
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

    for (auto bufferSet : m_bufferSets) {
        bufferSet.value->remoteBufferSetWasDestroyed();
        send(Messages::RemoteRenderingBackend::CreateRemoteImageBufferSet(bufferSet.value->identifier(), bufferSet.value->displayListResourceIdentifier()));
    }
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

RefPtr<ImageBuffer> RemoteRenderingBackendProxy::createImageBuffer(const FloatSize& size, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, OptionSet<ImageBufferOptions> options)
{
    RefPtr<ImageBuffer> imageBuffer;

    bool avoidBackendSizeCheckForTesting = options.contains(ImageBufferOptions::AvoidBackendSizeCheckForTesting);

#if HAVE(IOSURFACE)
    if (options.contains(ImageBufferOptions::Accelerated)) {
        // Unless DOM rendering is always enabled when any GPU process rendering is enabled,
        // we need to create ImageBuffers for e.g. Canvas that are actually mapped into the
        // Web Content process, so they can be painted into the tiles.
        if (!WebProcess::singleton().shouldUseRemoteRenderingFor(RenderingPurpose::DOM))
            imageBuffer = RemoteImageBufferProxy::create<ImageBufferShareableMappedIOSurfaceBackend>(size, resolutionScale, colorSpace, pixelFormat, purpose, *this, avoidBackendSizeCheckForTesting);
        else
            imageBuffer = RemoteImageBufferProxy::create<ImageBufferRemoteIOSurfaceBackend>(size, resolutionScale, colorSpace, pixelFormat, purpose, *this, avoidBackendSizeCheckForTesting);
    }
#endif
    if (!imageBuffer)
        imageBuffer = RemoteImageBufferProxy::create<ImageBufferShareableBitmapBackend>(size, resolutionScale, colorSpace, pixelFormat, purpose, *this, avoidBackendSizeCheckForTesting);

    if (imageBuffer) {
        createRemoteImageBuffer(*imageBuffer);
        return imageBuffer;
    }

    return nullptr;
}

std::unique_ptr<RemoteDisplayListRecorderProxy> RemoteRenderingBackendProxy::createDisplayListRecorder(WebCore::RenderingResourceIdentifier renderingResourceIdentifier, const FloatSize& size, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, OptionSet<ImageBufferOptions> options)
{
    ASSERT(WebProcess::singleton().shouldUseRemoteRenderingFor(RenderingPurpose::DOM));
    ImageBufferParameters parameters { size, resolutionScale, colorSpace, pixelFormat, purpose };
    auto renderingMode = RenderingMode::Unaccelerated;
    auto transform = ImageBufferBackend::calculateBaseTransform(ImageBuffer::backendParameters(parameters), ImageBufferShareableBitmapBackend::isOriginAtBottomLeftCorner);

#if HAVE(IOSURFACE)
    if (options.contains(ImageBufferOptions::Accelerated)) {
        renderingMode = RenderingMode::Accelerated;
        transform = ImageBufferBackend::calculateBaseTransform(ImageBuffer::backendParameters(parameters), ImageBufferRemoteIOSurfaceBackend::isOriginAtBottomLeftCorner);
    }
#endif

    return makeUnique<RemoteDisplayListRecorderProxy>(*this, renderingResourceIdentifier, colorSpace, renderingMode, FloatRect { { }, size }, transform);
}

void RemoteRenderingBackendProxy::releaseImageBuffer(RenderingResourceIdentifier renderingResourceIdentifier)
{
    if (!m_streamConnection)
        return;
    send(Messages::RemoteRenderingBackend::ReleaseImageBuffer(renderingResourceIdentifier));
}

RefPtr<RemoteImageBufferSetProxy>  RemoteRenderingBackendProxy::createRemoteImageBufferSet()
{
    RefPtr<RemoteImageBufferSetProxy> result = adoptRef(new RemoteImageBufferSetProxy(*this));
    send(Messages::RemoteRenderingBackend::CreateRemoteImageBufferSet(result->identifier(), result->displayListResourceIdentifier()));

    auto addResult = m_bufferSets.add(result->identifier(), result);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    return result;
}

void RemoteRenderingBackendProxy::releaseRemoteImageBufferSet(RemoteImageBufferSetProxy& bufferSet)
{
    bool success = m_bufferSets.remove(bufferSet.identifier());
    ASSERT_UNUSED(success, success);

    if (!m_streamConnection)
        return;
    send(Messages::RemoteRenderingBackend::ReleaseRemoteImageBufferSet(bufferSet.identifier()));
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
        auto sendResult = sendSync(Messages::RemoteImageBuffer::GetPixelBufferWithNewMemory(WTFMove(*handle), destinationFormat, srcRect), imageBuffer);
        if (!sendResult.succeeded())
            return false;
    } else {
        if (!m_getPixelBufferSharedMemory)
            return false;
        auto sendResult = sendSync(Messages::RemoteImageBuffer::GetPixelBuffer(destinationFormat, srcRect), imageBuffer);
        if (!sendResult.succeeded())
            return false;
    }
    memcpy(result.data(), m_getPixelBufferSharedMemory->data(), result.size());
    return true;
}

void RemoteRenderingBackendProxy::putPixelBufferForImageBuffer(RenderingResourceIdentifier imageBuffer, const PixelBuffer& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    send(Messages::RemoteImageBuffer::PutPixelBuffer(Ref { const_cast<PixelBuffer&>(pixelBuffer) }, srcRect, destPoint, destFormat), imageBuffer);
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
    auto sendResult = sendSync(Messages::RemoteImageBuffer::GetShareableBitmap(preserveResolution), imageBuffer);
    if (!sendResult.succeeded())
        return { };
    auto [handle] = sendResult.takeReply();
    if (!handle)
        return { };
    handle->takeOwnershipOfMemory(MemoryLedger::Graphics);
    return ShareableBitmap::create(WTFMove(*handle));
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
    send(Messages::RemoteRenderingBackend::CacheGradient(WTFMove(gradient)));
}

void RemoteRenderingBackendProxy::cacheFilter(Ref<Filter>&& filter)
{
    send(Messages::RemoteRenderingBackend::CacheFilter(WTFMove(filter)));
}

void RemoteRenderingBackendProxy::releaseAllDrawingResources()
{
    if (!m_streamConnection)
        return;
    send(Messages::RemoteRenderingBackend::ReleaseAllDrawingResources());
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
Vector<SwapBuffersDisplayRequirement> RemoteRenderingBackendProxy::prepareImageBufferSetsForDisplay(Vector<LayerPrepareBuffersData>&& prepareBuffersInput)
{
    if (prepareBuffersInput.isEmpty())
        return Vector<SwapBuffersDisplayRequirement>();

    bool needsSync = false;

    auto inputData = WTF::map(prepareBuffersInput, [&](auto& perLayerData) {
        // If the front buffer might be volatile, then we have to wait for the callback
        // to find out we were able to copy pixels from it or if it had been discarded.
        if (perLayerData.bufferSet->requestedVolatility().contains(BufferInSetType::Front))
            needsSync = true;

        // Using the  will mark buffers as non-volatile and
        // we don't know exactly which. Assume they all are non-volatile.
        perLayerData.bufferSet->clearVolatilityUntilAfter(m_currentVolatilityRequest);
        perLayerData.bufferSet->willPrepareForDisplay();

        return ImageBufferSetPrepareBufferForDisplayInputData {
            perLayerData.bufferSet->identifier(),
            perLayerData.dirtyRegion,
            perLayerData.supportsPartialRepaint,
            perLayerData.hasEmptyDirtyRegion,
            perLayerData.requiresClearedPixels,
        };
    });

    LOG_WITH_STREAM(RemoteLayerBuffers, stream << "RemoteRenderingBackendProxy::prepareImageBufferSetsForDisplay - input buffers  " << inputData);

    Vector<SwapBuffersDisplayRequirement> result;
    if (needsSync) {
        auto sendResult = streamConnection().sendSync(Messages::RemoteRenderingBackend::PrepareImageBufferSetsForDisplaySync(inputData, m_renderingUpdateID), renderingBackendIdentifier(), defaultTimeout);
        if (!sendResult.succeeded()) {
            result.grow(inputData.size());
            for (auto& displayRequirement : result)
                displayRequirement = SwapBuffersDisplayRequirement::NeedsFullDisplay;
        } else
            std::tie(result) = sendResult.takeReply();
    } else {
        streamConnection().send(Messages::RemoteRenderingBackend::PrepareImageBufferSetsForDisplay(inputData, m_renderingUpdateID), renderingBackendIdentifier(), defaultTimeout);
        result.grow(inputData.size());
        for (auto& displayRequirement : result)
            displayRequirement = SwapBuffersDisplayRequirement::NeedsNormalDisplay;
    }
    return result;
}
#endif

void RemoteRenderingBackendProxy::markSurfacesVolatile(Vector<std::pair<Ref<RemoteImageBufferSetProxy>, OptionSet<BufferInSetType>>>&& bufferSets, CompletionHandler<void(bool)>&& completionHandler, bool forcePurge)
{
    Vector<std::pair<RemoteImageBufferSetIdentifier, OptionSet<BufferInSetType>>> identifiers;
    for (auto& pair : bufferSets) {
        identifiers.append(std::make_pair(pair.first->identifier(), pair.second));
        pair.first->addRequestedVolatility(pair.second);
    }
    m_currentVolatilityRequest = MarkSurfacesAsVolatileRequestIdentifier::generate();
    m_markAsVolatileRequests.add(m_currentVolatilityRequest, WTFMove(completionHandler));

    send(Messages::RemoteRenderingBackend::MarkSurfacesVolatile(m_currentVolatilityRequest, identifiers, forcePurge));
}

void RemoteRenderingBackendProxy::didMarkLayersAsVolatile(MarkSurfacesAsVolatileRequestIdentifier requestIdentifier, Vector<std::pair<RemoteImageBufferSetIdentifier, OptionSet<BufferInSetType>>> markedBufferSets, bool didMarkAllLayersAsVolatile)
{
    ASSERT(requestIdentifier);
    auto completionHandler = m_markAsVolatileRequests.take(requestIdentifier);
    if (!completionHandler)
        return;

    for (auto& bufferSetIdentifierAndType : markedBufferSets) {
        auto bufferSet = m_bufferSets.get(bufferSetIdentifierAndType.first);
        if (!bufferSet)
            continue;

        bufferSet->setConfirmedVolatility(requestIdentifier, bufferSetIdentifierAndType.second);
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

#if USE(GRAPHICS_LAYER_WC)

// Because RemoteRenderingBackend and RemoteImageBuffer share a single stream connection at the moment,
// it is sufficient to flush the single command queue.
// This should take Vector<Ref<ImageBuffer>> as an argument if a RemoteImageBuffer uses own stream connection.
Function<bool()> RemoteRenderingBackendProxy::flushImageBuffers()
{
    IPC::Semaphore flushSemaphore;
    send(Messages::RemoteRenderingBackend::Flush(flushSemaphore));
    return [flushSemaphore = WTFMove(flushSemaphore)]() mutable {
        return flushSemaphore.waitFor(RemoteRenderingBackendProxy::defaultTimeout);
    };
}

#endif // USE(GRAPHICS_LAYER_WC)

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
