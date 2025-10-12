/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GPUComputePassEncoder.h"

#include "GPUBindGroup.h"
#include "GPUBuffer.h"
#include "GPUComputePipeline.h"
#include "GPUQuerySet.h"
#include "WebGPUDevice.h"

namespace WebCore {

GPUComputePassEncoder::GPUComputePassEncoder(Ref<WebGPU::ComputePassEncoder>&& backing, WebGPU::Device& device)
    : m_backing(WTFMove(backing))
    , m_device(&device)
{
}

String GPUComputePassEncoder::label() const
{
    return m_overrideLabel ? *m_overrideLabel : m_backing->label();
}

void GPUComputePassEncoder::setLabel(String&& label)
{
    protectedBacking()->setLabel(WTFMove(label));
}

void GPUComputePassEncoder::setPipeline(const GPUComputePipeline& computePipeline)
{
    protectedBacking()->setPipeline(computePipeline.backing());
}

void GPUComputePassEncoder::dispatchWorkgroups(GPUSize32 workgroupCountX, std::optional<GPUSize32> workgroupCountY, std::optional<GPUSize32> workgroupCountZ)
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=240219 we should be able to specify the
    // default values via the idl file
    protectedBacking()->dispatch(workgroupCountX, workgroupCountY.value_or(1), workgroupCountZ.value_or(1));
}

void GPUComputePassEncoder::dispatchWorkgroupsIndirect(const GPUBuffer& indirectBuffer, GPUSize64 indirectOffset)
{
    protectedBacking()->dispatchIndirect(indirectBuffer.backing(), indirectOffset);
}

void GPUComputePassEncoder::end()
{
    protectedBacking()->end();
    if (RefPtr device = m_device.get()) {
        m_overrideLabel = label();
        m_backing = device->invalidComputePassEncoder();
    }
}

void GPUComputePassEncoder::setBindGroup(GPUIndex32 index, const GPUBindGroup* bindGroup,
    std::optional<Vector<GPUBufferDynamicOffset>>&& dynamicOffsets)
{
    protectedBacking()->setBindGroup(index, bindGroup ? &bindGroup->backing() : nullptr, WTFMove(dynamicOffsets));
}

ExceptionOr<void> GPUComputePassEncoder::setBindGroup(GPUIndex32 index, const GPUBindGroup* bindGroup,
    const JSC::Uint32Array& dynamicOffsetsData,
    GPUSize64 dynamicOffsetsDataStart,
    GPUSize32 dynamicOffsetsDataLength)
{
    auto offset = checkedSum<uint64_t>(dynamicOffsetsDataStart, dynamicOffsetsDataLength);
    if (offset.hasOverflowed() || offset > dynamicOffsetsData.length())
        return Exception { ExceptionCode::RangeError, "dynamic offsets overflowed"_s };

    protectedBacking()->setBindGroup(index, bindGroup ? &bindGroup->backing() : nullptr, dynamicOffsetsData.typedSpan(), dynamicOffsetsDataStart, dynamicOffsetsDataLength);
    return { };
}

void GPUComputePassEncoder::pushDebugGroup(String&& groupLabel)
{
    protectedBacking()->pushDebugGroup(WTFMove(groupLabel));
}

void GPUComputePassEncoder::popDebugGroup()
{
    protectedBacking()->popDebugGroup();
}

void GPUComputePassEncoder::insertDebugMarker(String&& markerLabel)
{
    protectedBacking()->insertDebugMarker(WTFMove(markerLabel));
}

}
