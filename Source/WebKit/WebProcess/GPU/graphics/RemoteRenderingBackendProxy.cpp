/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "DisplayListWriterHandle.h"
#include "GPUConnectionToWebProcess.h"
#include "ImageDataReference.h"
#include "PlatformRemoteImageBufferProxy.h"
#include "RemoteRenderingBackendMessages.h"
#include "RemoteRenderingBackendProxyMessages.h"
#include "SharedMemory.h"
#include "WebProcess.h"

namespace WebKit {

using namespace WebCore;

std::unique_ptr<RemoteRenderingBackendProxy> RemoteRenderingBackendProxy::create()
{
    return std::unique_ptr<RemoteRenderingBackendProxy>(new RemoteRenderingBackendProxy());
}

RemoteRenderingBackendProxy::RemoteRenderingBackendProxy()
{
    connectToGPUProcess();
}

RemoteRenderingBackendProxy::~RemoteRenderingBackendProxy()
{
    // Un-register itself as a MessageReceiver.
    IPC::MessageReceiverMap& messageReceiverMap = WebProcess::singleton().ensureGPUProcessConnection().messageReceiverMap();
    messageReceiverMap.removeMessageReceiver(*this);

    // Release the RemoteRenderingBackend.
    send(Messages::GPUConnectionToWebProcess::ReleaseRenderingBackend(m_renderingBackendIdentifier), 0);
}

void RemoteRenderingBackendProxy::connectToGPUProcess()
{
    auto& connection = WebProcess::singleton().ensureGPUProcessConnection();
    connection.addClient(*this);
    connection.messageReceiverMap().addMessageReceiver(Messages::RemoteRenderingBackendProxy::messageReceiverName(), m_renderingBackendIdentifier.toUInt64(), *this);

    send(Messages::GPUConnectionToWebProcess::CreateRenderingBackend(m_renderingBackendIdentifier), 0);
}

template<typename T>
static void recreateImageBuffer(RemoteRenderingBackendProxy& proxy, T& imageBuffer, RenderingResourceIdentifier resourceIdentifier, RenderingBackendIdentifier renderingBackendIdentifier)
{
    imageBuffer.clearBackend();
    proxy.send(Messages::RemoteRenderingBackend::CreateImageBuffer(imageBuffer.size(), imageBuffer.renderingMode(), imageBuffer.resolutionScale(), imageBuffer.colorSpace(), imageBuffer.pixelFormat(), resourceIdentifier), renderingBackendIdentifier);
}

void RemoteRenderingBackendProxy::reestablishGPUProcessConnection()
{
    connectToGPUProcess();

    for (auto& pair : m_remoteResourceCacheProxy.imageBuffers()) {
        if (auto& baseImageBuffer = pair.value) {
            if (is<AcceleratedRemoteImageBufferProxy>(*baseImageBuffer))
                recreateImageBuffer(*this, downcast<AcceleratedRemoteImageBufferProxy>(*baseImageBuffer), pair.key, m_renderingBackendIdentifier);
            else
                recreateImageBuffer(*this, downcast<UnacceleratedRemoteImageBufferProxy>(*baseImageBuffer), pair.key, m_renderingBackendIdentifier);
        }
    }
}

void RemoteRenderingBackendProxy::gpuProcessConnectionDidClose(GPUProcessConnection& previousConnection)
{
    previousConnection.removeClient(*this);

    m_identifiersOfReusableHandles.clear();
    m_identifiersOfHandlesAvailableForWriting.clear();
    m_sharedDisplayListHandles.clear();

    reestablishGPUProcessConnection();
}

IPC::Connection* RemoteRenderingBackendProxy::messageSenderConnection() const
{
    return &WebProcess::singleton().ensureGPUProcessConnection().connection();
}

uint64_t RemoteRenderingBackendProxy::messageSenderDestinationID() const
{
    return m_renderingBackendIdentifier.toUInt64();
}

bool RemoteRenderingBackendProxy::waitForImageBufferBackendWasCreated()
{
    Ref<IPC::Connection> connection = WebProcess::singleton().ensureGPUProcessConnection().connection();
    return connection->waitForAndDispatchImmediately<Messages::RemoteRenderingBackendProxy::ImageBufferBackendWasCreated>(m_renderingBackendIdentifier, 1_s, IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives);
}

bool RemoteRenderingBackendProxy::waitForFlushDisplayListWasCommitted()
{
    Ref<IPC::Connection> connection = WebProcess::singleton().ensureGPUProcessConnection().connection();
    return connection->waitForAndDispatchImmediately<Messages::RemoteRenderingBackendProxy::FlushDisplayListWasCommitted>(m_renderingBackendIdentifier, 1_s, IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives);
}

RefPtr<ImageBuffer> RemoteRenderingBackendProxy::createImageBuffer(const FloatSize& size, RenderingMode renderingMode, float resolutionScale, ColorSpace colorSpace, PixelFormat pixelFormat)
{
    RefPtr<ImageBuffer> imageBuffer;

    if (renderingMode == RenderingMode::Accelerated)
        imageBuffer = AcceleratedRemoteImageBufferProxy::create(size, renderingMode, resolutionScale, colorSpace, pixelFormat, *this);

    if (!imageBuffer)
        imageBuffer = UnacceleratedRemoteImageBufferProxy::create(size, renderingMode, resolutionScale, colorSpace, pixelFormat, *this);

    if (imageBuffer) {
        send(Messages::RemoteRenderingBackend::CreateImageBuffer(size, renderingMode, resolutionScale, colorSpace, pixelFormat, imageBuffer->renderingResourceIdentifier()), m_renderingBackendIdentifier);
        return imageBuffer;
    }

    return nullptr;
}

RefPtr<ImageData> RemoteRenderingBackendProxy::getImageData(AlphaPremultiplication outputFormat, const IntRect& srcRect, RenderingResourceIdentifier renderingResourceIdentifier)
{
    IPC::ImageDataReference imageDataReference;
    sendSync(Messages::RemoteRenderingBackend::GetImageData(outputFormat, srcRect, renderingResourceIdentifier), Messages::RemoteRenderingBackend::GetImageData::Reply(imageDataReference), m_renderingBackendIdentifier, 1_s);
    return imageDataReference.buffer();
}

void RemoteRenderingBackendProxy::submitDisplayList(const DisplayList::DisplayList& displayList, RenderingResourceIdentifier destinationBufferIdentifier)
{
    Optional<std::pair<DisplayList::ItemBufferIdentifier, size_t>> identifierAndOffsetForWakeUpMessage;
    bool isFirstHandle = true;

    displayList.forEachItemBuffer([&] (auto& handle) {
        m_identifiersOfHandlesAvailableForWriting.add(handle.identifier);

        auto* sharedHandle = m_sharedDisplayListHandles.get(handle.identifier);
        RELEASE_ASSERT_WITH_MESSAGE(sharedHandle, "%s failed to find shared display list", __PRETTY_FUNCTION__);

        bool unreadCountWasEmpty = sharedHandle->advance(handle.capacity) == handle.capacity;
        if (isFirstHandle && unreadCountWasEmpty)
            identifierAndOffsetForWakeUpMessage = {{ handle.identifier, handle.data - sharedHandle->data() }};

        isFirstHandle = false;
    });

    if (identifierAndOffsetForWakeUpMessage) {
        auto [identifier, offset] = *identifierAndOffsetForWakeUpMessage;
        send(Messages::RemoteRenderingBackend::WakeUpAndApplyDisplayList(identifier, offset, destinationBufferIdentifier), m_renderingBackendIdentifier);
    }
}

void RemoteRenderingBackendProxy::cacheNativeImage(NativeImage& image)
{
    send(Messages::RemoteRenderingBackend::CacheNativeImage(makeRef(image)), m_renderingBackendIdentifier);
}

void RemoteRenderingBackendProxy::releaseRemoteResource(RenderingResourceIdentifier renderingResourceIdentifier)
{
    send(Messages::RemoteRenderingBackend::ReleaseRemoteResource(renderingResourceIdentifier), m_renderingBackendIdentifier);
}

void RemoteRenderingBackendProxy::imageBufferBackendWasCreated(const FloatSize& logicalSize, const IntSize& backendSize, float resolutionScale, ColorSpace colorSpace, PixelFormat pixelFormat, ImageBufferBackendHandle handle, RenderingResourceIdentifier renderingResourceIdentifier)
{
    auto imageBuffer = m_remoteResourceCacheProxy.cachedImageBuffer(renderingResourceIdentifier);
    if (!imageBuffer)
        return;
    
    if (imageBuffer->isAccelerated())
        downcast<AcceleratedRemoteImageBufferProxy>(*imageBuffer).createBackend(logicalSize, backendSize, resolutionScale, colorSpace, pixelFormat, WTFMove(handle));
    else
        downcast<UnacceleratedRemoteImageBufferProxy>(*imageBuffer).createBackend(logicalSize, backendSize, resolutionScale, colorSpace, pixelFormat, WTFMove(handle));
}

void RemoteRenderingBackendProxy::flushDisplayListWasCommitted(DisplayList::FlushIdentifier flushIdentifier, RenderingResourceIdentifier renderingResourceIdentifier)
{
    auto imageBuffer = m_remoteResourceCacheProxy.cachedImageBuffer(renderingResourceIdentifier);
    if (!imageBuffer)
        return;

    if (imageBuffer->isAccelerated())
        downcast<AcceleratedRemoteImageBufferProxy>(*imageBuffer).commitFlushDisplayList(flushIdentifier);
    else
        downcast<UnacceleratedRemoteImageBufferProxy>(*imageBuffer).commitFlushDisplayList(flushIdentifier);
}

void RemoteRenderingBackendProxy::updateReusableHandles()
{
    for (auto identifier : m_identifiersOfHandlesAvailableForWriting) {
        auto* handle = m_sharedDisplayListHandles.get(identifier);
        if (!handle->resetWritableOffsetIfPossible())
            continue;

        if (m_identifiersOfReusableHandles.contains(identifier))
            continue;

        m_identifiersOfReusableHandles.append(identifier);
    }
}

DisplayList::ItemBufferHandle RemoteRenderingBackendProxy::createItemBuffer(size_t capacity, RenderingResourceIdentifier destinationBufferIdentifier)
{
    updateReusableHandles();

    while (!m_identifiersOfReusableHandles.isEmpty()) {
        auto identifier = m_identifiersOfReusableHandles.first();
        auto* reusableHandle = m_sharedDisplayListHandles.get(identifier);
        RELEASE_ASSERT_WITH_MESSAGE(reusableHandle, "%s failed to find shared display list", __PRETTY_FUNCTION__);

        if (m_identifiersOfHandlesAvailableForWriting.contains(identifier) && reusableHandle->availableCapacity() >= capacity) {
            m_identifiersOfHandlesAvailableForWriting.remove(identifier);
            return reusableHandle->createHandle();
        }

        m_identifiersOfReusableHandles.removeFirst();
    }

    static constexpr size_t defaultSharedItemBufferSize = 1 << 16;

    auto sharedMemory = SharedMemory::allocate(std::max(defaultSharedItemBufferSize, capacity + SharedDisplayListHandle::reservedCapacityAtStart));
    if (!sharedMemory)
        return { };

    SharedMemory::Handle sharedMemoryHandle;
    sharedMemory->createHandle(sharedMemoryHandle, SharedMemory::Protection::ReadWrite);

    auto identifier = DisplayList::ItemBufferIdentifier::generate();
    send(Messages::RemoteRenderingBackend::DidCreateSharedDisplayListHandle(identifier, { WTFMove(sharedMemoryHandle), sharedMemory->size() }, destinationBufferIdentifier), m_renderingBackendIdentifier);

    auto newHandle = DisplayListWriterHandle::create(identifier, sharedMemory.releaseNonNull());
    auto displayListHandle = newHandle->createHandle();

    m_identifiersOfReusableHandles.append(identifier);
    m_sharedDisplayListHandles.set(identifier, WTFMove(newHandle));

    return displayListHandle;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
