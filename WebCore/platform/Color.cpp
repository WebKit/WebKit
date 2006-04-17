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

#include "config.h"
#include "Color.h"

#include "PlatformString.h"
#include <kxmlcore/Assertions.h>

#include "ColorData.c"

using namespace std;

namespace WebCore {

RGBA32 makeRGB(int r, int g, int b)
{
    return 0xFF000000 | max(0, min(r, 255)) << 16 | max(0, min(g, 255)) << 8 | max(0, min(b, 255));
}

RGBA32 makeRGBA(int r, int g, int b, int a)
{
    return max(0, min(a, 255)) << 24 | max(0, min(r, 255)) << 16 | max(0, min(g, 255)) << 8 | max(0, min(b, 255));
}

// originally moved here from the CSS parser
static inline bool parseHexColor(const String& name, RGBA32 &rgb)
{
    int len = name.length();
    if (!len)
        return false;

    if (len == 3 || len == 6) {
        bool ok;
        int val = name.deprecatedString().toInt(&ok, 16);
        if (ok) {
            if (len == 6) {
                rgb = (0xff << 24) | val;
                return true;
            }
            if (len == 3) {
                // #abc converts to #aabbcc according to the specs
                rgb = (0xff << 24)
                    | (val & 0xf00) << 12 | (val & 0xf00) << 8
                    | (val & 0xf0) << 8 | (val & 0xf0) << 4
                    | (val & 0xf) << 4 | (val & 0xf);
                return true;
            }
        }
    }
    return false;
}

Color::Color(const String& name)
{
    if (name.startsWith("#"))
        valid = parseHexColor(name.substring(1), color);
    else
        setNamedColor(name);
}

Color::Color(const char* name)
{
    if (name[0] == '#')
        valid = parseHexColor(&name[1], color);
    else {
        const NamedColor* foundColor = findColor(name, strlen(name));
        color = foundColor ? foundColor->RGBValue : 0;
        color |= 0xFF000000;
        valid = foundColor;
    }
}

String Color::name() const
{
    String name;
    if (alpha() < 0xFF)
        name = String::sprintf("#%02X%02X%02X%02X", red(), green(), blue(), alpha());
    else
        name = String::sprintf("#%02X%02X%02X", red(), green(), blue());
    return name;
}

void Color::setNamedColor(const String& name)
{
    DeprecatedString dname = name.deprecatedString();
    const NamedColor* foundColor = dname.isAllASCII() ? findColor(dname.latin1(), dname.length()) : 0;
    color = foundColor ? foundColor->RGBValue : 0;
    color |= 0xFF000000;
    valid = foundColor;
}

void Color::hsv(int *h, int *s, int *v) const
{
    int r = red(); 
    int g = green(); 
    int b = blue(); 
    int i, w, x, f;
        
    x = w = r;
    
    if (g > x) {
        x = g;
    } 
    if (g < w) {
        w = g;
    }
    
    if (b > x) {
        x = b;
    } 
    if (b < w) {
        w = b;
    }
  
    if (w == x) {
        *h = -1;
        *s = 0;
        *v = w;
    }
    else {
        f = (r == x) ? g - b : ((g == x) ? b - r : r - g); 
        i = (r == x) ? 3 : ((g == x) ? 5 : 1); 
        *h = i - f /(w - x);
        if (w != 0)
            *s = (w - x)/w;
        else
            *s = 0;
        *v = w; 
    }
}

void Color::setHsv(int h, int s, int v)
{
    int i, f, p, q, t;
    
    if( s == 0 ) {
        // achromatic (gray)
        setRgb(v, v, v);
        return;
    }
    
    // FIXME: The math here is totally wrong.  Why did it floor an int?
    h /= 60;                    // sector 0 to 5
    i = h;
    f = h - i;                  // factorial part of h
    p = v * (1 - s);
    q = v * (1 - s * f);
    t = v * (1 - s * (1 - f));
    
    switch( i ) {
        case 0:
            setRgb(v, t, p);
            break;
        case 1:
            setRgb(q, v, p);
            break;
        case 2:
            setRgb(p, v, t);
            break;
        case 3:
            setRgb(p, q, v);
            break;
        case 4:
            setRgb(t, p, v);
            break;
        default:                // case 5:
            setRgb(v, p, q);
            break;
    }
}


Color Color::light(int factor) const
{
    if (factor <= 0) {
        return Color(*this);
    }
    else if (factor < 100) {
        // NOTE: this is actually a darken operation
        return dark(10000 / factor); 
    }

    int h, s, v;
    
    hsv(&h, &s, &v);
    v = (factor * v) / 100;

    if (v > 255) {
        s -= (v - 255);
        if (s < 0) {
            s = 0;
        }
        v = 255;
    }

    Color result;
    
    result.setHsv(h, s, v);
    
    return result;
}

Color Color::dark(int factor) const
{
    if (factor <= 0) {
        return Color(*this);
    }
    else if (factor < 100) {
        // NOTE: this is actually a lighten operation
        return light(10000 / factor); 
    }

    int h, s, v;
    
    hsv(&h, &s, &v);
    v = (v * 100) / factor;

    Color result;
    
    result.setHsv(h, s, v);
    
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

}
