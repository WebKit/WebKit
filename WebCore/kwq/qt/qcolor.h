/*
 * Copyright (C) 2001, 2002 Apple Computer, Inc.  All rights reserved.
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

#include <qstring.h>

#ifdef __OBJC__
@class NSColor;
#else
class NSColor;
#endif

typedef unsigned int QRgb;			// RGB triplet

QRgb qRgb(int r, int g, int b);
QRgb qRgba(int r, int g, int b, int a);

class QColor {
public:
    QColor();
    QColor(int,int,int);
    explicit QColor(const QString &);
    QColor(const char *); // can't be explicit because of helper.cpp
    QColor(const QColor &);

    ~QColor();

    QString name() const;
    void setNamedColor(const QString&);

    bool isValid() const;

    int red() const;
    int green() const;
    int blue() const;
    QRgb rgb() const;
    void setRgb(int,int,int);
    void setRgb(int);

    void hsv(int *, int *, int *) const;
    void setHsv(int h, int s, int v);

    QColor light(int f = 150) const;
    QColor dark(int f = 200) const;

    QColor &operator=(const QColor &);
    bool operator==(const QColor &x) const;
    bool operator!=(const QColor &x) const;

    NSColor *getNSColor() const;

private:
    NSColor *color;
};

#endif
