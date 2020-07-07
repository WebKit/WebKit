/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#import "config.h"
#import "GPUQueue.h"

#if ENABLE(WEBGPU)

#import "GPUCommandBuffer.h"
#import "GPUDevice.h"
#import "GPUSwapChain.h"
#import "Logging.h"
#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/text/WTFString.h>

namespace WebCore {

static NSString * const commandQueueDefaultLabel = @"com.apple.WebKit";
static NSString * const commandQueueLabelPrefix = @"com.apple.WebKit.";

RefPtr<GPUQueue> GPUQueue::tryCreate(const GPUDevice& device)
{
    if (!device.platformDevice()) {
        LOG(WebGPU, "GPUQueue::tryCreate(): Invalid GPUDevice!");
        return nullptr;
    }

    RetainPtr<MTLCommandQueue> queue;

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    queue = adoptNS([device.platformDevice() newCommandQueue]);
    [queue setLabel:commandQueueDefaultLabel];

    END_BLOCK_OBJC_EXCEPTIONS

    if (!queue) {
        LOG(WebGPU, "GPUQueue::tryCreate(): Unable to create MTLCommandQueue!");
        return nullptr;
    }

    return adoptRef(new GPUQueue(WTFMove(queue), device));
}

GPUQueue::GPUQueue(RetainPtr<MTLCommandQueue>&& queue, const GPUDevice& device)
    : m_platformQueue(WTFMove(queue))
    , m_device(makeWeakPtr(device))
{
}

void GPUQueue::submit(Vector<Ref<GPUCommandBuffer>>&& commandBuffers)
{
    for (auto& commandBuffer : commandBuffers) {
        // Validate resource integrity.
        for (auto& buffer : commandBuffer->usedBuffers()) {
            if (buffer->state() != GPUBuffer::State::Unmapped) {
                LOG(WebGPU, "GPUQueue::submit(): Invalid GPUBuffer set on a GPUCommandBuffer!");
                return;
            }
        }
        for (auto& texture : commandBuffer->usedTextures()) {
            if (!texture->platformTexture()) {
                LOG(WebGPU, "GPUQueue::submit(): Invalid GPUTexture set on a GPUCommandBuffer!");
                return;
            }
        }
        commandBuffer->endBlitEncoding();
        // Okay to commit; prevent any buffer mapping callbacks from executing until command buffer is complete.
        for (auto& buffer : commandBuffer->usedBuffers())
            buffer->commandBufferCommitted(commandBuffer->platformCommandBuffer());
        BEGIN_BLOCK_OBJC_EXCEPTIONS
        [commandBuffer->platformCommandBuffer() commit];
        END_BLOCK_OBJC_EXCEPTIONS
    }

    if (m_presentTask.hasPendingTask() || !m_device || !m_device->swapChain())
        return;

    // If a GPUSwapChain exists, ensure that a present is scheduled after all command buffers.
    m_presentTask.scheduleTask([this, protectedThis = makeRef(*this), swapChain = makeRef(*m_device->swapChain())] () {
        auto currentDrawable = swapChain->takeDrawable();
        // If the GPUSwapChain has no drawable, it was invalidated between command submission and this task.
        if (!currentDrawable)
            return;

        auto presentCommandBuffer = [m_platformQueue commandBuffer];
        [presentCommandBuffer presentDrawable:static_cast<id<MTLDrawable>>(currentDrawable.get())];
        [presentCommandBuffer commit];
    });
}

String GPUQueue::label() const
{
    if (!m_platformQueue)
        return emptyString();

    NSString *prefixedLabel = [m_platformQueue label];

    if ([prefixedLabel isEqualToString:commandQueueDefaultLabel])
        return emptyString();

    ASSERT(prefixedLabel.length > commandQueueLabelPrefix.length);
    return [prefixedLabel substringFromIndex:commandQueueLabelPrefix.length];
}

void GPUQueue::setLabel(const String& label) const
{
    if (label.isEmpty())
        [m_platformQueue setLabel:commandQueueDefaultLabel];
    else
        [m_platformQueue setLabel:[commandQueueLabelPrefix stringByAppendingString:label]];
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
