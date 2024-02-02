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

#pragma once

#import "CommandsMixin.h"
#import <wtf/FastMalloc.h>
#import <wtf/HashMap.h>
#import <wtf/Ref.h>
#import <wtf/RefCounted.h>
#import <wtf/Vector.h>

struct WGPUComputePassEncoderImpl {
};

namespace WebGPU {

class BindGroup;
class Buffer;
class CommandEncoder;
class ComputePipeline;
class Device;
class QuerySet;

struct BindableResources;

// https://gpuweb.github.io/gpuweb/#gpucomputepassencoder
class ComputePassEncoder : public WGPUComputePassEncoderImpl, public RefCounted<ComputePassEncoder>, public CommandsMixin {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<ComputePassEncoder> create(id<MTLComputeCommandEncoder> computeCommandEncoder, const WGPUComputePassDescriptor& descriptor, CommandEncoder& parentEncoder, Device& device)
    {
        return adoptRef(*new ComputePassEncoder(computeCommandEncoder, descriptor, parentEncoder, device));
    }
    static Ref<ComputePassEncoder> createInvalid(CommandEncoder& parentEncoder, Device& device, NSString* errorString)
    {
        return adoptRef(*new ComputePassEncoder(parentEncoder, device, errorString));
    }

    ~ComputePassEncoder();

    void dispatch(uint32_t x, uint32_t y, uint32_t z);
    void dispatchIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset);
    void endPass();
    void insertDebugMarker(String&& markerLabel);
    void popDebugGroup();
    void pushDebugGroup(String&& groupLabel);

    void setBindGroup(uint32_t groupIndex, const BindGroup&, uint32_t dynamicOffsetCount, const uint32_t* dynamicOffsets);
    void setPipeline(const ComputePipeline&);
    void setLabel(String&&);

    Device& device() const { return m_device; }

    bool isValid() const { return m_computeCommandEncoder; }

private:
    ComputePassEncoder(id<MTLComputeCommandEncoder>, const WGPUComputePassDescriptor&, CommandEncoder&, Device&);
    ComputePassEncoder(CommandEncoder&, Device&, NSString*);

    bool validatePopDebugGroup() const;

    void makeInvalid(NSString* = nil);
    void executePreDispatchCommands(id<MTLBuffer> = nil);
    id<MTLBuffer> runPredispatchIndirectCallValidation(const Buffer&, uint64_t);

    id<MTLComputeCommandEncoder> m_computeCommandEncoder { nil };

    struct PendingTimestampWrites {
        Ref<QuerySet> querySet;
        uint32_t queryIndex;
    };
    Vector<PendingTimestampWrites> m_pendingTimestampWrites;
    uint64_t m_debugGroupStackSize { 0 };

    const Ref<Device> m_device;
    MTLSize m_threadsPerThreadgroup;
    Vector<uint32_t> m_computeDynamicOffsets;
    const ComputePipeline* m_pipeline { nullptr };
    Ref<CommandEncoder> m_parentEncoder;
    HashMap<uint32_t, Vector<uint32_t>, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> m_bindGroupDynamicOffsets;
    HashMap<uint32_t, Vector<const BindableResources*>, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> m_bindGroupResources;
    HashMap<uint32_t, WeakPtr<BindGroup>, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> m_bindGroups;
    NSString *m_lastErrorString { nil };
    bool m_passEnded { false };
};

} // namespace WebGPU
