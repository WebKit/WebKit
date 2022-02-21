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
#include "PlatformRemoteImageBuffer.h"
#include "QualifiedRenderingResourceIdentifier.h"
#include "RemoteDisplayListRecorderMessages.h"
#include "RemoteMediaPlayerManagerProxy.h"
#include "RemoteMediaPlayerProxy.h"
#include "RemoteRenderingBackendCreationParameters.h"
#include "RemoteRenderingBackendMessages.h"
#include "RemoteRenderingBackendProxyMessages.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/HTMLCanvasElement.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/StdLibExtras.h>
#include <wtf/SystemTracing.h>

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

Ref<RemoteRenderingBackend> RemoteRenderingBackend::create(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteRenderingBackendCreationParameters&& creationParameters, IPC::StreamConnectionBuffer&& streamBuffer)
{
    auto instance = adoptRef(*new RemoteRenderingBackend(gpuConnectionToWebProcess, WTFMove(creationParameters), WTFMove(streamBuffer)));
    instance->startListeningForIPC();
    return instance;
}

RemoteRenderingBackend::RemoteRenderingBackend(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteRenderingBackendCreationParameters&& creationParameters, IPC::StreamConnectionBuffer&& streamBuffer)
    : m_workQueue(IPC::StreamConnectionWorkQueue::create("RemoteRenderingBackend work queue"))
    , m_streamConnection(IPC::StreamServerConnection::create(gpuConnectionToWebProcess.connection(), WTFMove(streamBuffer), m_workQueue.get()))
    , m_remoteResourceCache(gpuConnectionToWebProcess.webProcessIdentifier())
    , m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
    , m_resourceOwner(gpuConnectionToWebProcess.webProcessIdentity())
    , m_renderingBackendIdentifier(creationParameters.identifier)
{
    ASSERT(RunLoop::isMain());
    send(Messages::RemoteRenderingBackendProxy::DidCreateWakeUpSemaphoreForDisplayListStream(m_workQueue->wakeUpSemaphore()), m_renderingBackendIdentifier);
}

RemoteRenderingBackend::~RemoteRenderingBackend() = default;

void RemoteRenderingBackend::startListeningForIPC()
{
    {
        Locker locker { m_remoteDisplayListsLock };
        m_canRegisterRemoteDisplayLists = true;
    }

    m_streamConnection->startReceivingMessages(*this, Messages::RemoteRenderingBackend::messageReceiverName(), m_renderingBackendIdentifier.toUInt64());
    // RemoteDisplayListRecorder messages depend on RemoteRenderingBackend, because RemoteRenderingBackend creates RemoteDisplayListRecorder and
    // makes a receive queue for it. In order to guarantee correct ordering, ensure that all RemoteDisplayListRecorder messages are processed in
    // the same sequence as RemoteRenderingBackend messages.
    m_streamConnection->startReceivingMessages(Messages::RemoteDisplayListRecorder::messageReceiverName());
}

void RemoteRenderingBackend::stopListeningForIPC()
{
    ASSERT(RunLoop::isMain());
    // Make sure we destroy the ResourceCache on the WorkQueue since it gets populated on the WorkQueue.
    // Make sure rendering resource request is released after destroying the cache.
    m_workQueue->dispatch([renderingResourcesRequest = WTFMove(m_renderingResourcesRequest), remoteResourceCache = WTFMove(m_remoteResourceCache)] { });
    m_workQueue->stopAndWaitForCompletion();

    m_streamConnection->stopReceivingMessages(Messages::RemoteRenderingBackend::messageReceiverName(), m_renderingBackendIdentifier.toUInt64());
    m_streamConnection->stopReceivingMessages(Messages::RemoteDisplayListRecorder::messageReceiverName());

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
    return &m_gpuConnectionToWebProcess->connection();
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

void RemoteRenderingBackend::didFlush(GraphicsContextFlushIdentifier flushIdentifier, QualifiedRenderingResourceIdentifier renderingResourceIdentifier)
{
    MESSAGE_CHECK(renderingResourceIdentifier.processIdentifier() == m_gpuConnectionToWebProcess->webProcessIdentifier(), "Sending didFlush() message to the wrong web process.");
    send(Messages::RemoteRenderingBackendProxy::DidFlush(flushIdentifier, renderingResourceIdentifier.object()), m_renderingBackendIdentifier);
}

void RemoteRenderingBackend::createImageBuffer(const FloatSize& logicalSize, RenderingMode renderingMode, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, RenderingResourceIdentifier imageBufferResourceIdentifier)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    createImageBufferWithQualifiedIdentifier(logicalSize, renderingMode, resolutionScale, colorSpace, pixelFormat, { imageBufferResourceIdentifier, m_gpuConnectionToWebProcess->webProcessIdentifier() });
}

void RemoteRenderingBackend::createImageBufferWithQualifiedIdentifier(const FloatSize& logicalSize, RenderingMode renderingMode, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, QualifiedRenderingResourceIdentifier imageBufferResourceIdentifier)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(renderingMode == RenderingMode::Accelerated || renderingMode == RenderingMode::Unaccelerated);

    RefPtr<ImageBuffer> imageBuffer;

    if (renderingMode == RenderingMode::Accelerated) {
        if (auto acceleratedImageBuffer = AcceleratedRemoteImageBuffer::create(logicalSize, resolutionScale, colorSpace, pixelFormat, *this, imageBufferResourceIdentifier)) {
            // Mark the IOSurface as being owned by the WebProcess even though it was constructed by the GPUProcess so that Jetsam knows which process to kill.
            if (m_resourceOwner)
                acceleratedImageBuffer->setOwnershipIdentity(m_resourceOwner);
            imageBuffer = WTFMove(acceleratedImageBuffer);
        }
    }

    if (!imageBuffer)
        imageBuffer = UnacceleratedRemoteImageBuffer::create(logicalSize, resolutionScale, colorSpace, pixelFormat, *this, imageBufferResourceIdentifier);

    if (!imageBuffer) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_remoteResourceCache.cacheImageBuffer(*imageBuffer, imageBufferResourceIdentifier);
    updateRenderingResourceRequest();
}

void RemoteRenderingBackend::getPixelBufferForImageBuffer(RenderingResourceIdentifier imageBuffer, WebCore::PixelBufferFormat&& destinationFormat, WebCore::IntRect&& srcRect, CompletionHandler<void()>&& completionHandler)
{
    MESSAGE_CHECK(m_getPixelBufferSharedMemory, "No shared memory for getPixelBufferForImageBuffer");
    QualifiedRenderingResourceIdentifier qualifiedImageBuffer { imageBuffer, m_gpuConnectionToWebProcess->webProcessIdentifier() };
    if (auto imageBuffer = m_remoteResourceCache.cachedImageBuffer(qualifiedImageBuffer)) {
        auto pixelBuffer = imageBuffer->getPixelBuffer(destinationFormat, srcRect);
        if (pixelBuffer) {
            MESSAGE_CHECK(pixelBuffer->data().byteLength() <= m_getPixelBufferSharedMemory->size(), "Shmem for return of getPixelBuffer is too small");
            memcpy(m_getPixelBufferSharedMemory->data(), pixelBuffer->data().data(), pixelBuffer->data().byteLength());
        } else
            memset(m_getPixelBufferSharedMemory->data(), 0, m_getPixelBufferSharedMemory->size());
    }
    completionHandler();
}

void RemoteRenderingBackend::getPixelBufferForImageBufferWithNewMemory(RenderingResourceIdentifier imageBuffer, SharedMemory::IPCHandle&& handle, WebCore::PixelBufferFormat&& destinationFormat, WebCore::IntRect&& srcRect, CompletionHandler<void()>&& completionHandler)
{
    m_getPixelBufferSharedMemory = nullptr;
    auto sharedMemory = WebKit::SharedMemory::map(handle.handle, WebKit::SharedMemory::Protection::ReadWrite);
    MESSAGE_CHECK(sharedMemory, "Shared memory could not be mapped.");
    MESSAGE_CHECK(sharedMemory->size() <= HTMLCanvasElement::maxActivePixelMemory(), "Shared memory too big.");
    m_getPixelBufferSharedMemory = WTFMove(sharedMemory);
    getPixelBufferForImageBuffer(imageBuffer, WTFMove(destinationFormat), WTFMove(srcRect), WTFMove(completionHandler));
}

void RemoteRenderingBackend::destroyGetPixelBufferSharedMemory()
{
    m_getPixelBufferSharedMemory = nullptr;
}

void RemoteRenderingBackend::putPixelBufferForImageBuffer(WebCore::RenderingResourceIdentifier imageBuffer, WebCore::PixelBuffer&& pixelBuffer, WebCore::IntRect&& srcRect, WebCore::IntPoint&& destPoint, WebCore::AlphaPremultiplication destFormat)
{
    QualifiedRenderingResourceIdentifier qualifiedImageBuffer { imageBuffer, m_gpuConnectionToWebProcess->webProcessIdentifier() };
    if (auto imageBuffer = m_remoteResourceCache.cachedImageBuffer(qualifiedImageBuffer))
        imageBuffer->putPixelBuffer(pixelBuffer, srcRect, destPoint, destFormat);
}

void RemoteRenderingBackend::getDataURLForImageBuffer(const String& mimeType, std::optional<double> quality, WebCore::PreserveResolution preserveResolution, WebCore::RenderingResourceIdentifier renderingResourceIdentifier, CompletionHandler<void(String&&)>&& completionHandler)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    getDataURLForImageBufferWithQualifiedIdentifier(mimeType, quality, preserveResolution, { renderingResourceIdentifier, m_gpuConnectionToWebProcess->webProcessIdentifier() }, WTFMove(completionHandler));
}

void RemoteRenderingBackend::getDataURLForImageBufferWithQualifiedIdentifier(const String& mimeType, std::optional<double> quality, WebCore::PreserveResolution preserveResolution, QualifiedRenderingResourceIdentifier renderingResourceIdentifier, CompletionHandler<void(String&&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    String urlString;
    if (auto imageBuffer = m_remoteResourceCache.cachedImageBuffer(renderingResourceIdentifier))
        urlString = imageBuffer->toDataURL(mimeType, quality, preserveResolution);
    completionHandler(WTFMove(urlString));
}

void RemoteRenderingBackend::getDataForImageBuffer(const String& mimeType, std::optional<double> quality, WebCore::RenderingResourceIdentifier renderingResourceIdentifier, CompletionHandler<void(Vector<uint8_t>&&)>&& completionHandler)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    getDataForImageBufferWithQualifiedIdentifier(mimeType, quality, { renderingResourceIdentifier, m_gpuConnectionToWebProcess->webProcessIdentifier() }, WTFMove(completionHandler));
}

void RemoteRenderingBackend::getDataForImageBufferWithQualifiedIdentifier(const String& mimeType, std::optional<double> quality, QualifiedRenderingResourceIdentifier renderingResourceIdentifier, CompletionHandler<void(Vector<uint8_t>&&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    Vector<uint8_t> data;
    if (auto imageBuffer = m_remoteResourceCache.cachedImageBuffer(renderingResourceIdentifier))
        data = imageBuffer->toData(mimeType, quality);
    completionHandler(WTFMove(data));
}

void RemoteRenderingBackend::getShareableBitmapForImageBuffer(WebCore::RenderingResourceIdentifier identifier, WebCore::PreserveResolution preserveResolution, CompletionHandler<void(ShareableBitmap::Handle&&)>&& completionHandler)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    getShareableBitmapForImageBufferWithQualifiedIdentifier({ identifier, m_gpuConnectionToWebProcess->webProcessIdentifier() }, preserveResolution, WTFMove(completionHandler));
}

void RemoteRenderingBackend::getShareableBitmapForImageBufferWithQualifiedIdentifier(QualifiedRenderingResourceIdentifier identifier, WebCore::PreserveResolution preserveResolution, CompletionHandler<void(ShareableBitmap::Handle&&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    ShareableBitmap::Handle handle;
    [&]() {
        auto imageBuffer = m_remoteResourceCache.cachedImageBuffer(identifier);
        if (!imageBuffer)
            return;
        auto backendSize = imageBuffer->backendSize();
        auto resultSize = preserveResolution == WebCore::PreserveResolution::Yes ? backendSize : imageBuffer->truncatedLogicalSize();
        auto bitmap = ShareableBitmap::createShareable(resultSize, { imageBuffer->colorSpace() });
        if (!bitmap)
            return;
        auto context = bitmap->createGraphicsContext();
        if (!context)
            return;
        context->drawImageBuffer(*imageBuffer, FloatRect { { }, resultSize }, FloatRect { { }, backendSize }, { WebCore::CompositeOperator::Copy });
        bitmap->createHandle(handle);
    }();
    completionHandler(WTFMove(handle));
}

void RemoteRenderingBackend::getFilteredImageForImageBuffer(RenderingResourceIdentifier identifier, IPC::FilterReference&& filterReference, CompletionHandler<void(ShareableBitmap::Handle&&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto filter = filterReference.takeFilter();

    ShareableBitmap::Handle handle;
    [&]() {
        auto imageBuffer = m_remoteResourceCache.cachedImageBuffer({ identifier, m_gpuConnectionToWebProcess->webProcessIdentifier() });
        if (!imageBuffer)
            return;
        auto image = imageBuffer->filteredImage(filter);
        if (!image)
            return;
        auto imageSize = image->size();
        auto bitmap = ShareableBitmap::createShareable(IntSize(imageSize), { imageBuffer->colorSpace() });
        if (!bitmap)
            return;
        auto context = bitmap->createGraphicsContext();
        if (!context)
            return;
        context->drawImage(*image, FloatPoint());
        bitmap->createHandle(handle);
    }();
    completionHandler(WTFMove(handle));
}

void RemoteRenderingBackend::cacheNativeImage(const ShareableBitmap::Handle& handle, RenderingResourceIdentifier nativeImageResourceIdentifier)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    cacheNativeImageWithQualifiedIdentifier(handle, { nativeImageResourceIdentifier, m_gpuConnectionToWebProcess->webProcessIdentifier() });
}

void RemoteRenderingBackend::cacheNativeImageWithQualifiedIdentifier(const ShareableBitmap::Handle& handle, QualifiedRenderingResourceIdentifier nativeImageResourceIdentifier)
{
    ASSERT(!RunLoop::isMain());

    auto bitmap = ShareableBitmap::create(handle);
    if (!bitmap)
        return;

    auto image = NativeImage::create(bitmap->createPlatformImage(), nativeImageResourceIdentifier.object());
    if (!image)
        return;

    m_remoteResourceCache.cacheNativeImage(*image, nativeImageResourceIdentifier);
}

void RemoteRenderingBackend::cacheFont(Ref<WebCore::Font>&& font)
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

void RemoteRenderingBackend::deleteAllFonts()
{
    ASSERT(!RunLoop::isMain());
    m_remoteResourceCache.deleteAllFonts();
}

void RemoteRenderingBackend::releaseRemoteResource(RenderingResourceIdentifier renderingResourceIdentifier)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    releaseRemoteResourceWithQualifiedIdentifier({ renderingResourceIdentifier, m_gpuConnectionToWebProcess->webProcessIdentifier() });
}

void RemoteRenderingBackend::releaseRemoteResourceWithQualifiedIdentifier(QualifiedRenderingResourceIdentifier renderingResourceIdentifier)
{
    ASSERT(!RunLoop::isMain());
    auto success = m_remoteResourceCache.releaseRemoteResource(renderingResourceIdentifier);
    MESSAGE_CHECK(success, "Resource is being released before being cached.");
    updateRenderingResourceRequest();

    Locker locker { m_remoteDisplayListsLock };
    m_remoteDisplayLists.remove(renderingResourceIdentifier);
}

static std::optional<ImageBufferBackendHandle> handleFromBuffer(WebCore::ImageBuffer& buffer)
{
    auto* backend = buffer.ensureBackendCreated();
    if (!backend)
        return std::nullopt;

    auto* sharing = backend->toBackendSharing();
    if (is<ImageBufferBackendHandleSharing>(sharing))
        return downcast<ImageBufferBackendHandleSharing>(*sharing).createBackendHandle();

    return std::nullopt;
}

void RemoteRenderingBackend::markSurfaceNonVolatile(WebCore::RenderingResourceIdentifier identifier, CompletionHandler<void(std::optional<ImageBufferBackendHandle> backendHandle, bool bufferWasEmpty)>&& completionHandler)
{
    auto imageBuffer = m_remoteResourceCache.cachedImageBuffer({ identifier, m_gpuConnectionToWebProcess->webProcessIdentifier() });
    if (!imageBuffer) {
        completionHandler({ }, false);
        return;
    }

    auto backendHandle = handleFromBuffer(*imageBuffer);
    auto previousState = imageBuffer->setNonVolatile();
    completionHandler(WTFMove(backendHandle), previousState == VolatilityState::Empty);
}

// This is the GPU Process version of RemoteLayerBackingStore::swapToValidFrontBuffer().
void RemoteRenderingBackend::swapToValidFrontBuffer(const BufferIdentifierSet& bufferSet, CompletionHandler<void(const BufferIdentifierSet& swappedBufferSet, std::optional<ImageBufferBackendHandle>&& frontBufferHandle, bool frontBufferWasEmpty)>&& completionHandler)
{
    auto fetchBuffer = [&](std::optional<RenderingResourceIdentifier> identifier) -> ImageBuffer* {
        return identifier ? m_remoteResourceCache.cachedImageBuffer({ *identifier, m_gpuConnectionToWebProcess->webProcessIdentifier() }) : nullptr;
    };

    auto frontBuffer = fetchBuffer(bufferSet.front);
    auto backBuffer = fetchBuffer(bufferSet.back);
    auto secondaryBackBuffer = fetchBuffer(bufferSet.secondaryBack);

    if (!backBuffer || backBuffer->isInUse()) {
        std::swap(backBuffer, secondaryBackBuffer);

        // When pulling the secondary back buffer out of hibernation (to become
        // the new front buffer), if it is somehow still in use (e.g. we got
        // three swaps ahead of the render server), just give up and discard it.
        if (backBuffer && backBuffer->isInUse())
            backBuffer = nullptr;
    }

    std::swap(frontBuffer, backBuffer);

    std::optional<ImageBufferBackendHandle> frontBufferHandle;
    bool frontBufferWasEmpty = false;
    if (frontBuffer) {
        auto previousState = frontBuffer->setNonVolatile();
        frontBufferWasEmpty = previousState == VolatilityState::Empty;
        frontBufferHandle = handleFromBuffer(*frontBuffer);
    }

    auto bufferIdentifer = [](ImageBuffer* buffer) -> std::optional<RenderingResourceIdentifier> {
        if (!buffer)
            return std::nullopt;
        return buffer->renderingResourceIdentifier();
    };

    completionHandler({ bufferIdentifer(frontBuffer), bufferIdentifer(backBuffer), bufferIdentifer(secondaryBackBuffer) }, WTFMove(frontBufferHandle), frontBufferWasEmpty);
}

void RemoteRenderingBackend::markSurfacesVolatile(const Vector<WebCore::RenderingResourceIdentifier>& identifiers, CompletionHandler<void(const Vector<WebCore::RenderingResourceIdentifier>& inUseBufferIdentifiers)>&& completionHandler)
{
    LOG_WITH_STREAM(RemoteRenderingBufferVolatility, stream << "GPU Process: RemoteRenderingBackend::markSurfacesVolatile " << identifiers);

    auto makeVolatile = [](ImageBuffer& imageBuffer) {
        imageBuffer.releaseGraphicsContext();
        return imageBuffer.setVolatile();
    };

    Vector<WebCore::RenderingResourceIdentifier> inUseBufferIdentifiers;
    for (auto identifier : identifiers) {
        auto imageBuffer = m_remoteResourceCache.cachedImageBuffer({ identifier, m_gpuConnectionToWebProcess->webProcessIdentifier() });
        if (imageBuffer) {
            if (!makeVolatile(*imageBuffer))
                inUseBufferIdentifiers.append(identifier);
        } else
            LOG_WITH_STREAM(RemoteRenderingBufferVolatility, stream << " failed to find ImageBuffer for identifier " << identifier);
    }

    LOG_WITH_STREAM(RemoteRenderingBufferVolatility, stream << "GPU Process: markSurfacesVolatile - in-use surfaces " << inUseBufferIdentifiers);

    completionHandler(inUseBufferIdentifiers);
}

void RemoteRenderingBackend::finalizeRenderingUpdate(RenderingUpdateID renderingUpdateID)
{
    send(Messages::RemoteRenderingBackendProxy::DidFinalizeRenderingUpdate(renderingUpdateID), m_renderingBackendIdentifier);
}

void RemoteRenderingBackend::updateRenderingResourceRequest()
{
    bool hasActiveDrawables = m_remoteResourceCache.hasActiveDrawables();
    bool hasActiveRequest = m_renderingResourcesRequest.isRequested();
    if (hasActiveDrawables && !hasActiveRequest)
        m_renderingResourcesRequest = ScopedRenderingResourcesRequest::acquire();
    else if (!hasActiveDrawables && hasActiveRequest)
        m_renderingResourcesRequest = { };
}

bool RemoteRenderingBackend::allowsExitUnderMemoryPressure() const
{
    ASSERT(isMainRunLoop());
    return !m_remoteResourceCache.hasActiveDrawables();
}

void RemoteRenderingBackend::performWithMediaPlayerOnMainThread(MediaPlayerIdentifier identifier, Function<void(MediaPlayer&)>&& callback)
{
    callOnMainRunLoopAndWait([&, gpuConnectionToWebProcess = m_gpuConnectionToWebProcess, identifier] {
        if (auto player = gpuConnectionToWebProcess->remoteMediaPlayerManagerProxy().mediaPlayer(identifier))
            callback(*player);
    });
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
