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
#include "qpen.h"
#include "qbrush.h"
#include "qregion.h"
#include "qpoint.h"
#include "qstring.h"
#include "qfontmetrics.h"

class QFont;
class QPixmap;

// class QWMatrix ==============================================================

class QWMatrix {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------
    
    // constructors, copy constructors, and destructors ------------------------
    
    QWMatrix();
    
    ~QWMatrix();

    // member functions --------------------------------------------------------

    QWMatrix &scale(double, double);

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

}; // class QWMatrix ===========================================================


// class QPainter ==============================================================

class QPainter : public Qt {
public:

    // typedefs ----------------------------------------------------------------
    // enums -------------------------------------------------------------------
    // constants ---------------------------------------------------------------
    // static member functions -------------------------------------------------

    // constructors, copy constructors, and destructors ------------------------

    QPainter();
    QPainter(const QPaintDevice *);
    
    ~QPainter();
    
    // member functions --------------------------------------------------------

    const QFont &font() const;
    void setFont(const QFont &);
    QFontMetrics fontMetrics() const;
    const QPen &pen() const;
    void setPen(const QPen &);
    void setPen(PenStyle);
    void setBrush(const QBrush &);
    void setBrush(BrushStyle);

    QRect xForm(const QRect &) const;

    void save();
    void restore();
    
    void drawRect(int, int, int, int);
    void drawLine(int, int, int, int);
    void drawEllipse(int, int, int, int);
    void drawArc(int, int, int, int, int, int);
    void drawPolyline(const QPointArray &, int index=0, int npoints=-1);
    void drawPolygon(const QPointArray &, bool winding=FALSE, int index=0,
        int npoints=-1);
    void drawPixmap(const QPoint &, const QPixmap &);
    void drawPixmap(const QPoint &, const QPixmap &, const QRect &);
    void drawTiledPixmap(int, int, int, int, const QPixmap &, int sx=0, 
        int sy=0);
    void drawText(int x, int y, const QString &, int len=-1);
    void drawText(int, int, int, int, AlignmentFlags, const QString &);
    void drawText(int, int, int, int, int flags, const QString&, int len=-1, 
        QRect *br=0, char **internal=0);
    void fillRect(int, int, int, int, const QBrush &);

    void setClipping(bool);
    void setClipRegion(const QRegion &);
    const QRegion &clipRegion() const;
    bool hasClipping() const;
    RasterOp rasterOp() const;
    void setRasterOp(RasterOp);

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------

// private ---------------------------------------------------------------------

private:
    // no copying or assignment
    QPainter(const QPainter &);
    QPainter &operator=(const QPainter &);

}; // end class QPainter

// =============================================================================

#endif
