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
#include <pal/graphics/WebGPU/WebGPUIntegralTypes.h>
#include <pal/graphics/WebGPU/WebGPUVertexFormat.h>

namespace WebKit::WebGPU {

struct VertexAttribute {
    PAL::WebGPU::VertexFormat format { PAL::WebGPU::VertexFormat::Uint8x2 };
    PAL::WebGPU::Size64 offset { 0 };

    PAL::WebGPU::Index32 shaderLocation { 0 };

    template<class Encoder> void encode(Encoder& encoder) const
    {
        encoder << format;
        encoder << offset;
        encoder << shaderLocation;
    }

    template<class Decoder> static std::optional<VertexAttribute> decode(Decoder& decoder)
    {
        std::optional<PAL::WebGPU::VertexFormat> format;
        decoder >> format;
        if (!format)
            return std::nullopt;

        std::optional<PAL::WebGPU::Size64> offset;
        decoder >> offset;
        if (!offset)
            return std::nullopt;

        std::optional<PAL::WebGPU::Index32> shaderLocation;
        decoder >> shaderLocation;
        if (!shaderLocation)
            return std::nullopt;

        return { { WTFMove(*format), WTFMove(*offset), WTFMove(*shaderLocation) } };
    }
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
