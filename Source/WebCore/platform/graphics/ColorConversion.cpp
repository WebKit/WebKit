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

#include <wtf/MathExtras.h>

namespace WebCore {

// Chromatic Adaptation Matrices.

// http://www.brucelindbloom.com/index.html?Eqn_ChromAdapt.html
static constexpr ColorMatrix<3, 3> D50ToD65Matrix {
     0.9555766f, -0.0230393f, 0.0631636f,
    -0.0282895f,  1.0099416f, 0.0210077f,
     0.0122982f, -0.0204830f, 1.3299098f
};

// http://www.brucelindbloom.com/index.html?Eqn_ChromAdapt.html
static constexpr ColorMatrix<3, 3> D65ToD50Matrix {
     1.0478112f, 0.0228866f, -0.0501270f,
     0.0295424f, 0.9904844f, -0.0170491f,
    -0.0092345f, 0.0150436f,  0.7521316f
};

// MARK: Chromatic Adaptation conversions.

XYZA<float, WhitePoint::D65> ColorConversion<XYZA<float, WhitePoint::D65>, XYZA<float, WhitePoint::D50>>::convert(const XYZA<float, WhitePoint::D50>& color)
{
    return makeFromComponentsClampingExceptAlpha<XYZA<float, WhitePoint::D65>>(D50ToD65Matrix.transformedColorComponents(asColorComponents(color)));
}

XYZA<float, WhitePoint::D50> ColorConversion<XYZA<float, WhitePoint::D50>, XYZA<float, WhitePoint::D65>>::convert(const XYZA<float, WhitePoint::D65>& color)
{
    return makeFromComponentsClampingExceptAlpha<XYZA<float, WhitePoint::D50>>(D65ToD50Matrix.transformedColorComponents(asColorComponents(color)));
}

// MARK: Gamma conversions.

template<typename ColorType> static auto toLinear(const ColorType& color) -> typename ColorType::LinearCounterpart
{
    auto [c1, c2, c3, alpha] = color;
    return { ColorType::TransferFunction::toLinear(c1), ColorType::TransferFunction::toLinear(c2), ColorType::TransferFunction::toLinear(c3), alpha };
}

template<typename ColorType> static auto toGammaEncoded(const ColorType& color) -> typename ColorType::GammaEncodedCounterpart
{
    auto [c1, c2, c3, alpha] = color;
    return { ColorType::TransferFunction::toGammaEncoded(c1), ColorType::TransferFunction::toGammaEncoded(c2), ColorType::TransferFunction::toGammaEncoded(c3), alpha };
}

// A98RGB <-> LinearA98RGB conversions.

LinearA98RGB<float> ColorConversion<LinearA98RGB<float>, A98RGB<float>>::convert(const A98RGB<float>& color)
{
    return toLinear(color);
}

A98RGB<float> ColorConversion<A98RGB<float>, LinearA98RGB<float>>::convert(const LinearA98RGB<float>& color)
{
    return toGammaEncoded(color);
}

// DisplayP3 <-> LinearDisplayP3 conversions.

LinearDisplayP3<float> ColorConversion<LinearDisplayP3<float>, DisplayP3<float>>::convert(const DisplayP3<float>& color)
{
    return toLinear(color);
}

DisplayP3<float> ColorConversion<DisplayP3<float>, LinearDisplayP3<float>>::convert(const LinearDisplayP3<float>& color)
{
    return toGammaEncoded(color);
}

// ExtendedSRGBA <-> LinearExtendedSRGBA conversions.

LinearExtendedSRGBA<float> ColorConversion<LinearExtendedSRGBA<float>, ExtendedSRGBA<float>>::convert(const ExtendedSRGBA<float>& color)
{
    return toLinear(color);
}

ExtendedSRGBA<float> ColorConversion<ExtendedSRGBA<float>, LinearExtendedSRGBA<float>>::convert(const LinearExtendedSRGBA<float>& color)
{
    return toGammaEncoded(color);
}

// ProPhotoRGB <-> LinearProPhotoRGB conversions.

LinearProPhotoRGB<float> ColorConversion<LinearProPhotoRGB<float>, ProPhotoRGB<float>>::convert(const ProPhotoRGB<float>& color)
{
    return toLinear(color);
}

ProPhotoRGB<float> ColorConversion<ProPhotoRGB<float>, LinearProPhotoRGB<float>>::convert(const LinearProPhotoRGB<float>& color)
{
    return toGammaEncoded(color);
}

// Rec2020 <-> LinearRec2020 conversions.

LinearRec2020<float> ColorConversion<LinearRec2020<float>, Rec2020<float>>::convert(const Rec2020<float>& color)
{
    return toLinear(color);
}

Rec2020<float> ColorConversion<Rec2020<float>, LinearRec2020<float>>::convert(const LinearRec2020<float>& color)
{
    return toGammaEncoded(color);
}

// SRGBA <-> LinearSRGBA conversions.

LinearSRGBA<float> ColorConversion<LinearSRGBA<float>, SRGBA<float>>::convert(const SRGBA<float>& color)
{
    return toLinear(color);
}

SRGBA<float> ColorConversion<SRGBA<float>, LinearSRGBA<float>>::convert(const LinearSRGBA<float>& color)
{
    return toGammaEncoded(color);
}

// MARK: Matrix conversions (to and from XYZ for all linear color types).

template<typename ColorType> static auto xyzToLinear(const XYZFor<ColorType>& color) -> ColorType
{
    return makeFromComponentsClampingExceptAlpha<ColorType>(ColorType::xyzToLinear.transformedColorComponents(asColorComponents(color)));
}

template<typename ColorType> static auto linearToXYZ(const ColorType& color) -> XYZFor<ColorType>
{
    return makeFromComponentsClampingExceptAlpha<XYZFor<ColorType>>(ColorType::linearToXYZ.transformedColorComponents(asColorComponents(color)));
}

// - LinearA98RGB matrix conversions.

LinearA98RGB<float> ColorConversion<LinearA98RGB<float>, XYZFor<LinearA98RGB<float>>>::convert(const XYZFor<LinearA98RGB<float>>& color)
{
    return xyzToLinear<LinearA98RGB<float>>(color);
}

XYZFor<LinearA98RGB<float>> ColorConversion<XYZFor<LinearA98RGB<float>>, LinearA98RGB<float>>::convert(const LinearA98RGB<float>& color)
{
    return linearToXYZ<LinearA98RGB<float>>(color);
}

// - LinearDisplayP3 matrix conversions.

LinearDisplayP3<float> ColorConversion<LinearDisplayP3<float>, XYZFor<LinearDisplayP3<float>>>::convert(const XYZFor<LinearDisplayP3<float>>& color)
{
    return xyzToLinear<LinearDisplayP3<float>>(color);
}

XYZFor<LinearDisplayP3<float>> ColorConversion<XYZFor<LinearDisplayP3<float>>, LinearDisplayP3<float>>::convert(const LinearDisplayP3<float>& color)
{
    return linearToXYZ<LinearDisplayP3<float>>(color);
}

// - LinearExtendedSRGBA matrix conversions.

LinearExtendedSRGBA<float> ColorConversion<LinearExtendedSRGBA<float>, XYZFor<LinearExtendedSRGBA<float>>>::convert(const XYZFor<LinearExtendedSRGBA<float>>& color)
{
    return xyzToLinear<LinearExtendedSRGBA<float>>(color);
}

XYZFor<LinearExtendedSRGBA<float>> ColorConversion<XYZFor<LinearExtendedSRGBA<float>>, LinearExtendedSRGBA<float>>::convert(const LinearExtendedSRGBA<float>& color)
{
    return linearToXYZ<LinearExtendedSRGBA<float>>(color);
}

// - LinearProPhotoRGB matrix conversions.

LinearProPhotoRGB<float> ColorConversion<LinearProPhotoRGB<float>, XYZFor<LinearProPhotoRGB<float>>>::convert(const XYZFor<LinearProPhotoRGB<float>>& color)
{
    return xyzToLinear<LinearProPhotoRGB<float>>(color);
}

XYZFor<LinearProPhotoRGB<float>> ColorConversion<XYZFor<LinearProPhotoRGB<float>>, LinearProPhotoRGB<float>>::convert(const LinearProPhotoRGB<float>& color)
{
    return linearToXYZ<LinearProPhotoRGB<float>>(color);
}

// - LinearRec2020 matrix conversions.

LinearRec2020<float> ColorConversion<LinearRec2020<float>, XYZFor<LinearRec2020<float>>>::convert(const XYZFor<LinearRec2020<float>>& color)
{
    return xyzToLinear<LinearRec2020<float>>(color);
}

XYZFor<LinearRec2020<float>> ColorConversion<XYZFor<LinearRec2020<float>>, LinearRec2020<float>>::convert(const LinearRec2020<float>& color)
{
    return linearToXYZ<LinearRec2020<float>>(color);
}

// - LinearSRGBA matrix conversions.

LinearSRGBA<float> ColorConversion<LinearSRGBA<float>, XYZFor<LinearSRGBA<float>>>::convert(const XYZFor<LinearSRGBA<float>>& color)
{
    return xyzToLinear<LinearSRGBA<float>>(color);
}

XYZFor<LinearSRGBA<float>> ColorConversion<XYZFor<LinearSRGBA<float>>, LinearSRGBA<float>>::convert(const LinearSRGBA<float>& color)
{
    return linearToXYZ<LinearSRGBA<float>>(color);
}

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

    hue /= 360.0f;

    return { hue, min, max, chroma };
}

HSLA<float> ColorConversion<HSLA<float>, SRGBA<float>>::convert(const SRGBA<float>& color)
{
    // https://drafts.csswg.org/css-color-4/#hsl-to-rgb
    auto [r, g, b, alpha] = color;
    auto [hue, min, max, chroma] = calculateHSLHue(color);

    float lightness = 0.5f * (max + min);
    float saturation;
    if (!chroma)
        saturation = 0;
    else if (lightness <= 0.5f)
        saturation = (chroma / (max + min));
    else
        saturation = (chroma / (2.0f - (max + min)));

    return { hue, saturation, lightness, alpha };
}

SRGBA<float> ColorConversion<SRGBA<float>, HSLA<float>>::convert(const HSLA<float>& color)
{
    // https://drafts.csswg.org/css-color-4/#hsl-to-rgb
    auto [hue, saturation, lightness, alpha] = color;

    if (!saturation)
        return { lightness, lightness, lightness, alpha };
    
    float temp2 = lightness <= 0.5f ? lightness * (1.0f + saturation) : lightness + saturation - lightness * saturation;
    float temp1 = 2.0f * lightness - temp2;
    
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

    // hueToRGB() wants hue in the 0-6 range.
    hue *= 6.0f;

    auto hueForRed = hue + 2.0f;
    auto hueForGreen = hue;
    auto hueForBlue = hue - 2.0f;
    if (hueForRed > 6.0f)
        hueForRed -= 6.0f;
    else if (hueForBlue < 0.0f)
        hueForBlue += 6.0f;

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
    auto whiteness = min;
    auto blackness = 1.0f - max;
    
    return { hue, whiteness, blackness, color.alpha };
}

SRGBA<float> ColorConversion<SRGBA<float>, HWBA<float>>::convert(const HWBA<float>& color)
{
    // https://drafts.csswg.org/css-color-4/#hwb-to-rgb
    auto [hue, whiteness, blackness, alpha] = color;

    if (whiteness + blackness == 1.0f)
        return { whiteness, whiteness, whiteness, alpha };

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

    auto applyWhitenessBlackness = [&](float component) {
        return (component * (1.0f - color.whiteness - color.blackness)) + color.whiteness;
    };

    // hueToRGB() wants hue in the 0-6 range.
    hue *= 6.0f;

    auto hueForRed = hue + 2.0f;
    auto hueForGreen = hue;
    auto hueForBlue = hue - 2.0f;
    if (hueForRed > 6.0f)
        hueForRed -= 6.0f;
    else if (hueForBlue < 0.0f)
        hueForBlue += 6.0f;

    return {
        applyWhitenessBlackness(hueToRGB(hueForRed)),
        applyWhitenessBlackness(hueToRGB(hueForGreen)),
        applyWhitenessBlackness(hueToRGB(hueForBlue)),
        alpha
    };
}

// MARK: Lab conversions.

static constexpr float LABe = 216.0f / 24389.0f;
static constexpr float LABk = 24389.0f / 27.0f;
static constexpr float D50WhiteValues[] = { 0.96422f, 1.0f, 0.82521f };

XYZFor<Lab<float>> ColorConversion<XYZFor<Lab<float>>, Lab<float>>::convert(const Lab<float>& color)
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

Lab<float> ColorConversion<Lab<float>, XYZFor<Lab<float>>>::convert(const XYZFor<Lab<float>>& color)
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

// MARK: Combination conversions (constructed from more basic conversions above).

// - A98RGB combination functions.

XYZFor<A98RGB<float>> ColorConversion<XYZFor<A98RGB<float>>, A98RGB<float>>::convert(const A98RGB<float>& color)
{
    return convertColor<XYZFor<A98RGB<float>>>(convertColor<LinearA98RGB<float>>(color));
}

A98RGB<float> ColorConversion<A98RGB<float>, XYZFor<A98RGB<float>>>::convert(const XYZFor<A98RGB<float>>& color)
{
    return convertColor<A98RGB<float>>(convertColor<LinearA98RGB<float>>(color));
}

// - DisplayP3 combination functions.

XYZFor<DisplayP3<float>> ColorConversion<XYZFor<DisplayP3<float>>, DisplayP3<float>>::convert(const DisplayP3<float>& color)
{
    return convertColor<XYZFor<DisplayP3<float>>>(convertColor<LinearDisplayP3<float>>(color));
}

DisplayP3<float> ColorConversion<DisplayP3<float>, XYZFor<DisplayP3<float>>>::convert(const XYZFor<DisplayP3<float>>& color)
{
    return convertColor<DisplayP3<float>>(convertColor<LinearDisplayP3<float>>(color));
}

// - ExtendedSRGB combination functions.

XYZFor<ExtendedSRGBA<float>> ColorConversion<XYZFor<ExtendedSRGBA<float>>, ExtendedSRGBA<float>>::convert(const ExtendedSRGBA<float>& color)
{
    return convertColor<XYZFor<ExtendedSRGBA<float>>>(convertColor<LinearExtendedSRGBA<float>>(color));
}

ExtendedSRGBA<float> ColorConversion<ExtendedSRGBA<float>, XYZFor<ExtendedSRGBA<float>>>::convert(const XYZFor<ExtendedSRGBA<float>>& color)
{
    return convertColor<ExtendedSRGBA<float>>(convertColor<LinearExtendedSRGBA<float>>(color));
}

// - HSLA combination functions.

XYZFor<HSLA<float>> ColorConversion<XYZFor<HSLA<float>>, HSLA<float>>::convert(const HSLA<float>& color)
{
    return convertColor<XYZFor<HSLA<float>>>(convertColor<SRGBA<float>>(color));
}

HSLA<float> ColorConversion<HSLA<float>, XYZFor<HSLA<float>>>::convert(const XYZFor<HSLA<float>>& color)
{
    return convertColor<HSLA<float>>(convertColor<SRGBA<float>>(color));
}

// - HWBA combination functions.

XYZFor<HWBA<float>> ColorConversion<XYZFor<HWBA<float>>, HWBA<float>>::convert(const HWBA<float>& color)
{
    return convertColor<XYZFor<HWBA<float>>>(convertColor<SRGBA<float>>(color));
}

HWBA<float> ColorConversion<HWBA<float>, XYZFor<HWBA<float>>>::convert(const XYZFor<HWBA<float>>& color)
{
    return convertColor<HWBA<float>>(convertColor<SRGBA<float>>(color));
}

// - LCHA combination functions.

XYZFor<LCHA<float>> ColorConversion<XYZFor<LCHA<float>>, LCHA<float>>::convert(const LCHA<float>& color)
{
    return convertColor<XYZFor<LCHA<float>>>(convertColor<Lab<float>>(color));
}

LCHA<float> ColorConversion<LCHA<float>, XYZFor<LCHA<float>>>::convert(const XYZFor<LCHA<float>>& color)
{
    return convertColor<LCHA<float>>(convertColor<Lab<float>>(color));
}

// - ProPhotoRGB combination functions.

XYZFor<ProPhotoRGB<float>> ColorConversion<XYZFor<ProPhotoRGB<float>>, ProPhotoRGB<float>>::convert(const ProPhotoRGB<float>& color)
{
    return convertColor<XYZFor<ProPhotoRGB<float>>>(convertColor<LinearProPhotoRGB<float>>(color));
}

ProPhotoRGB<float> ColorConversion<ProPhotoRGB<float>, XYZFor<ProPhotoRGB<float>>>::convert(const XYZFor<ProPhotoRGB<float>>& color)
{
    return convertColor<ProPhotoRGB<float>>(convertColor<LinearProPhotoRGB<float>>(color));
}

// - Rec2020 combination functions.

XYZFor<Rec2020<float>> ColorConversion<XYZFor<Rec2020<float>>, Rec2020<float>>::convert(const Rec2020<float>& color)
{
    return convertColor<XYZFor<Rec2020<float>>>(convertColor<LinearRec2020<float>>(color));
}

Rec2020<float> ColorConversion<Rec2020<float>, XYZFor<Rec2020<float>>>::convert(const XYZFor<Rec2020<float>>& color)
{
    return convertColor<Rec2020<float>>(convertColor<LinearRec2020<float>>(color));
}

// - SRGB combination functions.

XYZFor<SRGBA<float>> ColorConversion<XYZFor<SRGBA<float>>, SRGBA<float>>::convert(const SRGBA<float>& color)
{
    return convertColor<XYZFor<SRGBA<float>>>(convertColor<LinearSRGBA<float>>(color));
}

SRGBA<float> ColorConversion<SRGBA<float>, XYZFor<SRGBA<float>>>::convert(const XYZFor<SRGBA<float>>& color)
{
    return convertColor<SRGBA<float>>(convertColor<LinearSRGBA<float>>(color));
}

// MARK: SRGBA<uint8_t> component type conversions.

SRGBA<float> ColorConversion<SRGBA<float>, SRGBA<uint8_t>>::convert(const SRGBA<uint8_t>& color)
{
    return makeFromComponents<SRGBA<float>>(asColorComponents(color).map([](uint8_t value) -> float {
        return value / 255.0f;
    }));
}

SRGBA<uint8_t> ColorConversion<SRGBA<uint8_t>, SRGBA<float>>::convert(const SRGBA<float>& color)
{
    return makeFromComponents<SRGBA<uint8_t>>(asColorComponents(color).map([](float value) -> uint8_t {
        return std::clamp(std::lround(value * 255.0f), 0l, 255l);
    }));
}

} // namespace WebCore
