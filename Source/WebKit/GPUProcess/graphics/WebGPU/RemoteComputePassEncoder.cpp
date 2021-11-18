/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "RemoteComputePassEncoder.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUObjectHeap.h"
#include <pal/graphics/WebGPU/WebGPUComputePassEncoder.h>

namespace WebKit {

RemoteComputePassEncoder::RemoteComputePassEncoder(PAL::WebGPU::ComputePassEncoder& computePassEncoder, WebGPU::ObjectHeap& objectHeap)
    : m_backing(computePassEncoder)
    , m_objectHeap(objectHeap)
{
}

RemoteComputePassEncoder::~RemoteComputePassEncoder()
{
}

void RemoteComputePassEncoder::setPipeline(WebGPUIdentifier computePipeline)
{
    UNUSED_PARAM(computePipeline);
}

void RemoteComputePassEncoder::dispatch(PAL::WebGPU::Size32 x, std::optional<PAL::WebGPU::Size32> y, std::optional<PAL::WebGPU::Size32> z)
{
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    UNUSED_PARAM(z);
}

void RemoteComputePassEncoder::dispatchIndirect(WebGPUIdentifier indirectBuffer, PAL::WebGPU::Size64 indirectOffset)
{
    UNUSED_PARAM(indirectBuffer);
    UNUSED_PARAM(indirectOffset);
}

void RemoteComputePassEncoder::beginPipelineStatisticsQuery(WebGPUIdentifier querySet, PAL::WebGPU::Size32 queryIndex)
{
    UNUSED_PARAM(querySet);
    UNUSED_PARAM(queryIndex);
}

void RemoteComputePassEncoder::endPipelineStatisticsQuery()
{
}

void RemoteComputePassEncoder::endPass()
{
}

void RemoteComputePassEncoder::setBindGroup(PAL::WebGPU::Index32 index, WebGPUIdentifier bindGroup,
    std::optional<Vector<PAL::WebGPU::BufferDynamicOffset>>&& offsets)
{
    UNUSED_PARAM(index);
    UNUSED_PARAM(bindGroup);
    UNUSED_PARAM(offsets);
}

void RemoteComputePassEncoder::pushDebugGroup(String&& groupLabel)
{
    UNUSED_PARAM(groupLabel);
}

void RemoteComputePassEncoder::popDebugGroup()
{
}

void RemoteComputePassEncoder::insertDebugMarker(String&& markerLabel)
{
    UNUSED_PARAM(markerLabel);
}

void RemoteComputePassEncoder::setLabel(String&& label)
{
    UNUSED_PARAM(label);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
