/*
 * Copyright (C) 2026 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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

#include "config.h"
#include "GPUTextureViewDescriptor.h"

#include "JSGPUTextureAspect.h"
#include "JSGPUTextureFormat.h"
#include "JSGPUTextureViewDimension.h"
#include <wtf/JSONValues.h>

namespace WebCore {

static std::optional<WebGPU::ComponentSwizzle> parseComponentSwizzle(char16_t character)
{
    switch (character) {
    case 'r':
        return WebGPU::ComponentSwizzle::Red;
    case 'g':
        return WebGPU::ComponentSwizzle::Green;
    case 'b':
        return WebGPU::ComponentSwizzle::Blue;
    case 'a':
        return WebGPU::ComponentSwizzle::Alpha;
    case '0':
        return WebGPU::ComponentSwizzle::Zero;
    case '1':
        return WebGPU::ComponentSwizzle::One;
    default:
        return std::nullopt;
    }
}

std::optional<WebGPU::TextureComponentSwizzle> parseGPUTextureComponentSwizzle(const String& swizzle)
{
    if (swizzle.length() != 4)
        return std::nullopt;

    std::array<WebGPU::ComponentSwizzle, 4> components;
    for (unsigned i = 0; i < components.size(); ++i) {
        auto component = parseComponentSwizzle(swizzle[i]);
        if (!component)
            return std::nullopt;
        components[i] = *component;
    }

    return WebGPU::TextureComponentSwizzle { components[0], components[1], components[2], components[3] };
}

Ref<JSON::Object> GPUTextureViewDescriptor::toJSON() const
{
    Ref json = GPUObjectDescriptorBase::toJSON();
    if (format)
        json->setString("format"_s, convertEnumerationToString(*format));
    if (dimension)
        json->setString("dimension"_s, convertEnumerationToString(*dimension));
    json->setDouble("usage"_s, usage);
    json->setString("aspect"_s, convertEnumerationToString(aspect));
    json->setDouble("baseMipLevel"_s, baseMipLevel);
    if (mipLevelCount)
        json->setDouble("mipLevelCount"_s, *mipLevelCount);
    json->setDouble("baseArrayLayer"_s, baseArrayLayer);
    if (arrayLayerCount)
        json->setDouble("arrayLayerCount"_s, *arrayLayerCount);
    json->setString("swizzle"_s, swizzle);
    return json;
}

} // namespace WebCore
