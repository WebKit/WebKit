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

#include "WebGPUBufferBindingLayout.h"
#include "WebGPUExternalTextureBindingLayout.h"
#include "WebGPUSamplerBindingLayout.h"
#include "WebGPUStorageTextureBindingLayout.h"
#include "WebGPUTextureBindingLayout.h"
#include <optional>
#include <pal/graphics/WebGPU/WebGPUIntegralTypes.h>
#include <pal/graphics/WebGPU/WebGPUShaderStage.h>

namespace WebKit::WebGPU {

struct BindGroupLayoutEntry {
    PAL::WebGPU::Index32 binding { 0 };
    PAL::WebGPU::ShaderStageFlags visibility;

    std::optional<BufferBindingLayout> buffer;
    std::optional<SamplerBindingLayout> sampler;
    std::optional<TextureBindingLayout> texture;
    std::optional<StorageTextureBindingLayout> storageTexture;
    std::optional<ExternalTextureBindingLayout> externalTexture;

    template<class Encoder> void encode(Encoder& encoder) const
    {
        encoder << binding;
        encoder << visibility;
        encoder << buffer;
        encoder << sampler;
        encoder << texture;
        encoder << storageTexture;
        encoder << externalTexture;
    }

    template<class Decoder> static std::optional<BindGroupLayoutEntry> decode(Decoder& decoder)
    {
        std::optional<PAL::WebGPU::Index32> binding;
        decoder >> binding;
        if (!binding)
            return std::nullopt;

        std::optional<PAL::WebGPU::ShaderStageFlags> visibility;
        decoder >> visibility;
        if (!visibility)
            return std::nullopt;

        std::optional<std::optional<BufferBindingLayout>> buffer;
        decoder >> buffer;
        if (!buffer)
            return std::nullopt;

        std::optional<std::optional<SamplerBindingLayout>> sampler;
        decoder >> sampler;
        if (!sampler)
            return std::nullopt;

        std::optional<std::optional<TextureBindingLayout>> texture;
        decoder >> texture;
        if (!texture)
            return std::nullopt;

        std::optional<std::optional<StorageTextureBindingLayout>> storageTexture;
        decoder >> storageTexture;
        if (!storageTexture)
            return std::nullopt;

        std::optional<std::optional<ExternalTextureBindingLayout>> externalTexture;
        decoder >> externalTexture;
        if (!externalTexture)
            return std::nullopt;

        return { { WTFMove(*binding), WTFMove(*visibility), WTFMove(*buffer), WTFMove(*sampler), WTFMove(*texture), WTFMove(*storageTexture), WTFMove(*externalTexture) } };
    }
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
