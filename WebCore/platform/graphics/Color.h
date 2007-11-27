/*
 * Copyright (C) 2003-6 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#include <wtf/Platform.h>

#if PLATFORM(CG)
typedef struct CGColor* CGColorRef;
#endif

#if PLATFORM(QT)
class QColor;
#endif

#if PLATFORM(GTK)
typedef struct _GdkColor GdkColor;
#endif

#if PLATFORM(WX)
class wxColour;
#endif

namespace WebCore {

class String;
class Color;

typedef unsigned RGBA32;        // RGBA quadruplet

RGBA32 makeRGB(int r, int g, int b);
RGBA32 makeRGBA(int r, int g, int b, int a);
RGBA32 makeRGBAFromHSLA(double h, double s, double l, double a);

int differenceSquared(const Color&, const Color&);

class Color {
public:
    Color() : m_color(0), m_valid(false) { }
    Color(RGBA32 col) : m_color(col), m_valid(true) { }
    Color(int r, int g, int b) : m_color(makeRGB(r, g, b)), m_valid(true) { }
    Color(int r, int g, int b, int a) : m_color(makeRGBA(r, g, b, a)), m_valid(true) { }
    explicit Color(const String&);
    explicit Color(const char*);
    
    String name() const;
    void setNamedColor(const String&);

    bool isValid() const { return m_valid; }

    bool hasAlpha() const { return alpha() < 255; }

    int red() const { return (m_color >> 16) & 0xFF; }
    int green() const { return (m_color >> 8) & 0xFF; }
    int blue() const { return m_color & 0xFF; }
    int alpha() const { return (m_color >> 24) & 0xFF; }
    
    RGBA32 rgb() const { return m_color; } // Preserve the alpha.
    void setRGB(int r, int g, int b) { m_color = makeRGB(r, g, b); m_valid = true; }
    void setRGB(RGBA32 rgb) { m_color = rgb; m_valid = true; }
    void getRGBA(float& r, float& g, float& b, float& a) const;
    void getRGBA(double& r, double& g, double& b, double& a) const;

    Color light() const;
    Color dark() const;

    Color blend(const Color&) const;
    Color blendWithWhite() const;

#if PLATFORM(QT)
    Color(const QColor&);
    operator QColor() const;
#endif

#if PLATFORM(GTK)
    Color(const GdkColor&);
    // We can't sensibly go back to GdkColor without losing the alpha value
#endif

#if PLATFORM(WX)
    Color(const wxColour&);
    operator wxColour() const;
#endif

#if PLATFORM(CG)
    Color(CGColorRef);
#endif

    static bool parseHexColor(const String& name, RGBA32& rgb);

    static const RGBA32 black = 0xFF000000;
    static const RGBA32 white = 0xFFFFFFFF;
    static const RGBA32 darkGray = 0xFF808080;
    static const RGBA32 gray = 0xFFA0A0A0;
    static const RGBA32 lightGray = 0xFFC0C0C0;
    static const RGBA32 transparent = 0x00000000;

private:
    RGBA32 m_color;
    bool m_valid : 1;
};

inline bool operator==(const Color& a, const Color& b)
{
    return a.rgb() == b.rgb() && a.isValid() == b.isValid();
}

inline bool operator!=(const Color& a, const Color& b)
{
    return !(a == b);
}

Color focusRingColor();
void setFocusRingColorChangeFunction(void (*)());

#if PLATFORM(CG)
CGColorRef cgColor(const Color&);
#endif

} // namespace WebCore

#endif // Color_h
