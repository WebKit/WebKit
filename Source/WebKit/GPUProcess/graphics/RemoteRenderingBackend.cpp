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
#include "RemoteRenderingBackend.h"

#if ENABLE(GPU_PROCESS)

#include "GPUConnectionToWebProcess.h"
#include "PlatformRemoteImageBuffer.h"
#include "RemoteRenderingBackendMessages.h"
#include "RemoteRenderingBackendProxyMessages.h"

namespace WebKit {
using namespace WebCore;

std::unique_ptr<RemoteRenderingBackend> RemoteRenderingBackend::create(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RenderingBackendIdentifier renderingBackendIdentifier)
{
    return std::unique_ptr<RemoteRenderingBackend>(new RemoteRenderingBackend(gpuConnectionToWebProcess, renderingBackendIdentifier));
}

RemoteRenderingBackend::RemoteRenderingBackend(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RenderingBackendIdentifier renderingBackendIdentifier)
    : m_gpuConnectionToWebProcess(makeWeakPtr(gpuConnectionToWebProcess))
    , m_renderingBackendIdentifier(renderingBackendIdentifier)
{
    if (auto* gpuConnectionToWebProcess = m_gpuConnectionToWebProcess.get())
        gpuConnectionToWebProcess->messageReceiverMap().addMessageReceiver(Messages::RemoteRenderingBackend::messageReceiverName(), renderingBackendIdentifier.toUInt64(), *this);
}

RemoteRenderingBackend::~RemoteRenderingBackend()
{
    if (auto* gpuConnectionToWebProcess = m_gpuConnectionToWebProcess.get())
        gpuConnectionToWebProcess->messageReceiverMap().removeMessageReceiver(Messages::RemoteRenderingBackend::messageReceiverName(), m_renderingBackendIdentifier.toUInt64());
}

GPUConnectionToWebProcess* RemoteRenderingBackend::gpuConnectionToWebProcess() const
{
    return m_gpuConnectionToWebProcess.get();
}

IPC::Connection* RemoteRenderingBackend::messageSenderConnection() const
{
    if (auto* gpuConnectionToWebProcess = m_gpuConnectionToWebProcess.get())
        return &gpuConnectionToWebProcess->connection();
    return nullptr;
}

uint64_t RemoteRenderingBackend::messageSenderDestinationID() const
{
    return m_renderingBackendIdentifier.toUInt64();
}

bool RemoteRenderingBackend::applyMediaItem(DisplayList::ItemHandle item, GraphicsContext& context)
{
    if (!item.is<DisplayList::PaintFrameForMedia>())
        return false;

    auto& mediaItem = item.get<DisplayList::PaintFrameForMedia>();
    auto process = gpuConnectionToWebProcess();
    if (!process)
        return false;

    auto playerProxy = process->remoteMediaPlayerManagerProxy().getProxy(mediaItem.identifier());
    if (!playerProxy)
        return false;

    auto player = playerProxy->mediaPlayer();
    if (!player)
        return false;

    context.paintFrameForMedia(*player, mediaItem.destination());
    return true;
}

void RemoteRenderingBackend::imageBufferBackendWasCreated(const FloatSize& logicalSize, const IntSize& backendSize, float resolutionScale, ColorSpace colorSpace, ImageBufferBackendHandle handle, RenderingResourceIdentifier renderingResourceIdentifier)
{
    send(Messages::RemoteRenderingBackendProxy::ImageBufferBackendWasCreated(logicalSize, backendSize, resolutionScale, colorSpace, WTFMove(handle), renderingResourceIdentifier), m_renderingBackendIdentifier);
}

void RemoteRenderingBackend::flushDisplayListWasCommitted(DisplayListFlushIdentifier flushIdentifier, RenderingResourceIdentifier renderingResourceIdentifier)
{
    send(Messages::RemoteRenderingBackendProxy::FlushDisplayListWasCommitted(flushIdentifier, renderingResourceIdentifier), m_renderingBackendIdentifier);
}

void RemoteRenderingBackend::createImageBuffer(const FloatSize& logicalSize, RenderingMode renderingMode, float resolutionScale, ColorSpace colorSpace, WebCore::RenderingResourceIdentifier renderingResourceIdentifier)
{
    ASSERT(renderingMode == RenderingMode::Accelerated || renderingMode == RenderingMode::Unaccelerated);

    RefPtr<ImageBuffer> imageBuffer;

    if (renderingMode == RenderingMode::Accelerated)
        imageBuffer = AcceleratedRemoteImageBuffer::create(logicalSize, resolutionScale, colorSpace, *this, renderingResourceIdentifier);

    if (!imageBuffer)
        imageBuffer = UnacceleratedRemoteImageBuffer::create(logicalSize, resolutionScale, colorSpace, *this, renderingResourceIdentifier);

    if (!imageBuffer) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_remoteResourceCache.cacheImageBuffer(makeRef(*imageBuffer));
}

void RemoteRenderingBackend::applyDisplayList(const SharedDisplayListHandle& handle, RenderingResourceIdentifier renderingResourceIdentifier, ShouldFlushContext flushContext)
{
    auto displayList = handle.createDisplayList([&] (WebCore::DisplayList::ItemBufferIdentifier identifier) -> uint8_t* {
        if (auto sharedMemory = m_sharedItemBuffers.take(identifier))
            return reinterpret_cast<uint8_t*>(sharedMemory->data());
        return nullptr;
    });

    if (!displayList) {
        // FIXME: Add a message check to terminate the web process.
        return;
    }

    auto imageBuffer = m_remoteResourceCache.cachedImageBuffer(renderingResourceIdentifier);
    if (!imageBuffer) {
        // FIXME: Add a message check to terminate the web process.
        return;
    }

    if (imageBuffer->isAccelerated())
        displayList->setItemBufferClient(static_cast<AcceleratedRemoteImageBuffer*>(imageBuffer));
    else
        displayList->setItemBufferClient(static_cast<UnacceleratedRemoteImageBuffer*>(imageBuffer));

    imageBuffer->flushDisplayList(*displayList);

    if (flushContext == ShouldFlushContext::Yes)
        imageBuffer->flushContext();
}

void RemoteRenderingBackend::flushDisplayList(const SharedDisplayListHandle& handle, RenderingResourceIdentifier renderingResourceIdentifier)
{
    applyDisplayList(handle, renderingResourceIdentifier, ShouldFlushContext::No);
}

void RemoteRenderingBackend::flushDisplayListAndCommit(const SharedDisplayListHandle& handle, DisplayListFlushIdentifier flushIdentifier, RenderingResourceIdentifier renderingResourceIdentifier)
{
    applyDisplayList(handle, renderingResourceIdentifier, ShouldFlushContext::Yes);
    flushDisplayListWasCommitted(flushIdentifier, renderingResourceIdentifier);
}

void RemoteRenderingBackend::getImageData(AlphaPremultiplication outputFormat, IntRect srcRect, RenderingResourceIdentifier renderingResourceIdentifier, CompletionHandler<void(IPC::ImageDataReference&&)>&& completionHandler)
{
    RefPtr<ImageData> imageData;
    if (auto imageBuffer = m_remoteResourceCache.cachedImageBuffer(renderingResourceIdentifier))
        imageData = imageBuffer->getImageData(outputFormat, srcRect);
    completionHandler(IPC::ImageDataReference(WTFMove(imageData)));
}

void RemoteRenderingBackend::releaseRemoteResource(RenderingResourceIdentifier renderingResourceIdentifier)
{
    m_remoteResourceCache.releaseRemoteResource(renderingResourceIdentifier);
}

void RemoteRenderingBackend::didCreateSharedItemData(DisplayList::ItemBufferIdentifier identifier, const SharedMemory::IPCHandle& handle)
{
    if (auto sharedMemory = SharedMemory::map(handle.handle, SharedMemory::Protection::ReadOnly))
        m_sharedItemBuffers.set(identifier, WTFMove(sharedMemory));
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
