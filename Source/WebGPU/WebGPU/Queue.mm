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

#import "Buffer.h"
#import "CommandBuffer.h"
#import "WebGPUExt.h"

namespace WebGPU {

Queue::Queue(id <MTLCommandQueue> commandQueue)
    : m_commandQueue(commandQueue)
{
    UNUSED_VARIABLE(m_commandQueue);
}

Queue::~Queue() = default;

void Queue::onSubmittedWorkDone(uint64_t signalValue, WTF::Function<void(WGPUQueueWorkDoneStatus)>&& callback)
{
    UNUSED_PARAM(signalValue);
    UNUSED_PARAM(callback);
}

void Queue::submit(Vector<std::reference_wrapper<const CommandBuffer>>&& commands)
{
    UNUSED_PARAM(commands);
}

void Queue::writeBuffer(const Buffer& buffer, uint64_t bufferOffset, const void* data, size_t size)
{
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(bufferOffset);
    UNUSED_PARAM(data);
    UNUSED_PARAM(size);
}

void Queue::writeTexture(const WGPUImageCopyTexture* destination, const void* data, size_t dataSize, const WGPUTextureDataLayout* dataLayout, const WGPUExtent3D* writeSize)
{
    UNUSED_PARAM(destination);
    UNUSED_PARAM(data);
    UNUSED_PARAM(dataSize);
    UNUSED_PARAM(dataLayout);
    UNUSED_PARAM(writeSize);
}

void Queue::setLabel(const char* label)
{
    UNUSED_PARAM(label);
}

} // namespace WebGPU

void wgpuQueueRelease(WGPUQueue queue)
{
    delete queue;
}

void wgpuQueueOnSubmittedWorkDone(WGPUQueue queue, uint64_t signalValue, WGPUQueueWorkDoneCallback callback, void* userdata)
{
    queue->queue->onSubmittedWorkDone(signalValue, [callback, userdata] (WGPUQueueWorkDoneStatus status) {
        callback(status, userdata);
    });
}

void wgpuQueueSubmit(WGPUQueue queue, uint32_t commandCount, const WGPUCommandBuffer* commands)
{
    Vector<std::reference_wrapper<const WebGPU::CommandBuffer>> commandsToForward;
    for (uint32_t i = 0; i < commandCount; ++i)
        commandsToForward.append(commands[i]->commandBuffer);
    queue->queue->submit(WTFMove(commandsToForward));
}

void wgpuQueueWriteBuffer(WGPUQueue queue, WGPUBuffer buffer, uint64_t bufferOffset, const void* data, size_t size)
{
    queue->queue->writeBuffer(buffer->buffer, bufferOffset, data, size);
}

void wgpuQueueWriteTexture(WGPUQueue queue, const WGPUImageCopyTexture* destination, const void* data, size_t dataSize, const WGPUTextureDataLayout* dataLayout, const WGPUExtent3D* writeSize)
{
    queue->queue->writeTexture(destination, data, dataSize, dataLayout, writeSize);
}

void wgpuQueueSetLabel(WGPUQueue queue, const char* label)
{
    queue->queue->setLabel(label);
}

