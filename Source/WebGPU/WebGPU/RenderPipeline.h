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
#import <wtf/HashMap.h>
#import <wtf/Ref.h>
#import <wtf/RefCounted.h>

struct WGPURenderPipelineImpl {
};

namespace WebGPU {

class BindGroupLayout;
class Device;
class PipelineLayout;

// https://gpuweb.github.io/gpuweb/#gpurenderpipeline
class RenderPipeline : public WGPURenderPipelineImpl, public RefCounted<RenderPipeline> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RenderPipeline> create(id<MTLRenderPipelineState> renderPipelineState, MTLPrimitiveType primitiveType, std::optional<MTLIndexType> indexType, MTLWinding frontFace, MTLCullMode cullMode, id<MTLDepthStencilState> depthStencilState, MTLRenderPipelineReflection *reflection, uint32_t vertexShaderInputBufferCount, Device& device)
    {
        return adoptRef(*new RenderPipeline(renderPipelineState, primitiveType, indexType, frontFace, cullMode, depthStencilState, reflection, vertexShaderInputBufferCount, device));
    }

    static Ref<RenderPipeline> create(id<MTLRenderPipelineState> renderPipelineState, MTLPrimitiveType primitiveType, std::optional<MTLIndexType> indexType, MTLWinding frontFace, MTLCullMode cullMode, id<MTLDepthStencilState> depthStencilState, const PipelineLayout &pipelineLayout, uint32_t vertexShaderInputBufferCount, Device& device)
    {
        return adoptRef(*new RenderPipeline(renderPipelineState, primitiveType, indexType, frontFace, cullMode, depthStencilState, pipelineLayout, vertexShaderInputBufferCount, device));
    }

    static Ref<RenderPipeline> createInvalid(Device& device)
    {
        return adoptRef(*new RenderPipeline(device));
    }

    ~RenderPipeline();

    BindGroupLayout* getBindGroupLayout(uint32_t groupIndex);
    void setLabel(String&&);

    bool isValid() const { return m_renderPipelineState; }

    id<MTLRenderPipelineState> renderPipelineState() const { return m_renderPipelineState; }
    id<MTLDepthStencilState> depthStencilState() const { return m_depthStencilState; }
    MTLPrimitiveType primitiveType() const { return m_primitiveType; }
    MTLWinding frontFace() const { return m_frontFace; }
    MTLCullMode cullMode() const { return m_cullMode; }

    Device& device() const { return m_device; }
    uint32_t vertexShaderInputBufferCount() const { return m_vertexShaderInputBufferCount; }

private:
    RenderPipeline(id<MTLRenderPipelineState>, MTLPrimitiveType, std::optional<MTLIndexType>, MTLWinding, MTLCullMode, id<MTLDepthStencilState>, MTLRenderPipelineReflection*, uint32_t, Device&);
    RenderPipeline(id<MTLRenderPipelineState>, MTLPrimitiveType, std::optional<MTLIndexType>, MTLWinding, MTLCullMode, id<MTLDepthStencilState>, const PipelineLayout&, uint32_t, Device&);
    RenderPipeline(Device&);

    const id<MTLRenderPipelineState> m_renderPipelineState { nil };

    const Ref<Device> m_device;
    HashMap<uint32_t, Ref<BindGroupLayout>> m_cachedBindGroupLayouts;
    MTLPrimitiveType m_primitiveType;
    std::optional<MTLIndexType> m_indexType;
    MTLWinding m_frontFace;
    MTLCullMode m_cullMode;
    id<MTLDepthStencilState> m_depthStencilState;
#if HAVE(METAL_BUFFER_BINDING_REFLECTION)
    MTLRenderPipelineReflection *m_reflection { nil };
#endif
    const PipelineLayout *m_pipelineLayout { nullptr };
    uint32_t m_vertexShaderInputBufferCount { 0 };
};

} // namespace WebGPU
