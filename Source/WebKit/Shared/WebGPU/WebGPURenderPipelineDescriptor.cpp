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
#include "WebGPURenderPipelineDescriptor.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUConvertFromBackingContext.h"
#include "WebGPUConvertToBackingContext.h"
#include <WebCore/WebGPURenderPipelineDescriptor.h>

namespace WebKit::WebGPU {

std::optional<RenderPipelineDescriptor> ConvertToBackingContext::convertToBacking(const ::WebGPU::RenderPipelineDescriptor& renderPipelineDescriptor)
{
    std::optional<WebGPUIdentifier> layout;
    if (renderPipelineDescriptor.layout)
        layout = convertToBacking(*protect(renderPipelineDescriptor.layout));

    auto vertex = convertToBacking(renderPipelineDescriptor.vertex);
    if (!vertex)
        return std::nullopt;

    auto primitive = convertToBacking(renderPipelineDescriptor.primitive);
    if (!primitive)
        return std::nullopt;

    std::optional<DepthStencilState> depthStencil;
    if (renderPipelineDescriptor.depthStencil) {
        depthStencil = convertToBacking(*renderPipelineDescriptor.depthStencil);
        if (!depthStencil)
            return std::nullopt;
    }

    auto multisample = convertToBacking(renderPipelineDescriptor.multisample);
    if (!multisample)
        return std::nullopt;

    std::optional<FragmentState> fragment;
    if (renderPipelineDescriptor.fragment) {
        fragment = convertToBacking(*renderPipelineDescriptor.fragment);
        if (!fragment)
            return std::nullopt;
    }

    return { { { { renderPipelineDescriptor.label }, layout }, WTF::move(*vertex), WTF::move(primitive), WTF::move(depthStencil), WTF::move(multisample), WTF::move(fragment) } };
}

std::optional<::WebGPU::RenderPipelineDescriptor> ConvertFromBackingContext::convertFromBacking(const RenderPipelineDescriptor& renderPipelineDescriptor, RenderPipelineDescriptorStorage& storage, bool allowMissingPipelineLayout)
{
    auto layout = convertLayoutFromBacking(renderPipelineDescriptor, allowMissingPipelineLayout);
    if (!layout)
        return std::nullopt;

    auto vertex = convertFromBacking(renderPipelineDescriptor.vertex, storage);
    if (!vertex)
        return std::nullopt;

    ::WebGPU::PrimitiveState primitive;
    if (renderPipelineDescriptor.primitive) {
        auto convertedPrimitive = convertFromBacking(*renderPipelineDescriptor.primitive);
        if (!convertedPrimitive)
            return std::nullopt;
        primitive = *convertedPrimitive;
    }

    std::optional<::WebGPU::DepthStencilState> depthStencil;
    if (renderPipelineDescriptor.depthStencil) {
        depthStencil = convertFromBacking(*renderPipelineDescriptor.depthStencil);
        if (!depthStencil)
            return std::nullopt;
    }

    ::WebGPU::MultisampleState multisample;
    if (renderPipelineDescriptor.multisample) {
        auto convertedMultisample = convertFromBacking(*renderPipelineDescriptor.multisample);
        if (!convertedMultisample)
            return std::nullopt;
        multisample = *convertedMultisample;
    }

    std::optional<::WebGPU::FragmentState> fragment;
    if (renderPipelineDescriptor.fragment) {
        fragment = convertFromBacking(*renderPipelineDescriptor.fragment, storage);
        if (!fragment)
            return std::nullopt;
    }

    return { { renderPipelineDescriptor.label, WTF::move(*layout), WTF::move(*vertex), primitive, WTF::move(depthStencil), multisample, WTF::move(fragment) } };
}

std::optional<RefPtr<::WebGPU::PipelineLayout>> ConvertFromBackingContext::convertLayoutFromBacking(const PipelineDescriptorBase& pipelineDescriptorBase, bool allowMissingPipelineLayout)
{
    if (!pipelineDescriptorBase.layout) {
        if (!allowMissingPipelineLayout)
            return std::nullopt;
        return { nullptr };
    }

    RefPtr layout = convertPipelineLayoutFromBacking(*pipelineDescriptorBase.layout);
    if (!layout)
        return std::nullopt;
    return { WTF::move(layout) };
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
