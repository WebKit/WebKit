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
#include "WebGPUComputePassDescriptor.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUConvertFromBackingContext.h"
#include "WebGPUConvertToBackingContext.h"
#include <WebCore/WebGPUComputePassDescriptor.h>

namespace WebKit::WebGPU {

std::optional<ComputePassDescriptor> ConvertToBackingContext::convertToBacking(const WebCore::WebGPU::ComputePassDescriptor& computePassDescriptor)
{
    auto base = convertToBacking(static_cast<const WebCore::WebGPU::ObjectDescriptorBase&>(computePassDescriptor));
    if (!base)
        return std::nullopt;

    auto timestampWrites = computePassDescriptor.timestampWrites ? convertToBacking(*computePassDescriptor.timestampWrites) : std::nullopt;

    return { { WTFMove(*base), timestampWrites } };
}

std::optional<WebCore::WebGPU::ComputePassDescriptor> ConvertFromBackingContext::convertFromBacking(const ComputePassDescriptor& computePassDescriptor)
{
    auto base = convertFromBacking(static_cast<const ObjectDescriptorBase&>(computePassDescriptor));
    if (!base)
        return std::nullopt;

    auto timestampWrites = computePassDescriptor.timestampWrites ? convertFromBacking(*computePassDescriptor.timestampWrites) : std::nullopt;

    return { { WTFMove(*base), timestampWrites } };
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
