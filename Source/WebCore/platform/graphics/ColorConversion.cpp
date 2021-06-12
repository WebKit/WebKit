/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ColorConversion.h"

#include "Color.h"
#include "ColorSpace.h"
#include "DestinationColorSpace.h"
#include <wtf/MathExtras.h>

namespace WebCore {

// MARK: HSL conversions.

struct HSLHueCalculationResult {
    float hue;
    float min;
    float max;
    float chroma;
};

static HSLHueCalculationResult calculateHSLHue(const SRGBA<float>& color)
{
    auto [r, g, b, alpha] = color;
    auto [min, max] = std::minmax({ r, g, b });
    float chroma = max - min;

    float hue;
    if (!chroma)
        hue = 0;
    else if (max == r)
        hue = (60.0f * ((g - b) / chroma)) + 360.0f;
    else if (max == g)
        hue = (60.0f * ((b - r) / chroma)) + 120.0f;
    else
        hue = (60.0f * ((r - g) / chroma)) + 240.0f;

    if (hue >= 360.0f)
        hue -= 360.0f;

    return { hue, min, max, chroma };
}

HSLA<float> ColorConversion<HSLA<float>, SRGBA<float>>::convert(const SRGBA<float>& color)
{
    // https://drafts.csswg.org/css-color-4/#hsl-to-rgb
    auto [r, g, b, alpha] = color;
    auto [hue, min, max, chroma] = calculateHSLHue(color);

    float lightness = (0.5f * (max + min)) * 100.0f;
    float saturation;
    if (!chroma)
        saturation = 0;
    else if (lightness <= 50.0f)
        saturation = (chroma / (max + min)) * 100.0f;
    else
        saturation = (chroma / (2.0f - (max + min))) * 100.0f;

    return { hue, saturation, lightness, alpha };
}

SRGBA<float> ColorConversion<SRGBA<float>, HSLA<float>>::convert(const HSLA<float>& color)
{
    // https://drafts.csswg.org/css-color-4/#hsl-to-rgb
    auto [hue, saturation, lightness, alpha] = color;

    if (!saturation) {
        auto grey = lightness / 100.0f;
        return { grey, grey, grey, alpha };
    }

    // hueToRGB() wants hue in the 0-6 range.
    auto normalizedHue = (hue / 360.0f) * 6.0f;
    auto normalizedLightness = lightness / 100.0f;
    auto normalizedSaturation = saturation / 100.0f;

    auto hueForRed = normalizedHue + 2.0f;
    auto hueForGreen = normalizedHue;
    auto hueForBlue = normalizedHue - 2.0f;
    if (hueForRed > 6.0f)
        hueForRed -= 6.0f;
    else if (hueForBlue < 0.0f)
        hueForBlue += 6.0f;

    float temp2 = normalizedLightness <= 0.5f ? normalizedLightness * (1.0f + normalizedSaturation) : normalizedLightness + normalizedSaturation - normalizedLightness * normalizedSaturation;
    float temp1 = 2.0f * normalizedLightness - temp2;
    
    // Hue is in the range 0-6, other args in 0-1.
    auto hueToRGB = [](float temp1, float temp2, float hue) {
        if (hue < 1.0f)
            return temp1 + (temp2 - temp1) * hue;
        if (hue < 3.0f)
            return temp2;
        if (hue < 4.0f)
            return temp1 + (temp2 - temp1) * (4.0f - hue);
        return temp1;
    };

    return {
        hueToRGB(temp1, temp2, hueForRed),
        hueToRGB(temp1, temp2, hueForGreen),
        hueToRGB(temp1, temp2, hueForBlue),
        alpha
    };
}

// MARK: HWB conversions.

HWBA<float> ColorConversion<HWBA<float>, SRGBA<float>>::convert(const SRGBA<float>& color)
{
    // https://drafts.csswg.org/css-color-4/#rgb-to-hwb
    auto [hue, min, max, chroma] = calculateHSLHue(color);
    auto whiteness = min * 100.0f;
    auto blackness = (1.0f - max) * 100.0f;
    
    return { hue, whiteness, blackness, color.alpha };
}

SRGBA<float> ColorConversion<SRGBA<float>, HWBA<float>>::convert(const HWBA<float>& color)
{
    // https://drafts.csswg.org/css-color-4/#hwb-to-rgb
    auto [hue, whiteness, blackness, alpha] = color;

    if (whiteness + blackness == 100.0f) {
        auto grey = whiteness / 100.0f;
        return { grey, grey, grey, alpha };
    }

    // hueToRGB() wants hue in the 0-6 range.
    auto normalizedHue = (hue / 360.0f) * 6.0f;

    auto hueForRed = normalizedHue + 2.0f;
    auto hueForGreen = normalizedHue;
    auto hueForBlue = normalizedHue - 2.0f;
    if (hueForRed > 6.0f)
        hueForRed -= 6.0f;
    else if (hueForBlue < 0.0f)
        hueForBlue += 6.0f;

    auto normalizedWhiteness = whiteness / 100.0f;
    auto normalizedBlackness = blackness / 100.0f;

    // This is the hueToRGB function in convertColor<SRGBA<float>>(const HSLA&) with temp1 == 0
    // and temp2 == 1 strength reduced through it.
    auto hueToRGB = [](float hue) {
        if (hue < 1.0f)
            return hue;
        if (hue < 3.0f)
            return 1.0f;
        if (hue < 4.0f)
            return 4.0f - hue;
        return 0.0f;
    };

    auto applyWhitenessBlackness = [](float component, auto whiteness, auto blackness) {
        return (component * (1.0f - whiteness - blackness)) + whiteness;
    };

    return {
        applyWhitenessBlackness(hueToRGB(hueForRed), normalizedWhiteness, normalizedBlackness),
        applyWhitenessBlackness(hueToRGB(hueForGreen), normalizedWhiteness, normalizedBlackness),
        applyWhitenessBlackness(hueToRGB(hueForBlue), normalizedWhiteness, normalizedBlackness),
        alpha
    };
}

// MARK: Lab conversions.

static constexpr float LABe = 216.0f / 24389.0f;
static constexpr float LABk = 24389.0f / 27.0f;
static constexpr float D50WhiteValues[] = { 0.96422f, 1.0f, 0.82521f };

XYZA<float, WhitePoint::D50> ColorConversion<XYZA<float, WhitePoint::D50>, Lab<float>>::convert(const Lab<float>& color)
{
    float f1 = (color.lightness + 16.0f) / 116.0f;
    float f0 = f1 + (color.a / 500.0f);
    float f2 = f1 - (color.b / 200.0f);

    auto computeXAndZ = [](float t) {
        float tCubed = t * t * t;
        if (tCubed > LABe)
            return tCubed;

        return (116.0f * t - 16.0f) / LABk;
    };

    auto computeY = [](float t) {
        if (t > (LABk * LABe)) {
            float value = (t + 16.0) / 116.0;
            return value * value * value;
        }

        return t / LABk;
    };

    float x = D50WhiteValues[0] * computeXAndZ(f0);
    float y = D50WhiteValues[1] * computeY(color.lightness);
    float z = D50WhiteValues[2] * computeXAndZ(f2);

    return { x, y, z, color.alpha };
}

Lab<float> ColorConversion<Lab<float>, XYZA<float, WhitePoint::D50>>::convert(const XYZA<float, WhitePoint::D50>& color)
{
    float x = color.x / D50WhiteValues[0];
    float y = color.y / D50WhiteValues[1];
    float z = color.z / D50WhiteValues[2];

    auto fTransform = [](float value) {
        return value > LABe ? std::cbrt(value) : (LABk * value + 16.0f) / 116.0f;
    };

    float f0 = fTransform(x);
    float f1 = fTransform(y);
    float f2 = fTransform(z);

    float lightness = (116.0f * f1) - 16.0f;
    float a = 500.0f * (f0 - f1);
    float b = 200.0f * (f1 - f2);

    return { lightness, a, b, color.alpha };
}


// MARK: LCH conversions.

LCHA<float> ColorConversion<LCHA<float>, Lab<float>>::convert(const Lab<float>& color)
{
    // https://www.w3.org/TR/css-color-4/#lab-to-lch
    float hue = rad2deg(atan2(color.b, color.a));

    return {
        color.lightness,
        std::hypot(color.a, color.b),
        hue >= 0 ? hue : hue + 360,
        color.alpha
    };
}

Lab<float> ColorConversion<Lab<float>, LCHA<float>>::convert(const LCHA<float>& color)
{
    // https://www.w3.org/TR/css-color-4/#lch-to-lab
    float hueAngleRadians = deg2rad(color.hue);

    return {
        color.lightness,
        color.chroma * std::cos(hueAngleRadians),
        color.chroma * std::sin(hueAngleRadians),
        color.alpha
    };
}

// MARK: Conversion functions for raw color components with associated color spaces.

ColorComponents<float, 4> converColorComponents(ColorSpace inputColorSpace, ColorComponents<float, 4> inputColorComponents, ColorSpace outputColorSpace)
{
    return callWithColorType(inputColorComponents, inputColorSpace, [outputColorSpace] (const auto& inputColor) {
        switch (outputColorSpace) {
        case ColorSpace::A98RGB:
            return asColorComponents(convertColor<A98RGB<float>>(inputColor));
        case ColorSpace::DisplayP3:
            return asColorComponents(convertColor<DisplayP3<float>>(inputColor));
        case ColorSpace::LCH:
            return asColorComponents(convertColor<LCHA<float>>(inputColor));
        case ColorSpace::Lab:
            return asColorComponents(convertColor<Lab<float>>(inputColor));
        case ColorSpace::LinearSRGB:
            return asColorComponents(convertColor<LinearSRGBA<float>>(inputColor));
        case ColorSpace::ProPhotoRGB:
            return asColorComponents(convertColor<ProPhotoRGB<float>>(inputColor));
        case ColorSpace::Rec2020:
            return asColorComponents(convertColor<Rec2020<float>>(inputColor));
        case ColorSpace::SRGB:
            return asColorComponents(convertColor<SRGBA<float>>(inputColor));
        case ColorSpace::XYZ_D50:
            return asColorComponents(convertColor<XYZA<float, WhitePoint::D50>>(inputColor));
        }

        ASSERT_NOT_REACHED();
        return asColorComponents(convertColor<SRGBA<float>>(inputColor));
    });
}

ColorComponents<float, 4> converColorComponents(ColorSpace inputColorSpace, ColorComponents<float, 4> inputColorComponents, const DestinationColorSpace& outputColorSpace)
{
#if USE(CG)
    return platformConvertColorComponents(inputColorSpace, inputColorComponents, outputColorSpace);
#else
    return callWithColorType(inputColorComponents, inputColorSpace, [outputColorSpace] (const auto& inputColor) {
        switch (outputColorSpace.platformColorSpace()) {
        case PlatformColorSpace::Name::SRGB:
            return asColorComponents(convertColor<SRGBA<float>>(inputColor));
#if ENABLE(DESTINATION_COLOR_SPACE_LINEAR_SRGB)
        case PlatformColorSpace::Name::LinearSRGB:
            return asColorComponents(convertColor<LinearSRGBA<float>>(inputColor));
#endif
#if ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3)
        case PlatformColorSpace::Name::DisplayP3:
            return asColorComponents(convertColor<DisplayP3<float>>(inputColor));
#endif
        }

        ASSERT_NOT_REACHED();
        return asColorComponents(convertColor<SRGBA<float>>(inputColor));
    });
#endif
}

} // namespace WebCore
