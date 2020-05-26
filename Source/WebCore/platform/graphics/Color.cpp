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
#include "HashTools.h"
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

static inline const NamedColor* findNamedColor(const String& name)
{
    char buffer[64]; // easily big enough for the longest color name
    unsigned length = name.length();
    if (length > sizeof(buffer) - 1)
        return nullptr;
    for (unsigned i = 0; i < length; ++i) {
        UChar c = name[i];
        if (!c || !WTF::isASCII(c))
            return nullptr;
        buffer[i] = toASCIILower(static_cast<char>(c));
    }
    buffer[length] = '\0';
    return findColor(buffer, length);
}

Color::Color(const String& name)
{
    if (name[0] == '#') {
        Optional<SimpleColor> color;
        if (name.is8Bit())
            color = SimpleColor::parseHexColor(name.characters8() + 1, name.length() - 1);
        else
            color = SimpleColor::parseHexColor(name.characters16() + 1, name.length() - 1);

        if (color)
            setSimpleColor(*color);
    } else {
        if (auto* foundColor = findNamedColor(name))
            setSimpleColor({ foundColor->ARGBValue });
    }
}

Color::Color(const char* name)
{
    if (name[0] == '#') {
        auto color = SimpleColor::parseHexColor(reinterpret_cast<const LChar*>(&name[1]), std::strlen(&name[1]));
        if (color)
            setSimpleColor(*color);
    } else if (auto* foundColor = findColor(name, strlen(name)))
        setSimpleColor({ foundColor->ARGBValue });
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

Color::Color(float c1, float c2, float c3, float alpha, ColorSpace colorSpace)
    : Color(ExtendedColor::create(c1, c2, c3, alpha, colorSpace))
{
}

Color::Color(Ref<ExtendedColor>&& extendedColor)
{
    // Zero the union, just in case a 32-bit system only assigns the
    // top 32 bits when copying the ExtendedColor pointer below.
    m_colorData.simpleColorAndFlags = 0;
    m_colorData.extendedColor = &extendedColor.leakRef();
    ASSERT(isExtended());
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
    return asSimpleColor().serializationForHTML();
}

String Color::cssText() const
{
    if (isExtended())
        return asExtended().cssText();
    return asSimpleColor().serializationForCSS();
}

String Color::nameForRenderTreeAsText() const
{
    if (isExtended())
        return asExtended().cssText();
    return asSimpleColor().serializationForRenderTreeAsText();
}

Color Color::light() const
{
    // Hardcode this common case for speed.
    if (!isExtended() && asSimpleColor() == black)
        return lightenedBlack;
    
    const float scaleFactor = nextafterf(256.0f, 0.0f);

    auto [r, g, b, a] = toSRGBAComponentsLossy();

    float v = std::max({ r, g, b });

    if (v == 0.0f)
        return lightenedBlack.colorWithAlpha(alpha());

    float multiplier = std::min(1.0f, v + 0.33f) / v;

    return {
        static_cast<int>(multiplier * r * scaleFactor),
        static_cast<int>(multiplier * g * scaleFactor),
        static_cast<int>(multiplier * b * scaleFactor),
        alpha()
    };
}

Color Color::dark() const
{
    // Hardcode this common case for speed.
    if (!isExtended() && asSimpleColor() == white)
        return darkenedWhite;
    
    const float scaleFactor = nextafterf(256.0f, 0.0f);

    auto [r, g, b, a] = toSRGBAComponentsLossy();

    float v = std::max({ r, g, b });
    float multiplier = std::max(0.0f, (v - 0.33f) / v);

    return {
        static_cast<int>(multiplier * r * scaleFactor),
        static_cast<int>(multiplier * g * scaleFactor),
        static_cast<int>(multiplier * b * scaleFactor),
        alpha()
    };
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
        result.setIsSemantic();
    return result;
}

Color Color::colorWithAlphaMultipliedBy(float amount) const
{
    float newAlpha = amount * (isExtended() ? m_colorData.extendedColor->alpha() : static_cast<float>(alpha()) / 255);
    return colorWithAlpha(newAlpha);
}

Color Color::colorWithAlphaMultipliedByUsingAlternativeRounding(float amount) const
{
    float newAlpha = amount * (isExtended() ? m_colorData.extendedColor->alpha() : static_cast<float>(alpha()) / 255);
    return colorWithAlphaUsingAlternativeRounding(newAlpha);
}

Color Color::colorWithAlpha(float alpha) const
{
    if (isExtended())
        return asExtended().colorWithAlpha(alpha);

    // FIXME: This is where this function differs from colorWithAlphaUsingAlternativeRounding.
    uint8_t newAlpha = alpha * 0xFF;

    Color result = asSimpleColor().colorWithAlpha(newAlpha);
    if (isSemantic())
        result.setIsSemantic();
    return result;
}

Color Color::colorWithAlphaUsingAlternativeRounding(float alpha) const
{
    if (isExtended())
        return asExtended().colorWithAlpha(alpha);

    // FIXME: This is where this function differs from colorWithAlphaUsing.
    uint8_t newAlpha = colorFloatToSimpleColorByte(alpha);

    Color result = asSimpleColor().colorWithAlpha(newAlpha);
    if (isSemantic())
        result.setIsSemantic();
    return result;
}

Color Color::invertedColorWithAlpha(float alpha) const
{
    if (isExtended())
        return Color { asExtended().invertedColorWithAlpha(alpha) };

    auto [r, g, b, existingAlpha] = asSimpleColor();
    return { 0xFF - r, 0xFF - g, 0xFF - b, colorFloatToSimpleColorByte(alpha) };
}

Color Color::semanticColor() const
{
    if (isSemantic())
        return *this;

    return { toSRGBASimpleColorLossy(), Semantic };
}

std::pair<ColorSpace, FloatComponents> Color::colorSpaceAndComponents() const
{
    if (isExtended()) {
        auto& extendedColor = asExtended();
        return { extendedColor.colorSpace(), extendedColor.channels() };
    }

    auto [r, g, b, a] = asSimpleColor();
    return { ColorSpace::SRGB, FloatComponents { r / 255.0f, g / 255.0f, b / 255.0f,  a / 255.0f } };
}

SimpleColor Color::toSRGBASimpleColorLossy() const
{
    if (isExtended()) {
        auto [r, g, b, a] = toSRGBAComponentsLossy();
        return makeSimpleColorFromFloats(r, g, b, a);
    }

    return asSimpleColor();
}

FloatComponents Color::toSRGBAComponentsLossy() const
{
    auto [colorSpace, components] = colorSpaceAndComponents();
    switch (colorSpace) {
    case ColorSpace::SRGB:
        return components;
    case ColorSpace::LinearRGB:
        return linearToRGBComponents(components);
    case ColorSpace::DisplayP3:
        return p3ToSRGB(components);
    }
    ASSERT_NOT_REACHED();
    return { };
}

bool extendedColorsEqual(const Color& a, const Color& b)
{
    if (a.isExtended() && b.isExtended())
        return a.asExtended() == b.asExtended();

    ASSERT(a.isExtended() || b.isExtended());
    return false;
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

    return {
        WebCore::blend(fromSRGB.redComponent(), toSRGB.redComponent(), progress),
        WebCore::blend(fromSRGB.greenComponent(), toSRGB.greenComponent(), progress),
        WebCore::blend(fromSRGB.blueComponent(), toSRGB.blueComponent(), progress),
        WebCore::blend(fromSRGB.alphaComponent(), toSRGB.alphaComponent(), progress)
    };
}

void Color::tagAsValid()
{
    m_colorData.simpleColorAndFlags |= validSimpleColor;
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
