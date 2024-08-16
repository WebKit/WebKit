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

#pragma once

#import <wtf/FastMalloc.h>
#import <wtf/Ref.h>
#import <wtf/RefCounted.h>
#import <wtf/TZoneMalloc.h>
#import <wtf/WeakPtr.h>
#import <wtf/threads/BinarySemaphore.h>

struct WGPUCommandBufferImpl {
};

namespace WebGPU {

class Device;

// https://gpuweb.github.io/gpuweb/#gpucommandbuffer
class CommandBuffer : public WGPUCommandBufferImpl, public RefCounted<CommandBuffer>, public CanMakeWeakPtr<CommandBuffer> {
    WTF_MAKE_TZONE_ALLOCATED(CommandBuffer);
public:
    static Ref<CommandBuffer> create(id<MTLCommandBuffer> commandBuffer, id<MTLSharedEvent> event, Device& device)
    {
        return adoptRef(*new CommandBuffer(commandBuffer, event, device));
    }
    static Ref<CommandBuffer> createInvalid(Device& device)
    {
        return adoptRef(*new CommandBuffer(device));
    }

    ~CommandBuffer();

    void setLabel(String&&);

    bool isValid() const { return m_commandBuffer; }

    id<MTLCommandBuffer> commandBuffer() const { return m_commandBuffer; }

    Device& device() const { return m_device; }
    void makeInvalid(NSString*);
    void makeInvalidDueToCommit(NSString*);
    void setBufferMapCount(int);
    int bufferMapCount() const;
    NSString* lastError() const;
    void waitForCompletion();

private:
    CommandBuffer(id<MTLCommandBuffer>, id<MTLSharedEvent>, Device&);
    CommandBuffer(Device&);

    id<MTLCommandBuffer> m_commandBuffer { nil };
    id<MTLSharedEvent> m_abortEvent { nil };
    id<MTLCommandBuffer> m_cachedCommandBuffer { nil };
    int m_bufferMapCount { 0 };

    const Ref<Device> m_device;
    NSString* m_lastErrorString { nil };
    // FIXME: we should not need this semaphore - https://bugs.webkit.org/show_bug.cgi?id=272353
    BinarySemaphore m_commandBufferComplete;
};

} // namespace WebGPU
