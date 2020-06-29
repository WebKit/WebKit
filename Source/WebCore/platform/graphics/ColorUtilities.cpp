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
#include "ColorUtilities.h"

#include "ColorComponents.h"
#include "ColorMatrix.h"
#include "ColorTypes.h"

namespace WebCore {

// These are the standard sRGB <-> linearRGB / standard DisplayP3 <-> LinearDisplayP3 conversion functions (https://en.wikipedia.org/wiki/SRGB).
float linearToRGBColorComponent(float c)
{
    if (c < 0.0031308f)
        return 12.92f * c;

    return clampTo<float>(1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f, 0, 1);
}

float rgbToLinearColorComponent(float c)
{
    if (c <= 0.04045f)
        return c / 12.92f;

    return clampTo<float>(std::pow((c + 0.055f) / 1.055f, 2.4f), 0, 1);
}

LinearSRGBA<float> toLinearSRGBA(const SRGBA<float>& color)
{
    return {
        rgbToLinearColorComponent(color.red),
        rgbToLinearColorComponent(color.green),
        rgbToLinearColorComponent(color.blue),
        color.alpha
    };
}

SRGBA<float> toSRGBA(const LinearSRGBA<float>& color)
{
    return {
        linearToRGBColorComponent(color.red),
        linearToRGBColorComponent(color.green),
        linearToRGBColorComponent(color.blue),
        color.alpha
    };
}

LinearDisplayP3<float> toLinearDisplayP3(const DisplayP3<float>& color)
{
    return {
        rgbToLinearColorComponent(color.red),
        rgbToLinearColorComponent(color.green),
        rgbToLinearColorComponent(color.blue),
        color.alpha
    };
}

DisplayP3<float> toDisplayP3(const LinearDisplayP3<float>& color)
{
    return {
        linearToRGBColorComponent(color.red),
        linearToRGBColorComponent(color.green),
        linearToRGBColorComponent(color.blue),
        color.alpha
    };
}

static LinearSRGBA<float> toLinearSRGBA(const XYZA<float>& color)
{
    // https://en.wikipedia.org/wiki/SRGB
    // http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
    constexpr ColorMatrix<3, 3> xyzToLinearSRGBMatrix {
         3.2404542f, -1.5371385f, -0.4985314f,
        -0.9692660f,  1.8760108f,  0.0415560f,
         0.0556434f, -0.2040259f,  1.0572252f
    };
    return asLinearSRGBA(xyzToLinearSRGBMatrix.transformedColorComponents(asColorComponents(color)));
}

static XYZA<float> toXYZ(const LinearSRGBA<float>& color)
{
    // https://en.wikipedia.org/wiki/SRGB
    // http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
    constexpr ColorMatrix<3, 3> linearSRGBToXYZMatrix {
        0.4124564f,  0.3575761f,  0.1804375f,
        0.2126729f,  0.7151522f,  0.0721750f,
        0.0193339f,  0.1191920f,  0.9503041f
    };
    return asXYZA(linearSRGBToXYZMatrix.transformedColorComponents(asColorComponents(color)));
}

static LinearDisplayP3<float> toLinearDisplayP3(const XYZA<float>& color)
{
    // https://drafts.csswg.org/css-color/#color-conversion-code
    constexpr ColorMatrix<3, 3> xyzToLinearDisplayP3Matrix {
         2.493496911941425f,  -0.9313836179191239f, -0.4027107844507168f,
        -0.8294889695615747f,  1.7626640603183463f,  0.0236246858419436f,
         0.0358458302437845f, -0.0761723892680418f,  0.9568845240076872f
    };
    return asLinearDisplayP3(xyzToLinearDisplayP3Matrix.transformedColorComponents(asColorComponents(color)));
}

static XYZA<float> toXYZ(const LinearDisplayP3<float>& color)
{
    // https://drafts.csswg.org/css-color/#color-conversion-code
    constexpr ColorMatrix<3, 3> linearDisplayP3ToXYZMatrix {
        0.4865709486482162f, 0.2656676931690931f, 0.198217285234363f,
        0.2289745640697488f, 0.6917385218365064f, 0.079286914093745f,
        0.0f,                0.0451133818589026f, 1.043944368900976f
    };
    return asXYZA(linearDisplayP3ToXYZMatrix.transformedColorComponents(asColorComponents(color)));
}

SRGBA<float> toSRGBA(const DisplayP3<float>& color)
{
    return toSRGBA(toLinearSRGBA(toXYZ(toLinearDisplayP3(color))));
}

DisplayP3<float> toDisplayP3(const SRGBA<float>& color)
{
    return toDisplayP3(toLinearDisplayP3(toXYZ(toLinearSRGBA(color))));
}

float lightness(const SRGBA<float>& color)
{
    auto [r, g, b, a] = color;
    auto [min, max] = std::minmax({ r, g, b });
    return 0.5f * (max + min);
}

float luminance(const SRGBA<float>& color)
{
    // NOTE: This is the equivalent of toXYZA(toLinearSRGBA(color)).y
    // FIMXE: If we can generalize ColorMatrix a bit more, it might be nice to write this as:
    //      return toLinearSRGBA(color) * linearSRGBToXYZMatrix.row(1);
    auto [r, g, b, a] = toLinearSRGBA(color);
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

float contrastRatio(const SRGBA<float>& colorA, const SRGBA<float>& colorB)
{
    // Uses the WCAG 2.0 definition of contrast ratio.
    // https://www.w3.org/TR/WCAG20/#contrast-ratiodef
    float lighterLuminance = luminance(colorA);
    float darkerLuminance = luminance(colorB);

    if (lighterLuminance < darkerLuminance)
        std::swap(lighterLuminance, darkerLuminance);

    return (lighterLuminance + 0.05) / (darkerLuminance + 0.05);
}

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

SRGBA<float> toSRGBA(const CMYKA<float>& color)
{
    auto [c, m, y, k, a] = color;
    float colors = 1 - k;
    float r = colors * (1.0f - c);
    float g = colors * (1.0f - m);
    float b = colors * (1.0f - y);
    return { r, g, b, a };
}

SRGBA<float> premultiplied(const SRGBA<float>& color)
{
    auto [r, g, b, a] = color;
    return {
        r * a,
        g * a,
        b * a,
        a
    };
}

} // namespace WebCore
