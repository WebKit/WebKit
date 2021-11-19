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

#if ENABLE(GPU_PROCESS)

#include "WebGPUDepthStencilState.h"
#include "WebGPUFragmentState.h"
#include "WebGPUMultisampleState.h"
#include "WebGPUPipelineDescriptorBase.h"
#include "WebGPUPrimitiveState.h"
#include "WebGPUVertexState.h"
#include <optional>

namespace WebKit::WebGPU {

struct RenderPipelineDescriptor : public PipelineDescriptorBase {
    VertexState vertex;
    std::optional<PrimitiveState> primitive;
    std::optional<DepthStencilState> depthStencil;
    std::optional<MultisampleState> multisample;
    std::optional<FragmentState> fragment;

    template<class Encoder> void encode(Encoder& encoder) const
    {
        encoder << static_cast<const PipelineDescriptorBase&>(*this);
        encoder << vertex;
        encoder << primitive;
        encoder << depthStencil;
        encoder << multisample;
        encoder << fragment;
    }

    template<class Decoder> static std::optional<RenderPipelineDescriptor> decode(Decoder& decoder)
    {
        std::optional<PipelineDescriptorBase> pipelineDescriptorBase;
        decoder >> pipelineDescriptorBase;
        if (!pipelineDescriptorBase)
            return std::nullopt;

        std::optional<VertexState> vertex;
        decoder >> vertex;
        if (!vertex)
            return std::nullopt;

        std::optional<std::optional<PrimitiveState>> primitive;
        decoder >> primitive;
        if (!primitive)
            return std::nullopt;

        std::optional<std::optional<DepthStencilState>> depthStencil;
        decoder >> depthStencil;
        if (!depthStencil)
            return std::nullopt;

        std::optional<std::optional<MultisampleState>> multisample;
        decoder >> multisample;
        if (!multisample)
            return std::nullopt;

        std::optional<std::optional<FragmentState>> fragment;
        decoder >> fragment;
        if (!fragment)
            return std::nullopt;

        return { { WTFMove(*pipelineDescriptorBase), WTFMove(*vertex), WTFMove(*primitive), WTFMove(*depthStencil), WTFMove(*multisample), WTFMove(*fragment) } };
    }
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
