/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import "KWQColor.h"

#import "KWQNamespace.h"
#import "KWQString.h"
#import "KWQAssertions.h"

// Turn off inlining to avoid warning with newer gcc.
#undef __inline
#define __inline
#import "KWQColorData.c"
#undef __inline

const QColor Qt::black    (0x00, 0x00, 0x00);
const QColor Qt::white    (0xFF, 0xFF, 0xFF);
const QColor Qt::darkGray (0x80, 0x80, 0x80);
const QColor Qt::gray     (0xA0, 0xA0, 0xA0);
const QColor Qt::lightGray(0xC0, 0xC0, 0xC0);
const QColor Qt::red      (0xFF, 0x00, 0x00);
const QColor Qt::green    (0x00, 0xFF, 0x00);
const QColor Qt::blue     (0x00, 0x00, 0xFF);
const QColor Qt::cyan     (0x00, 0xFF, 0xFF);
const QColor Qt::magenta  (0xFF, 0x00, 0xFF);
const QColor Qt::yellow   (0xFF, 0xFF, 0x00);

QRgb qRgb(int r, int g, int b)
{
    return r << 16 | g << 8 | b;
}

QRgb qRgba(int r, int g, int b, int a)
{
    return a << 24 | r << 16 | g << 8 | b;
}

QColor::QColor(const QString &name)
{
    setNamedColor(name);
}

QColor::QColor(const char *name)
{
    setNamedColor(name);
}

QString QColor::name() const
{
    QString name;
    name.sprintf("#%02X%02X%02X", red(), green(), blue());
    return name;
}

void QColor::setNamedColor(const QString &name)
{
    // FIXME: The combination of this code with the code that
    // is in khtml/misc/helper.cpp makes setting colors by
    // name a real crock. We need to look at the process
    // of mapping names to colors and figure out something
    // better.
    // 
    // [kocienda: 2001-11-08]: I've made some improvements
    // but it's still a crock.
    
    if (name.isEmpty()) {
        color = KWQInvalidColor;
        return;
    } 
    
    QString lowerName = name.lower();
    const Color *foundColor = findColor(name.latin1(), name.length());
    if (foundColor) {
        int RGBValue = foundColor->RGBValue;
        setRgb((RGBValue >> 16) & 0xFF, (RGBValue >> 8) & 0xFF, RGBValue & 0xFF);
        return;
    }

    ERROR("couldn't create color using name %s", name.ascii());
    color = KWQInvalidColor;
}

void QColor::hsv(int *h, int *s, int *v) const
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

void QColor::setHsv(int h, int s, int v)
{
    int i, f, p, q, t;
    
    if( s == 0 ) {
        // achromatic (gray)
        setRgb(v, v, v);
        return;
    }
    
    h /= 60;			// sector 0 to 5
    i = (int)floor(h);
    f = h - i;			// factorial part of h
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
        default:		// case 5:
            setRgb(v, p, q);
            break;
    }
}


QColor QColor::light(int factor) const
{
    if (factor <= 0) {
        return QColor(*this);
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

    QColor result;
    
    result.setHsv(h, s, v);
    
    return result;
}

QColor QColor::dark(int factor) const
{
    if (factor <= 0) {
        return QColor(*this);
    }
    else if (factor < 100) {
        // NOTE: this is actually a lighten operation
        return light(10000 / factor); 
    }

    int h, s, v;
    
    hsv(&h, &s, &v);
    v = (v * 100) / factor;

    QColor result;
    
    result.setHsv(h, s, v);
    
    return result;
}

NSColor *QColor::getNSColor() const
{
    return [NSColor colorWithCalibratedRed:red() / 255.0
                                     green:green() / 255.0
                                      blue:blue() / 255.0
                                     alpha:1.0];
}
