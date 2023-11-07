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
#import <wtf/HashMap.h>
#import <wtf/Ref.h>
#import <wtf/RefCounted.h>
#import <wtf/Vector.h>

struct WGPURenderBundleEncoderImpl {
};

@interface RenderBundleICBWithResources : NSObject

- (instancetype)initWithICB:(id<MTLIndirectCommandBuffer>)icb pipelineState:(id<MTLRenderPipelineState>)pipelineState depthStencilState:(id<MTLDepthStencilState>)depthStencilState cullMode:(MTLCullMode)cullMode frontFace:(MTLWinding)frontFace depthClipMode:(MTLDepthClipMode)depthClipMode depthBias:(float)depthBias depthBiasSlopeScale:(float)depthBiasSlopeScale depthBiasClamp:(float)depthBiasClamp NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@property (readonly, nonatomic) id<MTLIndirectCommandBuffer> indirectCommandBuffer;
@property (readonly, nonatomic) id<MTLRenderPipelineState> currentPipelineState;
@property (readonly, nonatomic) id<MTLDepthStencilState> depthStencilState;
@property (readonly, nonatomic) MTLCullMode cullMode;
@property (readonly, nonatomic) MTLWinding frontFace;
@property (readonly, nonatomic) MTLDepthClipMode depthClipMode;
@property (readonly, nonatomic) float depthBias;
@property (readonly, nonatomic) float depthBiasSlopeScale;
@property (readonly, nonatomic) float depthBiasClamp;

- (Vector<WebGPU::BindableResources>*)resources;
@end

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
    void setBindGroup(uint32_t groupIndex, const BindGroup&, std::optional<Vector<uint32_t>>&& dynamicOffsets);
    void setIndexBuffer(const Buffer&, WGPUIndexFormat, uint64_t offset, uint64_t size);
    void setPipeline(const RenderPipeline&);
    void setVertexBuffer(uint32_t slot, const Buffer&, uint64_t offset, uint64_t size);
    void setLabel(String&&);

    Device& device() const { return m_device; }

    bool isValid() const { return m_indirectCommandBuffer; }
    void replayCommands(id<MTLRenderCommandEncoder> commmandEncoder);

private:
    RenderBundleEncoder(MTLIndirectCommandBufferDescriptor*, Device&);
    RenderBundleEncoder(Device&);

    bool validatePopDebugGroup() const;
    id<MTLIndirectRenderCommand> currentRenderCommand();

    void makeInvalid() { m_indirectCommandBuffer = nil; }
    void executePreDrawCommands();
    void endCurrentICB();
    void addResource(RenderBundle::ResourcesContainer*, id<MTLResource>, ResourceUsageAndRenderStage*);
    void addResource(RenderBundle::ResourcesContainer*, id<MTLResource>, MTLRenderStages);
    bool icbNeedsToBeSplit(const RenderPipeline& a, const RenderPipeline& b);
    void finalizeRenderCommand();

    id<MTLIndirectCommandBuffer> m_indirectCommandBuffer { nil };
    MTLIndirectCommandBufferDescriptor *m_icbDescriptor { nil };

    uint64_t m_debugGroupStackSize { 0 };
    uint64_t m_currentCommandIndex { 0 };
    id<MTLBuffer> m_indexBuffer { nil };
    id<MTLRenderPipelineState> m_currentPipelineState { nil };
    id<MTLDepthStencilState> m_depthStencilState { nil };
    MTLCullMode m_cullMode { MTLCullModeNone };
    MTLWinding m_frontFace { MTLWindingClockwise };
    MTLDepthClipMode m_depthClipMode { MTLDepthClipModeClip };
    float m_depthBias { 0 };
    float m_depthBiasSlopeScale { 0 };
    float m_depthBiasClamp { 0 };

    MTLPrimitiveType m_primitiveType { MTLPrimitiveTypeTriangle };
    MTLIndexType m_indexType { MTLIndexTypeUInt16 };
    NSUInteger m_indexBufferOffset { 0 };
    Vector<WTF::Function<bool(void)>> m_recordedCommands;
    NSMapTable<id<MTLResource>, ResourceUsageAndRenderStage*>* m_resources;
    struct BufferAndOffset {
        id<MTLBuffer> buffer { nil };
        uint64_t offset { 0 };
        uint32_t dynamicOffsetCount { 0 };
        const uint32_t* dynamicOffsets { nullptr };
    };
    Vector<BufferAndOffset> m_vertexBuffers;
    Vector<BufferAndOffset> m_fragmentBuffers;
    const Ref<Device> m_device;
    const RenderPipeline* m_pipeline { nullptr };
    using BindGroupDynamicOffsetsContainer = HashMap<uint32_t, Vector<uint32_t>, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;
    std::optional<BindGroupDynamicOffsetsContainer> m_bindGroupDynamicOffsets;
    NSMutableArray<RenderBundleICBWithResources*> *m_icbArray;
    id<MTLBuffer> m_dynamicOffsetsVertexBuffer { nil };
    id<MTLBuffer> m_dynamicOffsetsFragmentBuffer { nil };
    uint64_t m_vertexDynamicOffset { 0 };
    uint64_t m_fragmentDynamicOffset { 0 };

    id<MTLRenderCommandEncoder> m_commandEncoder { nil };
    id<MTLIndirectRenderCommand> m_currentCommand { nil };
    bool m_requiresCommandReplay { false };
    bool m_requiresMetalWorkaround { true };
};

} // namespace WebGPU
