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

    return CommandEncoder::create(commandBuffer, *this);
}

CommandEncoder::CommandEncoder(id<MTLCommandBuffer> commandBuffer, Device& device)
    : m_commandBuffer(commandBuffer)
    , m_device(device)
{
}

CommandEncoder::~CommandEncoder()
{
    finalizeBlitCommandEncoder();
}

void CommandEncoder::ensureBlitCommandEncoder()
{
    if (!m_blitCommandEncoder)
        m_blitCommandEncoder = [m_commandBuffer blitCommandEncoder];
}

void CommandEncoder::finalizeBlitCommandEncoder()
{
    if (m_blitCommandEncoder) {
        [m_blitCommandEncoder endEncoding];
        m_blitCommandEncoder = nil;
    }
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
        // "generate a validation error and stop."
        m_device->generateAValidationError("Validation failure."_s);
        return;
    }

    ensureBlitCommandEncoder();

    [m_blitCommandEncoder copyFromBuffer:source.buffer() sourceOffset:static_cast<NSUInteger>(sourceOffset) toBuffer:destination.buffer() destinationOffset:static_cast<NSUInteger>(destinationOffset) size:static_cast<NSUInteger>(size)];
}

static bool validateImageCopyBuffer(const WGPUImageCopyBuffer& imageCopyBuffer)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-validating-gpuimagecopybuffer

    // FIXME: "imageCopyBuffer.buffer must be a valid GPUBuffer."

    // "imageCopyBuffer.bytesPerRow must be a multiple of 256."
    if (imageCopyBuffer.layout.bytesPerRow % 256)
        return false;

    return false;
}

static bool validateCopyBufferToTexture(const WGPUImageCopyBuffer& source, const WGPUImageCopyTexture& destination, const WGPUExtent3D& copySize)
{
    // "Let dstTextureDesc be destination.texture.[[descriptor]]."
    const auto& dstTextureDesc = fromAPI(destination.texture).descriptor();

    // "validating GPUImageCopyBuffer(source) returns true."
    if (!validateImageCopyBuffer(source))
        return false;

    // "source.buffer.[[usage]] contains COPY_SRC."
    if (!(fromAPI(source.buffer).usage() & WGPUBufferUsage_CopySrc))
        return false;

    // "validating GPUImageCopyTexture(destination, copySize) returns true."
    if (!Texture::validateImageCopyTexture(destination, copySize))
        return false;

    // "dstTextureDesc.usage contains COPY_DST."
    if (!(dstTextureDesc.usage & WGPUTextureUsage_CopyDst))
        return false;

    // "dstTextureDesc.sampleCount is 1."
    if (dstTextureDesc.sampleCount != 1)
        return false;

    // "Let aspectSpecificFormat = dstTextureDesc.format."
    WGPUTextureFormat aspectSpecificFormat = dstTextureDesc.format;

    // "If dstTextureDesc.format is a depth-or-stencil format:"
    if (Texture::isDepthOrStencilFormat(dstTextureDesc.format)) {
        // "destination.aspect must refer to a single aspect of dstTextureDesc.format"
        if (!Texture::refersToSingleAspect(dstTextureDesc.format, destination.aspect))
            return false;

        // "That aspect must be a valid image copy destination according to § 25.1.2 Depth-stencil formats."
        if (!Texture::isValidImageCopyDestination(dstTextureDesc.format, destination.aspect))
            return false;

        // "Set aspectSpecificFormat to the aspect-specific format according to § 25.1.2 Depth-stencil formats."
        aspectSpecificFormat = Texture::aspectSpecificFormat(dstTextureDesc.format, destination.aspect);
    }

    // "validating texture copy range(destination, copySize) return true."
    if (!Texture::validateTextureCopyRange(destination, copySize))
        return false;

    // "If dstTextureDesc.format is not a depth-or-stencil format:"
    if (!Texture::isDepthOrStencilFormat(dstTextureDesc.format)) {
        // "source.offset is a multiple of the texel block size of dstTextureDesc.format."
        auto texelBlockSize = Texture::texelBlockSize(dstTextureDesc.format);
        if (source.layout.offset % texelBlockSize)
            return false;
    }

    // "If dstTextureDesc.format is a depth-or-stencil format:"
    if (Texture::isDepthOrStencilFormat(dstTextureDesc.format)) {
        // "source.offset is a multiple of 4."
        if (source.layout.offset % 4)
            return false;
    }

    // "validating linear texture data(source, source.buffer.[[size]], aspectSpecificFormat, copySize) succeeds."
    if (!Texture::validateLinearTextureData(source.layout, fromAPI(source.buffer).size(), aspectSpecificFormat, copySize))
        return false;

    return true;
}

void CommandEncoder::copyBufferToTexture(const WGPUImageCopyBuffer& source, const WGPUImageCopyTexture& destination, const WGPUExtent3D& copySize)
{
    if (source.nextInChain || source.layout.nextInChain || destination.nextInChain)
        return;

    // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-copybuffertotexture

    // "Prepare the encoder state of this. If it returns false, stop."
    if (!prepareTheEncoderState())
        return;

    // "If any of the following conditions are unsatisfied"
    if (!validateCopyBufferToTexture(source, destination, copySize)) {
        // "generate a validation error and stop."
        m_device->generateAValidationError("Validation failure.");
        return;
    }

    ensureBlitCommandEncoder();

    NSUInteger sourceBytesPerRow = source.layout.bytesPerRow;

    // FIXME: Use checked arithmetic
    NSUInteger sourceBytesPerImage = source.layout.rowsPerImage * source.layout.bytesPerRow;

    MTLBlitOption options = MTLBlitOptionNone;
    switch (destination.aspect) {
    case WGPUTextureAspect_All:
        options = MTLBlitOptionNone;
        break;
    case WGPUTextureAspect_StencilOnly:
        options = MTLBlitOptionStencilFromDepthStencil;
        break;
    case WGPUTextureAspect_DepthOnly:
        options = MTLBlitOptionDepthFromDepthStencil;
        break;
    case WGPUTextureAspect_Force32:
        return;
    }

    auto logicalSize = fromAPI(destination.texture).logicalMiplevelSpecificTextureExtent(destination.mipLevel);
    auto widthForMetal = std::min(copySize.width, logicalSize.width);
    auto heightForMetal = std::min(copySize.height, logicalSize.height);
    auto depthForMetal = std::min(copySize.depthOrArrayLayers, logicalSize.depthOrArrayLayers);

    auto& destinationDescriptor = fromAPI(destination.texture).descriptor();
    switch (destinationDescriptor.dimension) {
    case WGPUTextureDimension_1D: {
        // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400771-copyfrombuffer?language=objc
        // "When you copy to a 1D texture, height and depth must be 1."
        auto sourceSize = MTLSizeMake(widthForMetal, 1, 1);
        auto destinationOrigin = MTLOriginMake(destination.origin.x, 1, 1);
        for (uint32_t layer = 0; layer < copySize.depthOrArrayLayers; ++layer) {
            // FIXME: Use checked arithmetic.
            auto sourceOffset = static_cast<NSUInteger>(source.layout.offset + layer * sourceBytesPerImage);
            NSUInteger destinationSlice = destination.origin.z + layer;
            [m_blitCommandEncoder
                copyFromBuffer:fromAPI(source.buffer).buffer()
                sourceOffset:sourceOffset
                sourceBytesPerRow:sourceBytesPerRow
                sourceBytesPerImage:sourceBytesPerImage
                sourceSize:sourceSize
                toTexture:fromAPI(destination.texture).texture()
                destinationSlice:destinationSlice
                destinationLevel:destination.mipLevel
                destinationOrigin:destinationOrigin
                options:options];
        }
        break;
    }
    case WGPUTextureDimension_2D: {
        // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400771-copyfrombuffer?language=objc
        // "When you copy to a 2D texture, depth must be 1."
        auto sourceSize = MTLSizeMake(widthForMetal, heightForMetal, 1);
        auto destinationOrigin = MTLOriginMake(destination.origin.x, destination.origin.y, 1);
        for (uint32_t layer = 0; layer < copySize.depthOrArrayLayers; ++layer) {
            // FIXME: Use checked arithmetic.
            auto sourceOffset = static_cast<NSUInteger>(source.layout.offset + layer * sourceBytesPerImage);
            NSUInteger destinationSlice = destination.origin.z + layer;
            [m_blitCommandEncoder
                copyFromBuffer:fromAPI(source.buffer).buffer()
                sourceOffset:sourceOffset
                sourceBytesPerRow:sourceBytesPerRow
                sourceBytesPerImage:sourceBytesPerImage
                sourceSize:sourceSize
                toTexture:fromAPI(destination.texture).texture()
                destinationSlice:destinationSlice
                destinationLevel:destination.mipLevel
                destinationOrigin:destinationOrigin
                options:options];
        }
        break;
    }
    case WGPUTextureDimension_3D: {
        auto sourceSize = MTLSizeMake(widthForMetal, heightForMetal, depthForMetal);
        auto destinationOrigin = MTLOriginMake(destination.origin.x, destination.origin.y, destination.origin.z);
        auto sourceOffset = static_cast<NSUInteger>(source.layout.offset);
        [m_blitCommandEncoder
            copyFromBuffer:fromAPI(source.buffer).buffer()
            sourceOffset:sourceOffset
            sourceBytesPerRow:sourceBytesPerRow
            sourceBytesPerImage:sourceBytesPerImage
            sourceSize:sourceSize
            toTexture:fromAPI(destination.texture).texture()
            destinationSlice:0
            destinationLevel:destination.mipLevel
            destinationOrigin:destinationOrigin
            options:options];
        break;
    }
    case WGPUTextureDimension_Force32:
        return;
    }
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
        // "generate a validation error and stop."
        m_device->generateAValidationError("Validation failure."_s);
        return;
    }

    ensureBlitCommandEncoder();

    [m_blitCommandEncoder fillBuffer:buffer.buffer() range:NSMakeRange(static_cast<NSUInteger>(offset), static_cast<NSUInteger>(size)) value:0];
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
        // "Generate a validation error."
        m_device->generateAValidationError("Validation failure."_s);

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
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-insertdebugmarker

    // "Prepare the encoder state of this. If it returns false, stop."
    if (!prepareTheEncoderState())
        return;

    finalizeBlitCommandEncoder();

    // There's no direct way of doing this, so we just push/pop an empty debug group.
    [m_commandBuffer pushDebugGroup:markerLabel];
    [m_commandBuffer popDebugGroup];
}

bool CommandEncoder::validatePopDebugGroup() const
{
    // "this.[[debug_group_stack]] must not be empty."
    if (!m_debugGroupStackSize)
        return false;

    return true;
}

void CommandEncoder::popDebugGroup()
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-popdebuggroup

    // "Prepare the encoder state of this. If it returns false, stop."
    if (!prepareTheEncoderState())
        return;

    // "If any of the following requirements are unmet"
    if (!validatePopDebugGroup()) {
        // FIXME: "make this invalid, and stop."
        return;
    }

    finalizeBlitCommandEncoder();

    // "Pop an entry off of this.[[debug_group_stack]]."
    --m_debugGroupStackSize;
    [m_commandBuffer popDebugGroup];
}

void CommandEncoder::pushDebugGroup(String&& groupLabel)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-pushdebuggroup

    // "Prepare the encoder state of this. If it returns false, stop."
    if (!prepareTheEncoderState())
        return;
    
    finalizeBlitCommandEncoder();

    // "Push groupLabel onto this.[[debug_group_stack]]."
    ++m_debugGroupStackSize;
    [m_commandBuffer pushDebugGroup:groupLabel];
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

#pragma mark WGPU Stubs

void wgpuCommandEncoderRelease(WGPUCommandEncoder commandEncoder)
{
    WebGPU::fromAPI(commandEncoder).deref();
}

WGPUComputePassEncoder wgpuCommandEncoderBeginComputePass(WGPUCommandEncoder commandEncoder, const WGPUComputePassDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::fromAPI(commandEncoder).beginComputePass(*descriptor));
}

WGPURenderPassEncoder wgpuCommandEncoderBeginRenderPass(WGPUCommandEncoder commandEncoder, const WGPURenderPassDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::fromAPI(commandEncoder).beginRenderPass(*descriptor));
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
    return WebGPU::releaseToAPI(WebGPU::fromAPI(commandEncoder).finish(*descriptor));
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
