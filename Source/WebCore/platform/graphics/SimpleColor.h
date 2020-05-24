/*
 * Copyright (C) 2003-2020 Apple Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "ColorUtilities.h"
#include <algorithm>
#include <cmath>
#include <wtf/Forward.h>

namespace WebCore {

// Color value with 8-bit components for red, green, blue, and alpha.
// For historical reasons, stored as a 32-bit integer, with alpha in the high bits: ARGB.
class SimpleColor {
public:
    constexpr SimpleColor(uint32_t value = 0) : m_value { value } { }

    constexpr uint32_t value() const { return m_value; }

    constexpr uint8_t redComponent() const { return m_value >> 16; }
    constexpr uint8_t greenComponent() const { return m_value >> 8; }
    constexpr uint8_t blueComponent() const { return m_value; }
    constexpr uint8_t alphaComponent() const { return m_value >> 24; }

    constexpr float alphaComponentAsFloat() const { return static_cast<float>(alphaComponent()) / 0xFF; }

    constexpr bool isOpaque() const { return alphaComponent() == 0xFF; }
    constexpr bool isVisible() const { return alphaComponent(); }

    String serializationForHTML() const;
    String serializationForCSS() const;
    String serializationForRenderTreeAsText() const;

    constexpr SimpleColor colorWithAlpha(uint8_t alpha) const { return { (m_value & 0x00FFFFFF) | alpha << 24 }; }

    template<std::size_t N>
    constexpr uint8_t get() const
    {
        static_assert(N < 4);
        if constexpr (!N)
            return redComponent();
        else if constexpr (N == 1)
            return greenComponent();
        else if constexpr (N == 2)
            return blueComponent();
        else if constexpr (N == 3)
            return alphaComponent();
    }

private:
    uint32_t m_value { 0 };
};

bool operator==(SimpleColor, SimpleColor);
bool operator!=(SimpleColor, SimpleColor);

// FIXME: Remove this after migrating to the new name.
using RGBA32 = SimpleColor;

constexpr RGBA32 makeRGB(int r, int g, int b);
constexpr RGBA32 makeRGBA(int r, int g, int b, int a);

RGBA32 makePremultipliedRGBA(int r, int g, int b, int a, bool ceiling = true);
RGBA32 makePremultipliedRGBA(RGBA32);
RGBA32 makeUnPremultipliedRGBA(int r, int g, int b, int a);
RGBA32 makeUnPremultipliedRGBA(RGBA32);

WEBCORE_EXPORT RGBA32 makeRGBA32FromFloats(float r, float g, float b, float a);
WEBCORE_EXPORT RGBA32 makeRGBAFromHSLA(float h, float s, float l, float a);
RGBA32 makeRGBAFromCMYKA(float c, float m, float y, float k, float a);

uint8_t roundAndClampColorChannel(int);
uint8_t roundAndClampColorChannel(float);

uint16_t fastMultiplyBy255(uint16_t value);
uint16_t fastDivideBy255(uint16_t);

uint8_t colorFloatToRGBAByte(float);

inline bool operator==(SimpleColor a, SimpleColor b)
{
    return a.value() == b.value();
}

inline bool operator!=(SimpleColor a, SimpleColor b)
{
    return !(a == b);
}

inline uint8_t roundAndClampColorChannel(int value)
{
    return std::max(0, std::min(255, value));
}

inline uint8_t roundAndClampColorChannel(float value)
{
    return std::max(0.f, std::min(255.f, std::round(value)));
}

inline uint16_t fastMultiplyBy255(uint16_t value)
{
    return (value << 8) - value;
}

inline uint16_t fastDivideBy255(uint16_t value)
{
    // While this is an approximate algorithm for division by 255, it gives perfectly accurate results for 16-bit values.
    // FIXME: Since this gives accurate results for 16-bit values, we should get this optimization into compilers like clang.
    uint16_t approximation = value >> 8;
    uint16_t remainder = value - (approximation * 255) + 1;
    return approximation + (remainder >> 8);
}

inline uint8_t colorFloatToRGBAByte(float f)
{
    // FIXME: Consolidate with clampedColorComponent().
    // We use lroundf and 255 instead of nextafterf(256, 0) to match CG's rounding
    return std::max(0, std::min(static_cast<int>(lroundf(255.0f * f)), 255));
}

constexpr RGBA32 makeRGB(int r, int g, int b)
{
    return makeRGBA(r, g, b, 0xFF);
}

constexpr RGBA32 makeRGBA(int r, int g, int b, int a)
{
    return { static_cast<unsigned>(std::max(0, std::min(a, 0xFF)) << 24 | std::max(0, std::min(r, 0xFF)) << 16 | std::max(0, std::min(g, 0xFF)) << 8 | std::max(0, std::min(b, 0xFF))) };
}

} // namespace WebCore

namespace std {

template<>
class tuple_size<WebCore::SimpleColor> : public std::integral_constant<std::size_t, 4> {
};

template<std::size_t N>
class tuple_element<N, WebCore::SimpleColor> {
public:
    using type = uint8_t;
};

}
