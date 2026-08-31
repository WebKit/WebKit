/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "RemoteQueue.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteBufferProxy.h"
#include "RemoteGPU.h"
#include "RemoteQueueMessages.h"
#include "StreamServerConnection.h"
#include "WebGPUObjectHeap.h"
#include <WebCore/SharedMemory.h>
#include <WebCore/WebGPUBuffer.h>
#include <WebCore/WebGPUQueue.h>
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(COCOA) && ENABLE(VIDEO)
#include "GPUConnectionToWebProcess.h"
#include <WebCore/MediaPlayer.h>
#include <WebCore/VideoFrame.h>
#endif

#if HAVE(WEBGPU_IMPLEMENTATION)
#include <WebGPU/WebGPU.h>
#include <WebGPU/WebGPUExt.h>
#endif

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteQueue);

// For transfers at or above WGPU_LARGE_BUFFER_SIZE the backend uses newBufferWithBytesNoCopy and aliases `data`'s mapping; keep it alive until the GPU has consumed the bytes. Smaller transfers are copied into a Metal buffer synchronously, so `data` can be released as soon as we return.
static void keepAliveUntilSubmittedWorkDone(WebCore::WebGPU::Queue& backing, RefPtr<WebCore::SharedMemory>&& data)
{
#if HAVE(WEBGPU_IMPLEMENTATION)
    if (!data || data->size() < WGPU_LARGE_BUFFER_SIZE)
        return;
    backing.onSubmittedWorkDone([data = WTF::move(data)]() mutable {
        data = nullptr;
    });
#else
    // Only the Metal backend aliases the caller's storage, and it is the only WebGPU implementation.
    UNUSED_PARAM(backing);
    UNUSED_PARAM(data);
#endif
}

RemoteQueue::RemoteQueue(WebCore::WebGPU::Queue& queue, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, RemoteGPU& gpu, WebGPUIdentifier identifier)
    : m_backing(queue)
    , m_objectHeap(objectHeap)
    , m_streamConnection(WTF::move(streamConnection))
    , m_gpu(gpu)
    , m_identifier(identifier)
{
    protect(m_streamConnection)->startReceivingMessages(*this, Messages::RemoteQueue::messageReceiverName(), m_identifier.toUInt64());
}

RemoteQueue::~RemoteQueue() = default;

void RemoteQueue::destruct()
{
    protect(m_objectHeap)->removeObject(m_identifier);
}

void RemoteQueue::stopListeningForIPC()
{
    protect(m_streamConnection)->stopReceivingMessages(Messages::RemoteQueue::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteQueue::submit(Vector<WebGPUIdentifier>&& commandBuffers)
{
    Vector<Ref<WebCore::WebGPU::CommandBuffer>> convertedCommandBuffers;
    convertedCommandBuffers.reserveInitialCapacity(commandBuffers.size());
    for (WebGPUIdentifier identifier : commandBuffers) {
        auto convertedCommandBuffer = protect(m_objectHeap)->convertCommandBufferFromBacking(identifier);
        ASSERT(convertedCommandBuffer);
        if (!convertedCommandBuffer)
            return;
        convertedCommandBuffers.append(protect(*convertedCommandBuffer));
    }
    protect(m_backing)->submit(WTF::move(convertedCommandBuffers));
}

void RemoteQueue::onSubmittedWorkDone(CompletionHandler<void()>&& callback)
{
    protect(m_backing)->onSubmittedWorkDone([callback = WTF::move(callback)] () mutable {
        callback();
    });
}

void RemoteQueue::writeBuffer(
    WebGPUIdentifier buffer,
    WebCore::WebGPU::Size64 bufferOffset,
    std::optional<WebCore::SharedMemoryHandle>&& dataHandle,
    CompletionHandler<void(bool)>&& completionHandler)
{
    auto data = dataHandle ? WebCore::SharedMemory::map(WTF::move(*dataHandle), WebCore::SharedMemory::Protection::ReadOnly) : nullptr;
    auto convertedBuffer = protect(m_objectHeap)->convertBufferFromBacking(buffer);
    ASSERT(convertedBuffer);
    if (!convertedBuffer || !data || data->size() <= WebGPU::maxCrossProcessResourceCopySize) {
        completionHandler(false);
        return;
    }

    Ref backing = protect(m_backing);
    backing->writeBufferNoCopy(protect(*convertedBuffer), bufferOffset, data->mutableSpan(), 0, std::nullopt);
    keepAliveUntilSubmittedWorkDone(backing, WTF::move(data));
    completionHandler(true);
}

void RemoteQueue::writeBufferWithCopy(
    WebGPUIdentifier buffer,
    WebCore::WebGPU::Size64 bufferOffset,
    Vector<uint8_t>&& data)
{
    Ref objectHeap = m_objectHeap.get();
    auto convertedBuffer = objectHeap->convertBufferFromBacking(buffer);
    ASSERT(convertedBuffer);
    if (!convertedBuffer)
        return;

    protect(m_backing)->writeBufferNoCopy(protect(*convertedBuffer), bufferOffset, data.mutableSpan(), 0, std::nullopt);
}

void RemoteQueue::writeTexture(
    const WebGPU::ImageCopyTexture& destination,
    std::optional<WebCore::SharedMemoryHandle>&& dataHandle,
    const WebGPU::ImageDataLayout& dataLayout,
    const WebGPU::Extent3D& size,
    CompletionHandler<void(bool)>&& completionHandler)
{
    auto data = dataHandle ? WebCore::SharedMemory::map(WTF::move(*dataHandle), WebCore::SharedMemory::Protection::ReadOnly) : nullptr;
    Ref objectHeap = m_objectHeap.get();
    auto convertedDestination = objectHeap->convertFromBacking(destination);
    ASSERT(convertedDestination);
    auto convertedDataLayout = objectHeap->convertFromBacking(dataLayout);
    ASSERT(convertedDataLayout);
    auto convertedSize = objectHeap->convertFromBacking(size);
    ASSERT(convertedSize);
    if (!convertedDestination || !convertedDataLayout || !convertedSize || !data || data->size() <= WebGPU::maxCrossProcessResourceCopySize) {
        completionHandler(false);
        return;
    }

    Ref backing = protect(m_backing);
    backing->writeTexture(*convertedDestination, data->mutableSpan(), *convertedDataLayout, *convertedSize);
    keepAliveUntilSubmittedWorkDone(backing, WTF::move(data));
    completionHandler(true);
}

void RemoteQueue::writeTextureWithCopy(
    const WebGPU::ImageCopyTexture& destination,
    Vector<uint8_t>&& data,
    const WebGPU::ImageDataLayout& dataLayout,
    const WebGPU::Extent3D& size)
{
    Ref objectHeap = m_objectHeap.get();
    auto convertedDestination = objectHeap->convertFromBacking(destination);
    ASSERT(convertedDestination);
    auto convertedDataLayout = objectHeap->convertFromBacking(dataLayout);
    ASSERT(convertedDestination);
    auto convertedSize = objectHeap->convertFromBacking(size);
    ASSERT(convertedSize);
    if (!convertedDestination || !convertedDestination || !convertedSize)
        return;

    protect(m_backing)->writeTexture(*convertedDestination, data.mutableSpan(), *convertedDataLayout, *convertedSize);
}

void RemoteQueue::copyExternalImageToTexture(
    const WebGPU::ImageCopyExternalImage& source,
    const WebGPU::ImageCopyTextureTagged& destination,
    const WebGPU::Extent3D& copySize,
    CompletionHandler<void()>&& completionHandler)
{
    // This message is synchronous, so that the source ImageBuffer cannot be released before it is
    // resolved below, and so that drawing recorded against the source after this call cannot be
    // applied before the copy is encoded. Neither is otherwise ordered: the source travels by
    // identifier and lives on the RemoteRenderingBackend's stream, which has no ordering
    // relationship with this one.
    CompletionHandlerCallingScope callCompletionHandler(WTF::move(completionHandler));

    Ref objectHeap = m_objectHeap.get();
    auto convertedSource = objectHeap->convertFromBacking(source);
    ASSERT(convertedSource);
    auto convertedDestination = objectHeap->convertFromBacking(destination);
    ASSERT(convertedDestination);
    auto convertedCopySize = objectHeap->convertFromBacking(copySize);
    ASSERT(convertedCopySize);
    if (!convertedSource || !convertedDestination || !convertedCopySize)
        return;

    // ConvertFromBackingContext cannot resolve the source, because only RemoteGPU can reach the
    // RemoteRenderingBackend which owns the ImageBuffer.
    if (source.imageBufferIdentifier) {
        RefPtr sourceImageBuffer = protect(m_gpu)->imageBuffer(*source.imageBufferIdentifier);
        if (!sourceImageBuffer)
            return;
        convertedSource->imageBuffer = WTF::move(sourceImageBuffer);
    }

    protect(m_backing)->copyExternalImageToTexture(*convertedSource, *convertedDestination, *convertedCopySize);
}

#if PLATFORM(COCOA) && ENABLE(VIDEO)
void RemoteQueue::setSharedVideoFrameSemaphore(IPC::Semaphore&& semaphore)
{
    m_sharedVideoFrameReader.setSemaphore(WTF::move(semaphore));
}

void RemoteQueue::setSharedVideoFrameMemory(WebCore::SharedMemoryHandle&& handle)
{
    m_sharedVideoFrameReader.setSharedMemory(WTF::move(handle));
}

void RemoteQueue::copyExternalImageFromVideoFrameToTexture(
    WebGPU::ImageCopyExternalImageVideoSource&& source,
    const WebGPU::ImageCopyTextureTagged& destination,
    const WebGPU::Extent3D& copySize)
{
    // Resolved the same way RemoteDevice resolves an external texture's frame: a shared frame is read
    // out of the shared video frame memory, and a media player identifier is asked for the frame it is
    // showing right now.
    RetainPtr<CVPixelBufferRef> pixelBuffer;
    // A frame's pixels are stored as decoded, so the transform that presents them travels separately.
    // A media player has already applied its asset's transform to the buffer it hands over, but a
    // WebCodecs frame carries one of its own.
    auto rotation = WebCore::VideoFrameRotation::None;
    bool isMirrored = false;
    auto takeFrame = [&](WebCore::VideoFrame& videoFrame) {
        pixelBuffer = videoFrame.pixelBuffer();
        rotation = videoFrame.rotation();
        isMirrored = videoFrame.isMirrored();
    };
    if (source.sharedFrame) {
        if (auto videoFrame = m_sharedVideoFrameReader.read(WTF::move(*source.sharedFrame)))
            takeFrame(*videoFrame);
    } else if (source.mediaIdentifier) {
        if (RefPtr connection = protect(m_gpu)->gpuConnectionToWebProcess()) {
            connection->performWithMediaPlayerOnMainThread(*source.mediaIdentifier, [&](auto& player) mutable {
                if (auto videoFrame = player.videoFrameForCurrentTime())
                    takeFrame(*videoFrame);
            });
        }
    }

    // A frame which could not be resolved leaves the destination texture alone, exactly as a video
    // with nothing decoded yet did on the readback path.
    if (!pixelBuffer)
        return;

    Ref objectHeap = m_objectHeap.get();
    auto convertedSource = objectHeap->convertFromBacking(source, WTF::move(pixelBuffer), rotation, isMirrored);
    ASSERT(convertedSource);
    auto convertedDestination = objectHeap->convertFromBacking(destination);
    ASSERT(convertedDestination);
    auto convertedCopySize = objectHeap->convertFromBacking(copySize);
    ASSERT(convertedCopySize);
    if (!convertedSource || !convertedDestination || !convertedCopySize)
        return;

    protect(m_backing)->copyExternalImageToTexture(*convertedSource, *convertedDestination, *convertedCopySize);
}
#endif

void RemoteQueue::setLabel(String&& label)
{
    protect(m_backing)->setLabel(WTF::move(label));
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
