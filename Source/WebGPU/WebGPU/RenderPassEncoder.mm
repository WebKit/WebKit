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

#import "config.h"
#import "RenderPassEncoder.h"

#import "APIConversions.h"
#import "BindGroup.h"
#import "Buffer.h"
#import "CommandEncoder.h"
#import "QuerySet.h"
#import "RenderBundle.h"
#import "RenderPipeline.h"
#import <wtf/StdLibExtras.h>

namespace WebGPU {

RenderPassEncoder::RenderPassEncoder(id<MTLRenderCommandEncoder> renderCommandEncoder, const WGPURenderPassDescriptor& descriptor, NSUInteger visibilityResultBufferSize, bool depthReadOnly, bool stencilReadOnly, CommandEncoder& parentEncoder, id<MTLBuffer> visibilityResultBuffer, Device& device)
    : m_renderCommandEncoder(renderCommandEncoder)
    , m_device(device)
    , m_visibilityResultBufferSize(visibilityResultBufferSize)
    , m_depthReadOnly(depthReadOnly)
    , m_stencilReadOnly(stencilReadOnly)
    , m_parentEncoder(&parentEncoder)
    , m_visibilityResultBuffer(visibilityResultBuffer)
{
    m_parentEncoder->lock(true);

    if (m_device->baseCapabilities().counterSamplingAPI == HardwareCapabilities::BaseCapabilities::CounterSamplingAPI::CommandBoundary) {
        if (descriptor.timestampWrites) {
            const auto& timestampWrite = *descriptor.timestampWrites;
            [m_renderCommandEncoder sampleCountersInBuffer:fromAPI(timestampWrite.querySet).counterSampleBuffer() atSampleIndex:timestampWrite.beginningOfPassWriteIndex withBarrier:NO];
            m_pendingTimestampWrites.append({ fromAPI(timestampWrite.querySet), timestampWrite.endOfPassWriteIndex });
        }
    }

    m_attachmentsToClear = [NSMutableDictionary dictionary];
    m_allColorAttachments = [NSMutableDictionary dictionary];
    for (uint32_t i = 0; i < descriptor.colorAttachmentCount; ++i) {
        const auto& attachment = descriptor.colorAttachments[i];
        if (!attachment.view)
            continue;

        auto& texture = fromAPI(attachment.view);
        texture.setPreviouslyCleared();
        m_renderTargetWidth = texture.width();
        m_renderTargetHeight = texture.height();

        id<MTLTexture> textureToClear = texture.texture();
        TextureAndClearColor *textureWithResolve = [[TextureAndClearColor alloc] initWithTexture:textureToClear];
        if (attachment.storeOp != WGPUStoreOp_Discard) {
            auto& c = attachment.clearValue;
            textureWithResolve.clearColor = MTLClearColorMake(c.r, c.g, c.b, c.a);
        } else
            [m_attachmentsToClear setObject:textureWithResolve forKey:@(i)];

        [m_allColorAttachments setObject:textureWithResolve forKey:@(i)];
    }

    if (const auto* attachment = descriptor.depthStencilAttachment) {
        m_depthClearValue = attachment->depthStoreOp == WGPUStoreOp_Discard ? 0 : attachment->depthClearValue;
        auto& textureView = fromAPI(attachment->view);
        textureView.setPreviouslyCleared();
        id<MTLTexture> depthTexture = textureView.texture();
        if (!Device::isStencilOnlyFormat(depthTexture.pixelFormat)) {
            m_clearDepthAttachment = attachment->depthStoreOp == WGPUStoreOp_Discard;
            m_depthStencilAttachmentToClear = depthTexture;
        }

        m_stencilClearValue = attachment->stencilStoreOp == WGPUStoreOp_Discard ? 0 : attachment->stencilClearValue;
        if (Texture::stencilOnlyAspectMetalFormat(textureView.descriptor().format)) {
            m_clearStencilAttachment = attachment->stencilStoreOp == WGPUStoreOp_Discard;
            m_depthStencilAttachmentToClear = textureView.texture();
        }
    }
}

RenderPassEncoder::RenderPassEncoder(Device& device)
    : m_device(device)
{
}

RenderPassEncoder::~RenderPassEncoder()
{
    [m_renderCommandEncoder endEncoding];
}

void RenderPassEncoder::beginOcclusionQuery(uint32_t queryIndex)
{
    queryIndex *= sizeof(uint64_t);
    if (queryIndex < m_visibilityResultBufferSize) {
        m_visibilityResultBufferOffset = queryIndex;
        [m_renderCommandEncoder setVisibilityResultMode:MTLVisibilityResultModeCounting offset:queryIndex];
        m_queryBufferIndicesToClear.add(m_visibilityResultBufferOffset);
    }
}

static void setViewportMinMaxDepthIntoBuffer(auto& fragmentDynamicOffsets, float minDepth, float maxDepth)
{
    static_assert(sizeof(fragmentDynamicOffsets[0]) == sizeof(minDepth), "expect dynamic offsets container to have matching size to depth values");
    static_assert(RenderBundleEncoder::startIndexForFragmentDynamicOffsets > 1, "code path assumes value is 2 or more");
    if (fragmentDynamicOffsets.size() < RenderBundleEncoder::startIndexForFragmentDynamicOffsets)
        fragmentDynamicOffsets.grow(RenderBundleEncoder::startIndexForFragmentDynamicOffsets);

    using destType = typename std::remove_reference<decltype(fragmentDynamicOffsets[0])>::type;
    fragmentDynamicOffsets[0] = bitwise_cast<destType>(minDepth);
    fragmentDynamicOffsets[1] = bitwise_cast<destType>(maxDepth);
}

void RenderPassEncoder::executePreDrawCommands()
{
    if (!m_pipeline)
        return;

    m_issuedDrawCall = true;
    m_queryBufferIndicesToClear.remove(m_visibilityResultBufferOffset);

    for (auto& kvp : m_bindGroupDynamicOffsets) {
        auto& pipelineLayout = m_pipeline->pipelineLayout();
        auto bindGroupIndex = kvp.key;

        auto* pvertexOffsets = pipelineLayout.vertexOffsets(bindGroupIndex, kvp.value);
        if (pvertexOffsets && pvertexOffsets->size()) {
            auto& vertexOffsets = *pvertexOffsets;
            auto startIndex = pipelineLayout.vertexOffsetForBindGroup(bindGroupIndex);
            RELEASE_ASSERT(vertexOffsets.size() <= m_vertexDynamicOffsets.size() + startIndex);
            memcpy(&m_vertexDynamicOffsets[startIndex], &vertexOffsets[0], sizeof(vertexOffsets[0]) * vertexOffsets.size());
        }

        auto* pfragmentOffsets = pipelineLayout.fragmentOffsets(bindGroupIndex, kvp.value);
        if (pfragmentOffsets && pfragmentOffsets->size()) {
            auto& fragmentOffsets = *pfragmentOffsets;
            auto startIndex = pipelineLayout.fragmentOffsetForBindGroup(bindGroupIndex);
            RELEASE_ASSERT(fragmentOffsets.size() <= m_fragmentDynamicOffsets.size() + startIndex);
            memcpy(&m_fragmentDynamicOffsets[startIndex + RenderBundleEncoder::startIndexForFragmentDynamicOffsets], &fragmentOffsets[0], sizeof(fragmentOffsets[0]) * fragmentOffsets.size());
        }
    }

    if (m_vertexDynamicOffsets.size())
        [m_renderCommandEncoder setVertexBytes:&m_vertexDynamicOffsets[0] length:m_vertexDynamicOffsets.size() * sizeof(m_vertexDynamicOffsets[0]) atIndex:m_device->maxBuffersPlusVertexBuffersForVertexStage()];

    setViewportMinMaxDepthIntoBuffer(m_fragmentDynamicOffsets, m_minDepth, m_maxDepth);
    ASSERT(m_fragmentDynamicOffsets.size());
    [m_renderCommandEncoder setFragmentBytes:&m_fragmentDynamicOffsets[0] length:m_fragmentDynamicOffsets.size() * sizeof(m_fragmentDynamicOffsets[0]) atIndex:m_device->maxBuffersForFragmentStage()];

    m_bindGroupDynamicOffsets.clear();
}

void RenderPassEncoder::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    if (!instanceCount || !vertexCount)
        return;

    // FIXME: validation according to
    // https://gpuweb.github.io/gpuweb/#dom-gpurendercommandsmixin-draw
    executePreDrawCommands();
    [m_renderCommandEncoder
        drawPrimitives:m_primitiveType
        vertexStart:firstVertex
        vertexCount:vertexCount
        instanceCount:instanceCount
        baseInstance:firstInstance];
}

void RenderPassEncoder::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
    if (!instanceCount || !indexCount)
        return;

    executePreDrawCommands();
    auto firstIndexOffsetInBytes = firstIndex * (m_indexType == MTLIndexTypeUInt16 ? sizeof(uint16_t) : sizeof(uint32_t));
    [m_renderCommandEncoder drawIndexedPrimitives:m_primitiveType indexCount:indexCount indexType:m_indexType indexBuffer:m_indexBuffer indexBufferOffset:(m_indexBufferOffset + firstIndexOffsetInBytes) instanceCount:instanceCount baseVertex:baseVertex baseInstance:firstInstance];
}

void RenderPassEncoder::drawIndexedIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset)
{
    if (!m_indexBuffer.length || !indirectBuffer.buffer().length)
        return;

    executePreDrawCommands();
    [m_renderCommandEncoder drawIndexedPrimitives:m_primitiveType indexType:m_indexType indexBuffer:m_indexBuffer indexBufferOffset:m_indexBufferOffset indirectBuffer:indirectBuffer.buffer() indirectBufferOffset:indirectOffset];
}

void RenderPassEncoder::drawIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset)
{
    executePreDrawCommands();
    [m_renderCommandEncoder drawPrimitives:m_primitiveType indirectBuffer:indirectBuffer.buffer() indirectBufferOffset:indirectOffset];
}

void RenderPassEncoder::endOcclusionQuery()
{
    [m_renderCommandEncoder setVisibilityResultMode:MTLVisibilityResultModeDisabled offset:m_visibilityResultBufferOffset];
}

void RenderPassEncoder::endPass()
{
    if (!m_parentEncoder) {
        ASSERT(!m_renderCommandEncoder);
        return;
    }

    if (m_debugGroupStackSize || !isValid()) {
        m_parentEncoder->makeInvalid();
        return;
    }

    ASSERT(m_pendingTimestampWrites.isEmpty() || m_device->baseCapabilities().counterSamplingAPI == HardwareCapabilities::BaseCapabilities::CounterSamplingAPI::CommandBoundary);
    for (const auto& pendingTimestampWrite : m_pendingTimestampWrites)
        [m_renderCommandEncoder sampleCountersInBuffer:pendingTimestampWrite.querySet->counterSampleBuffer() atSampleIndex:pendingTimestampWrite.queryIndex withBarrier:NO];
    m_pendingTimestampWrites.clear();
    [m_renderCommandEncoder endEncoding];
    m_renderCommandEncoder = nil;

    m_parentEncoder->lock(false);

    if (m_attachmentsToClear.count || (m_depthStencilAttachmentToClear && (m_clearDepthAttachment || m_clearStencilAttachment))) {
        if (m_depthStencilAttachmentToClear && !m_issuedDrawCall) {
            auto pixelFormat = m_depthStencilAttachmentToClear.pixelFormat;
            m_clearDepthAttachment = !Device::isStencilOnlyFormat(pixelFormat);
            m_clearStencilAttachment = pixelFormat == MTLPixelFormatDepth32Float_Stencil8 || pixelFormat == MTLPixelFormatStencil8 || pixelFormat == MTLPixelFormatX32_Stencil8;
        }
        m_parentEncoder->runClearEncoder(m_issuedDrawCall ? m_attachmentsToClear : m_allColorAttachments, m_depthStencilAttachmentToClear, m_clearDepthAttachment, m_clearStencilAttachment, m_depthClearValue, m_stencilClearValue);
    }

    if (m_queryBufferIndicesToClear.size()) {
        id<MTLBlitCommandEncoder> blitCommandEncoder = m_parentEncoder->ensureBlitCommandEncoder();
        for (auto& offset : m_queryBufferIndicesToClear)
            [blitCommandEncoder fillBuffer:m_visibilityResultBuffer range:NSMakeRange(static_cast<NSUInteger>(offset), sizeof(uint64_t)) value:0];

        m_queryBufferIndicesToClear.clear();
        m_parentEncoder->finalizeBlitCommandEncoder();
    }

}

void RenderPassEncoder::executeBundles(Vector<std::reference_wrapper<RenderBundle>>&& bundles)
{
    m_queryBufferIndicesToClear.remove(m_visibilityResultBufferOffset);

    for (auto& bundle : bundles) {
        auto& renderBundle = bundle.get();
        renderBundle.updateMinMaxDepths(m_minDepth, m_maxDepth);

        for (RenderBundleICBWithResources* icb in renderBundle.renderBundlesResources()) {
            if (id<MTLDepthStencilState> depthStencilState = icb.depthStencilState)
                [m_renderCommandEncoder setDepthStencilState:depthStencilState];
            [m_renderCommandEncoder setCullMode:icb.cullMode];
            [m_renderCommandEncoder setFrontFacingWinding:icb.frontFace];
            [m_renderCommandEncoder setDepthClipMode:icb.depthClipMode];
            [m_renderCommandEncoder setDepthBias:icb.depthBias slopeScale:icb.depthBiasSlopeScale clamp:icb.depthBiasClamp];

            for (const auto& resource : *icb.resources) {
                if (resource.renderStages & (MTLRenderStageVertex | MTLRenderStageFragment))
                    [m_renderCommandEncoder useResources:&resource.mtlResources[0] count:resource.mtlResources.size() usage:resource.usage stages:resource.renderStages];
            }

            id<MTLIndirectCommandBuffer> indirectCommandBuffer = icb.indirectCommandBuffer;
            m_issuedDrawCall = true;
            [m_renderCommandEncoder executeCommandsInBuffer:indirectCommandBuffer withRange:NSMakeRange(0, indirectCommandBuffer.size)];
        }

        renderBundle.replayCommands(m_renderCommandEncoder);
    }
}

void RenderPassEncoder::insertDebugMarker(String&& markerLabel)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-insertdebugmarker

    if (!prepareTheEncoderState())
        return;

    [m_renderCommandEncoder insertDebugSignpost:markerLabel];
}

bool RenderPassEncoder::validatePopDebugGroup() const
{
    if (!m_debugGroupStackSize)
        return false;

    return true;
}

void RenderPassEncoder::makeInvalid()
{
    [m_renderCommandEncoder endEncoding];
    m_renderCommandEncoder = nil;
}

void RenderPassEncoder::popDebugGroup()
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-popdebuggroup

    if (!prepareTheEncoderState())
        return;

    if (!validatePopDebugGroup()) {
        makeInvalid();
        return;
    }

    --m_debugGroupStackSize;
    [m_renderCommandEncoder popDebugGroup];
}

void RenderPassEncoder::pushDebugGroup(String&& groupLabel)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-pushdebuggroup

    if (!prepareTheEncoderState())
        return;

    ++m_debugGroupStackSize;
    [m_renderCommandEncoder pushDebugGroup:groupLabel];
}

void RenderPassEncoder::setBindGroup(uint32_t groupIndex, const BindGroup& group, uint32_t dynamicOffsetCount, const uint32_t* dynamicOffsets)
{
    if (dynamicOffsetCount)
        m_bindGroupDynamicOffsets.add(groupIndex, Vector<uint32_t>(dynamicOffsets, dynamicOffsetCount));

    for (const auto& resource : group.resources()) {
        if (resource.renderStages & (MTLRenderStageVertex | MTLRenderStageFragment))
            [m_renderCommandEncoder useResources:&resource.mtlResources[0] count:resource.mtlResources.size() usage:resource.usage stages:resource.renderStages];
    }

    [m_renderCommandEncoder setVertexBuffer:group.vertexArgumentBuffer() offset:0 atIndex:m_device->vertexBufferIndexForBindGroup(groupIndex)];
    [m_renderCommandEncoder setFragmentBuffer:group.fragmentArgumentBuffer() offset:0 atIndex:groupIndex];
}

void RenderPassEncoder::setBlendConstant(const WGPUColor& color)
{
    [m_renderCommandEncoder setBlendColorRed:color.r green:color.g blue:color.b alpha:color.a];
}

void RenderPassEncoder::setIndexBuffer(const Buffer& buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size)
{
    m_indexBuffer = buffer.buffer();
    m_indexType = format == WGPUIndexFormat_Uint32 ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16;
    m_indexBufferOffset = offset;
    UNUSED_PARAM(size);
}

void RenderPassEncoder::setPipeline(const RenderPipeline& pipeline)
{
    // FIXME: validation according to
    // https://gpuweb.github.io/gpuweb/#dom-gpurendercommandsmixin-setpipeline.
    if (!pipeline.isValid()) {
        m_device->generateAValidationError("invalid RenderPipeline in RenderPassEncoder.setPipeline"_s);
        makeInvalid();
        return;
    }

    if (!pipeline.validateDepthStencilState(m_depthReadOnly, m_stencilReadOnly))
        return;

    m_primitiveType = pipeline.primitiveType();
    m_pipeline = &pipeline;

    m_vertexDynamicOffsets.resize(pipeline.pipelineLayout().sizeOfVertexDynamicOffsets());
    m_fragmentDynamicOffsets.resize(pipeline.pipelineLayout().sizeOfFragmentDynamicOffsets() + RenderBundleEncoder::startIndexForFragmentDynamicOffsets);

    if (pipeline.renderPipelineState())
        [m_renderCommandEncoder setRenderPipelineState:pipeline.renderPipelineState()];
    if (pipeline.depthStencilState())
        [m_renderCommandEncoder setDepthStencilState:pipeline.depthStencilState()];
    [m_renderCommandEncoder setCullMode:pipeline.cullMode()];
    [m_renderCommandEncoder setFrontFacingWinding:pipeline.frontFace()];
    [m_renderCommandEncoder setDepthClipMode:pipeline.depthClipMode()];
    [m_renderCommandEncoder setDepthBias:pipeline.depthBias() slopeScale:pipeline.depthBiasSlopeScale() clamp:pipeline.depthBiasClamp()];
    if (m_fragmentDynamicOffsets.size() < RenderBundleEncoder::startIndexForFragmentDynamicOffsets)
        m_fragmentDynamicOffsets.grow(RenderBundleEncoder::startIndexForFragmentDynamicOffsets);
    static_assert(RenderBundleEncoder::startIndexForFragmentDynamicOffsets > 2, "code path assumes value is greater than 2");
    m_fragmentDynamicOffsets[2] = pipeline.sampleMask();
}

void RenderPassEncoder::setScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    // FIXME: Validate according to https://www.w3.org/TR/webgpu/#dom-gpurenderpassencoder-setscissorrect
    [m_renderCommandEncoder setScissorRect: { x, y, std::min(width, m_renderTargetWidth), std::min(height, m_renderTargetHeight) } ];
}

void RenderPassEncoder::setStencilReference(uint32_t reference)
{
    [m_renderCommandEncoder setStencilReferenceValue:(reference & 0xFF)];
}

void RenderPassEncoder::setVertexBuffer(uint32_t slot, const Buffer& buffer, uint64_t offset, uint64_t size)
{
    UNUSED_PARAM(size);
    [m_renderCommandEncoder setVertexBuffer:buffer.buffer() offset:offset atIndex:slot];
}

void RenderPassEncoder::setViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
{
    m_minDepth = minDepth;
    m_maxDepth = maxDepth;
    [m_renderCommandEncoder setViewport: { x, y, width, height, minDepth, maxDepth } ];
}

void RenderPassEncoder::setLabel(String&& label)
{
    m_renderCommandEncoder.label = label;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuRenderPassEncoderReference(WGPURenderPassEncoder renderPassEncoder)
{
    WebGPU::fromAPI(renderPassEncoder).ref();
}

void wgpuRenderPassEncoderRelease(WGPURenderPassEncoder renderPassEncoder)
{
    WebGPU::fromAPI(renderPassEncoder).deref();
}

void wgpuRenderPassEncoderBeginOcclusionQuery(WGPURenderPassEncoder renderPassEncoder, uint32_t queryIndex)
{
    WebGPU::fromAPI(renderPassEncoder).beginOcclusionQuery(queryIndex);
}

void wgpuRenderPassEncoderDraw(WGPURenderPassEncoder renderPassEncoder, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    WebGPU::fromAPI(renderPassEncoder).draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void wgpuRenderPassEncoderDrawIndexed(WGPURenderPassEncoder renderPassEncoder, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
    WebGPU::fromAPI(renderPassEncoder).drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

void wgpuRenderPassEncoderDrawIndexedIndirect(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset)
{
    WebGPU::fromAPI(renderPassEncoder).drawIndexedIndirect(WebGPU::fromAPI(indirectBuffer), indirectOffset);
}

void wgpuRenderPassEncoderDrawIndirect(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset)
{
    WebGPU::fromAPI(renderPassEncoder).drawIndirect(WebGPU::fromAPI(indirectBuffer), indirectOffset);
}

void wgpuRenderPassEncoderEndOcclusionQuery(WGPURenderPassEncoder renderPassEncoder)
{
    WebGPU::fromAPI(renderPassEncoder).endOcclusionQuery();
}

void wgpuRenderPassEncoderEnd(WGPURenderPassEncoder renderPassEncoder)
{
    WebGPU::fromAPI(renderPassEncoder).endPass();
}

void wgpuRenderPassEncoderExecuteBundles(WGPURenderPassEncoder renderPassEncoder, size_t bundlesCount, const WGPURenderBundle* bundles)
{
    Vector<std::reference_wrapper<WebGPU::RenderBundle>> bundlesToForward;
    for (uint32_t i = 0; i < bundlesCount; ++i)
        bundlesToForward.append(WebGPU::fromAPI(bundles[i]));
    WebGPU::fromAPI(renderPassEncoder).executeBundles(WTFMove(bundlesToForward));
}

void wgpuRenderPassEncoderInsertDebugMarker(WGPURenderPassEncoder renderPassEncoder, const char* markerLabel)
{
    WebGPU::fromAPI(renderPassEncoder).insertDebugMarker(WebGPU::fromAPI(markerLabel));
}

void wgpuRenderPassEncoderPopDebugGroup(WGPURenderPassEncoder renderPassEncoder)
{
    WebGPU::fromAPI(renderPassEncoder).popDebugGroup();
}

void wgpuRenderPassEncoderPushDebugGroup(WGPURenderPassEncoder renderPassEncoder, const char* groupLabel)
{
    WebGPU::fromAPI(renderPassEncoder).pushDebugGroup(WebGPU::fromAPI(groupLabel));
}

void wgpuRenderPassEncoderSetBindGroup(WGPURenderPassEncoder renderPassEncoder, uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, const uint32_t* dynamicOffsets)
{
    WebGPU::fromAPI(renderPassEncoder).setBindGroup(groupIndex, WebGPU::fromAPI(group), dynamicOffsetCount, dynamicOffsets);
}

void wgpuRenderPassEncoderSetBlendConstant(WGPURenderPassEncoder renderPassEncoder, const WGPUColor* color)
{
    WebGPU::fromAPI(renderPassEncoder).setBlendConstant(*color);
}

void wgpuRenderPassEncoderSetIndexBuffer(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size)
{
    WebGPU::fromAPI(renderPassEncoder).setIndexBuffer(WebGPU::fromAPI(buffer), format, offset, size);
}

void wgpuRenderPassEncoderSetPipeline(WGPURenderPassEncoder renderPassEncoder, WGPURenderPipeline pipeline)
{
    WebGPU::fromAPI(renderPassEncoder).setPipeline(WebGPU::fromAPI(pipeline));
}

void wgpuRenderPassEncoderSetScissorRect(WGPURenderPassEncoder renderPassEncoder, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    WebGPU::fromAPI(renderPassEncoder).setScissorRect(x, y, width, height);
}

void wgpuRenderPassEncoderSetStencilReference(WGPURenderPassEncoder renderPassEncoder, uint32_t reference)
{
    WebGPU::fromAPI(renderPassEncoder).setStencilReference(reference);
}

void wgpuRenderPassEncoderSetVertexBuffer(WGPURenderPassEncoder renderPassEncoder, uint32_t slot, WGPUBuffer buffer, uint64_t offset, uint64_t size)
{
    WebGPU::fromAPI(renderPassEncoder).setVertexBuffer(slot, WebGPU::fromAPI(buffer), offset, size);
}

void wgpuRenderPassEncoderSetViewport(WGPURenderPassEncoder renderPassEncoder, float x, float y, float width, float height, float minDepth, float maxDepth)
{
    WebGPU::fromAPI(renderPassEncoder).setViewport(x, y, width, height, minDepth, maxDepth);
}

void wgpuRenderPassEncoderSetLabel(WGPURenderPassEncoder renderPassEncoder, const char* label)
{
    WebGPU::fromAPI(renderPassEncoder).setLabel(WebGPU::fromAPI(label));
}
