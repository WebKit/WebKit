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

#pragma once

#include <algorithm>
#include <math.h>

namespace WebCore {

template<typename> struct ColorComponents;

// 0-1 components, result is clamped.
float linearToRGBColorComponent(float);
float rgbToLinearColorComponent(float);

ColorComponents<float> rgbToLinearComponents(const ColorComponents<float>&);
ColorComponents<float> linearToRGBComponents(const ColorComponents<float>&);

ColorComponents<float> p3ToSRGB(const ColorComponents<float>&);
ColorComponents<float> sRGBToP3(const ColorComponents<float>&);

WEBCORE_EXPORT ColorComponents<float> sRGBToHSL(const ColorComponents<float>&);
WEBCORE_EXPORT ColorComponents<float> hslToSRGB(const ColorComponents<float>&);

float lightness(const ColorComponents<float>& sRGBCompontents);
float luminance(const ColorComponents<float>& sRGBCompontents);
float contrastRatio(const ColorComponents<float>& sRGBCompontentsA, const ColorComponents<float>& sRGBCompontentsB);

ColorComponents<float> premultiplied(const ColorComponents<float>& sRGBCompontents);

bool areEssentiallyEqual(const ColorComponents<float>&, const ColorComponents<float>&);

inline uint8_t roundAndClampColorChannel(int value)
{
    return std::clamp(value, 0, 255);
}

inline uint8_t roundAndClampColorChannel(float value)
{
    return std::clamp(std::round(value), 0.f, 255.f);
}

inline uint8_t scaleRoundAndClampColorChannel(float f)
{
    return std::clamp(static_cast<int>(lroundf(255.0f * f)), 0, 255);
}

constexpr uint16_t fastMultiplyBy255(uint16_t value)
{
    return (value << 8) - value;
}

constexpr uint16_t fastDivideBy255(uint16_t value)
{
    // While this is an approximate algorithm for division by 255, it gives perfectly accurate results for 16-bit values.
    // FIXME: Since this gives accurate results for 16-bit values, we should get this optimization into compilers like clang.
    uint16_t approximation = value >> 8;
    uint16_t remainder = value - (approximation * 255) + 1;
    return approximation + (remainder >> 8);
}


} // namespace WebCore
