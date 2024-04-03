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
#include "WebGPURenderPassDescriptor.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUConvertFromBackingContext.h"
#include "WebGPUConvertToBackingContext.h"
#include <WebCore/WebGPURenderPassDescriptor.h>

namespace WebKit::WebGPU {

std::optional<RenderPassDescriptor> ConvertToBackingContext::convertToBacking(const WebCore::WebGPU::RenderPassDescriptor& renderPassDescriptor)
{
    auto base = convertToBacking(static_cast<const WebCore::WebGPU::ObjectDescriptorBase&>(renderPassDescriptor));
    if (!base)
        return std::nullopt;

    Vector<std::optional<RenderPassColorAttachment>> colorAttachments;
    colorAttachments.reserveInitialCapacity(renderPassDescriptor.colorAttachments.size());
    for (const auto& colorAttachment : renderPassDescriptor.colorAttachments) {
        if (colorAttachment) {
            auto backingColorAttachment = convertToBacking(*colorAttachment);
            if (!backingColorAttachment)
                return std::nullopt;
            colorAttachments.append(WTFMove(*backingColorAttachment));
        } else
            colorAttachments.append(std::nullopt);
    }

    std::optional<RenderPassDepthStencilAttachment> depthStencilAttachment;
    if (renderPassDescriptor.depthStencilAttachment) {
        depthStencilAttachment = convertToBacking(*renderPassDescriptor.depthStencilAttachment);
        if (!depthStencilAttachment)
            return std::nullopt;
    }

    std::optional<WebGPUIdentifier> occlusionQuerySet;
    if (renderPassDescriptor.occlusionQuerySet) {
        occlusionQuerySet = convertToBacking(*renderPassDescriptor.occlusionQuerySet);
        if (!occlusionQuerySet)
            return std::nullopt;
    }

    auto timestampWrites = renderPassDescriptor.timestampWrites ? convertToBacking(*renderPassDescriptor.timestampWrites) : std::nullopt;

    return { { WTFMove(*base), WTFMove(colorAttachments), WTFMove(depthStencilAttachment), occlusionQuerySet, WTFMove(timestampWrites), renderPassDescriptor.maxDrawCount } };
}

std::optional<WebCore::WebGPU::RenderPassDescriptor> ConvertFromBackingContext::convertFromBacking(const RenderPassDescriptor& renderPassDescriptor)
{
    auto base = convertFromBacking(static_cast<const ObjectDescriptorBase&>(renderPassDescriptor));
    if (!base)
        return std::nullopt;

    Vector<std::optional<WebCore::WebGPU::RenderPassColorAttachment>> colorAttachments;
    colorAttachments.reserveInitialCapacity(renderPassDescriptor.colorAttachments.size());
    for (const auto& backingColorAttachment : renderPassDescriptor.colorAttachments) {
        if (backingColorAttachment) {
            auto colorAttachment = convertFromBacking(*backingColorAttachment);
            if (!colorAttachment)
                return std::nullopt;
            colorAttachments.append(WTFMove(*colorAttachment));
        } else
            colorAttachments.append(std::nullopt);
    }

    auto depthStencilAttachment = ([&] () -> std::optional<WebCore::WebGPU::RenderPassDepthStencilAttachment> {
        if (renderPassDescriptor.depthStencilAttachment)
            return convertFromBacking(*renderPassDescriptor.depthStencilAttachment);
        return std::nullopt;
    })();
    if (renderPassDescriptor.depthStencilAttachment && !depthStencilAttachment)
        return std::nullopt;

    WebCore::WebGPU::QuerySet* occlusionQuerySet = nullptr;
    if (renderPassDescriptor.occlusionQuerySet) {
        occlusionQuerySet = convertQuerySetFromBacking(renderPassDescriptor.occlusionQuerySet.value());
        if (!occlusionQuerySet)
            return std::nullopt;
    }

    auto timestampWrites = renderPassDescriptor.timestampWrites ? convertFromBacking(*renderPassDescriptor.timestampWrites) : std::nullopt;

    return { { WTFMove(*base), WTFMove(colorAttachments), WTFMove(depthStencilAttachment), occlusionQuerySet, WTFMove(timestampWrites), renderPassDescriptor.maxDrawCount } };
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
