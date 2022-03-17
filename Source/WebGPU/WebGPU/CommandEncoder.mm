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
#import "CommandEncoder.h"

#import "APIConversions.h"
#import "Buffer.h"
#import "CommandBuffer.h"
#import "ComputePassEncoder.h"
#import "Device.h"
#import "QuerySet.h"
#import "RenderPassEncoder.h"
#import "Utilities.h"

namespace WebGPU {

RefPtr<CommandEncoder> Device::createCommandEncoder(const WGPUCommandEncoderDescriptor& descriptor)
{
    if (descriptor.nextInChain)
        return nullptr;

    // https://gpuweb.github.io/gpuweb/#dom-gpudevice-createcommandencoder

    auto *commandBufferDescriptor = [MTLCommandBufferDescriptor new];
    commandBufferDescriptor.errorOptions = MTLCommandBufferErrorOptionEncoderExecutionStatus;
    id<MTLCommandBuffer> commandBuffer = [getQueue().commandQueue() commandBufferWithDescriptor:commandBufferDescriptor];
    if (!commandBuffer)
        return nullptr;

    commandBuffer.label = fromAPI(descriptor.label);

    return CommandEncoder::create(commandBuffer);
}

CommandEncoder::CommandEncoder(id<MTLCommandBuffer> commandBuffer)
    : m_commandBuffer(commandBuffer)
{
}

CommandEncoder::~CommandEncoder() = default;

void CommandEncoder::ensureBlitCommandEncoder()
{
    if (!m_blitCommandEncoder)
        m_blitCommandEncoder = [m_commandBuffer blitCommandEncoder];
}

void CommandEncoder::finalizeBlitCommandEncoder()
{
    if (m_blitCommandEncoder)
        [m_blitCommandEncoder endEncoding];
}

RefPtr<ComputePassEncoder> CommandEncoder::beginComputePass(const WGPUComputePassDescriptor& descriptor)
{
    UNUSED_PARAM(descriptor);
    return ComputePassEncoder::create(nil);
}

RefPtr<RenderPassEncoder> CommandEncoder::beginRenderPass(const WGPURenderPassDescriptor& descriptor)
{
    UNUSED_PARAM(descriptor);
    return RenderPassEncoder::create(nil);
}

static bool validateCopyBufferToBuffer(const Buffer& source, uint64_t sourceOffset, const Buffer& destination, uint64_t destinationOffset, uint64_t size)
{
    // FIXME: "source is valid to use with this."

    // FIXME: "destination is valid to use with this."

    // "source.[[usage]] contains COPY_SRC."
    if (!(source.usage() & WGPUBufferUsage_CopySrc))
        return false;

    // "destination.[[usage]] contains COPY_DST."
    if (!(destination.usage() & WGPUBufferUsage_CopyDst))
        return false;

    // "size is a multiple of 4."
    if (size % 4)
        return false;

    // "sourceOffset is a multiple of 4."
    if (sourceOffset % 4)
        return false;

    // "destinationOffset is a multiple of 4."
    if (destinationOffset % 4)
        return false;

    // FIXME: "(sourceOffset + size) does not overflow a GPUSize64."

    // FIXME: "(destinationOffset + size) does not overflow a GPUSize64."

    // FIXME: "source.[[size]] is greater than or equal to (sourceOffset + size)."
    // FIXME: Use checked arithmetic
    if (source.size() < sourceOffset + size)
        return false;

    // FIXME: "destination.[[size]] is greater than or equal to (destinationOffset + size)."
    // FIXME: Use checked arithmetic
    if (destination.size() < destinationOffset + size)
        return false;

    // FIXME: "source and destination are not the same GPUBuffer."
    if (&source == &destination)
        return false;

    return true;
}

void CommandEncoder::copyBufferToBuffer(const Buffer& source, uint64_t sourceOffset, const Buffer& destination, uint64_t destinationOffset, uint64_t size)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-copybuffertobuffer

    // "Prepare the encoder state of this. If it returns false, stop."
    if (!prepareTheEncoderState())
        return;

    // "If any of the following conditions are unsatisfied
    if (!validateCopyBufferToBuffer(source, sourceOffset, destination, destinationOffset, size)) {
        // FIXME: "generate a validation error and stop."
        return;
    }

    ensureBlitCommandEncoder();

    [m_blitCommandEncoder copyFromBuffer:source.buffer() sourceOffset:static_cast<NSUInteger>(sourceOffset) toBuffer:destination.buffer() destinationOffset:destinationOffset size:size];
}

void CommandEncoder::copyBufferToTexture(const WGPUImageCopyBuffer& source, const WGPUImageCopyTexture& destination, const WGPUExtent3D& copySize)
{
    UNUSED_PARAM(source);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(copySize);
}

void CommandEncoder::copyTextureToBuffer(const WGPUImageCopyTexture& source, const WGPUImageCopyBuffer& destination, const WGPUExtent3D& copySize)
{
    UNUSED_PARAM(source);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(copySize);
}

void CommandEncoder::copyTextureToTexture(const WGPUImageCopyTexture& source, const WGPUImageCopyTexture& destination, const WGPUExtent3D& copySize)
{
    UNUSED_PARAM(source);
    UNUSED_PARAM(destination);
    UNUSED_PARAM(copySize);
}

static bool validateClearBuffer(const Buffer& buffer, uint64_t offset, uint64_t size)
{
    // FIXME: "buffer is valid to use with this."

    // "buffer.[[usage]] contains COPY_DST."
    if (!(buffer.usage() & WGPUBufferUsage_CopyDst))
        return false;

    // "size is a multiple of 4."
    if (size % 4)
        return false;

    // "offset is a multiple of 4."
    if (offset % 4)
        return false;

    // "buffer.[[size]] is greater than or equal to (offset + size)."
    // FIXME: Use checked arithmetic.
    if (buffer.size() < offset + size)
        return false;

    return true;
}

void CommandEncoder::clearBuffer(const Buffer& buffer, uint64_t offset, uint64_t size)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-clearbuffer

    // "Prepare the encoder state of this. If it returns false, stop."
    if (!prepareTheEncoderState())
        return;

    // "If size is missing, set size to max(0, |buffer|.{{GPUBuffer/[[size]]}} - |offset|)."
    if (size == WGPU_WHOLE_SIZE) {
        // FIXME: Use checked arithmetic.
        size = buffer.size() - offset;
    }

    // "If any of the following conditions are unsatisfied"
    if (!validateClearBuffer(buffer, offset, size)) {
        // FIXME: "generate a validation error and stop."
        return;
    }

    ensureBlitCommandEncoder();

    [m_blitCommandEncoder fillBuffer:buffer.buffer() range:NSMakeRange(offset, size) value:0];
}

bool CommandEncoder::validateFinish() const
{
    // "Let validationSucceeded be true if all of the following requirements are met, and false otherwise."

    // FIXME: "this must be valid."

    // "this.[[state]] must be "open"."
    if (m_state != EncoderState::Open)
        return false;

    // FIXME: "this.[[debug_group_stack]] must be empty."

    // FIXME: "Every usage scope contained in this must satisfy the usage scope validation."

    return true;
}

RefPtr<CommandBuffer> CommandEncoder::finish(const WGPUCommandBufferDescriptor& descriptor)
{
    if (descriptor.nextInChain)
        return nullptr;

    // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-finish

    // "Let validationSucceeded be true if all of the following requirements are met, and false otherwise."
    auto validationFailed = !validateFinish();

    // "Set this.[[state]] to "ended"."
    m_state = EncoderState::Ended;

    // "If validationSucceeded is false, then:"
    if (validationFailed) {
        // FIXME: "Generate a validation error."

        // FIXME: "Return a new invalid GPUCommandBuffer."
        return nullptr;
    }

    finalizeBlitCommandEncoder();

    // "Set commandBuffer.[[command_list]] to this.[[commands]]."
    auto *commandBuffer = m_commandBuffer;
    m_commandBuffer = nil;

    commandBuffer.label = fromAPI(descriptor.label);

    return CommandBuffer::create(commandBuffer);
}

void CommandEncoder::insertDebugMarker(String&& markerLabel)
{
    UNUSED_PARAM(markerLabel);
}

void CommandEncoder::popDebugGroup()
{
}

void CommandEncoder::pushDebugGroup(String&& groupLabel)
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

void CommandEncoder::setLabel(String&& label)
{
    m_commandBuffer.label = label;
}

} // namespace WebGPU

void wgpuCommandEncoderRelease(WGPUCommandEncoder commandEncoder)
{
    delete commandEncoder;
}

WGPUComputePassEncoder wgpuCommandEncoderBeginComputePass(WGPUCommandEncoder commandEncoder, const WGPUComputePassDescriptor* descriptor)
{
    auto result = WebGPU::fromAPI(commandEncoder).beginComputePass(*descriptor);
    return result ? new WGPUComputePassEncoderImpl { result.releaseNonNull() } : nullptr;
}

WGPURenderPassEncoder wgpuCommandEncoderBeginRenderPass(WGPUCommandEncoder commandEncoder, const WGPURenderPassDescriptor* descriptor)
{
    auto result = WebGPU::fromAPI(commandEncoder).beginRenderPass(*descriptor);
    return result ? new WGPURenderPassEncoderImpl { result.releaseNonNull() } : nullptr;
}

void wgpuCommandEncoderCopyBufferToBuffer(WGPUCommandEncoder commandEncoder, WGPUBuffer source, uint64_t sourceOffset, WGPUBuffer destination, uint64_t destinationOffset, uint64_t size)
{
    WebGPU::fromAPI(commandEncoder).copyBufferToBuffer(WebGPU::fromAPI(source), sourceOffset, WebGPU::fromAPI(destination), destinationOffset, size);
}

void wgpuCommandEncoderCopyBufferToTexture(WGPUCommandEncoder commandEncoder, const WGPUImageCopyBuffer* source, const WGPUImageCopyTexture* destination, const WGPUExtent3D* copySize)
{
    WebGPU::fromAPI(commandEncoder).copyBufferToTexture(*source, *destination, *copySize);
}

void wgpuCommandEncoderCopyTextureToBuffer(WGPUCommandEncoder commandEncoder, const WGPUImageCopyTexture* source, const WGPUImageCopyBuffer* destination, const WGPUExtent3D* copySize)
{
    WebGPU::fromAPI(commandEncoder).copyTextureToBuffer(*source, *destination, *copySize);
}

void wgpuCommandEncoderCopyTextureToTexture(WGPUCommandEncoder commandEncoder, const WGPUImageCopyTexture* source, const WGPUImageCopyTexture* destination, const WGPUExtent3D* copySize)
{
    WebGPU::fromAPI(commandEncoder).copyTextureToTexture(*source, *destination, *copySize);
}

void wgpuCommandEncoderClearBuffer(WGPUCommandEncoder commandEncoder, WGPUBuffer buffer, uint64_t offset, uint64_t size)
{
    WebGPU::fromAPI(commandEncoder).clearBuffer(WebGPU::fromAPI(buffer), offset, size);
}

WGPUCommandBuffer wgpuCommandEncoderFinish(WGPUCommandEncoder commandEncoder, const WGPUCommandBufferDescriptor* descriptor)
{
    auto result = WebGPU::fromAPI(commandEncoder).finish(*descriptor);
    return result ? new WGPUCommandBufferImpl { result.releaseNonNull() } : nullptr;
}

void wgpuCommandEncoderInsertDebugMarker(WGPUCommandEncoder commandEncoder, const char* markerLabel)
{
    WebGPU::fromAPI(commandEncoder).insertDebugMarker(WebGPU::fromAPI(markerLabel));
}

void wgpuCommandEncoderPopDebugGroup(WGPUCommandEncoder commandEncoder)
{
    WebGPU::fromAPI(commandEncoder).popDebugGroup();
}

void wgpuCommandEncoderPushDebugGroup(WGPUCommandEncoder commandEncoder, const char* groupLabel)
{
    WebGPU::fromAPI(commandEncoder).pushDebugGroup(WebGPU::fromAPI(groupLabel));
}

void wgpuCommandEncoderResolveQuerySet(WGPUCommandEncoder commandEncoder, WGPUQuerySet querySet, uint32_t firstQuery, uint32_t queryCount, WGPUBuffer destination, uint64_t destinationOffset)
{
    WebGPU::fromAPI(commandEncoder).resolveQuerySet(WebGPU::fromAPI(querySet), firstQuery, queryCount, WebGPU::fromAPI(destination), destinationOffset);
}

void wgpuCommandEncoderWriteTimestamp(WGPUCommandEncoder commandEncoder, WGPUQuerySet querySet, uint32_t queryIndex)
{
    WebGPU::fromAPI(commandEncoder).writeTimestamp(WebGPU::fromAPI(querySet), queryIndex);
}

void wgpuCommandEncoderSetLabel(WGPUCommandEncoder commandEncoder, const char* label)
{
    WebGPU::fromAPI(commandEncoder).setLabel(WebGPU::fromAPI(label));
}
