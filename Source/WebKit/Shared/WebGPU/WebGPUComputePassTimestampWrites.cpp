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
#include "WebGPUComputePassTimestampWrites.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUConvertFromBackingContext.h"
#include "WebGPUConvertToBackingContext.h"
#include <pal/graphics/WebGPU/WebGPUComputePassTimestampWrites.h>

namespace WebKit::WebGPU {

std::optional<ComputePassTimestampWrite> ConvertToBackingContext::convertToBacking(const PAL::WebGPU::ComputePassTimestampWrite& computePassTimestampWrite)
{
    auto querySet = convertToBacking(computePassTimestampWrite.querySet);
    if (!querySet)
        return std::nullopt;

    return { { querySet, computePassTimestampWrite.queryIndex, computePassTimestampWrite.location } };
}

std::optional<ComputePassTimestampWrites> ConvertToBackingContext::convertToBacking(const PAL::WebGPU::ComputePassTimestampWrites& computePassTimestampWrites)
{
    Vector<ComputePassTimestampWrite> timestampWrites;
    timestampWrites.reserveInitialCapacity(computePassTimestampWrites.size());
    for (const auto& timestampWrite : computePassTimestampWrites) {
        auto convertedTimestampWrite = convertToBacking(timestampWrite);
        if (!convertedTimestampWrite)
            return std::nullopt;
        timestampWrites.uncheckedAppend(WTFMove(*convertedTimestampWrite));
    }
    return timestampWrites;
}

std::optional<PAL::WebGPU::ComputePassTimestampWrite> ConvertFromBackingContext::convertFromBacking(const ComputePassTimestampWrite& computePassTimestampWrite)
{
    auto* querySet = convertQuerySetFromBacking(computePassTimestampWrite.querySet);
    if (!querySet)
        return std::nullopt;

    return { { *querySet, computePassTimestampWrite.queryIndex, computePassTimestampWrite.location } };
}

std::optional<PAL::WebGPU::ComputePassTimestampWrites> ConvertFromBackingContext::convertFromBacking(const ComputePassTimestampWrites& computePassTimestampWrites)
{
    Vector<PAL::WebGPU::ComputePassTimestampWrite> timestampWrites;
    timestampWrites.reserveInitialCapacity(computePassTimestampWrites.size());
    for (const auto& backingTimestampWrite : computePassTimestampWrites) {
        auto timestampWrite = convertFromBacking(backingTimestampWrite);
        if (!timestampWrite)
            return std::nullopt;
        timestampWrites.uncheckedAppend(WTFMove(*timestampWrite));
    }
    return timestampWrites;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
