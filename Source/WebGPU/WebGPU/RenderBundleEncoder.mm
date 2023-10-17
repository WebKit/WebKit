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
#import "RenderBundleEncoder.h"

#import "APIConversions.h"
#import "BindGroup.h"
#import "Buffer.h"
#import "Device.h"
#import "RenderBundle.h"
#import "RenderPipeline.h"

namespace WebGPU {

Ref<RenderBundleEncoder> Device::createRenderBundleEncoder(const WGPURenderBundleEncoderDescriptor& descriptor)
{
    if (descriptor.nextInChain)
        return RenderBundleEncoder::createInvalid(*this);

    MTLIndirectCommandBufferDescriptor *icbDescriptor = [MTLIndirectCommandBufferDescriptor new];
    icbDescriptor.inheritBuffers = NO;
    icbDescriptor.inheritPipelineState = YES;

    return RenderBundleEncoder::create(icbDescriptor, *this);
}

RenderBundleEncoder::RenderBundleEncoder(MTLIndirectCommandBufferDescriptor *indirectCommandBufferDescriptor, Device& device)
    : m_icbDescriptor(indirectCommandBufferDescriptor)
    , m_resources([NSMapTable strongToStrongObjectsMapTable])
    , m_device(device)
{
}

RenderBundleEncoder::RenderBundleEncoder(Device& device)
    : m_device(device)
{
}

RenderBundleEncoder::~RenderBundleEncoder() = default;

id<MTLIndirectRenderCommand> RenderBundleEncoder::currentRenderCommand()
{
    ASSERT(!m_indirectCommandBuffer || m_currentCommandIndex < m_indirectCommandBuffer.size);
    return m_currentCommandIndex < m_indirectCommandBuffer.size ? [m_indirectCommandBuffer indirectRenderCommandAtIndex:m_currentCommandIndex] : nil;
}

void RenderBundleEncoder::executePreDrawCommands()
{
    if (id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand()) {
        for (size_t i = 0, sz = m_vertexBuffers.size(); i < sz; ++i)
            [icbCommand setVertexBuffer:m_vertexBuffers[i].buffer offset:m_vertexBuffers[i].offset atIndex:i];

        for (size_t i = 0, sz = m_fragmentBuffers.size(); i < sz; ++i)
            [icbCommand setFragmentBuffer:m_fragmentBuffers[i].buffer offset:m_fragmentBuffers[i].offset atIndex:i];
    }

    if (!m_pipeline)
        return;

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
            memcpy(&m_fragmentDynamicOffsets[startIndex], &fragmentOffsets[0], sizeof(fragmentOffsets[0]) * fragmentOffsets.size());
        }
    }

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=262208 implement dynamic offsets in render bundles

    m_bindGroupDynamicOffsets.clear();
}

void RenderBundleEncoder::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    if (id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand()) {
        executePreDrawCommands();
        [icbCommand drawPrimitives:m_primitiveType vertexStart:firstVertex vertexCount:vertexCount instanceCount:instanceCount baseInstance:firstInstance];
    } else {
        m_icbDescriptor.commandTypes |= MTLIndirectCommandTypeDraw;

        m_recordedCommands.append([vertexCount, instanceCount, firstVertex, firstInstance, protectedThis = Ref { *this }] {
            protectedThis->draw(vertexCount, instanceCount, firstVertex, firstInstance);
        });
    }

    ++m_currentCommandIndex;
}

void RenderBundleEncoder::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
    if (id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand()) {
        executePreDrawCommands();
        auto firstIndexOffsetInBytes = firstIndex * (m_indexType == MTLIndexTypeUInt16 ? sizeof(uint16_t) : sizeof(uint32_t));
        [icbCommand drawIndexedPrimitives:m_primitiveType indexCount:indexCount indexType:m_indexType indexBuffer:m_indexBuffer indexBufferOffset:(m_indexBufferOffset + firstIndexOffsetInBytes) instanceCount:instanceCount baseVertex:baseVertex baseInstance:firstInstance];
    } else {
        m_icbDescriptor.commandTypes |= MTLIndirectCommandTypeDrawIndexed;

        m_recordedCommands.append([indexCount, instanceCount, firstIndex, baseVertex, firstInstance, protectedThis = Ref { *this }] {
            protectedThis->drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
        });
    }

    ++m_currentCommandIndex;
}

void RenderBundleEncoder::drawIndexedIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset)
{
    UNUSED_PARAM(indirectOffset);

    auto contents = (MTLDrawIndexedPrimitivesIndirectArguments*)indirectBuffer.buffer().contents;
    if (!contents)
        return;

    if (id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand()) {
        ASSERT(m_indexBufferOffset == contents->indexStart);
        executePreDrawCommands();
        [icbCommand drawIndexedPrimitives:m_primitiveType indexCount:contents->indexCount indexType:m_indexType indexBuffer:m_indexBuffer indexBufferOffset:m_indexBufferOffset instanceCount:contents->instanceCount baseVertex:contents->baseVertex baseInstance:contents->baseInstance];
    } else {
        m_icbDescriptor.commandTypes |= MTLIndirectCommandTypeDrawIndexed;

        m_recordedCommands.append([&indirectBuffer, indirectOffset, protectedThis = Ref { *this }] {
            protectedThis->drawIndexedIndirect(indirectBuffer, indirectOffset);
        });
    }

    ++m_currentCommandIndex;
}

void RenderBundleEncoder::drawIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset)
{
    UNUSED_PARAM(indirectOffset);

    auto contents = (MTLDrawPrimitivesIndirectArguments*)indirectBuffer.buffer().contents;
    if (!contents)
        return;

    if (id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand()) {
        executePreDrawCommands();
        [icbCommand drawPrimitives:m_primitiveType vertexStart:contents->vertexStart vertexCount:contents->vertexCount instanceCount:contents->instanceCount baseInstance:contents->baseInstance];
    } else {
        m_icbDescriptor.commandTypes |= MTLIndirectCommandTypeDraw;

        m_recordedCommands.append([&indirectBuffer, indirectOffset, protectedThis = Ref { *this }] {
            protectedThis->drawIndirect(indirectBuffer, indirectOffset);
        });
    }

    ++m_currentCommandIndex;
}

Ref<RenderBundle> RenderBundleEncoder::finish(const WGPURenderBundleDescriptor& descriptor)
{
    auto commandCount = m_currentCommandIndex;
    m_currentCommandIndex = 0;

    if (!m_indirectCommandBuffer) {
        m_icbDescriptor.maxVertexBufferBindCount = m_device->maxBuffersPlusVertexBuffersForVertexStage();
        m_vertexBuffers.resize(m_icbDescriptor.maxVertexBufferBindCount);
        m_fragmentBuffers.resize(m_icbDescriptor.maxFragmentBufferBindCount);
        m_indirectCommandBuffer = [m_device->device() newIndirectCommandBufferWithDescriptor:m_icbDescriptor maxCommandCount:commandCount options:0];

        for (auto& command : m_recordedCommands)
            command();

        m_recordedCommands.clear();
    }

    if (!m_currentPipelineState)
        return RenderBundle::createInvalid(m_device);

    auto renderBundle = RenderBundle::create(m_indirectCommandBuffer, m_resources, m_currentPipelineState, m_depthStencilState, m_cullMode, m_frontFace, m_depthClipMode, m_device);
    renderBundle->setLabel(String::fromUTF8(descriptor.label));
    m_currentPipelineState = nil;
    m_resources = nil;
    m_vertexBuffers.clear();
    m_fragmentBuffers.clear();

    return renderBundle;
}

void RenderBundleEncoder::insertDebugMarker(String&&)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-insertdebugmarker

    if (!prepareTheEncoderState())
        return;

    // MTLIndirectCommandBuffers don't support debug commands.
}

bool RenderBundleEncoder::validatePopDebugGroup() const
{
    if (!m_debugGroupStackSize)
        return false;

    return true;
}

void RenderBundleEncoder::popDebugGroup()
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-popdebuggroup

    if (!prepareTheEncoderState())
        return;

    if (!validatePopDebugGroup()) {
        makeInvalid();
        return;
    }

    --m_debugGroupStackSize;
    // MTLIndirectCommandBuffers don't support debug commands.
}

void RenderBundleEncoder::pushDebugGroup(String&&)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-pushdebuggroup

    if (!prepareTheEncoderState())
        return;

    ++m_debugGroupStackSize;
    // MTLIndirectCommandBuffers don't support debug commands.
}

static void addResource(RenderBundle::ResourcesContainer* resources, id<MTLResource> mtlResource, ResourceUsageAndRenderStage *resource)
{
    if (ResourceUsageAndRenderStage *existingResource = [resources objectForKey:mtlResource]) {
        existingResource.usage |= resource.usage;
        existingResource.renderStages |= resource.renderStages;
    } else
        [resources setObject:resource forKey:mtlResource];
}

static void addResource(RenderBundle::ResourcesContainer* resources, id<MTLResource> mtlResource, MTLRenderStages stage)
{
    return addResource(resources, mtlResource, [[ResourceUsageAndRenderStage alloc] initWithUsage:MTLResourceUsageRead renderStages:stage]);
}

void RenderBundleEncoder::setBindGroup(uint32_t groupIndex, const BindGroup& group, uint32_t dynamicOffsetCount, const uint32_t* dynamicOffsets)
{
    id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand();
    if (!icbCommand) {
        if (group.fragmentArgumentBuffer())
            m_icbDescriptor.maxFragmentBufferBindCount = std::max<NSUInteger>(m_icbDescriptor.maxFragmentBufferBindCount, 1 + groupIndex);

        m_recordedCommands.append([groupIndex, &group, protectedThis = Ref { *this }, dynamicOffsets, dynamicOffsetCount] {
            protectedThis->setBindGroup(groupIndex, group, dynamicOffsetCount, dynamicOffsets);
        });
        return;
    }

    if (!m_currentPipelineState)
        return;

    if (dynamicOffsetCount)
        m_bindGroupDynamicOffsets.add(groupIndex, Vector<uint32_t>(dynamicOffsets, dynamicOffsetCount));

    for (const auto& resource : group.resources()) {
        ResourceUsageAndRenderStage* usageAndRenderStage = [[ResourceUsageAndRenderStage alloc] initWithUsage:resource.usage renderStages:resource.renderStages];
        for (id<MTLResource> mtlResource : resource.mtlResources)
            addResource(m_resources, mtlResource, usageAndRenderStage);
    }

    if (group.vertexArgumentBuffer()) {
        addResource(m_resources, group.vertexArgumentBuffer(), MTLRenderStageVertex);
        m_vertexBuffers[m_device->vertexBufferIndexForBindGroup(groupIndex)] = { group.vertexArgumentBuffer(), 0, dynamicOffsetCount, dynamicOffsets };
    }
    if (group.fragmentArgumentBuffer()) {
        addResource(m_resources, group.fragmentArgumentBuffer(), MTLRenderStageFragment);
        m_fragmentBuffers[groupIndex] = { group.fragmentArgumentBuffer(), 0, dynamicOffsetCount, dynamicOffsets };
    }
}

void RenderBundleEncoder::setIndexBuffer(const Buffer& buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size)
{
    if (!currentRenderCommand()) {
        m_recordedCommands.append([&buffer, format, offset, size, protectedThis = Ref { *this }] {
            protectedThis->setIndexBuffer(buffer, format, offset, size);
        });
        return;
    }

    m_indexBuffer = buffer.buffer();
    RELEASE_ASSERT(m_indexBuffer);
    m_indexType = format == WGPUIndexFormat_Uint32 ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16;
    m_indexBufferOffset = offset;
    if (m_indexBuffer)
        addResource(m_resources, m_indexBuffer, MTLRenderStageVertex);
}

void RenderBundleEncoder::setPipeline(const RenderPipeline& pipeline)
{
    if (!pipeline.renderPipelineState())
        return;

    if (id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand()) {
        m_pipeline = &pipeline;

        m_vertexDynamicOffsets.resize(pipeline.pipelineLayout().sizeOfVertexDynamicOffsets());
        m_fragmentDynamicOffsets.resize(pipeline.pipelineLayout().sizeOfFragmentDynamicOffsets());

        m_currentPipelineState = pipeline.renderPipelineState();
        m_depthStencilState = pipeline.depthStencilState();
        m_cullMode = pipeline.cullMode();
        m_frontFace = pipeline.frontFace();
        m_depthClipMode = pipeline.depthClipMode();
    } else {
        m_recordedCommands.append([&pipeline, protectedThis = Ref { *this }] {
            protectedThis->setPipeline(pipeline);
        });
    }
}

void RenderBundleEncoder::setVertexBuffer(uint32_t slot, const Buffer& buffer, uint64_t offset, uint64_t size)
{
    UNUSED_PARAM(size);
    if (id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand()) {
        addResource(m_resources, buffer.buffer(), MTLRenderStageVertex);
        m_vertexBuffers[slot] = { buffer.buffer(), offset };
    } else {
        m_recordedCommands.append([slot, &buffer, offset, size, protectedThis = Ref { *this }] {
            protectedThis->setVertexBuffer(slot, buffer, offset, size);
        });
    }
}

void RenderBundleEncoder::setLabel(String&& label)
{
    m_indirectCommandBuffer.label = label;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuRenderBundleEncoderReference(WGPURenderBundleEncoder renderBundleEncoder)
{
    WebGPU::fromAPI(renderBundleEncoder).ref();
}

void wgpuRenderBundleEncoderRelease(WGPURenderBundleEncoder renderBundleEncoder)
{
    WebGPU::fromAPI(renderBundleEncoder).deref();
}

void wgpuRenderBundleEncoderDraw(WGPURenderBundleEncoder renderBundleEncoder, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    WebGPU::fromAPI(renderBundleEncoder).draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void wgpuRenderBundleEncoderDrawIndexed(WGPURenderBundleEncoder renderBundleEncoder, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
    WebGPU::fromAPI(renderBundleEncoder).drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

void wgpuRenderBundleEncoderDrawIndexedIndirect(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset)
{
    WebGPU::fromAPI(renderBundleEncoder).drawIndexedIndirect(WebGPU::fromAPI(indirectBuffer), indirectOffset);
}

void wgpuRenderBundleEncoderDrawIndirect(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset)
{
    WebGPU::fromAPI(renderBundleEncoder).drawIndirect(WebGPU::fromAPI(indirectBuffer), indirectOffset);
}

WGPURenderBundle wgpuRenderBundleEncoderFinish(WGPURenderBundleEncoder renderBundleEncoder, const WGPURenderBundleDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::fromAPI(renderBundleEncoder).finish(*descriptor));
}

void wgpuRenderBundleEncoderInsertDebugMarker(WGPURenderBundleEncoder renderBundleEncoder, const char* markerLabel)
{
    WebGPU::fromAPI(renderBundleEncoder).insertDebugMarker(WebGPU::fromAPI(markerLabel));
}

void wgpuRenderBundleEncoderPopDebugGroup(WGPURenderBundleEncoder renderBundleEncoder)
{
    WebGPU::fromAPI(renderBundleEncoder).popDebugGroup();
}

void wgpuRenderBundleEncoderPushDebugGroup(WGPURenderBundleEncoder renderBundleEncoder, const char* groupLabel)
{
    WebGPU::fromAPI(renderBundleEncoder).pushDebugGroup(WebGPU::fromAPI(groupLabel));
}

void wgpuRenderBundleEncoderSetBindGroup(WGPURenderBundleEncoder renderBundleEncoder, uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, const uint32_t* dynamicOffsets)
{
    WebGPU::fromAPI(renderBundleEncoder).setBindGroup(groupIndex, WebGPU::fromAPI(group), dynamicOffsetCount, dynamicOffsets);
}

void wgpuRenderBundleEncoderSetIndexBuffer(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size)
{
    WebGPU::fromAPI(renderBundleEncoder).setIndexBuffer(WebGPU::fromAPI(buffer), format, offset, size);
}

void wgpuRenderBundleEncoderSetPipeline(WGPURenderBundleEncoder renderBundleEncoder, WGPURenderPipeline pipeline)
{
    WebGPU::fromAPI(renderBundleEncoder).setPipeline(WebGPU::fromAPI(pipeline));
}

void wgpuRenderBundleEncoderSetVertexBuffer(WGPURenderBundleEncoder renderBundleEncoder, uint32_t slot, WGPUBuffer buffer, uint64_t offset, uint64_t size)
{
    WebGPU::fromAPI(renderBundleEncoder).setVertexBuffer(slot, WebGPU::fromAPI(buffer), offset, size);
}

void wgpuRenderBundleEncoderSetLabel(WGPURenderBundleEncoder renderBundleEncoder, const char* label)
{
    WebGPU::fromAPI(renderBundleEncoder).setLabel(WebGPU::fromAPI(label));
}
