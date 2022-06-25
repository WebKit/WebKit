/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#include "WebGPUExtent3D.h"
#include "WebGPUObjectDescriptorBase.h"
#include <optional>
#include <pal/graphics/WebGPU/WebGPUIntegralTypes.h>
#include <pal/graphics/WebGPU/WebGPUTextureDimension.h>
#include <pal/graphics/WebGPU/WebGPUTextureFormat.h>
#include <pal/graphics/WebGPU/WebGPUTextureUsage.h>

namespace WebKit::WebGPU {

struct TextureDescriptor : public ObjectDescriptorBase {
    Extent3D size;
    PAL::WebGPU::IntegerCoordinate mipLevelCount { 1 };
    PAL::WebGPU::Size32 sampleCount { 1 };
    PAL::WebGPU::TextureDimension dimension { PAL::WebGPU::TextureDimension::_2d };
    PAL::WebGPU::TextureFormat format { PAL::WebGPU::TextureFormat::R8unorm };
    PAL::WebGPU::TextureUsageFlags usage;
    Vector<PAL::WebGPU::TextureFormat> viewFormats;

    template<class Encoder> void encode(Encoder& encoder) const
    {
        encoder << static_cast<const ObjectDescriptorBase&>(*this);
        encoder << size;
        encoder << mipLevelCount;
        encoder << sampleCount;
        encoder << dimension;
        encoder << format;
        encoder << usage;
        encoder << viewFormats;
    }

    template<class Decoder> static std::optional<TextureDescriptor> decode(Decoder& decoder)
    {
        std::optional<ObjectDescriptorBase> objectDescriptorBase;
        decoder >> objectDescriptorBase;
        if (!objectDescriptorBase)
            return std::nullopt;

        std::optional<Extent3D> size;
        decoder >> size;
        if (!size)
            return std::nullopt;

        std::optional<PAL::WebGPU::IntegerCoordinate> mipLevelCount;
        decoder >> mipLevelCount;
        if (!mipLevelCount)
            return std::nullopt;

        std::optional<PAL::WebGPU::Size32> sampleCount;
        decoder >> sampleCount;
        if (!sampleCount)
            return std::nullopt;

        std::optional<PAL::WebGPU::TextureDimension> dimension;
        decoder >> dimension;
        if (!dimension)
            return std::nullopt;

        std::optional<PAL::WebGPU::TextureFormat> format;
        decoder >> format;
        if (!format)
            return std::nullopt;

        std::optional<PAL::WebGPU::TextureUsageFlags> usage;
        decoder >> usage;
        if (!usage)
            return std::nullopt;

        std::optional<Vector<PAL::WebGPU::TextureFormat>> viewFormats;
        decoder >> viewFormats;
        if (!viewFormats)
            return std::nullopt;

        return { { WTFMove(*objectDescriptorBase), WTFMove(*size), WTFMove(*mipLevelCount), WTFMove(*sampleCount), WTFMove(*dimension), WTFMove(*format), WTFMove(*usage), WTFMove(*viewFormats) } };
    }
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
