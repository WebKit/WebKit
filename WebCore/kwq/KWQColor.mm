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

QRgb qRgb(int r, int g, int b)
{
    if (r < 0) r = 0; else if (r > 255) r = 255;
    if (g < 0) g = 0; else if (g > 255) g = 255;
    if (b < 0) b = 0; else if (b > 255) b = 255;
    return r << 16 | g << 8 | b;
}

QColor::QColor(const char *name)
{
    const Color *foundColor = findColor(name, strlen(name));
    color = foundColor ? foundColor->RGBValue : KWQInvalidColor;
}

QString QColor::name() const
{
    QString name;
    name.sprintf("#%02X%02X%02X", red(), green(), blue());
    return name;
}

void QColor::setNamedColor(const QString &name)
{
    const Color *foundColor = name.isAllASCII() ? findColor(name.latin1(), name.length()) : 0;
    color = foundColor ? foundColor->RGBValue : KWQInvalidColor;
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
    unsigned c = color & 0xFFFFFF;
    switch (c) {
        case Qt::black: {
            static NSColor *blackColor = [[NSColor blackColor] retain];
            return blackColor;
        }
        case Qt::white: {
            static NSColor *whiteColor = [[NSColor whiteColor] retain];
            return whiteColor;
        }
        default: {
            const int cacheSize = 32;
            static unsigned cachedRGBValues[cacheSize];
            static NSColor *cachedColors[cacheSize];

            for (int i = 0; i != cacheSize; ++i) {
                if (cachedRGBValues[i] == c) {
                    return cachedColors[i];
                }
            }

            NSColor *result = [NSColor colorWithCalibratedRed:red() / 255.0
                                                        green:green() / 255.0
                                                         blue:blue() / 255.0
                                                        alpha:1.0];

            static int cursor;
            cachedRGBValues[cursor] = c;
            [cachedColors[cursor] autorelease];
            cachedColors[cursor] = [result retain];
            if (++cursor == cacheSize) {
                cursor = 0;
            }

            return result;
        }
    }
}
