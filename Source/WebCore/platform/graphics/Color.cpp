/*
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "Color.h"

#include "ColorLuminance.h"
#include "ColorSerialization.h"
#include <wtf/Assertions.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

static constexpr auto lightenedBlack = SRGBA<uint8_t> { 84, 84, 84 };
static constexpr auto darkenedWhite = SRGBA<uint8_t> { 171, 171, 171 };

Color::Color(const Color& other)
    : m_colorAndFlags(other.m_colorAndFlags)
{
    if (isOutOfLine())
        asOutOfLine().ref();
}

Color::Color(Color&& other)
{
    *this = WTFMove(other);
}

Color& Color::operator=(const Color& other)
{
    if (*this == other)
        return *this;

    if (isOutOfLine())
        asOutOfLine().deref();

    m_colorAndFlags = other.m_colorAndFlags;

    if (isOutOfLine())
        asOutOfLine().ref();

    return *this;
}

Color& Color::operator=(Color&& other)
{
    if (*this == other)
        return *this;

    if (isOutOfLine())
        asOutOfLine().deref();

    m_colorAndFlags = other.m_colorAndFlags;
    other.m_colorAndFlags = invalidColorAndFlags;

    return *this;
}

Color Color::lightened() const
{
    // Hardcode this common case for speed.
    if (isInline() && asInline() == black)
        return lightenedBlack;

    auto [r, g, b, a] = toSRGBALossy<float>();
    float v = std::max({ r, g, b });

    if (v == 0.0f)
        return lightenedBlack.colorWithAlphaByte(alphaByte());

    float multiplier = std::min(1.0f, v + 0.33f) / v;

    return convertColor<SRGBA<uint8_t>>(SRGBA<float> { multiplier * r, multiplier * g, multiplier * b, a });
}

Color Color::darkened() const
{
    // Hardcode this common case for speed.
    if (isInline() && asInline() == white)
        return darkenedWhite;
    
    auto [r, g, b, a] = toSRGBALossy<float>();

    float v = std::max({ r, g, b });
    float multiplier = std::max(0.0f, (v - 0.33f) / v);

    return convertColor<SRGBA<uint8_t>>(SRGBA<float> { multiplier * r, multiplier * g, multiplier * b, a });
}

double Color::lightness() const
{
    // FIXME: Replace remaining uses with luminance.
    auto [r, g, b, a] = toSRGBALossy<float>();
    auto [min, max] = std::minmax({ r, g, b });
    return 0.5 * (max + min);
}

double Color::luminance() const
{
    return callOnUnderlyingType([&] (const auto& underlyingColor) {
        return WebCore::relativeLuminance(underlyingColor);
    });
}

Color Color::colorWithAlpha(float alpha) const
{
    return callOnUnderlyingType([&] (const auto& underlyingColor) -> Color {
        auto result = colorWithOverriddenAlpha(underlyingColor, alpha);

        // FIXME: Why is preserving the semantic bit desired and/or correct here?
        if (isSemantic())
            return { result, Flags::Semantic };

        return { result };
    });
}

Color Color::invertedColorWithAlpha(float alpha) const
{
    return callOnUnderlyingType([&] (const auto& underlyingColor) -> Color {
        using ColorType = std::decay_t<decltype(underlyingColor)>;

        // FIXME: Determine if there is a meaningful understanding of inversion that works
        // better for non-invertible color types like Lab or consider removing this in favor
        // of alternatives.
        if constexpr (ColorType::Model::isInvertible)
            return invertedColorWithOverriddenAlpha(underlyingColor, alpha);
        else
            return invertedColorWithOverriddenAlpha(convertColor<SRGBA<float>>(underlyingColor), alpha);
    });
}

Color Color::semanticColor() const
{
    if (isSemantic())
        return *this;
    
    if (isOutOfLine())
        return { asOutOfLineRef(), colorSpace(), Flags::Semantic };
    return { asInline(), Flags::Semantic };
}

ColorComponents<float, 4> Color::toColorComponentsInColorSpace(ColorSpace outputColorSpace) const
{
    auto [inputColorSpace, components] = colorSpaceAndComponents();
    return converColorComponents(inputColorSpace, components, outputColorSpace);
}

ColorComponents<float, 4> Color::toColorComponentsInColorSpace(const DestinationColorSpace& outputColorSpace) const
{
    auto [inputColorSpace, components] = colorSpaceAndComponents();
    return converColorComponents(inputColorSpace, components, outputColorSpace);
}

std::pair<ColorSpace, ColorComponents<float, 4>> Color::colorSpaceAndComponents() const
{
    if (isOutOfLine())
        return { colorSpace(), asOutOfLine().components() };
    return { ColorSpace::SRGB, asColorComponents(convertColor<SRGBA<float>>(asInline())) };
}

bool Color::isBlackColor(const Color& color)
{
    return color.callOnUnderlyingType([] (const auto& underlyingColor) {
        return WebCore::isBlack(underlyingColor);
    });
}

bool Color::isWhiteColor(const Color& color)
{
    return color.callOnUnderlyingType([] (const auto& underlyingColor) {
        return WebCore::isWhite(underlyingColor);
    });
}

TextStream& operator<<(TextStream& ts, const Color& color)
{
    return ts << serializationForRenderTreeAsText(color);
}

} // namespace WebCore
