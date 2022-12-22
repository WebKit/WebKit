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

void RenderBundleEncoder::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    if (id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand())
        [icbCommand drawPrimitives:m_primitiveType vertexStart:firstVertex vertexCount:vertexCount instanceCount:instanceCount baseInstance:firstInstance];
    else {
        m_icbDescriptor.commandTypes |= MTLIndirectCommandTypeDraw;

        m_recordedCommands.append([vertexCount, instanceCount, firstVertex, firstInstance, protectedThis = Ref { *this }] {
            protectedThis->draw(vertexCount, instanceCount, firstVertex, firstInstance);
        });
    }

    ++m_currentCommandIndex;
}

void RenderBundleEncoder::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
    UNUSED_PARAM(firstIndex);
    if (id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand())
        [icbCommand drawIndexedPrimitives:m_primitiveType indexCount:indexCount indexType:m_indexType indexBuffer:m_indexBuffer indexBufferOffset:m_indexBufferOffset instanceCount:instanceCount baseVertex:baseVertex baseInstance:firstInstance];
    else {
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

    if (id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand())
        [icbCommand drawPrimitives:m_primitiveType vertexStart:contents->vertexStart vertexCount:contents->vertexCount instanceCount:contents->instanceCount baseInstance:contents->baseInstance];
    else {
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
        m_indirectCommandBuffer = [m_device->device() newIndirectCommandBufferWithDescriptor:m_icbDescriptor maxCommandCount:commandCount options:0];

        for (auto& command : m_recordedCommands)
            command();

        m_recordedCommands.clear();
    }

    auto renderBundle = RenderBundle::create(m_indirectCommandBuffer, WTFMove(m_resources), m_device);
    renderBundle->setLabel(String::fromUTF8(descriptor.label));

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

void RenderBundleEncoder::setBindGroup(uint32_t groupIndex, const BindGroup& group, uint32_t dynamicOffsetCount, const uint32_t* dynamicOffsets)
{
    UNUSED_PARAM(dynamicOffsetCount);
    UNUSED_PARAM(dynamicOffsets);

    id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand();
    if (!icbCommand) {
        if (group.vertexArgumentBuffer())
            m_icbDescriptor.maxVertexBufferBindCount = std::max<NSUInteger>(m_icbDescriptor.maxVertexBufferBindCount, 1 + groupIndex);

        if (group.fragmentArgumentBuffer())
            m_icbDescriptor.maxFragmentBufferBindCount = std::max<NSUInteger>(m_icbDescriptor.maxFragmentBufferBindCount, 1 + groupIndex);

        m_recordedCommands.append([groupIndex, &group, protectedThis = Ref { *this }] {
            protectedThis->setBindGroup(groupIndex, group, 0, nullptr);
        });
        return;
    }

    for (const auto& resource : group.resources())
        m_resources.append(resource);

    [icbCommand setVertexBuffer:group.vertexArgumentBuffer() offset:0 atIndex:groupIndex];
    [icbCommand setFragmentBuffer:group.fragmentArgumentBuffer() offset:0 atIndex:groupIndex];
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
    m_indexType = format == WGPUIndexFormat_Uint32 ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16;
    m_indexBufferOffset = offset;
}

void RenderBundleEncoder::setPipeline(const RenderPipeline& pipeline)
{
    if (id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand())
        [icbCommand setRenderPipelineState:pipeline.renderPipelineState()];
    else {
        m_recordedCommands.append([&pipeline, protectedThis = Ref { *this }] {
            protectedThis->setPipeline(pipeline);
        });
    }
}

void RenderBundleEncoder::setVertexBuffer(uint32_t slot, const Buffer& buffer, uint64_t offset, uint64_t size)
{
    UNUSED_PARAM(size);
    if (id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand())
        [icbCommand setVertexBuffer:buffer.buffer() offset:offset atIndex:slot];
    else {
        m_icbDescriptor.maxVertexBufferBindCount = std::max<NSUInteger>(m_icbDescriptor.maxVertexBufferBindCount, 1 + slot);
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

void wgpuRenderBundleEncoderSetBindGroup(WGPURenderBundleEncoder renderBundleEncoder, uint32_t groupIndex, WGPUBindGroup group, uint32_t dynamicOffsetCount, const uint32_t* dynamicOffsets)
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
