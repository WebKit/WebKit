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

#include "WebGPUIdentifier.h"
#include "WebGPUOrigin3D.h"
#include <optional>
#include <pal/graphics/WebGPU/WebGPUIntegralTypes.h>
#include <pal/graphics/WebGPU/WebGPUTextureAspect.h>
#include <wtf/Ref.h>

namespace WebKit::WebGPU {

struct ImageCopyTexture {
    WebGPUIdentifier texture;
    PAL::WebGPU::IntegerCoordinate mipLevel { 0 };
    std::optional<Origin3D> origin;
    PAL::WebGPU::TextureAspect aspect { PAL::WebGPU::TextureAspect::All };

    template<class Encoder> void encode(Encoder& encoder) const
    {
        encoder << texture;
        encoder << mipLevel;
        encoder << origin;
        encoder << aspect;
    }

    template<class Decoder> static std::optional<ImageCopyTexture> decode(Decoder& decoder)
    {
        std::optional<WebGPUIdentifier> texture;
        decoder >> texture;
        if (!texture)
            return std::nullopt;

        std::optional<PAL::WebGPU::IntegerCoordinate> mipLevel;
        decoder >> mipLevel;
        if (!mipLevel)
            return std::nullopt;

        std::optional<std::optional<Origin3D>> origin;
        decoder >> origin;
        if (!origin)
            return std::nullopt;

        std::optional<PAL::WebGPU::TextureAspect> aspect;
        decoder >> aspect;
        if (!aspect)
            return std::nullopt;

        return { { WTFMove(*texture), WTFMove(*mipLevel), WTFMove(*origin), WTFMove(*aspect) } };
    }
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
