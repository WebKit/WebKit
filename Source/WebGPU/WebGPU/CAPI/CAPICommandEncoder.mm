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
    WebGPU::CAPI::fromAPI(commandEncoder).ref();
}

void wgpuCommandEncoderRelease(WGPUCommandEncoder commandEncoder)
{
    @autoreleasepool {
        WebGPU::CAPI::fromAPI(commandEncoder).deref();
    }
}

WGPUComputePassEncoder wgpuCommandEncoderBeginComputePass(WGPUCommandEncoder commandEncoder, const WGPUComputePassDescriptor* descriptor)
{
    @autoreleasepool {
        Ref protectedCommandEncoder = WebGPU::CAPI::fromAPI(commandEncoder);
        // A null descriptor is the default one.
        WGPUComputePassDescriptor defaultDescriptor = WGPU_COMPUTE_PASS_DESCRIPTOR_INIT;
        auto apiDescriptor = WebGPU::CAPI::fromAPI(descriptor ? *descriptor : defaultDescriptor);
        if (!apiDescriptor)
            return WebGPU::CAPI::releaseToAPI(protectedCommandEncoder->beginInvalidComputePass("GPUComputePassDescriptor has timestamp writes without a query set"_s));
        return WebGPU::CAPI::releaseToAPI(protectedCommandEncoder->beginComputePass(*apiDescriptor));
    }
}

WGPURenderPassEncoder wgpuCommandEncoderBeginRenderPass(WGPUCommandEncoder commandEncoder, const WGPURenderPassDescriptor* descriptor)
{
    @autoreleasepool {
        Ref protectedCommandEncoder = WebGPU::CAPI::fromAPI(commandEncoder);
        WebGPU::CAPI::RenderPassDescriptorStorage storage;
        auto apiDescriptor = WebGPU::CAPI::fromAPI(*descriptor, storage);
        if (!apiDescriptor)
            return WebGPU::CAPI::releaseToAPI(protectedCommandEncoder->beginInvalidRenderPass("GPURenderPassDescriptor has an invalid enum value, a depth stencil attachment without a view or timestamp writes without a query set"_s));
        return WebGPU::CAPI::releaseToAPI(protectedCommandEncoder->beginRenderPass(*apiDescriptor));
    }
}

void wgpuCommandEncoderCopyBufferToBuffer(WGPUCommandEncoder commandEncoder, WGPUBuffer source, uint64_t sourceOffset, WGPUBuffer destination, uint64_t destinationOffset, uint64_t size)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(commandEncoder))->copyBufferToBuffer(protect(WebGPU::CAPI::fromAPI(source)), sourceOffset, protect(WebGPU::CAPI::fromAPI(destination)), destinationOffset, size);
    }
}

// A copy whose source or destination does not convert invalidates the encoder, as a failed copy
// validation does.
static constexpr auto invalidTexelCopyMessage = "GPUCommandEncoder copy has a null buffer or texture or an invalid aspect"_s;

void wgpuCommandEncoderCopyBufferToTexture(WGPUCommandEncoder commandEncoder, const WGPUTexelCopyBufferInfo* source, const WGPUTexelCopyTextureInfo* destination, const WGPUExtent3D* copySize)
{
    @autoreleasepool {
        Ref protectedCommandEncoder = WebGPU::CAPI::fromAPI(commandEncoder);
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
        Ref protectedCommandEncoder = WebGPU::CAPI::fromAPI(commandEncoder);
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
        Ref protectedCommandEncoder = WebGPU::CAPI::fromAPI(commandEncoder);
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
        protect(WebGPU::CAPI::fromAPI(commandEncoder))->clearBuffer(protect(WebGPU::CAPI::fromAPI(buffer)), offset, size == WGPU_WHOLE_SIZE ? std::nullopt : std::optional { size });
    }
}

WGPUCommandBuffer wgpuCommandEncoderFinish(WGPUCommandEncoder commandEncoder, const WGPUCommandBufferDescriptor* descriptor)
{
    @autoreleasepool {
        return WebGPU::CAPI::releaseToAPI(protect(WebGPU::CAPI::fromAPI(commandEncoder))->finish({ .label = descriptor ? WebGPU::CAPI::fromAPI(descriptor->label) : String() }));
    }
}

void wgpuCommandEncoderInsertDebugMarker(WGPUCommandEncoder commandEncoder, WGPUStringView markerLabel)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(commandEncoder))->insertDebugMarker(WebGPU::CAPI::fromAPI(markerLabel));
    }
}

void wgpuCommandEncoderPopDebugGroup(WGPUCommandEncoder commandEncoder)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(commandEncoder))->popDebugGroup();
    }
}

void wgpuCommandEncoderPushDebugGroup(WGPUCommandEncoder commandEncoder, WGPUStringView groupLabel)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(commandEncoder))->pushDebugGroup(WebGPU::CAPI::fromAPI(groupLabel));
    }
}

void wgpuCommandEncoderResolveQuerySet(WGPUCommandEncoder commandEncoder, WGPUQuerySet querySet, uint32_t firstQuery, uint32_t queryCount, WGPUBuffer destination, uint64_t destinationOffset)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(commandEncoder))->resolveQuerySet(protect(WebGPU::CAPI::fromAPI(querySet)), firstQuery, queryCount, protect(WebGPU::CAPI::fromAPI(destination)), destinationOffset);
    }
}

void wgpuCommandEncoderWriteTimestamp(WGPUCommandEncoder commandEncoder, WGPUQuerySet querySet, uint32_t queryIndex)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(commandEncoder))->writeTimestamp(protect(WebGPU::CAPI::fromAPI(querySet)), queryIndex);
    }
}

void wgpuCommandEncoderSetLabel(WGPUCommandEncoder commandEncoder, WGPUStringView label)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(commandEncoder))->setLabel(WebGPU::CAPI::fromAPI(label));
    }
}
