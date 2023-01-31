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
#include "WebGPUQueueImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUBufferImpl.h"
#include "WebGPUCommandBufferImpl.h"
#include "WebGPUConvertToBackingContext.h"
#include "WebGPUTextureImpl.h"
#include <WebGPU/WebGPUExt.h>
#include <wtf/BlockPtr.h>

namespace PAL::WebGPU {

QueueImpl::QueueImpl(Ref<DeviceHolderImpl>&& deviceHolder, ConvertToBackingContext& convertToBackingContext)
    : m_deviceHolder(WTFMove(deviceHolder))
    , m_convertToBackingContext(convertToBackingContext)
{
}

QueueImpl::~QueueImpl() = default;

void QueueImpl::submit(Vector<std::reference_wrapper<CommandBuffer>>&& commandBuffers)
{
    auto backingCommandBuffers = commandBuffers.map([this] (auto commandBuffer) {
        return m_convertToBackingContext->convertToBacking(commandBuffer);
    });

    wgpuQueueSubmit(backing(), backingCommandBuffers.size(), backingCommandBuffers.data());
}

void QueueImpl::onSubmittedWorkDone(CompletionHandler<void()>&& callback)
{
    wgpuQueueOnSubmittedWorkDoneWithBlock(backing(), makeBlockPtr([callback = WTFMove(callback)](WGPUQueueWorkDoneStatus) mutable {
        callback();
    }).get());
}

void QueueImpl::writeBuffer(
    const Buffer& buffer,
    Size64 bufferOffset,
    const void* source,
    size_t byteLength,
    Size64 dataOffset,
    std::optional<Size64> size)
{
    // FIXME: Use checked arithmetic and check the cast
    wgpuQueueWriteBuffer(backing(), m_convertToBackingContext->convertToBacking(buffer), bufferOffset, static_cast<const uint8_t*>(source) + dataOffset, static_cast<size_t>(size.value_or(byteLength - dataOffset)));
}

void QueueImpl::writeTexture(
    const ImageCopyTexture& destination,
    const void* source,
    size_t byteLength,
    const ImageDataLayout& dataLayout,
    const Extent3D& size)
{
    WGPUImageCopyTexture backingDestination {
        nullptr,
        m_convertToBackingContext->convertToBacking(destination.texture),
        destination.mipLevel,
        destination.origin ? m_convertToBackingContext->convertToBacking(*destination.origin) : WGPUOrigin3D { 0, 0, 0 },
        m_convertToBackingContext->convertToBacking(destination.aspect),
    };

    WGPUTextureDataLayout backingDataLayout {
        nullptr,
        dataLayout.offset,
        dataLayout.bytesPerRow.value_or(0),
        dataLayout.rowsPerImage.value_or(0),
    };

    WGPUExtent3D backingSize = m_convertToBackingContext->convertToBacking(size);

    wgpuQueueWriteTexture(backing(), &backingDestination, source, byteLength, &backingDataLayout, &backingSize);
}

void QueueImpl::copyExternalImageToTexture(
    const ImageCopyExternalImage& source,
    const ImageCopyTextureTagged& destination,
    const Extent3D& copySize)
{
    UNUSED_PARAM(source);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(copySize);
}

void QueueImpl::setLabelInternal(const String& label)
{
    wgpuQueueSetLabel(backing(), label.utf8().data());
}

} // namespace PAL::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
