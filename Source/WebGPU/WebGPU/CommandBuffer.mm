/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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
#import "CommandBuffer.h"

#import "APIConversions.h"

namespace WebGPU {

CommandBuffer::CommandBuffer(id<MTLCommandBuffer> commandBuffer, Device& device)
    : m_commandBuffer(commandBuffer)
    , m_device(device)
{
}

CommandBuffer::CommandBuffer(Device& device)
    : m_device(device)
{
}

CommandBuffer::~CommandBuffer() = default;

void CommandBuffer::setLabel(String&& label)
{
    m_commandBuffer.label = label;
}

void CommandBuffer::makeInvalid()
{
    m_commandBuffer = nil;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuCommandBufferReference(WGPUCommandBuffer commandBuffer)
{
    WebGPU::fromAPI(commandBuffer).ref();
}

void wgpuCommandBufferRelease(WGPUCommandBuffer commandBuffer)
{
    WebGPU::fromAPI(commandBuffer).deref();
}

void wgpuCommandBufferSetLabel(WGPUCommandBuffer commandBuffer, const char* label)
{
    WebGPU::fromAPI(commandBuffer).setLabel(WebGPU::fromAPI(label));
}
