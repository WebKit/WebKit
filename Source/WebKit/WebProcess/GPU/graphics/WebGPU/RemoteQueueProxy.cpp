/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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
#include "RemoteQueueProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteBufferProxy.h"
#include "RemoteQueueMessages.h"
#include "WebGPUConvertToBackingContext.h"
#include "WebProcess.h"
#include <WebCore/NativeImage.h>
#include <WebCore/WebCodecsVideoFrame.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit::WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteQueueProxy);

RemoteQueueProxy::RemoteQueueProxy(RemoteAdapterProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_parent(parent)
{
#if ENABLE(VIDEO) && PLATFORM(COCOA) && ENABLE(WEB_CODECS)
    RefPtr<RemoteVideoFrameObjectHeapProxy> videoFrameObjectHeapProxy;
    callOnMainRunLoopAndWait([&videoFrameObjectHeapProxy] {
        videoFrameObjectHeapProxy = protect(WebProcess::singleton().ensureGPUProcessConnection())->videoFrameObjectHeapProxy();
    });

    m_videoFrameObjectHeapProxy = videoFrameObjectHeapProxy;
#endif
}

RemoteQueueProxy::~RemoteQueueProxy()
{
    auto sendResult = send(Messages::RemoteQueue::Destruct());
    UNUSED_VARIABLE(sendResult);
}

void RemoteQueueProxy::submit(Vector<Ref<WebCore::WebGPU::CommandBuffer>>&& commandBuffers)
{
    auto convertedCommandBuffers = WTF::compactMap(commandBuffers, [&](auto& commandBuffer) -> std::optional<WebGPUIdentifier> {
        auto convertedCommandBuffer = m_convertToBackingContext->convertToBacking(commandBuffer);
        return convertedCommandBuffer;
    });

    auto sendResult = send(Messages::RemoteQueue::Submit(convertedCommandBuffers));
    UNUSED_VARIABLE(sendResult);
}

void RemoteQueueProxy::onSubmittedWorkDone(CompletionHandler<void()>&& callback)
{
    auto sendResult = sendWithAsyncReply(Messages::RemoteQueue::OnSubmittedWorkDone(), [callback = WTF::move(callback)]() mutable {
        callback();
    });
    UNUSED_PARAM(sendResult);
}

void RemoteQueueProxy::writeBuffer(
    const WebCore::WebGPU::Buffer& buffer,
    WebCore::WebGPU::Size64 bufferOffset,
    std::span<const uint8_t> source,
    WebCore::WebGPU::Size64 dataOffset,
    std::optional<WebCore::WebGPU::Size64> size)
{
    auto convertedBuffer = m_convertToBackingContext->convertToBacking(buffer);

    size_t actualSourceSize = static_cast<size_t>(size.value_or(source.size() - dataOffset));
    if (actualSourceSize > maxCrossProcessResourceCopySize) {
        auto handle = WebCore::SharedMemoryHandle::createCopy(source.subspan(dataOffset, actualSourceSize), WebCore::SharedMemoryProtection::ReadOnly);
        auto sendResult = sendWithAsyncReply(Messages::RemoteQueue::WriteBuffer(convertedBuffer, bufferOffset, WTF::move(handle)), [](auto) mutable {
        });
        UNUSED_VARIABLE(sendResult);
    } else {
        auto sendResult = send(Messages::RemoteQueue::WriteBufferWithCopy(convertedBuffer, bufferOffset, source.subspan(dataOffset, actualSourceSize)));
        UNUSED_VARIABLE(sendResult);
    }
}

void RemoteQueueProxy::writeTexture(
    const WebCore::WebGPU::ImageCopyTexture& destination,
    std::span<const uint8_t> source,
    const WebCore::WebGPU::ImageDataLayout& dataLayout,
    const WebCore::WebGPU::Extent3D& size)
{
    auto convertedDestination = m_convertToBackingContext->convertToBacking(destination);
    ASSERT(convertedDestination);
    auto convertedDataLayout = m_convertToBackingContext->convertToBacking(dataLayout);
    ASSERT(convertedDataLayout);
    auto convertedSize = m_convertToBackingContext->convertToBacking(size);
    ASSERT(convertedSize);
    if (!convertedDestination || !convertedDataLayout || !convertedSize)
        return;

    if (source.size() > maxCrossProcessResourceCopySize) {
        auto handle = WebCore::SharedMemoryHandle::createCopy(source, WebCore::SharedMemoryProtection::ReadOnly);
        auto sendResult = sendWithAsyncReply(Messages::RemoteQueue::WriteTexture(*convertedDestination, WTF::move(handle), *convertedDataLayout, *convertedSize), [](auto) mutable {
        });
        UNUSED_VARIABLE(sendResult);
    } else {
        auto sendResult = send(Messages::RemoteQueue::WriteTextureWithCopy(*convertedDestination, Vector(source), *convertedDataLayout, *convertedSize));
        UNUSED_VARIABLE(sendResult);
    }
}

void RemoteQueueProxy::writeBufferNoCopy(
    const WebCore::WebGPU::Buffer&,
    WebCore::WebGPU::Size64,
    std::span<uint8_t>,
    WebCore::WebGPU::Size64,
    std::optional<WebCore::WebGPU::Size64>)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void RemoteQueueProxy::writeTexture(
    const WebCore::WebGPU::ImageCopyTexture&,
    std::span<uint8_t>,
    const WebCore::WebGPU::ImageDataLayout&,
    const WebCore::WebGPU::Extent3D&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void RemoteQueueProxy::copyExternalImageToTexture(
    const WebCore::WebGPU::ImageCopyExternalImage& source,
    const WebCore::WebGPU::ImageCopyTextureTagged& destination,
    const WebCore::WebGPU::Extent3D& copySize)
{
#if PLATFORM(COCOA) && ENABLE(VIDEO)
    if (source.videoSource) {
        copyExternalImageFromVideoFrameToTexture(source, destination, copySize);
        return;
    }
#endif

    Ref convertToBackingContext = m_convertToBackingContext;

    // The GPU process resolves the source by identifier through its RemoteRenderingBackend, so make
    // sure every drawing command recorded against the source buffer has been submitted first.
    RefPtr sourceImageBuffer = source.imageBuffer;
    if (sourceImageBuffer)
        sourceImageBuffer->flushDrawingContext();

    auto convertedSource = convertToBackingContext->convertToBacking(source);
    ASSERT(convertedSource);
    auto convertedDestination = convertToBackingContext->convertToBacking(destination);
    ASSERT(convertedDestination);
    auto convertedCopySize = convertToBackingContext->convertToBacking(copySize);
    ASSERT(convertedCopySize);
    if (!convertedSource || !convertedDestination || !convertedCopySize)
        return;

    // Sent synchronously, because the source is identified rather than referenced: releasing it, or
    // drawing into it again, travels on the RemoteRenderingBackend's stream, which is not ordered
    // against this one. Blocking until the GPU process has resolved the identifier and encoded the
    // copy is what keeps `sourceImageBuffer` from being released, or overwritten, too early.
    auto sendResult = sendSync(Messages::RemoteQueue::CopyExternalImageToTexture(*convertedSource, *convertedDestination, *convertedCopySize));
    UNUSED_VARIABLE(sendResult);
}

#if PLATFORM(COCOA) && ENABLE(VIDEO)
void RemoteQueueProxy::copyExternalImageFromVideoFrameToTexture(
    const WebCore::WebGPU::ImageCopyExternalImage& source,
    const WebCore::WebGPU::ImageCopyTextureTagged& destination,
    const WebCore::WebGPU::Extent3D& copySize)
{
    Ref convertToBackingContext = m_convertToBackingContext;

    auto convertedSource = convertToBackingContext->convertToBackingVideoSource(source);
    ASSERT(convertedSource);
    auto convertedDestination = convertToBackingContext->convertToBacking(destination);
    ASSERT(convertedDestination);
    auto convertedCopySize = convertToBackingContext->convertToBacking(copySize);
    ASSERT(convertedCopySize);
    if (!convertedSource || !convertedDestination || !convertedCopySize)
        return;

    // A frame with no media player behind it - every WebCodecs frame, and a media element whose
    // player has not been given an identifier - has to be shipped across itself, the same way
    // RemoteDeviceProxy ships one for an external texture.
    if (!convertedSource->mediaIdentifier) {
        auto* videoFrame = std::get_if<RefPtr<WebCore::VideoFrame>>(&*source.videoSource);
        if (!videoFrame || !videoFrame->get())
            return;

        convertedSource->sharedFrame = m_sharedVideoFrameWriter.write(*videoFrame->get(), [this, protectedThis = protect(*this)](auto& semaphore) {
            auto sendResult = send(Messages::RemoteQueue::SetSharedVideoFrameSemaphore { semaphore });
            UNUSED_VARIABLE(sendResult);
        }, [this, protectedThis = protect(*this)](WebCore::SharedMemory::Handle&& handle) {
            auto sendResult = send(Messages::RemoteQueue::SetSharedVideoFrameMemory { WTF::move(handle) });
            UNUSED_VARIABLE(sendResult);
        });
        if (!convertedSource->sharedFrame)
            return;
    }

    // Sent asynchronously, unlike the ImageBuffer copy above: the frame either travels with the
    // message or is named by a media player the GPU process resolves for itself, so there is no
    // identifier on another stream whose lifetime this call has to hold open.
    auto sendResult = send(Messages::RemoteQueue::CopyExternalImageFromVideoFrameToTexture(WTF::move(*convertedSource), *convertedDestination, *convertedCopySize));
    UNUSED_VARIABLE(sendResult);
}
#endif

void RemoteQueueProxy::setLabelInternal(const String& label)
{
    auto sendResult = send(Messages::RemoteQueue::SetLabel(label));
    UNUSED_VARIABLE(sendResult);
}

RefPtr<WebCore::NativeImage> RemoteQueueProxy::getNativeImage(WebCore::VideoFrame& videoFrame)
{
    RefPtr<WebCore::NativeImage> nativeImage;
#if ENABLE(VIDEO) && PLATFORM(COCOA) && ENABLE(WEB_CODECS)
    callOnMainRunLoopAndWait([&nativeImage, videoFrame = protect(videoFrame), videoFrameHeap = protect(m_videoFrameObjectHeapProxy)] {
        nativeImage = videoFrameHeap->getNativeImage(videoFrame);
    });
#endif
    return nativeImage;
}


} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
