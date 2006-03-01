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

#include "Brush.h"
#include "Image.h"
#include "Pen.h"

#if __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#endif

class QFontMetrics;
class QString;

namespace WebCore {

#if SVG_SUPPORT
class KRenderingDevice;
class KRenderingDeviceContext;
#endif

class Font;
class IntPoint;
class IntPointArray;
class QPainterPrivate;

class QPainter : public Qt {
public:
    enum TextDirection { RTL, LTR };

    QPainter();
    QPainter(bool forPrinting);
    ~QPainter();
   
    const Font& font() const;
    void setFont(const Font&);
    QFontMetrics fontMetrics() const;
    
    const Pen& pen() const;
    void setPen(const Pen&);
    void setPen(Pen::PenStyle);
    void setPen(RGBA32);
    
    const Brush& brush() const;
    void setBrush(const Brush&);
    void setBrush(Brush::BrushStyle);
    void setBrush(RGBA32);

    IntRect xForm(const IntRect&) const;

    void save();
    void restore();
    
    void drawRect(int, int, int, int);
    void drawLine(int, int, int, int);
    void drawEllipse(int, int, int, int);
    void drawArc(int, int, int, int, int, int);
    void drawConvexPolygon(const IntPointArray&);

    void fillRect(int, int, int, int, const Brush&);
    void fillRect(const IntRect&, const Brush&);

    void drawImageAtPoint(Image*, const IntPoint&, Image::CompositeOperator = Image::CompositeSourceOver);
    void drawImageInRect(Image*, const IntRect&, Image::CompositeOperator = Image::CompositeSourceOver);

    void drawImage(Image*, int x, int y, 
        int sx = 0, int sy = 0, int sw = -1, int sh = -1,
        Image::CompositeOperator = Image::CompositeSourceOver, 
        void* nativeData = 0);
    void drawImage(Image*, int x, int y, int w, int h,
        int sx = 0, int sy = 0, int sw = -1, int sh = -1,
        Image::CompositeOperator = Image::CompositeSourceOver, 
        void* nativeData = 0);
    void drawFloatImage(Image*, float x, float y, float w, float h,
        float sx = 0, float sy = 0, float sw = -1, float sh = -1,
        Image::CompositeOperator = Image::CompositeSourceOver, 
        void* nativeData = 0);
    void drawTiledImage(Image*, int, int, int, int, int sx = 0, int sy = 0, void* nativeData = 0);
    void drawScaledAndTiledImage(Image*, int, int, int, int, int, int, int, int, 
                                 Image::TileRule hRule = Image::StretchTile, Image::TileRule vRule = Image::StretchTile,
                                 void* nativeData = 0);

    void addClip(const IntRect&);
    void addRoundedRectClip(const IntRect&, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight);

    void drawText(int x, int y, int tabWidth, int xpos, int, int, int alignmentFlags, const QString&);
    void drawHighlightForText(int x, int y, int h, int tabWidth, int xpos,
        const QChar*, int length, int from, int to, int toAdd,
        const Color& backgroundColor, TextDirection, bool visuallyOrdered,
        int letterSpacing, int wordSpacing, bool smallCaps);
    void drawText(int x, int y, int tabWidth, int xpos,
        const QChar*, int length, int from, int to, int toAdd,
        const Color& backgroundColor, TextDirection, bool visuallyOrdered,
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

    void setShadow(int x, int y, int blur, const Color&);
    void clearShadow();

    void initFocusRing(int width, int offset);
    void addFocusRingRect(int x, int y, int width, int height);
    void drawFocusRing(const Color&);
    void clearFocusRing();
    
#if __APPLE__
    CGContextRef currentContext();

    static int getCompositeOperation(CGContextRef);
    static void setCompositeOperation(CGContextRef, const QString& operation);
    static void setCompositeOperation(CGContextRef, int operation);
#endif

#if SVG_SUPPORT
    KRenderingDeviceContext* createRenderingDeviceContext();
    static KRenderingDevice* renderingDevice();
#endif

    bool printing() const { return _isForPrinting; }

private:
    // no copying or assignment
    QPainter(const QPainter&);
    QPainter& operator=(const QPainter&);

    void _setColorFromBrush();
    void _setColorFromPen();

    void _fillRect(float x, float y, float w, float h, const Color&);

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
