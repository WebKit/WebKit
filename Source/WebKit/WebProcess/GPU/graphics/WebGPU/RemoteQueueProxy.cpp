/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

namespace WebKit::WebGPU {

RemoteQueueProxy::RemoteQueueProxy(RemoteDeviceProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_parent(parent)
{
}

RemoteQueueProxy::~RemoteQueueProxy()
{
}

void RemoteQueueProxy::submit(Vector<std::reference_wrapper<PAL::WebGPU::CommandBuffer>>&& commandBuffers)
{
    Vector<WebGPUIdentifier> convertedCommandBuffers;
    convertedCommandBuffers.reserveInitialCapacity(commandBuffers.size());
    for (auto commandBuffer : commandBuffers) {
        auto convertedCommandBuffer = m_convertToBackingContext->convertToBacking(commandBuffer);
        ASSERT(convertedCommandBuffer);
        if (!convertedCommandBuffer)
            return;
        convertedCommandBuffers.uncheckedAppend(convertedCommandBuffer);
    }
    auto sendResult = send(Messages::RemoteQueue::Submit(convertedCommandBuffers));
    UNUSED_VARIABLE(sendResult);
}

void RemoteQueueProxy::onSubmittedWorkDone(CompletionHandler<void()>&& callback)
{
    auto sendResult = sendSync(Messages::RemoteQueue::OnSubmittedWorkDone());
    UNUSED_VARIABLE(sendResult);

    callback();
}

void RemoteQueueProxy::writeBuffer(
    const PAL::WebGPU::Buffer& buffer,
    PAL::WebGPU::Size64 bufferOffset,
    const void* source,
    size_t byteLength,
    PAL::WebGPU::Size64 dataOffset,
    std::optional<PAL::WebGPU::Size64> size)
{
    auto convertedBuffer = m_convertToBackingContext->convertToBacking(buffer);
    ASSERT(convertedBuffer);
    if (!convertedBuffer)
        return;

    auto sendResult = send(Messages::RemoteQueue::WriteBuffer(convertedBuffer, bufferOffset, Vector<uint8_t>(static_cast<const uint8_t*>(source) + dataOffset, size.value_or(byteLength - dataOffset))));
    UNUSED_VARIABLE(sendResult);
}

void RemoteQueueProxy::writeTexture(
    const PAL::WebGPU::ImageCopyTexture& destination,
    const void* source,
    size_t byteLength,
    const PAL::WebGPU::ImageDataLayout& dataLayout,
    const PAL::WebGPU::Extent3D& size)
{
    auto convertedDestination = m_convertToBackingContext->convertToBacking(destination);
    ASSERT(convertedDestination);
    auto convertedDataLayout = m_convertToBackingContext->convertToBacking(dataLayout);
    ASSERT(convertedDataLayout);
    auto convertedSize = m_convertToBackingContext->convertToBacking(size);
    ASSERT(convertedSize);
    if (!convertedDestination || !convertedDataLayout || !convertedSize)
        return;

    auto sendResult = send(Messages::RemoteQueue::WriteTexture(*convertedDestination, Vector<uint8_t>(static_cast<const uint8_t*>(source), byteLength), *convertedDataLayout, *convertedSize));
    UNUSED_VARIABLE(sendResult);
}

void RemoteQueueProxy::copyExternalImageToTexture(
    const PAL::WebGPU::ImageCopyExternalImage& source,
    const PAL::WebGPU::ImageCopyTextureTagged& destination,
    const PAL::WebGPU::Extent3D& copySize)
{
    auto convertedSource = m_convertToBackingContext->convertToBacking(source);
    ASSERT(convertedSource);
    auto convertedDestination = m_convertToBackingContext->convertToBacking(destination);
    ASSERT(convertedDestination);
    auto convertedCopySize = m_convertToBackingContext->convertToBacking(copySize);
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

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
