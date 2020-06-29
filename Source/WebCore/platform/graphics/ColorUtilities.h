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

template<typename> struct CMYKA;
template<typename> struct DisplayP3;
template<typename> struct HSLA;
template<typename> struct LinearDisplayP3;
template<typename> struct LinearSRGBA;
template<typename> struct SRGBA;

// 0-1 components, result is clamped.
float linearToRGBColorComponent(float);
float rgbToLinearColorComponent(float);

LinearSRGBA<float> toLinearSRGBA(const SRGBA<float>&);
SRGBA<float> toSRGBA(const LinearSRGBA<float>&);

LinearDisplayP3<float> toLinearDisplayP3(const DisplayP3<float>&);
DisplayP3<float> toDisplayP3(const LinearDisplayP3<float>&);

SRGBA<float> toSRGBA(const DisplayP3<float>&);
DisplayP3<float> toDisplayP3(const SRGBA<float>&);

WEBCORE_EXPORT HSLA<float> toHSLA(const SRGBA<float>&);
WEBCORE_EXPORT SRGBA<float> toSRGBA(const HSLA<float>&);

SRGBA<float> toSRGBA(const CMYKA<float>&);


float lightness(const SRGBA<float>&);
float luminance(const SRGBA<float>&);
float contrastRatio(const SRGBA<float>&, const SRGBA<float>&);

SRGBA<float> premultiplied(const SRGBA<float>&);

inline uint8_t convertPrescaledToComponentByte(float f)
{
    return std::clamp(std::lround(f), 0l, 255l);
}

inline uint8_t convertToComponentByte(float f)
{
    return std::clamp(std::lround(f * 255.0f), 0l, 255l);
}

constexpr float convertToComponentFloat(uint8_t byte)
{
    return byte / 255.0f;
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
