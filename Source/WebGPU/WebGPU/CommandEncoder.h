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

#import "CommandsMixin.h"
#import <wtf/FastMalloc.h>
#import <wtf/Ref.h>
#import <wtf/RefCounted.h>

struct WGPUCommandEncoderImpl {
};

namespace WebGPU {

class Buffer;
class CommandBuffer;
class ComputePassEncoder;
class Device;
class QuerySet;
class RenderPassEncoder;

// https://gpuweb.github.io/gpuweb/#gpucommandencoder
class CommandEncoder : public WGPUCommandEncoderImpl, public RefCounted<CommandEncoder>, public CommandsMixin {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<CommandEncoder> create(id<MTLCommandBuffer> commandBuffer, Device& device)
    {
        return adoptRef(*new CommandEncoder(commandBuffer, device));
    }
    static Ref<CommandEncoder> createInvalid(Device& device)
    {
        return adoptRef(*new CommandEncoder(device));
    }

    ~CommandEncoder();

    Ref<ComputePassEncoder> beginComputePass(const WGPUComputePassDescriptor&);
    Ref<RenderPassEncoder> beginRenderPass(const WGPURenderPassDescriptor&);
    void copyBufferToBuffer(const Buffer& source, uint64_t sourceOffset, const Buffer& destination, uint64_t destinationOffset, uint64_t size);
    void copyBufferToTexture(const WGPUImageCopyBuffer& source, const WGPUImageCopyTexture& destination, const WGPUExtent3D& copySize);
    void copyTextureToBuffer(const WGPUImageCopyTexture& source, const WGPUImageCopyBuffer& destination, const WGPUExtent3D& copySize);
    void copyTextureToTexture(const WGPUImageCopyTexture& source, const WGPUImageCopyTexture& destination, const WGPUExtent3D& copySize);
    void clearBuffer(const Buffer&, uint64_t offset, uint64_t size);
    Ref<CommandBuffer> finish(const WGPUCommandBufferDescriptor&);
    void insertDebugMarker(String&& markerLabel);
    void popDebugGroup();
    void pushDebugGroup(String&& groupLabel);
    void resolveQuerySet(const QuerySet&, uint32_t firstQuery, uint32_t queryCount, const Buffer& destination, uint64_t destinationOffset);
    void writeTimestamp(QuerySet&, uint32_t queryIndex);
    void setLabel(String&&);

    Device& device() const { return m_device; }

    bool isValid() const { return m_commandBuffer; }

private:
    CommandEncoder(id<MTLCommandBuffer>, Device&);
    CommandEncoder(Device&);

    bool validateCopyBufferToBuffer(const Buffer& source, uint64_t sourceOffset, const Buffer& destination, uint64_t destinationOffset, uint64_t size);
    bool validateClearBuffer(const Buffer&, uint64_t offset, uint64_t size);
    bool validateFinish() const;
    bool validatePopDebugGroup() const;
    bool validateRenderPassDescriptor(const WGPURenderPassDescriptor&) const;

    void makeInvalid() { m_commandBuffer = nil; }

    void ensureBlitCommandEncoder();
    void finalizeBlitCommandEncoder();

    id<MTLCommandBuffer> m_commandBuffer { nil };
    id<MTLBlitCommandEncoder> m_blitCommandEncoder { nil };

    uint64_t m_debugGroupStackSize { 0 };

    const Ref<Device> m_device;
};

} // namespace WebGPU
