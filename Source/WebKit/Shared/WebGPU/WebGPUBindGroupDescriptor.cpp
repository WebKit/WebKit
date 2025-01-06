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
#include "WebGPUBindGroupDescriptor.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUConvertFromBackingContext.h"
#include "WebGPUConvertToBackingContext.h"
#include <WebCore/WebGPUBindGroupDescriptor.h>
#include <WebCore/WebGPUBindGroupLayout.h>

namespace WebKit::WebGPU {

std::optional<BindGroupDescriptor> ConvertToBackingContext::convertToBacking(const WebCore::WebGPU::BindGroupDescriptor& bindGroupDescriptor)
{
    auto base = convertToBacking(static_cast<const WebCore::WebGPU::ObjectDescriptorBase&>(bindGroupDescriptor));
    if (!base)
        return std::nullopt;

    auto identifier = convertToBacking(bindGroupDescriptor.protectedLayout().get());

    Vector<BindGroupEntry> entries;
    entries.reserveInitialCapacity(bindGroupDescriptor.entries.size());
    for (const auto& entry : bindGroupDescriptor.entries) {
        auto convertedEntry = convertToBacking(entry);
        if (!convertedEntry)
            return std::nullopt;
        entries.append(WTFMove(*convertedEntry));
    }

    return { { WTFMove(*base), identifier, WTFMove(entries) } };
}

std::optional<WebCore::WebGPU::BindGroupDescriptor> ConvertFromBackingContext::convertFromBacking(const BindGroupDescriptor& bindGroupDescriptor)
{
    auto base = convertFromBacking(static_cast<const ObjectDescriptorBase&>(bindGroupDescriptor));
    if (!base)
        return std::nullopt;

    WeakPtr bindGroupLayout = convertBindGroupLayoutFromBacking(bindGroupDescriptor.bindGroupLayout);
    if (!bindGroupLayout)
        return std::nullopt;

    Vector<WebCore::WebGPU::BindGroupEntry> entries;
    entries.reserveInitialCapacity(bindGroupDescriptor.entries.size());
    for (const auto& backingEntry : bindGroupDescriptor.entries) {
        auto entry = convertFromBacking(backingEntry);
        if (!entry)
            return std::nullopt;
        entries.append(WTFMove(*entry));
    }

    return { { WTFMove(*base), *bindGroupLayout, WTFMove(entries) } };
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
