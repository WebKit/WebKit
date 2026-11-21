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

NODELETE void wgpuComputePassEncoderAddRef(WGPUComputePassEncoder computePassEncoder)
{
    WebGPU::CAPI::fromAPI(computePassEncoder).ref();
}

void wgpuComputePassEncoderRelease(WGPUComputePassEncoder computePassEncoder)
{
    @autoreleasepool {
        WebGPU::CAPI::fromAPI(computePassEncoder).deref();
    }
}

void wgpuComputePassEncoderDispatchWorkgroups(WGPUComputePassEncoder computePassEncoder, uint32_t x, uint32_t y, uint32_t z)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(computePassEncoder))->dispatch(x, y, z);
    }
}

void wgpuComputePassEncoderDispatchWorkgroupsIndirect(WGPUComputePassEncoder computePassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(computePassEncoder))->dispatchIndirect(protect(WebGPU::CAPI::fromAPI(indirectBuffer)), indirectOffset);
    }
}

void wgpuComputePassEncoderEnd(WGPUComputePassEncoder computePassEncoder)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(computePassEncoder))->end();
    }
}

void wgpuComputePassEncoderInsertDebugMarker(WGPUComputePassEncoder computePassEncoder, WGPUStringView markerLabel)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(computePassEncoder))->insertDebugMarker(WebGPU::CAPI::fromAPI(markerLabel));
    }
}

void wgpuComputePassEncoderPopDebugGroup(WGPUComputePassEncoder computePassEncoder)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(computePassEncoder))->popDebugGroup();
    }
}

void wgpuComputePassEncoderPushDebugGroup(WGPUComputePassEncoder computePassEncoder, WGPUStringView groupLabel)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(computePassEncoder))->pushDebugGroup(WebGPU::CAPI::fromAPI(groupLabel));
    }
}

// No dynamic offsets is std::nullopt.
void wgpuComputePassEncoderSetBindGroup(WGPUComputePassEncoder computePassEncoder, uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, const uint32_t* dynamicOffsets)
{
    @autoreleasepool {
        auto offsets = dynamicOffsetCount ? std::optional { unsafeMakeSpan(dynamicOffsets, dynamicOffsetCount) } : std::nullopt;
        protect(WebGPU::CAPI::fromAPI(computePassEncoder))->setBindGroup(groupIndex, group ? protect(WebGPU::CAPI::fromAPI(group)).ptr() : nullptr, offsets);
    }
}

void wgpuComputePassEncoderSetPipeline(WGPUComputePassEncoder computePassEncoder, WGPUComputePipeline pipeline)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(computePassEncoder))->setPipeline(protect(WebGPU::CAPI::fromAPI(pipeline)));
    }
}

void wgpuComputePassEncoderSetLabel(WGPUComputePassEncoder computePassEncoder, WGPUStringView label)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(computePassEncoder))->setLabel(WebGPU::CAPI::fromAPI(label));
    }
}
