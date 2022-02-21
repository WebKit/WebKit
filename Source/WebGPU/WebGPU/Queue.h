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
#import <wtf/Function.h>
#import <wtf/Ref.h>
#import <wtf/RefCounted.h>
#import <wtf/Vector.h>

namespace WebGPU {

class Buffer;
class CommandBuffer;

class Queue : public RefCounted<Queue> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<Queue> create(id <MTLCommandQueue> commandQueue)
    {
        return adoptRef(*new Queue(commandQueue));
    }

    ~Queue();

    void onSubmittedWorkDone(uint64_t signalValue, WTF::Function<void(WGPUQueueWorkDoneStatus)>&& callback);
    void submit(Vector<std::reference_wrapper<const CommandBuffer>>&& commands);
    void writeBuffer(const Buffer&, uint64_t bufferOffset, const void* data, size_t);
    void writeTexture(const WGPUImageCopyTexture* destination, const void* data, size_t dataSize, const WGPUTextureDataLayout*, const WGPUExtent3D* writeSize);
    void setLabel(const char*);

private:
    Queue(id <MTLCommandQueue>);

    id <MTLCommandQueue> m_commandQueue { nil };
};

} // namespace WebGPU

struct WGPUQueueImpl {
    Ref<WebGPU::Queue> queue;
};
