/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
#include "GPUQueue.h"

#include "GPUBuffer.h"

namespace WebCore {

String GPUQueue::label() const
{
    return m_backing->label();
}

void GPUQueue::setLabel(String&& label)
{
    m_backing->setLabel(WTFMove(label));
}

void GPUQueue::submit(Vector<RefPtr<GPUCommandBuffer>>&& commandBuffers)
{
    Vector<std::reference_wrapper<PAL::WebGPU::CommandBuffer>> result;
    result.reserveInitialCapacity(commandBuffers.size());
    for (const auto& commandBuffer : commandBuffers) {
        if (!commandBuffer)
            continue;
        result.uncheckedAppend(commandBuffer->backing());
    }
    m_backing->submit(WTFMove(result));
}

void GPUQueue::onSubmittedWorkDone(OnSubmittedWorkDonePromise&& promise)
{
    m_backing->onSubmittedWorkDone([promise = WTFMove(promise)] () mutable {
        promise.resolve(nullptr);
    });
}

void GPUQueue::writeBuffer(
    const GPUBuffer& buffer,
    GPUSize64 bufferOffset,
    BufferSource&& data,
    GPUSize64 dataOffset,
    std::optional<GPUSize64> size)
{
    m_backing->writeBuffer(buffer.backing(), bufferOffset, data.data(), data.length(), dataOffset, size);
}

void GPUQueue::writeTexture(
    const GPUImageCopyTexture& destination,
    BufferSource&& data,
    const GPUImageDataLayout& imageDataLayout,
    const GPUExtent3D& size)
{
    m_backing->writeTexture(destination.convertToBacking(), data.data(), data.length(), imageDataLayout.convertToBacking(), convertToBacking(size));
}

void GPUQueue::copyExternalImageToTexture(
    const GPUImageCopyExternalImage& source,
    const GPUImageCopyTextureTagged& destination,
    const GPUExtent3D& copySize)
{
    m_backing->copyExternalImageToTexture(source.convertToBacking(), destination.convertToBacking(), convertToBacking(copySize));
}

}
