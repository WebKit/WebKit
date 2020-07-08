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

namespace WebCore {

Color blendSourceOver(const Color& backdrop, const Color& source)
{
    if (!backdrop.isVisible() || source.isOpaque())
        return source;

    if (!source.alpha())
        return backdrop;

    auto [backdropR, backdropG, backdropB, backdropA] = backdrop.toSRGBALossy<uint8_t>();
    auto [sourceR, sourceG, sourceB, sourceA] = source.toSRGBALossy<uint8_t>();

    int d = 0xFF * (backdropA + sourceA) - backdropA * sourceA;
    int a = d / 0xFF;
    int r = (backdropR * backdropA * (0xFF - sourceA) + 0xFF * sourceA * sourceR) / d;
    int g = (backdropG * backdropA * (0xFF - sourceA) + 0xFF * sourceA * sourceG) / d;
    int b = (backdropB * backdropA * (0xFF - sourceA) + 0xFF * sourceA * sourceB) / d;

    return makeSimpleColor(r, g, b, a);
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

    auto [existingR, existingG, existingB, existingAlpha] = color.toSRGBALossy<uint8_t>();

    SRGBA<uint8_t> result;
    for (int alpha = startAlpha; alpha <= endAlpha; alpha += alphaIncrement) {
        // We have a solid color.  Convert to an equivalent color that looks the same when blended with white
        // at the current alpha.  Try using less transparency if the numbers end up being negative.
        int r = blendComponent(existingR, alpha);
        int g = blendComponent(existingG, alpha);
        int b = blendComponent(existingB, alpha);

        result = makeSimpleColor(r, g, b, alpha);

        if (r >= 0 && g >= 0 && b >= 0)
            break;
    }

    // FIXME: Why is preserving the semantic bit desired and/or correct here?
    if (color.isSemantic())
        return Color(result, Color::Semantic);
    return result;
}

Color blend(const Color& from, const Color& to, double progress)
{
    // FIXME: ExtendedColor - needs to handle color spaces.
    // We need to preserve the state of the valid flag at the end of the animation
    if (progress == 1 && !to.isValid())
        return { };

    auto premultipliedFrom = premultipliedCeiling(from.toSRGBALossy<uint8_t>());
    auto premultipliedTo = premultipliedCeiling(to.toSRGBALossy<uint8_t>());

    auto premultipliedBlended = clampToComponentBytes<SRGBA>(
        WebCore::blend(premultipliedFrom.red, premultipliedTo.red, progress),
        WebCore::blend(premultipliedFrom.green, premultipliedTo.green, progress),
        WebCore::blend(premultipliedFrom.blue, premultipliedTo.blue, progress),
        WebCore::blend(premultipliedFrom.alpha, premultipliedTo.alpha, progress)
    );

    return unpremultiplied(premultipliedBlended);
}

Color blendWithoutPremultiply(const Color& from, const Color& to, double progress)
{
    // FIXME: ExtendedColor - needs to handle color spaces.
    // We need to preserve the state of the valid flag at the end of the animation
    if (progress == 1 && !to.isValid())
        return { };

    auto fromSRGB = from.toSRGBALossy<uint8_t>();
    auto toSRGB = from.toSRGBALossy<uint8_t>();

    return clampToComponentBytes<SRGBA>(
        WebCore::blend(fromSRGB.red, toSRGB.red, progress),
        WebCore::blend(fromSRGB.green, toSRGB.green, progress),
        WebCore::blend(fromSRGB.blue, toSRGB.blue, progress),
        WebCore::blend(fromSRGB.alpha, toSRGB.alpha, progress)
    );
}

} // namespace WebCore
