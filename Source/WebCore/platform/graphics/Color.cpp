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

#include "AnimationUtilities.h"
#include "ColorUtilities.h"
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

static constexpr SimpleColor lightenedBlack { 0xFF545454 };
static constexpr SimpleColor darkenedWhite { 0xFFABABAB };

int differenceSquared(const Color& c1, const Color& c2)
{
    // FIXME: ExtendedColor - needs to handle color spaces.
    // FIXME: This should probably return a floating point number, but many of the call
    // sites have picked comparison values based on feel. We'd need to break out
    // our logarithm tables to change them :)

    auto [c1Red, c1Blue, c1Green, c1Alpha] = c1.toSRGBASimpleColorLossy();
    auto [c2Red, c2Blue, c2Green, c2Alpha] = c2.toSRGBASimpleColorLossy();

    int dR = c1Red - c2Red;
    int dG = c1Green - c2Green;
    int dB = c1Blue - c2Blue;
    return dR * dR + dG * dG + dB * dB;
}

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
    other.m_colorData.simpleColorAndFlags = invalidSimpleColor;

    return *this;
}

String Color::serialized() const
{
    if (isExtended())
        return asExtended().cssText();
    return asSimple().serializationForHTML();
}

String Color::cssText() const
{
    if (isExtended())
        return asExtended().cssText();
    return asSimple().serializationForCSS();
}

String Color::nameForRenderTreeAsText() const
{
    if (isExtended())
        return asExtended().cssText();
    return asSimple().serializationForRenderTreeAsText();
}

Color Color::light() const
{
    // Hardcode this common case for speed.
    if (!isExtended() && asSimple() == black)
        return lightenedBlack;
    
    const float scaleFactor = nextafterf(256.0f, 0.0f);

    auto [r, g, b, a] = toSRGBAComponentsLossy();

    float v = std::max({ r, g, b });

    if (v == 0.0f)
        return lightenedBlack.colorWithAlpha(alpha());

    float multiplier = std::min(1.0f, v + 0.33f) / v;

    return makeSimpleColor(
        static_cast<int>(multiplier * r * scaleFactor),
        static_cast<int>(multiplier * g * scaleFactor),
        static_cast<int>(multiplier * b * scaleFactor),
        alpha()
    );
}

Color Color::dark() const
{
    // Hardcode this common case for speed.
    if (!isExtended() && asSimple() == white)
        return darkenedWhite;
    
    const float scaleFactor = nextafterf(256.0f, 0.0f);

    auto [r, g, b, a] = toSRGBAComponentsLossy();

    float v = std::max({ r, g, b });
    float multiplier = std::max(0.0f, (v - 0.33f) / v);

    return makeSimpleColor(
        static_cast<int>(multiplier * r * scaleFactor),
        static_cast<int>(multiplier * g * scaleFactor),
        static_cast<int>(multiplier * b * scaleFactor),
        alpha()
    );
}

bool Color::isDark() const
{
    // FIXME: This should probably be using luminance.
    auto [r, g, b, a] = toSRGBAComponentsLossy();
    float largestNonAlphaChannel = std::max({ r, g, b });
    return a > 0.5 && largestNonAlphaChannel < 0.5;
}

float Color::lightness() const
{
    // FIXME: This can probably avoid conversion to sRGB by having per-colorspace algorithms for HSL.
    return WebCore::lightness(toSRGBAComponentsLossy());
}

Color Color::blend(const Color& source) const
{
    if (!isVisible() || source.isOpaque())
        return source;

    if (!source.alpha())
        return *this;

    auto [selfR, selfG, selfB, selfA] = toSRGBASimpleColorLossy();
    auto [sourceR, sourceG, sourceB, sourceA] = source.toSRGBASimpleColorLossy();

    int d = 0xFF * (selfA + sourceA) - selfA * sourceA;
    int a = d / 0xFF;
    int r = (selfR * selfA * (0xFF - sourceA) + 0xFF * sourceA * sourceR) / d;
    int g = (selfG * selfA * (0xFF - sourceA) + 0xFF * sourceA * sourceG) / d;
    int b = (selfB * selfA * (0xFF - sourceA) + 0xFF * sourceA * sourceB) / d;

    return makeSimpleColor(r, g, b, a);
}

Color Color::blendWithWhite() const
{
    constexpr int startAlpha = 153; // 60%
    constexpr int endAlpha = 204; // 80%;
    constexpr int alphaIncrement = 17;

    auto blendComponent = [](int c, int a) -> int {
        float alpha = a / 255.0f;
        int whiteBlend = 255 - a;
        c -= whiteBlend;
        return static_cast<int>(c / alpha);
    };

    // If the color contains alpha already, we leave it alone.
    if (!isOpaque())
        return *this;

    auto [existingR, existingG, existingB, existingAlpha] = toSRGBASimpleColorLossy();

    Color result;
    for (int alpha = startAlpha; alpha <= endAlpha; alpha += alphaIncrement) {
        // We have a solid color.  Convert to an equivalent color that looks the same when blended with white
        // at the current alpha.  Try using less transparency if the numbers end up being negative.
        int r = blendComponent(existingR, alpha);
        int g = blendComponent(existingG, alpha);
        int b = blendComponent(existingB, alpha);
        
        result = makeSimpleColor(r, g, b, alpha);

        if (r >= 0 && g >= 0 && b >= 0)
            break;
    }

    // FIXME: Why is preserving the semantic bit desired and/or correct here?
    if (isSemantic())
        result.tagAsSemantic();
    return result;
}

Color Color::colorWithAlphaMultipliedBy(float amount) const
{
    float newAlpha = amount * alphaAsFloat();
    return colorWithAlpha(newAlpha);
}

Color Color::colorWithAlphaMultipliedByUsingAlternativeRounding(float amount) const
{
    float newAlpha = amount * alphaAsFloat();
    return colorWithAlphaUsingAlternativeRounding(newAlpha);
}

Color Color::colorWithAlpha(float alpha) const
{
    if (isExtended())
        return asExtended().colorWithAlpha(alpha);

    // FIXME: This is where this function differs from colorWithAlphaUsingAlternativeRounding.
    uint8_t newAlpha = alpha * 0xFF;

    Color result = asSimple().colorWithAlpha(newAlpha);
    if (isSemantic())
        result.tagAsSemantic();
    return result;
}

Color Color::colorWithAlphaUsingAlternativeRounding(float alpha) const
{
    if (isExtended())
        return asExtended().colorWithAlpha(alpha);

    // FIXME: This is where this function differs from colorWithAlphaUsing.
    uint8_t newAlpha = scaleRoundAndClampColorChannel(alpha);

    Color result = asSimple().colorWithAlpha(newAlpha);
    if (isSemantic())
        result.tagAsSemantic();
    return result;
}

Color Color::invertedColorWithAlpha(float alpha) const
{
    if (isExtended())
        return asExtended().invertedColorWithAlpha(alpha);
    return asSimple().invertedColorWithAlpha(scaleRoundAndClampColorChannel(alpha));
}

Color Color::semanticColor() const
{
    if (isSemantic())
        return *this;

    return { toSRGBASimpleColorLossy(), Semantic };
}

std::pair<ColorSpace, ColorComponents<float>> Color::colorSpaceAndComponents() const
{
    if (isExtended())
        return { asExtended().colorSpace(), asExtended().components() };
    return { ColorSpace::SRGB, asSimple().asSRGBFloatComponents() };
}

SimpleColor Color::toSRGBASimpleColorLossy() const
{
    if (isExtended())
        return makeSimpleColor(toSRGBAComponentsLossy());
    return asSimple();
}

ColorComponents<float> Color::toSRGBAComponentsLossy() const
{
    if (isExtended())
        return asExtended().toSRGBAComponentsLossy();
    return asSimple().asSRGBFloatComponents();
}

Color blend(const Color& from, const Color& to, double progress)
{
    // FIXME: ExtendedColor - needs to handle color spaces.
    // We need to preserve the state of the valid flag at the end of the animation
    if (progress == 1 && !to.isValid())
        return { };

    // Since makePremultipliedSimpleColor() bails on zero alpha, special-case that.
    auto premultFrom = from.alpha() ? makePremultipliedSimpleColor(from.toSRGBASimpleColorLossy()) : Color::transparent;
    auto premultTo = to.alpha() ? makePremultipliedSimpleColor(to.toSRGBASimpleColorLossy()) : Color::transparent;

    SimpleColor premultBlended = makeSimpleColor(
        WebCore::blend(premultFrom.redComponent(), premultTo.redComponent(), progress),
        WebCore::blend(premultFrom.greenComponent(), premultTo.greenComponent(), progress),
        WebCore::blend(premultFrom.blueComponent(), premultTo.blueComponent(), progress),
        WebCore::blend(premultFrom.alphaComponent(), premultTo.alphaComponent(), progress)
    );

    return makeUnpremultipliedSimpleColor(premultBlended);
}

Color blendWithoutPremultiply(const Color& from, const Color& to, double progress)
{
    // FIXME: ExtendedColor - needs to handle color spaces.
    // We need to preserve the state of the valid flag at the end of the animation
    if (progress == 1 && !to.isValid())
        return { };

    auto fromSRGB = from.toSRGBASimpleColorLossy();
    auto toSRGB = from.toSRGBASimpleColorLossy();

    return makeSimpleColorFromFloats(
        WebCore::blend(fromSRGB.redComponent(), toSRGB.redComponent(), progress),
        WebCore::blend(fromSRGB.greenComponent(), toSRGB.greenComponent(), progress),
        WebCore::blend(fromSRGB.blueComponent(), toSRGB.blueComponent(), progress),
        WebCore::blend(fromSRGB.alphaComponent(), toSRGB.alphaComponent(), progress)
    );
}

TextStream& operator<<(TextStream& ts, const Color& color)
{
    return ts << color.nameForRenderTreeAsText();
}

TextStream& operator<<(TextStream& ts, ColorSpace colorSpace)
{
    switch (colorSpace) {
    case ColorSpace::SRGB:
        ts << "sRGB";
        break;
    case ColorSpace::LinearRGB:
        ts << "LinearRGB";
        break;
    case ColorSpace::DisplayP3:
        ts << "DisplayP3";
        break;
    }
    return ts;
}

} // namespace WebCore
