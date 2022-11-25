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
#include "RemoteRenderingBackend.h"

#if ENABLE(GPU_PROCESS)

#include "BufferIdentifierSet.h"
#include "FilterReference.h"
#include "GPUConnectionToWebProcess.h"
#include "Logging.h"
#include "PlatformImageBufferShareableBackend.h"
#include "QualifiedRenderingResourceIdentifier.h"
#include "RemoteDisplayListRecorder.h"
#include "RemoteDisplayListRecorderMessages.h"
#include "RemoteImageBuffer.h"
#include "RemoteMediaPlayerManagerProxy.h"
#include "RemoteMediaPlayerProxy.h"
#include "RemoteRenderingBackendCreationParameters.h"
#include "RemoteRenderingBackendMessages.h"
#include "RemoteRenderingBackendProxyMessages.h"
#include "SwapBuffersDisplayRequirement.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/HTMLCanvasElement.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/StdLibExtras.h>
#include <wtf/SystemTracing.h>

#if HAVE(IOSURFACE)
#include <WebCore/IOSurfacePool.h>
#endif

#if ENABLE(IPC_TESTING_API)
#define WEB_PROCESS_TERMINATE_CONDITION !m_gpuConnectionToWebProcess->connection().ignoreInvalidMessageForTesting()
#else
#define WEB_PROCESS_TERMINATE_CONDITION true
#endif

#define TERMINATE_WEB_PROCESS_WITH_MESSAGE(message) \
    if (WEB_PROCESS_TERMINATE_CONDITION) { \
        RELEASE_LOG_FAULT(IPC, "Requesting termination of web process %" PRIu64 " for reason: %" PUBLIC_LOG_STRING, m_gpuConnectionToWebProcess->webProcessIdentifier().toUInt64(), #message); \
        m_gpuConnectionToWebProcess->terminateWebProcess(); \
    }

#define MESSAGE_CHECK(assertion, message) do { \
    if (UNLIKELY(!(assertion))) { \
        TERMINATE_WEB_PROCESS_WITH_MESSAGE(message); \
        return; \
    } \
} while (0)

#define MESSAGE_CHECK_WITH_RETURN_VALUE(assertion, returnValue, message) do { \
    if (UNLIKELY(!(assertion))) { \
        TERMINATE_WEB_PROCESS_WITH_MESSAGE(message); \
        return (returnValue); \
    } \
} while (0)

namespace WebKit {
using namespace WebCore;

Ref<RemoteRenderingBackend> RemoteRenderingBackend::create(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteRenderingBackendCreationParameters&& creationParameters, IPC::StreamServerConnection::Handle&& connectionHandle)
{
    auto instance = adoptRef(*new RemoteRenderingBackend(gpuConnectionToWebProcess, WTFMove(creationParameters), WTFMove(connectionHandle)));
    instance->startListeningForIPC();
    return instance;
}

RemoteRenderingBackend::RemoteRenderingBackend(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteRenderingBackendCreationParameters&& creationParameters, IPC::StreamServerConnection::Handle&& connectionHandle)
    : m_workQueue(IPC::StreamConnectionWorkQueue::create("RemoteRenderingBackend work queue"))
    , m_streamConnection(IPC::StreamServerConnection::create(WTFMove(connectionHandle), m_workQueue.get()))
    , m_remoteResourceCache(gpuConnectionToWebProcess.webProcessIdentifier())
    , m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
    , m_resourceOwner(gpuConnectionToWebProcess.webProcessIdentity())
    , m_renderingBackendIdentifier(creationParameters.identifier)
#if HAVE(IOSURFACE)
    , m_ioSurfacePool(IOSurfacePool::create())
#endif
{
    ASSERT(RunLoop::isMain());
}

RemoteRenderingBackend::~RemoteRenderingBackend() = default;

void RemoteRenderingBackend::startListeningForIPC()
{
    {
        Locker locker { m_remoteDisplayListsLock };
        m_canRegisterRemoteDisplayLists = true;
    }
    m_streamConnection->startReceivingMessages(*this, Messages::RemoteRenderingBackend::messageReceiverName(), m_renderingBackendIdentifier.toUInt64());
    m_streamConnection->open();
    send(Messages::RemoteRenderingBackendProxy::DidInitialize(m_workQueue->wakeUpSemaphore(), m_streamConnection->clientWaitSemaphore()), m_renderingBackendIdentifier);
}

void RemoteRenderingBackend::stopListeningForIPC()
{
    ASSERT(RunLoop::isMain());
    m_streamConnection->invalidate();

    // This item is dispatched to the WorkQueue such that it will process it last, after any existing work.
    m_workQueue->stopAndWaitForCompletion([&] {
        // Make sure we destroy the ResourceCache on the WorkQueue since it gets populated on the WorkQueue.
        m_remoteResourceCache = { m_gpuConnectionToWebProcess->webProcessIdentifier() };
    });

    m_streamConnection->stopReceivingMessages(Messages::RemoteRenderingBackend::messageReceiverName(), m_renderingBackendIdentifier.toUInt64());

    {
        Locker locker { m_remoteDisplayListsLock };
        m_canRegisterRemoteDisplayLists = false;
        for (auto& remoteContext : std::exchange(m_remoteDisplayLists, { }))
            remoteContext.value->stopListeningForIPC();
    }
}

void RemoteRenderingBackend::dispatch(Function<void()>&& task)
{
    m_workQueue->dispatch(WTFMove(task));
}

IPC::Connection* RemoteRenderingBackend::messageSenderConnection() const
{
    return &m_streamConnection->connection();
}

uint64_t RemoteRenderingBackend::messageSenderDestinationID() const
{
    return m_renderingBackendIdentifier.toUInt64();
}

void RemoteRenderingBackend::didCreateImageBufferBackend(ImageBufferBackendHandle handle, QualifiedRenderingResourceIdentifier renderingResourceIdentifier, RemoteDisplayListRecorder& remoteDisplayList)
{
    {
        Locker locker { m_remoteDisplayListsLock };
        if (m_canRegisterRemoteDisplayLists)
            m_remoteDisplayLists.add(renderingResourceIdentifier, remoteDisplayList);
    }
    MESSAGE_CHECK(renderingResourceIdentifier.processIdentifier() == m_gpuConnectionToWebProcess->webProcessIdentifier(), "Sending didCreateImageBufferBackend() message to the wrong web process.");
    send(Messages::RemoteRenderingBackendProxy::DidCreateImageBufferBackend(WTFMove(handle), renderingResourceIdentifier.object()), m_renderingBackendIdentifier);
}

void RemoteRenderingBackend::didFlush(DisplayListRecorderFlushIdentifier flushIdentifier, QualifiedRenderingResourceIdentifier renderingResourceIdentifier)
{
    MESSAGE_CHECK(renderingResourceIdentifier.processIdentifier() == m_gpuConnectionToWebProcess->webProcessIdentifier(), "Sending didFlush() message to the wrong web process.");
    send(Messages::RemoteRenderingBackendProxy::DidFlush(flushIdentifier, renderingResourceIdentifier.object()), m_renderingBackendIdentifier);
}

void RemoteRenderingBackend::createImageBuffer(const FloatSize& logicalSize, RenderingMode renderingMode, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, RenderingResourceIdentifier imageBufferResourceIdentifier)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    createImageBufferWithQualifiedIdentifier(logicalSize, renderingMode, purpose, resolutionScale, colorSpace, pixelFormat, { imageBufferResourceIdentifier, m_gpuConnectionToWebProcess->webProcessIdentifier() });
}

void RemoteRenderingBackend::createImageBufferWithQualifiedIdentifier(const FloatSize& logicalSize, RenderingMode renderingMode, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, QualifiedRenderingResourceIdentifier imageBufferResourceIdentifier)
{
    ASSERT(!RunLoop::isMain());

    RefPtr<ImageBuffer> imageBuffer;

    if (renderingMode == RenderingMode::Accelerated) {
        if (auto acceleratedImageBuffer = RemoteImageBuffer::create<AcceleratedImageBufferShareableMappedBackend>(logicalSize, resolutionScale, colorSpace, pixelFormat, purpose, *this, imageBufferResourceIdentifier)) {
            // Mark the IOSurface as being owned by the WebProcess even though it was constructed by the GPUProcess so that Jetsam knows which process to kill.
            if (m_resourceOwner)
                acceleratedImageBuffer->setOwnershipIdentity(m_resourceOwner);
            imageBuffer = WTFMove(acceleratedImageBuffer);
        }
    }

    if (!imageBuffer)
        imageBuffer = RemoteImageBuffer::create<UnacceleratedImageBufferShareableBackend>(logicalSize, resolutionScale, colorSpace, pixelFormat, purpose, *this, imageBufferResourceIdentifier);

    if (!imageBuffer) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_remoteResourceCache.cacheImageBuffer(*imageBuffer, imageBufferResourceIdentifier);
}

void RemoteRenderingBackend::getPixelBufferForImageBuffer(RenderingResourceIdentifier imageBuffer, PixelBufferFormat&& destinationFormat, IntRect&& srcRect, CompletionHandler<void()>&& completionHandler)
{
    MESSAGE_CHECK(m_getPixelBufferSharedMemory, "No shared memory for getPixelBufferForImageBuffer");
    MESSAGE_CHECK(PixelBuffer::supportedPixelFormat(destinationFormat.pixelFormat), "Pixel format not supported");
    QualifiedRenderingResourceIdentifier qualifiedImageBuffer { imageBuffer, m_gpuConnectionToWebProcess->webProcessIdentifier() };
    if (auto imageBuffer = m_remoteResourceCache.cachedImageBuffer(qualifiedImageBuffer)) {
        auto pixelBuffer = imageBuffer->getPixelBuffer(destinationFormat, srcRect);
        if (pixelBuffer) {
            MESSAGE_CHECK(pixelBuffer->sizeInBytes() <= m_getPixelBufferSharedMemory->size(), "Shmem for return of getPixelBuffer is too small");
            memcpy(m_getPixelBufferSharedMemory->data(), pixelBuffer->bytes(), pixelBuffer->sizeInBytes());
        } else
            memset(m_getPixelBufferSharedMemory->data(), 0, m_getPixelBufferSharedMemory->size());
    }
    completionHandler();
}

void RemoteRenderingBackend::getPixelBufferForImageBufferWithNewMemory(RenderingResourceIdentifier imageBuffer, SharedMemory::Handle&& handle, PixelBufferFormat&& destinationFormat, IntRect&& srcRect, CompletionHandler<void()>&& completionHandler)
{
    m_getPixelBufferSharedMemory = nullptr;
    auto sharedMemory = WebKit::SharedMemory::map(handle, WebKit::SharedMemory::Protection::ReadWrite);
    MESSAGE_CHECK(sharedMemory, "Shared memory could not be mapped.");
    MESSAGE_CHECK(sharedMemory->size() <= HTMLCanvasElement::maxActivePixelMemory(), "Shared memory too big.");
    m_getPixelBufferSharedMemory = WTFMove(sharedMemory);
    getPixelBufferForImageBuffer(imageBuffer, WTFMove(destinationFormat), WTFMove(srcRect), WTFMove(completionHandler));
}

void RemoteRenderingBackend::destroyGetPixelBufferSharedMemory()
{
    m_getPixelBufferSharedMemory = nullptr;
}

void RemoteRenderingBackend::putPixelBufferForImageBuffer(RenderingResourceIdentifier imageBuffer, Ref<PixelBuffer>&& pixelBuffer, IntRect&& srcRect, IntPoint&& destPoint, AlphaPremultiplication destFormat)
{
    QualifiedRenderingResourceIdentifier qualifiedImageBuffer { imageBuffer, m_gpuConnectionToWebProcess->webProcessIdentifier() };
    if (auto imageBuffer = m_remoteResourceCache.cachedImageBuffer(qualifiedImageBuffer)) {
        imageBuffer->putPixelBuffer(pixelBuffer, srcRect, destPoint, destFormat);
    }
}

void RemoteRenderingBackend::getShareableBitmapForImageBuffer(RenderingResourceIdentifier identifier, PreserveResolution preserveResolution, CompletionHandler<void(ShareableBitmapHandle&&)>&& completionHandler)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    getShareableBitmapForImageBufferWithQualifiedIdentifier({ identifier, m_gpuConnectionToWebProcess->webProcessIdentifier() }, preserveResolution, WTFMove(completionHandler));
}

void RemoteRenderingBackend::getShareableBitmapForImageBufferWithQualifiedIdentifier(QualifiedRenderingResourceIdentifier identifier, PreserveResolution preserveResolution, CompletionHandler<void(ShareableBitmapHandle&&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    ShareableBitmapHandle handle;
    [&]() {
        auto imageBuffer = m_remoteResourceCache.cachedImageBuffer(identifier);
        if (!imageBuffer)
            return;
        auto backendSize = imageBuffer->backendSize();
        auto logicalSize = imageBuffer->logicalSize();
        auto resultSize = preserveResolution == PreserveResolution::Yes ? backendSize : imageBuffer->truncatedLogicalSize();
        auto bitmap = ShareableBitmap::create(resultSize, { imageBuffer->colorSpace() });
        if (!bitmap)
            return;
        auto context = bitmap->createGraphicsContext();
        if (!context)
            return;
        context->drawImageBuffer(*imageBuffer, FloatRect { { }, resultSize }, FloatRect { { }, logicalSize }, { CompositeOperator::Copy });
        if (auto bitmapHandle = bitmap->createHandle())
            handle = WTFMove(*bitmapHandle);
    }();
    completionHandler(WTFMove(handle));
}

void RemoteRenderingBackend::getFilteredImageForImageBuffer(RenderingResourceIdentifier identifier, IPC::FilterReference&& filterReference, CompletionHandler<void(ShareableBitmapHandle&&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto filter = filterReference.takeFilter();

    ShareableBitmapHandle handle;
    [&]() {
        auto imageBuffer = m_remoteResourceCache.cachedImageBuffer({ identifier, m_gpuConnectionToWebProcess->webProcessIdentifier() });
        if (!imageBuffer)
            return;
        auto image = imageBuffer->filteredImage(filter);
        if (!image)
            return;
        auto imageSize = image->size();
        auto bitmap = ShareableBitmap::create(IntSize(imageSize), { imageBuffer->colorSpace() });
        if (!bitmap)
            return;
        auto context = bitmap->createGraphicsContext();
        if (!context)
            return;
        context->drawImage(*image, FloatPoint());
        if (auto bitmapHandle = bitmap->createHandle())
            handle = WTFMove(*bitmapHandle);
    }();
    completionHandler(WTFMove(handle));
}

void RemoteRenderingBackend::cacheNativeImage(const ShareableBitmapHandle& handle, RenderingResourceIdentifier nativeImageResourceIdentifier)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    cacheNativeImageWithQualifiedIdentifier(handle, { nativeImageResourceIdentifier, m_gpuConnectionToWebProcess->webProcessIdentifier() });
}

void RemoteRenderingBackend::cacheNativeImageWithQualifiedIdentifier(const ShareableBitmapHandle& handle, QualifiedRenderingResourceIdentifier nativeImageResourceIdentifier)
{
    ASSERT(!RunLoop::isMain());

    auto bitmap = ShareableBitmap::create(handle);
    if (!bitmap)
        return;

    auto image = NativeImage::create(bitmap->createPlatformImage(DontCopyBackingStore, ShouldInterpolate::Yes), nativeImageResourceIdentifier.object());
    if (!image)
        return;

    m_remoteResourceCache.cacheNativeImage(image.releaseNonNull(), nativeImageResourceIdentifier);
}

void RemoteRenderingBackend::cacheFont(Ref<Font>&& font)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    auto renderingResourceIdentifier = font->renderingResourceIdentifier();
    cacheFontWithQualifiedIdentifier(WTFMove(font), { renderingResourceIdentifier, m_gpuConnectionToWebProcess->webProcessIdentifier() });
}

void RemoteRenderingBackend::cacheFontWithQualifiedIdentifier(Ref<Font>&& font, QualifiedRenderingResourceIdentifier fontResourceIdentifier)
{
    ASSERT(!RunLoop::isMain());
    m_remoteResourceCache.cacheFont(WTFMove(font), fontResourceIdentifier);
}

void RemoteRenderingBackend::cacheDecomposedGlyphs(Ref<DecomposedGlyphs>&& decomposedGlyphs)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    auto renderingResourceIdentifier = decomposedGlyphs->renderingResourceIdentifier();
    cacheDecomposedGlyphsWithQualifiedIdentifier(WTFMove(decomposedGlyphs), { renderingResourceIdentifier, m_gpuConnectionToWebProcess->webProcessIdentifier() });
}

void RemoteRenderingBackend::cacheDecomposedGlyphsWithQualifiedIdentifier(Ref<DecomposedGlyphs>&& decomposedGlyphs, QualifiedRenderingResourceIdentifier decomposedGlyphsIdentifier)
{
    ASSERT(!RunLoop::isMain());
    m_remoteResourceCache.cacheDecomposedGlyphs(WTFMove(decomposedGlyphs), decomposedGlyphsIdentifier);
}

void RemoteRenderingBackend::releaseAllResources()
{
    ASSERT(!RunLoop::isMain());
    m_remoteResourceCache.releaseAllResources();
}

void RemoteRenderingBackend::releaseResource(RenderingResourceIdentifier renderingResourceIdentifier)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    releaseResourceWithQualifiedIdentifier({ renderingResourceIdentifier, m_gpuConnectionToWebProcess->webProcessIdentifier() });
}

void RemoteRenderingBackend::releaseResourceWithQualifiedIdentifier(QualifiedRenderingResourceIdentifier renderingResourceIdentifier)
{
    ASSERT(!RunLoop::isMain());
    {
        Locker locker { m_remoteDisplayListsLock };
        if (auto remoteDisplayList = m_remoteDisplayLists.take(renderingResourceIdentifier))
            remoteDisplayList->clearImageBufferReference();
    }
    auto success = m_remoteResourceCache.releaseResource(renderingResourceIdentifier);
    MESSAGE_CHECK(success, "Resource is being released before being cached.");
}

static std::optional<ImageBufferBackendHandle> handleFromBuffer(ImageBuffer& buffer)
{
    auto* backend = buffer.ensureBackendCreated();
    if (!backend)
        return std::nullopt;

    auto* sharing = backend->toBackendSharing();
    if (is<ImageBufferBackendHandleSharing>(sharing))
        return downcast<ImageBufferBackendHandleSharing>(*sharing).createBackendHandle();

    return std::nullopt;
}

void RemoteRenderingBackend::prepareBuffersForDisplay(Vector<PrepareBackingStoreBuffersInputData> swapBuffersInput, CompletionHandler<void(const Vector<PrepareBackingStoreBuffersOutputData>&)>&& completionHandler)
{
    Vector<PrepareBackingStoreBuffersOutputData> outputData;
    outputData.resizeToFit(swapBuffersInput.size());

    for (unsigned i = 0; i < swapBuffersInput.size(); ++i)
        prepareLayerBuffersForDisplay(swapBuffersInput[i], outputData[i]);

    completionHandler(WTFMove(outputData));
}

// This is the GPU Process version of RemoteLayerBackingStore::prepareBuffers().
void RemoteRenderingBackend::prepareLayerBuffersForDisplay(const PrepareBackingStoreBuffersInputData& inputData, PrepareBackingStoreBuffersOutputData& outputData)
{
    auto fetchBuffer = [&](std::optional<RenderingResourceIdentifier> identifier) -> ImageBuffer* {
        return identifier ? m_remoteResourceCache.cachedImageBuffer({ *identifier, m_gpuConnectionToWebProcess->webProcessIdentifier() }) : nullptr;
    };

    auto bufferIdentifier = [](ImageBuffer* buffer) -> std::optional<RenderingResourceIdentifier> {
        if (!buffer)
            return std::nullopt;
        return buffer->renderingResourceIdentifier();
    };

    auto frontBuffer = fetchBuffer(inputData.bufferSet.front);
    auto backBuffer = fetchBuffer(inputData.bufferSet.back);
    auto secondaryBackBuffer = fetchBuffer(inputData.bufferSet.secondaryBack);

    LOG_WITH_STREAM(RemoteRenderingBufferVolatility, stream << "GPU Process: RemoteRenderingBackend::prepareBuffersForDisplay - front "
        << inputData.bufferSet.front << " (in-use " << (frontBuffer && frontBuffer->isInUse()) << ") "
        << inputData.bufferSet.back << " (in-use " << (backBuffer && backBuffer->isInUse()) << ") "
        << inputData.bufferSet.secondaryBack << " (in-use " << (secondaryBackBuffer && secondaryBackBuffer->isInUse()) << ") ");

    bool needsFullDisplay = false;

    if (frontBuffer) {
        auto previousState = frontBuffer->setNonVolatile();
        if (previousState == SetNonVolatileResult::Empty)
            needsFullDisplay = true;
    }

    if (frontBuffer && !needsFullDisplay && inputData.hasEmptyDirtyRegion) {
        // No swap necessary, but we do need to return the front buffer handle.
        outputData.frontBufferHandle = handleFromBuffer(*frontBuffer);
        outputData.bufferSet = BufferIdentifierSet { bufferIdentifier(frontBuffer), bufferIdentifier(backBuffer), bufferIdentifier(secondaryBackBuffer) };
        outputData.displayRequirement = SwapBuffersDisplayRequirement::NeedsNoDisplay;
        return;
    }
    
    if (!frontBuffer || !inputData.supportsPartialRepaint)
        needsFullDisplay = true;

    if (!backBuffer || backBuffer->isInUse()) {
        std::swap(backBuffer, secondaryBackBuffer);

        // When pulling the secondary back buffer out of hibernation (to become
        // the new front buffer), if it is somehow still in use (e.g. we got
        // three swaps ahead of the render server), just give up and discard it.
        if (backBuffer && backBuffer->isInUse())
            backBuffer = nullptr;
    }

    std::swap(frontBuffer, backBuffer);

    outputData.bufferSet = BufferIdentifierSet { bufferIdentifier(frontBuffer), bufferIdentifier(backBuffer), bufferIdentifier(secondaryBackBuffer) };
    if (frontBuffer) {
        auto previousState = frontBuffer->setNonVolatile();
        if (previousState == SetNonVolatileResult::Empty)
            needsFullDisplay = true;

        outputData.frontBufferHandle = handleFromBuffer(*frontBuffer);
    } else
        needsFullDisplay = true;

    LOG_WITH_STREAM(RemoteRenderingBufferVolatility, stream << "GPU Process: prepareBuffersForDisplay - swapped from ["
        << inputData.bufferSet.front << ", " << inputData.bufferSet.back << ", " << inputData.bufferSet.secondaryBack << "] to ["
        << outputData.bufferSet.front << ", " << outputData.bufferSet.back << ", " << outputData.bufferSet.secondaryBack << "]");

    outputData.displayRequirement = needsFullDisplay ? SwapBuffersDisplayRequirement::NeedsFullDisplay : SwapBuffersDisplayRequirement::NeedsNormalDisplay;
}

void RemoteRenderingBackend::markSurfacesVolatile(MarkSurfacesAsVolatileRequestIdentifier requestIdentifier, const Vector<RenderingResourceIdentifier>& identifiers)
{
    LOG_WITH_STREAM(RemoteRenderingBufferVolatility, stream << "GPU Process: RemoteRenderingBackend::markSurfacesVolatile " << identifiers);

    auto makeVolatile = [](ImageBuffer& imageBuffer) {
        imageBuffer.releaseGraphicsContext();
        return imageBuffer.setVolatile();
    };

    Vector<RenderingResourceIdentifier> markedVolatileBufferIdentifiers;
    for (auto identifier : identifiers) {
        auto imageBuffer = m_remoteResourceCache.cachedImageBuffer({ identifier, m_gpuConnectionToWebProcess->webProcessIdentifier() });
        if (imageBuffer) {
            if (makeVolatile(*imageBuffer))
                markedVolatileBufferIdentifiers.append(identifier);
        } else
            LOG_WITH_STREAM(RemoteRenderingBufferVolatility, stream << " failed to find ImageBuffer for identifier " << identifier);
    }

    LOG_WITH_STREAM(RemoteRenderingBufferVolatility, stream << "GPU Process: markSurfacesVolatile - surfaces marked volatile " << markedVolatileBufferIdentifiers);

    bool didMarkAllLayerAsVolatile = identifiers.size() == markedVolatileBufferIdentifiers.size();
    send(Messages::RemoteRenderingBackendProxy::DidMarkLayersAsVolatile(requestIdentifier, markedVolatileBufferIdentifiers, didMarkAllLayerAsVolatile), m_renderingBackendIdentifier);
}

void RemoteRenderingBackend::finalizeRenderingUpdate(RenderingUpdateID renderingUpdateID)
{
    send(Messages::RemoteRenderingBackendProxy::DidFinalizeRenderingUpdate(renderingUpdateID), m_renderingBackendIdentifier);
}

void RemoteRenderingBackend::performWithMediaPlayerOnMainThread(MediaPlayerIdentifier identifier, Function<void(MediaPlayer&)>&& callback)
{
    callOnMainRunLoopAndWait([&, gpuConnectionToWebProcess = m_gpuConnectionToWebProcess, identifier] {
        if (auto player = gpuConnectionToWebProcess->remoteMediaPlayerManagerProxy().mediaPlayer(identifier))
            callback(*player);
    });
}

void RemoteRenderingBackend::lowMemoryHandler(Critical, Synchronous)
{
    ASSERT(isMainRunLoop());
#if HAVE(IOSURFACE)
    m_ioSurfacePool->discardAllSurfaces();
#endif
}

} // namespace WebKit

#undef MESSAGE_CHECK
#undef MESSAGE_CHECK_WITH_RETURN_VALUE

#endif // ENABLE(GPU_PROCESS)
