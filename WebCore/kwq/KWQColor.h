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

#ifndef QCOLOR_H_
#define QCOLOR_H_

#include "KWQString.h"

#ifdef __OBJC__
@class NSColor;
#else
class NSColor;
#endif

typedef unsigned int QRgb;			// RGB triplet

QRgb qRgb(int r, int g, int b);

const QRgb KWQInvalidColor = 0x40000000;

class QColor {
public:
    QColor() : color(KWQInvalidColor) { }
    QColor(QRgb col) : color(col) { }
    QColor(int r, int g, int b) : color(qRgb(r, g, b)) { }
    explicit QColor(const char *);
    
    QString name() const;
    void setNamedColor(const QString&);

    bool isValid() const { return color != KWQInvalidColor; }

    int red() const { return (color >> 16) & 0xFF; }
    int green() const { return (color >> 8) & 0xFF; }
    int blue() const { return color & 0xFF; }
    QRgb rgb() const { return color & 0xFFFFFF; }
    void setRgb(int r, int g, int b) { color = qRgb(r, g, b); }
    void setRgb(int rgb) { color = rgb; }

    void hsv(int *, int *, int *) const;
    void setHsv(int h, int s, int v);

    QColor light(int f = 150) const;
    QColor dark(int f = 200) const;

    friend bool operator==(const QColor &a, const QColor &b);
    friend bool operator!=(const QColor &a, const QColor &b);

    NSColor *getNSColor() const;

private:
    QRgb color;
};

inline bool operator==(const QColor &a, const QColor &b)
{
    return a.color == b.color;
}

inline bool operator!=(const QColor &a, const QColor &b)
{
    return a.color != b.color;
}

#endif
