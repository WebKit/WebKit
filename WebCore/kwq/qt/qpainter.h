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

#ifndef QPAINTER_H_
#define QPAINTER_H_

#include <qnamespace.h>
#include <qpaintdevice.h>
#include <qcolor.h>
#include <qpen.h>
#include <qbrush.h>
#include <qrect.h>
#include <qregion.h>
#include <qpoint.h>
#include <qstring.h>
#include <qfontmetrics.h>

class QFont;
class QPixmap;
class QWidget;
class QPainterPrivate;

class QWMatrix {
friend class QPainter;
friend class QPixmap;
public:
    QWMatrix();
    QWMatrix &scale(double, double);

private:
    bool empty;
    double sx;
    double sy;
};

class QPainter : public Qt {
public:
    typedef enum { RTL, LTR } TextDirection;

    QPainter();
    QPainter(QPixmap *);
    QPainter(QWidget *);
    ~QPainter();
    
    const QFont &font() const;
    void setFont(const QFont &);
    QFontMetrics fontMetrics() const;
    const QPen &pen() const;
    void setPen(const QPen &);
    void setPen(PenStyle);
    const QBrush &QPainter::brush() const;
    void setBrush(const QBrush &);
    void setBrush(BrushStyle);

    QRect xForm(const QRect &) const;

    void save();
    void restore();
    
    void drawRect(int, int, int, int);
    void drawLine(int, int, int, int);
    void drawLineSegments(const QPointArray &, int index=0, int nlines=-1);
    void drawEllipse(int, int, int, int);
    void drawArc(int, int, int, int, int, int);
    void drawPolyline(const QPointArray &, int index=0, int npoints=-1);
    void drawPolygon(const QPointArray &, bool winding=FALSE, int index=0,
        int npoints=-1);
    void drawPixmap(const QPoint &, const QPixmap &);
    void drawPixmap(const QPoint &, const QPixmap &, const QRect &);
    void drawPixmap( int x, int y, const QPixmap &,
			    int sx=0, int sy=0, int sw=-1, int sh=-1 );
    void drawTiledPixmap(int, int, int, int, const QPixmap &, int sx=0, int sy=0);
    void drawText(int x, int y, const QString &, int len=-1);
    void drawText(int, int, int, int, int flags, const QString&, int len=-1, 
        QRect *br=0, char **internal=0);
    void drawText(int, int, const QString &, int, TextDirection);
    void drawText(int, int, const QString &, int, int, TextDirection);
    void drawText(int, int, const QString &, int, int, const QColor& backgroundColor);

    void fillRect(int, int, int, int, const QBrush &);
    void drawConvexPolygon(const QPointArray &);

    void setClipping(bool);
    void setClipRegion(const QRegion &);
    void setClipRect(const QRect &);
    void setClipRect(int,int,int,int);
    const QRegion &clipRegion() const;
    bool hasClipping() const;
    RasterOp rasterOp() const;
    void setRasterOp(RasterOp);

    void translate(double dx, double dy);
    void scale(double dx, double dy);

    void begin(QPixmap *) { }
    void end() { }
    QPaintDevice *device() const { return 0; }

    void drawUnderlineForText(int x, int y, const QString &, int len=-1);

    static QColor selectedTextBackgroundColor();
    
private:
    // no copying or assignment
    // note that these are "standard" (no pendantic stuff needed)
    QPainter(const QPainter &);
    QPainter &operator=(const QPainter &);

    void _lockFocus();
    void _unlockFocus();

    void _setColorFromBrush();
    void _setColorFromPen();

    void _drawPoints(const QPointArray &_points, bool winding, int index, int _npoints, bool fill);

    QPainterPrivate *data;
};

#endif
