/*
 * Copyright (C) 2024-2026 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleAppleInvertLightnessFunction.h"

#include "CSSAppleInvertLightnessFunction.h"
#include "ColorMatrix.h"

namespace WebCore {
namespace Style {

// FIXME: This hueRotate code exists to allow AppleInvertLightness to perform hue rotation
// on color values outside of the non-extended SRGB value range (0-1) to maintain the behavior of colors
// prior to clamping being enforced. It should likely just use the existing hueRotateColorMatrix(amount)
// in ColorMatrix.h
static ColorComponents<float, 4> hueRotate(const ColorComponents<float, 4>& color, float amount)
{
    auto [r, g, b, alpha] = color;

    auto [min, max] = std::minmax({ r, g, b });
    float chroma = max - min;

    float lightness = 0.5f * (max + min);
    float saturation;
    if (!chroma)
        saturation = 0;
    else if (lightness <= 0.5f)
        saturation = (chroma / (max + min));
    else
        saturation = (chroma / (2.0f - (max + min)));

    if (!saturation)
        return { lightness, lightness, lightness, alpha };

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

    // Perform rotation.
    hue = std::fmod(hue + amount, 1.0f);

    float temp2 = lightness <= 0.5f ? lightness * (1.0f + saturation) : lightness + saturation - lightness * saturation;
    float temp1 = 2.0f * lightness - temp2;
    
    hue *= 6.0f; // calcHue() wants hue in the 0-6 range.

    // Hue is in the range 0-6, other args in 0-1.
    auto calcHue = [](float temp1, float temp2, float hueVal) {
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
    };

    return {
        calcHue(temp1, temp2, hue + 2.0f),
        calcHue(temp1, temp2, hue),
        calcHue(temp1, temp2, hue - 2.0f),
        alpha
    };
}

bool AppleInvertLightness::transformColor(SRGBA<float>& color) const
{
    auto hueRotatedSRGBAComponents = hueRotate(asColorComponents(color.resolved()), 0.5f);
    
    // Apply the matrix. See rdar://problem/41146650 for how this matrix was derived.
    constexpr ColorMatrix<5, 3> toDarkModeMatrix {
       -0.770f,  0.059f, -0.089f, 0.0f, 1.0f,
        0.030f, -0.741f, -0.089f, 0.0f, 1.0f,
        0.030f,  0.059f, -0.890f, 0.0f, 1.0f
    };
    color = makeFromComponentsClamping<SRGBA<float>>(toDarkModeMatrix.transformedColorComponents(hueRotatedSRGBAComponents));
    return true;
}

bool AppleInvertLightness::inverseTransformColor(SRGBA<float>& color) const
{
    // Apply the matrix.
    constexpr ColorMatrix<5, 3> toLightModeMatrix {
        -1.300f, -0.097f,  0.147f, 0.0f, 1.25f,
        -0.049f, -1.347f,  0.146f, 0.0f, 1.25f,
        -0.049f, -0.097f, -1.104f, 0.0f, 1.25f
    };
    auto convertedToLightModeComponents = toLightModeMatrix.transformedColorComponents(asColorComponents(color.resolved()));

    auto hueRotatedSRGBAComponents = hueRotate(convertedToLightModeComponents, 0.5f);

    color = makeFromComponentsClamping<SRGBA<float>>(hueRotatedSRGBAComponents);
    return true;
}

} // namespace Style
} // namespace WebCore
