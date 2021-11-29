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
#include <variant>
#include <wtf/Vector.h>

namespace WebKit::WebGPU {

struct Origin3DDict {
    PAL::WebGPU::IntegerCoordinate x { 0 };
    PAL::WebGPU::IntegerCoordinate y { 0 };
    PAL::WebGPU::IntegerCoordinate z { 0 };

    template<class Encoder> void encode(Encoder& encoder) const
    {
        encoder << x;
        encoder << y;
        encoder << z;
    }

    template<class Decoder> static std::optional<Origin3DDict> decode(Decoder& decoder)
    {
        std::optional<PAL::WebGPU::IntegerCoordinate> x;
        decoder >> x;
        if (!x)
            return std::nullopt;

        std::optional<PAL::WebGPU::IntegerCoordinate> y;
        decoder >> y;
        if (!y)
            return std::nullopt;

        std::optional<PAL::WebGPU::IntegerCoordinate> z;
        decoder >> z;
        if (!z)
            return std::nullopt;

        return { { WTFMove(*x), WTFMove(*y), WTFMove(*z) } };
    }
};

using Origin3D = std::variant<Vector<PAL::WebGPU::IntegerCoordinate>, Origin3DDict>;

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
