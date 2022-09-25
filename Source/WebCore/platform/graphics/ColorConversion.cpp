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

namespace WebCore {

// MARK: Lab-Like to LCH-Like conversion utilities.

template<typename LCHLike, typename LabLike>
LCHLike convertToPolarForm(const LabLike& color)
{
    // https://drafts.csswg.org/css-color/#lab-to-lch
    auto [lightness, a, b, alpha] = color.resolved();

    float hue = rad2deg(atan2(b, a));
    float chroma = std::hypot(a, b);

    return { lightness, chroma, hue >= 0.0f ? hue : hue + 360.0f, alpha };
}

template<typename LabLike, typename LCHLike>
LabLike convertToRectangularForm(const LCHLike& color)
{
    // https://drafts.csswg.org/css-color/#lch-to-lab
    auto [lightness, chroma, hue, alpha] = color.resolved();

    float hueAngleRadians = deg2rad(hue);
    float a = chroma * std::cos(hueAngleRadians);
    float b = chroma * std::sin(hueAngleRadians);

    return { lightness, a, b, alpha };
}

// MARK: HSL conversions.

struct HSLHueCalculationResult {
    float hue;
    float min;
    float max;
    float chroma;
};

static HSLHueCalculationResult calculateHSLHue(float r, float g, float b)
{
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
    auto [r, g, b, alpha] = color.resolved();
    auto [hue, min, max, chroma] = calculateHSLHue(r, g, b);

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
    auto [hue, saturation, lightness, alpha] = color.resolved();

    if (!saturation) {
        auto grey = lightness / 100.0f;
        return { grey, grey, grey, alpha };
    }

    // hueToRGB() wants hue in the 0-6 range.
    auto scaledHue = (normalizeHue(hue) / 360.0f) * 6.0f;
    auto scaledLightness = lightness / 100.0f;
    auto scaledSaturation = saturation / 100.0f;

    auto hueForRed = scaledHue + 2.0f;
    auto hueForGreen = scaledHue;
    auto hueForBlue = scaledHue - 2.0f;
    if (hueForRed > 6.0f)
        hueForRed -= 6.0f;
    else if (hueForBlue < 0.0f)
        hueForBlue += 6.0f;

    float temp2 = scaledLightness <= 0.5f ? scaledLightness * (1.0f + scaledSaturation) : scaledLightness + scaledSaturation - scaledLightness * scaledSaturation;
    float temp1 = 2.0f * scaledLightness - temp2;
    
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
    auto [r, g, b, alpha] = color.resolved();
    auto [hue, min, max, chroma] = calculateHSLHue(r, g, b);
    auto whiteness = min * 100.0f;
    auto blackness = (1.0f - max) * 100.0f;
    
    return { hue, whiteness, blackness, alpha };
}

SRGBA<float> ColorConversion<SRGBA<float>, HWBA<float>>::convert(const HWBA<float>& color)
{
    // https://drafts.csswg.org/css-color-4/#hwb-to-rgb
    auto [hue, whiteness, blackness, alpha] = color.resolved();

    if (whiteness + blackness == 100.0f) {
        auto grey = whiteness / 100.0f;
        return { grey, grey, grey, alpha };
    }

    // hueToRGB() wants hue in the 0-6 range.
    auto scaledHue = (normalizeHue(hue) / 360.0f) * 6.0f;

    auto hueForRed = scaledHue + 2.0f;
    auto hueForGreen = scaledHue;
    auto hueForBlue = scaledHue - 2.0f;
    if (hueForRed > 6.0f)
        hueForRed -= 6.0f;
    else if (hueForBlue < 0.0f)
        hueForBlue += 6.0f;

    auto scaledWhiteness = whiteness / 100.0f;
    auto scaledBlackness = blackness / 100.0f;

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
        applyWhitenessBlackness(hueToRGB(hueForRed), scaledWhiteness, scaledBlackness),
        applyWhitenessBlackness(hueToRGB(hueForGreen), scaledWhiteness, scaledBlackness),
        applyWhitenessBlackness(hueToRGB(hueForBlue), scaledWhiteness, scaledBlackness),
        alpha
    };
}

// MARK: Lab conversions.

static constexpr float LABe = 216.0f / 24389.0f;
static constexpr float LABk = 24389.0f / 27.0f;
static constexpr float D50WhiteValues[] = { 0.96422f, 1.0f, 0.82521f };

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

    float x = D50WhiteValues[0] * computeXAndZ(f0);
    float y = D50WhiteValues[1] * computeY(lightness);
    float z = D50WhiteValues[2] * computeXAndZ(f2);

    return { x, y, z, alpha };
}

Lab<float> ColorConversion<Lab<float>, XYZA<float, WhitePoint::D50>>::convert(const XYZA<float, WhitePoint::D50>& color)
{
    auto [x, y, z, alpha] = color.resolved();

    float adjustedX = x / D50WhiteValues[0];
    float adjustedY = y / D50WhiteValues[1];
    float adjustedZ = z / D50WhiteValues[2];

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

    static constexpr ColorMatrix<3, 3> LinearLMSToXYZD65 {
        1.2268798733741557f,  -0.5578149965554813f,  0.28139105017721583f,
       -0.04057576262431372f,  1.1122868293970594f, -0.07171106666151701f,
       -0.07637294974672142f, -0.4214933239627914f,  1.5869240244272418f
    };

    static constexpr ColorMatrix<3, 3> OKLabToNonLinearLMS {
        0.99999999845051981432f,  0.39633779217376785678f,   0.21580375806075880339f,
        1.0000000088817607767f,  -0.1055613423236563494f,   -0.063854174771705903402f,
        1.0000000546724109177f,  -0.089484182094965759684f, -1.2914855378640917399f
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

    static constexpr ColorMatrix<3, 3> XYZD65ToLinearLMS {
        0.8190224432164319f,   0.3619062562801221f, -0.12887378261216414f,
        0.0329836671980271f,   0.9292868468965546f,  0.03614466816999844f,
        0.048177199566046255f, 0.26423952494422764f, 0.6335478258136937f
    };

    static constexpr ColorMatrix<3, 3> NonLinearLMSToOKLab {
        0.2104542553f,  0.7936177850f, -0.0040720468f,
        1.9779984951f, -2.4285922050f,  0.4505937099f,
        0.0259040371f,  0.7827717662f, -0.8086757660f
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
    return callWithColorType(inputColorComponents, inputColorSpace, [outputColorSpace] (const auto& inputColor) {
        switch (outputColorSpace) {
        case ColorSpace::A98RGB:
            return asColorComponents(convertColor<A98RGB<float>>(inputColor).resolved());
        case ColorSpace::DisplayP3:
            return asColorComponents(convertColor<DisplayP3<float>>(inputColor).resolved());
        case ColorSpace::ExtendedA98RGB:
            return asColorComponents(convertColor<ExtendedA98RGB<float>>(inputColor).resolved());
        case ColorSpace::ExtendedDisplayP3:
            return asColorComponents(convertColor<ExtendedDisplayP3<float>>(inputColor).resolved());
        case ColorSpace::ExtendedLinearSRGB:
            return asColorComponents(convertColor<ExtendedLinearSRGBA<float>>(inputColor).resolved());
        case ColorSpace::ExtendedProPhotoRGB:
            return asColorComponents(convertColor<ExtendedProPhotoRGB<float>>(inputColor).resolved());
        case ColorSpace::ExtendedRec2020:
            return asColorComponents(convertColor<ExtendedRec2020<float>>(inputColor).resolved());
        case ColorSpace::ExtendedSRGB:
            return asColorComponents(convertColor<ExtendedSRGBA<float>>(inputColor).resolved());
        case ColorSpace::HSL:
            return asColorComponents(convertColor<HSLA<float>>(inputColor).resolved());
        case ColorSpace::HWB:
            return asColorComponents(convertColor<HWBA<float>>(inputColor).resolved());
        case ColorSpace::LCH:
            return asColorComponents(convertColor<LCHA<float>>(inputColor).resolved());
        case ColorSpace::Lab:
            return asColorComponents(convertColor<Lab<float>>(inputColor).resolved());
        case ColorSpace::LinearSRGB:
            return asColorComponents(convertColor<LinearSRGBA<float>>(inputColor).resolved());
        case ColorSpace::OKLCH:
            return asColorComponents(convertColor<OKLCHA<float>>(inputColor).resolved());
        case ColorSpace::OKLab:
            return asColorComponents(convertColor<OKLab<float>>(inputColor).resolved());
        case ColorSpace::ProPhotoRGB:
            return asColorComponents(convertColor<ProPhotoRGB<float>>(inputColor).resolved());
        case ColorSpace::Rec2020:
            return asColorComponents(convertColor<Rec2020<float>>(inputColor).resolved());
        case ColorSpace::SRGB:
            return asColorComponents(convertColor<SRGBA<float>>(inputColor).resolved());
        case ColorSpace::XYZ_D50:
            return asColorComponents(convertColor<XYZA<float, WhitePoint::D50>>(inputColor).resolved());
        case ColorSpace::XYZ_D65:
            return asColorComponents(convertColor<XYZA<float, WhitePoint::D65>>(inputColor).resolved());
        }

        ASSERT_NOT_REACHED();
        return asColorComponents(convertColor<SRGBA<float>>(inputColor).resolved());
    });
}

ColorComponents<float, 4> convertAndResolveColorComponents(ColorSpace inputColorSpace, ColorComponents<float, 4> inputColorComponents, const DestinationColorSpace& outputColorSpace)
{
#if USE(CG)
    return platformConvertColorComponents(inputColorSpace, inputColorComponents, outputColorSpace);
#else
    return callWithColorType(inputColorComponents, inputColorSpace, [outputColorSpace] (const auto& inputColor) {
        switch (outputColorSpace.platformColorSpace()) {
        case PlatformColorSpace::Name::SRGB:
            return asColorComponents(convertColor<SRGBA<float>>(inputColor).resolved());
#if ENABLE(DESTINATION_COLOR_SPACE_LINEAR_SRGB)
        case PlatformColorSpace::Name::LinearSRGB:
            return asColorComponents(convertColor<LinearSRGBA<float>>(inputColor).resolved());
#endif
#if ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3)
        case PlatformColorSpace::Name::DisplayP3:
            return asColorComponents(convertColor<DisplayP3<float>>(inputColor).resolved());
#endif
        }

        ASSERT_NOT_REACHED();
        return asColorComponents(convertColor<SRGBA<float>>(inputColor).resolved());
    });
#endif
}

} // namespace WebCore
