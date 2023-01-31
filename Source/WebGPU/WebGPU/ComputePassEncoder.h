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

#pragma once

#import "CommandsMixin.h"
#import <wtf/FastMalloc.h>
#import <wtf/Ref.h>
#import <wtf/RefCounted.h>

struct WGPUComputePassEncoderImpl {
};

namespace WebGPU {

class BindGroup;
class Buffer;
class ComputePipeline;
class Device;
class QuerySet;

// https://gpuweb.github.io/gpuweb/#gpucomputepassencoder
class ComputePassEncoder : public WGPUComputePassEncoderImpl, public RefCounted<ComputePassEncoder>, public CommandsMixin {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<ComputePassEncoder> create(id<MTLComputeCommandEncoder> computeCommandEncoder, Device& device)
    {
        return adoptRef(*new ComputePassEncoder(computeCommandEncoder, device));
    }
    static Ref<ComputePassEncoder> createInvalid(Device& device)
    {
        return adoptRef(*new ComputePassEncoder(device));
    }

    ~ComputePassEncoder();

    void beginPipelineStatisticsQuery(const QuerySet&, uint32_t queryIndex);
    void dispatch(uint32_t x, uint32_t y, uint32_t z);
    void dispatchIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset);
    void endPass();
    void endPipelineStatisticsQuery();
    void insertDebugMarker(String&& markerLabel);
    void popDebugGroup();
    void pushDebugGroup(String&& groupLabel);
    void setBindGroup(uint32_t groupIndex, const BindGroup&, uint32_t dynamicOffsetCount, const uint32_t* dynamicOffsets);
    void setPipeline(const ComputePipeline&);
    void setLabel(String&&);

    Device& device() const { return m_device; }

    bool isValid() const { return m_computeCommandEncoder; }

private:
    ComputePassEncoder(id<MTLComputeCommandEncoder>, Device&);
    ComputePassEncoder(Device&);

    bool validatePopDebugGroup() const;

    void makeInvalid() { m_computeCommandEncoder = nil; }

    id<MTLComputeCommandEncoder> m_computeCommandEncoder { nil };

    uint64_t m_debugGroupStackSize { 0 };

    const Ref<Device> m_device;
    MTLSize m_threadsPerThreadgroup;
};

} // namespace WebGPU
