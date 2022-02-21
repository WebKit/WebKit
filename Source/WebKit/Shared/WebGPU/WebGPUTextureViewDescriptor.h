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

#include "WebGPUObjectDescriptorBase.h"
#include <optional>
#include <pal/graphics/WebGPU/WebGPUIntegralTypes.h>
#include <pal/graphics/WebGPU/WebGPUTextureAspect.h>
#include <pal/graphics/WebGPU/WebGPUTextureFormat.h>
#include <pal/graphics/WebGPU/WebGPUTextureViewDimension.h>

namespace WebKit::WebGPU {

struct TextureViewDescriptor : public ObjectDescriptorBase {
    std::optional<PAL::WebGPU::TextureFormat> format;
    std::optional<PAL::WebGPU::TextureViewDimension> dimension;
    PAL::WebGPU::TextureAspect aspect { PAL::WebGPU::TextureAspect::All };
    PAL::WebGPU::IntegerCoordinate baseMipLevel { 0 };
    std::optional<PAL::WebGPU::IntegerCoordinate> mipLevelCount;
    PAL::WebGPU::IntegerCoordinate baseArrayLayer { 0 };
    std::optional<PAL::WebGPU::IntegerCoordinate> arrayLayerCount;

    template<class Encoder> void encode(Encoder& encoder) const
    {
        encoder << static_cast<const ObjectDescriptorBase&>(*this);
        encoder << format;
        encoder << dimension;
        encoder << aspect;
        encoder << baseMipLevel;
        encoder << mipLevelCount;
        encoder << baseArrayLayer;
        encoder << arrayLayerCount;
    }

    template<class Decoder> static std::optional<TextureViewDescriptor> decode(Decoder& decoder)
    {
        std::optional<ObjectDescriptorBase> objectDescriptorBase;
        decoder >> objectDescriptorBase;
        if (!objectDescriptorBase)
            return std::nullopt;

        std::optional<std::optional<PAL::WebGPU::TextureFormat>> format;
        decoder >> format;
        if (!format)
            return std::nullopt;

        std::optional<std::optional<PAL::WebGPU::TextureViewDimension>> dimension;
        decoder >> dimension;
        if (!dimension)
            return std::nullopt;

        std::optional<PAL::WebGPU::TextureAspect> aspect;
        decoder >> aspect;
        if (!aspect)
            return std::nullopt;

        std::optional<PAL::WebGPU::IntegerCoordinate> baseMipLevel;
        decoder >> baseMipLevel;
        if (!baseMipLevel)
            return std::nullopt;

        std::optional<std::optional<PAL::WebGPU::IntegerCoordinate>> mipLevelCount;
        decoder >> mipLevelCount;
        if (!mipLevelCount)
            return std::nullopt;

        std::optional<PAL::WebGPU::IntegerCoordinate> baseArrayLayer;
        decoder >> baseArrayLayer;
        if (!baseArrayLayer)
            return std::nullopt;

        std::optional<std::optional<PAL::WebGPU::IntegerCoordinate>> arrayLayerCount;
        decoder >> arrayLayerCount;
        if (!arrayLayerCount)
            return std::nullopt;

        return { { WTFMove(*objectDescriptorBase), WTFMove(*format), WTFMove(*dimension), WTFMove(*aspect), WTFMove(*baseMipLevel), WTFMove(*mipLevelCount), WTFMove(*baseArrayLayer), WTFMove(*arrayLayerCount) } };
    }
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
