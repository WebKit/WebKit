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

#import "CAPIConversions.h"
#import "WebGPUCppConversions.h"

NODELETE void wgpuCommandEncoderAddRef(WGPUCommandEncoder commandEncoder)
{
    WebGPU::CAPI::metalFromAPI(commandEncoder).ref();
}

void wgpuCommandEncoderRelease(WGPUCommandEncoder commandEncoder)
{
    @autoreleasepool {
        WebGPU::CAPI::metalFromAPI(commandEncoder).deref();
    }
}

WGPUComputePassEncoder wgpuCommandEncoderBeginComputePass(WGPUCommandEncoder commandEncoder, const WGPUComputePassDescriptor* descriptor)
{
    @autoreleasepool {
        Ref protectedCommandEncoder = WebGPU::CAPI::metalFromAPI(commandEncoder);
        // A null descriptor is the default one.
        WGPUComputePassDescriptor defaultDescriptor = WGPU_COMPUTE_PASS_DESCRIPTOR_INIT;
        auto apiDescriptor = WebGPU::CAPI::fromAPI(descriptor ? *descriptor : defaultDescriptor);
        if (!apiDescriptor)
            return WebGPU::CAPI::releaseToAPI(WebGPU::Metal::ComputePassEncoder::createInvalid(protectedCommandEncoder, protectedCommandEncoder->device(), @"GPUComputePassDescriptor has timestamp writes without a query set"));
        return WebGPU::CAPI::releaseToAPI(protectedCommandEncoder->beginComputePass(*apiDescriptor));
    }
}

WGPURenderPassEncoder wgpuCommandEncoderBeginRenderPass(WGPUCommandEncoder commandEncoder, const WGPURenderPassDescriptor* descriptor)
{
    @autoreleasepool {
        Ref protectedCommandEncoder = WebGPU::CAPI::metalFromAPI(commandEncoder);
        WebGPU::CAPI::RenderPassDescriptorStorage storage;
        auto apiDescriptor = WebGPU::CAPI::fromAPI(*descriptor, storage);
        if (!apiDescriptor)
            return WebGPU::CAPI::releaseToAPI(WebGPU::Metal::RenderPassEncoder::createInvalid(protectedCommandEncoder, protectedCommandEncoder->device(), @"GPURenderPassDescriptor has an invalid enum value, a depth stencil attachment without a view or timestamp writes without a query set"));
        // Every WebGPU::RenderPassEncoder that a WebGPU::Metal::CommandEncoder creates is a WebGPU::Metal::RenderPassEncoder.
        Ref renderPassEncoder = downcast<WebGPU::Metal::RenderPassEncoder>(*protectedCommandEncoder->beginRenderPass(*apiDescriptor));
        return WebGPU::CAPI::releaseToAPI(WTF::move(renderPassEncoder));
    }
}

void wgpuCommandEncoderCopyBufferToBuffer(WGPUCommandEncoder commandEncoder, WGPUBuffer source, uint64_t sourceOffset, WGPUBuffer destination, uint64_t destinationOffset, uint64_t size)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::metalFromAPI(commandEncoder))->copyBufferToBuffer(protect(WebGPU::CAPI::metalFromAPI(source)), sourceOffset, protect(WebGPU::CAPI::metalFromAPI(destination)), destinationOffset, size);
    }
}

// A copy whose source or destination does not convert invalidates the encoder, as a failed copy
// validation does.
static constexpr auto invalidTexelCopyMessage = @"GPUCommandEncoder copy has a null buffer or texture or an invalid aspect";

void wgpuCommandEncoderCopyBufferToTexture(WGPUCommandEncoder commandEncoder, const WGPUTexelCopyBufferInfo* source, const WGPUTexelCopyTextureInfo* destination, const WGPUExtent3D* copySize)
{
    @autoreleasepool {
        Ref protectedCommandEncoder = WebGPU::CAPI::metalFromAPI(commandEncoder);
        auto apiSource = WebGPU::CAPI::fromAPI(*source);
        auto apiDestination = WebGPU::CAPI::fromAPI(*destination);
        if (!apiSource || !apiDestination)
            return protectedCommandEncoder->makeInvalid(invalidTexelCopyMessage);
        protectedCommandEncoder->copyBufferToTexture(*apiSource, *apiDestination, WebGPU::CAPI::fromAPI(*copySize));
    }
}

void wgpuCommandEncoderCopyTextureToBuffer(WGPUCommandEncoder commandEncoder, const WGPUTexelCopyTextureInfo* source, const WGPUTexelCopyBufferInfo* destination, const WGPUExtent3D* copySize)
{
    @autoreleasepool {
        Ref protectedCommandEncoder = WebGPU::CAPI::metalFromAPI(commandEncoder);
        auto apiSource = WebGPU::CAPI::fromAPI(*source);
        auto apiDestination = WebGPU::CAPI::fromAPI(*destination);
        if (!apiSource || !apiDestination)
            return protectedCommandEncoder->makeInvalid(invalidTexelCopyMessage);
        protectedCommandEncoder->copyTextureToBuffer(*apiSource, *apiDestination, WebGPU::CAPI::fromAPI(*copySize));
    }
}

void wgpuCommandEncoderCopyTextureToTexture(WGPUCommandEncoder commandEncoder, const WGPUTexelCopyTextureInfo* source, const WGPUTexelCopyTextureInfo* destination, const WGPUExtent3D* copySize)
{
    @autoreleasepool {
        Ref protectedCommandEncoder = WebGPU::CAPI::metalFromAPI(commandEncoder);
        auto apiSource = WebGPU::CAPI::fromAPI(*source);
        auto apiDestination = WebGPU::CAPI::fromAPI(*destination);
        if (!apiSource || !apiDestination)
            return protectedCommandEncoder->makeInvalid(invalidTexelCopyMessage);
        protectedCommandEncoder->copyTextureToTexture(*apiSource, *apiDestination, WebGPU::CAPI::fromAPI(*copySize));
    }
}

void wgpuCommandEncoderClearBuffer(WGPUCommandEncoder commandEncoder, WGPUBuffer buffer, uint64_t offset, uint64_t size)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::metalFromAPI(commandEncoder))->clearBuffer(protect(WebGPU::CAPI::metalFromAPI(buffer)), offset, size == WGPU_WHOLE_SIZE ? std::nullopt : std::optional { size });
    }
}

WGPUCommandBuffer wgpuCommandEncoderFinish(WGPUCommandEncoder commandEncoder, const WGPUCommandBufferDescriptor* descriptor)
{
    @autoreleasepool {
        // Every WebGPU::CommandBuffer that a WebGPU::Metal::CommandEncoder creates is a WebGPU::Metal::CommandBuffer.
        Ref commandBuffer = downcast<WebGPU::Metal::CommandBuffer>(*protect(WebGPU::CAPI::metalFromAPI(commandEncoder))->finish({ .label = descriptor ? WebGPU::CAPI::fromAPI(descriptor->label) : String() }));
        return WebGPU::CAPI::releaseToAPI(WTF::move(commandBuffer));
    }
}

void wgpuCommandEncoderInsertDebugMarker(WGPUCommandEncoder commandEncoder, WGPUStringView markerLabel)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::metalFromAPI(commandEncoder))->insertDebugMarker(WebGPU::CAPI::fromAPI(markerLabel));
    }
}

void wgpuCommandEncoderPopDebugGroup(WGPUCommandEncoder commandEncoder)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::metalFromAPI(commandEncoder))->popDebugGroup();
    }
}

void wgpuCommandEncoderPushDebugGroup(WGPUCommandEncoder commandEncoder, WGPUStringView groupLabel)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::metalFromAPI(commandEncoder))->pushDebugGroup(WebGPU::CAPI::fromAPI(groupLabel));
    }
}

void wgpuCommandEncoderResolveQuerySet(WGPUCommandEncoder commandEncoder, WGPUQuerySet querySet, uint32_t firstQuery, uint32_t queryCount, WGPUBuffer destination, uint64_t destinationOffset)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::metalFromAPI(commandEncoder))->resolveQuerySet(protect(WebGPU::CAPI::metalFromAPI(querySet)), firstQuery, queryCount, protect(WebGPU::CAPI::metalFromAPI(destination)), destinationOffset);
    }
}

void wgpuCommandEncoderWriteTimestamp(WGPUCommandEncoder commandEncoder, WGPUQuerySet querySet, uint32_t queryIndex)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::metalFromAPI(commandEncoder))->writeTimestamp(protect(WebGPU::CAPI::metalFromAPI(querySet)), queryIndex);
    }
}

void wgpuCommandEncoderSetLabel(WGPUCommandEncoder commandEncoder, WGPUStringView label)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::metalFromAPI(commandEncoder))->setLabel(WebGPU::CAPI::fromAPI(label));
    }
}
