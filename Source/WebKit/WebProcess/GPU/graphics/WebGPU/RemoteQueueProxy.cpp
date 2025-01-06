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
#include "RemoteQueueProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteQueueMessages.h"
#include "WebGPUConvertToBackingContext.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit::WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteQueueProxy);

RemoteQueueProxy::RemoteQueueProxy(RemoteAdapterProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_parent(parent)
{
}

RemoteQueueProxy::~RemoteQueueProxy()
{
    auto sendResult = send(Messages::RemoteQueue::Destruct());
    UNUSED_VARIABLE(sendResult);
}

void RemoteQueueProxy::submit(Vector<Ref<WebCore::WebGPU::CommandBuffer>>&& commandBuffers)
{
    auto convertedCommandBuffers = WTF::compactMap(commandBuffers, [&](auto& commandBuffer) -> std::optional<WebGPUIdentifier> {
        auto convertedCommandBuffer = protectedConvertToBackingContext()->convertToBacking(commandBuffer);
        return convertedCommandBuffer;
    });

    auto sendResult = send(Messages::RemoteQueue::Submit(convertedCommandBuffers));
    UNUSED_VARIABLE(sendResult);
}

void RemoteQueueProxy::onSubmittedWorkDone(CompletionHandler<void()>&& callback)
{
    auto sendResult = sendWithAsyncReply(Messages::RemoteQueue::OnSubmittedWorkDone(), [callback = WTFMove(callback)]() mutable {
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
    auto convertedBuffer = protectedConvertToBackingContext()->convertToBacking(buffer);

    auto sharedMemory = WebCore::SharedMemory::copySpan(source.subspan(dataOffset, static_cast<size_t>(size.value_or(source.size() - dataOffset))));
    std::optional<WebCore::SharedMemoryHandle> handle;
    if (sharedMemory)
        handle = sharedMemory->createHandle(WebCore::SharedMemory::Protection::ReadOnly);
    auto sendResult = sendWithAsyncReply(Messages::RemoteQueue::WriteBuffer(convertedBuffer, bufferOffset, WTFMove(handle)), [sharedMemory = sharedMemory.copyRef(), handleHasValue = handle.has_value()](auto) mutable {
        RELEASE_ASSERT(sharedMemory.get() || !handleHasValue);
    });
    UNUSED_VARIABLE(sendResult);
}

void RemoteQueueProxy::writeTexture(
    const WebCore::WebGPU::ImageCopyTexture& destination,
    std::span<const uint8_t> source,
    const WebCore::WebGPU::ImageDataLayout& dataLayout,
    const WebCore::WebGPU::Extent3D& size)
{
    Ref convertToBackingContext = m_convertToBackingContext;
    auto convertedDestination = convertToBackingContext->convertToBacking(destination);
    ASSERT(convertedDestination);
    auto convertedDataLayout = convertToBackingContext->convertToBacking(dataLayout);
    ASSERT(convertedDataLayout);
    auto convertedSize = convertToBackingContext->convertToBacking(size);
    ASSERT(convertedSize);
    if (!convertedDestination || !convertedDataLayout || !convertedSize)
        return;

    auto sharedMemory = WebCore::SharedMemory::copySpan(source);
    std::optional<WebCore::SharedMemoryHandle> handle;
    if (sharedMemory)
        handle = sharedMemory->createHandle(WebCore::SharedMemory::Protection::ReadOnly);
    auto sendResult = sendWithAsyncReply(Messages::RemoteQueue::WriteTexture(*convertedDestination, WTFMove(handle), *convertedDataLayout, *convertedSize), [sharedMemory = sharedMemory.copyRef(), handleHasValue = handle.has_value()](auto) mutable {
        RELEASE_ASSERT(sharedMemory.get() || !handleHasValue);
    });
    UNUSED_VARIABLE(sendResult);
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
    Ref convertToBackingContext = m_convertToBackingContext;
    auto convertedSource = convertToBackingContext->convertToBacking(source);
    ASSERT(convertedSource);
    auto convertedDestination = convertToBackingContext->convertToBacking(destination);
    ASSERT(convertedDestination);
    auto convertedCopySize = convertToBackingContext->convertToBacking(copySize);
    ASSERT(convertedCopySize);
    if (!convertedSource || !convertedDestination || !convertedCopySize)
        return;

    auto sendResult = send(Messages::RemoteQueue::CopyExternalImageToTexture(*convertedSource, *convertedDestination, *convertedCopySize));
    UNUSED_VARIABLE(sendResult);
}

void RemoteQueueProxy::setLabelInternal(const String& label)
{
    auto sendResult = send(Messages::RemoteQueue::SetLabel(label));
    UNUSED_VARIABLE(sendResult);
}

Ref<ConvertToBackingContext> RemoteQueueProxy::protectedConvertToBackingContext() const
{
    return m_convertToBackingContext;
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
