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

#include "ColorBuilder.h"
#include "ColorComponents.h"
#include "ColorTypes.h"
#include "ColorUtilities.h"

namespace WebCore {

class SimpleColor {
public:
    constexpr explicit SimpleColor(SRGBA<uint8_t> value) : m_value { value } { }

    constexpr uint8_t alphaComponent() const { return m_value.alpha; }
    constexpr float alphaComponentAsFloat() const { return convertToComponentFloat(m_value.alpha); }

    constexpr bool isOpaque() const { return m_value.alpha == 0xFF; }
    constexpr bool isVisible() const { return m_value.alpha; }

    constexpr SimpleColor colorWithAlpha(uint8_t alpha) const
    {
        return { m_value.red, m_value.green, m_value.blue, alpha };
    }

    constexpr SimpleColor invertedColorWithAlpha(uint8_t alpha) const
    {
        return { static_cast<uint8_t>(0xFF - m_value.red), static_cast<uint8_t>(0xFF - m_value.green), static_cast<uint8_t>(0xFF - m_value.blue), alpha };
    }

    template<typename T> constexpr SRGBA<T> asSRGBA() const
    {
        if constexpr (std::is_same_v<T, float>)
            return convertToComponentFloats(m_value);
        else if constexpr (std::is_same_v<T, uint8_t>)
            return m_value;
    }

    template<std::size_t N>
    constexpr uint8_t get() const
    {
        static_assert(N < 4);
        if constexpr (!N)
            return m_value.red;
        else if constexpr (N == 1)
            return m_value.green;
        else if constexpr (N == 2)
            return m_value.blue;
        else if constexpr (N == 3)
            return m_value.alpha;
    }

private:
    constexpr SimpleColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) : m_value { r, g, b, a } { }

    SRGBA<uint8_t> m_value { 0, 0, 0, 0 };
};

bool operator==(SimpleColor, SimpleColor);
bool operator!=(SimpleColor, SimpleColor);

inline bool operator==(SimpleColor a, SimpleColor b)
{
    return a.asSRGBA<uint8_t>() == b.asSRGBA<uint8_t>();
}

inline bool operator!=(SimpleColor a, SimpleColor b)
{
    return !(a == b);
}

constexpr ColorBuilder<SRGBA<uint8_t>> makeSimpleColor(int r, int g, int b);
constexpr ColorBuilder<SRGBA<uint8_t>> makeSimpleColor(int r, int g, int b, int a);
constexpr ColorBuilder<SRGBA<uint8_t>> makeSimpleColor(SRGBA<uint8_t>);
ColorBuilder<SRGBA<uint8_t>> makeSimpleColor(const SRGBA<float>&);

constexpr ColorBuilder<SRGBA<uint8_t>> makeSimpleColor(int r, int g, int b)
{
    return clampToComponentBytes<SRGBA>(r, g, b, 0xFF);
}

constexpr ColorBuilder<SRGBA<uint8_t>> makeSimpleColor(int r, int g, int b, int a)
{
    return clampToComponentBytes<SRGBA>(r, g, b, a);
}

constexpr ColorBuilder<SRGBA<uint8_t>> makeSimpleColor(SRGBA<uint8_t> sRGBA)
{
    return sRGBA;
}

inline ColorBuilder<SRGBA<uint8_t>> makeSimpleColor(const SRGBA<float>& sRGBA)
{
    return convertToComponentBytes(sRGBA);
}

} // namespace WebCore

namespace std {

template<> class tuple_size<WebCore::SimpleColor> : public std::integral_constant<std::size_t, 4> {
};

template<std::size_t N> class tuple_element<N, WebCore::SimpleColor> {
public:
    using type = uint8_t;
};

}
