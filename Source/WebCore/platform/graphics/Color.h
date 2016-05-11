/*
 * Copyright (C) 2003-2006, 2010, 2015 Apple Inc. All rights reserved.
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

#ifndef Color_h
#define Color_h

#include "ColorSpace.h"
#include "PlatformExportMacros.h"
#include <algorithm>
#include <cmath>
#include <unicode/uchar.h>
#include <wtf/Forward.h>
#include <wtf/Optional.h>
#include <wtf/text/LChar.h>

#if USE(CG)
typedef struct CGColor* CGColorRef;
#endif

#if PLATFORM(GTK)
typedef struct _GdkColor GdkColor;
#ifndef GTK_API_VERSION_2
typedef struct _GdkRGBA GdkRGBA;
#endif
#endif

namespace WebCore {

class TextStream;

typedef unsigned RGBA32; // Deprecated: Type for an RGBA quadruplet. Use RGBA class instead.

WEBCORE_EXPORT RGBA32 makeRGB(int r, int g, int b);
WEBCORE_EXPORT RGBA32 makeRGBA(int r, int g, int b, int a);

WEBCORE_EXPORT RGBA32 colorWithOverrideAlpha(RGBA32 color, float overrideAlpha);
WEBCORE_EXPORT RGBA32 colorWithOverrideAlpha(RGBA32 color, Optional<float> overrideAlpha);

WEBCORE_EXPORT RGBA32 makeRGBA32FromFloats(float r, float g, float b, float a);
RGBA32 makeRGBAFromHSLA(double h, double s, double l, double a);
RGBA32 makeRGBAFromCMYKA(float c, float m, float y, float k, float a);

inline int redChannel(RGBA32 color) { return (color >> 16) & 0xFF; }
inline int greenChannel(RGBA32 color) { return (color >> 8) & 0xFF; }
inline int blueChannel(RGBA32 color) { return color & 0xFF; }
inline int alphaChannel(RGBA32 color) { return (color >> 24) & 0xFF; }

uint8_t roundAndClampColorChannel(int);
uint8_t roundAndClampColorChannel(float);

class RGBA {
public:
    RGBA(); // all channels zero, including alpha
    RGBA(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha);
    RGBA(uint8_t red, uint8_t green, uint8_t blue); // opaque, alpha of 1

    uint8_t red() const;
    uint8_t green() const;
    uint8_t blue() const;
    uint8_t alpha() const;

    bool hasAlpha() const;

private:
    friend class Color;

    unsigned m_integer { 0 };
};

bool operator==(const RGBA&, const RGBA&);
bool operator!=(const RGBA&, const RGBA&);

class Color {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Color() : m_color(0), m_valid(false) { }
    Color(RGBA, ColorSpace);

    // FIXME: Remove all these constructors and creation functions and replace the ones that are still needed with free functions.
    Color(RGBA32 color, bool valid = true) : m_color(color), m_valid(valid) { ASSERT(!m_color || m_valid); }
    Color(int r, int g, int b) : m_color(makeRGB(r, g, b)), m_valid(true) { }
    Color(int r, int g, int b, int a) : m_color(makeRGBA(r, g, b, a)), m_valid(true) { }
    // Color is currently limited to 32bit RGBA, perhaps some day we'll support better colors
    Color(float r, float g, float b, float a) : m_color(makeRGBA32FromFloats(r, g, b, a)), m_valid(true) { }
    // Creates a new color from the specific CMYK and alpha values.
    Color(float c, float m, float y, float k, float a) : m_color(makeRGBAFromCMYKA(c, m, y, k, a)), m_valid(true) { }
    WEBCORE_EXPORT explicit Color(const String&);
    explicit Color(const char*);
    static Color createUnchecked(int r, int g, int b)
    {
        RGBA32 color = 0xFF000000 | r << 16 | g << 8 | b;
        return Color(color);
    }
    static Color createUnchecked(int r, int g, int b, int a)
    {
        RGBA32 color = a << 24 | r << 16 | g << 8 | b;
        return Color(color);
    }

    // Returns the color serialized according to HTML5
    // <https://html.spec.whatwg.org/multipage/scripting.html#fill-and-stroke-styles> (10 September 2015)
    WEBCORE_EXPORT String serialized() const;

    String cssText() const;

    // Returns the color serialized as either #RRGGBB or #RRGGBBAA
    // The latter format is not a valid CSS color, and should only be seen in DRT dumps.
    String nameForRenderTreeAsText() const;

    void setNamedColor(const String&);

    // FIXME: Remove this after moving clients to all use OptionalColor instead.
    bool isValid() const { return m_valid; }

    bool hasAlpha() const { return alpha() < 255; }

    int red() const { return redChannel(m_color); }
    int green() const { return greenChannel(m_color); }
    int blue() const { return blueChannel(m_color); }
    int alpha() const { return alphaChannel(m_color); }
    
    RGBA32 rgb() const { return m_color; } // Preserve the alpha.
    void setRGB(int r, int g, int b) { m_color = makeRGB(r, g, b); m_valid = true; }
    void setRGB(RGBA32 rgb) { m_color = rgb; m_valid = true; }
    WEBCORE_EXPORT void getRGBA(float& r, float& g, float& b, float& a) const;
    WEBCORE_EXPORT void getRGBA(double& r, double& g, double& b, double& a) const;
    WEBCORE_EXPORT void getHSL(double& h, double& s, double& l) const;
    WEBCORE_EXPORT void getHSV(double& h, double& s, double& v) const;

    Color light() const;
    Color dark() const;

    bool isDark() const;

    // This is an implementation of Porter-Duff's "source-over" equation
    Color blend(const Color&) const;
    Color blendWithWhite() const;

#if PLATFORM(GTK)
    Color(const GdkColor&);
    // We can't sensibly go back to GdkColor without losing the alpha value
#ifndef GTK_API_VERSION_2
    Color(const GdkRGBA&);
    operator GdkRGBA() const;
#endif
#endif

#if USE(CG)
    WEBCORE_EXPORT Color(CGColorRef);
#endif

    static bool parseHexColor(const String&, RGBA32&);
    static bool parseHexColor(const LChar*, unsigned, RGBA32&);
    static bool parseHexColor(const UChar*, unsigned, RGBA32&);

    static const RGBA32 black = 0xFF000000;
    WEBCORE_EXPORT static const RGBA32 white = 0xFFFFFFFF;
    static const RGBA32 darkGray = 0xFF808080;
    static const RGBA32 gray = 0xFFA0A0A0;
    static const RGBA32 lightGray = 0xFFC0C0C0;
    WEBCORE_EXPORT static const RGBA32 transparent = 0x00000000;
    static const RGBA32 cyan = 0xFF00FFFF;

#if PLATFORM(IOS)
    static const RGBA32 compositionFill = 0x3CAFC0E3;
#else
    static const RGBA32 compositionFill = 0xFFE1DD55;
#endif

private:
    RGBA32 m_color;
    bool m_valid;
};

class OptionalColor : public Color {
public:
    OptionalColor();
    OptionalColor(const Color&);

    explicit operator bool() const;

private:
    // FIXME: Change to use Optional<Color>?
    // FIXME: Convert all callers to use Optional<Color>?
    bool m_isEngaged;
    Color m_color;
};

bool operator==(const Color&, const Color&);
bool operator!=(const Color&, const Color&);

Color colorFromPremultipliedARGB(RGBA32);
RGBA32 premultipliedARGBFromColor(const Color&);

Color blend(const Color& from, const Color& to, double progress, bool blendPremultiplied = true);

int differenceSquared(const Color&, const Color&);

uint16_t fastDivideBy255(uint16_t);

#if USE(CG)
WEBCORE_EXPORT CGColorRef cachedCGColor(const Color&);
#endif

inline RGBA::RGBA()
{
}

inline RGBA::RGBA(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
    : m_integer(alpha << 24 | red << 16 | green << 8 | blue)
{
}

inline RGBA::RGBA(uint8_t red, uint8_t green, uint8_t blue)
    : m_integer(0xFF000000 | red << 16 | green << 8 | blue)
{
}

inline uint8_t RGBA::red() const
{
    return m_integer >> 16;
}

inline uint8_t RGBA::green() const
{
    return m_integer >> 8;
}

inline uint8_t RGBA::blue() const
{
    return m_integer;
}

inline uint8_t RGBA::alpha() const
{
    return m_integer >> 24;
}

inline bool RGBA::hasAlpha() const
{
    return (m_integer & 0xFF000000) != 0xFF000000;
}

inline Color::Color(RGBA color, ColorSpace space)
    : m_color(color.m_integer)
    , m_valid(true)
{
    ASSERT_UNUSED(space, space == ColorSpaceSRGB);
}

inline bool operator==(const Color& a, const Color& b)
{
    return a.rgb() == b.rgb() && a.isValid() == b.isValid();
}

inline bool operator!=(const Color& a, const Color& b)
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

inline uint16_t fastDivideBy255(uint16_t value)
{
    // While this is an approximate algorithm for division by 255, it gives perfectly accurate results for 16-bit values.
    // FIXME: Since this gives accurate results for 16-bit values, we should get this optimization into compilers like clang.
    uint16_t approximation = value >> 8;
    uint16_t remainder = value - (approximation * 255) + 1;
    return approximation + (remainder >> 8);
}

inline RGBA32 colorWithOverrideAlpha(RGBA32 color, Optional<float> overrideAlpha)
{
    return overrideAlpha ? colorWithOverrideAlpha(color, overrideAlpha.value()) : color;
}

WEBCORE_EXPORT TextStream& operator<<(TextStream&, const Color&);

} // namespace WebCore

#endif // Color_h
