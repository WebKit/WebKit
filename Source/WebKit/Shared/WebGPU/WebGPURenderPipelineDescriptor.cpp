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
#include "WebGPURenderPipelineDescriptor.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUConvertFromBackingContext.h"
#include "WebGPUConvertToBackingContext.h"
#include <pal/graphics/WebGPU/WebGPURenderPipelineDescriptor.h>

namespace WebKit::WebGPU {

std::optional<RenderPipelineDescriptor> ConvertToBackingContext::convertToBacking(const PAL::WebGPU::RenderPipelineDescriptor& renderPipelineDescriptor)
{
    auto base = convertToBacking(static_cast<const PAL::WebGPU::PipelineDescriptorBase&>(renderPipelineDescriptor));
    if (!base)
        return std::nullopt;

    auto vertex = convertToBacking(renderPipelineDescriptor.vertex);
    if (!vertex)
        return std::nullopt;

    std::optional<PrimitiveState> primitive;
    if (renderPipelineDescriptor.primitive) {
        primitive = convertToBacking(*renderPipelineDescriptor.primitive);
        if (!primitive)
            return std::nullopt;
    }

    std::optional<DepthStencilState> depthStencil;
    if (renderPipelineDescriptor.depthStencil) {
        depthStencil = convertToBacking(*renderPipelineDescriptor.depthStencil);
        if (!depthStencil)
            return std::nullopt;
    }

    std::optional<MultisampleState> multisample;
    if (renderPipelineDescriptor.multisample) {
        multisample = convertToBacking(*renderPipelineDescriptor.multisample);
        if (!multisample)
            return std::nullopt;
    }

    std::optional<FragmentState> fragment;
    if (renderPipelineDescriptor.fragment) {
        fragment = convertToBacking(*renderPipelineDescriptor.fragment);
        if (!fragment)
            return std::nullopt;
    }
    if (renderPipelineDescriptor.fragment && !fragment)
        return std::nullopt;

    return { { WTFMove(*base), WTFMove(*vertex), WTFMove(primitive), WTFMove(depthStencil), WTFMove(multisample), WTFMove(fragment) } };
}

std::optional<PAL::WebGPU::RenderPipelineDescriptor> ConvertFromBackingContext::convertFromBacking(const RenderPipelineDescriptor& renderPipelineDescriptor)
{
    auto base = convertFromBacking(static_cast<const PipelineDescriptorBase&>(renderPipelineDescriptor));
    if (!base)
        return std::nullopt;

    auto vertex = convertFromBacking(renderPipelineDescriptor.vertex);
    if (!vertex)
        return std::nullopt;

    std::optional<PAL::WebGPU::PrimitiveState> primitive;
    if (renderPipelineDescriptor.primitive) {
        primitive = convertFromBacking(*renderPipelineDescriptor.primitive);
        if (!primitive)
            return std::nullopt;
    }

    std::optional<PAL::WebGPU::DepthStencilState> depthStencil;
    if (renderPipelineDescriptor.depthStencil) {
        depthStencil = convertFromBacking(*renderPipelineDescriptor.depthStencil);
        if (!depthStencil)
            return std::nullopt;
    }

    std::optional<PAL::WebGPU::MultisampleState> multisample;
    if (renderPipelineDescriptor.multisample) {
        multisample = convertFromBacking(*renderPipelineDescriptor.multisample);
        if (!multisample)
            return std::nullopt;
    }

    auto fragment = ([&] () -> std::optional<PAL::WebGPU::FragmentState> {
        if (renderPipelineDescriptor.fragment)
            return convertFromBacking(*renderPipelineDescriptor.fragment);
        return std::nullopt;
    })();
    if (renderPipelineDescriptor.fragment && !fragment)
        return std::nullopt;

    return { { WTFMove(*base), WTFMove(*vertex), WTFMove(primitive), WTFMove(depthStencil), WTFMove(multisample), WTFMove(fragment) } };
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
