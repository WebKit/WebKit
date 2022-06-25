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

#include <optional>
#include <pal/graphics/WebGPU/WebGPUCullMode.h>
#include <pal/graphics/WebGPU/WebGPUFrontFace.h>
#include <pal/graphics/WebGPU/WebGPUIndexFormat.h>
#include <pal/graphics/WebGPU/WebGPUIntegralTypes.h>
#include <pal/graphics/WebGPU/WebGPUPrimitiveTopology.h>

namespace WebKit::WebGPU {

struct PrimitiveState {
    PAL::WebGPU::PrimitiveTopology topology { PAL::WebGPU::PrimitiveTopology::TriangleList };
    std::optional<PAL::WebGPU::IndexFormat> stripIndexFormat;
    PAL::WebGPU::FrontFace frontFace { PAL::WebGPU::FrontFace::CCW };
    PAL::WebGPU::CullMode cullMode { PAL::WebGPU::CullMode::None };

    // Requires "depth-clip-control" feature.
    bool unclippedDepth;

    template<class Encoder> void encode(Encoder& encoder) const
    {
        encoder << topology;
        encoder << stripIndexFormat;
        encoder << frontFace;
        encoder << cullMode;
        encoder << unclippedDepth;
    }

    template<class Decoder> static std::optional<PrimitiveState> decode(Decoder& decoder)
    {
        std::optional<PAL::WebGPU::PrimitiveTopology> topology;
        decoder >> topology;
        if (!topology)
            return std::nullopt;

        std::optional<std::optional<PAL::WebGPU::IndexFormat>> stripIndexFormat;
        decoder >> stripIndexFormat;
        if (!stripIndexFormat)
            return std::nullopt;

        std::optional<PAL::WebGPU::FrontFace> frontFace;
        decoder >> frontFace;
        if (!frontFace)
            return std::nullopt;

        std::optional<PAL::WebGPU::CullMode> cullMode;
        decoder >> cullMode;
        if (!cullMode)
            return std::nullopt;

        std::optional<bool> unclippedDepth;
        decoder >> unclippedDepth;
        if (!unclippedDepth)
            return std::nullopt;

        return { { WTFMove(*topology), WTFMove(*stripIndexFormat), WTFMove(*frontFace), WTFMove(*cullMode), WTFMove(*unclippedDepth) } };
    }
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
