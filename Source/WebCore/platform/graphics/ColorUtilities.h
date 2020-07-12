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

uint8_t convertPrescaledToComponentByte(float);

uint8_t convertToComponentByte(float);
constexpr float convertToComponentFloat(uint8_t);

constexpr uint8_t clampToComponentByte(int);
constexpr uint8_t clampToComponentFloat(float);

template<typename T> T convertComponentByteTo(uint8_t);
template<typename T> T convertComponentFloatTo(float);

template<template<typename> typename ColorType> ColorType<uint8_t> convertToComponentBytes(const ColorType<float>&);
template<template<typename> typename ColorType> constexpr ColorType<float> convertToComponentFloats(const ColorType<uint8_t>&);

template<template<typename> typename ColorType> constexpr ColorType<uint8_t> clampToComponentBytes(int r, int g, int b, int a);
template<template<typename> typename ColorType> constexpr ColorType<float> clampToComponentFloats(float r, float g, float b, float a);

template<typename ColorType, typename Functor> ColorType colorByModifingEachNonAlphaComponent(const ColorType&, Functor&&);

template<typename ColorType> constexpr ColorType colorWithOverridenAlpha(const ColorType&, uint8_t overrideAlpha);
template<typename ColorType> ColorType colorWithOverridenAlpha(const ColorType&, float overrideAlpha);

template<typename ColorType> constexpr ColorType invertedColorWithOverridenAlpha(const ColorType&, uint8_t overrideAlpha);
template<typename ColorType> ColorType invertedColorWithOverridenAlpha(const ColorType&, float overrideAlpha);

template<typename ColorType> constexpr bool isBlack(const ColorType&);
template<typename ColorType> constexpr bool isWhite(const ColorType&);

constexpr uint16_t fastMultiplyBy255(uint16_t);
constexpr uint16_t fastDivideBy255(uint16_t);


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

constexpr uint8_t clampToComponentByte(int c)
{
    return std::clamp(c, 0, 255);
}

constexpr uint8_t clampToComponentFloat(float f)
{
    return std::clamp(f, 0.0f, 1.0f);
}

template<> constexpr uint8_t convertComponentByteTo<uint8_t>(uint8_t value)
{
    return value;
}

template<> constexpr float convertComponentByteTo<float>(uint8_t value)
{
    return convertToComponentFloat(value);
}

template<> inline uint8_t convertComponentFloatTo<uint8_t>(float value)
{
    return convertToComponentByte(value);
}

template<> inline float convertComponentFloatTo<float>(float value)
{
    return clampToComponentFloat(value);
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

template<typename ColorType, typename Functor> ColorType colorByModifingEachNonAlphaComponent(const ColorType& color, Functor&& functor)
{
    // FIXME: This should be made to work with colors that don't use the names red, green, and blue for their channels.
    auto copy = color;
    copy.red = functor(color.red);
    copy.green = functor(color.green);
    copy.blue = functor(color.blue);
    return copy;
}

template<typename ColorType> constexpr ColorType colorWithOverridenAlpha(const ColorType& color, uint8_t overrideAlpha)
{
    ColorType copy = color;
    copy.alpha = convertComponentByteTo<decltype(copy.alpha)>(overrideAlpha);
    return copy;
}

template<typename ColorType> ColorType colorWithOverridenAlpha(const ColorType& color, float overrideAlpha)
{
    ColorType copy = color;
    copy.alpha = convertComponentFloatTo<decltype(copy.alpha)>(overrideAlpha);
    return copy;
}

constexpr uint8_t invertComponent(uint8_t component)
{
    return 255 - component;
}

constexpr float invertComponent(float component)
{
    return 1.0f - component;
}

template<typename ColorType> constexpr ColorType invertedColorWithOverridenAlpha(const ColorType& color, uint8_t overrideAlpha)
{
    auto copy = colorByModifingEachNonAlphaComponent(color, [] (auto component) {
        return invertComponent(component);
    });
    copy.alpha = convertComponentByteTo<decltype(copy.alpha)>(overrideAlpha);
    return copy;
}

template<typename ColorType> ColorType invertedColorWithOverridenAlpha(const ColorType& color, float overrideAlpha)
{
    auto copy = colorByModifingEachNonAlphaComponent(color, [] (auto component) {
        return invertComponent(component);
    });
    copy.alpha = convertComponentFloatTo<decltype(copy.alpha)>(overrideAlpha);
    return copy;
}

template<typename ColorType> constexpr bool isBlack(const ColorType& color)
{
    constexpr auto min = ComponentTraits<typename ColorType::ComponentType>::minValue;
    constexpr auto max = ComponentTraits<typename ColorType::ComponentType>::maxValue;

    auto [c1, c2, c3, alpha] = color;
    return c1 == min && c2 == min && c3 == min && alpha == max;
}

template<typename ColorType> constexpr bool isWhite(const ColorType& color)
{
    constexpr auto max = ComponentTraits<typename ColorType::ComponentType>::maxValue;

    auto [c1, c2, c3, alpha] = color;
    return c1 == max && c2 == max && c3 == max && alpha == max;
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
