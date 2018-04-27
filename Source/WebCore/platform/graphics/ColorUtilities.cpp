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
    if (c < 0.0031308)
        return 12.92 * c;

    return clampTo<float>(1.055 * powf(c, 1.0 / 2.4) - 0.055, 0, 1);
}

float sRGBToLinearColorComponent(float c)
{
    if (c <= 0.04045)
        return c / 12.92;

    return clampTo<float>(powf((c + 0.055) / 1.055, 2.4), 0, 1);
}

Color linearToSRGBColor(const Color& color)
{
    float r, g, b, a;
    color.getRGBA(r, g, b, a);
    r = linearToSRGBColorComponent(r);
    g = linearToSRGBColorComponent(g);
    b = linearToSRGBColorComponent(b);

    return Color(r, g, b, a);
}

Color sRGBToLinearColor(const Color& color)
{
    float r, g, b, a;
    color.getRGBA(r, g, b, a);
    r = sRGBToLinearColorComponent(r);
    g = sRGBToLinearColorComponent(g);
    b = sRGBToLinearColorComponent(b);

    return Color(r, g, b, a);
}


ColorMatrix::ColorMatrix()
{
    makeIdentity();
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
