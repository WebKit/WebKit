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
#import <wtf/TZoneMallocInlines.h>

namespace WebGPU {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CommandBuffer);

CommandBuffer::CommandBuffer(id<MTLCommandBuffer> commandBuffer, Device& device, id<MTLSharedEvent> sharedEvent, uint64_t sharedEventSignalValue)
    : m_commandBuffer(commandBuffer)
    , m_device(device)
    , m_sharedEvent(sharedEvent)
    , m_sharedEventSignalValue(sharedEventSignalValue)
{
}

CommandBuffer::CommandBuffer(Device& device)
    : m_device(device)
{
}

CommandBuffer::~CommandBuffer()
{
    m_device->protectedQueue()->removeMTLCommandBuffer(m_commandBuffer);
}

void CommandBuffer::setLabel(String&& label)
{
    m_commandBuffer.label = label;
}

void CommandBuffer::makeInvalid(NSString* lastError)
{
    if (!m_commandBuffer || m_commandBuffer.status >= MTLCommandBufferStatusCommitted)
        return;

    m_lastErrorString = lastError;
    m_device->protectedQueue()->removeMTLCommandBuffer(m_commandBuffer);
    m_commandBuffer = nil;
}

void CommandBuffer::makeInvalidDueToCommit(NSString* lastError)
{
    if (m_sharedEvent)
        [m_commandBuffer encodeSignalEvent:m_sharedEvent value:m_sharedEventSignalValue];

    m_cachedCommandBuffer = m_commandBuffer;
    [m_commandBuffer addCompletedHandler:[protectedThis = Ref { *this }](id<MTLCommandBuffer>) {
        protectedThis->m_commandBufferComplete.signal();
        protectedThis->m_cachedCommandBuffer = nil;
    }];
    m_lastErrorString = lastError;
    m_commandBuffer = nil;
}

NSString* CommandBuffer::lastError() const
{
    return m_lastErrorString;
}

bool CommandBuffer::waitForCompletion()
{
    auto status = [m_cachedCommandBuffer status];
    constexpr auto commandBufferSubmissionTimeout = 500_ms;
    if (status == MTLCommandBufferStatusCommitted || status == MTLCommandBufferStatusScheduled)
        return m_commandBufferComplete.waitFor(commandBufferSubmissionTimeout);

    return true;
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
    WebGPU::protectedFromAPI(commandBuffer)->setLabel(WebGPU::fromAPI(label));
}
