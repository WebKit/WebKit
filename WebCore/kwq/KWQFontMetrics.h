/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#include "IntRect.h"
#include "IntSize.h"
#include "QString.h"

class QFont;
class QFontMetricsPrivate;

class QFontMetrics {
public:
    QFontMetrics();
    QFontMetrics(const QFont &);
    ~QFontMetrics();

    QFontMetrics(const QFontMetrics &);
    QFontMetrics &operator=(const QFontMetrics &);

    const QFont &font() const;
    void setFont(const QFont &);
    
    int ascent() const;
    int descent() const;
    int height() const;
    int lineSpacing() const;
    float xHeight() const;
    
    int width(const QString &, int tabWidth, int xpos, int len=-1) const;
    int width(const QChar *, int len, int tabWidth, int xpos) const;
    float floatWidth(const QChar *, int slen, int pos, int len, int tabWidth, int xpos, int letterSpacing, int wordSpacing, bool smallCaps) const;
    IntRect selectionRectForText(int x, int y, int h, int tabWidth, int xpos,
                            const QChar *str, int len, int from, int to, int toAdd,
                            bool rtl, bool visuallyOrdered, int letterSpacing,
                            int wordSpacing, bool smallCaps) const;
    int checkSelectionPoint(QChar *s, int slen, int pos, int len, int toAdd, int tabWidth, int xpos, int letterSpacing, int wordSpacing, bool smallCaps, int x, bool reversed, bool dirOverride, bool includePartialGlyphs) const;

    IntRect boundingRect(QChar) const;
    IntRect boundingRect(const QString &, int tabWidth, int xpos, int len=-1) const;
    IntRect boundingRect(int, int, int, int, int, const QString &, int tabWidth, int xpos) const;

    IntSize size(int, const QString &, int tabWidth, int xpos) const;

    int baselineOffset() const { return ascent(); }
    
private:
#ifndef WIN32_COMPILE_HACK
    RefPtr<QFontMetricsPrivate> data;
#endif
};

#endif

