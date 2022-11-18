/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "WebGPUSwapChainDescriptor.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUConvertFromBackingContext.h"
#include "WebGPUConvertToBackingContext.h"
#include <pal/graphics/WebGPU/WebGPUSwapChainDescriptor.h>

namespace WebKit::WebGPU {

std::optional<SwapChainDescriptor> ConvertToBackingContext::convertToBacking(const PAL::WebGPU::SwapChainDescriptor& swapChainDescriptor)
{
    auto base = convertToBacking(static_cast<const PAL::WebGPU::ObjectDescriptorBase&>(swapChainDescriptor));
    if (!base)
        return std::nullopt;

    auto size = convertToBacking(swapChainDescriptor.size);
    if (!size)
        return std::nullopt;

    return { { WTFMove(*base), WTFMove(*size),  swapChainDescriptor.sampleCount, swapChainDescriptor.format, swapChainDescriptor.usage } };
}

std::optional<PAL::WebGPU::SwapChainDescriptor> ConvertFromBackingContext::convertFromBacking(const SwapChainDescriptor& swapChainDescriptor)
{
    auto base = convertFromBacking(static_cast<const ObjectDescriptorBase&>(swapChainDescriptor));
    if (!base)
        return std::nullopt;

    auto size = convertFromBacking(swapChainDescriptor.size);
    if (!size)
        return std::nullopt;

    return { { WTFMove(*base), WTFMove(*size), swapChainDescriptor.sampleCount, swapChainDescriptor.format, swapChainDescriptor.usage } };
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
