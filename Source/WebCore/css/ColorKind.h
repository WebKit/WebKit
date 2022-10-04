/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Color.h"
#include "ColorInterpolationMethod.h"
#include <variant>
#include <vector>

namespace WebCore {

struct CurrentColor {
    bool operator==(const CurrentColor&) const
    {
        return true;
    }
    bool operator!=(const CurrentColor& other) const
    {
        return !(*this == other);
    }
};

struct ColorMixPercentages {
    double p1;
    double p2;
    std::optional<double> alphaMultiplier;
    bool operator==(ColorMixPercentages const&) const = default;
    friend WTF::TextStream& operator<<(WTF::TextStream&, ColorMixPercentages const&);
};

template <typename T> class ColorMixComponent;

template <typename T>
class ColorMix {
public:
    ColorMix(ColorInterpolationMethod colorInterpolationMethod, ColorMixComponent<T> const& a, ColorMixComponent<T> const& b, ColorMixPercentages const& percentages)
    : m_colorInterpolationMethod { colorInterpolationMethod }
    , m_percentages { percentages }
    {
        m_components = std::vector { a, b };
    }

    bool operator==(ColorMix<T> const& other) const {
        return m_components == other.m_components;
    }

    const T& colorA() const
    {
        ASSERT(m_components.size() == 2);
        return m_components[0].color();
    }

    const T& colorB() const
    {
        ASSERT(m_components.size() == 2);
        return m_components[1].color();
    }
    
    ColorInterpolationMethod colorInterpolationMethod() const
    {
        return m_colorInterpolationMethod;
    }
    
    ColorMixPercentages percentages() const
    {
        return m_percentages;
    }

    friend WTF::TextStream& operator<<(WTF::TextStream& ts, ColorMix<T> const&) {
        return ts;
    }

private:
    ColorInterpolationMethod m_colorInterpolationMethod;
    std::vector<ColorMixComponent<T>> m_components;
    ColorMixPercentages m_percentages;
};

template <typename T>
class ColorMixComponent {
public:
    ColorMixComponent(const T& color, std::optional<double> percentage) 
    : m_color(color)
    , m_percentage(percentage) {}

    ColorMixComponent(const ColorMixComponent<T>&) = default;
    ColorMixComponent& operator=(const ColorMixComponent<T>&) = default;
    bool operator==(ColorMixComponent<T> const& other) const {
        return m_color == other.m_color && m_percentage == other.m_percentage;
    }
    void setColor(const T& color) { m_color = color; }
    void setPercentage(double percentage) { m_percentage = percentage; }
    const T& color() const { return m_color; }
    std::optional<double> percentage() const { return m_percentage; }
    friend WTF::TextStream& operator<<(WTF::TextStream&, ColorMixComponent<T> const&);
private:
    T m_color;
    std::optional<double> m_percentage;
};

struct CSSColorKind final : std::variant<Color, ColorMix<CSSColorKind>>
{
};

struct StyleColorKind final : std::variant<Color, CurrentColor, ColorMix<StyleColorKind>>
{
};

WEBCORE_EXPORT String serializationForCSS(const CSSColorKind&);
bool operator==(const CSSColorKind&, const CSSColorKind&);

} // namespace WebCore
