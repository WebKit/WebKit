/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <qnamespace.h>
#include <qstring.h>

#if (defined(__APPLE__) && defined(__OBJC__) && defined(__cplusplus))
#define Fixed MacFixed
#define Rect MacRect
#define Boolean MacBoolean
#import <Cocoa/Cocoa.h>
#undef Fixed
#undef Rect
#undef Boolean
#endif


typedef unsigned int QRgb;			// RGB triplet

QRgb qRgb(int r, int g, int b);
QRgb qRgba(int r, int g, int b, int a);

// class QColor ================================================================

class QColor {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------

    QColor();
    QColor(int,int,int);
    QColor(const QString &);
    QColor(const char *);
    QColor(const QColor &);

    ~QColor();

    // member functions --------------------------------------------------------

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

    QColor light(int f = 150) const;
    QColor dark(int f = 200) const;

    // operators ---------------------------------------------------------------

    QColor &operator=(const QColor &);
    bool operator==(const QColor &x) const;
    bool operator!=(const QColor &x) const;

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------
    static bool globals_init;

    void initGlobalColors();

#ifdef _KWQ_
    void _initialize(int,int,int);
    
#if (defined(__APPLE__) && defined(__OBJC__) && defined(__cplusplus))
    NSColor *getNSColor();
#else
    void *getNSColor();
#endif

#if (defined(__APPLE__) && defined(__OBJC__) && defined(__cplusplus))
    NSColor *color;
#else
    void *color;
#endif
    QString cname;
#endif

}; // class QColor =============================================================

#endif
