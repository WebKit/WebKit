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

#include "WebGPUBufferBinding.h"
#include "WebGPUIdentifier.h"
#include <cstdint>
#include <optional>
#include <pal/graphics/WebGPU/WebGPUIntegralTypes.h>

namespace WebKit::WebGPU {

enum class BindingResourceType : uint8_t {
    Sampler,
    TextureView,
    BufferBinding,
    ExternalTexture,
};

struct BindGroupEntry {
    PAL::WebGPU::Index32 binding { 0 };
    BufferBinding bufferBinding;
    WebGPUIdentifier identifier;
    BindingResourceType type { BindingResourceType::Sampler };

    template<class Encoder> void encode(Encoder& encoder) const
    {
        encoder << binding;
        encoder << bufferBinding;
        encoder << identifier;
        encoder << type;
    }

    template<class Decoder> static std::optional<BindGroupEntry> decode(Decoder& decoder)
    {
        std::optional<PAL::WebGPU::Index32> binding;
        decoder >> binding;
        if (!binding)
            return std::nullopt;

        std::optional<BufferBinding> bufferBinding;
        decoder >> bufferBinding;
        if (!bufferBinding)
            return std::nullopt;

        std::optional<WebGPUIdentifier> identifier;
        decoder >> identifier;
        if (!identifier)
            return std::nullopt;

        std::optional<BindingResourceType> type;
        decoder >> type;
        if (!type)
            return std::nullopt;

        return { { WTFMove(*binding), WTFMove(*bufferBinding), WTFMove(*identifier), WTFMove(*type) } };
    }
};

} // namespace WebKit::WebGPU

namespace WTF {

template<> struct EnumTraits<WebKit::WebGPU::BindingResourceType> {
    using values = EnumValues<
        WebKit::WebGPU::BindingResourceType,
        WebKit::WebGPU::BindingResourceType::Sampler,
        WebKit::WebGPU::BindingResourceType::TextureView,
        WebKit::WebGPU::BindingResourceType::BufferBinding,
        WebKit::WebGPU::BindingResourceType::ExternalTexture
    >;
};

} // namespace WTF

#endif // ENABLE(GPU_PROCESS)
