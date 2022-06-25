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
#include <pal/graphics/WebGPU/WebGPUStorageTextureAccess.h>
#include <pal/graphics/WebGPU/WebGPUTextureFormat.h>
#include <pal/graphics/WebGPU/WebGPUTextureViewDimension.h>

namespace WebKit::WebGPU {

struct StorageTextureBindingLayout {
    PAL::WebGPU::StorageTextureAccess access { PAL::WebGPU::StorageTextureAccess::WriteOnly };
    PAL::WebGPU::TextureFormat format { PAL::WebGPU::TextureFormat::R8unorm };
    PAL::WebGPU::TextureViewDimension viewDimension { PAL::WebGPU::TextureViewDimension::_2d };

    template<class Encoder> void encode(Encoder& encoder) const
    {
        encoder << access;
        encoder << format;
        encoder << viewDimension;
    }

    template<class Decoder> static std::optional<StorageTextureBindingLayout> decode(Decoder& decoder)
    {
        std::optional<PAL::WebGPU::StorageTextureAccess> access;
        decoder >> access;
        if (!access)
            return std::nullopt;

        std::optional<PAL::WebGPU::TextureFormat> format;
        decoder >> format;
        if (!format)
            return std::nullopt;

        std::optional<PAL::WebGPU::TextureViewDimension> viewDimension;
        decoder >> viewDimension;
        if (!viewDimension)
            return std::nullopt;

        return { { WTFMove(*access), WTFMove(*format), WTFMove(*viewDimension) } };
    }
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
