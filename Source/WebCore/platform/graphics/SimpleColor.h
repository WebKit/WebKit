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
#include <unicode/uchar.h>
#include <wtf/Forward.h>
#include <wtf/Optional.h>
#include <wtf/text/LChar.h>

namespace WebCore {

// Color value with 8-bit components for red, green, blue, and alpha.
// For historical reasons, stored as a 32-bit integer, with alpha in the high bits: ARGB.
// FIXME: This class should be consolidated with ColorComponents.
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

    static Optional<SimpleColor> parseHexColor(const String&);
    static Optional<SimpleColor> parseHexColor(const StringView&);
    static Optional<SimpleColor> parseHexColor(const LChar*, unsigned);
    static Optional<SimpleColor> parseHexColor(const UChar*, unsigned);

private:
    uint32_t m_value { 0 };
};

bool operator==(SimpleColor, SimpleColor);
bool operator!=(SimpleColor, SimpleColor);

constexpr SimpleColor makeSimpleColor(int r, int g, int b);
constexpr SimpleColor makeSimpleColor(int r, int g, int b, int a);

SimpleColor makePremultipliedSimpleColor(int r, int g, int b, int a, bool ceiling = true);
SimpleColor makePremultipliedSimpleColor(SimpleColor);
SimpleColor makeUnpremultipliedSimpleColor(int r, int g, int b, int a);
SimpleColor makeUnpremultipliedSimpleColor(SimpleColor);

WEBCORE_EXPORT SimpleColor makeSimpleColorFromFloats(float r, float g, float b, float a);
WEBCORE_EXPORT SimpleColor makeSimpleColorFromHSLA(float h, float s, float l, float a);
SimpleColor makeSimpleColorFromCMYKA(float c, float m, float y, float k, float a);

uint8_t roundAndClampColorChannel(int);
uint8_t roundAndClampColorChannel(float);

uint16_t fastMultiplyBy255(uint16_t value);
uint16_t fastDivideBy255(uint16_t);

uint8_t colorFloatToSimpleColorByte(float);

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
    return std::clamp(value, 0, 255);
}

inline uint8_t roundAndClampColorChannel(float value)
{
    return std::clamp(std::round(value), 0.f, 255.f);
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

inline uint8_t colorFloatToSimpleColorByte(float f)
{
    // FIXME: Consolidate with clampedColorComponent().
    // We use lroundf and 255 instead of nextafterf(256, 0) to match CG's rounding
    return std::clamp(static_cast<int>(lroundf(255.0f * f)), 0, 255);
}

constexpr SimpleColor makeSimpleColor(int r, int g, int b)
{
    return makeSimpleColor(r, g, b, 0xFF);
}

constexpr SimpleColor makeSimpleColor(int r, int g, int b, int a)
{
    return { static_cast<unsigned>(std::clamp(a, 0, 0xFF) << 24 | std::clamp(r, 0, 0xFF) << 16 | std::clamp(g, 0, 0xFF) << 8 | std::clamp(b, 0, 0xFF)) };
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
