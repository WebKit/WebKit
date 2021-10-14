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
#include "RemoteRenderingBackendProxy.h"

#if ENABLE(GPU_PROCESS)

#include "GPUConnectionToWebProcess.h"
#include "Logging.h"
#include "PlatformRemoteImageBufferProxy.h"
#include "RemoteRenderingBackendMessages.h"
#include "RemoteRenderingBackendProxyMessages.h"
#include "SharedMemory.h"
#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <JavaScriptCore/TypedArrayInlines.h>

namespace WebKit {

using namespace WebCore;

std::unique_ptr<RemoteRenderingBackendProxy> RemoteRenderingBackendProxy::create(WebPage& webPage)
{
    return std::unique_ptr<RemoteRenderingBackendProxy>(new RemoteRenderingBackendProxy(webPage));
}

RemoteRenderingBackendProxy::RemoteRenderingBackendProxy(WebPage& webPage)
    : m_parameters {
        RenderingBackendIdentifier::generate(),
        webPage.webPageProxyIdentifier(),
        webPage.identifier()
    }
{
}

RemoteRenderingBackendProxy::~RemoteRenderingBackendProxy()
{
    if (!m_gpuProcessConnection)
        return;

    // Un-register itself as a MessageReceiver.
    m_gpuProcessConnection->messageReceiverMap().removeMessageReceiver(*this);

    // Release the RemoteRenderingBackend.
    send(Messages::GPUConnectionToWebProcess::ReleaseRenderingBackend(renderingBackendIdentifier()), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

GPUProcessConnection& RemoteRenderingBackendProxy::ensureGPUProcessConnection()
{
    if (!m_gpuProcessConnection) {
        auto& gpuProcessConnection = WebProcess::singleton().ensureGPUProcessConnection();
        gpuProcessConnection.addClient(*this);
        gpuProcessConnection.messageReceiverMap().addMessageReceiver(Messages::RemoteRenderingBackendProxy::messageReceiverName(), renderingBackendIdentifier().toUInt64(), *this);
        static constexpr auto connectionBufferSize = 1 << 21;
        m_streamConnection = makeUnique<IPC::StreamClientConnection>(gpuProcessConnection.connection(), connectionBufferSize);
        gpuProcessConnection.connection().send(Messages::GPUConnectionToWebProcess::CreateRenderingBackend(m_parameters, m_streamConnection->streamBuffer()), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
        m_gpuProcessConnection = gpuProcessConnection;
    }
    return *m_gpuProcessConnection;
}

void RemoteRenderingBackendProxy::gpuProcessConnectionDidClose(GPUProcessConnection& previousConnection)
{
    previousConnection.removeClient(*this);
    previousConnection.messageReceiverMap().removeMessageReceiver(*this);
    m_gpuProcessConnection = nullptr;

    m_remoteResourceCacheProxy.remoteResourceCacheWasDestroyed();

    if (m_destroyGetPixelBufferSharedMemoryTimer.isActive())
        m_destroyGetPixelBufferSharedMemoryTimer.stop();
    m_getPixelBufferSemaphore = std::nullopt;
    m_getPixelBufferSharedMemoryLength = 0;
    m_getPixelBufferSharedMemory = nullptr;
    
    m_renderingUpdateID = { };
    m_didRenderingUpdateID = { };
}

IPC::Connection* RemoteRenderingBackendProxy::messageSenderConnection() const
{
    return &const_cast<RemoteRenderingBackendProxy&>(*this).ensureGPUProcessConnection().connection();
}

uint64_t RemoteRenderingBackendProxy::messageSenderDestinationID() const
{
    return renderingBackendIdentifier().toUInt64();
}

RemoteRenderingBackendProxy::DidReceiveBackendCreationResult RemoteRenderingBackendProxy::waitForDidCreateImageBufferBackend()
{
    RefPtr connection = messageSenderConnection();
    if (!connection->waitForAndDispatchImmediately<Messages::RemoteRenderingBackendProxy::DidCreateImageBufferBackend>(renderingBackendIdentifier(), 1_s, IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives))
        return DidReceiveBackendCreationResult::TimeoutOrIPCFailure;
    return DidReceiveBackendCreationResult::ReceivedAnyResponse;
}

bool RemoteRenderingBackendProxy::waitForDidFlush()
{
    RefPtr connection = messageSenderConnection();
    return connection->waitForAndDispatchImmediately<Messages::RemoteRenderingBackendProxy::DidFlush>(renderingBackendIdentifier(), 1_s, IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives);
}

void RemoteRenderingBackendProxy::createRemoteImageBuffer(ImageBuffer& imageBuffer)
{
    sendToStream(Messages::RemoteRenderingBackend::CreateImageBuffer(imageBuffer.logicalSize(), imageBuffer.renderingMode(), imageBuffer.resolutionScale(), imageBuffer.colorSpace(), imageBuffer.pixelFormat(), imageBuffer.renderingResourceIdentifier()));
}

RefPtr<ImageBuffer> RemoteRenderingBackendProxy::createImageBuffer(const FloatSize& size, RenderingMode renderingMode, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat)
{
    RefPtr<ImageBuffer> imageBuffer;

    if (renderingMode == RenderingMode::Accelerated) {
        // Unless DOM rendering is always enabled when any GPU process rendering is enabled,
        // we need to create ImageBuffers for e.g. Canvas that are actually mapped into the
        // Web Content process, so they can be painted into the tiles.
        if (!WebProcess::singleton().shouldUseRemoteRenderingFor(RenderingPurpose::DOM))
            imageBuffer = AcceleratedRemoteImageBufferMappedProxy::create(size, resolutionScale, colorSpace, pixelFormat, *this);
        else
            imageBuffer = AcceleratedRemoteImageBufferProxy::create(size, resolutionScale, colorSpace, pixelFormat, *this);
    }

    if (!imageBuffer)
        imageBuffer = UnacceleratedRemoteImageBufferProxy::create(size, resolutionScale, colorSpace, pixelFormat, *this);

    if (imageBuffer) {
        createRemoteImageBuffer(*imageBuffer);
        return imageBuffer;
    }

    return nullptr;
}

SharedMemory* RemoteRenderingBackendProxy::sharedMemoryForGetPixelBuffer(size_t dataSize, IPC::Timeout timeout)
{
    bool needsSharedMemory = !m_getPixelBufferSharedMemory || dataSize > m_getPixelBufferSharedMemoryLength;
    bool needsSemaphore = !m_getPixelBufferSemaphore;

    if (needsSharedMemory)
        m_getPixelBufferSharedMemory = nullptr;

    SharedMemory::IPCHandle handle;
    IPC::Semaphore semaphore;

    if (needsSharedMemory && needsSemaphore)
        sendSyncToStream(Messages::RemoteRenderingBackend::UpdateSharedMemoryAndSemaphoreForGetPixelBuffer(dataSize), Messages::RemoteRenderingBackend::UpdateSharedMemoryAndSemaphoreForGetPixelBuffer::Reply(handle, semaphore), timeout);
    else if (needsSharedMemory)
        sendSyncToStream(Messages::RemoteRenderingBackend::UpdateSharedMemoryForGetPixelBuffer(dataSize), Messages::RemoteRenderingBackend::UpdateSharedMemoryForGetPixelBuffer::Reply(handle), timeout);
    else if (needsSemaphore)
        sendSyncToStream(Messages::RemoteRenderingBackend::SemaphoreForGetPixelBuffer(), Messages::RemoteRenderingBackend::SemaphoreForGetPixelBuffer::Reply(semaphore), timeout);

    if (!handle.handle.isNull()) {
        m_getPixelBufferSharedMemory = SharedMemory::map(handle.handle, SharedMemory::Protection::ReadOnly);
        m_getPixelBufferSharedMemoryLength = handle.dataSize;
    }
    if (needsSemaphore)
        m_getPixelBufferSemaphore = WTFMove(semaphore);

    if (m_destroyGetPixelBufferSharedMemoryTimer.isActive())
        m_destroyGetPixelBufferSharedMemoryTimer.stop();
    m_destroyGetPixelBufferSharedMemoryTimer.startOneShot(5_s);

    return m_getPixelBufferSharedMemory.get();
}

bool RemoteRenderingBackendProxy::waitForGetPixelBufferToComplete(IPC::Timeout timeout)
{
    ASSERT(m_getPixelBufferSemaphore);
    return m_getPixelBufferSemaphore->waitFor(timeout);
}

void RemoteRenderingBackendProxy::destroyGetPixelBufferSharedMemory()
{
    m_getPixelBufferSharedMemory = nullptr;
    sendToStream(Messages::RemoteRenderingBackend::DestroyGetPixelBufferSharedMemory());
}

String RemoteRenderingBackendProxy::getDataURLForImageBuffer(const String& mimeType, std::optional<double> quality, PreserveResolution preserveResolution, RenderingResourceIdentifier renderingResourceIdentifier)
{
    String urlString;
    sendSyncToStream(Messages::RemoteRenderingBackend::GetDataURLForImageBuffer(mimeType, quality, preserveResolution, renderingResourceIdentifier), Messages::RemoteRenderingBackend::GetDataURLForImageBuffer::Reply(urlString), 1_s);
    return urlString;
}

Vector<uint8_t> RemoteRenderingBackendProxy::getDataForImageBuffer(const String& mimeType, std::optional<double> quality, RenderingResourceIdentifier renderingResourceIdentifier)
{
    Vector<uint8_t> data;
    sendSyncToStream(Messages::RemoteRenderingBackend::GetDataForImageBuffer(mimeType, quality, renderingResourceIdentifier), Messages::RemoteRenderingBackend::GetDataForImageBuffer::Reply(data), 1_s);
    return data;
}

RefPtr<ShareableBitmap> RemoteRenderingBackendProxy::getShareableBitmap(RenderingResourceIdentifier imageBuffer, PreserveResolution preserveResolution)
{
    ShareableBitmap::Handle handle;
    auto sendResult = sendSyncToStream(Messages::RemoteRenderingBackend::GetShareableBitmapForImageBuffer(imageBuffer, preserveResolution), Messages::RemoteRenderingBackend::GetShareableBitmapForImageBuffer::Reply(handle), 1_s);
    if (handle.isNull())
        return { };
    ASSERT_UNUSED(sendResult, sendResult);
    return ShareableBitmap::create(handle);
}

void RemoteRenderingBackendProxy::cacheNativeImage(const ShareableBitmap::Handle& handle, RenderingResourceIdentifier renderingResourceIdentifier)
{
    sendToStream(Messages::RemoteRenderingBackend::CacheNativeImage(handle, renderingResourceIdentifier));
}

void RemoteRenderingBackendProxy::cacheFont(Ref<Font>&& font)
{
    sendToStream(Messages::RemoteRenderingBackend::CacheFont(WTFMove(font)));
}

void RemoteRenderingBackendProxy::deleteAllFonts()
{
    sendToStream(Messages::RemoteRenderingBackend::DeleteAllFonts());
}

void RemoteRenderingBackendProxy::releaseRemoteResource(RenderingResourceIdentifier renderingResourceIdentifier, uint64_t useCount)
{
    sendToStream(Messages::RemoteRenderingBackend::ReleaseRemoteResource(renderingResourceIdentifier, useCount));
}

void RemoteRenderingBackendProxy::finalizeRenderingUpdate()
{
    sendToStream(Messages::RemoteRenderingBackend::FinalizeRenderingUpdate(m_renderingUpdateID));
    m_remoteResourceCacheProxy.finalizeRenderingUpdate();
    m_renderingUpdateID.increment();
}

void RemoteRenderingBackendProxy::didCreateImageBufferBackend(ImageBufferBackendHandle handle, RenderingResourceIdentifier renderingResourceIdentifier)
{
    auto imageBuffer = m_remoteResourceCacheProxy.cachedImageBuffer(renderingResourceIdentifier);
    if (!imageBuffer)
        return;

    if (imageBuffer->renderingMode() == RenderingMode::Unaccelerated)
        imageBuffer->setBackend(UnacceleratedImageBufferShareableBackend::create(imageBuffer->parameters(), WTFMove(handle)));
    else if (imageBuffer->canMapBackingStore())
        imageBuffer->setBackend(AcceleratedImageBufferShareableMappedBackend::create(imageBuffer->parameters(), WTFMove(handle)));
    else
        imageBuffer->setBackend(AcceleratedImageBufferShareableBackend::create(imageBuffer->parameters(), WTFMove(handle)));
}

void RemoteRenderingBackendProxy::didFlush(GraphicsContextFlushIdentifier flushIdentifier, RenderingResourceIdentifier renderingResourceIdentifier)
{
    if (auto imageBuffer = m_remoteResourceCacheProxy.cachedImageBuffer(renderingResourceIdentifier))
        imageBuffer->didFlush(flushIdentifier);
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
    if (UNLIKELY(m_needsWakeUpSemaphoreForDisplayListStream))
        messageSenderConnection()->waitForAndDispatchImmediately<Messages::RemoteRenderingBackendProxy::DidCreateWakeUpSemaphoreForDisplayListStream>(renderingBackendIdentifier(), 3_s, IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives);
    return *m_streamConnection;
}

void RemoteRenderingBackendProxy::didCreateWakeUpSemaphoreForDisplayListStream(IPC::Semaphore&& semaphore)
{
    if (!m_streamConnection) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_streamConnection->setWakeUpSemaphore(WTFMove(semaphore));
    m_needsWakeUpSemaphoreForDisplayListStream = false;
}

void RemoteRenderingBackendProxy::recordNativeImageUse(NativeImage& image)
{
    m_remoteResourceCacheProxy.recordNativeImageUse(image);
}

void RemoteRenderingBackendProxy::recordFontUse(Font& font)
{
    m_remoteResourceCacheProxy.recordFontUse(font);
}

void RemoteRenderingBackendProxy::recordImageBufferUse(ImageBuffer& imageBuffer)
{
    m_remoteResourceCacheProxy.recordImageBufferUse(imageBuffer);
}

bool RemoteRenderingBackendProxy::isCached(const ImageBuffer& imageBuffer) const
{
    if (auto cachedImageBuffer = m_remoteResourceCacheProxy.cachedImageBuffer(imageBuffer.renderingResourceIdentifier())) {
        ASSERT(cachedImageBuffer == &imageBuffer);
        return true;
    }
    return false;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
