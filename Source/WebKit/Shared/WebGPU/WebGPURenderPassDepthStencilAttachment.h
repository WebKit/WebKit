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
#include <pal/graphics/WebGPU/WebGPUIntegralTypes.h>
#include <pal/graphics/WebGPU/WebGPULoadOp.h>
#include <pal/graphics/WebGPU/WebGPUStoreOp.h>
#include <variant>
#include <wtf/Ref.h>

namespace WebKit::WebGPU {

struct RenderPassDepthStencilAttachment {
    WebGPUIdentifier view;

    float depthClearValue { 0 };
    std::optional<PAL::WebGPU::LoadOp> depthLoadOp;
    std::optional<PAL::WebGPU::StoreOp> depthStoreOp;
    bool depthReadOnly { false };

    PAL::WebGPU::StencilValue stencilClearValue { 0 };
    std::optional<PAL::WebGPU::LoadOp> stencilLoadOp;
    std::optional<PAL::WebGPU::StoreOp> stencilStoreOp;
    bool stencilReadOnly { false };

    template<class Encoder> void encode(Encoder& encoder) const
    {
        encoder << view;
        encoder << depthClearValue;
        encoder << depthLoadOp;
        encoder << depthStoreOp;
        encoder << depthReadOnly;
        encoder << stencilClearValue;
        encoder << stencilLoadOp;
        encoder << stencilStoreOp;
        encoder << stencilReadOnly;
    }

    template<class Decoder> static std::optional<RenderPassDepthStencilAttachment> decode(Decoder& decoder)
    {
        std::optional<WebGPUIdentifier> view;
        decoder >> view;
        if (!view)
            return std::nullopt;

        std::optional<float> depthClearValue;
        decoder >> depthClearValue;
        if (!depthClearValue)
            return std::nullopt;

        std::optional<std::optional<PAL::WebGPU::LoadOp>> depthLoadOp;
        decoder >> depthLoadOp;
        if (!depthLoadOp)
            return std::nullopt;

        std::optional<std::optional<PAL::WebGPU::StoreOp>> depthStoreOp;
        decoder >> depthStoreOp;
        if (!depthStoreOp)
            return std::nullopt;

        std::optional<bool> depthReadOnly;
        decoder >> depthReadOnly;
        if (!depthReadOnly)
            return std::nullopt;

        std::optional<PAL::WebGPU::StencilValue> stencilClearValue;
        decoder >> stencilClearValue;
        if (!stencilClearValue)
            return std::nullopt;

        std::optional<std::optional<PAL::WebGPU::LoadOp>> stencilLoadOp;
        decoder >> stencilLoadOp;
        if (!stencilLoadOp)
            return std::nullopt;

        std::optional<std::optional<PAL::WebGPU::StoreOp>> stencilStoreOp;
        decoder >> stencilStoreOp;
        if (!stencilStoreOp)
            return std::nullopt;

        std::optional<bool> stencilReadOnly;
        decoder >> stencilReadOnly;
        if (!stencilReadOnly)
            return std::nullopt;

        return { { WTFMove(*view), WTFMove(*depthClearValue), WTFMove(*depthLoadOp), WTFMove(*depthStoreOp), WTFMove(*depthReadOnly), WTFMove(*stencilClearValue), WTFMove(*stencilLoadOp), WTFMove(*stencilStoreOp), WTFMove(*stencilReadOnly) } };
    }
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
