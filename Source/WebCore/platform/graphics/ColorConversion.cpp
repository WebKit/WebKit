/*
 * Copyright (C) 2017, 2020 Apple Inc. All rights reserved.
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
#include <wtf/MathExtras.h>

namespace WebCore {

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

// Gamma conversions.

float SRGBTransferFunction::fromLinearClamping(float c)
{
    if (c < 0.0031308f)
        return std::max<float>(12.92f * c, 0);

    return clampTo<float>(1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f, 0, 1);
}

float SRGBTransferFunction::toLinearClamping(float c)
{
    if (c <= 0.04045f)
        return std::max<float>(c / 12.92f, 0);

    return clampTo<float>(std::pow((c + 0.055f) / 1.055f, 2.4f), 0, 1);
}

float SRGBTransferFunction::fromLinearNonClamping(float c)
{
    float sign = std::signbit(c) ? -1.0f : 1.0f;
    c = std::abs(c);

    if (c < 0.0031308f)
        return 12.92f * c * sign;

    return (1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f) * sign;
}

float SRGBTransferFunction::toLinearNonClamping(float c)
{
    float sign = std::signbit(c) ? -1.0f : 1.0f;
    c = std::abs(c);

    if (c <= 0.04045f)
        return c / 12.92f * sign;

    return std::pow((c + 0.055f) / 1.055f, 2.4f) * sign;
}

float A98RGBTransferFunction::fromLinearClamping(float c)
{
    return clampTo<float>(fromLinearNonClamping(c), 0, 1);
}

float A98RGBTransferFunction::toLinearClamping(float c)
{
    return clampTo<float>(toLinearNonClamping(c), 0, 1);
}

float A98RGBTransferFunction::fromLinearNonClamping(float c)
{
    float sign = std::signbit(c) ? -1.0f : 1.0f;
    return std::pow(std::abs(c), 256.0f / 563.0f) * sign;
}

float A98RGBTransferFunction::toLinearNonClamping(float c)
{
    float sign = std::signbit(c) ? -1.0f : 1.0f;
    return std::pow(std::abs(c), 563.0f / 256.0f) * sign;
}

template<typename TransferFunction, typename T> static auto toLinearClamping(const T& color) -> typename T::LinearCounterpart
{
    auto [c1, c2, c3, alpha] = color;
    return { TransferFunction::toLinearClamping(c1), TransferFunction::toLinearClamping(c2), TransferFunction::toLinearClamping(c3), alpha };
}

template<typename TransferFunction, typename T> static auto fromLinearClamping(const T& color) -> typename T::GammaEncodedCounterpart
{
    auto [c1, c2, c3, alpha] = color;
    return { TransferFunction::fromLinearClamping(c1), TransferFunction::fromLinearClamping(c2), TransferFunction::fromLinearClamping(c3), alpha };
}

template<typename TransferFunction, typename T> static auto toLinearNonClamping(const T& color) -> typename T::LinearCounterpart
{
    auto [c1, c2, c3, alpha] = color;
    return { TransferFunction::toLinearNonClamping(c1), TransferFunction::toLinearNonClamping(c2), TransferFunction::toLinearNonClamping(c3), alpha };
}

template<typename TransferFunction, typename T> static auto fromLinearNonClamping(const T& color) -> typename T::GammaEncodedCounterpart
{
    auto [c1, c2, c3, alpha] = color;
    return { TransferFunction::fromLinearNonClamping(c1), TransferFunction::fromLinearNonClamping(c2), TransferFunction::fromLinearNonClamping(c3), alpha };
}

LinearSRGBA<float> toLinearSRGBA(const SRGBA<float>& color)
{
    return toLinearClamping<SRGBTransferFunction>(color);
}

LinearExtendedSRGBA<float> toLinearExtendedSRGBA(const ExtendedSRGBA<float>& color)
{
    return toLinearNonClamping<SRGBTransferFunction>(color);
}

SRGBA<float> toSRGBA(const LinearSRGBA<float>& color)
{
    return fromLinearClamping<SRGBTransferFunction>(color);
}

ExtendedSRGBA<float> toExtendedSRGBA(const LinearExtendedSRGBA<float>& color)
{
    return fromLinearNonClamping<SRGBTransferFunction>(color);
}

LinearDisplayP3<float> toLinearDisplayP3(const DisplayP3<float>& color)
{
    return toLinearClamping<SRGBTransferFunction>(color);
}

DisplayP3<float> toDisplayP3(const LinearDisplayP3<float>& color)
{
    return fromLinearClamping<SRGBTransferFunction>(color);
}

LinearA98RGB<float> toLinearA98RGB(const A98RGB<float>& color)
{
    return toLinearClamping<A98RGBTransferFunction>(color);
}

A98RGB<float> toA98RGB(const LinearA98RGB<float>& color)
{
    return fromLinearClamping<A98RGBTransferFunction>(color);
}

// Matrix conversions (to and from XYZ for all linear color types).

// - LinearSRGBA matrix conversions.

LinearSRGBA<float> toLinearSRGBA(const XYZA<float>& color)
{
    return makeFromComponentsClampingExceptAlpha<LinearSRGBA<float>>(xyzToLinearSRGBMatrix.transformedColorComponents(asColorComponents(color)));
}

XYZA<float> toXYZA(const LinearSRGBA<float>& color)
{
    return makeFromComponentsClampingExceptAlpha<XYZA<float>>(linearSRGBToXYZMatrix.transformedColorComponents(asColorComponents(color)));
}

// - LinearExtendedSRGBA matrix conversions.

LinearExtendedSRGBA<float> toLinearExtendedSRGBA(const XYZA<float>& color)
{
    return makeFromComponentsClampingExceptAlpha<LinearExtendedSRGBA<float>>(xyzToLinearSRGBMatrix.transformedColorComponents(asColorComponents(color)));
}

XYZA<float> toXYZA(const LinearExtendedSRGBA<float>& color)
{
    return makeFromComponentsClampingExceptAlpha<XYZA<float>>(linearSRGBToXYZMatrix.transformedColorComponents(asColorComponents(color)));
}

// - LinearDisplayP3 matrix conversions.

LinearDisplayP3<float> toLinearDisplayP3(const XYZA<float>& color)
{
    return makeFromComponentsClampingExceptAlpha<LinearDisplayP3<float>>(xyzToLinearDisplayP3Matrix.transformedColorComponents(asColorComponents(color)));
}

XYZA<float> toXYZA(const LinearDisplayP3<float>& color)
{
    return makeFromComponentsClampingExceptAlpha<XYZA<float>>(linearDisplayP3ToXYZMatrix.transformedColorComponents(asColorComponents(color)));
}

// - LinearA98RGB matrix conversions.

LinearA98RGB<float> toLinearA98RGB(const XYZA<float>& color)
{
    return makeFromComponentsClampingExceptAlpha<LinearA98RGB<float>>(xyzToLinearA98RGBMatrix.transformedColorComponents(asColorComponents(color)));
}

XYZA<float> toXYZA(const LinearA98RGB<float>& color)
{
    return makeFromComponentsClampingExceptAlpha<XYZA<float>>(linearA98RGBToXYZMatrix.transformedColorComponents(asColorComponents(color)));
}

// Chromatic Adaptation conversions.

static XYZA<float> convertFromD50WhitePointToD65WhitePoint(const XYZA<float>& color)
{
    return makeFromComponentsClampingExceptAlpha<XYZA<float>>(D50ToD65Matrix.transformedColorComponents(asColorComponents(color)));
}

static XYZA<float> convertFromD65WhitePointToD50WhitePoint(const XYZA<float>& color)
{
    return makeFromComponentsClampingExceptAlpha<XYZA<float>>(D65ToD50Matrix.transformedColorComponents(asColorComponents(color)));
}

// Lab conversions.

static constexpr float LABe = 216.0f / 24389.0f;
static constexpr float LABk = 24389.0f / 27.0f;
static constexpr float D50WhiteValues[] = { 0.96422f, 1.0f, 0.82521f };

XYZA<float> toXYZA(const Lab<float>& color)
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

    XYZA<float> result { x, y, z, color.alpha };

    // We expect XYZA colors to be using the D65 white point, unlike Lab, which uses a
    // D50 white point, so as a final step, use the Bradford transform to convert the
    // resulting XYZA color to a D65 white point.
    return convertFromD50WhitePointToD65WhitePoint(result);
}

Lab<float> toLab(const XYZA<float>& color)
{
    // We expect XYZA colors to be using the D65 white point, unlike Lab, which uses a
    // D50 white point, so as a first step, use the Bradford transform to convert the
    // incoming XYZA color to D50 white point.
    auto colorWithD50WhitePoint = convertFromD65WhitePointToD50WhitePoint(color);

    float x = colorWithD50WhitePoint.x / D50WhiteValues[0];
    float y = colorWithD50WhitePoint.y / D50WhiteValues[1];
    float z = colorWithD50WhitePoint.z / D50WhiteValues[2];

    auto fTransform = [](float value) {
        return value > LABe ? std::cbrt(value) : (LABk * value + 16.0f) / 116.0f;
    };

    float f0 = fTransform(x);
    float f1 = fTransform(y);
    float f2 = fTransform(z);

    float lightness = (116.0f * f1) - 16.0f;
    float a = 500.0f * (f0 - f1);
    float b = 200.0f * (f1 - f2);

    return { lightness, a, b, colorWithD50WhitePoint.alpha };
}


// LCH conversions.

LCHA<float> toLCHA(const Lab<float>& color)
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

Lab<float> toLab(const LCHA<float>& color)
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

// HSL conversions.

HSLA<float> toHSLA(const SRGBA<float>& color)
{
    // http://en.wikipedia.org/wiki/HSL_color_space.
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

    float lightness = 0.5f * (max + min);
    float saturation;
    if (!chroma)
        saturation = 0;
    else if (lightness <= 0.5f)
        saturation = (chroma / (max + min));
    else
        saturation = (chroma / (2.0f - (max + min)));

    return {
        hue,
        saturation,
        lightness,
        alpha
    };
}

// Hue is in the range 0-6, other args in 0-1.
static float calcHue(float temp1, float temp2, float hueVal)
{
    if (hueVal < 0.0f)
        hueVal += 6.0f;
    else if (hueVal >= 6.0f)
        hueVal -= 6.0f;
    if (hueVal < 1.0f)
        return temp1 + (temp2 - temp1) * hueVal;
    if (hueVal < 3.0f)
        return temp2;
    if (hueVal < 4.0f)
        return temp1 + (temp2 - temp1) * (4.0f - hueVal);
    return temp1;
}

// Explanation of this algorithm can be found in the CSS Color 4 Module
// specification at https://drafts.csswg.org/css-color-4/#hsl-to-rgb with
// further explanation available at http://en.wikipedia.org/wiki/HSL_color_space
SRGBA<float> toSRGBA(const HSLA<float>& color)
{
    auto [hue, saturation, lightness, alpha] = color;

    // Convert back to RGB.
    if (!saturation) {
        return {
            lightness,
            lightness,
            lightness,
            alpha
        };
    }
    
    float temp2 = lightness <= 0.5f ? lightness * (1.0f + saturation) : lightness + saturation - lightness * saturation;
    float temp1 = 2.0f * lightness - temp2;
    
    hue *= 6.0f; // calcHue() wants hue in the 0-6 range.
    return {
        calcHue(temp1, temp2, hue + 2.0f),
        calcHue(temp1, temp2, hue),
        calcHue(temp1, temp2, hue - 2.0f),
        alpha
    };
}


// Combination conversions (constructed from more basic conversions above).

// - SRGB combination functions.

XYZA<float> toXYZA(const SRGBA<float>& color)
{
    return toXYZA(toLinearSRGBA(color));
}

SRGBA<float> toSRGBA(const XYZA<float>& color)
{
    return toSRGBA(toLinearSRGBA(color));
}

// - ExtendedSRGB combination functions.

XYZA<float> toXYZA(const ExtendedSRGBA<float>& color)
{
    return toXYZA(toLinearExtendedSRGBA(color));
}

ExtendedSRGBA<float> toExtendedSRGBA(const XYZA<float>& color)
{
    return toExtendedSRGBA(toLinearExtendedSRGBA(color));
}

// - DisplayP3 combination functions.

XYZA<float> toXYZA(const DisplayP3<float>& color)
{
    return toXYZA(toLinearDisplayP3(color));
}

DisplayP3<float> toDisplayP3(const XYZA<float>& color)
{
    return toDisplayP3(toLinearDisplayP3(color));
}

// - A98RGB combination functions.

XYZA<float> toXYZA(const A98RGB<float>& color)
{
    return toXYZA(toLinearA98RGB(color));
}

A98RGB<float> toA98RGB(const XYZA<float>& color)
{
    return toA98RGB(toLinearA98RGB(color));
}

// - LCHA combination functions.

XYZA<float> toXYZA(const LCHA<float>& color)
{
    return toXYZA(toLab(color));
}

LCHA<float> toLCHA(const XYZA<float>& color)
{
    return toLCHA(toLab(color));
}

// - HSLA combination functions.

XYZA<float> toXYZA(const HSLA<float>& color)
{
    return toXYZA(toSRGBA(color));
}

HSLA<float> toHSLA(const XYZA<float>& color)
{
    return toHSLA(toSRGBA(color));
}

} // namespace WebCore
