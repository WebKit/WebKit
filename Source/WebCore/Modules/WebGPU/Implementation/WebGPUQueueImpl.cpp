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
#include "WebGPUQueueImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUBufferImpl.h"
#include "WebGPUCommandBufferImpl.h"
#include "WebGPUConvertToBackingContext.h"
#include "WebGPUTextureImpl.h"
#include <WebGPU/WebGPUExt.h>
#include <wtf/BlockPtr.h>

namespace WebCore::WebGPU {

QueueImpl::QueueImpl(WebGPUPtr<WGPUQueue>&& queue, ConvertToBackingContext& convertToBackingContext)
    : m_backing(WTFMove(queue))
    , m_convertToBackingContext(convertToBackingContext)
{
}

QueueImpl::~QueueImpl() = default;

void QueueImpl::submit(Vector<std::reference_wrapper<CommandBuffer>>&& commandBuffers)
{
    auto backingCommandBuffers = commandBuffers.map([&convertToBackingContext = m_convertToBackingContext.get()](auto commandBuffer) {
        return convertToBackingContext.convertToBacking(commandBuffer);
    });

    wgpuQueueSubmit(m_backing.get(), backingCommandBuffers.size(), backingCommandBuffers.data());
}

static void onSubmittedWorkDoneCallback(WGPUQueueWorkDoneStatus status, void* userdata)
{
    auto block = reinterpret_cast<void(^)(WGPUQueueWorkDoneStatus)>(userdata);
    block(status);
    Block_release(block); // Block_release is matched with Block_copy below in QueueImpl::submit().
}

void QueueImpl::onSubmittedWorkDone(CompletionHandler<void()>&& callback)
{
    auto blockPtr = makeBlockPtr([callback = WTFMove(callback)](WGPUQueueWorkDoneStatus) mutable {
        callback();
    });
    wgpuQueueOnSubmittedWorkDone(m_backing.get(), &onSubmittedWorkDoneCallback, Block_copy(blockPtr.get())); // Block_copy is matched with Block_release above in onSubmittedWorkDoneCallback().
}

void QueueImpl::writeBuffer(
    const Buffer&,
    Size64,
    const void*,
    size_t,
    Size64,
    std::optional<Size64>)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void QueueImpl::writeTexture(
    const ImageCopyTexture&,
    const void*,
    size_t,
    const ImageDataLayout&,
    const Extent3D&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void QueueImpl::writeBuffer(
    const Buffer& buffer,
    Size64 bufferOffset,
    void* source,
    size_t byteLength,
    Size64 dataOffset,
    std::optional<Size64> size)
{
    // FIXME: Use checked arithmetic and check the cast
    wgpuQueueWriteBuffer(m_backing.get(), m_convertToBackingContext->convertToBacking(buffer), bufferOffset, static_cast<uint8_t*>(source) + dataOffset, static_cast<size_t>(size.value_or(byteLength - dataOffset)));
}

void QueueImpl::writeTexture(
    const ImageCopyTexture& destination,
    void* source,
    size_t byteLength,
    const ImageDataLayout& dataLayout,
    const Extent3D& size)
{
    WGPUImageCopyTexture backingDestination {
        .nextInChain = nullptr,
        .texture = m_convertToBackingContext->convertToBacking(destination.texture),
        .mipLevel = destination.mipLevel,
        .origin = destination.origin ? m_convertToBackingContext->convertToBacking(*destination.origin) : WGPUOrigin3D { 0, 0, 0 },
        .aspect = m_convertToBackingContext->convertToBacking(destination.aspect),
    };

    WGPUTextureDataLayout backingDataLayout {
        .nextInChain = nullptr,
        .offset = dataLayout.offset,
        .bytesPerRow = dataLayout.bytesPerRow.value_or(WGPU_COPY_STRIDE_UNDEFINED),
        .rowsPerImage = dataLayout.rowsPerImage.value_or(WGPU_COPY_STRIDE_UNDEFINED),
    };

    WGPUExtent3D backingSize = m_convertToBackingContext->convertToBacking(size);

    wgpuQueueWriteTexture(m_backing.get(), &backingDestination, source, byteLength, &backingDataLayout, &backingSize);
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
    wgpuQueueSetLabel(m_backing.get(), label.utf8().data());
}

} // namespace WebCore::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
