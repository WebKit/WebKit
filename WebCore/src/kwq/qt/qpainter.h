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

#ifndef QPAINTER_H_
#define QPAINTER_H_

#include "qnamespace.h"
#include "qpaintdevice.h"
#include "qcolor.h"
#include "qbrush.h"
#include "qpen.h"
#include "qregion.h"

class QFont;
class QFontMetrics;
class QPixmap;

class QPainter : public Qt {
public:
    QPainter();
    QPainter(const QPaintDevice *);
    
    void setFont(const QFont &);
    QFontMetrics fontMetrics() const;
    const QPen &pen() const;
    void setPen(const QPen &);
    void setBrush(BrushStyle);

    QRect xForm(const QRect &) const;

    void drawRect( int x, int y, int w, int h );
    void fillRect(int x, int y, int w, int h, const QBrush &);
    void drawTiledPixmap(int x, int y, int w, int h, const QPixmap &, int sx=0, int sy=0);
    void setClipping(bool);
    void setClipRegion(const QRegion &);
    const QRegion &clipRegion() const;
    bool hasClipping() const;

};

#endif
