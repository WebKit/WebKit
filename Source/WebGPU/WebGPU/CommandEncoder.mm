/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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
#import "CommandEncoder.h"

#import "Buffer.h"
#import "CommandBuffer.h"
#import "ComputePassEncoder.h"
#import "QuerySet.h"
#import "RenderPassEncoder.h"
#import "WebGPUExt.h"

namespace WebGPU {

CommandEncoder::CommandEncoder() = default;

CommandEncoder::~CommandEncoder() = default;

Ref<ComputePassEncoder> CommandEncoder::beginComputePass(const WGPUComputePassDescriptor* descriptor)
{
    UNUSED_PARAM(descriptor);
    return ComputePassEncoder::create();
}

Ref<RenderPassEncoder> CommandEncoder::beginRenderPass(const WGPURenderPassDescriptor* descriptor)
{
    UNUSED_PARAM(descriptor);
    return RenderPassEncoder::create();
}

void CommandEncoder::copyBufferToBuffer(const Buffer& source, uint64_t sourceOffset, const Buffer& destination, uint64_t destinationOffset, uint64_t size)
{
    UNUSED_PARAM(source);
    UNUSED_PARAM(sourceOffset);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(destinationOffset);
    UNUSED_PARAM(size);
}

void CommandEncoder::copyBufferToTexture(const WGPUImageCopyBuffer* source, const WGPUImageCopyTexture* destination, const WGPUExtent3D* copySize)
{
    UNUSED_PARAM(source);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(copySize);
}

void CommandEncoder::copyTextureToBuffer(const WGPUImageCopyTexture* source, const WGPUImageCopyBuffer* destination, const WGPUExtent3D* copySize)
{
    UNUSED_PARAM(source);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(copySize);
}

void CommandEncoder::copyTextureToTexture(const WGPUImageCopyTexture* source, const WGPUImageCopyTexture* destination, const WGPUExtent3D* copySize)
{
    UNUSED_PARAM(source);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(copySize);
}

void CommandEncoder::clearBuffer(const Buffer& buffer, uint64_t offset, uint64_t size)
{
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(size);
}

Ref<CommandBuffer> CommandEncoder::finish(const WGPUCommandBufferDescriptor* descriptor)
{
    UNUSED_PARAM(descriptor);
    return CommandBuffer::create();
}

void CommandEncoder::insertDebugMarker(const char* markerLabel)
{
    UNUSED_PARAM(markerLabel);
}

void CommandEncoder::popDebugGroup()
{
}

void CommandEncoder::pushDebugGroup(const char* groupLabel)
{
    UNUSED_PARAM(groupLabel);
}

void CommandEncoder::resolveQuerySet(const QuerySet& querySet, uint32_t firstQuery, uint32_t queryCount, const Buffer& destination, uint64_t destinationOffset)
{
    UNUSED_PARAM(querySet);
    UNUSED_PARAM(firstQuery);
    UNUSED_PARAM(queryCount);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(destinationOffset);
}

void CommandEncoder::writeTimestamp(const QuerySet& querySet, uint32_t queryIndex)
{
    UNUSED_PARAM(querySet);
    UNUSED_PARAM(queryIndex);
}

void CommandEncoder::setLabel(const char* label)
{
    UNUSED_PARAM(label);
}

} // namespace WebGPU

void wgpuCommandEncoderRelease(WGPUCommandEncoder commandEncoder)
{
    delete commandEncoder;
}

WGPUComputePassEncoder wgpuCommandEncoderBeginComputePass(WGPUCommandEncoder commandEncoder, const WGPUComputePassDescriptor* descriptor)
{
    return new WGPUComputePassEncoderImpl { commandEncoder->commandEncoder->beginComputePass(descriptor) };
}

WGPURenderPassEncoder wgpuCommandEncoderBeginRenderPass(WGPUCommandEncoder commandEncoder, const WGPURenderPassDescriptor* descriptor)
{
    return new WGPURenderPassEncoderImpl { commandEncoder->commandEncoder->beginRenderPass(descriptor) };
}

void wgpuCommandEncoderCopyBufferToBuffer(WGPUCommandEncoder commandEncoder, WGPUBuffer source, uint64_t sourceOffset, WGPUBuffer destination, uint64_t destinationOffset, uint64_t size)
{
    commandEncoder->commandEncoder->copyBufferToBuffer(source->buffer, sourceOffset, destination->buffer, destinationOffset, size);
}

void wgpuCommandEncoderCopyBufferToTexture(WGPUCommandEncoder commandEncoder, const WGPUImageCopyBuffer* source, const WGPUImageCopyTexture* destination, const WGPUExtent3D* copySize)
{
    commandEncoder->commandEncoder->copyBufferToTexture(source, destination, copySize);
}

void wgpuCommandEncoderCopyTextureToBuffer(WGPUCommandEncoder commandEncoder, const WGPUImageCopyTexture* source, const WGPUImageCopyBuffer* destination, const WGPUExtent3D* copySize)
{
    commandEncoder->commandEncoder->copyTextureToBuffer(source, destination, copySize);
}

void wgpuCommandEncoderCopyTextureToTexture(WGPUCommandEncoder commandEncoder, const WGPUImageCopyTexture* source, const WGPUImageCopyTexture* destination, const WGPUExtent3D* copySize)
{
    commandEncoder->commandEncoder->copyTextureToTexture(source, destination, copySize);
}

void wgpuCommandEncoderClearBuffer(WGPUCommandEncoder commandEncoder, WGPUBuffer buffer, uint64_t offset, uint64_t size)
{
    commandEncoder->commandEncoder->clearBuffer(buffer->buffer, offset, size);
}

WGPUCommandBuffer wgpuCommandEncoderFinish(WGPUCommandEncoder commandEncoder, const WGPUCommandBufferDescriptor* descriptor)
{
    return new WGPUCommandBufferImpl { commandEncoder->commandEncoder->finish(descriptor) };
}

void wgpuCommandEncoderInsertDebugMarker(WGPUCommandEncoder commandEncoder, const char* markerLabel)
{
    commandEncoder->commandEncoder->insertDebugMarker(markerLabel);
}

void wgpuCommandEncoderPopDebugGroup(WGPUCommandEncoder commandEncoder)
{
    commandEncoder->commandEncoder->popDebugGroup();
}

void wgpuCommandEncoderPushDebugGroup(WGPUCommandEncoder commandEncoder, const char* groupLabel)
{
    commandEncoder->commandEncoder->pushDebugGroup(groupLabel);
}

void wgpuCommandEncoderResolveQuerySet(WGPUCommandEncoder commandEncoder, WGPUQuerySet querySet, uint32_t firstQuery, uint32_t queryCount, WGPUBuffer destination, uint64_t destinationOffset)
{
    commandEncoder->commandEncoder->resolveQuerySet(querySet->querySet, firstQuery, queryCount, destination->buffer, destinationOffset);
}

void wgpuCommandEncoderWriteTimestamp(WGPUCommandEncoder commandEncoder, WGPUQuerySet querySet, uint32_t queryIndex)
{
    commandEncoder->commandEncoder->writeTimestamp(querySet->querySet, queryIndex);
}

void wgpuCommandEncoderSetLabel(WGPUCommandEncoder commandEncoder, const char* label)
{
    commandEncoder->commandEncoder->setLabel(label);
}

