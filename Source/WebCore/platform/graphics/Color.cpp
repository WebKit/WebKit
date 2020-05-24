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
#include <wtf/HexNumber.h>
#include <wtf/MathExtras.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

static constexpr SimpleColor lightenedBlack { 0xFF545454 };
static constexpr SimpleColor darkenedWhite { 0xFFABABAB };

static inline unsigned premultipliedChannel(unsigned c, unsigned a, bool ceiling = true)
{
    return fastDivideBy255(ceiling ? c * a + 254 : c * a);
}

static inline unsigned unpremultipliedChannel(unsigned c, unsigned a)
{
    return (fastMultiplyBy255(c) + a - 1) / a;
}

RGBA32 makePremultipliedRGBA(int r, int g, int b, int a, bool ceiling)
{
    return makeRGBA(premultipliedChannel(r, a, ceiling), premultipliedChannel(g, a, ceiling), premultipliedChannel(b, a, ceiling), a);
}

RGBA32 makePremultipliedRGBA(RGBA32 pixelColor)
{
    if (pixelColor.isOpaque())
        return pixelColor;
    return makePremultipliedRGBA(pixelColor.redComponent(), pixelColor.greenComponent(), pixelColor.blueComponent(), pixelColor.alphaComponent());
}

RGBA32 makeUnPremultipliedRGBA(int r, int g, int b, int a)
{
    return makeRGBA(unpremultipliedChannel(r, a), unpremultipliedChannel(g, a), unpremultipliedChannel(b, a), a);
}

RGBA32 makeUnPremultipliedRGBA(RGBA32 pixelColor)
{
    if (pixelColor.isVisible() && !pixelColor.isOpaque())
        return makeUnPremultipliedRGBA(pixelColor.redComponent(), pixelColor.greenComponent(), pixelColor.blueComponent(), pixelColor.alphaComponent());
    return pixelColor;
}

static int colorFloatToRGBAByte(float f)
{
    // We use lroundf and 255 instead of nextafterf(256, 0) to match CG's rounding
    return std::max(0, std::min(static_cast<int>(lroundf(255.0f * f)), 255));
}

RGBA32 makeRGBA32FromFloats(float r, float g, float b, float a)
{
    return makeRGBA(colorFloatToRGBAByte(r), colorFloatToRGBAByte(g), colorFloatToRGBAByte(b), colorFloatToRGBAByte(a));
}

RGBA32 makeRGBAFromHSLA(float hue, float saturation, float lightness, float alpha)
{
    const float scaleFactor = 255.0;
    FloatComponents floatResult = hslToSRGB({ hue, saturation, lightness, alpha });
    return makeRGBA(
        round(floatResult.components[0] * scaleFactor),
        round(floatResult.components[1] * scaleFactor),
        round(floatResult.components[2] * scaleFactor),
        round(floatResult.components[3] * scaleFactor));
}

RGBA32 makeRGBAFromCMYKA(float c, float m, float y, float k, float a)
{
    double colors = 1 - k;
    int r = static_cast<int>(nextafter(256, 0) * (colors * (1 - c)));
    int g = static_cast<int>(nextafter(256, 0) * (colors * (1 - m)));
    int b = static_cast<int>(nextafter(256, 0) * (colors * (1 - y)));
    return makeRGBA(r, g, b, static_cast<float>(nextafter(256, 0) * a));
}

// originally moved here from the CSS parser
template <typename CharacterType>
static inline bool parseHexColorInternal(const CharacterType* name, unsigned length, RGBA32& rgb)
{
    if (length != 3 && length != 4 && length != 6 && length != 8)
        return false;
    unsigned value = 0;
    for (unsigned i = 0; i < length; ++i) {
        if (!isASCIIHexDigit(name[i]))
            return false;
        value <<= 4;
        value |= toASCIIHexValue(name[i]);
    }
    if (length == 6) {
        rgb = { 0xFF000000 | value };
        return true;
    }
    if (length == 8) {
        // We parsed the values into RGBA order, but the RGBA32 type
        // expects them to be in ARGB order, so we right rotate eight bits.
        rgb = { value << 24 | value >> 8 };
        return true;
    }
    if (length == 4) {
        // #abcd converts to ddaabbcc in RGBA32.
        rgb = { (value & 0xF) << 28 | (value & 0xF) << 24
            | (value & 0xF000) << 8 | (value & 0xF000) << 4
            | (value & 0xF00) << 4 | (value & 0xF00)
            | (value & 0xF0) | (value & 0xF0) >> 4 };
        return true;
    }
    // #abc converts to #aabbcc
    rgb = { 0xFF000000
        | (value & 0xF00) << 12 | (value & 0xF00) << 8
        | (value & 0xF0) << 8 | (value & 0xF0) << 4
        | (value & 0xF) << 4 | (value & 0xF) };
    return true;
}

bool Color::parseHexColor(const LChar* name, unsigned length, RGBA32& rgb)
{
    return parseHexColorInternal(name, length, rgb);
}

bool Color::parseHexColor(const UChar* name, unsigned length, RGBA32& rgb)
{
    return parseHexColorInternal(name, length, rgb);
}

bool Color::parseHexColor(const String& name, RGBA32& rgb)
{
    unsigned length = name.length();
    if (!length)
        return false;
    if (name.is8Bit())
        return parseHexColor(name.characters8(), name.length(), rgb);
    return parseHexColor(name.characters16(), name.length(), rgb);
}

bool Color::parseHexColor(const StringView& name, RGBA32& rgb)
{
    unsigned length = name.length();
    if (!length)
        return false;
    if (name.is8Bit())
        return parseHexColor(name.characters8(), name.length(), rgb);
    return parseHexColor(name.characters16(), name.length(), rgb);
}

int differenceSquared(const Color& c1, const Color& c2)
{
    // FIXME: This is assuming that the colors are in the same colorspace.
    // FIXME: This should probably return a floating point number, but many of the call
    // sites have picked comparison values based on feel. We'd need to break out
    // our logarithm tables to change them :)
    int c1Red = c1.isExtended() ? c1.asExtended().red() * 255 : c1.rgb().redComponent();
    int c1Green = c1.isExtended() ? c1.asExtended().green() * 255 : c1.rgb().greenComponent();
    int c1Blue = c1.isExtended() ? c1.asExtended().blue() * 255 : c1.rgb().blueComponent();
    int c2Red = c2.isExtended() ? c2.asExtended().red() * 255 : c2.rgb().redComponent();
    int c2Green = c2.isExtended() ? c2.asExtended().green() * 255 : c2.rgb().greenComponent();
    int c2Blue = c2.isExtended() ? c2.asExtended().blue() * 255 : c2.rgb().blueComponent();
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
        RGBA32 color;
        bool valid;

        if (name.is8Bit())
            valid = parseHexColor(name.characters8() + 1, name.length() - 1, color);
        else
            valid = parseHexColor(name.characters16() + 1, name.length() - 1, color);

        if (valid)
            setRGB(color);
    } else {
        if (auto* foundColor = findNamedColor(name))
            setRGB({ foundColor->ARGBValue });
    }
}

Color::Color(const char* name)
{
    if (name[0] == '#') {
        SimpleColor color;
        if (parseHexColor(reinterpret_cast<const LChar*>(&name[1]), std::strlen(&name[1]), color))
            setRGB(color);
    } else if (auto* foundColor = findColor(name, strlen(name)))
        setRGB({ foundColor->ARGBValue });
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
    // top 32 bits when copying the extendedColor pointer below.
    m_colorData.rgbaAndFlags = 0;
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
    other.m_colorData.rgbaAndFlags = invalidRGBAColor;

    return *this;
}

String SimpleColor::serializationForHTML() const
{
    if (isOpaque())
        return makeString('#', hex(redComponent(), 2, Lowercase), hex(greenComponent(), 2, Lowercase), hex(blueComponent(), 2, Lowercase));
    return serializationForCSS();
}

String Color::serialized() const
{
    if (isExtended())
        return asExtended().cssText();
    return rgb().serializationForHTML();
}

static char decimalDigit(unsigned number)
{
    ASSERT(number < 10);
    return '0' + number;
}

static std::array<char, 4> fractionDigitsForFractionalAlphaValue(uint8_t alpha)
{
    ASSERT(alpha > 0);
    ASSERT(alpha < 0xFF);
    if (((alpha * 100 + 0x7F) / 0xFF * 0xFF + 50) / 100 != alpha)
        return { { decimalDigit(alpha * 10 / 0xFF % 10), decimalDigit(alpha * 100 / 0xFF % 10), decimalDigit((alpha * 1000 + 0x7F) / 0xFF % 10), '\0' } };
    if (int thirdDigit = (alpha * 100 + 0x7F) / 0xFF % 10)
        return { { decimalDigit(alpha * 10 / 0xFF), decimalDigit(thirdDigit), '\0', '\0' } };
    return { { decimalDigit((alpha * 10 + 0x7F) / 0xFF), '\0', '\0', '\0' } };
}

String SimpleColor::serializationForCSS() const
{
    switch (alphaComponent()) {
    case 0:
        return makeString("rgba(", redComponent(), ", ", greenComponent(), ", ", blueComponent(), ", 0)");
    case 0xFF:
        return makeString("rgb(", redComponent(), ", ", greenComponent(), ", ", blueComponent(), ')');
    default:
        return makeString("rgba(", redComponent(), ", ", greenComponent(), ", ", blueComponent(), ", 0.", fractionDigitsForFractionalAlphaValue(alphaComponent()).data(), ')');
    }
}

String Color::cssText() const
{
    if (isExtended())
        return asExtended().cssText();
    return rgb().serializationForCSS();
}

String RGBA32::serializationForRenderTreeAsText() const
{
    if (alphaComponent() < 0xFF)
        return makeString('#', hex(redComponent(), 2), hex(greenComponent(), 2), hex(blueComponent(), 2), hex(alphaComponent(), 2));
    return makeString('#', hex(redComponent(), 2), hex(greenComponent(), 2), hex(blueComponent(), 2));
}

String Color::nameForRenderTreeAsText() const
{
    // FIXME: Handle extended colors.
    return rgb().serializationForRenderTreeAsText();
}

Color Color::light() const
{
    // Hardcode this common case for speed.
    if (rgb() == black)
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
    if (rgb() == white)
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

    return makeRGBA(r, g, b, a);
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
        
        result = makeRGBA(r, g, b, alpha);

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
    if (isExtended()) {
        auto& extendedColor = asExtended();
        return Color { extendedColor.red(), extendedColor.green(), extendedColor.blue(), alpha, extendedColor.colorSpace() };
    }

    // FIXME: This is where this function differs from colorWithAlphaUsingAlternativeRounding.
    uint8_t newAlpha = alpha * 0xFF;

    Color result = rgb().colorWithAlpha(newAlpha);
    if (isSemantic())
        result.setIsSemantic();
    return result;
}

Color Color::colorWithAlphaUsingAlternativeRounding(float alpha) const
{
    if (isExtended()) {
        auto& extendedColor = asExtended();
        return Color { extendedColor.red(), extendedColor.green(), extendedColor.blue(), alpha, extendedColor.colorSpace() };
    }

    // FIXME: This is where this function differs from colorWithAlphaUsing.
    uint8_t newAlpha = colorFloatToRGBAByte(alpha);

    Color result = rgb().colorWithAlpha(newAlpha);
    if (isSemantic())
        result.setIsSemantic();
    return result;
}

Color Color::invertedColorWithAlpha(float alpha) const
{
    if (isExtended())
        return Color { asExtended().invertedColorWithAlpha(alpha) };

    auto [r, g, b, existingAlpha] = rgb();
    return { 0xFF - r, 0xFF - g, 0xFF - b, colorFloatToRGBAByte(alpha) };
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

    auto [r, g, b, a] = rgb();
    return { ColorSpace::SRGB, FloatComponents { r / 255.0f, g / 255.0f, b / 255.0f,  a / 255.0f } };
}

SimpleColor Color::toSRGBASimpleColorLossy() const
{
    if (!isExtended())
        return rgb();

    auto [r, g, b, a] = toSRGBAComponentsLossy();
    return makeRGBA32FromFloats(r, g, b, a);
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
        return Color();

    // Since makePremultipliedRGBA() bails on zero alpha, special-case that.
    auto premultFrom = from.alpha() ? makePremultipliedRGBA(from.toSRGBASimpleColorLossy()) : Color::transparent;
    auto premultTo = to.alpha() ? makePremultipliedRGBA(to.toSRGBASimpleColorLossy()) : Color::transparent;

    RGBA32 premultBlended = makeRGBA(
        WebCore::blend(premultFrom.redComponent(), premultTo.redComponent(), progress),
        WebCore::blend(premultFrom.greenComponent(), premultTo.greenComponent(), progress),
        WebCore::blend(premultFrom.blueComponent(), premultTo.blueComponent(), progress),
        WebCore::blend(premultFrom.alphaComponent(), premultTo.alphaComponent(), progress)
    );

    return makeUnPremultipliedRGBA(premultBlended);
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
    m_colorData.rgbaAndFlags |= validRGBAColor;
}

const ExtendedColor& Color::asExtended() const
{
    ASSERT(isExtended());
    return *m_colorData.extendedColor;
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
