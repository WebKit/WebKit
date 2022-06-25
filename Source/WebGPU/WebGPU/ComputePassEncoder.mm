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
#import "ComputePassEncoder.h"

#import "APIConversions.h"
#import "BindGroup.h"
#import "Buffer.h"
#import "ComputePipeline.h"
#import "QuerySet.h"

namespace WebGPU {

ComputePassEncoder::ComputePassEncoder(id<MTLComputeCommandEncoder> computeCommandEncoder, Device& device)
    : m_computeCommandEncoder(computeCommandEncoder)
    , m_device(device)
{
}

ComputePassEncoder::ComputePassEncoder(Device& device)
    : m_device(device)
{
}

ComputePassEncoder::~ComputePassEncoder() = default;

void ComputePassEncoder::beginPipelineStatisticsQuery(const QuerySet& querySet, uint32_t queryIndex)
{
    UNUSED_PARAM(querySet);
    UNUSED_PARAM(queryIndex);
}

void ComputePassEncoder::dispatch(uint32_t x, uint32_t y, uint32_t z)
{
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    UNUSED_PARAM(z);
}

void ComputePassEncoder::dispatchIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset)
{
    UNUSED_PARAM(indirectBuffer);
    UNUSED_PARAM(indirectOffset);
}

void ComputePassEncoder::endPass()
{
}

void ComputePassEncoder::endPipelineStatisticsQuery()
{
}

void ComputePassEncoder::insertDebugMarker(String&& markerLabel)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-insertdebugmarker

    if (!prepareTheEncoderState())
        return;

    [m_computeCommandEncoder insertDebugSignpost:markerLabel];
}

bool ComputePassEncoder::validatePopDebugGroup() const
{
    if (!m_debugGroupStackSize)
        return false;

    return true;
}

void ComputePassEncoder::popDebugGroup()
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-popdebuggroup

    if (!prepareTheEncoderState())
        return;

    if (!validatePopDebugGroup()) {
        makeInvalid();
        return;
    }

    --m_debugGroupStackSize;
    [m_computeCommandEncoder popDebugGroup];
}

void ComputePassEncoder::pushDebugGroup(String&& groupLabel)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-pushdebuggroup

    if (!prepareTheEncoderState())
        return;

    ++m_debugGroupStackSize;
    [m_computeCommandEncoder pushDebugGroup:groupLabel];
}

void ComputePassEncoder::setBindGroup(uint32_t groupIndex, const BindGroup& group, uint32_t dynamicOffsetCount, const uint32_t* dynamicOffsets)
{
    UNUSED_PARAM(groupIndex);
    UNUSED_PARAM(group);
    UNUSED_PARAM(dynamicOffsetCount);
    UNUSED_PARAM(dynamicOffsets);
}

void ComputePassEncoder::setPipeline(const ComputePipeline& pipeline)
{
    UNUSED_PARAM(pipeline);
}

void ComputePassEncoder::setLabel(String&& label)
{
    m_computeCommandEncoder.label = label;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuComputePassEncoderRelease(WGPUComputePassEncoder computePassEncoder)
{
    WebGPU::fromAPI(computePassEncoder).deref();
}

void wgpuComputePassEncoderBeginPipelineStatisticsQuery(WGPUComputePassEncoder computePassEncoder, WGPUQuerySet querySet, uint32_t queryIndex)
{
    WebGPU::fromAPI(computePassEncoder).beginPipelineStatisticsQuery(WebGPU::fromAPI(querySet), queryIndex);
}

void wgpuComputePassEncoderDispatch(WGPUComputePassEncoder computePassEncoder, uint32_t x, uint32_t y, uint32_t z)
{
    WebGPU::fromAPI(computePassEncoder).dispatch(x, y, z);
}

void wgpuComputePassEncoderDispatchIndirect(WGPUComputePassEncoder computePassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset)
{
    WebGPU::fromAPI(computePassEncoder).dispatchIndirect(WebGPU::fromAPI(indirectBuffer), indirectOffset);
}

void wgpuComputePassEncoderEndPass(WGPUComputePassEncoder computePassEncoder)
{
    WebGPU::fromAPI(computePassEncoder).endPass();
}

void wgpuComputePassEncoderEndPipelineStatisticsQuery(WGPUComputePassEncoder computePassEncoder)
{
    WebGPU::fromAPI(computePassEncoder).endPipelineStatisticsQuery();
}

void wgpuComputePassEncoderInsertDebugMarker(WGPUComputePassEncoder computePassEncoder, const char* markerLabel)
{
    WebGPU::fromAPI(computePassEncoder).insertDebugMarker(WebGPU::fromAPI(markerLabel));
}

void wgpuComputePassEncoderPopDebugGroup(WGPUComputePassEncoder computePassEncoder)
{
    WebGPU::fromAPI(computePassEncoder).popDebugGroup();
}

void wgpuComputePassEncoderPushDebugGroup(WGPUComputePassEncoder computePassEncoder, const char* groupLabel)
{
    WebGPU::fromAPI(computePassEncoder).pushDebugGroup(WebGPU::fromAPI(groupLabel));
}

void wgpuComputePassEncoderSetBindGroup(WGPUComputePassEncoder computePassEncoder, uint32_t groupIndex, WGPUBindGroup group, uint32_t dynamicOffsetCount, const uint32_t* dynamicOffsets)
{
    WebGPU::fromAPI(computePassEncoder).setBindGroup(groupIndex, WebGPU::fromAPI(group), dynamicOffsetCount, dynamicOffsets);
}

void wgpuComputePassEncoderSetPipeline(WGPUComputePassEncoder computePassEncoder, WGPUComputePipeline pipeline)
{
    WebGPU::fromAPI(computePassEncoder).setPipeline(WebGPU::fromAPI(pipeline));
}

void wgpuComputePassEncoderSetLabel(WGPUComputePassEncoder computePassEncoder, const char* label)
{
    WebGPU::fromAPI(computePassEncoder).setLabel(WebGPU::fromAPI(label));
}
