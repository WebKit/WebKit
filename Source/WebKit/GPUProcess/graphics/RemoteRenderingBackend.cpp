/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

#include "GPUConnectionToWebProcess.h"
#include "Logging.h"
#include "PlatformRemoteImageBuffer.h"
#include "QualifiedRenderingResourceIdentifier.h"
#include "RemoteMediaPlayerManagerProxy.h"
#include "RemoteMediaPlayerProxy.h"
#include "RemoteRenderingBackendCreationParameters.h"
#include "RemoteRenderingBackendMessages.h"
#include "RemoteRenderingBackendProxyMessages.h"
#include "WebCoreArgumentCoders.h"
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
#if HAVE(IOSURFACE_SET_OWNERSHIP_IDENTITY)
            // Mark the IOSurface as being owned by the WebProcess even though it was constructed by the GPUProcess so that Jetsam knows which process to kill.
            acceleratedImageBuffer->setProcessOwnership(m_gpuConnectionToWebProcess->webProcessIdentityToken());
#endif
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

std::optional<SharedMemory::IPCHandle> RemoteRenderingBackend::updateSharedMemoryForGetPixelBufferHelper(size_t byteCount)
{
    MESSAGE_CHECK_WITH_RETURN_VALUE(!m_getPixelBufferSharedMemory || byteCount > m_getPixelBufferSharedMemory->size(), std::nullopt, "The existing Shmem for getPixelBuffer() is already big enough to handle the request");

    if (byteCount > 64 * MB) {
        // Just a sanity check. A 4K image is 36MB.
        return std::nullopt;
    }

    destroyGetPixelBufferSharedMemory();
    m_getPixelBufferSharedMemory = SharedMemory::allocate(byteCount);
    SharedMemory::Handle handle;
    if (m_getPixelBufferSharedMemory)
        m_getPixelBufferSharedMemory->createHandle(handle, SharedMemory::Protection::ReadOnly);
    return SharedMemory::IPCHandle { WTFMove(handle), m_getPixelBufferSharedMemory ? m_getPixelBufferSharedMemory->size() : 0 };
}

void RemoteRenderingBackend::updateSharedMemoryForGetPixelBuffer(uint32_t byteCount, CompletionHandler<void(const SharedMemory::IPCHandle&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (auto handle = updateSharedMemoryForGetPixelBufferHelper(byteCount))
        completionHandler(WTFMove(handle.value()));
    else
        completionHandler({ });
}

void RemoteRenderingBackend::semaphoreForGetPixelBuffer(CompletionHandler<void(const IPC::Semaphore&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());
    completionHandler(m_getPixelBufferSemaphore);
}

void RemoteRenderingBackend::updateSharedMemoryAndSemaphoreForGetPixelBuffer(uint32_t byteCount, CompletionHandler<void(const SharedMemory::IPCHandle&, const IPC::Semaphore&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (auto handle = updateSharedMemoryForGetPixelBufferHelper(byteCount))
        completionHandler(WTFMove(handle.value()), m_getPixelBufferSemaphore);
    else
        completionHandler({ }, m_getPixelBufferSemaphore);
}

void RemoteRenderingBackend::destroyGetPixelBufferSharedMemory()
{
    m_getPixelBufferSharedMemory = nullptr;
}

void RemoteRenderingBackend::populateGetPixelBufferSharedMemory(std::optional<WebCore::PixelBuffer>&& pixelBuffer)
{
    MESSAGE_CHECK(m_getPixelBufferSharedMemory, "We can't run getPixelBuffer without a buffer to write into");

    if (pixelBuffer) {
        MESSAGE_CHECK(pixelBuffer->data().byteLength() <= m_getPixelBufferSharedMemory->size(), "Shmem for return of getPixelBuffer is too small");
        memcpy(m_getPixelBufferSharedMemory->data(), pixelBuffer->data().data(), pixelBuffer->data().byteLength());
    } else
        memset(m_getPixelBufferSharedMemory->data(), 0, m_getPixelBufferSharedMemory->size());

    m_getPixelBufferSemaphore.signal();
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
        auto image = imageBuffer->copyNativeImage(WebCore::BackingStoreCopy::DontCopyBackingStore);
        if (!image)
            return;
        auto backendSize = imageBuffer->backendSize();
        auto resultSize = preserveResolution == WebCore::PreserveResolution::Yes ? backendSize : imageBuffer->truncatedLogicalSize();
        auto bitmap = ShareableBitmap::createShareable(resultSize, { imageBuffer->colorSpace() });
        if (!bitmap)
            return;
        auto context = bitmap->createGraphicsContext();
        if (!context)
            return;
        context->drawNativeImage(*image, resultSize, FloatRect { { }, resultSize }, FloatRect { { }, backendSize }, { WebCore::CompositeOperator::Copy });
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

void RemoteRenderingBackend::releaseRemoteResource(RenderingResourceIdentifier renderingResourceIdentifier, uint64_t useCount)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    releaseRemoteResourceWithQualifiedIdentifier({ renderingResourceIdentifier, m_gpuConnectionToWebProcess->webProcessIdentifier() }, useCount);
}

void RemoteRenderingBackend::releaseRemoteResourceWithQualifiedIdentifier(QualifiedRenderingResourceIdentifier renderingResourceIdentifier, uint64_t useCount)
{
    ASSERT(!RunLoop::isMain());
    auto success = m_remoteResourceCache.releaseRemoteResource(renderingResourceIdentifier, useCount);
    MESSAGE_CHECK(success, "Resource is being released before being cached.");
    updateRenderingResourceRequest();

    Locker locker { m_remoteDisplayListsLock };
    m_remoteDisplayLists.remove(renderingResourceIdentifier);
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
