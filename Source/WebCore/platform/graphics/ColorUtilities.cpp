/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "Color.h"
#include <wtf/MathExtras.h>

namespace WebCore {

FloatComponents::FloatComponents(const Color& color)
{
    color.getRGBA(components[0], components[1], components[2], components[3]);
}

ColorComponents::ColorComponents(const FloatComponents& floatComponents)
{
    components[0] = clampedColorComponent(floatComponents.components[0]);
    components[1] = clampedColorComponent(floatComponents.components[1]);
    components[2] = clampedColorComponent(floatComponents.components[2]);
    components[3] = clampedColorComponent(floatComponents.components[3]);
}

// These are the standard sRGB <-> linearRGB conversion functions (https://en.wikipedia.org/wiki/SRGB).
float linearToSRGBColorComponent(float c)
{
    if (c < 0.0031308f)
        return 12.92f * c;

    return clampTo<float>(1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f, 0, 1);
}

float sRGBToLinearColorComponent(float c)
{
    if (c <= 0.04045f)
        return c / 12.92f;

    return clampTo<float>(std::pow((c + 0.055f) / 1.055f, 2.4f), 0, 1);
}

FloatComponents sRGBColorToLinearComponents(const Color& color)
{
    float r, g, b, a;
    color.getRGBA(r, g, b, a);
    return {
        sRGBToLinearColorComponent(r),
        sRGBToLinearColorComponent(g),
        sRGBToLinearColorComponent(b),
        a
    };
}

FloatComponents sRGBToLinearComponents(const FloatComponents& sRGBColor)
{
    return {
        sRGBToLinearColorComponent(sRGBColor.components[0]),
        sRGBToLinearColorComponent(sRGBColor.components[1]),
        sRGBToLinearColorComponent(sRGBColor.components[2]),
        sRGBColor.components[3]
    };
}

FloatComponents linearToSRGBComponents(const FloatComponents& linearRGB)
{
    return {
        linearToSRGBColorComponent(linearRGB.components[0]),
        linearToSRGBColorComponent(linearRGB.components[1]),
        linearToSRGBColorComponent(linearRGB.components[2]),
        linearRGB.components[3]
    };
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

float luminance(const FloatComponents& sRGBComponents)
{
    // Values from https://www.w3.org/TR/2008/REC-WCAG20-20081211/#relativeluminancedef
    return 0.2126f * sRGBToLinearColorComponentForLuminance(sRGBComponents.components[0])
        + 0.7152f * sRGBToLinearColorComponentForLuminance(sRGBComponents.components[1])
        + 0.0722f * sRGBToLinearColorComponentForLuminance(sRGBComponents.components[2]);
}

float contrastRatio(const FloatComponents& componentsA, const FloatComponents& componentsB)
{
    // Uses the WCAG 2.0 definition of contrast ratio.
    // https://www.w3.org/TR/WCAG20/#contrast-ratiodef
    float lighterLuminance = luminance(componentsA);
    float darkerLuminance = luminance(componentsB);

    if (lighterLuminance < darkerLuminance)
        std::swap(lighterLuminance, darkerLuminance);

    return (lighterLuminance + 0.05) / (darkerLuminance + 0.05);
}

FloatComponents sRGBToHSL(const FloatComponents& sRGBColor)
{
    // http://en.wikipedia.org/wiki/HSL_color_space.
    float r = sRGBColor.components[0];
    float g = sRGBColor.components[1];
    float b = sRGBColor.components[2];

    float max = std::max(std::max(r, g), b);
    float min = std::min(std::min(r, g), b);
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
        sRGBColor.components[3]
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
FloatComponents HSLToSRGB(const FloatComponents& hslColor)
{
    float hue = hslColor.components[0];
    float saturation = hslColor.components[1];
    float lightness = hslColor.components[2];

    // Convert back to RGB.
    if (!saturation) {
        return {
            lightness,
            lightness,
            lightness,
            hslColor.components[3]
        };
    }
    
    float temp2 = lightness <= 0.5f ? lightness * (1.0f + saturation) : lightness + saturation - lightness * saturation;
    float temp1 = 2.0f * lightness - temp2;
    
    hue *= 6.0f; // calcHue() wants hue in the 0-6 range.
    return {
        calcHue(temp1, temp2, hue + 2.0f),
        calcHue(temp1, temp2, hue),
        calcHue(temp1, temp2, hue - 2.0f),
        hslColor.components[3]
    };
}

ColorMatrix::ColorMatrix()
{
    makeIdentity();
}

ColorMatrix::ColorMatrix(float values[20])
{
    m_matrix[0][0] = values[0];
    m_matrix[0][1] = values[1];
    m_matrix[0][2] = values[2];
    m_matrix[0][3] = values[3];
    m_matrix[0][4] = values[4];

    m_matrix[1][0] = values[5];
    m_matrix[1][1] = values[6];
    m_matrix[1][2] = values[7];
    m_matrix[1][3] = values[8];
    m_matrix[1][4] = values[9];

    m_matrix[2][0] = values[10];
    m_matrix[2][1] = values[11];
    m_matrix[2][2] = values[12];
    m_matrix[2][3] = values[13];
    m_matrix[2][4] = values[14];

    m_matrix[3][0] = values[15];
    m_matrix[3][1] = values[16];
    m_matrix[3][2] = values[17];
    m_matrix[3][3] = values[18];
    m_matrix[3][4] = values[19];
}

void ColorMatrix::makeIdentity()
{
    memset(m_matrix, 0, sizeof(m_matrix));
    m_matrix[0][0] = 1;
    m_matrix[1][1] = 1;
    m_matrix[2][2] = 1;
    m_matrix[3][3] = 1;
}

ColorMatrix ColorMatrix::grayscaleMatrix(float amount)
{
    ColorMatrix matrix;

    float oneMinusAmount = clampTo(1 - amount, 0.0, 1.0);

    // Values from https://www.w3.org/TR/filter-effects-1/#grayscaleEquivalent
    matrix.m_matrix[0][0] = 0.2126f + 0.7874f * oneMinusAmount;
    matrix.m_matrix[0][1] = 0.7152f - 0.7152f * oneMinusAmount;
    matrix.m_matrix[0][2] = 0.0722f - 0.0722f * oneMinusAmount;

    matrix.m_matrix[1][0] = 0.2126f - 0.2126f * oneMinusAmount;
    matrix.m_matrix[1][1] = 0.7152f + 0.2848f * oneMinusAmount;
    matrix.m_matrix[1][2] = 0.0722f - 0.0722f * oneMinusAmount;

    matrix.m_matrix[2][0] = 0.2126f - 0.2126f * oneMinusAmount;
    matrix.m_matrix[2][1] = 0.7152f - 0.7152f * oneMinusAmount;
    matrix.m_matrix[2][2] = 0.0722f + 0.9278f * oneMinusAmount;
    
    return matrix;
}

ColorMatrix ColorMatrix::saturationMatrix(float amount)
{
    ColorMatrix matrix;

    // Values from https://www.w3.org/TR/filter-effects-1/#feColorMatrixElement
    matrix.m_matrix[0][0] = 0.213f + 0.787f * amount;
    matrix.m_matrix[0][1] = 0.715f - 0.715f * amount;
    matrix.m_matrix[0][2] = 0.072f - 0.072f * amount;

    matrix.m_matrix[1][0] = 0.213f - 0.213f * amount;
    matrix.m_matrix[1][1] = 0.715f + 0.285f * amount;
    matrix.m_matrix[1][2] = 0.072f - 0.072f * amount;

    matrix.m_matrix[2][0] = 0.213f - 0.213f * amount;
    matrix.m_matrix[2][1] = 0.715f - 0.715f * amount;
    matrix.m_matrix[2][2] = 0.072f + 0.928f * amount;

    return matrix;
}

ColorMatrix ColorMatrix::hueRotateMatrix(float angleInDegrees)
{
    float cosHue = cos(deg2rad(angleInDegrees));
    float sinHue = sin(deg2rad(angleInDegrees));

    ColorMatrix matrix;

    // Values from https://www.w3.org/TR/filter-effects-1/#feColorMatrixElement
    matrix.m_matrix[0][0] = 0.213f + cosHue * 0.787f - sinHue * 0.213f;
    matrix.m_matrix[0][1] = 0.715f - cosHue * 0.715f - sinHue * 0.715f;
    matrix.m_matrix[0][2] = 0.072f - cosHue * 0.072f + sinHue * 0.928f;

    matrix.m_matrix[1][0] = 0.213f - cosHue * 0.213f + sinHue * 0.143f;
    matrix.m_matrix[1][1] = 0.715f + cosHue * 0.285f + sinHue * 0.140f;
    matrix.m_matrix[1][2] = 0.072f - cosHue * 0.072f - sinHue * 0.283f;

    matrix.m_matrix[2][0] = 0.213f - cosHue * 0.213f - sinHue * 0.787f;
    matrix.m_matrix[2][1] = 0.715f - cosHue * 0.715f + sinHue * 0.715f;
    matrix.m_matrix[2][2] = 0.072f + cosHue * 0.928f + sinHue * 0.072f;

    return matrix;
}

ColorMatrix ColorMatrix::sepiaMatrix(float amount)
{
    ColorMatrix matrix;

    float oneMinusAmount = clampTo(1 - amount, 0.0, 1.0);

    // Values from https://www.w3.org/TR/filter-effects-1/#sepiaEquivalent
    matrix.m_matrix[0][0] = 0.393f + 0.607f * oneMinusAmount;
    matrix.m_matrix[0][1] = 0.769f - 0.769f * oneMinusAmount;
    matrix.m_matrix[0][2] = 0.189f - 0.189f * oneMinusAmount;

    matrix.m_matrix[1][0] = 0.349f - 0.349f * oneMinusAmount;
    matrix.m_matrix[1][1] = 0.686f + 0.314f * oneMinusAmount;
    matrix.m_matrix[1][2] = 0.168f - 0.168f * oneMinusAmount;

    matrix.m_matrix[2][0] = 0.272f - 0.272f * oneMinusAmount;
    matrix.m_matrix[2][1] = 0.534f - 0.534f * oneMinusAmount;
    matrix.m_matrix[2][2] = 0.131f + 0.869f * oneMinusAmount;

    return matrix;
}

void ColorMatrix::transformColorComponents(FloatComponents& colorComonents) const
{
    float red = colorComonents.components[0];
    float green = colorComonents.components[1];
    float blue = colorComonents.components[2];
    float alpha = colorComonents.components[3];

    colorComonents.components[0] = m_matrix[0][0] * red + m_matrix[0][1] * green + m_matrix[0][2] * blue + m_matrix[0][3] * alpha + m_matrix[0][4];
    colorComonents.components[1] = m_matrix[1][0] * red + m_matrix[1][1] * green + m_matrix[1][2] * blue + m_matrix[1][3] * alpha + m_matrix[1][4];
    colorComonents.components[2] = m_matrix[2][0] * red + m_matrix[2][1] * green + m_matrix[2][2] * blue + m_matrix[2][3] * alpha + m_matrix[2][4];
    colorComonents.components[3] = m_matrix[3][0] * red + m_matrix[3][1] * green + m_matrix[3][2] * blue + m_matrix[3][3] * alpha + m_matrix[3][4];
}

} // namespace WebCore
