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

#include "config.h"
#include "Color.h"

#include "ColorSerialization.h"
#include "ColorUtilities.h"
#include <wtf/Assertions.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

static constexpr auto lightenedBlack = SRGBA<uint8_t> { 84, 84, 84 };
static constexpr auto darkenedWhite = SRGBA<uint8_t> { 171, 171, 171 };

Color::Color(const Color& other)
    : m_colorData(other.m_colorData)
{
    if (isExtended())
        m_colorData.extendedColor->ref();
}

Color::Color(Color&& other)
{
    *this = WTFMove(other);
}

Color& Color::operator=(const Color& other)
{
    if (*this == other)
        return *this;

    if (isExtended())
        m_colorData.extendedColor->deref();

    m_colorData = other.m_colorData;

    if (isExtended())
        m_colorData.extendedColor->ref();
    return *this;
}

Color& Color::operator=(Color&& other)
{
    if (*this == other)
        return *this;

    if (isExtended())
        m_colorData.extendedColor->deref();

    m_colorData = other.m_colorData;
    other.m_colorData.inlineColorAndFlags = invalidInlineColor;

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

    return convertToComponentBytes(SRGBA { multiplier * r, multiplier * g, multiplier * b, a });
}

Color Color::darkened() const
{
    // Hardcode this common case for speed.
    if (isInline() && asInline() == white)
        return darkenedWhite;
    
    auto [r, g, b, a] = toSRGBALossy<float>();

    float v = std::max({ r, g, b });
    float multiplier = std::max(0.0f, (v - 0.33f) / v);

    return convertToComponentBytes(SRGBA { multiplier * r, multiplier * g, multiplier * b, a });
}

float Color::lightness() const
{
    // FIXME: This can probably avoid conversion to sRGB by having per-colorspace algorithms for HSL.
    return WebCore::lightness(toSRGBALossy<float>());
}

float Color::luminance() const
{
    // FIXME: This can probably avoid conversion to sRGB by having per-colorspace algorithms
    // for luminance (e.g. convertToXYZ(c).yComponent()).
    return WebCore::luminance(toSRGBALossy<float>());
}

Color Color::colorWithAlpha(float alpha) const
{
    return callOnUnderlyingType(WTF::makeVisitor(
        [&] (const SRGBA<uint8_t>& underlyingColor) -> Color {
            Color result = colorWithOverridenAlpha(underlyingColor, alpha);

            // FIXME: Why is preserving the semantic bit desired and/or correct here?
            if (isSemantic())
                result.tagAsSemantic();
            return result;
        },
        [&] (const auto& underlyingColor) -> Color {
            return colorWithOverridenAlpha(underlyingColor, alpha);
        }
    ));
}

Color Color::invertedColorWithAlpha(float alpha) const
{
    return callOnUnderlyingType([&] (const auto& underlyingColor) -> Color {
        return invertedColorWithOverridenAlpha(underlyingColor, alpha);
    });
}

Color Color::semanticColor() const
{
    if (isSemantic())
        return *this;

    return { toSRGBALossy<uint8_t>(), Semantic };
}

std::pair<ColorSpace, ColorComponents<float>> Color::colorSpaceAndComponents() const
{
    if (isExtended())
        return { asExtended().colorSpace(), asExtended().components() };
    return { ColorSpace::SRGB, asColorComponents(convertToComponentFloats(asInline())) };
}

TextStream& operator<<(TextStream& ts, const Color& color)
{
    return ts << serializationForRenderTreeAsText(color);
}

#ifdef __HAIKU__
bool Color::isBlackColor(const Color& color)
{
    return color.callOnUnderlyingType([] (const auto& color) {
        return WebCore::isBlack(color);
    });
}

bool Color::isWhiteColor(const Color& color)
{
    return color.callOnUnderlyingType([] (const auto& color) {
        return WebCore::isWhite(color);
    });
}
#endif

} // namespace WebCore
