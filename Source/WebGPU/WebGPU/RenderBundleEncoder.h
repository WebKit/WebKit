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
#import "RenderBundle.h"
#import <wtf/FastMalloc.h>
#import <wtf/Function.h>
#import <wtf/Ref.h>
#import <wtf/RefCounted.h>
#import <wtf/Vector.h>

struct WGPURenderBundleEncoderImpl {
};

namespace WebGPU {

class BindGroup;
class Buffer;
class Device;
class RenderBundle;
class RenderPipeline;

// https://gpuweb.github.io/gpuweb/#gpurenderbundleencoder
class RenderBundleEncoder : public WGPURenderBundleEncoderImpl, public RefCounted<RenderBundleEncoder>, public CommandsMixin {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RenderBundleEncoder> create(MTLIndirectCommandBufferDescriptor *indirectCommandBufferDescriptor, Device& device)
    {
        return adoptRef(*new RenderBundleEncoder(indirectCommandBufferDescriptor, device));
    }
    static Ref<RenderBundleEncoder> createInvalid(Device& device)
    {
        return adoptRef(*new RenderBundleEncoder(device));
    }

    ~RenderBundleEncoder();

    void draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
    void drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance);
    void drawIndexedIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset);
    void drawIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset);
    Ref<RenderBundle> finish(const WGPURenderBundleDescriptor&);
    void insertDebugMarker(String&& markerLabel);
    void popDebugGroup();
    void pushDebugGroup(String&& groupLabel);
    void setBindGroup(uint32_t groupIndex, const BindGroup&, uint32_t dynamicOffsetCount, const uint32_t* dynamicOffsets);
    void setIndexBuffer(const Buffer&, WGPUIndexFormat, uint64_t offset, uint64_t size);
    void setPipeline(const RenderPipeline&);
    void setVertexBuffer(uint32_t slot, const Buffer&, uint64_t offset, uint64_t size);
    void setLabel(String&&);

    Device& device() const { return m_device; }

    bool isValid() const { return m_indirectCommandBuffer; }

private:
    RenderBundleEncoder(MTLIndirectCommandBufferDescriptor*, Device&);
    RenderBundleEncoder(Device&);

    bool validatePopDebugGroup() const;
    id<MTLIndirectRenderCommand> currentRenderCommand();

    void makeInvalid() { m_indirectCommandBuffer = nil; }

    id<MTLIndirectCommandBuffer> m_indirectCommandBuffer { nil };
    MTLIndirectCommandBufferDescriptor *m_icbDescriptor { nil };

    uint64_t m_debugGroupStackSize { 0 };
    uint64_t m_currentCommandIndex { 0 };
    id<MTLBuffer> m_indexBuffer { nil };
    MTLPrimitiveType m_primitiveType { MTLPrimitiveTypeTriangle };
    MTLIndexType m_indexType { MTLIndexTypeUInt16 };
    NSUInteger m_indexBufferOffset { 0 };
    Vector<WTF::Function<void(void)>> m_recordedCommands;
    Vector<BindableResource> m_resources;
    const Ref<Device> m_device;
};

} // namespace WebGPU
