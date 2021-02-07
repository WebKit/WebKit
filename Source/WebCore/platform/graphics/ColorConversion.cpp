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

#include "ColorComponents.h"
#include "ColorMatrix.h"
#include "ColorTransferFunctions.h"
#include <wtf/MathExtras.h>

namespace WebCore {

// A98RGB Matrices.

// https://drafts.csswg.org/css-color/#color-conversion-code
static constexpr ColorMatrix<3, 3> xyzToLinearA98RGBMatrix {
     2.493496911941425f,  -0.9313836179191239f, -0.4027107844507168f,
    -0.8294889695615747f,  1.7626640603183463f,  0.0236246858419436f,
     0.0358458302437845f, -0.0761723892680418f,  0.9568845240076872f
};

// https://drafts.csswg.org/css-color/#color-conversion-code
static constexpr ColorMatrix<3, 3> linearA98RGBToXYZMatrix {
    0.5766690429101305f,   0.1855582379065463f,   0.1882286462349947f,
    0.29734497525053605f,  0.6273635662554661f,   0.07529145849399788f,
    0.02703136138641234f,  0.07068885253582723f,  0.9913375368376388f
};

// DisplayP3 Matrices.

// https://drafts.csswg.org/css-color/#color-conversion-code
static constexpr ColorMatrix<3, 3> xyzToLinearDisplayP3Matrix {
     2.493496911941425f,  -0.9313836179191239f, -0.4027107844507168f,
    -0.8294889695615747f,  1.7626640603183463f,  0.0236246858419436f,
     0.0358458302437845f, -0.0761723892680418f,  0.9568845240076872f
};

// https://drafts.csswg.org/css-color/#color-conversion-code
static constexpr ColorMatrix<3, 3> linearDisplayP3ToXYZMatrix {
    0.4865709486482162f, 0.2656676931690931f, 0.198217285234363f,
    0.2289745640697488f, 0.6917385218365064f, 0.079286914093745f,
    0.0f,                0.0451133818589026f, 1.043944368900976f
};

// ProPhotoRGB Matrices.

// https://drafts.csswg.org/css-color/#color-conversion-code
static constexpr ColorMatrix<3, 3> xyzToLinearProPhotoRGBMatrix {
     1.3457989731028281f,  -0.25558010007997534f,  -0.05110628506753401f,
    -0.5446224939028347f,   1.5082327413132781f,    0.02053603239147973f,
     0.0f,                  0.0f,                   1.2119675456389454f
};

// https://drafts.csswg.org/css-color/#color-conversion-code
static constexpr ColorMatrix<3, 3> linearProPhotoRGBToXYZMatrix {
    0.7977604896723027f,  0.13518583717574031f,  0.0313493495815248f,
    0.2880711282292934f,  0.7118432178101014f,   0.00008565396060525902f,
    0.0f,                 0.0f,                  0.8251046025104601f
};

// Rec2020 Matrices.

// https://drafts.csswg.org/css-color/#color-conversion-code
static constexpr ColorMatrix<3, 3> xyzToLinearRec2020Matrix {
     1.7166511879712674f,   -0.35567078377639233f, -0.25336628137365974f,
    -0.6666843518324892f,    1.6164812366349395f,   0.01576854581391113f,
     0.017639857445310783f, -0.042770613257808524f, 0.9421031212354738f
};

// https://drafts.csswg.org/css-color/#color-conversion-code
static constexpr ColorMatrix<3, 3> linearRec2020ToXYZMatrix {
    0.6369580483012914f, 0.14461690358620832f,  0.1688809751641721f,
    0.2627002120112671f, 0.6779980715188708f,   0.05930171646986196f,
    0.000000000000000f,  0.028072693049087428f, 1.060985057710791f
};

// sRGB Matrices.

// https://en.wikipedia.org/wiki/SRGB
// http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
static constexpr ColorMatrix<3, 3> xyzToLinearSRGBMatrix {
     3.2404542f, -1.5371385f, -0.4985314f,
    -0.9692660f,  1.8760108f,  0.0415560f,
     0.0556434f, -0.2040259f,  1.0572252f
};

// https://en.wikipedia.org/wiki/SRGB
// http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
static constexpr ColorMatrix<3, 3> linearSRGBToXYZMatrix {
    0.4124564f,  0.3575761f,  0.1804375f,
    0.2126729f,  0.7151522f,  0.0721750f,
    0.0193339f,  0.1191920f,  0.9503041f
};

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

template<typename TransferFunction, typename ColorType> static auto toLinear(const ColorType& color) -> typename ColorType::LinearCounterpart
{
    auto [c1, c2, c3, alpha] = color;
    return { TransferFunction::toLinear(c1), TransferFunction::toLinear(c2), TransferFunction::toLinear(c3), alpha };
}

template<typename TransferFunction, typename ColorType> static auto toGammaEncoded(const ColorType& color) -> typename ColorType::GammaEncodedCounterpart
{
    auto [c1, c2, c3, alpha] = color;
    return { TransferFunction::toGammaEncoded(c1), TransferFunction::toGammaEncoded(c2), TransferFunction::toGammaEncoded(c3), alpha };
}

// A98RGB <-> LinearA98RGB conversions.

LinearA98RGB<float> ColorConversion<LinearA98RGB<float>, A98RGB<float>>::convert(const A98RGB<float>& color)
{
    return toLinear<A98RGBTransferFunction<float, TransferFunctionMode::Clamped>>(color);
}

A98RGB<float> ColorConversion<A98RGB<float>, LinearA98RGB<float>>::convert(const LinearA98RGB<float>& color)
{
    return toGammaEncoded<A98RGBTransferFunction<float, TransferFunctionMode::Clamped>>(color);
}

// DisplayP3 <-> LinearDisplayP3 conversions.

LinearDisplayP3<float> ColorConversion<LinearDisplayP3<float>, DisplayP3<float>>::convert(const DisplayP3<float>& color)
{
    return toLinear<SRGBTransferFunction<float, TransferFunctionMode::Clamped>>(color);
}

DisplayP3<float> ColorConversion<DisplayP3<float>, LinearDisplayP3<float>>::convert(const LinearDisplayP3<float>& color)
{
    return toGammaEncoded<SRGBTransferFunction<float, TransferFunctionMode::Clamped>>(color);
}

// ExtendedSRGBA <-> LinearExtendedSRGBA conversions.

LinearExtendedSRGBA<float> ColorConversion<LinearExtendedSRGBA<float>, ExtendedSRGBA<float>>::convert(const ExtendedSRGBA<float>& color)
{
    return toLinear<SRGBTransferFunction<float, TransferFunctionMode::Unclamped>>(color);
}

ExtendedSRGBA<float> ColorConversion<ExtendedSRGBA<float>, LinearExtendedSRGBA<float>>::convert(const LinearExtendedSRGBA<float>& color)
{
    return toGammaEncoded<SRGBTransferFunction<float, TransferFunctionMode::Unclamped>>(color);
}

// ProPhotoRGB <-> LinearProPhotoRGB conversions.

LinearProPhotoRGB<float> ColorConversion<LinearProPhotoRGB<float>, ProPhotoRGB<float>>::convert(const ProPhotoRGB<float>& color)
{
    return toLinear<ProPhotoRGBTransferFunction<float, TransferFunctionMode::Clamped>>(color);
}

ProPhotoRGB<float> ColorConversion<ProPhotoRGB<float>, LinearProPhotoRGB<float>>::convert(const LinearProPhotoRGB<float>& color)
{
    return toGammaEncoded<ProPhotoRGBTransferFunction<float, TransferFunctionMode::Clamped>>(color);
}

// Rec2020 <-> LinearRec2020 conversions.

LinearRec2020<float> ColorConversion<LinearRec2020<float>, Rec2020<float>>::convert(const Rec2020<float>& color)
{
    return toLinear<Rec2020TransferFunction<float, TransferFunctionMode::Clamped>>(color);
}

Rec2020<float> ColorConversion<Rec2020<float>, LinearRec2020<float>>::convert(const LinearRec2020<float>& color)
{
    return toGammaEncoded<Rec2020TransferFunction<float, TransferFunctionMode::Clamped>>(color);
}

// SRGBA <-> LinearSRGBA conversions.

LinearSRGBA<float> ColorConversion<LinearSRGBA<float>, SRGBA<float>>::convert(const SRGBA<float>& color)
{
    return toLinear<SRGBTransferFunction<float, TransferFunctionMode::Clamped>>(color);
}

SRGBA<float> ColorConversion<SRGBA<float>, LinearSRGBA<float>>::convert(const LinearSRGBA<float>& color)
{
    return toGammaEncoded<SRGBTransferFunction<float, TransferFunctionMode::Clamped>>(color);
}

// MARK: Matrix conversions (to and from XYZ for all linear color types).

// - LinearA98RGB matrix conversions.

LinearA98RGB<float> ColorConversion<LinearA98RGB<float>, LinearA98RGB<float>::ReferenceXYZ>::convert(const LinearA98RGB<float>::ReferenceXYZ& color)
{
    return makeFromComponentsClampingExceptAlpha<LinearA98RGB<float>>(xyzToLinearA98RGBMatrix.transformedColorComponents(asColorComponents(color)));
}

LinearA98RGB<float>::ReferenceXYZ ColorConversion<LinearA98RGB<float>::ReferenceXYZ, LinearA98RGB<float>>::convert(const LinearA98RGB<float>& color)
{
    return makeFromComponentsClampingExceptAlpha<LinearA98RGB<float>::ReferenceXYZ>(linearA98RGBToXYZMatrix.transformedColorComponents(asColorComponents(color)));
}

// - LinearDisplayP3 matrix conversions.

LinearDisplayP3<float> ColorConversion<LinearDisplayP3<float>, LinearDisplayP3<float>::ReferenceXYZ>::convert(const LinearDisplayP3<float>::ReferenceXYZ& color)
{
    return makeFromComponentsClampingExceptAlpha<LinearDisplayP3<float>>(xyzToLinearDisplayP3Matrix.transformedColorComponents(asColorComponents(color)));
}

LinearDisplayP3<float>::ReferenceXYZ ColorConversion<LinearDisplayP3<float>::ReferenceXYZ, LinearDisplayP3<float>>::convert(const LinearDisplayP3<float>& color)
{
    return makeFromComponentsClampingExceptAlpha<LinearDisplayP3<float>::ReferenceXYZ>(linearDisplayP3ToXYZMatrix.transformedColorComponents(asColorComponents(color)));
}

// - LinearExtendedSRGBA matrix conversions.

LinearExtendedSRGBA<float> ColorConversion<LinearExtendedSRGBA<float>, LinearExtendedSRGBA<float>::ReferenceXYZ>::convert(const LinearExtendedSRGBA<float>::ReferenceXYZ& color)
{
    return makeFromComponentsClampingExceptAlpha<LinearExtendedSRGBA<float>>(xyzToLinearSRGBMatrix.transformedColorComponents(asColorComponents(color)));
}

LinearExtendedSRGBA<float>::ReferenceXYZ ColorConversion<LinearExtendedSRGBA<float>::ReferenceXYZ, LinearExtendedSRGBA<float>>::convert(const LinearExtendedSRGBA<float>& color)
{
    return makeFromComponentsClampingExceptAlpha<LinearExtendedSRGBA<float>::ReferenceXYZ>(linearSRGBToXYZMatrix.transformedColorComponents(asColorComponents(color)));
}

// - LinearProPhotoRGB matrix conversions.

LinearProPhotoRGB<float> ColorConversion<LinearProPhotoRGB<float>, LinearProPhotoRGB<float>::ReferenceXYZ>::convert(const LinearProPhotoRGB<float>::ReferenceXYZ& color)
{
    return makeFromComponentsClampingExceptAlpha<LinearProPhotoRGB<float>>(xyzToLinearProPhotoRGBMatrix.transformedColorComponents(asColorComponents(color)));
}

LinearProPhotoRGB<float>::ReferenceXYZ ColorConversion<LinearProPhotoRGB<float>::ReferenceXYZ, LinearProPhotoRGB<float>>::convert(const LinearProPhotoRGB<float>& color)
{
    return makeFromComponentsClampingExceptAlpha<LinearProPhotoRGB<float>::ReferenceXYZ>(linearProPhotoRGBToXYZMatrix.transformedColorComponents(asColorComponents(color)));
}

// - LinearRec2020 matrix conversions.

LinearRec2020<float> ColorConversion<LinearRec2020<float>, LinearRec2020<float>::ReferenceXYZ>::convert(const LinearRec2020<float>::ReferenceXYZ& color)
{
    return makeFromComponentsClampingExceptAlpha<LinearRec2020<float>>(xyzToLinearRec2020Matrix.transformedColorComponents(asColorComponents(color)));
}

LinearRec2020<float>::ReferenceXYZ ColorConversion<LinearRec2020<float>::ReferenceXYZ, LinearRec2020<float>>::convert(const LinearRec2020<float>& color)
{
    return makeFromComponentsClampingExceptAlpha<LinearRec2020<float>::ReferenceXYZ>(linearRec2020ToXYZMatrix.transformedColorComponents(asColorComponents(color)));
}

// - LinearSRGBA matrix conversions.

LinearSRGBA<float> ColorConversion<LinearSRGBA<float>, LinearSRGBA<float>::ReferenceXYZ>::convert(const LinearSRGBA<float>::ReferenceXYZ& color)
{
    return makeFromComponentsClampingExceptAlpha<LinearSRGBA<float>>(xyzToLinearSRGBMatrix.transformedColorComponents(asColorComponents(color)));
}

LinearSRGBA<float>::ReferenceXYZ ColorConversion<LinearSRGBA<float>::ReferenceXYZ, LinearSRGBA<float>>::convert(const LinearSRGBA<float>& color)
{
    return makeFromComponentsClampingExceptAlpha<LinearSRGBA<float>::ReferenceXYZ>(linearSRGBToXYZMatrix.transformedColorComponents(asColorComponents(color)));
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

Lab<float>::ReferenceXYZ ColorConversion<Lab<float>::ReferenceXYZ, Lab<float>>::convert(const Lab<float>& color)
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

Lab<float> ColorConversion<Lab<float>, Lab<float>::ReferenceXYZ>::convert(const Lab<float>::ReferenceXYZ& color)
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

A98RGB<float>::ReferenceXYZ ColorConversion<A98RGB<float>::ReferenceXYZ, A98RGB<float>>::convert(const A98RGB<float>& color)
{
    return convertColor<A98RGB<float>::ReferenceXYZ>(convertColor<LinearA98RGB<float>>(color));
}

A98RGB<float> ColorConversion<A98RGB<float>, A98RGB<float>::ReferenceXYZ>::convert(const A98RGB<float>::ReferenceXYZ& color)
{
    return convertColor<A98RGB<float>>(convertColor<LinearA98RGB<float>>(color));
}

// - DisplayP3 combination functions.

DisplayP3<float>::ReferenceXYZ ColorConversion<DisplayP3<float>::ReferenceXYZ, DisplayP3<float>>::convert(const DisplayP3<float>& color)
{
    return convertColor<DisplayP3<float>::ReferenceXYZ>(convertColor<LinearDisplayP3<float>>(color));
}

DisplayP3<float> ColorConversion<DisplayP3<float>, DisplayP3<float>::ReferenceXYZ>::convert(const DisplayP3<float>::ReferenceXYZ& color)
{
    return convertColor<DisplayP3<float>>(convertColor<LinearDisplayP3<float>>(color));
}

// - ExtendedSRGB combination functions.

ExtendedSRGBA<float>::ReferenceXYZ ColorConversion<ExtendedSRGBA<float>::ReferenceXYZ, ExtendedSRGBA<float>>::convert(const ExtendedSRGBA<float>& color)
{
    return convertColor<ExtendedSRGBA<float>::ReferenceXYZ>(convertColor<LinearExtendedSRGBA<float>>(color));
}

ExtendedSRGBA<float> ColorConversion<ExtendedSRGBA<float>, ExtendedSRGBA<float>::ReferenceXYZ>::convert(const ExtendedSRGBA<float>::ReferenceXYZ& color)
{
    return convertColor<ExtendedSRGBA<float>>(convertColor<LinearExtendedSRGBA<float>>(color));
}

// - HSLA combination functions.

HSLA<float>::ReferenceXYZ ColorConversion<HSLA<float>::ReferenceXYZ, HSLA<float>>::convert(const HSLA<float>& color)
{
    return convertColor<HSLA<float>::ReferenceXYZ>(convertColor<SRGBA<float>>(color));
}

HSLA<float> ColorConversion<HSLA<float>, HSLA<float>::ReferenceXYZ>::convert(const HSLA<float>::ReferenceXYZ& color)
{
    return convertColor<HSLA<float>>(convertColor<SRGBA<float>>(color));
}

// - HWBA combination functions.

HWBA<float>::ReferenceXYZ ColorConversion<HWBA<float>::ReferenceXYZ, HWBA<float>>::convert(const HWBA<float>& color)
{
    return convertColor<HWBA<float>::ReferenceXYZ>(convertColor<SRGBA<float>>(color));
}

HWBA<float> ColorConversion<HWBA<float>, HWBA<float>::ReferenceXYZ>::convert(const HWBA<float>::ReferenceXYZ& color)
{
    return convertColor<HWBA<float>>(convertColor<SRGBA<float>>(color));
}

// - LCHA combination functions.

LCHA<float>::ReferenceXYZ ColorConversion<LCHA<float>::ReferenceXYZ, LCHA<float>>::convert(const LCHA<float>& color)
{
    return convertColor<LCHA<float>::ReferenceXYZ>(convertColor<Lab<float>>(color));
}

LCHA<float> ColorConversion<LCHA<float>, LCHA<float>::ReferenceXYZ>::convert(const LCHA<float>::ReferenceXYZ& color)
{
    return convertColor<LCHA<float>>(convertColor<Lab<float>>(color));
}

// - ProPhotoRGB combination functions.

ProPhotoRGB<float>::ReferenceXYZ ColorConversion<ProPhotoRGB<float>::ReferenceXYZ, ProPhotoRGB<float>>::convert(const ProPhotoRGB<float>& color)
{
    return convertColor<ProPhotoRGB<float>::ReferenceXYZ>(convertColor<LinearProPhotoRGB<float>>(color));
}

ProPhotoRGB<float> ColorConversion<ProPhotoRGB<float>, ProPhotoRGB<float>::ReferenceXYZ>::convert(const ProPhotoRGB<float>::ReferenceXYZ& color)
{
    return convertColor<ProPhotoRGB<float>>(convertColor<LinearProPhotoRGB<float>>(color));
}

// - Rec2020 combination functions.

Rec2020<float>::ReferenceXYZ ColorConversion<Rec2020<float>::ReferenceXYZ, Rec2020<float>>::convert(const Rec2020<float>& color)
{
    return convertColor<Rec2020<float>::ReferenceXYZ>(convertColor<LinearRec2020<float>>(color));
}

Rec2020<float> ColorConversion<Rec2020<float>, Rec2020<float>::ReferenceXYZ>::convert(const Rec2020<float>::ReferenceXYZ& color)
{
    return convertColor<Rec2020<float>>(convertColor<LinearRec2020<float>>(color));
}

// - SRGB combination functions.

SRGBA<float>::ReferenceXYZ ColorConversion<SRGBA<float>::ReferenceXYZ, SRGBA<float>>::convert(const SRGBA<float>& color)
{
    return convertColor<SRGBA<float>::ReferenceXYZ>(convertColor<LinearSRGBA<float>>(color));
}

SRGBA<float> ColorConversion<SRGBA<float>, SRGBA<float>::ReferenceXYZ>::convert(const SRGBA<float>::ReferenceXYZ& color)
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
        return std::lround(value * 255.0f);
    }));
}

} // namespace WebCore
