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
#import <wtf/Borrow.h>

NODELETE void wgpuRenderPassEncoderAddRef(WGPURenderPassEncoder renderPassEncoder)
{
    WebGPU::CAPI::fromAPI(renderPassEncoder).ref();
}

void wgpuRenderPassEncoderRelease(WGPURenderPassEncoder renderPassEncoder)
{
    @autoreleasepool {
        WebGPU::CAPI::fromAPI(renderPassEncoder).deref();
    }
}

void wgpuRenderPassEncoderBeginOcclusionQuery(WGPURenderPassEncoder renderPassEncoder, uint32_t queryIndex)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(renderPassEncoder))->beginOcclusionQuery(queryIndex);
    }
}

void wgpuRenderPassEncoderDraw(WGPURenderPassEncoder renderPassEncoder, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(renderPassEncoder))->draw(vertexCount, instanceCount, firstVertex, firstInstance);
    }
}

void wgpuRenderPassEncoderDrawIndexed(WGPURenderPassEncoder renderPassEncoder, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(renderPassEncoder))->drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
    }
}

void wgpuRenderPassEncoderDrawIndexedIndirect(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(renderPassEncoder))->drawIndexedIndirect(protect(WebGPU::CAPI::fromAPI(indirectBuffer)), indirectOffset);
    }
}

void wgpuRenderPassEncoderDrawIndirect(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(renderPassEncoder))->drawIndirect(protect(WebGPU::CAPI::fromAPI(indirectBuffer)), indirectOffset);
    }
}

void wgpuRenderPassEncoderEndOcclusionQuery(WGPURenderPassEncoder renderPassEncoder)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(renderPassEncoder))->endOcclusionQuery();
    }
}

void wgpuRenderPassEncoderEnd(WGPURenderPassEncoder renderPassEncoder)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(renderPassEncoder))->end();
    }
}

void wgpuRenderPassEncoderExecuteBundles(WGPURenderPassEncoder renderPassEncoder, size_t bundlesCount, const WGPURenderBundle* bundles)
{
    @autoreleasepool {
        auto apiBundles = WTF::map(unsafeMakeSpan(bundles, bundlesCount), [](auto bundle) {
            return Ref { WebGPU::CAPI::fromAPI(bundle) };
        });
        protect(WebGPU::CAPI::fromAPI(renderPassEncoder))->executeBundles(borrow(apiBundles)->span());
    }
}

void wgpuRenderPassEncoderInsertDebugMarker(WGPURenderPassEncoder renderPassEncoder, WGPUStringView markerLabel)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(renderPassEncoder))->insertDebugMarker(WebGPU::CAPI::fromAPI(markerLabel));
    }
}

void wgpuRenderPassEncoderPopDebugGroup(WGPURenderPassEncoder renderPassEncoder)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(renderPassEncoder))->popDebugGroup();
    }
}

void wgpuRenderPassEncoderPushDebugGroup(WGPURenderPassEncoder renderPassEncoder, WGPUStringView groupLabel)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(renderPassEncoder))->pushDebugGroup(WebGPU::CAPI::fromAPI(groupLabel));
    }
}

// No dynamic offsets is std::nullopt.
void wgpuRenderPassEncoderSetBindGroup(WGPURenderPassEncoder renderPassEncoder, uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, const uint32_t* dynamicOffsets)
{
    @autoreleasepool {
        auto offsets = dynamicOffsetCount ? std::optional { unsafeMakeSpan(dynamicOffsets, dynamicOffsetCount) } : std::nullopt;
        protect(WebGPU::CAPI::fromAPI(renderPassEncoder))->setBindGroup(groupIndex, group ? protect(WebGPU::CAPI::fromAPI(group)).ptr() : nullptr, offsets);
    }
}

void wgpuRenderPassEncoderSetBlendConstant(WGPURenderPassEncoder renderPassEncoder, const WGPUColor* color)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(renderPassEncoder))->setBlendConstant(WebGPU::CAPI::fromAPI(*color));
    }
}

void wgpuRenderPassEncoderSetIndexBuffer(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size)
{
    @autoreleasepool {
        Ref protectedRenderPassEncoder = WebGPU::CAPI::fromAPI(renderPassEncoder);
        auto apiFormat = WebGPU::CAPI::fromAPI(format);
        if (!apiFormat)
            return protectedRenderPassEncoder->makeInvalid("setIndexBuffer: invalid index format"_s);
        protectedRenderPassEncoder->setIndexBuffer(protect(WebGPU::CAPI::fromAPI(buffer)), *apiFormat, offset, size == WGPU_WHOLE_SIZE ? std::nullopt : std::optional { size });
    }
}

void wgpuRenderPassEncoderSetPipeline(WGPURenderPassEncoder renderPassEncoder, WGPURenderPipeline pipeline)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(renderPassEncoder))->setPipeline(protect(WebGPU::CAPI::fromAPI(pipeline)));
    }
}

void wgpuRenderPassEncoderSetScissorRect(WGPURenderPassEncoder renderPassEncoder, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(renderPassEncoder))->setScissorRect(x, y, width, height);
    }
}

void wgpuRenderPassEncoderSetStencilReference(WGPURenderPassEncoder renderPassEncoder, uint32_t reference)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(renderPassEncoder))->setStencilReference(reference);
    }
}

void wgpuRenderPassEncoderSetVertexBuffer(WGPURenderPassEncoder renderPassEncoder, uint32_t slot, WGPUBuffer buffer, uint64_t offset, uint64_t size)
{
    @autoreleasepool {
        RefPtr<WebGPU::Buffer> optionalBuffer;
        if (buffer)
            optionalBuffer = protect(WebGPU::CAPI::fromAPI(buffer)).ptr();
        protect(WebGPU::CAPI::fromAPI(renderPassEncoder))->setVertexBuffer(slot, optionalBuffer.get(), offset, size == WGPU_WHOLE_SIZE ? std::nullopt : std::optional { size });
    }
}

void wgpuRenderPassEncoderSetViewport(WGPURenderPassEncoder renderPassEncoder, float x, float y, float width, float height, float minDepth, float maxDepth)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(renderPassEncoder))->setViewport(x, y, width, height, minDepth, maxDepth);
    }
}

void wgpuRenderPassEncoderSetLabel(WGPURenderPassEncoder renderPassEncoder, WGPUStringView label)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(renderPassEncoder))->setLabel(WebGPU::CAPI::fromAPI(label));
    }
}
