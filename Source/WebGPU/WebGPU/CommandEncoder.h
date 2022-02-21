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
#import <wtf/RefPtr.h>

namespace WebGPU {

class Buffer;
class CommandBuffer;
class ComputePassEncoder;
class QuerySet;
class RenderPassEncoder;

class CommandEncoder : public RefCounted<CommandEncoder> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<CommandEncoder> create(id <MTLCommandBuffer> commandBuffer)
    {
        return adoptRef(*new CommandEncoder(commandBuffer));
    }

    ~CommandEncoder();

    RefPtr<ComputePassEncoder> beginComputePass(const WGPUComputePassDescriptor*);
    RefPtr<RenderPassEncoder> beginRenderPass(const WGPURenderPassDescriptor*);
    void copyBufferToBuffer(const Buffer& source, uint64_t sourceOffset, const Buffer& destination, uint64_t destinationOffset, uint64_t size);
    void copyBufferToTexture(const WGPUImageCopyBuffer* source, const WGPUImageCopyTexture* destination, const WGPUExtent3D* copySize);
    void copyTextureToBuffer(const WGPUImageCopyTexture* source, const WGPUImageCopyBuffer* destination, const WGPUExtent3D* copySize);
    void copyTextureToTexture(const WGPUImageCopyTexture* source, const WGPUImageCopyTexture* destination, const WGPUExtent3D* copySize);
    void clearBuffer(const Buffer&, uint64_t offset, uint64_t size);
    RefPtr<CommandBuffer> finish(const WGPUCommandBufferDescriptor*);
    void insertDebugMarker(const char* markerLabel);
    void popDebugGroup();
    void pushDebugGroup(const char* groupLabel);
    void resolveQuerySet(const QuerySet&, uint32_t firstQuery, uint32_t queryCount, const Buffer& destination, uint64_t destinationOffset);
    void writeTimestamp(const QuerySet&, uint32_t queryIndex);
    void setLabel(const char*);

private:
    CommandEncoder(id <MTLCommandBuffer>);

    id <MTLCommandBuffer> m_commandBuffer { nil };
};

} // namespace WebGPU

struct WGPUCommandEncoderImpl {
    Ref<WebGPU::CommandEncoder> commandEncoder;
};
