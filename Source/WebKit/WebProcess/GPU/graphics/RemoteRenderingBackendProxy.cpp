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
#include "RemoteRenderingBackendCreationParameters.h"
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
    send(Messages::GPUConnectionToWebProcess::CreateRenderingBackend({
        m_renderingBackendIdentifier,
#if PLATFORM(COCOA)
        m_resumeDisplayListSemaphore.createSendRight(),
#endif
    }), 0);
}

void RemoteRenderingBackendProxy::reestablishGPUProcessConnection()
{
    connectToGPUProcess();

    for (auto& imageBuffer : m_remoteResourceCacheProxy.imageBuffers().values()) {
        if (!imageBuffer)
            continue;
        send(Messages::RemoteRenderingBackend::CreateImageBuffer(imageBuffer->logicalSize(), imageBuffer->renderingMode(), imageBuffer->resolutionScale(), imageBuffer->colorSpace(), imageBuffer->pixelFormat(), imageBuffer->renderingResourceIdentifier()), m_renderingBackendIdentifier);
        imageBuffer->clearBackend();
    }
}

void RemoteRenderingBackendProxy::gpuProcessConnectionDidClose(GPUProcessConnection& previousConnection)
{
    previousConnection.removeClient(*this);

    m_identifiersOfReusableHandles.clear();
    m_sharedDisplayListHandles.clear();
    m_currentDestinationImageBufferIdentifier = WTF::nullopt;
    m_deferredWakeupMessageArguments = WTF::nullopt;
    m_remainingItemsToAppendBeforeSendingWakeup = 0;

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

RemoteRenderingBackendProxy::DidReceiveBackendCreationResult RemoteRenderingBackendProxy::waitForDidCreateImageBufferBackend()
{
    Ref<IPC::Connection> connection = WebProcess::singleton().ensureGPUProcessConnection().connection();
    if (!connection->waitForAndDispatchImmediately<Messages::RemoteRenderingBackendProxy::DidCreateImageBufferBackend>(m_renderingBackendIdentifier, 1_s, IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives))
        return DidReceiveBackendCreationResult::TimeoutOrIPCFailure;
    return DidReceiveBackendCreationResult::ReceivedAnyResponse;
}

bool RemoteRenderingBackendProxy::waitForDidFlush()
{
    Ref<IPC::Connection> connection = WebProcess::singleton().ensureGPUProcessConnection().connection();
    return connection->waitForAndDispatchImmediately<Messages::RemoteRenderingBackendProxy::DidFlush>(m_renderingBackendIdentifier, 1_s, IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives);
}

RefPtr<ImageBuffer> RemoteRenderingBackendProxy::createImageBuffer(const FloatSize& size, RenderingMode renderingMode, float resolutionScale, ColorSpace colorSpace, PixelFormat pixelFormat)
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
        send(Messages::RemoteRenderingBackend::CreateImageBuffer(size, renderingMode, resolutionScale, colorSpace, pixelFormat, imageBuffer->renderingResourceIdentifier()), m_renderingBackendIdentifier);
        return imageBuffer;
    }

    return nullptr;
}

RefPtr<ImageData> RemoteRenderingBackendProxy::getImageData(AlphaPremultiplication outputFormat, const IntRect& srcRect, RenderingResourceIdentifier renderingResourceIdentifier)
{
    sendDeferredWakeupMessageIfNeeded();

    IPC::ImageDataReference imageDataReference;
    sendSync(Messages::RemoteRenderingBackend::GetImageData(outputFormat, srcRect, renderingResourceIdentifier), Messages::RemoteRenderingBackend::GetImageData::Reply(imageDataReference), m_renderingBackendIdentifier, 1_s);
    return imageDataReference.buffer();
}

String RemoteRenderingBackendProxy::getDataURLForImageBuffer(const String& mimeType, Optional<double> quality, PreserveResolution preserveResolution, RenderingResourceIdentifier renderingResourceIdentifier)
{
    sendDeferredWakeupMessageIfNeeded();

    String urlString;
    sendSync(Messages::RemoteRenderingBackend::GetDataURLForImageBuffer(mimeType, quality, preserveResolution, renderingResourceIdentifier), Messages::RemoteRenderingBackend::GetDataURLForImageBuffer::Reply(urlString), m_renderingBackendIdentifier, 1_s);
    return urlString;
}

Vector<uint8_t> RemoteRenderingBackendProxy::getDataForImageBuffer(const String& mimeType, Optional<double> quality, RenderingResourceIdentifier renderingResourceIdentifier)
{
    sendDeferredWakeupMessageIfNeeded();

    Vector<uint8_t> data;
    sendSync(Messages::RemoteRenderingBackend::GetDataForImageBuffer(mimeType, quality, renderingResourceIdentifier), Messages::RemoteRenderingBackend::GetDataForImageBuffer::Reply(data), m_renderingBackendIdentifier, 1_s);
    return data;
}

Vector<uint8_t> RemoteRenderingBackendProxy::getBGRADataForImageBuffer(RenderingResourceIdentifier renderingResourceIdentifier)
{
    sendDeferredWakeupMessageIfNeeded();

    Vector<uint8_t> data;
    sendSync(Messages::RemoteRenderingBackend::GetBGRADataForImageBuffer(renderingResourceIdentifier), Messages::RemoteRenderingBackend::GetBGRADataForImageBuffer::Reply(data), m_renderingBackendIdentifier, 1_s);
    return data;
}

void RemoteRenderingBackendProxy::cacheNativeImage(const ShareableBitmap::Handle& handle, RenderingResourceIdentifier renderingResourceIdentifier)
{
    send(Messages::RemoteRenderingBackend::CacheNativeImage(handle, renderingResourceIdentifier), m_renderingBackendIdentifier);
}

void RemoteRenderingBackendProxy::cacheFont(Ref<WebCore::Font>&& font)
{
    send(Messages::RemoteRenderingBackend::CacheFont(WTFMove(font)), m_renderingBackendIdentifier);
}

void RemoteRenderingBackendProxy::deleteAllFonts()
{
    send(Messages::RemoteRenderingBackend::DeleteAllFonts(), m_renderingBackendIdentifier);
}

void RemoteRenderingBackendProxy::releaseRemoteResource(RenderingResourceIdentifier renderingResourceIdentifier)
{
    if (renderingResourceIdentifier == m_currentDestinationImageBufferIdentifier)
        m_currentDestinationImageBufferIdentifier = WTF::nullopt;

    send(Messages::RemoteRenderingBackend::ReleaseRemoteResource(renderingResourceIdentifier), m_renderingBackendIdentifier);
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

void RemoteRenderingBackendProxy::didFlush(DisplayList::FlushIdentifier flushIdentifier, RenderingResourceIdentifier renderingResourceIdentifier)
{
    if (auto imageBuffer = m_remoteResourceCacheProxy.cachedImageBuffer(renderingResourceIdentifier))
        imageBuffer->didFlush(flushIdentifier);
}

void RemoteRenderingBackendProxy::willAppendItem(RenderingResourceIdentifier newDestinationIdentifier)
{
    if (m_currentDestinationImageBufferIdentifier == newDestinationIdentifier)
        return;

    if (auto previousDestinationBufferIdentifier = std::exchange(m_currentDestinationImageBufferIdentifier, newDestinationIdentifier)) {
        if (auto imageBuffer = m_remoteResourceCacheProxy.cachedImageBuffer(*previousDestinationBufferIdentifier))
            imageBuffer->changeDestinationImageBuffer(newDestinationIdentifier);
        else
            ASSERT_NOT_REACHED();
    }

    auto handle = mostRecentlyUsedDisplayListHandle();
    if (UNLIKELY(!handle))
        return;

    auto newDestination = m_remoteResourceCacheProxy.cachedImageBuffer(newDestinationIdentifier);
    if (UNLIKELY(!newDestination)) {
        ASSERT_NOT_REACHED();
        return;
    }

    handle->moveWritableOffsetToStartIfPossible();
    newDestination->prepareToAppendDisplayListItems(handle->createHandle());
}

void RemoteRenderingBackendProxy::sendWakeupMessage(const GPUProcessWakeupMessageArguments& arguments)
{
    send(Messages::RemoteRenderingBackend::WakeUpAndApplyDisplayList(arguments), m_renderingBackendIdentifier);
}

void RemoteRenderingBackendProxy::sendDeferredWakeupMessageIfNeeded()
{
    auto arguments = std::exchange(m_deferredWakeupMessageArguments, WTF::nullopt);
    if (!arguments)
        return;

    sendWakeupMessage(*arguments);
    m_remainingItemsToAppendBeforeSendingWakeup = 0;
}

void RemoteRenderingBackendProxy::didAppendData(const DisplayList::ItemBufferHandle& handle, size_t numberOfBytes, DisplayList::DidChangeItemBuffer didChangeItemBuffer, RenderingResourceIdentifier destinationImageBuffer)
{
    auto* sharedHandle = m_sharedDisplayListHandles.get(handle.identifier);
    if (UNLIKELY(!sharedHandle))
        RELEASE_ASSERT_NOT_REACHED();

    bool wasEmpty = sharedHandle->advance(numberOfBytes) == numberOfBytes;
    if (!wasEmpty || didChangeItemBuffer == DisplayList::DidChangeItemBuffer::Yes) {
        if (m_deferredWakeupMessageArguments) {
            if (sharedHandle->tryToResume({ m_deferredWakeupMessageArguments->offset, m_deferredWakeupMessageArguments->destinationImageBufferIdentifier.toUInt64() })) {
#if PLATFORM(COCOA)
                m_resumeDisplayListSemaphore.signal();
#endif
                m_deferredWakeupMessageArguments = WTF::nullopt;
                m_remainingItemsToAppendBeforeSendingWakeup = 0;
            } else if (!--m_remainingItemsToAppendBeforeSendingWakeup) {
                m_deferredWakeupMessageArguments->reason = GPUProcessWakeupReason::ItemCountHysteresisExceeded;
                sendWakeupMessage(*std::exchange(m_deferredWakeupMessageArguments, WTF::nullopt));
            }
        }
        return;
    }

    sendDeferredWakeupMessageIfNeeded();

    auto offsetToRead = sharedHandle->writableOffset() - numberOfBytes;
    if (sharedHandle->tryToResume({ offsetToRead, destinationImageBuffer.toUInt64() })) {
#if PLATFORM(COCOA)
        m_resumeDisplayListSemaphore.signal();
#endif
        return;
    }

    // Instead of sending the wakeup message immediately, wait for some additional data. This gives the
    // web process a "head start", decreasing the likelihood that the GPU process will encounter frequent
    // wakeups when processing a large amount of display list items.
    constexpr unsigned itemCountHysteresisBeforeSendingWakeup = 512;

    m_remainingItemsToAppendBeforeSendingWakeup = itemCountHysteresisBeforeSendingWakeup;
    m_deferredWakeupMessageArguments = {{ handle.identifier, offsetToRead, destinationImageBuffer }};
}

RefPtr<DisplayListWriterHandle> RemoteRenderingBackendProxy::mostRecentlyUsedDisplayListHandle()
{
    if (UNLIKELY(m_identifiersOfReusableHandles.isEmpty()))
        return nullptr;

    return m_sharedDisplayListHandles.get(m_identifiersOfReusableHandles.first());
}

RefPtr<DisplayListWriterHandle> RemoteRenderingBackendProxy::findReusableDisplayListHandle(size_t capacity)
{
    auto mostRecentlyUsedHandle = mostRecentlyUsedDisplayListHandle();
    if (UNLIKELY(!mostRecentlyUsedHandle))
        return nullptr;

    mostRecentlyUsedHandle->moveWritableOffsetToStartIfPossible();
    if (mostRecentlyUsedHandle->availableCapacity() >= capacity)
        return mostRecentlyUsedHandle;

    m_identifiersOfReusableHandles.append(m_identifiersOfReusableHandles.takeFirst());

    auto leastRecentlyUsedIdentifier = m_identifiersOfReusableHandles.first();
    if (leastRecentlyUsedIdentifier != mostRecentlyUsedHandle->identifier()) {
        auto handle = makeRefPtr(m_sharedDisplayListHandles.get(leastRecentlyUsedIdentifier));
        if (handle->moveWritableOffsetToStartIfPossible() && handle->availableCapacity() >= capacity)
            return handle;
    }

    return nullptr;
}

DisplayList::ItemBufferHandle RemoteRenderingBackendProxy::createItemBuffer(size_t capacity, RenderingResourceIdentifier destinationBufferIdentifier)
{
    if (auto handle = findReusableDisplayListHandle(capacity))
        return handle->createHandle();

    static constexpr size_t defaultSharedItemBufferSize = 1 << 16;
    static_assert(defaultSharedItemBufferSize > SharedDisplayListHandle::headerSize());

    auto sharedMemory = SharedMemory::allocate(std::max(defaultSharedItemBufferSize, capacity + SharedDisplayListHandle::headerSize()));
    if (!sharedMemory)
        return { };

    SharedMemory::Handle sharedMemoryHandle;
    sharedMemory->createHandle(sharedMemoryHandle, SharedMemory::Protection::ReadWrite);

    auto identifier = DisplayList::ItemBufferIdentifier::generate();
    send(Messages::RemoteRenderingBackend::DidCreateSharedDisplayListHandle(identifier, { WTFMove(sharedMemoryHandle), sharedMemory->size() }, destinationBufferIdentifier), m_renderingBackendIdentifier);

    auto newHandle = DisplayListWriterHandle::create(identifier, sharedMemory.releaseNonNull());
    auto displayListHandle = newHandle->createHandle();

    m_identifiersOfReusableHandles.prepend(identifier);
    m_sharedDisplayListHandles.set(identifier, WTFMove(newHandle));

    return displayListHandle;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
