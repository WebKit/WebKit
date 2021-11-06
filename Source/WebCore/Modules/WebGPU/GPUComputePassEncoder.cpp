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

#pragma once

#include "config.h"
#include "GPUComputePassEncoder.h"

#include "GPUBindGroup.h"
#include "GPUBuffer.h"
#include "GPUComputePipeline.h"
#include "GPUQuerySet.h"

namespace WebCore {

String GPUComputePassEncoder::label() const
{
    return StringImpl::empty();
}

void GPUComputePassEncoder::setLabel(String&&)
{
}

void GPUComputePassEncoder::setPipeline(const GPUComputePipeline&)
{
}

void GPUComputePassEncoder::dispatch(GPUSize32 x, std::optional<GPUSize32> y, std::optional<GPUSize32> z)
{
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    UNUSED_PARAM(z);
}

void GPUComputePassEncoder::dispatchIndirect(const GPUBuffer& indirectBuffer, GPUSize64 indirectOffset)
{
    UNUSED_PARAM(indirectBuffer);
    UNUSED_PARAM(indirectOffset);
}

void GPUComputePassEncoder::beginPipelineStatisticsQuery(const GPUQuerySet&, GPUSize32 queryIndex)
{
    UNUSED_PARAM(queryIndex);
}

void GPUComputePassEncoder::endPipelineStatisticsQuery()
{

}

void GPUComputePassEncoder::endPass()
{

}

void GPUComputePassEncoder::setBindGroup(GPUIndex32, const GPUBindGroup&,
    std::optional<Vector<GPUBufferDynamicOffset>>&&)
{

}

void GPUComputePassEncoder::setBindGroup(GPUIndex32, const GPUBindGroup&,
    const JSC::Uint32Array& dynamicOffsetsData,
    GPUSize64 dynamicOffsetsDataStart,
    GPUSize32 dynamicOffsetsDataLength)
{
    UNUSED_PARAM(dynamicOffsetsData);
    UNUSED_PARAM(dynamicOffsetsDataStart);
    UNUSED_PARAM(dynamicOffsetsDataLength);
}

void GPUComputePassEncoder::pushDebugGroup(String&& groupLabel)
{
    UNUSED_PARAM(groupLabel);
}

void GPUComputePassEncoder::popDebugGroup()
{

}

void GPUComputePassEncoder::insertDebugMarker(String&& markerLabel)
{
    UNUSED_PARAM(markerLabel);
}

}
