/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "Color.h"
#include "Pen.h"
#include "Brush.h"
#include "KWQFontMetrics.h"
#include "KWQNamespace.h"
#include "IntRect.h"

#if __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#endif

class QFont;
class QString;
class QWidget;

#if SVG_SUPPORT
class KRenderingDevice;
class KRenderingDeviceContext;
#endif

namespace WebCore {

class Image;
class QPainterPrivate;

class QPainter : public Qt {
public:
    enum TextDirection { RTL, LTR };
    enum TileRule { STRETCH, ROUND, REPEAT };

    QPainter();
    QPainter(bool forPrinting);
    ~QPainter();
   
    const QFont &font() const;
    void setFont(const QFont &);
    QFontMetrics fontMetrics() const;
    
    const Pen &pen() const;
    void setPen(const Pen &);
    void setPen(Pen::PenStyle);
    void setPen(RGBA32);
    
    const WebCore::Brush &QPainter::brush() const;
    void setBrush(const WebCore::Brush &);
    void setBrush(WebCore::Brush::BrushStyle);
    void setBrush(RGBA32);

    IntRect xForm(const IntRect &) const;

    void save();
    void restore();
    
    void drawRect(int, int, int, int);
    void drawLine(int, int, int, int);
    void drawEllipse(int, int, int, int);
    void drawArc(int, int, int, int, int, int);
    void drawConvexPolygon(const IntPointArray &);

    void fillRect(int, int, int, int, const WebCore::Brush &);
    void fillRect(const IntRect &, const WebCore::Brush &);

    void drawImage(const IntPoint &, const Image &);
    void drawImage(const IntPoint &, const Image &, const IntRect &);
    void drawImage(const IntPoint &, const Image &, const IntRect &, const QString &);
#if __APPLE__
    void drawImage( int x, int y, const Image &,
                     int sx=0, int sy=0, int sw=-1, int sh=-1, int compositeOperator=-1, CGContextRef context=0);
    void drawImage( int x, int y, int w, int h, const Image &,
                      int sx=0, int sy=0, int sw=-1, int sh=-1, int compositeOperator=-1, CGContextRef context=0);
    void drawFloatImage( float x, float y, float w, float h, const Image &,
                          float sx=0, float sy=0, float sw=-1, float sh=-1, int compositeOperator=-1, CGContextRef context=0);
    void drawTiledImage(int, int, int, int, const Image &, int sx=0, int sy=0, CGContextRef context=0);
    void drawScaledAndTiledImage(int, int, int, int, const Image &, int, int, int, int, TileRule hRule = STRETCH, TileRule vRule = STRETCH,
                                  CGContextRef context=0);
#else
    void drawImage( int x, int y, const Image &,
                     int sx=0, int sy=0, int sw=-1, int sh=-1, int compositeOperator=-1);
    void drawImage( int x, int y, int w, int h, const Image &,
                     int sx=0, int sy=0, int sw=-1, int sh=-1, int compositeOperator=-1);
    void drawFloatImage( float x, float y, float w, float h, const Image &,
                          float sx=0, float sy=0, float sw=-1, float sh=-1, int compositeOperator=-1);
    void drawTiledImage(int, int, int, int, const Image &, int sx=0, int sy=0);
    void drawScaledAndTiledImage(int, int, int, int, const Image &, int, int, int, int, TileRule hRule = STRETCH, TileRule vRule = STRETCH);
#endif

    void addClip(const IntRect &);
    void addRoundedRectClip(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight,
                            const IntSize& bottomLeft, const IntSize& bottomRight);

    RasterOp rasterOp() const;
    void setRasterOp(RasterOp);

    void drawText(int x, int y, int tabWidth, int xpos, int, int, int alignmentFlags, const QString &);
    void drawHighlightForText(int x, int y, int h, int tabWidth, int xpos,
                  const QChar *, int length, int from, int to, int toAdd,
                  const Color& backgroundColor, QPainter::TextDirection d, bool visuallyOrdered,
                  int letterSpacing, int wordSpacing, bool smallCaps);
    void drawText(int x, int y, int tabWidth, int xpos, const QChar *, int length, int from, int to, int toAdd,
                  const Color& backgroundColor, QPainter::TextDirection d, bool visuallyOrdered,
                  int letterSpacing, int wordSpacing, bool smallCaps);
    void drawLineForText(int x, int y, int yOffset, int width);
    void drawLineForMisspelling(int x, int y, int width);
    int misspellingLineThickness() const;

    Color selectedTextBackgroundColor() const;
    void setUsesInactiveTextBackgroundColor(bool u) { _usesInactiveTextBackgroundColor = u; }
    
    bool paintingDisabled() const;
    void setPaintingDisabled(bool);
    
    bool updatingControlTints() const { return _updatingControlTints; }
    void setUpdatingControlTints(bool b) { setPaintingDisabled(b); _updatingControlTints = b; }

    void beginTransparencyLayer(float opacity);
    void endTransparencyLayer();

    void setShadow(int x, int y, int blur, const Color& color);
    void clearShadow();

    void initFocusRing(int width, int offset);
    void initFocusRing(int width, int offset, const Color& color);
    void addFocusRingRect(int x, int y, int width, int height);
    void drawFocusRing();
    void clearFocusRing();
    
#if __APPLE__
    CGContextRef currentContext();
#endif

#if SVG_SUPPORT
    KRenderingDeviceContext *createRenderingDeviceContext();
    static KRenderingDevice *renderingDevice();
#endif
    
#if __APPLE__
    static int compositeOperatorFromString (const QString &aString);
    static int getCompositeOperation(CGContextRef context);
    static void setCompositeOperation (CGContextRef context, const QString &operation);
    static void setCompositeOperation (CGContextRef context, int operation);
#endif

    bool printing() const { return _isForPrinting; }

private:
    // no copying or assignment
    QPainter(const QPainter &);
    QPainter &operator=(const QPainter &);

    void _setColorFromBrush();
    void _setColorFromPen();

    void _fillRect(float x, float y, float w, float h, const Color& color);
    
    void _updateRenderer();

    QPainterPrivate *data;
    bool _isForPrinting;
    bool _usesInactiveTextBackgroundColor;
    bool _updatingControlTints;
};

}

// FIXME: Remove when everything is in the WebCore namespace.
using WebCore::QPainter;

#endif
