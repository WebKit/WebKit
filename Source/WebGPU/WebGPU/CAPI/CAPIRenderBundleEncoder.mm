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

NODELETE void wgpuRenderBundleEncoderAddRef(WGPURenderBundleEncoder renderBundleEncoder)
{
    WebGPU::CAPI::fromAPI(renderBundleEncoder).ref();
}

void wgpuRenderBundleEncoderRelease(WGPURenderBundleEncoder renderBundleEncoder)
{
    @autoreleasepool {
        WebGPU::CAPI::fromAPI(renderBundleEncoder).deref();
    }
}

void wgpuRenderBundleEncoderDraw(WGPURenderBundleEncoder renderBundleEncoder, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(renderBundleEncoder))->draw(vertexCount, instanceCount, firstVertex, firstInstance);
    }
}

void wgpuRenderBundleEncoderDrawIndexed(WGPURenderBundleEncoder renderBundleEncoder, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(renderBundleEncoder))->drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
    }
}

void wgpuRenderBundleEncoderDrawIndexedIndirect(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset)
{
    @autoreleasepool {
        RELEASE_ASSERT(indirectBuffer);
        protect(WebGPU::CAPI::fromAPI(renderBundleEncoder))->drawIndexedIndirect(protect(WebGPU::CAPI::fromAPI(indirectBuffer)), indirectOffset);
    }
}

void wgpuRenderBundleEncoderDrawIndirect(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset)
{
    @autoreleasepool {
        RELEASE_ASSERT(indirectBuffer);
        protect(WebGPU::CAPI::fromAPI(renderBundleEncoder))->drawIndirect(protect(WebGPU::CAPI::fromAPI(indirectBuffer)), indirectOffset);
    }
}

WGPURenderBundle wgpuRenderBundleEncoderFinish(WGPURenderBundleEncoder renderBundleEncoder, const WGPURenderBundleDescriptor* descriptor)
{
    @autoreleasepool {
        return WebGPU::CAPI::releaseToAPI(protect(WebGPU::CAPI::fromAPI(renderBundleEncoder))->finish({ .label = descriptor ? WebGPU::CAPI::fromAPI(descriptor->label) : String() }));
    }
}

void wgpuRenderBundleEncoderInsertDebugMarker(WGPURenderBundleEncoder renderBundleEncoder, WGPUStringView markerLabel)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(renderBundleEncoder))->insertDebugMarker(WebGPU::CAPI::fromAPI(markerLabel));
    }
}

void wgpuRenderBundleEncoderPopDebugGroup(WGPURenderBundleEncoder renderBundleEncoder)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(renderBundleEncoder))->popDebugGroup();
    }
}

void wgpuRenderBundleEncoderPushDebugGroup(WGPURenderBundleEncoder renderBundleEncoder, WGPUStringView groupLabel)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(renderBundleEncoder))->pushDebugGroup(WebGPU::CAPI::fromAPI(groupLabel));
    }
}

NODELETE void wgpuRenderBundleEncoderSetBindGroup(WGPURenderBundleEncoder, uint32_t, WGPUBindGroup, size_t, const uint32_t*)
{
}

void wgpuRenderBundleEncoderSetIndexBuffer(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size)
{
    @autoreleasepool {
        Ref protectedRenderBundleEncoder = WebGPU::CAPI::fromAPI(renderBundleEncoder);
        auto apiFormat = WebGPU::CAPI::fromAPI(format);
        if (!apiFormat)
            return protectedRenderBundleEncoder->makeInvalid("setIndexBuffer: invalid index format"_s);
        protectedRenderBundleEncoder->setIndexBuffer(protect(WebGPU::CAPI::fromAPI(buffer)), *apiFormat, offset, size == WGPU_WHOLE_SIZE ? std::nullopt : std::optional { size });
    }
}

void wgpuRenderBundleEncoderSetPipeline(WGPURenderBundleEncoder renderBundleEncoder, WGPURenderPipeline pipeline)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(renderBundleEncoder))->setPipeline(protect(WebGPU::CAPI::fromAPI(pipeline)));
    }
}

void wgpuRenderBundleEncoderSetVertexBuffer(WGPURenderBundleEncoder renderBundleEncoder, uint32_t slot, WGPUBuffer buffer, uint64_t offset, uint64_t size)
{
    @autoreleasepool {
        RefPtr<WebGPU::Buffer> optionalBuffer;
        if (buffer)
            optionalBuffer = protect(WebGPU::CAPI::fromAPI(buffer)).ptr();
        protect(WebGPU::CAPI::fromAPI(renderBundleEncoder))->setVertexBuffer(slot, optionalBuffer.get(), offset, size == WGPU_WHOLE_SIZE ? std::nullopt : std::optional { size });
    }
}

void wgpuRenderBundleEncoderSetLabel(WGPURenderBundleEncoder renderBundleEncoder, WGPUStringView label)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(renderBundleEncoder))->setLabel(WebGPU::CAPI::fromAPI(label));
    }
}
