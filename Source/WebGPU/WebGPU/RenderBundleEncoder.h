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

namespace WebGPU {
class RenderPipeline;
}

struct WGPURenderBundleEncoderImpl {
};

@interface RenderBundleICBWithResources : NSObject

- (instancetype)initWithICB:(id<MTLIndirectCommandBuffer>)icb pipelineState:(id<MTLRenderPipelineState>)pipelineState depthStencilState:(id<MTLDepthStencilState>)depthStencilState cullMode:(MTLCullMode)cullMode frontFace:(MTLWinding)frontFace depthClipMode:(MTLDepthClipMode)depthClipMode depthBias:(float)depthBias depthBiasSlopeScale:(float)depthBiasSlopeScale depthBiasClamp:(float)depthBiasClamp fragmentDynamicOffsetsBuffer:(id<MTLBuffer>)fragmentDynamicOffsetsBuffer pipeline:(const WebGPU::RenderPipeline*)pipeline NS_DESIGNATED_INITIALIZER;
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
@property (readonly, nonatomic) id<MTLBuffer> fragmentDynamicOffsetsBuffer;
@property (readonly, nonatomic) const WebGPU::RenderPipeline* pipeline;

- (Vector<WebGPU::BindableResources>*)resources;
@end

namespace WebGPU {

class BindGroup;
class Buffer;
class Device;
class RenderBundle;
class RenderPipeline;
class TextureView;

// https://gpuweb.github.io/gpuweb/#gpurenderbundleencoder
class RenderBundleEncoder : public WGPURenderBundleEncoderImpl, public RefCounted<RenderBundleEncoder>, public CommandsMixin {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RenderBundleEncoder> create(MTLIndirectCommandBufferDescriptor *indirectCommandBufferDescriptor, const WGPURenderBundleEncoderDescriptor& descriptor, Device& device)
    {
        return adoptRef(*new RenderBundleEncoder(indirectCommandBufferDescriptor, descriptor, device));
    }
    static Ref<RenderBundleEncoder> createInvalid(Device& device, NSString* errorString)
    {
        return adoptRef(*new RenderBundleEncoder(device, errorString));
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
    void setVertexBuffer(uint32_t slot, const Buffer*, uint64_t offset, uint64_t size);
    void setLabel(String&&);

    bool isValid() const { return m_indirectCommandBuffer; }
    void replayCommands(RenderPassEncoder&);

    static constexpr auto startIndexForFragmentDynamicOffsets = 3;
    static constexpr uint32_t defaultSampleMask = UINT32_MAX;

    bool validateDepthStencilState(bool depthReadOnly, bool stencilReadOnly) const;
    Device& device() const { return m_device; }

private:
    RenderBundleEncoder(MTLIndirectCommandBufferDescriptor*, const WGPURenderBundleEncoderDescriptor&, Device&);
    RenderBundleEncoder(Device&, NSString*);

    bool validatePopDebugGroup() const;
    id<MTLIndirectRenderCommand> currentRenderCommand();

    void makeInvalid(NSString* = nil);
    bool executePreDrawCommands();
    void endCurrentICB();
    void addResource(RenderBundle::ResourcesContainer*, id<MTLResource>, ResourceUsageAndRenderStage*);
    void addResource(RenderBundle::ResourcesContainer*, id<MTLResource>, MTLRenderStages, const BindGroupEntryUsageData::Resource&);
    void addResource(RenderBundle::ResourcesContainer*, id<MTLResource>, MTLRenderStages);
    bool icbNeedsToBeSplit(const RenderPipeline& a, const RenderPipeline& b);
    void finalizeRenderCommand();
    bool validToEncodeCommand() const;
    bool returnIfEncodingIsFinished(NSString* errorString);
    bool runIndexBufferValidation(uint32_t firstInstance, uint32_t instanceCount);
    bool runVertexBufferValidation(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
    NSString* errorValidatingDraw() const;
    NSString* errorValidatingDrawIndexed() const;
    uint32_t maxVertexBufferIndex() const;
    uint32_t maxBindGroupIndex() const;
    void recordCommand(WTF::Function<bool(void)>&&);

    const Ref<Device> m_device;
    WeakPtr<Buffer> m_indexBuffer;
    MTLIndexType m_indexType { MTLIndexTypeUInt16 };
    NSUInteger m_indexBufferOffset { 0 };
    NSUInteger m_indexBufferSize { 0 };
    WeakPtr<RenderPipeline> m_pipeline;
    uint32_t m_maxVertexBufferSlot { 0 };
    uint32_t m_maxBindGroupSlot { 0 };
    using UtilizedBufferIndicesContainer = HashMap<uint32_t, uint64_t, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;
    UtilizedBufferIndicesContainer m_utilizedBufferIndices;

    id<MTLIndirectCommandBuffer> m_indirectCommandBuffer { nil };
    MTLIndirectCommandBufferDescriptor *m_icbDescriptor { nil };

    uint64_t m_debugGroupStackSize { 0 };
    uint64_t m_currentCommandIndex { 0 };
    id<MTLRenderPipelineState> m_currentPipelineState { nil };
    id<MTLDepthStencilState> m_depthStencilState { nil };
    MTLCullMode m_cullMode { MTLCullModeNone };
    MTLWinding m_frontFace { MTLWindingClockwise };
    MTLDepthClipMode m_depthClipMode { MTLDepthClipModeClip };
    float m_depthBias { 0 };
    float m_depthBiasSlopeScale { 0 };
    float m_depthBiasClamp { 0 };

    MTLPrimitiveType m_primitiveType { MTLPrimitiveTypeTriangle };
    Vector<WTF::Function<bool(void)>> m_recordedCommands;
    NSMapTable<id<MTLResource>, ResourceUsageAndRenderStage*>* m_resources;
    struct BufferAndOffset {
        id<MTLBuffer> buffer { nil };
        uint64_t offset { 0 };
        uint32_t dynamicOffsetCount { 0 };
        const uint32_t* dynamicOffsets { nullptr };
        uint64_t size { 0 };
    };
    Vector<BufferAndOffset> m_vertexBuffers;
    Vector<BufferAndOffset> m_fragmentBuffers;
    using BindGroupDynamicOffsetsContainer = HashMap<uint32_t, Vector<uint32_t>, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;
    std::optional<BindGroupDynamicOffsetsContainer> m_bindGroupDynamicOffsets;
    HashMap<uint32_t, WeakPtr<BindGroup>, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> m_bindGroups;
    NSMutableArray<RenderBundleICBWithResources*> *m_icbArray;
    id<MTLBuffer> m_dynamicOffsetsVertexBuffer { nil };
    id<MTLBuffer> m_dynamicOffsetsFragmentBuffer { nil };
    uint64_t m_vertexDynamicOffset { 0 };
    uint64_t m_fragmentDynamicOffset { 0 };

    RenderPassEncoder* m_renderPassEncoder { nullptr };
    id<MTLIndirectRenderCommand> m_currentCommand { nil };
    WGPURenderBundleEncoderDescriptor m_descriptor;
    Vector<WGPUTextureFormat> m_descriptorColorFormats;
    NSString* m_lastErrorString { nil };
    bool m_requiresCommandReplay { false };
    bool m_requiresMetalWorkaround { true };
    bool m_finished { false };
    uint32_t m_sampleMask { defaultSampleMask };
};

} // namespace WebGPU
