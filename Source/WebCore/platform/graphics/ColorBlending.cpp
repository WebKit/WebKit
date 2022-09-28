/*
 * Copyright (C) 2003-2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ColorBlending.h"

#include "AnimationUtilities.h"
#include "Color.h"
#include "ColorInterpolation.h"

namespace WebCore {

Color blendSourceOver(const Color& backdrop, const Color& source)
{
    if (!backdrop.isVisible() || source.isOpaque())
        return source;

    if (!source.isVisible())
        return backdrop;

    auto [backdropR, backdropG, backdropB, backdropA] = backdrop.toColorTypeLossy<SRGBA<uint8_t>>().resolved();
    auto [sourceR, sourceG, sourceB, sourceA] = source.toColorTypeLossy<SRGBA<uint8_t>>().resolved();

    int d = 0xFF * (backdropA + sourceA) - backdropA * sourceA;
    int a = d / 0xFF;
    int r = (backdropR * backdropA * (0xFF - sourceA) + 0xFF * sourceA * sourceR) / d;
    int g = (backdropG * backdropA * (0xFF - sourceA) + 0xFF * sourceA * sourceG) / d;
    int b = (backdropB * backdropA * (0xFF - sourceA) + 0xFF * sourceA * sourceB) / d;

    return makeFromComponentsClamping<SRGBA<uint8_t>>(r, g, b, a);
}

Color blendWithWhite(const Color& color)
{
    constexpr int startAlpha = 153; // 60%
    constexpr int endAlpha = 204; // 80%;
    constexpr int alphaIncrement = 17;

    auto blendComponent = [](int c, int a) -> int {
        float alpha = a / 255.0f;
        int whiteBlend = 255 - a;
        c -= whiteBlend;
        return static_cast<int>(c / alpha);
    };

    // If the color contains alpha already, we leave it alone.
    if (!color.isOpaque())
        return color;

    auto [existingR, existingG, existingB, existingAlpha] = color.toColorTypeLossy<SRGBA<uint8_t>>().resolved();

    SRGBA<uint8_t> result;
    for (int alpha = startAlpha; alpha <= endAlpha; alpha += alphaIncrement) {
        // We have a solid color.  Convert to an equivalent color that looks the same when blended with white
        // at the current alpha.  Try using less transparency if the numbers end up being negative.
        int r = blendComponent(existingR, alpha);
        int g = blendComponent(existingG, alpha);
        int b = blendComponent(existingB, alpha);

        result = makeFromComponentsClamping<SRGBA<uint8_t>>(r, g, b, alpha);

        if (r >= 0 && g >= 0 && b >= 0)
            break;
    }

    // FIXME: Why is preserving the semantic bit desired and/or correct here?
    if (color.isSemantic())
        return { result, Color::Flags::Semantic };
    return result;
}

static bool requiresLegacyInterpolationRules(const Color& color)
{
    return color.callOnUnderlyingType([&] (const auto& underlyingColor) {
        using ColorType = std::decay_t<decltype(underlyingColor)>;

        if constexpr (std::is_same_v<ColorType, SRGBA<uint8_t>>)
            return true;
        else if constexpr (std::is_same_v<ColorType, SRGBA<float>>)
            return true;
        else if constexpr (std::is_same_v<ColorType, HSLA<float>>)
            return true;
        else if constexpr (std::is_same_v<ColorType, HWBA<float>>)
            return true;
        else if constexpr (std::is_same_v<ColorType, ExtendedSRGBA<float>>)
            return !color.usesColorFunctionSerialization();
        else
            return false;
    });
}

Color blend(const Color& from, const Color& to, const BlendingContext& context)
{
    // We need to preserve the state of the valid flag at the end of the animation
    if (context.progress == 1 && !to.isValid())
        return { };

    if (requiresLegacyInterpolationRules(from) && requiresLegacyInterpolationRules(to)) {
        using InterpolationColorSpace = ColorInterpolationMethod::SRGB;

        auto fromComponents = from.toColorTypeLossy<typename InterpolationColorSpace::ColorType>();
        auto toComponents = to.toColorTypeLossy<typename InterpolationColorSpace::ColorType>();

        switch (context.compositeOperation) {
        case CompositeOperation::Replace: {
            auto interpolatedColor = interpolateColorComponents<AlphaPremultiplication::Premultiplied>(InterpolationColorSpace { }, fromComponents, 1.0 - context.progress, toComponents, context.progress);
            return convertColor<SRGBA<uint8_t>>(clipToGamut<SRGBA<float>>(interpolatedColor));
        }
        case CompositeOperation::Add:
        case CompositeOperation::Accumulate:
            ASSERT(context.progress == 1.0);
            return addColorComponents<AlphaPremultiplication::Premultiplied>(InterpolationColorSpace { }, fromComponents, toComponents);
        }
    } else {
        using InterpolationColorSpace = ColorInterpolationMethod::OKLab;

        auto fromComponents = from.toColorTypeLossy<typename InterpolationColorSpace::ColorType>();
        auto toComponents = to.toColorTypeLossy<typename InterpolationColorSpace::ColorType>();

        switch (context.compositeOperation) {
        case CompositeOperation::Replace:
            return interpolateColorComponents<AlphaPremultiplication::Premultiplied>(InterpolationColorSpace { }, fromComponents, 1.0 - context.progress, toComponents, context.progress);

        case CompositeOperation::Add:
        case CompositeOperation::Accumulate:
            ASSERT(context.progress == 1.0);
            return addColorComponents<AlphaPremultiplication::Premultiplied>(InterpolationColorSpace { }, fromComponents, toComponents);
        }
    }
}

Color blendWithoutPremultiply(const Color& from, const Color& to, const BlendingContext& context)
{
    // FIXME: ExtendedColor - needs to handle color spaces.
    // We need to preserve the state of the valid flag at the end of the animation
    if (context.progress == 1 && !to.isValid())
        return { };

    auto fromSRGB = from.toColorTypeLossy<SRGBA<float>>().resolved();
    auto toSRGB = to.toColorTypeLossy<SRGBA<float>>().resolved();

    auto blended = makeFromComponentsClamping<SRGBA<float>>(
        WebCore::blend(fromSRGB.red, toSRGB.red, context),
        WebCore::blend(fromSRGB.green, toSRGB.green, context),
        WebCore::blend(fromSRGB.blue, toSRGB.blue, context),
        WebCore::blend(fromSRGB.alpha, toSRGB.alpha, context)
    );

    return convertColor<SRGBA<uint8_t>>(blended);
}

} // namespace WebCore
