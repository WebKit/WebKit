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
#include <cmath>
#include <math.h>

namespace WebCore {

template<typename> struct SRGBA;

float lightness(const SRGBA<float>&);
float luminance(const SRGBA<float>&);
float contrastRatio(const SRGBA<float>&, const SRGBA<float>&);

SRGBA<float> premultiplied(const SRGBA<float>&);
SRGBA<float> unpremultiplied(const SRGBA<float>&);

SRGBA<uint8_t> premultipliedFlooring(SRGBA<uint8_t>);
SRGBA<uint8_t> premultipliedCeiling(SRGBA<uint8_t>);
SRGBA<uint8_t> unpremultiplied(SRGBA<uint8_t>);

inline uint8_t convertPrescaledToComponentByte(float f)
{
    return std::clamp(std::lround(f), 0l, 255l);
}

inline uint8_t convertToComponentByte(float f)
{
    return std::clamp(std::lround(f * 255.0f), 0l, 255l);
}

constexpr uint8_t clampToComponentByte(int c)
{
    return static_cast<uint8_t>(std::clamp(c, 0, 0xFF));
}

constexpr uint8_t clampToComponentFloat(float f)
{
    return std::clamp(f, 0.0f, 1.0f);
}

constexpr float convertToComponentFloat(uint8_t byte)
{
    return byte / 255.0f;
}

template<template<typename> typename ColorType> inline ColorType<uint8_t> convertToComponentBytes(const ColorType<float>& color)
{
    auto components = asColorComponents(color);
    return { convertToComponentByte(components[0]), convertToComponentByte(components[1]), convertToComponentByte(components[2]), convertToComponentByte(components[3]) };
}

template<template<typename> typename ColorType> constexpr ColorType<float> convertToComponentFloats(const ColorType<uint8_t>& color)
{
    auto components = asColorComponents(color);
    return { convertToComponentFloat(components[0]), convertToComponentFloat(components[1]), convertToComponentFloat(components[2]), convertToComponentFloat(components[3]) };
}

template<template<typename> typename ColorType> constexpr ColorType<uint8_t> clampToComponentBytes(int r, int g, int b, int a)
{
    return { clampToComponentByte(r), clampToComponentByte(g), clampToComponentByte(b), clampToComponentByte(a) };
}

template<template<typename> typename ColorType> constexpr ColorType<float> clampToComponentFloats(float r, float g, float b, float a)
{
    return { clampToComponentFloat(r), clampToComponentFloat(g), clampToComponentFloat(b), clampToComponentFloat(a) };
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
