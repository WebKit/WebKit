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

#ifndef QFONTMETRICS_H_
#define QFONTMETRICS_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "qrect.h"
#include "qsize.h"
#include "qstring.h"
#include "qfont.h"
#include "qfontinfo.h"

#if (defined(__APPLE__) && defined(__OBJC__) && defined(__cplusplus))
#import <Cocoa/Cocoa.h>
#endif

class QFontMetricsPrivate;

// class QFontMetrics ==========================================================

class QFontMetrics {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QFontMetrics();
    QFontMetrics(const QFont &);
    QFontMetrics(const QFontMetrics &);
    ~QFontMetrics();

    // operators ---------------------------------------------------------------

    QFontMetrics &operator=(const QFontMetrics &);

    // member functions --------------------------------------------------------

    int ascent() const;
    int height() const;
    int width(QChar) const;
    int width(char) const;
    int width(const QString &, int len=-1) const;
#if (defined(__APPLE__))
    int _width (CFStringRef string) const;
    int _width (const UniChar *uchars, int len) const;
#endif

    int descent() const;
    QRect boundingRect(const QString &, int len=-1) const;
    QRect boundingRect(QChar) const;
    QRect boundingRect(int, int, int, int, int, const QString &) const;

    QSize size(int, const QString &, int len=-1, int tabstops=0, 
        int *tabarray=0, char **intern=0 ) const;
    int rightBearing(QChar) const;
    int leftBearing(QChar) const;
    int baselineOffset() const;
    
private:
    KWQRefPtr<QFontMetricsPrivate> data;

}; // class QFontMetrics =======================================================

#endif

