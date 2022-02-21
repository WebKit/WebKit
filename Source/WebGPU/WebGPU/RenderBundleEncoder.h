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

class BindGroup;
class Buffer;
class RenderBundle;
class RenderPipeline;

class RenderBundleEncoder : public RefCounted<RenderBundleEncoder> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RenderBundleEncoder> create(id <MTLIndirectCommandBuffer> indirectCommandBuffer)
    {
        return adoptRef(*new RenderBundleEncoder(indirectCommandBuffer));
    }

    ~RenderBundleEncoder();

    void draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
    void drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance);
    void drawIndexedIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset);
    void drawIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset);
    RefPtr<RenderBundle> finish(const WGPURenderBundleDescriptor*);
    void insertDebugMarker(const char* markerLabel);
    void popDebugGroup();
    void pushDebugGroup(const char* groupLabel);
    void setBindGroup(uint32_t groupIndex, const BindGroup&, uint32_t dynamicOffsetCount, const uint32_t* dynamicOffsets);
    void setIndexBuffer(const Buffer&, WGPUIndexFormat, uint64_t offset, uint64_t size);
    void setPipeline(const RenderPipeline&);
    void setVertexBuffer(uint32_t slot, const Buffer&, uint64_t offset, uint64_t size);
    void setLabel(const char*);

private:
    RenderBundleEncoder(id <MTLIndirectCommandBuffer>);

    id <MTLIndirectCommandBuffer> m_indirectCommandBuffer { nil };
};

} // namespace WebGPU

struct WGPURenderBundleEncoderImpl {
    Ref<WebGPU::RenderBundleEncoder> renderBundleEncoder;
};
