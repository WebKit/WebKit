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
    case WGPULoadOp_Undefined:
    case WGPULoadOp_Force32:
        ASSERT_NOT_REACHED();
        return MTLLoadActionDontCare;
    }
}

static MTLStoreAction storeAction(WGPUStoreOp storeOp)
{
    switch (storeOp) {
    case WGPUStoreOp_Store:
        return MTLStoreActionStore;
    case WGPUStoreOp_Discard:
        return MTLStoreActionDontCare;
    case WGPUStoreOp_Undefined:
    case WGPUStoreOp_Force32:
        ASSERT_NOT_REACHED();
        return MTLStoreActionDontCare;
    }
}

Ref<CommandEncoder> Device::createCommandEncoder(const WGPUCommandEncoderDescriptor& descriptor)
{
    if (descriptor.nextInChain || !isValid())
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
    if (m_blitCommandEncoder && m_pendingTimestampWrites.isEmpty())
        return;

    auto pendingTimestampWrites = std::exchange(m_pendingTimestampWrites, { });
    if (m_blitCommandEncoder && !pendingTimestampWrites.isEmpty())
        finalizeBlitCommandEncoder();

    MTLBlitPassDescriptor *descriptor = [MTLBlitPassDescriptor new];
    ASSERT(pendingTimestampWrites.isEmpty() || m_device->baseCapabilities().counterSamplingAPI == HardwareCapabilities::BaseCapabilities::CounterSamplingAPI::StageBoundary);
    // FIXME: rdar://91371495 This approach won't actually work; we need to work around this limitation.
    for (size_t i = 0; i < pendingTimestampWrites.size(); ++i) {
        const auto& pendingTimestampWrite = pendingTimestampWrites[i];
        descriptor.sampleBufferAttachments[i].sampleBuffer = pendingTimestampWrite.querySet->counterSampleBuffer();
        descriptor.sampleBufferAttachments[i].startOfEncoderSampleIndex = pendingTimestampWrite.queryIndex;
    }
    m_blitCommandEncoder = [m_commandBuffer blitCommandEncoderWithDescriptor:descriptor];
}

void CommandEncoder::finalizeBlitCommandEncoder()
{
    if (m_blitCommandEncoder) {
        [m_blitCommandEncoder endEncoding];
        m_blitCommandEncoder = nil;
    }

    if (!m_pendingTimestampWrites.isEmpty()) {
        ASSERT(m_device->baseCapabilities().counterSamplingAPI == HardwareCapabilities::BaseCapabilities::CounterSamplingAPI::StageBoundary);
        ensureBlitCommandEncoder();
        finalizeBlitCommandEncoder();
    }
}

bool CommandEncoder::validateComputePassDescriptor(const WGPUComputePassDescriptor& descriptor) const
{
    // FIXME: Implement this according to
    // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-begincomputepass.

    if (descriptor.timestampWrites) {
        if (!m_device->hasFeature(WGPUFeatureName_TimestampQuery))
            return false;

        const auto& timestampWrite = *descriptor.timestampWrites;
        auto querySetCount = fromAPI(timestampWrite.querySet).count();
        if (timestampWrite.beginningOfPassWriteIndex >= querySetCount || timestampWrite.endOfPassWriteIndex >= querySetCount)
            return false;
    }

    return true;
}

Ref<ComputePassEncoder> CommandEncoder::beginComputePass(const WGPUComputePassDescriptor& descriptor)
{
    if (descriptor.nextInChain)
        return ComputePassEncoder::createInvalid(m_device);

    if (!validateComputePassDescriptor(descriptor))
        return ComputePassEncoder::createInvalid(m_device);

    finalizeBlitCommandEncoder();

    MTLComputePassDescriptor* computePassDescriptor = [MTLComputePassDescriptor new];
    computePassDescriptor.dispatchType = MTLDispatchTypeSerial;

    if (m_device->baseCapabilities().counterSamplingAPI == HardwareCapabilities::BaseCapabilities::CounterSamplingAPI::StageBoundary
        && descriptor.timestampWrites) {
        // rdar://91371495 is about how we can't just naively transform descriptor.timestampWrites into computePassDescriptor.sampleBufferAttachments.
        // Instead, we can resolve all the information to a dummy counter sample buffer, and then internally remember that the data
        // is in a different place than where it's supposed to be. Later, when we resolve the data, we can resolve it from our dummy
        // buffer instead of from where it's supposed to be.
        //
        // When rdar://91371495 is fixed, we can delete this indirection, and put the data directly where it's supposed to go.

        MTLCounterSampleBufferDescriptor *counterSampleBufferDescriptor = [MTLCounterSampleBufferDescriptor new];
        counterSampleBufferDescriptor.counterSet = m_device->baseCapabilities().timestampCounterSet;
        counterSampleBufferDescriptor.label = @"Dummy compute pass timestamp counter sample buffer";
        counterSampleBufferDescriptor.storageMode = MTLStorageModePrivate;
        counterSampleBufferDescriptor.sampleCount = 2;
        auto counterSampleBuffer = [m_device->device() newCounterSampleBufferWithDescriptor:counterSampleBufferDescriptor error:nil];
        // FIXME: We should probably do something sensible if the counter sample buffer failed to be created.
        auto dummyQuerySet = QuerySet::create(counterSampleBuffer, 2, WGPUQueryType_Timestamp, m_device);

        const auto startIndex = 0;
        const auto endIndex = 1;

        computePassDescriptor.sampleBufferAttachments[0].sampleBuffer = counterSampleBuffer;
        computePassDescriptor.sampleBufferAttachments[0].startOfEncoderSampleIndex = startIndex;
        computePassDescriptor.sampleBufferAttachments[0].endOfEncoderSampleIndex = endIndex;

        auto& timestampWrite = *descriptor.timestampWrites;
        fromAPI(timestampWrite.querySet).setOverrideLocation(dummyQuerySet, timestampWrite.beginningOfPassWriteIndex, timestampWrite.endOfPassWriteIndex);
    }

    id<MTLComputeCommandEncoder> computeCommandEncoder = [m_commandBuffer computeCommandEncoderWithDescriptor:computePassDescriptor];
    computeCommandEncoder.label = fromAPI(descriptor.label);

    return ComputePassEncoder::create(computeCommandEncoder, descriptor, *this, m_device);
}

bool CommandEncoder::validateRenderPassDescriptor(const WGPURenderPassDescriptor& descriptor) const
{
    // FIXME: Implement this according to
    // https://gpuweb.github.io/gpuweb/#dom-gpucommandencoder-beginrenderpass.

    if (descriptor.timestampWrites) {
        if (!m_device->hasFeature(WGPUFeatureName_TimestampQuery))
            return false;

        const auto& timestampWrite = *descriptor.timestampWrites;
        auto querySetCount = fromAPI(timestampWrite.querySet).count();
        if (timestampWrite.beginningOfPassWriteIndex >= querySetCount || timestampWrite.endOfPassWriteIndex >= querySetCount)
            return false;
    }

    return true;
}

bool Device::isStencilOnlyFormat(MTLPixelFormat format)
{
    switch (format) {
    case MTLPixelFormatStencil8:
    case MTLPixelFormatX32_Stencil8:
        return true;
    default:
        return false;
    }
}

Ref<RenderPassEncoder> CommandEncoder::beginRenderPass(const WGPURenderPassDescriptor& descriptor)
{
    if (descriptor.nextInChain)
        return RenderPassEncoder::createInvalid(m_device);

    if (!validateRenderPassDescriptor(descriptor))
        return RenderPassEncoder::createInvalid(m_device);

    MTLRenderPassDescriptor* mtlDescriptor = [MTLRenderPassDescriptor new];

    if (descriptor.colorAttachmentCount > 8)
        return RenderPassEncoder::createInvalid(m_device);

    finalizeBlitCommandEncoder();

    for (uint32_t i = 0; i < descriptor.colorAttachmentCount; ++i) {
        const auto& attachment = descriptor.colorAttachments[i];
        if (!attachment.view)
            continue;

        const auto& mtlAttachment = mtlDescriptor.colorAttachments[i];

        mtlAttachment.clearColor = MTLClearColorMake(attachment.clearValue.r,
            attachment.clearValue.g,
            attachment.clearValue.b,
            attachment.clearValue.a);

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
        }
    }

    bool depthReadOnly = false, stencilReadOnly = false;
    bool hasStencilComponent = false;
    if (const auto* attachment = descriptor.depthStencilAttachment) {
        auto& textureView = fromAPI(attachment->view);
        id<MTLTexture> metalDepthStencilTexture = textureView.texture();
        hasStencilComponent = Texture::stencilOnlyAspectMetalFormat(textureView.descriptor().format).has_value();
        if (!Device::isStencilOnlyFormat(metalDepthStencilTexture.pixelFormat)) {
            const auto& mtlAttachment = mtlDescriptor.depthAttachment;
            depthReadOnly = attachment->depthReadOnly;
            mtlAttachment.clearDepth = attachment->depthClearValue;
            mtlAttachment.texture = metalDepthStencilTexture;
            mtlAttachment.loadAction = loadAction(attachment->depthLoadOp);
            mtlAttachment.storeAction = storeAction(attachment->depthStoreOp);
        }
    }

    if (const auto* attachment = descriptor.depthStencilAttachment) {
        const auto& mtlAttachment = mtlDescriptor.stencilAttachment;
        stencilReadOnly = attachment->stencilReadOnly;
        if (hasStencilComponent)
            mtlAttachment.texture = fromAPI(attachment->view).texture();
        mtlAttachment.clearStencil = attachment->stencilClearValue;
        mtlAttachment.loadAction = loadAction(attachment->stencilLoadOp);
        mtlAttachment.storeAction = storeAction(attachment->stencilStoreOp);
    }

    size_t visibilityResultBufferSize = 0;
    if (auto* wgpuOcclusionQuery = descriptor.occlusionQuerySet) {
        const auto& occlusionQuery = fromAPI(wgpuOcclusionQuery);
        mtlDescriptor.visibilityResultBuffer = occlusionQuery.visibilityBuffer();
        visibilityResultBufferSize = occlusionQuery.visibilityBuffer().length;
    }

    if (m_device->baseCapabilities().counterSamplingAPI == HardwareCapabilities::BaseCapabilities::CounterSamplingAPI::StageBoundary
        && descriptor.timestampWrites) {
        // rdar://91371495 is about how we can't just naively transform descriptor.timestampWrites into computePassDescriptor.sampleBufferAttachments.
        // Instead, we can resolve all the information to a dummy counter sample buffer, and then internally remember that the data
        // is in a different place than where it's supposed to be. Later, when we resolve the data, we can resolve it from our dummy
        // buffer instead of from where it's supposed to be.
        //
        // When rdar://91371495 is fixed, we can delete this indirection, and put the data directly where it's supposed to go.

        MTLCounterSampleBufferDescriptor *counterSampleBufferDescriptor = [MTLCounterSampleBufferDescriptor new];
        counterSampleBufferDescriptor.counterSet = m_device->baseCapabilities().timestampCounterSet;
        counterSampleBufferDescriptor.label = @"Dummy render pass timestamp counter sample buffer";
        counterSampleBufferDescriptor.storageMode = MTLStorageModePrivate;
        counterSampleBufferDescriptor.sampleCount = 4;
        auto counterSampleBuffer = [m_device->device() newCounterSampleBufferWithDescriptor:counterSampleBufferDescriptor error:nil];
        // FIXME: We should probably do something sensible if the counter sample buffer failed to be created.
        auto dummyQuerySet = QuerySet::create(counterSampleBuffer, 4, WGPUQueryType_Timestamp, m_device);

        const auto startVertexIndex = 0;
        const auto endVertexIndex = 1;
        const auto startFragmentIndex = 2;
        const auto endFragmentIndex = 3;

        mtlDescriptor.sampleBufferAttachments[0].sampleBuffer = counterSampleBuffer;
        mtlDescriptor.sampleBufferAttachments[0].startOfVertexSampleIndex = startVertexIndex;
        mtlDescriptor.sampleBufferAttachments[0].endOfVertexSampleIndex = endVertexIndex;
        mtlDescriptor.sampleBufferAttachments[0].startOfFragmentSampleIndex = startFragmentIndex;
        mtlDescriptor.sampleBufferAttachments[0].endOfFragmentSampleIndex = endFragmentIndex;

        auto& timestampWrite = *descriptor.timestampWrites;
        fromAPI(timestampWrite.querySet).setOverrideLocation(dummyQuerySet, timestampWrite.beginningOfPassWriteIndex, timestampWrite.endOfPassWriteIndex);
    }

    auto mtlRenderCommandEncoder = [m_commandBuffer renderCommandEncoderWithDescriptor:mtlDescriptor];

    return RenderPassEncoder::create(mtlRenderCommandEncoder, descriptor, visibilityResultBufferSize, depthReadOnly, stencilReadOnly, *this, m_device);
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

    if (!prepareTheEncoderState() || !size)
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

    if (imageCopyBuffer.layout.bytesPerRow != WGPU_COPY_STRIDE_UNDEFINED && (imageCopyBuffer.layout.bytesPerRow % 256))
        return false;

    return true;
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
    const auto& destinationTexture = fromAPI(destination.texture);

    if (!validateImageCopyBuffer(source))
        return false;

    if (!(fromAPI(source.buffer).usage() & WGPUBufferUsage_CopySrc))
        return false;

    if (!Texture::validateImageCopyTexture(destination, copySize))
        return false;

    if (!(destinationTexture.usage() & WGPUTextureUsage_CopyDst))
        return false;

    if (destinationTexture.sampleCount() != 1)
        return false;

    WGPUTextureFormat aspectSpecificFormat = destinationTexture.format();

    if (Texture::isDepthOrStencilFormat(destinationTexture.format())) {
        if (!Texture::refersToSingleAspect(destinationTexture.format(), destination.aspect))
            return false;

        if (!Texture::isValidDepthStencilCopyDestination(destinationTexture.format(), destination.aspect))
            return false;

        aspectSpecificFormat = Texture::aspectSpecificFormat(destinationTexture.format(), destination.aspect);
    }

    if (!Texture::validateTextureCopyRange(destination, copySize))
        return false;

    if (!Texture::isDepthOrStencilFormat(destinationTexture.format())) {
        auto texelBlockSize = Texture::texelBlockSize(destinationTexture.format());
        if (source.layout.offset % texelBlockSize)
            return false;
    }

    if (Texture::isDepthOrStencilFormat(destinationTexture.format())) {
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

    const auto& destinationTexture = fromAPI(destination.texture);
    if (!validateCopyBufferToTexture(source, destination, copySize)) {
        m_device->generateAValidationError("Validation failure."_s);
        return;
    }

    if (!copySize.width && !copySize.height && !copySize.depthOrArrayLayers)
        return;

    ensureBlitCommandEncoder();

    NSUInteger sourceBytesPerRow = source.layout.bytesPerRow;
    id<MTLBuffer> sourceBuffer = fromAPI(source.buffer).buffer();
    RELEASE_ASSERT(sourceBuffer);
    if (sourceBytesPerRow == WGPU_COPY_STRIDE_UNDEFINED)
        sourceBytesPerRow = sourceBuffer.length;

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

    auto rowsPerImage = source.layout.rowsPerImage;
    if (rowsPerImage == WGPU_COPY_STRIDE_UNDEFINED)
        rowsPerImage = heightForMetal ?: 1;

    NSUInteger sourceBytesPerImage = rowsPerImage * sourceBytesPerRow;

    id<MTLTexture> mtlDestinationTexture = fromAPI(destination.texture).texture();

    switch (destinationTexture.dimension()) {
    case WGPUTextureDimension_1D: {
        // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400771-copyfrombuffer?language=objc
        // "When you copy to a 1D texture, height and depth must be 1."
        auto sourceSize = MTLSizeMake(widthForMetal, 1, 1);
        if (!widthForMetal)
            return;

        auto destinationOrigin = MTLOriginMake(destination.origin.x, 0, 0);
        for (uint32_t layer = 0; layer < copySize.depthOrArrayLayers; ++layer) {
            auto sourceOffset = static_cast<NSUInteger>(source.layout.offset + layer * sourceBytesPerImage);
            NSUInteger destinationSlice = destination.origin.z + layer;
            [m_blitCommandEncoder
                copyFromBuffer:sourceBuffer
                sourceOffset:sourceOffset
                sourceBytesPerRow:sourceBytesPerRow
                sourceBytesPerImage:sourceBytesPerImage
                sourceSize:sourceSize
                toTexture:mtlDestinationTexture
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
        if (!widthForMetal || !heightForMetal)
            return;

        auto destinationOrigin = MTLOriginMake(destination.origin.x, destination.origin.y, 0);
        for (uint32_t layer = 0; layer < copySize.depthOrArrayLayers; ++layer) {
            auto sourceOffset = static_cast<NSUInteger>(source.layout.offset + layer * sourceBytesPerImage);

            NSUInteger destinationSlice = destination.origin.z + layer;
            [m_blitCommandEncoder
                copyFromBuffer:sourceBuffer
                sourceOffset:sourceOffset
                sourceBytesPerRow:sourceBytesPerRow
                sourceBytesPerImage:sourceBytesPerImage
                sourceSize:sourceSize
                toTexture:mtlDestinationTexture
                destinationSlice:destinationSlice
                destinationLevel:destination.mipLevel
                destinationOrigin:destinationOrigin
                options:options];
        }
        break;
    }
    case WGPUTextureDimension_3D: {
        auto sourceSize = MTLSizeMake(widthForMetal, heightForMetal, depthForMetal);
        if (!widthForMetal || !heightForMetal || !depthForMetal)
            return;

        auto destinationOrigin = MTLOriginMake(destination.origin.x, destination.origin.y, destination.origin.z);
        auto sourceOffset = static_cast<NSUInteger>(source.layout.offset);
        [m_blitCommandEncoder
            copyFromBuffer:sourceBuffer
            sourceOffset:sourceOffset
            sourceBytesPerRow:sourceBytesPerRow
            sourceBytesPerImage:sourceBytesPerImage
            sourceSize:sourceSize
            toTexture:mtlDestinationTexture
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
    const auto& sourceTexture = fromAPI(source.texture);

    if (!Texture::validateImageCopyTexture(source, copySize))
        return false;

    if (!(sourceTexture.usage() & WGPUTextureUsage_CopySrc))
        return false;

    if (sourceTexture.sampleCount() != 1)
        return false;

    WGPUTextureFormat aspectSpecificFormat = sourceTexture.format();

    if (Texture::isDepthOrStencilFormat(sourceTexture.format())) {
        if (!Texture::refersToSingleAspect(sourceTexture.format(), source.aspect))
            return false;

        if (!Texture::isValidDepthStencilCopySource(sourceTexture.format(), source.aspect))
            return false;

        aspectSpecificFormat = Texture::aspectSpecificFormat(sourceTexture.format(), source.aspect);
    }

    if (!validateImageCopyBuffer(destination))
        return false;

    if (!(fromAPI(destination.buffer).usage() & WGPUBufferUsage_CopyDst))
        return false;

    if (!Texture::validateTextureCopyRange(source, copySize))
        return false;

    if (!Texture::isDepthOrStencilFormat(sourceTexture.format())) {
        auto texelBlockSize = Texture::texelBlockSize(sourceTexture.format());
        if (destination.layout.offset % texelBlockSize)
            return false;
    }

    if (Texture::isDepthOrStencilFormat(sourceTexture.format())) {
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

    const auto& sourceTexture = fromAPI(source.texture);
    if (!validateCopyTextureToBuffer(source, destination, copySize)) {
        m_device->generateAValidationError("Validation failure."_s);
        return;
    }

    ensureBlitCommandEncoder();

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

    auto destinationBuffer = fromAPI(destination.buffer).buffer();
    NSUInteger destinationBytesPerRow = destination.layout.bytesPerRow;
    if (destinationBytesPerRow == WGPU_COPY_STRIDE_UNDEFINED)
        destinationBytesPerRow = destinationBuffer.length;

    auto rowsPerImage = destination.layout.rowsPerImage;
    if (rowsPerImage == WGPU_COPY_STRIDE_UNDEFINED)
        rowsPerImage = heightForMetal ?: 1;
    NSUInteger destinationBytesPerImage = rowsPerImage * destinationBytesPerRow;

    switch (sourceTexture.dimension()) {
    case WGPUTextureDimension_1D: {
        // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400756-copyfromtexture?language=objc
        // "When you copy to a 1D texture, height and depth must be 1."
        auto sourceSize = MTLSizeMake(widthForMetal, 1, 1);
        if (!widthForMetal)
            return;

        auto sourceOrigin = MTLOriginMake(source.origin.x, 0, 0);
        for (uint32_t layer = 0; layer < copySize.depthOrArrayLayers; ++layer) {
            auto destinationOffset = static_cast<NSUInteger>(destination.layout.offset + layer * destinationBytesPerImage);
            NSUInteger sourceSlice = source.origin.z + layer;
            [m_blitCommandEncoder
                copyFromTexture:fromAPI(source.texture).texture()
                sourceSlice:sourceSlice
                sourceLevel:source.mipLevel
                sourceOrigin:sourceOrigin
                sourceSize:sourceSize
                toBuffer:destinationBuffer
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
        if (!widthForMetal || !heightForMetal)
            return;

        auto sourceOrigin = MTLOriginMake(source.origin.x, source.origin.y, 0);
        for (uint32_t layer = 0; layer < copySize.depthOrArrayLayers; ++layer) {
            auto destinationOffset = static_cast<NSUInteger>(destination.layout.offset + layer * destinationBytesPerImage);
            NSUInteger sourceSlice = source.origin.z + layer;
            [m_blitCommandEncoder
                copyFromTexture:fromAPI(source.texture).texture()
                sourceSlice:sourceSlice
                sourceLevel:source.mipLevel
                sourceOrigin:sourceOrigin
                sourceSize:sourceSize
                toBuffer:destinationBuffer
                destinationOffset:destinationOffset
                destinationBytesPerRow:destinationBytesPerRow
                destinationBytesPerImage:destinationBytesPerImage
                options:options];
        }
        break;
    }
    case WGPUTextureDimension_3D: {
        auto sourceSize = MTLSizeMake(widthForMetal, heightForMetal, depthForMetal);
        if (!widthForMetal || !heightForMetal || !depthForMetal)
            return;

        auto sourceOrigin = MTLOriginMake(source.origin.x, source.origin.y, source.origin.z);
        auto destinationOffset = static_cast<NSUInteger>(destination.layout.offset);
            [m_blitCommandEncoder
                copyFromTexture:fromAPI(source.texture).texture()
                sourceSlice:0
                sourceLevel:source.mipLevel
                sourceOrigin:sourceOrigin
                sourceSize:sourceSize
                toBuffer:destinationBuffer
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
    const auto& sourceTexture = fromAPI(source.texture);

    const auto& destinationTexture = fromAPI(destination.texture);

    if (!Texture::validateImageCopyTexture(source, copySize))
        return false;

    if (!(sourceTexture.usage() & WGPUTextureUsage_CopySrc))
        return false;

    if (!Texture::validateImageCopyTexture(destination, copySize))
        return false;

    if (!(destinationTexture.usage() & WGPUTextureUsage_CopyDst))
        return false;

    if (sourceTexture.sampleCount() != destinationTexture.sampleCount())
        return false;

    if (!areCopyCompatible(sourceTexture.format(), destinationTexture.format()))
        return false;

    if (Texture::isDepthOrStencilFormat(sourceTexture.format())) {
        if (!refersToAllAspects(sourceTexture.format(), source.aspect)
            || !refersToAllAspects(destinationTexture.format(), destination.aspect))
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
            switch (fromAPI(source.texture).dimension()) {
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

    auto& sourceTexture = fromAPI(source.texture);

    // FIXME(PERFORMANCE): Is it actually faster to use the -[MTLBlitCommandEncoder copyFromTexture:...toTexture:...levelCount:]
    // variant, where possible, rather than calling the other variant in a loop?
    switch (sourceTexture.dimension()) {
    case WGPUTextureDimension_1D: {
        // https://developer.apple.com/documentation/metal/mtlblitcommandencoder/1400756-copyfromtexture?language=objc
        // "When you copy to a 1D texture, height and depth must be 1."
        auto sourceSize = MTLSizeMake(copySize.width, 1, 1);
        if (!sourceSize.width)
            return;

        auto sourceOrigin = MTLOriginMake(source.origin.x, 0, 0);
        auto destinationOrigin = MTLOriginMake(destination.origin.x, 0, 0);

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
        if (!sourceSize.width || !sourceSize.height)
            return;

        auto sourceOrigin = MTLOriginMake(source.origin.x, source.origin.y, 0);
        auto destinationOrigin = MTLOriginMake(destination.origin.x, destination.origin.y, 0);

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
        if (!sourceSize.width || !sourceSize.height || !sourceSize.depth)
            return;

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

    if (!prepareTheEncoderState() || !size)
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
    // FIXME: Validate this properly
    if (querySet.count() < firstQuery + queryCount)
        return;

    ensureBlitCommandEncoder();
    switch (querySet.type()) {
    case WGPUQueryType_Occlusion: {
        [m_blitCommandEncoder copyFromBuffer:querySet.visibilityBuffer() sourceOffset:sizeof(uint64_t) * firstQuery toBuffer:destination.buffer() destinationOffset:destinationOffset size:sizeof(uint64_t) * queryCount];
        break;
    }
    case WGPUQueryType_Timestamp: {
        querySet.encodeResolveCommands(m_blitCommandEncoder, firstQuery, queryCount, destination, destinationOffset);
        break;
    }
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

void CommandEncoder::writeTimestamp(QuerySet& querySet, uint32_t queryIndex)
{
    // FIXME: Add validation.

    if (!m_device->hasFeature(WGPUFeatureName_TimestampQuery))
        return;

    switch (m_device->baseCapabilities().counterSamplingAPI) {
    case HardwareCapabilities::BaseCapabilities::CounterSamplingAPI::StageBoundary:
        m_pendingTimestampWrites.append({ querySet, queryIndex });
        break;
    case HardwareCapabilities::BaseCapabilities::CounterSamplingAPI::CommandBoundary:
        ensureBlitCommandEncoder();
        [m_blitCommandEncoder sampleCountersInBuffer:querySet.counterSampleBuffer() atSampleIndex:queryIndex withBarrier:NO];
        break;
    }
}

void CommandEncoder::setLabel(String&& label)
{
    m_commandBuffer.label = label;
}

void CommandEncoder::lock(bool shouldLock)
{
    m_state = shouldLock ? EncoderState::Locked : EncoderState::Open;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuCommandEncoderReference(WGPUCommandEncoder commandEncoder)
{
    WebGPU::fromAPI(commandEncoder).ref();
}

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
