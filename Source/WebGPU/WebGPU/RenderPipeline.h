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

#pragma once

#import <wtf/FastMalloc.h>
#import <wtf/HashMap.h>
#import <wtf/HashTraits.h>
#import <wtf/Ref.h>
#import <wtf/RefCounted.h>
#import <wtf/WeakPtr.h>

struct WGPURenderPipelineImpl {
};

namespace WebGPU {

class BindGroupLayout;
class Device;
class PipelineLayout;

// https://gpuweb.github.io/gpuweb/#gpurenderpipeline
class RenderPipeline : public WGPURenderPipelineImpl, public RefCounted<RenderPipeline>, public CanMakeWeakPtr<RenderPipeline> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    struct BufferData {
        uint64_t stride { 0 };
        uint64_t lastStride { 0 };
        WGPUVertexStepMode stepMode { WGPUVertexStepMode_Vertex };
    };
    using RequiredBufferIndicesContainer = HashMap<uint32_t, BufferData, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;

    static Ref<RenderPipeline> create(id<MTLRenderPipelineState> renderPipelineState, MTLPrimitiveType primitiveType, std::optional<MTLIndexType> indexType, MTLWinding frontFace, MTLCullMode cullMode, MTLDepthClipMode depthClipMode, MTLDepthStencilDescriptor *depthStencilDescriptor, Ref<PipelineLayout>&& pipelineLayout, float depthBias, float depthBiasSlopeScale, float depthBiasClamp, uint32_t sampleMask, MTLRenderPipelineDescriptor* renderPipelineDescriptor, uint32_t colorAttachmentCount, const WGPURenderPipelineDescriptor& descriptor, RequiredBufferIndicesContainer&& requiredBufferIndices, Device& device)
    {
        return adoptRef(*new RenderPipeline(renderPipelineState, primitiveType, indexType, frontFace, cullMode, depthClipMode, depthStencilDescriptor, WTFMove(pipelineLayout), depthBias, depthBiasSlopeScale, depthBiasClamp, sampleMask, renderPipelineDescriptor, colorAttachmentCount, descriptor, WTFMove(requiredBufferIndices), device));
    }

    static Ref<RenderPipeline> createInvalid(Device& device)
    {
        return adoptRef(*new RenderPipeline(device));
    }

    ~RenderPipeline();

    RefPtr<BindGroupLayout> getBindGroupLayout(uint32_t groupIndex);
    void setLabel(String&&);

    bool isValid() const { return m_renderPipelineState; }

    id<MTLRenderPipelineState> renderPipelineState() const { return m_renderPipelineState; }
    id<MTLDepthStencilState> depthStencilState() const;
    bool validateDepthStencilState(bool depthReadOnly, bool stencilReadOnly) const;
    MTLPrimitiveType primitiveType() const { return m_primitiveType; }
    MTLWinding frontFace() const { return m_frontFace; }
    MTLCullMode cullMode() const { return m_cullMode; }
    MTLDepthClipMode depthClipMode() const { return m_clipMode; }
    MTLDepthStencilDescriptor *depthStencilDescriptor() const { return m_depthStencilDescriptor; }
    float depthBias() const { return m_depthBias; }
    float depthBiasSlopeScale() const { return m_depthBiasSlopeScale; }
    float depthBiasClamp() const { return m_depthBiasClamp; }
    uint32_t sampleMask() const { return m_sampleMask; }

    Device& device() const { return m_device; }
    PipelineLayout& pipelineLayout() const;
    bool colorDepthStencilTargetsMatch(const WGPURenderPassDescriptor&) const;
    bool validateRenderBundle(const WGPURenderBundleEncoderDescriptor&) const;
    bool writesDepth() const;
    bool writesStencil() const;

    const RequiredBufferIndicesContainer& requiredBufferIndices() const { return m_requiredBufferIndices; }
    WGPUPrimitiveTopology primitiveTopology() const;
    MTLIndexType stripIndexFormat() const;
    size_t vertexStageInBufferCount() const;

private:
    RenderPipeline(id<MTLRenderPipelineState>, MTLPrimitiveType, std::optional<MTLIndexType>, MTLWinding, MTLCullMode, MTLDepthClipMode, MTLDepthStencilDescriptor *, Ref<PipelineLayout>&&, float depthBias, float depthBiasSlopeScale, float depthBiasClamp, uint32_t sampleMask, MTLRenderPipelineDescriptor*, uint32_t colorAttachmentCount, const WGPURenderPipelineDescriptor&, RequiredBufferIndicesContainer&&, Device&);
    RenderPipeline(Device&);
    bool colorTargetsMatch(MTLRenderPassDescriptor*, uint32_t) const;
    bool depthAttachmentMatches(MTLRenderPassDepthAttachmentDescriptor*) const;
    bool stencilAttachmentMatches(MTLRenderPassStencilAttachmentDescriptor*) const;

    const id<MTLRenderPipelineState> m_renderPipelineState { nil };

    const Ref<Device> m_device;
    MTLPrimitiveType m_primitiveType;
    std::optional<MTLIndexType> m_indexType;
    MTLWinding m_frontFace;
    MTLCullMode m_cullMode;
    MTLDepthClipMode m_clipMode;
    float m_depthBias { 0 };
    float m_depthBiasSlopeScale { 0 };
    float m_depthBiasClamp { 0 };
    uint32_t m_sampleMask { UINT32_MAX };
    MTLRenderPipelineDescriptor* m_renderPipelineDescriptor { nil };
    uint32_t m_colorAttachmentCount { 0 };
    MTLDepthStencilDescriptor *m_depthStencilDescriptor { nil };
    id<MTLDepthStencilState> m_depthStencilState;
    RequiredBufferIndicesContainer m_requiredBufferIndices;
    Ref<PipelineLayout> m_pipelineLayout;
    WGPURenderPipelineDescriptor m_descriptor;
    WGPUDepthStencilState m_descriptorDepthStencil;
    WGPUFragmentState m_descriptorFragment;
    Vector<WGPUColorTargetState> m_descriptorTargets;
    bool m_writesStencil { false };
};

} // namespace WebGPU
