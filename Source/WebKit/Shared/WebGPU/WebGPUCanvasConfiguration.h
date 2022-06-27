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

#include "WebGPUIdentifier.h"
#include <optional>
#include <pal/graphics/WebGPU/WebGPUCanvasCompositingAlphaMode.h>
#include <pal/graphics/WebGPU/WebGPUPredefinedColorSpace.h>
#include <pal/graphics/WebGPU/WebGPUTextureFormat.h>
#include <pal/graphics/WebGPU/WebGPUTextureUsage.h>
#include <wtf/Ref.h>

namespace WebKit::WebGPU {

class Device;

struct CanvasConfiguration {
    WebGPUIdentifier device;
    PAL::WebGPU::TextureFormat format { PAL::WebGPU::TextureFormat::R8unorm };
    PAL::WebGPU::TextureUsageFlags usage { PAL::WebGPU::TextureUsage::RenderAttachment };
    Vector<PAL::WebGPU::TextureFormat> viewFormats;
    PAL::WebGPU::PredefinedColorSpace colorSpace { PAL::WebGPU::PredefinedColorSpace::SRGB };
    PAL::WebGPU::CanvasCompositingAlphaMode compositingAlphaMode { PAL::WebGPU::CanvasCompositingAlphaMode::Opaque };

    template<class Encoder> void encode(Encoder& encoder) const
    {
        encoder << device;
        encoder << format;
        encoder << usage;
        encoder << viewFormats;
        encoder << colorSpace;
        encoder << compositingAlphaMode;
    }

    template<class Decoder> static std::optional<CanvasConfiguration> decode(Decoder& decoder)
    {
        std::optional<WebGPUIdentifier> device;
        decoder >> device;
        if (!device)
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

        std::optional<PAL::WebGPU::PredefinedColorSpace> colorSpace;
        decoder >> colorSpace;
        if (!colorSpace)
            return std::nullopt;

        std::optional<PAL::WebGPU::CanvasCompositingAlphaMode> compositingAlphaMode;
        decoder >> compositingAlphaMode;
        if (!compositingAlphaMode)
            return std::nullopt;

        return { { WTFMove(*device), WTFMove(*format), WTFMove(*usage), WTFMove(*viewFormats), WTFMove(*colorSpace), WTFMove(*compositingAlphaMode) } };
    }
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
