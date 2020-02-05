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

RGBA32 makeRGB(int r, int g, int b)
{
    return makeRGBA(r, g, b, 0xFF);
}

RGBA32 makeRGBA(int r, int g, int b, int a)
{
    return { static_cast<unsigned>(std::max(0, std::min(a, 0xFF)) << 24 | std::max(0, std::min(r, 0xFF)) << 16 | std::max(0, std::min(g, 0xFF)) << 8 | std::max(0, std::min(b, 0xFF))) };
}

RGBA32 makePremultipliedRGBA(int r, int g, int b, int a, bool ceiling)
{
    return makeRGBA(premultipliedChannel(r, a, ceiling), premultipliedChannel(g, a, ceiling), premultipliedChannel(b, a, ceiling), a);
}

RGBA32 makeUnPremultipliedRGBA(int r, int g, int b, int a)
{
    return makeRGBA(unpremultipliedChannel(r, a), unpremultipliedChannel(g, a), unpremultipliedChannel(b, a), a);
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

RGBA32 colorWithOverrideAlpha(RGBA32 color, float overrideAlpha)
{
    return { (color.value() & 0x00FFFFFF) | colorFloatToRGBAByte(overrideAlpha) << 24 };
}

RGBA32 makeRGBAFromHSLA(float hue, float saturation, float lightness, float alpha)
{
    const float scaleFactor = 255.0;
    FloatComponents floatResult = HSLToSRGB({ hue, saturation, lightness, alpha });
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
    int c1Red = c1.isExtended() ? c1.asExtended().red() * 255 : c1.red();
    int c1Green = c1.isExtended() ? c1.asExtended().green() * 255 : c1.green();
    int c1Blue = c1.isExtended() ? c1.asExtended().blue() * 255 : c1.blue();
    int c2Red = c2.isExtended() ? c2.asExtended().red() * 255 : c2.red();
    int c2Green = c2.isExtended() ? c2.asExtended().green() * 255 : c2.green();
    int c2Blue = c2.isExtended() ? c2.asExtended().blue() * 255 : c2.blue();
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

Color::Color(float r, float g, float b, float a, ColorSpace colorSpace)
{
    // Zero the union, just in case a 32-bit system only assigns the
    // top 32 bits when copying the extendedColor pointer below.
    m_colorData.rgbaAndFlags = 0;
    auto extendedColorRef = ExtendedColor::create(r, g, b, a, colorSpace);
    m_colorData.extendedColor = &extendedColorRef.leakRef();
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

    float r, g, b, a;
    getRGBA(r, g, b, a);

    float v = std::max(r, std::max(g, b));

    if (v == 0.0f)
        // Lightened black with alpha.
        return Color(0x54, 0x54, 0x54, alpha());

    float multiplier = std::min(1.0f, v + 0.33f) / v;

    return Color(static_cast<int>(multiplier * r * scaleFactor),
                 static_cast<int>(multiplier * g * scaleFactor),
                 static_cast<int>(multiplier * b * scaleFactor),
                 alpha());
}

Color Color::dark() const
{
    // Hardcode this common case for speed.
    if (rgb() == white)
        return darkenedWhite;
    
    const float scaleFactor = nextafterf(256.0f, 0.0f);

    float r, g, b, a;
    getRGBA(r, g, b, a);

    float v = std::max(r, std::max(g, b));
    float multiplier = std::max(0.0f, (v - 0.33f) / v);

    return Color(static_cast<int>(multiplier * r * scaleFactor),
                 static_cast<int>(multiplier * g * scaleFactor),
                 static_cast<int>(multiplier * b * scaleFactor),
                 alpha());
}

bool Color::isDark() const
{
    float red;
    float green;
    float blue;
    float alpha;
    getRGBA(red, green, blue, alpha);
    float largestNonAlphaChannel = std::max(red, std::max(green, blue));
    return alpha > 0.5 && largestNonAlphaChannel < 0.5;
}

static int blendComponent(int c, int a)
{
    // We use white.
    float alpha = a / 255.0f;
    int whiteBlend = 255 - a;
    c -= whiteBlend;
    return static_cast<int>(c / alpha);
}

const int cStartAlpha = 153; // 60%
const int cEndAlpha = 204; // 80%;
const int cAlphaIncrement = 17; // Increments in between.

Color Color::blend(const Color& source) const
{
    if (!isVisible() || source.isOpaque())
        return source;

    if (!source.alpha())
        return *this;

    int d = 255 * (alpha() + source.alpha()) - alpha() * source.alpha();
    int a = d / 255;
    int r = (red() * alpha() * (255 - source.alpha()) + 255 * source.alpha() * source.red()) / d;
    int g = (green() * alpha() * (255 - source.alpha()) + 255 * source.alpha() * source.green()) / d;
    int b = (blue() * alpha() * (255 - source.alpha()) + 255 * source.alpha() * source.blue()) / d;
    return Color(r, g, b, a);
}

Color Color::blendWithWhite() const
{
    // If the color contains alpha already, we leave it alone.
    if (!isOpaque())
        return *this;

    Color newColor;
    for (int alpha = cStartAlpha; alpha <= cEndAlpha; alpha += cAlphaIncrement) {
        // We have a solid color.  Convert to an equivalent color that looks the same when blended with white
        // at the current alpha.  Try using less transparency if the numbers end up being negative.
        int r = blendComponent(red(), alpha);
        int g = blendComponent(green(), alpha);
        int b = blendComponent(blue(), alpha);
        
        newColor = Color(r, g, b, alpha);

        if (r >= 0 && g >= 0 && b >= 0)
            break;
    }

    if (isSemantic())
        newColor.setIsSemantic();
    return newColor;
}

Color Color::colorWithAlphaMultipliedBy(float amount) const
{
    float newAlpha = amount * (isExtended() ? m_colorData.extendedColor->alpha() : static_cast<float>(alpha()) / 255);
    return colorWithAlpha(newAlpha);
}

Color Color::colorWithAlpha(float alpha) const
{
    if (isExtended())
        return Color { m_colorData.extendedColor->red(), m_colorData.extendedColor->green(), m_colorData.extendedColor->blue(), alpha, m_colorData.extendedColor->colorSpace() };

    int newAlpha = alpha * 255; // Why doesn't this use colorFloatToRGBAByte() like colorWithOverrideAlpha()?

    Color result = { red(), green(), blue(), newAlpha };
    if (isSemantic())
        result.setIsSemantic();
    return result;
}

void Color::getRGBA(float& r, float& g, float& b, float& a) const
{
    r = red() / 255.0f;
    g = green() / 255.0f;
    b = blue() / 255.0f;
    a = alpha() / 255.0f;
}

void Color::getRGBA(double& r, double& g, double& b, double& a) const
{
    r = red() / 255.0;
    g = green() / 255.0;
    b = blue() / 255.0;
    a = alpha() / 255.0;
}

// FIXME: Use sRGBToHSL().
void Color::getHSL(double& hue, double& saturation, double& lightness) const
{
    // http://en.wikipedia.org/wiki/HSL_color_space. This is a direct copy of
    // the algorithm therein, although it's 360^o based and we end up wanting
    // [0...6) based. It's clearer if we stick to 360^o until the end.
    double r = static_cast<double>(red()) / 255.0;
    double g = static_cast<double>(green()) / 255.0;
    double b = static_cast<double>(blue()) / 255.0;
    double max = std::max(std::max(r, g), b);
    double min = std::min(std::min(r, g), b);
    double chroma = max - min;

    if (!chroma)
        hue = 0.0;
    else if (max == r)
        hue = (60.0 * ((g - b) / chroma)) + 360.0;
    else if (max == g)
        hue = (60.0 * ((b - r) / chroma)) + 120.0;
    else
        hue = (60.0 * ((r - g) / chroma)) + 240.0;

    if (hue >= 360.0)
        hue -= 360.0;

    // makeRGBAFromHSLA assumes that hue is in [0...6).
    hue /= 60.0;

    lightness = 0.5 * (max + min);
    if (!chroma)
        saturation = 0.0;
    else if (lightness <= 0.5)
        saturation = (chroma / (max + min));
    else
        saturation = (chroma / (2.0 - (max + min)));
}

void Color::getHSV(double& hue, double& saturation, double& value) const
{
    double r = static_cast<double>(red()) / 255.0;
    double g = static_cast<double>(green()) / 255.0;
    double b = static_cast<double>(blue()) / 255.0;
    double max = std::max(std::max(r, g), b);
    double min = std::min(std::min(r, g), b);
    double chroma = max - min;

    if (!chroma)
        hue = 0.0;
    else if (max == r)
        hue = (60.0 * ((g - b) / chroma)) + 360.0;
    else if (max == g)
        hue = (60.0 * ((b - r) / chroma)) + 120.0;
    else
        hue = (60.0 * ((r - g) / chroma)) + 240.0;

    if (hue >= 360.0)
        hue -= 360.0;

    hue /= 360.0;

    if (!max)
        saturation = 0;
    else
        saturation = chroma / max;

    value = max;
}

Color colorFromPremultipliedARGB(RGBA32 pixelColor)
{
    if (pixelColor.isVisible() && !pixelColor.isOpaque())
        return makeUnPremultipliedRGBA(pixelColor.redComponent(), pixelColor.greenComponent(), pixelColor.blueComponent(), pixelColor.alphaComponent());
    return pixelColor;
}

RGBA32 premultipliedARGBFromColor(const Color& color)
{
    if (color.isOpaque()) {
        if (color.isExtended())
            return makeRGB(color.asExtended().red() * 255, color.asExtended().green() * 255, color.asExtended().blue() * 255);
        return color.rgb();
    }

    if (color.isExtended())
        return makePremultipliedRGBA(color.asExtended().red() * 255, color.asExtended().green() * 255, color.asExtended().blue() * 255, color.asExtended().alpha() * 255);

    return makePremultipliedRGBA(color.red(), color.green(), color.blue(), color.alpha());
}

Color blend(const Color& from, const Color& to, double progress, bool blendPremultiplied)
{
    // FIXME: ExtendedColor - needs to handle color spaces.
    // We need to preserve the state of the valid flag at the end of the animation
    if (progress == 1 && !to.isValid())
        return Color();

    if (blendPremultiplied) {
        // Since premultipliedARGBFromColor() bails on zero alpha, special-case that.
        Color premultFrom = from.alpha() ? premultipliedARGBFromColor(from) : Color::transparent;
        Color premultTo = to.alpha() ? premultipliedARGBFromColor(to) : Color::transparent;

        Color premultBlended(blend(premultFrom.red(), premultTo.red(), progress),
            blend(premultFrom.green(), premultTo.green(), progress),
            blend(premultFrom.blue(), premultTo.blue(), progress),
            blend(premultFrom.alpha(), premultTo.alpha(), progress));

        return Color(colorFromPremultipliedARGB(premultBlended.rgb()));
    }

    return Color(blend(from.red(), to.red(), progress),
        blend(from.green(), to.green(), progress),
        blend(from.blue(), to.blue(), progress),
        blend(from.alpha(), to.alpha(), progress));
}

void Color::tagAsValid()
{
    m_colorData.rgbaAndFlags |= validRGBAColor;
}

ExtendedColor& Color::asExtended() const
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
