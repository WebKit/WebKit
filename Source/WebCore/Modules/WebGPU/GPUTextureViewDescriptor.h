/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include "GPUIntegralTypes.h"
#include "GPUObjectDescriptorBase.h"
#include "GPUTextureAspect.h"
#include "GPUTextureFormat.h"
#include "GPUTextureUsage.h"
#include "GPUTextureViewDimension.h"
#include "WebGPUTextureViewDescriptor.h"
#include <optional>
#include <wtf/Forward.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

inline std::optional<WebGPU::ComponentSwizzle> convertSwizzleComponentToBacking(char16_t component)
{
    switch (component) {
    case 'r':
        return WebGPU::ComponentSwizzle::R;
    case 'g':
        return WebGPU::ComponentSwizzle::G;
    case 'b':
        return WebGPU::ComponentSwizzle::B;
    case 'a':
        return WebGPU::ComponentSwizzle::A;
    case '0':
        return WebGPU::ComponentSwizzle::Zero;
    case '1':
        return WebGPU::ComponentSwizzle::One;
    default:
        return std::nullopt;
    }
}

struct GPUTextureViewDescriptor : public GPUObjectDescriptorBase {
    // Returns std::nullopt if swizzle is not four characters, each of which is one of "rgba01".
    std::optional<WebGPU::TextureViewDescriptor> convertToBacking() const
    {
        if (swizzle.length() != 4)
            return std::nullopt;

        auto r = convertSwizzleComponentToBacking(swizzle[0]);
        auto g = convertSwizzleComponentToBacking(swizzle[1]);
        auto b = convertSwizzleComponentToBacking(swizzle[2]);
        auto a = convertSwizzleComponentToBacking(swizzle[3]);
        if (!r || !g || !b || !a)
            return std::nullopt;

        return { {
            { label },
            format ? std::optional { WebCore::convertToBacking(*format) } : std::nullopt,
            dimension ? std::optional { WebCore::convertToBacking(*dimension) } : std::nullopt,
            convertTextureUsageFlagsToBacking(usage),
            WebCore::convertToBacking(aspect),
            baseMipLevel,
            mipLevelCount,
            baseArrayLayer,
            arrayLayerCount,
            { *r, *g, *b, *a },
        } };
    }

    Ref<JSON::Object> toJSON() const;

    std::optional<GPUTextureFormat> format;
    std::optional<GPUTextureViewDimension> dimension;
    GPUTextureUsageFlags usage { 0 };
    GPUTextureAspect aspect { GPUTextureAspect::All };
    GPUIntegerCoordinate baseMipLevel { 0 };
    std::optional<GPUIntegerCoordinate> mipLevelCount;
    GPUIntegerCoordinate baseArrayLayer { 0 };
    std::optional<GPUIntegerCoordinate> arrayLayerCount;
    String swizzle { "rgba"_s };
};

}
