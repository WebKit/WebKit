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
#include "GPUDevice.h"
#include "GPUImageCopyExternalImage.h"
#include "GPUTexture.h"
#include "GPUTextureDescriptor.h"

#if HAVE(WEBGPU_IMPLEMENTATION)
#include <WebCore/ImageBuffer.h>
#include <WebCore/PixelBuffer.h>
#endif // HAVE(WEBGPU_IMPLEMENTATION)

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

#if HAVE(WEBGPU_IMPLEMENTATION)
static ImageBuffer* imageBufferForSource(const auto& source)
{
    return WTF::switchOn(source, [] (const RefPtr<ImageBitmap>& imageBitmap) {
        return imageBitmap->buffer();
    }, [] (const RefPtr<HTMLCanvasElement>& canvasElement) {
        return canvasElement->buffer();
    }, [] (const RefPtr<OffscreenCanvas>& offscreenCanvasElement) {
        return offscreenCanvasElement->buffer();
    });
}
#endif // HAVE(WEBGPU_IMPLEMENTATION)

void GPUQueue::copyExternalImageToTexture(
    const GPUImageCopyExternalImage& source,
    const GPUImageCopyTextureTagged& destination,
    const GPUExtent3D& copySize)
{
#if HAVE(WEBGPU_IMPLEMENTATION)
    auto imageBuffer = imageBufferForSource(source.source);
    if (!imageBuffer)
        return;

    auto size = imageBuffer->truncatedLogicalSize();
    if (!size.width() || !size.height())
        return;

    auto pixelBuffer = imageBuffer->getPixelBuffer({ AlphaPremultiplication::Unpremultiplied, PixelFormat::BGRA8, DestinationColorSpace::SRGB() }, { { }, size });
    ASSERT(pixelBuffer);

    auto sizeInBytes = pixelBuffer->sizeInBytes();
    auto rows = size.height();
    GPUImageDataLayout dataLayout { 0, sizeInBytes / rows, rows };
    m_backing->writeTexture(destination.convertToBacking(), pixelBuffer->bytes(), sizeInBytes, dataLayout.convertToBacking(), convertToBacking(copySize));
#else
    UNUSED_PARAM(source);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(copySize);
#endif // HAVE(WEBGPU_IMPLEMENTATION)
}

}
