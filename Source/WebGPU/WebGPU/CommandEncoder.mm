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
#import "IsValidToUseWith.h"
#import "QuerySet.h"
#import "RenderPassEncoder.h"
#import "Texture.h"
#import <wtf/CheckedArithmetic.h>

namespace WebGPU {

static MTLLoadAction loadAction(WGPULoadOp loadOp)
{
    switch (loadOp) {
    case WGPULoadOp_Load:
        return MTLLoadActionLoad;
    case WGPULoadOp_Clear:
        return MTLLoadActionClear;
    case WGPULoadOp_Force32:
        ASSERT_NOT_REACHED();
        return MTLLoadActionClear;
    }
}

static MTLStoreAction storeAction(WGPUStoreOp storeOp)
{
    switch (storeOp) {
    case WGPUStoreOp_Store:
        return MTLStoreActionStore;
    case WGPUStoreOp_Discard:
        return MTLStoreActionDontCare;
    case WGPUStoreOp_Force32:
        ASSERT_NOT_REACHED();
        return MTLStoreActionDontCare;
    }
}

Ref<CommandEncoder> Device::createCommandEncoder(const WGPUCommandEncoderDescriptor& descriptor)
{
    if (descriptor.nextInChain)
        return CommandEncoder::createInvalid(*this);

    captureFrameIfNeeded();
    // https://gpuweb.github.io/gpuweb/#dom-gpudevice-createcommandencoder

    auto *commandBufferDescriptor = [MTLCommandBufferDescriptor new];
    commandBufferDescriptor.errorOptions = MTLCommandBufferErrorOptionEncoderExecutionStatus;
    id<MTLCommandBuffer> commandBuffer = [getQueue().commandQueue() commandBufferWithDescriptor:commandBufferDescriptor];
    if (!commandBuffer)
        return CommandEncoder::createInvalid(*this);

    commandBuffer.label = fromAPI(descriptor.label);

    return CommandEncoder::create(commandBuffer, *this);
}

CommandEncoder::CommandEncoder(id<MTLCommandBuffer> commandBuffer, Device& device)
    : m_commandBuffer(commandBuffer)
    , m_device(device)
{
}

CommandEncoder::CommandEncoder(Device& device)
    : m_device(device)
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

Ref<ComputePassEncoder> CommandEncoder::beginComputePass(const WGPUComputePassDescriptor& descriptor)
{
    UNUSED_PARAM(descriptor);
    return ComputePassEncoder::createInvalid(m_device);
}

bool CommandEncoder::validateRenderPassDescriptor(const WGPURenderPassDescriptor& descriptor) const
{
    // FIXME: Implement this according to
    // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-beginrenderpass.
    UNUSED_PARAM(descriptor);

    return true;
}

Ref<RenderPassEncoder> CommandEncoder::beginRenderPass(const WGPURenderPassDescriptor& descriptor)
{
    UNUSED_PARAM(descriptor);
    if (descriptor.nextInChain)
        return RenderPassEncoder::createInvalid(m_device);

    if (!validateRenderPassDescriptor(descriptor))
        return RenderPassEncoder::createInvalid(m_device);

    MTLRenderPassDescriptor* mtlDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];

    if (descriptor.colorAttachmentCount > 8)
        return RenderPassEncoder::createInvalid(m_device);

    for (uint32_t i = 0; i < descriptor.colorAttachmentCount; ++i) {
        const auto& attachment = descriptor.colorAttachments[i];
        const auto& mtlAttachment = mtlDescriptor.colorAttachments[i];

        mtlAttachment.clearColor = MTLClearColorMake(attachment.clearColor.r,
            attachment.clearColor.g,
            attachment.clearColor.b,
            attachment.clearColor.a);

        mtlAttachment.texture = fromAPI(attachment.view).texture();
        mtlAttachment.level = 0;
        mtlAttachment.slice = 0;
        mtlAttachment.depthPlane = 0;
        mtlAttachment.loadAction = loadAction(attachment.loadOp);
        mtlAttachment.storeAction = attachment.resolveTarget ? MTLStoreActionStoreAndMultisampleResolve : storeAction(attachment.storeOp);

        if (attachment.resolveTarget) {
            mtlDescriptor.colorAttachments[i].resolveTexture = fromAPI(attachment.resolveTarget).texture();
            mtlAttachment.resolveLevel = 0;
            mtlAttachment.resolveSlice = 0;
            mtlAttachment.resolveDepthPlane = 0;
            mtlAttachment.storeAction = MTLStoreActionMultisampleResolve;
        }
    }

    if (const auto* attachment = descriptor.depthStencilAttachment) {
        const auto& mtlAttachment = mtlDescriptor.depthAttachment;
        ASSERT("FIXME: Add support for depthReadOnly" && !attachment->depthReadOnly);
        mtlAttachment.clearDepth = attachment->clearDepth;
        mtlAttachment.texture = fromAPI(attachment->view).texture();
        mtlAttachment.loadAction = loadAction(attachment->depthLoadOp);
        mtlAttachment.storeAction = storeAction(attachment->depthStoreOp);
    }

    if (const auto* attachment = descriptor.depthStencilAttachment) {
        const auto& mtlAttachment = mtlDescriptor.stencilAttachment;
        ASSERT("FIXME: Add support for stencilReadOnly" && !attachment->stencilReadOnly);
        // FIXME: assign the correct stencil texture
        // mtlAttachment.texture = fromAPI(attachment->view).texture();
        mtlAttachment.clearStencil = attachment->clearStencil;
        mtlAttachment.loadAction = loadAction(attachment->stencilLoadOp);
        mtlAttachment.storeAction = storeAction(attachment->stencilStoreOp);
    }

    size_t visibilityResultBufferSize = 0;
    if (auto* wgpuOcclusionQuery = descriptor.occlusionQuerySet) {
        const auto& occlusionQuery = fromAPI(wgpuOcclusionQuery);
        mtlDescriptor.visibilityResultBuffer = occlusionQuery.visibilityBuffer();
        visibilityResultBufferSize = occlusionQuery.visibilityBuffer().length;
    }

    // FIXME: we can only implement a subset of what the WebGPU specification promises, basically
    // the start and end times of the vertex and fragment stages
    if (auto* timestampWrites = descriptor.timestampWrites) {
        ASSERT(descriptor.timestampWriteCount > 0);
        auto& timestampWrite = descriptor.timestampWrites[0];
        auto& querySet = fromAPI(timestampWrite.querySet);

        MTLRenderPassSampleBufferAttachmentDescriptor *sampleAttachment = mtlDescriptor.sampleBufferAttachments[0];
        sampleAttachment.sampleBuffer = querySet.counterSampleBuffer();
        sampleAttachment.startOfVertexSampleIndex = 0;
        sampleAttachment.endOfVertexSampleIndex = 1;
        sampleAttachment.startOfFragmentSampleIndex = 2;
        sampleAttachment.endOfFragmentSampleIndex = 3;
    }

    auto mtlRenderCommandEncoder = [m_commandBuffer renderCommandEncoderWithDescriptor:mtlDescriptor];

    return RenderPassEncoder::create(mtlRenderCommandEncoder, visibilityResultBufferSize, m_device);
}

bool CommandEncoder::validateCopyBufferToBuffer(const Buffer& source, uint64_t sourceOffset, const Buffer& destination, uint64_t destinationOffset, uint64_t size)
{
    if (!isValidToUseWith(source, *this))
        return false;

    if (!isValidToUseWith(destination, *this))
        return false;

    if (!(source.usage() & WGPUBufferUsage_CopySrc))
        return false;

    if (!(destination.usage() & WGPUBufferUsage_CopyDst))
        return false;

    if (size % 4)
        return false;

    if (sourceOffset % 4)
        return false;

    if (destinationOffset % 4)
        return false;

    auto sourceEnd = checkedSum<uint64_t>(sourceOffset, size);
    if (sourceEnd.hasOverflowed())
        return false;

    auto destinationEnd = checkedSum<uint64_t>(destinationOffset, size);
    if (destinationEnd.hasOverflowed())
        return false;

    if (source.size() < sourceEnd.value())
        return false;

    if (destination.size() < destinationEnd.value())
        return false;

    if (&source == &destination)
        return false;

    return true;
}

void CommandEncoder::copyBufferToBuffer(const Buffer& source, uint64_t sourceOffset, const Buffer& destination, uint64_t destinationOffset, uint64_t size)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-copybuffertobuffer

    if (!prepareTheEncoderState())
        return;

    if (!validateCopyBufferToBuffer(source, sourceOffset, destination, destinationOffset, size)) {
        m_device->generateAValidationError("Validation failure."_s);
        return;
    }

    ensureBlitCommandEncoder();

    [m_blitCommandEncoder copyFromBuffer:source.buffer() sourceOffset:static_cast<NSUInteger>(sourceOffset) toBuffer:destination.buffer() destinationOffset:static_cast<NSUInteger>(destinationOffset) size:static_cast<NSUInteger>(size)];
}

static bool validateImageCopyBuffer(const WGPUImageCopyBuffer& imageCopyBuffer)
{
    // https://gpuweb.github.io/gpuweb/#abstract-opdef-validating-gpuimagecopybuffer

    if (!fromAPI(imageCopyBuffer.buffer).isValid())
        return false;

    if (imageCopyBuffer.layout.bytesPerRow % 256)
        return false;

    return false;
}

static bool refersToAllAspects(WGPUTextureFormat format, WGPUTextureAspect aspect)
{
    switch (aspect) {
    case WGPUTextureAspect_All:
        return true;
    case WGPUTextureAspect_StencilOnly:
        return Texture::containsStencilAspect(format) && !Texture::containsDepthAspect(format);
    case WGPUTextureAspect_DepthOnly:
        return Texture::containsDepthAspect(format) && !Texture::containsStencilAspect(format);
    case WGPUTextureAspect_Force32:
        ASSERT_NOT_REACHED();
        return false;
    }
}

static bool validateCopyBufferToTexture(const WGPUImageCopyBuffer& source, const WGPUImageCopyTexture& destination, const WGPUExtent3D& copySize)
{
    const auto& dstTextureDesc = fromAPI(destination.texture).descriptor();

    if (!validateImageCopyBuffer(source))
        return false;

    if (!(fromAPI(source.buffer).usage() & WGPUBufferUsage_CopySrc))
        return false;

    if (!Texture::validateImageCopyTexture(destination, copySize))
        return false;

    if (!(dstTextureDesc.usage & WGPUTextureUsage_CopyDst))
        return false;

    if (dstTextureDesc.sampleCount != 1)
        return false;

    WGPUTextureFormat aspectSpecificFormat = dstTextureDesc.format;

    if (Texture::isDepthOrStencilFormat(dstTextureDesc.format)) {
        if (!Texture::refersToSingleAspect(dstTextureDesc.format, destination.aspect))
            return false;

        if (!Texture::isValidImageCopyDestination(dstTextureDesc.format, destination.aspect))
            return false;

        aspectSpecificFormat = Texture::aspectSpecificFormat(dstTextureDesc.format, destination.aspect);
    }

    if (!Texture::validateTextureCopyRange(destination, copySize))
        return false;

    if (!Texture::isDepthOrStencilFormat(dstTextureDesc.format)) {
        auto texelBlockSize = Texture::texelBlockSize(dstTextureDesc.format);
        if (source.layout.offset % texelBlockSize)
            return false;
    }

    if (Texture::isDepthOrStencilFormat(dstTextureDesc.format)) {
        if (source.layout.offset % 4)
            return false;
    }

    if (!Texture::validateLinearTextureData(source.layout, fromAPI(source.buffer).size(), aspectSpecificFormat, copySize))
        return false;

    return true;
}

void CommandEncoder::copyBufferToTexture(const WGPUImageCopyBuffer& source, const WGPUImageCopyTexture& destination, const WGPUExtent3D& copySize)
{
    if (source.nextInChain || source.layout.nextInChain || destination.nextInChain)
        return;

    // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-copybuffertotexture

    if (!prepareTheEncoderState())
        return;

    if (!validateCopyBufferToTexture(source, destination, copySize)) {
        m_device->generateAValidationError("Validation failure."_s);
        return;
    }

    ensureBlitCommandEncoder();

    NSUInteger sourceBytesPerRow = source.layout.bytesPerRow;

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
        ASSERT_NOT_REACHED();
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
        ASSERT_NOT_REACHED();
        return;
    }
}

static bool validateCopyTextureToBuffer(const WGPUImageCopyTexture& source, const WGPUImageCopyBuffer& destination, const WGPUExtent3D& copySize)
{
    const auto& srcTextureDesc = fromAPI(source.texture).descriptor();

    if (!Texture::validateImageCopyTexture(source, copySize))
        return false;

    if (!(srcTextureDesc.usage & WGPUBufferUsage_CopySrc))
        return false;

    if (srcTextureDesc.sampleCount != 1)
        return false;

    WGPUTextureFormat aspectSpecificFormat = srcTextureDesc.format;

    if (Texture::isDepthOrStencilFormat(srcTextureDesc.format)) {
        if (!Texture::refersToSingleAspect(srcTextureDesc.format, source.aspect))
            return false;

        if (!Texture::isValidImageCopySource(srcTextureDesc.format, source.aspect))
            return false;

        aspectSpecificFormat = Texture::aspectSpecificFormat(srcTextureDesc.format, source.aspect);
    }

    if (!validateImageCopyBuffer(destination))
        return false;

    if (!(fromAPI(destination.buffer).usage() & WGPUBufferUsage_CopyDst))
        return false;

    if (!Texture::validateTextureCopyRange(source, copySize))
        return false;

    if (!Texture::isDepthOrStencilFormat(srcTextureDesc.format)) {
        auto texelBlockSize = Texture::texelBlockSize(srcTextureDesc.format);
        if (destination.layout.offset % texelBlockSize)
            return false;
    }

    if (Texture::isDepthOrStencilFormat(srcTextureDesc.format)) {
        if (destination.layout.offset % 4)
            return false;
    }

    if (!Texture::validateLinearTextureData(destination.layout, fromAPI(destination.buffer).size(), aspectSpecificFormat, copySize))
        return false;

    return true;
}

void CommandEncoder::copyTextureToBuffer(const WGPUImageCopyTexture& source, const WGPUImageCopyBuffer& destination, const WGPUExtent3D& copySize)
{
    if (source.nextInChain || destination.nextInChain || destination.layout.nextInChain)
        return;

    // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-copytexturetobuffer

    if (!prepareTheEncoderState())
        return;

    if (!validateCopyTextureToBuffer(source, destination, copySize)) {
        m_device->generateAValidationError("Validation failure."_s);
        return;
    }

    ensureBlitCommandEncoder();

    NSUInteger destinationBytesPerRow = destination.layout.bytesPerRow;

    NSUInteger destinationBytesPerImage = destination.layout.rowsPerImage * destination.layout.bytesPerRow;

    MTLBlitOption options = MTLBlitOptionNone;
    switch (source.aspect) {
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
        ASSERT_NOT_REACHED();
        return;
    }

    auto logicalSize = fromAPI(source.texture).logicalMiplevelSpecificTextureExtent(source.mipLevel);
    auto widthForMetal = std::min(copySize.width, logicalSize.width);
    auto heightForMetal = std::min(copySize.height, logicalSize.height);
    auto depthForMetal = std::min(copySize.depthOrArrayLayers, logicalSize.depthOrArrayLayers);

    auto& sourceDescriptor = fromAPI(source.texture).descriptor();
    switch (sourceDescriptor.dimension) {
    case WGPUTextureDimension_1D: {
        // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400756-copyfromtexture?language=objc
        // "When you copy to a 1D texture, height and depth must be 1."
        auto sourceSize = MTLSizeMake(widthForMetal, 1, 1);
        auto sourceOrigin = MTLOriginMake(source.origin.x, 1, 1);
        for (uint32_t layer = 0; layer < copySize.depthOrArrayLayers; ++layer) {
            auto destinationOffset = static_cast<NSUInteger>(destination.layout.offset + layer * destinationBytesPerImage);
            NSUInteger sourceSlice = source.origin.z + layer;
            [m_blitCommandEncoder
                copyFromTexture:fromAPI(source.texture).texture()
                sourceSlice:sourceSlice
                sourceLevel:source.mipLevel
                sourceOrigin:sourceOrigin
                sourceSize:sourceSize
                toBuffer:fromAPI(destination.buffer).buffer()
                destinationOffset:destinationOffset
                destinationBytesPerRow:destinationBytesPerRow
                destinationBytesPerImage:destinationBytesPerImage
                options:options];
        }
        break;
    }
    case WGPUTextureDimension_2D: {
        // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400756-copyfromtexture?language=objc
        // "When you copy to a 2D texture, depth must be 1."
        auto sourceSize = MTLSizeMake(widthForMetal, heightForMetal, 1);
        auto sourceOrigin = MTLOriginMake(source.origin.x, source.origin.y, 1);
        for (uint32_t layer = 0; layer < copySize.depthOrArrayLayers; ++layer) {
            auto destinationOffset = static_cast<NSUInteger>(destination.layout.offset + layer * destinationBytesPerImage);
            NSUInteger sourceSlice = source.origin.z + layer;
            [m_blitCommandEncoder
                copyFromTexture:fromAPI(source.texture).texture()
                sourceSlice:sourceSlice
                sourceLevel:source.mipLevel
                sourceOrigin:sourceOrigin
                sourceSize:sourceSize
                toBuffer:fromAPI(destination.buffer).buffer()
                destinationOffset:destinationOffset
                destinationBytesPerRow:destinationBytesPerRow
                destinationBytesPerImage:destinationBytesPerImage
                options:options];
        }
        break;
    }
    case WGPUTextureDimension_3D: {
        auto sourceSize = MTLSizeMake(widthForMetal, heightForMetal, depthForMetal);
        auto sourceOrigin = MTLOriginMake(source.origin.x, source.origin.y, source.origin.z);
        auto destinationOffset = static_cast<NSUInteger>(destination.layout.offset);
            [m_blitCommandEncoder
                copyFromTexture:fromAPI(source.texture).texture()
                sourceSlice:0
                sourceLevel:source.mipLevel
                sourceOrigin:sourceOrigin
                sourceSize:sourceSize
                toBuffer:fromAPI(destination.buffer).buffer()
                destinationOffset:destinationOffset
                destinationBytesPerRow:destinationBytesPerRow
                destinationBytesPerImage:destinationBytesPerImage
                options:options];
        break;
    }
    case WGPUTextureDimension_Force32:
        ASSERT_NOT_REACHED();
        return;
    }
}

static bool areCopyCompatible(WGPUTextureFormat format1, WGPUTextureFormat format2)
{
    // https://gpuweb.github.io/gpuweb/#copy-compatible

    if (format1 == format2)
        return true;

    return Texture::removeSRGBSuffix(format1) == Texture::removeSRGBSuffix(format2);
}

static bool validateCopyTextureToTexture(const WGPUImageCopyTexture& source, const WGPUImageCopyTexture& destination, const WGPUExtent3D& copySize)
{
    const auto& srcTextureDesc = fromAPI(source.texture).descriptor();

    const auto& dstTextureDesc = fromAPI(destination.texture).descriptor();

    if (!Texture::validateImageCopyTexture(source, copySize))
        return false;

    if (!(srcTextureDesc.usage & WGPUTextureUsage_CopySrc))
        return false;

    if (!Texture::validateImageCopyTexture(destination, copySize))
        return false;

    if (!(dstTextureDesc.usage & WGPUTextureUsage_CopyDst))
        return false;

    if (srcTextureDesc.sampleCount != dstTextureDesc.sampleCount)
        return false;

    if (!areCopyCompatible(srcTextureDesc.format, dstTextureDesc.format))
        return false;

    if (Texture::isDepthOrStencilFormat(srcTextureDesc.format)) {
        if (!refersToAllAspects(srcTextureDesc.format, source.aspect)
            || !refersToAllAspects(dstTextureDesc.format, destination.aspect))
            return false;
    }

    if (!Texture::validateTextureCopyRange(source, copySize))
        return false;

    if (!Texture::validateTextureCopyRange(destination, copySize))
        return false;

    // https://gpuweb.github.io/gpuweb/#abstract-opdef-set-of-subresources-for-texture-copy
    if (source.texture == destination.texture) {
        // Mip levels are never ranges.
        if (source.mipLevel == destination.mipLevel) {
            switch (fromAPI(source.texture).descriptor().dimension) {
            case WGPUTextureDimension_1D:
                return false;
            case WGPUTextureDimension_2D: {
                Range<uint32_t> sourceRange(source.origin.z, source.origin.z + copySize.depthOrArrayLayers);
                Range<uint32_t> destinationRange(destination.origin.z, source.origin.z + copySize.depthOrArrayLayers);
                if (sourceRange.overlaps(destinationRange))
                    return false;
                break;
            }
            case WGPUTextureDimension_3D:
                return false;
            case WGPUTextureDimension_Force32:
                ASSERT_NOT_REACHED();
                return false;
            }
        }
    }

    return true;
}

void CommandEncoder::copyTextureToTexture(const WGPUImageCopyTexture& source, const WGPUImageCopyTexture& destination, const WGPUExtent3D& copySize)
{
    if (source.nextInChain || destination.nextInChain)
        return;

    // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-copytexturetotexture

    if (!prepareTheEncoderState())
        return;

    if (!validateCopyTextureToTexture(source, destination, copySize)) {
        m_device->generateAValidationError("Validation failure."_s);
        return;
    }

    ensureBlitCommandEncoder();

    auto& sourceDescriptor = fromAPI(source.texture).descriptor();
    // FIXME(PERFORMANCE): Is it actually faster to use the -[MTLBlitCommandEncoder copyFromTexture:...toTexture:...levelCount:]
    // variant, where possible, rather than calling the other variant in a loop?
    switch (sourceDescriptor.dimension) {
    case WGPUTextureDimension_1D: {
        // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400756-copyfromtexture?language=objc
        // "When you copy to a 1D texture, height and depth must be 1."
        auto sourceSize = MTLSizeMake(copySize.width, 1, 1);
        auto sourceOrigin = MTLOriginMake(source.origin.x, 1, 1);
        auto destinationOrigin = MTLOriginMake(destination.origin.x, 1, 1);
        for (uint32_t layer = 0; layer < copySize.depthOrArrayLayers; ++layer) {
            NSUInteger sourceSlice = source.origin.z + layer;
            NSUInteger destinationSlice = destination.origin.z + layer;
            [m_blitCommandEncoder
                copyFromTexture:fromAPI(source.texture).texture()
                sourceSlice:sourceSlice
                sourceLevel:source.mipLevel
                sourceOrigin:sourceOrigin
                sourceSize:sourceSize
                toTexture:fromAPI(destination.texture).texture()
                destinationSlice:destinationSlice
                destinationLevel:destination.mipLevel
                destinationOrigin:destinationOrigin];
        }
        break;
    }
    case WGPUTextureDimension_2D: {
        // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400756-copyfromtexture?language=objc
        // "When you copy to a 2D texture, depth must be 1."
        auto sourceSize = MTLSizeMake(copySize.width, copySize.height, 1);
        auto sourceOrigin = MTLOriginMake(source.origin.x, source.origin.y, 1);
        auto destinationOrigin = MTLOriginMake(destination.origin.x, destination.origin.y, 1);
        for (uint32_t layer = 0; layer < copySize.depthOrArrayLayers; ++layer) {
            NSUInteger sourceSlice = source.origin.z + layer;
            NSUInteger destinationSlice = destination.origin.z + layer;
            [m_blitCommandEncoder
                copyFromTexture:fromAPI(source.texture).texture()
                sourceSlice:sourceSlice
                sourceLevel:source.mipLevel
                sourceOrigin:sourceOrigin
                sourceSize:sourceSize
                toTexture:fromAPI(destination.texture).texture()
                destinationSlice:destinationSlice
                destinationLevel:destination.mipLevel
                destinationOrigin:destinationOrigin];
        }
        break;
    }
    case WGPUTextureDimension_3D: {
        auto sourceSize = MTLSizeMake(copySize.width, copySize.height, copySize.depthOrArrayLayers);
        auto sourceOrigin = MTLOriginMake(source.origin.x, source.origin.y, source.origin.z);
        auto destinationOrigin = MTLOriginMake(destination.origin.x, destination.origin.y, destination.origin.z);
        [m_blitCommandEncoder
            copyFromTexture:fromAPI(source.texture).texture()
            sourceSlice:0
            sourceLevel:source.mipLevel
            sourceOrigin:sourceOrigin
            sourceSize:sourceSize
            toTexture:fromAPI(destination.texture).texture()
            destinationSlice:0
            destinationLevel:destination.mipLevel
            destinationOrigin:destinationOrigin];
        break;
    }
    case WGPUTextureDimension_Force32:
        ASSERT_NOT_REACHED();
        return;
    }
}

bool CommandEncoder::validateClearBuffer(const Buffer& buffer, uint64_t offset, uint64_t size)
{
    if (!isValidToUseWith(buffer, *this))
        return false;

    if (!(buffer.usage() & WGPUBufferUsage_CopyDst))
        return false;

    if (size % 4)
        return false;

    if (offset % 4)
        return false;

    auto end = checkedSum<uint64_t>(offset, size);
    if (end.hasOverflowed() || buffer.size() < end.value())
        return false;

    return true;
}

void CommandEncoder::clearBuffer(const Buffer& buffer, uint64_t offset, uint64_t size)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-clearbuffer

    if (!prepareTheEncoderState())
        return;

    if (size == WGPU_WHOLE_SIZE) {
        auto localSize = checkedDifference<uint64_t>(buffer.size(), offset);
        if (localSize.hasOverflowed()) {
            m_device->generateAValidationError("CommandEncoder::clearBuffer(): offset > buffer.size"_s);
            return;
        }
        size = localSize.value();
    }

    if (!validateClearBuffer(buffer, offset, size)) {
        m_device->generateAValidationError("Validation failure."_s);
        return;
    }

    ensureBlitCommandEncoder();

    [m_blitCommandEncoder fillBuffer:buffer.buffer() range:NSMakeRange(static_cast<NSUInteger>(offset), static_cast<NSUInteger>(size)) value:0];
}

bool CommandEncoder::validateFinish() const
{
    if (!isValid())
        return false;

    if (m_state != EncoderState::Open)
        return false;

    // FIXME: "this.[[debug_group_stack]] must be empty."

    // FIXME: "Every usage scope contained in this must satisfy the usage scope validation."

    return true;
}

Ref<CommandBuffer> CommandEncoder::finish(const WGPUCommandBufferDescriptor& descriptor)
{
    if (descriptor.nextInChain)
        return CommandBuffer::createInvalid(m_device);

    // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-finish

    auto validationFailed = !validateFinish();

    m_state = EncoderState::Ended;

    if (validationFailed) {
        m_device->generateAValidationError("Validation failure."_s);
        return CommandBuffer::createInvalid(m_device);
    }

    finalizeBlitCommandEncoder();

    auto *commandBuffer = m_commandBuffer;
    m_commandBuffer = nil;

    commandBuffer.label = fromAPI(descriptor.label);

    return CommandBuffer::create(commandBuffer, m_device);
}

void CommandEncoder::insertDebugMarker(String&& markerLabel)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-insertdebugmarker

    if (!prepareTheEncoderState())
        return;

    finalizeBlitCommandEncoder();

    // There's no direct way of doing this, so we just push/pop an empty debug group.
    [m_commandBuffer pushDebugGroup:markerLabel];
    [m_commandBuffer popDebugGroup];
}

bool CommandEncoder::validatePopDebugGroup() const
{
    if (!m_debugGroupStackSize)
        return false;

    return true;
}

void CommandEncoder::popDebugGroup()
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-popdebuggroup

    if (!prepareTheEncoderState())
        return;

    if (!validatePopDebugGroup()) {
        makeInvalid();
        return;
    }

    finalizeBlitCommandEncoder();

    --m_debugGroupStackSize;
    [m_commandBuffer popDebugGroup];
}

void CommandEncoder::pushDebugGroup(String&& groupLabel)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-pushdebuggroup

    if (!prepareTheEncoderState())
        return;
    
    finalizeBlitCommandEncoder();

    ++m_debugGroupStackSize;
    [m_commandBuffer pushDebugGroup:groupLabel];
}

void CommandEncoder::resolveQuerySet(const QuerySet& querySet, uint32_t firstQuery, uint32_t queryCount, const Buffer& destination, uint64_t destinationOffset)
{
    if (querySet.queryCount() < firstQuery + queryCount)
        return;

    auto block = [&querySet, firstQuery, queryCount, &destination, destinationOffset](id<MTLCommandBuffer>) {
        if (querySet.counterSampleBuffer()) {
            auto timestamps = querySet.resolveTimestamps();
            memcpy(static_cast<char*>(destination.buffer().contents) + destinationOffset, &timestamps[firstQuery], sizeof(uint64_t) * queryCount);
            return;
        }

        id<MTLBuffer> visibilityBuffer = querySet.visibilityBuffer();
        ASSERT(visibilityBuffer.length);
        memcpy(static_cast<char*>(destination.buffer().contents) + destinationOffset, (char*)visibilityBuffer.contents + sizeof(uint64_t) * firstQuery, sizeof(uint64_t) * queryCount);
    };

    if (m_commandBuffer)
        [m_commandBuffer addCompletedHandler:block];
    else
        block(nil);
}

void CommandEncoder::writeTimestamp(QuerySet& querySet, uint32_t queryIndex)
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
