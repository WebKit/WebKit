/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "Queue.h"

#import "APIConversions.h"
#import "Buffer.h"
#import "CommandBuffer.h"
#import "Device.h"

namespace WebGPU {

Queue::Queue(id<MTLCommandQueue> commandQueue, Device& device)
    : m_commandQueue(commandQueue)
    , m_device(device)
{
}

Queue::~Queue() = default;

void Queue::onSubmittedWorkDone(uint64_t, CompletionHandler<void(WGPUQueueWorkDoneStatus)>&& callback)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpuqueue-onsubmittedworkdone

    ASSERT(m_submittedCommandBufferCount >= m_completedCommandBufferCount);

    if (m_submittedCommandBufferCount == m_completedCommandBufferCount) {
        scheduleWork([callback = WTFMove(callback)]() mutable {
            callback(WGPUQueueWorkDoneStatus_Success);
        });
        return;
    }

    auto& callbacks = m_onSubmittedWorkDoneCallbacks.add(m_submittedCommandBufferCount, OnSubmittedWorkDoneCallbacks()).iterator->value;
    callbacks.append(WTFMove(callback));
}

bool Queue::validateSubmit() const
{
    // FIXME: "Every {{GPUCommandBuffer}} in |commandBuffers| is [$valid to use with$] |this|."

    // FIXME: "Every GPUBuffer referenced in any element of commandBuffers is in the "unmapped" buffer state."

    // FIXME: "Every GPUQuerySet referenced in a command in any element of commandBuffers is in the available state."
    // FIXME: "For occlusion queries, occlusionQuerySet in beginRenderPass() does not constitute a reference, while beginOcclusionQuery() does."

    // There's only one queue right now, so there is no need to make sure that the command buffers are being submitted to the correct queue.

    return true;
}

void Queue::submit(Vector<std::reference_wrapper<const CommandBuffer>>&& commands)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpuqueue-submit

    // "If any of the following conditions are unsatisfied"
    if (!validateSubmit()) {
        // FIXME: "generate a validation error and stop."
        return;
    }

    // "For each commandBuffer in commandBuffers:"
    for (auto commandBuffer : commands) {
        ASSERT(commandBuffer.get().commandBuffer().commandQueue == m_commandQueue); //
        [commandBuffer.get().commandBuffer() addCompletedHandler:[protectedThis = Ref { *this }] (id<MTLCommandBuffer>) {
            protectedThis->scheduleWork(CompletionHandler<void(void)>([protectedThis = protectedThis.copyRef()]() {
                ++(protectedThis->m_completedCommandBufferCount);
                for (auto& callback : protectedThis->m_onSubmittedWorkDoneCallbacks.take(protectedThis->m_completedCommandBufferCount))
                    callback(WGPUQueueWorkDoneStatus_Success);
            }, CompletionHandlerCallThread::MainThread));
        }];

        // "Execute each command in commandBuffer.[[command_list]]."
        [commandBuffer.get().commandBuffer() commit];
    }

    m_submittedCommandBufferCount += commands.size();
}

void Queue::writeBuffer(const Buffer& buffer, uint64_t bufferOffset, const void* data, size_t size)
{
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(bufferOffset);
    UNUSED_PARAM(data);
    UNUSED_PARAM(size);
}

void Queue::writeTexture(const WGPUImageCopyTexture& destination, const void* data, size_t dataSize, const WGPUTextureDataLayout& dataLayout, const WGPUExtent3D& writeSize)
{
    UNUSED_PARAM(destination);
    UNUSED_PARAM(data);
    UNUSED_PARAM(dataSize);
    UNUSED_PARAM(dataLayout);
    UNUSED_PARAM(writeSize);
}

void Queue::setLabel(String&& label)
{
    m_commandQueue.label = label;
}

void Queue::scheduleWork(Instance::WorkItem&& workItem)
{
    m_device.instance().scheduleWork(WTFMove(workItem));
}

} // namespace WebGPU

void wgpuQueueRelease(WGPUQueue queue)
{
    delete queue;
}

void wgpuQueueOnSubmittedWorkDone(WGPUQueue queue, uint64_t signalValue, WGPUQueueWorkDoneCallback callback, void* userdata)
{
    WebGPU::fromAPI(queue).onSubmittedWorkDone(signalValue, [callback, userdata] (WGPUQueueWorkDoneStatus status) {
        callback(status, userdata);
    });
}

void wgpuQueueOnSubmittedWorkDoneWithBlock(WGPUQueue queue, uint64_t signalValue, WGPUQueueWorkDoneBlockCallback callback)
{
    WebGPU::fromAPI(queue).onSubmittedWorkDone(signalValue, [callback] (WGPUQueueWorkDoneStatus status) {
        callback(status);
    });
}

void wgpuQueueSubmit(WGPUQueue queue, uint32_t commandCount, const WGPUCommandBuffer* commands)
{
    Vector<std::reference_wrapper<const WebGPU::CommandBuffer>> commandsToForward;
    for (uint32_t i = 0; i < commandCount; ++i)
        commandsToForward.append(WebGPU::fromAPI(commands[i]));
    WebGPU::fromAPI(queue).submit(WTFMove(commandsToForward));
}

void wgpuQueueWriteBuffer(WGPUQueue queue, WGPUBuffer buffer, uint64_t bufferOffset, const void* data, size_t size)
{
    WebGPU::fromAPI(queue).writeBuffer(WebGPU::fromAPI(buffer), bufferOffset, data, size);
}

void wgpuQueueWriteTexture(WGPUQueue queue, const WGPUImageCopyTexture* destination, const void* data, size_t dataSize, const WGPUTextureDataLayout* dataLayout, const WGPUExtent3D* writeSize)
{
    WebGPU::fromAPI(queue).writeTexture(*destination, data, dataSize, *dataLayout, *writeSize);
}

void wgpuQueueSetLabel(WGPUQueue queue, const char* label)
{
    WebGPU::fromAPI(queue).setLabel(WebGPU::fromAPI(label));
}
