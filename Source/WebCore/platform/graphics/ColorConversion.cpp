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
#include "ColorNormalization.h"
#include "ColorSpace.h"
#include "DestinationColorSpace.h"
#include <wtf/MathExtras.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebCore {

// MARK: Lab-Like to LCH-Like conversion utilities.

template<typename LCHLike, typename LabLike>
LCHLike convertToPolarForm(const LabLike& color)
{
    // https://drafts.csswg.org/css-color/#lab-to-lch
    auto [lightness, a, b, alpha] = color.resolved();

    // Epsilon chosen to ensure `white` (the SRGB named value) is
    // considered achromatic (e.g. we set hue to NaN) when converted
    // to lch/oklch.
    constexpr auto epsilon = 0.02f;

    float chroma = std::hypot(a, b);
    float hue = (std::abs(a) < epsilon && std::abs(b) < epsilon) ? std::numeric_limits<float>::quiet_NaN() : rad2deg(atan2(b, a));

    return { lightness, chroma, hue >= 0.0f ? hue : hue + 360.0f, alpha };
}

template<typename LabLike, typename LCHLike>
LabLike convertToRectangularForm(const LCHLike& color)
{
    // https://drafts.csswg.org/css-color/#lch-to-lab
    auto [lightness, chroma, hue, alpha] = color.resolved();

    if (std::isnan(color.unresolved().hue))
        return { lightness, 0, 0, alpha };

    float hueAngleRadians = deg2rad(hue);
    float a = chroma * std::cos(hueAngleRadians);
    float b = chroma * std::sin(hueAngleRadians);

    return { lightness, a, b, alpha };
}

// MARK: HSL conversions.

HSLA<float> ColorConversion<HSLA<float>, ExtendedSRGBA<float>>::convert(const ExtendedSRGBA<float>& color)
{
    // https://drafts.csswg.org/css-color-4/#hsl-to-rgb

    auto [red, green, blue, alpha] = color.resolved();

    auto [min, max] = std::minmax({ red, green, blue });
    auto d = max - min;

    float hue = std::numeric_limits<float>::quiet_NaN();
    float lightness = (min + max) / 2.0f;
    float saturation;

    if (d != 0.0f) {
        if (lightness == 0.0f || lightness == 1.0f)
            saturation = 0.0f;
        else
            saturation = (max - lightness) / std::min(lightness, 1.0f - lightness);

        if (max == red)
            hue = ((green - blue) / d) + (green < blue ? 6.0f : 0.0f);
        else if (max == green)
            hue = ((blue - red) / d) + 2.0f;
        else if (max == blue)
            hue = ((red - green) / d) + 4.0f;

        hue *= 60.0f;

        if (saturation < 0.0f) {
            hue += 180.0f;
            saturation = std::abs(saturation);
        }

        if (hue >= 360.0f)
            hue -= 360.0f;
    } else
        saturation = 0.0f;

    return { hue, saturation * 100.0f, lightness * 100.0f, alpha };
}

ExtendedSRGBA<float> ColorConversion<ExtendedSRGBA<float>, HSLA<float>>::convert(const HSLA<float>& color)
{
    // https://drafts.csswg.org/css-color-4/#hsl-to-rgb

    auto [hue, saturation, lightness, alpha] = color.resolved();

    float scaledHue = hue / 30.0f;
    float scaledSaturation = saturation * (1.0f / 100.0f);
    float scaledLightness = lightness * (1.0f / 100.0f);

    auto a = scaledSaturation * std::min(scaledLightness, 1.0f - scaledLightness);

    auto hueToRGB = [&](float n) {
        auto k = std::fmod(n + scaledHue, 12.0f);
        return scaledLightness - (a * std::max(-1.0f, std::min({ k - 3.0f, 9.0f - k, 1.0f })));
    };

    return { hueToRGB(0), hueToRGB(8), hueToRGB(4), alpha };
}

// MARK: HWB conversions.

HWBA<float> ColorConversion<HWBA<float>, ExtendedSRGBA<float>>::convert(const ExtendedSRGBA<float>& color)
{
    // https://drafts.csswg.org/css-color-4/#rgb-to-hwb

    auto [red, green, blue, alpha] = color.resolved();

    auto [min, max] = std::minmax({ red, green, blue });
    auto d = max - min;

    float hue = std::numeric_limits<float>::quiet_NaN();

    // Compute `hue` as done in conversion to HSLA, but don't adjust for negative saturation, in order to respect out-of-gamut colors.
    if (d != 0.0f) {
        if (max == red)
            hue = ((green - blue) / d) + (green < blue ? 6.0f : 0.0f);
        else if (max == green)
            hue = ((blue - red) / d) + 2.0f;
        else if (max == blue)
            hue = ((red - green) / d) + 4.0f;

        hue *= 60.0f;

        if (hue >= 360.0f)
            hue -= 360.0f;
    }

    auto whiteness = min * 100.0f;
    auto blackness = (1.0f - max) * 100.0f;
    
    return { hue, whiteness, blackness, alpha };
}

ExtendedSRGBA<float> ColorConversion<ExtendedSRGBA<float>, HWBA<float>>::convert(const HWBA<float>& color)
{
    // https://drafts.csswg.org/css-color-4/#hwb-to-rgb
    auto [hue, whiteness, blackness, alpha] = color.resolved();

    float scaledWhiteness = whiteness / 100.0f;
    float scaledBlackness = blackness / 100.0f;

    if (scaledWhiteness + scaledBlackness >= 1.0f) {
        auto grey = scaledWhiteness / (scaledWhiteness + scaledBlackness);
        return { grey, grey, grey, alpha };
    }

    float scaledHue = hue / 30.0f;
    float whitenessBlacknessFactor = 1.0f - scaledWhiteness - scaledBlackness;

    auto hueToRGB = [&](float n) {
        // Perform RGB selection from HSL conversion as if called with [ hue, 100%, 50% ].
        auto k = std::fmod(n + scaledHue, 12.0f);
        auto component = 0.5f - (std::max(-1.0f, std::min({ k - 3.0f, 9.0f - k, 1.0f })) / 2.0f);

        // Then apply whiteness/blackness to the component.
        return (component * whitenessBlacknessFactor) + scaledWhiteness;
    };

    return { hueToRGB(0), hueToRGB(8), hueToRGB(4), alpha };
}

// MARK: Lab conversions.

constexpr float LABe = 216.0 / 24389.0;
constexpr float LABk = 24389.0 / 27.0;

XYZA<float, WhitePoint::D50> ColorConversion<XYZA<float, WhitePoint::D50>, Lab<float>>::convert(const Lab<float>& color)
{
    auto [lightness, a, b, alpha] = color.resolved();

    float f1 = (lightness + 16.0f) / 116.0f;
    float f0 = f1 + (a / 500.0f);
    float f2 = f1 - (b / 200.0f);

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

    float x = D50WhitePoint[0] * computeXAndZ(f0);
    float y = D50WhitePoint[1] * computeY(lightness);
    float z = D50WhitePoint[2] * computeXAndZ(f2);

    return { x, y, z, alpha };
}

Lab<float> ColorConversion<Lab<float>, XYZA<float, WhitePoint::D50>>::convert(const XYZA<float, WhitePoint::D50>& color)
{
    auto [x, y, z, alpha] = color.resolved();

    float adjustedX = x / D50WhitePoint[0];
    float adjustedY = y / D50WhitePoint[1];
    float adjustedZ = z / D50WhitePoint[2];

    auto fTransform = [](float value) {
        return value > LABe ? std::cbrt(value) : (LABk * value + 16.0f) / 116.0f;
    };

    float f0 = fTransform(adjustedX);
    float f1 = fTransform(adjustedY);
    float f2 = fTransform(adjustedZ);

    float lightness = (116.0f * f1) - 16.0f;
    float a = 500.0f * (f0 - f1);
    float b = 200.0f * (f1 - f2);

    return makeFromComponentsClampingExceptAlpha<Lab<float>>(lightness, a, b, alpha);
}

// MARK: LCH conversions.

LCHA<float> ColorConversion<LCHA<float>, Lab<float>>::convert(const Lab<float>& color)
{
    return convertToPolarForm<LCHA<float>>(color);
}

Lab<float> ColorConversion<Lab<float>, LCHA<float>>::convert(const LCHA<float>& color)
{
    return convertToRectangularForm<Lab<float>>(color);
}

// MARK: OKLab conversions.

XYZA<float, WhitePoint::D65> ColorConversion<XYZA<float, WhitePoint::D65>, OKLab<float>>::convert(const OKLab<float>& color)
{
    // FIXME: This could be optimized for when we are not explicitly converting to XYZ-D65 by pre-multiplying the 'LMSToXYZD65'
    // matrix with any subsequent matrices in the conversion. This would mean teaching the main conversion about this matrix
    // and adding new logic for this transform.

    // https://bottosson.github.io/posts/oklab/ with XYZ <-> LMS matrices recalculated for consistent reference white in https://github.com/w3c/csswg-drafts/issues/6642#issuecomment-943521484

    constexpr ColorMatrix<3, 3> LinearLMSToXYZD65 {
         1.2268798758459243, -0.5578149944602171,  0.2813910456659647,
        -0.0405757452148008,  1.1122868032803170, -0.0717110580655164,
        -0.0763729366746601, -0.4214933324022432,  1.5869240198367816,
    };

    constexpr ColorMatrix<3, 3> OKLabToNonLinearLMS {
         1.0000000000000000,  0.3963377773761749,  0.2158037573099136,
         1.0000000000000000, -0.1055613458156586, -0.0638541728258133,
         1.0000000000000000, -0.0894841775298119, -1.2914855480194092,
    };

    auto [lightness, a, b, alpha] = color.resolved();

    auto components = ColorComponents<float, 3> { lightness, a, b };

    // 1. Transform from Lab-coordinates into non-linear LMS "approximate cone responses".
    auto nonLinearLMS = OKLabToNonLinearLMS.transformedColorComponents(components);

    // 2. Apply linearity.
    auto linearLMS = nonLinearLMS.map([] (float v) { return v * v * v; });

    // 3. Convert to XYZ.
    auto [x, y, z] = LinearLMSToXYZD65.transformedColorComponents(linearLMS);

    return { x, y, z, alpha };
}

OKLab<float> ColorConversion<OKLab<float>, XYZA<float, WhitePoint::D65>>::convert(const XYZA<float, WhitePoint::D65>& color)
{
    // FIXME: This could be optimized for when we are not explicitly converting from XYZ-D65 by pre-multiplying the 'XYZD65ToLMS'
    // matrix with any previous matrices in the conversion. This would mean teaching the main conversion about this matrix
    // and adding new logic for this transform.

    // https://bottosson.github.io/posts/oklab/ with XYZ <-> LMS matrices recalculated for consistent reference white in https://github.com/w3c/csswg-drafts/issues/6642#issuecomment-943521484

    constexpr ColorMatrix<3, 3> XYZD65ToLinearLMS {
        0.8190224379967030,  0.3619062600528904, -0.1288737815209879,
        0.0329836539323885,  0.9292868615863434,  0.0361446663506424,
        0.0481771893596242,  0.2642395317527308,  0.6335478284694309,
    };

    constexpr ColorMatrix<3, 3> NonLinearLMSToOKLab {
        0.2104542683093140,  0.7936177747023054, -0.0040720430116193,
        1.9779985324311684, -2.4285922420485799,  0.4505937096174110,
        0.0259040424655478,  0.7827717124575296, -0.8086757549230774,
    };

    auto [x, y, z, alpha] = color.resolved();

    // 1. Convert XYZ into LMS "approximate cone responses".
    auto linearLMS = XYZD65ToLinearLMS.transformedColorComponents(ColorComponents<float, 3> { x, y, z });

    // 2. Apply non-linearity.
    auto nonLinearLMS = linearLMS.map([] (float v) { return std::cbrt(v); });

    // 3. Transform into Lab-coordinates.
    auto [lightness, a, b] = NonLinearLMSToOKLab.transformedColorComponents(nonLinearLMS);

    return makeFromComponentsClampingExceptAlpha<OKLab<float>>(lightness, a, b, alpha);
}

// MARK: OKLCH conversions.

OKLCHA<float> ColorConversion<OKLCHA<float>, OKLab<float>>::convert(const OKLab<float>& color)
{
    return convertToPolarForm<OKLCHA<float>>(color);
}

OKLab<float> ColorConversion<OKLab<float>, OKLCHA<float>>::convert(const OKLCHA<float>& color)
{
    return convertToRectangularForm<OKLab<float>>(color);
}

// MARK: Conversion functions for raw color components with associated color spaces.

ColorComponents<float, 4> convertAndResolveColorComponents(ColorSpace inputColorSpace, ColorComponents<float, 4> inputColorComponents, ColorSpace outputColorSpace)
{
    return callWithColorType<float>(inputColorSpace, [&]<typename InputColorType>() {
        auto inputColor = makeFromComponents<InputColorType>(inputColorComponents);
        return callWithColorType<float>(outputColorSpace, [&]<typename OutputColorType>() {
            return asColorComponents(convertColor<OutputColorType>(inputColor).resolved());
        });
    });
}

ColorComponents<float, 4> convertAndResolveColorComponents(ColorSpace inputColorSpace, ColorComponents<float, 4> inputColorComponents, const DestinationColorSpace& outputColorSpace)
{
#if USE(CG)
    return platformConvertColorComponents(inputColorSpace, inputColorComponents, outputColorSpace);
#else
    return callWithColorType(inputColorComponents, inputColorSpace, [outputColorSpace] (const auto& inputColor) {
        if (outputColorSpace == DestinationColorSpace::SRGB())
            return asColorComponents(convertColor<SRGBA<float>>(inputColor).resolved());
        if (outputColorSpace == DestinationColorSpace::LinearSRGB())
            return asColorComponents(convertColor<LinearSRGBA<float>>(inputColor).resolved());
#if ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3)
        if (outputColorSpace == DestinationColorSpace::DisplayP3())
            return asColorComponents(convertColor<DisplayP3<float>>(inputColor).resolved());
#endif

        ASSERT_NOT_REACHED();
        return asColorComponents(convertColor<SRGBA<float>>(inputColor).resolved());
    });
#endif
}

} // namespace WebCore

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
