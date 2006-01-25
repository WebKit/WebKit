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

#include "KWQString.h"
#include <kxmlcore/Assertions.h>

// Turn off inlining to avoid warning with newer gcc.
#undef __inline
#define __inline
#include "ColorData.c"
#undef __inline

namespace WebCore {

RGBA32 makeRGB(int r, int g, int b)
{
    if (r < 0) r = 0; else if (r > 255) r = 255;
    if (g < 0) g = 0; else if (g > 255) g = 255;
    if (b < 0) b = 0; else if (b > 255) b = 255;
    return (r << 16 | g << 8 | b) | 0xFF000000;
}

RGBA32 makeRGBA(int r, int g, int b, int a)
{
    if (r < 0) r = 0; else if (r > 255) r = 255;
    if (g < 0) g = 0; else if (g > 255) g = 255;
    if (b < 0) b = 0; else if (b > 255) b = 255;
    if (a < 0) a = 0; else if (a > 255) a = 255;
    return (a << 24 | r << 16 | g << 8 | b);
}

// copied from css/cssparser.h
static inline bool parseHexColor(const QString &name, RGBA32 &rgb)
{
    int len = name.length();
    
    if ( !len )
        return false;
    bool ok;
    
    if ( len == 3 || len == 6 ) {
        int val = name.toInt(&ok, 16);
        if ( ok ) {
            if (len == 6) {
                rgb =  (0xff << 24) | val;
                return true;
            }
            else if ( len == 3 ) {
                // #abc converts to #aabbcc according to the specs
                rgb = (0xff << 24) |
                (val&0xf00)<<12 | (val&0xf00)<<8 |
                (val&0xf0)<<8 | (val&0xf0)<<4 |
                (val&0xf)<<4 | (val&0xf);
                return true;
            }
        }
    }
    return false;
}

Color::Color(const QString &name) {
    if(name.startsWith("#")) {
        valid = parseHexColor(name.mid(1), color);
    } else {
        const NamedColor *foundColor = findColor(name.ascii(), name.length());
        color = foundColor ? foundColor->RGBValue : 0;
        color |= 0xFF000000;
        valid = foundColor;
    }
}

Color::Color(const char *name)
{
    if(name[0] == '#') {
        valid = parseHexColor(QString(name).mid(1), color);
    } else {
        const NamedColor *foundColor = findColor(name, strlen(name));
        color = foundColor ? foundColor->RGBValue : 0;
        color |= 0xFF000000;
        valid = foundColor;
    }
}

QString Color::name() const
{
    QString name;
    if (alpha() < 0xFF)
        name.sprintf("#%02X%02X%02X%02X", red(), green(), blue(), alpha());
    else
        name.sprintf("#%02X%02X%02X", red(), green(), blue());
    return name;
}

void Color::setNamedColor(const QString &name)
{
    const NamedColor *foundColor = name.isAllASCII() ? findColor(name.latin1(), name.length()) : 0;
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

void Color::getRgbaF(float *r, float *g, float *b, float *a) const
{
    *r = (float)red() / 255.0;
    *g = (float)green() / 255.0;
    *b = (float)blue() / 255.0;
    *a = (float)alpha() / 255.0;
}

}
