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
#include <algorithm>

namespace WebCore {

bool areEssentiallyEqual(const ColorComponents<float>& a, const ColorComponents<float>& b)
{
    return WTF::areEssentiallyEqual(a[0], b[0])
        && WTF::areEssentiallyEqual(a[1], b[1])
        && WTF::areEssentiallyEqual(a[2], b[2])
        && WTF::areEssentiallyEqual(a[3], b[3]);
}

// These are the standard sRGB <-> linearRGB conversion functions (https://en.wikipedia.org/wiki/SRGB).
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

ColorComponents<float> rgbToLinearComponents(const ColorComponents<float>& RGBColor)
{
    return {
        rgbToLinearColorComponent(RGBColor[0]),
        rgbToLinearColorComponent(RGBColor[1]),
        rgbToLinearColorComponent(RGBColor[2]),
        RGBColor[3]
    };
}

ColorComponents<float> linearToRGBComponents(const ColorComponents<float>& linearRGB)
{
    return {
        linearToRGBColorComponent(linearRGB[0]),
        linearToRGBColorComponent(linearRGB[1]),
        linearToRGBColorComponent(linearRGB[2]),
        linearRGB[3]
    };
}

static ColorComponents<float> xyzToLinearSRGB(const ColorComponents<float>& XYZComponents)
{
    // https://en.wikipedia.org/wiki/SRGB
    // http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
    constexpr ColorMatrix<3, 3> xyzToLinearSRGBMatrix {
         3.2404542f, -1.5371385f, -0.4985314f,
        -0.9692660f,  1.8760108f,  0.0415560f,
         0.0556434f, -0.2040259f,  1.0572252f
    };
    return xyzToLinearSRGBMatrix.transformedColorComponents(XYZComponents);
}

static ColorComponents<float> linearSRGBToXYZ(const ColorComponents<float>& XYZComponents)
{
    // https://en.wikipedia.org/wiki/SRGB
    // http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
    constexpr ColorMatrix<3, 3> linearSRGBToXYZMatrix {
        0.4124564f,  0.3575761f,  0.1804375f,
        0.2126729f,  0.7151522f,  0.0721750f,
        0.0193339f,  0.1191920f,  0.9503041f
    };
    return linearSRGBToXYZMatrix.transformedColorComponents(XYZComponents);
}

static ColorComponents<float> XYZToLinearP3(const ColorComponents<float>& XYZComponents)
{
    // https://drafts.csswg.org/css-color/#color-conversion-code
    constexpr ColorMatrix<3, 3> xyzToLinearSRGBMatrix {
         2.493496911941425f,  -0.9313836179191239f, -0.4027107844507168f,
        -0.8294889695615747f,  1.7626640603183463f,  0.0236246858419436f,
         0.0358458302437845f, -0.0761723892680418f,  0.9568845240076872f
    };
    return xyzToLinearSRGBMatrix.transformedColorComponents(XYZComponents);
}

static ColorComponents<float> linearP3ToXYZ(const ColorComponents<float>& XYZComponents)
{
    // https://drafts.csswg.org/css-color/#color-conversion-code
    constexpr ColorMatrix<3, 3> linearP3ToXYZMatrix {
        0.4865709486482162f, 0.2656676931690931f, 0.198217285234363f,
        0.2289745640697488f, 0.6917385218365064f, 0.079286914093745f,
        0.0f,                0.0451133818589026f, 1.043944368900976f
    };
    return linearP3ToXYZMatrix.transformedColorComponents(XYZComponents);
}

ColorComponents<float> p3ToSRGB(const ColorComponents<float>& p3)
{
    auto linearP3 = rgbToLinearComponents(p3);
    auto xyz = linearP3ToXYZ(linearP3);
    auto linearSRGB = xyzToLinearSRGB(xyz);
    return linearToRGBComponents(linearSRGB);
}

ColorComponents<float> sRGBToP3(const ColorComponents<float>& sRGB)
{
    auto linearSRGB = rgbToLinearComponents(sRGB);
    auto xyz = linearSRGBToXYZ(linearSRGB);
    auto linearP3 = XYZToLinearP3(xyz);
    return linearToRGBComponents(linearP3);
}

float lightness(const ColorComponents<float>& sRGBCompontents)
{
    auto [r, g, b, a] = sRGBCompontents;

    auto [min, max] = std::minmax({ r, g, b });

    return 0.5f * (max + min);
}

// This is similar to sRGBToLinearColorComponent but for some reason
// https://www.w3.org/TR/2008/REC-WCAG20-20081211/#relativeluminancedef
// doesn't use the standard sRGB -> linearRGB threshold of 0.04045.
static float sRGBToLinearColorComponentForLuminance(float c)
{
    if (c <= 0.03928f)
        return c / 12.92f;

    return clampTo<float>(std::pow((c + 0.055f) / 1.055f, 2.4f), 0, 1);
}

float luminance(const ColorComponents<float>& sRGBComponents)
{
    // Values from https://www.w3.org/TR/2008/REC-WCAG20-20081211/#relativeluminancedef
    return 0.2126f * sRGBToLinearColorComponentForLuminance(sRGBComponents[0])
        + 0.7152f * sRGBToLinearColorComponentForLuminance(sRGBComponents[1])
        + 0.0722f * sRGBToLinearColorComponentForLuminance(sRGBComponents[2]);
}

float contrastRatio(const ColorComponents<float>& componentsA, const ColorComponents<float>& componentsB)
{
    // Uses the WCAG 2.0 definition of contrast ratio.
    // https://www.w3.org/TR/WCAG20/#contrast-ratiodef
    float lighterLuminance = luminance(componentsA);
    float darkerLuminance = luminance(componentsB);

    if (lighterLuminance < darkerLuminance)
        std::swap(lighterLuminance, darkerLuminance);

    return (lighterLuminance + 0.05) / (darkerLuminance + 0.05);
}

ColorComponents<float> sRGBToHSL(const ColorComponents<float>& sRGBCompontents)
{
    // http://en.wikipedia.org/wiki/HSL_color_space.
    auto [r, g, b, alpha] = sRGBCompontents;

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
ColorComponents<float> hslToSRGB(const ColorComponents<float>& hslColor)
{
    auto [hue, saturation, lightness, alpha] = hslColor;

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

ColorComponents<float> premultiplied(const ColorComponents<float>& sRGBComponents)
{
    auto [r, g, b, a] = sRGBComponents;
    return {
        r * a,
        g * a,
        b * a,
        a
    };
}

} // namespace WebCore
