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

namespace WebGPU {

RenderPassEncoder::RenderPassEncoder(id<MTLRenderCommandEncoder> renderCommandEncoder, const WGPURenderPassDescriptor& descriptor, NSUInteger visibilityResultBufferSize, bool depthReadOnly, bool stencilReadOnly, Ref<CommandEncoder>&& commandEncoder, Device& device)
    : m_renderCommandEncoder(renderCommandEncoder)
    , m_device(device)
    , m_commandEncoder(WTFMove(commandEncoder))
    , m_visibilityResultBufferSize(visibilityResultBufferSize)
    , m_depthReadOnly(depthReadOnly)
    , m_stencilReadOnly(stencilReadOnly)
{
    if (m_device->baseCapabilities().counterSamplingAPI == HardwareCapabilities::BaseCapabilities::CounterSamplingAPI::CommandBoundary) {
        for (uint32_t i = 0; i < descriptor.timestampWriteCount; ++i) {
            const auto& timestampWrite = descriptor.timestampWrites[i];
            switch (timestampWrite.location) {
            case WGPURenderPassTimestampLocation_Beginning:
                [m_renderCommandEncoder sampleCountersInBuffer:fromAPI(timestampWrite.querySet).counterSampleBuffer() atSampleIndex:timestampWrite.queryIndex withBarrier:NO];
                break;
            case WGPURenderPassTimestampLocation_End:
                m_pendingTimestampWrites.append({ fromAPI(timestampWrite.querySet), timestampWrite.queryIndex });
                break;
            case WGPURenderPassTimestampLocation_Force32:
                ASSERT_NOT_REACHED();
                break;
            }
        }
    }

    if (descriptor.colorAttachmentCount) {
        const auto& attachment = descriptor.colorAttachments[0];
        id<MTLTexture> texture = fromAPI(attachment.view).texture();
        m_attachmentWidth = texture.width;
        m_attachmentHeight = texture.height;
    }
}

RenderPassEncoder::RenderPassEncoder(Ref<CommandEncoder>&& commandEncoder, Device& device)
    : m_device(device)
    , m_commandEncoder(WTFMove(commandEncoder))
{
}

RenderPassEncoder::~RenderPassEncoder()
{
    [m_renderCommandEncoder endEncoding];
}

static uint32_t occlusionQuerySetCount()
{
    return UINT32_MAX;
}

void RenderPassEncoder::beginOcclusionQuery(uint32_t queryIndex)
{
    if (!prepareTheEncoderState())
        return;

    if (queryIndex >= occlusionQuerySetCount() || m_occlusionQueryWrites.contains(queryIndex) || m_occlusionQueryActive) {
        makeInvalid();
        return;
    }

    m_occlusionQueryWrites.add(queryIndex);
    m_occlusionQueryActive = true;
    queryIndex *= sizeof(uint64_t);
    if (queryIndex < m_visibilityResultBufferSize) {
        m_visibilityResultBufferOffset = queryIndex;
        [m_renderCommandEncoder setVisibilityResultMode:MTLVisibilityResultModeCounting offset:queryIndex];
    }
}

void RenderPassEncoder::beginPipelineStatisticsQuery(const QuerySet& querySet, uint32_t queryIndex)
{
    UNUSED_PARAM(querySet);
    UNUSED_PARAM(queryIndex);
}

void RenderPassEncoder::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    // FIXME: validation according to
    // https://gpuweb.github.io/gpuweb/#dom-gpurendercommandsmixin-draw
    [m_renderCommandEncoder
        drawPrimitives:m_primitiveType
        vertexStart:firstVertex
        vertexCount:vertexCount
        instanceCount:instanceCount
        baseInstance:firstInstance];
}

void RenderPassEncoder::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
    UNUSED_PARAM(firstIndex);
    [m_renderCommandEncoder drawIndexedPrimitives:m_primitiveType indexCount:indexCount indexType:m_indexType indexBuffer:m_indexBuffer indexBufferOffset:m_indexBufferOffset instanceCount:instanceCount baseVertex:baseVertex baseInstance:firstInstance];
}

void RenderPassEncoder::drawIndexedIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset)
{
    [m_renderCommandEncoder drawIndexedPrimitives:m_primitiveType indexType:m_indexType indexBuffer:m_indexBuffer indexBufferOffset:m_indexBufferOffset indirectBuffer:indirectBuffer.buffer() indirectBufferOffset:indirectOffset];
}

void RenderPassEncoder::drawIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset)
{
    [m_renderCommandEncoder drawPrimitives:m_primitiveType indirectBuffer:indirectBuffer.buffer() indirectBufferOffset:indirectOffset];
}

void RenderPassEncoder::endOcclusionQuery()
{
    if (!prepareTheEncoderState())
        return;

    if (!m_occlusionQueryActive) {
        makeInvalid();
        return;
    }

    m_occlusionQueryActive = false;
    [m_renderCommandEncoder setVisibilityResultMode:MTLVisibilityResultModeDisabled offset:m_visibilityResultBufferOffset];
}

void RenderPassEncoder::endPass()
{
    auto& parentEncoder = m_commandEncoder.get();
    if (state() != CommandsMixin::EncoderState::Open || parentEncoder.state() != CommandsMixin::EncoderState::Locked) {
        m_device->generateAValidationError("endPass failed valiation"_s);
        return;
    }

    setState(CommandsMixin::EncoderState::Ended);
    parentEncoder.setState(CommandsMixin::EncoderState::Open);

    if (!isValid() || m_occlusionQueryActive || m_debugGroupStackSize) {
        parentEncoder.makeInvalid();
        return;
    }

    ASSERT(m_pendingTimestampWrites.isEmpty() || m_device->baseCapabilities().counterSamplingAPI == HardwareCapabilities::BaseCapabilities::CounterSamplingAPI::CommandBoundary);
    for (const auto& pendingTimestampWrite : m_pendingTimestampWrites)
        [m_renderCommandEncoder sampleCountersInBuffer:pendingTimestampWrite.querySet->counterSampleBuffer() atSampleIndex:pendingTimestampWrite.queryIndex withBarrier:NO];
    m_pendingTimestampWrites.clear();
    [m_renderCommandEncoder endEncoding];
    m_renderCommandEncoder = nil;
    m_pipelineLayout = nullptr;
}

void RenderPassEncoder::endPipelineStatisticsQuery()
{

}

static bool validToUseWith(auto& object, auto& targetObject)
{
    if (!object.isValid())
        return false;

    if (!object.device().isValid())
        return false;

    if (&object.device() != &targetObject.device())
        return false;

    return true;
}

static bool equalLayouts(auto& object, auto& targetObject)
{
    return object.pipelineLayout() == targetObject.pipelineLayout();
}

void RenderPassEncoder::executeBundles(Vector<std::reference_wrapper<const RenderBundle>>&& bundles)
{
    if (!prepareTheEncoderState())
        return;

    for (auto& bundleRef : bundles) {
        auto& bundle = bundleRef.get();
        if (!validToUseWith(bundle, *this) || !equalLayouts(bundle, *this) || m_depthReadOnly != bundle.depthReadOnly() || m_stencilReadOnly != bundle.stencilReadOnly()) {
            makeInvalid();
            return;
        }
    }

    for (auto& bundle : bundles) {
        const auto& renderBundle = bundle.get();
        for (const auto& resource : renderBundle.resources())
            [m_renderCommandEncoder useResource:resource.mtlResource usage:resource.usage stages:resource.renderStages];

        id<MTLIndirectCommandBuffer> icb = renderBundle.indirectCommandBuffer();
        [m_renderCommandEncoder executeCommandsInBuffer:icb withRange:NSMakeRange(0, icb.size)];
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
    UNUSED_PARAM(dynamicOffsetCount);
    UNUSED_PARAM(dynamicOffsets);

    for (const auto& resource : group.resources())
        [m_renderCommandEncoder useResource:resource.mtlResource usage:resource.usage stages:resource.renderStages];

    [m_renderCommandEncoder setVertexBuffer:group.vertexArgumentBuffer() offset:0 atIndex:m_device->vertexBufferIndexForBindGroup(groupIndex)];
    [m_renderCommandEncoder setFragmentBuffer:group.fragmentArgumentBuffer() offset:0 atIndex:groupIndex];
}

void RenderPassEncoder::setBlendConstant(const WGPUColor& color)
{
    if (!prepareTheEncoderState())
        return;

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
    m_pipelineLayout = pipeline.pipelineLayout();

    if (pipeline.renderPipelineState())
        [m_renderCommandEncoder setRenderPipelineState:pipeline.renderPipelineState()];
    if (pipeline.depthStencilState())
        [m_renderCommandEncoder setDepthStencilState:pipeline.depthStencilState()];
    [m_renderCommandEncoder setCullMode:pipeline.cullMode()];
    [m_renderCommandEncoder setFrontFacingWinding:pipeline.frontFace()];
    [m_renderCommandEncoder setDepthClipMode:pipeline.depthClipMode()];
}

NSUInteger RenderPassEncoder::attachmentWidth() const
{
    return m_attachmentWidth;
}

NSUInteger RenderPassEncoder::attachmentHeight() const
{
    return m_attachmentHeight;
}

void RenderPassEncoder::setScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    if (!prepareTheEncoderState())
        return;

    if (x + width > attachmentWidth() || y + height > attachmentHeight()) {
        makeInvalid();
        return;
    }

    [m_renderCommandEncoder setScissorRect: { x, y, width, height } ];
}

void RenderPassEncoder::setStencilReference(uint32_t reference)
{
    if (!prepareTheEncoderState())
        return;

    [m_renderCommandEncoder setStencilReferenceValue:reference];
}

void RenderPassEncoder::setVertexBuffer(uint32_t slot, const Buffer& buffer, uint64_t offset, uint64_t size)
{
    UNUSED_PARAM(size);
    [m_renderCommandEncoder setVertexBuffer:buffer.buffer() offset:offset atIndex:slot];
}

void RenderPassEncoder::setViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
{
    if (!prepareTheEncoderState())
        return;

    if (x < 0 || y < 0 || width < 0 || height < 0 || x + width > attachmentWidth() || y + height > attachmentHeight() || minDepth < 0.f || minDepth > 1.f || maxDepth < 0.f || maxDepth > 1.f || minDepth >= maxDepth) {
        makeInvalid();
        return;
    }

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

void wgpuRenderPassEncoderBeginPipelineStatisticsQuery(WGPURenderPassEncoder renderPassEncoder, WGPUQuerySet querySet, uint32_t queryIndex)
{
    WebGPU::fromAPI(renderPassEncoder).beginPipelineStatisticsQuery(WebGPU::fromAPI(querySet), queryIndex);
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

void wgpuRenderPassEncoderEndPipelineStatisticsQuery(WGPURenderPassEncoder renderPassEncoder)
{
    WebGPU::fromAPI(renderPassEncoder).endPipelineStatisticsQuery();
}

void wgpuRenderPassEncoderExecuteBundles(WGPURenderPassEncoder renderPassEncoder, uint32_t bundlesCount, const WGPURenderBundle* bundles)
{
    Vector<std::reference_wrapper<const WebGPU::RenderBundle>> bundlesToForward;
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

void wgpuRenderPassEncoderSetBindGroup(WGPURenderPassEncoder renderPassEncoder, uint32_t groupIndex, WGPUBindGroup group, uint32_t dynamicOffsetCount, const uint32_t* dynamicOffsets)
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
