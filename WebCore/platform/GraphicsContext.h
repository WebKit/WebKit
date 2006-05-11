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

#ifndef GraphicsContext_h
#define GraphicsContext_h

#include "FloatRect.h"
#include "Image.h"
#include "IntRect.h"
#include "Pen.h"
#include "TextDirection.h"
#include <wtf/Noncopyable.h>
#include <unicode/umachine.h>

#if __APPLE__
typedef struct CGContext PlatformGraphicsContext;
#elif WIN32
typedef struct HDC__* HDC;
typedef struct _cairo PlatformGraphicsContext;
#else
typedef void PlatformGraphicsContext;
#endif

class DeprecatedString;

namespace WebCore {

    class Font;
    class GraphicsContextPrivate;
    class GraphicsContextPlatformPrivate;
    class IntPoint;
    class IntPointArray;
    class Path;
    class TextRun;

#if SVG_SUPPORT
    class KRenderingDeviceContext;
#endif

    class GraphicsContext : Noncopyable {
    public:
        GraphicsContext(PlatformGraphicsContext*);
        ~GraphicsContext();
       
        PlatformGraphicsContext* platformContext() const;

        const Font& font() const;
        void setFont(const Font&);
        
        const Pen& pen() const;
        void setPen(const Pen&);
        void setPen(Pen::PenStyle);
        void setPen(RGBA32);
        
        Color fillColor() const;
        void setFillColor(const Color&);

        void save();
        void restore();
        
        void drawRect(const IntRect&);
        void drawLine(const IntPoint&, const IntPoint&);
        void drawEllipse(const IntRect&);
        void drawArc(int, int, int, int, int, int);
        void drawConvexPolygon(const IntPointArray&);

        void fillRect(const IntRect&, const Color&);
        void clearRect(const FloatRect&);
        void strokeRect(const FloatRect&, float lineWidth);

        void drawImage(Image*, const IntPoint&, CompositeOperator = CompositeSourceOver);
        void drawImage(Image*, const IntRect&, CompositeOperator = CompositeSourceOver);
        void drawImage(Image*, const IntPoint& destPoint, const IntRect& srcRect, CompositeOperator = CompositeSourceOver);
        void drawImage(Image*, const IntRect& destRect, const IntRect& srcRect, CompositeOperator = CompositeSourceOver);
        void drawImage(Image*, const FloatRect& destRect, const FloatRect& srcRect = FloatRect(0, 0, -1, -1),
            CompositeOperator = CompositeSourceOver);
        void drawTiledImage(Image*, const IntRect& destRect, const IntPoint& srcPoint, const IntSize& tileSize);
        void drawTiledImage(Image*, const IntRect& destRect, const IntRect& srcRect, 
            Image::TileRule hRule = Image::StretchTile, Image::TileRule vRule = Image::StretchTile);

        void addClip(const IntRect&);
        void addRoundedRectClip(const IntRect&, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight);

        // Functions to work around bugs in focus ring clipping on Mac.
        void setFocusRingClip(const IntRect&);
        void clearFocusRingClip();

        void drawText(const TextRun&, const IntPoint&, int tabWidth = 0, int xpos = 0, int toAdd = 0,
                      TextDirection = LTR, bool visuallyOrdered = false);
        void drawHighlightForText(const TextRun&, const IntPoint&, int h, 
                                  int tabWidth, int xpos, int toAdd, TextDirection, bool visuallyOrdered, const Color& backgroundColor);
                                  
        
        void drawLineForText(const IntPoint&, int yOffset, int width);
        void drawLineForMisspelling(const IntPoint&, int width);
        int misspellingLineThickness();

        Color selectedTextBackgroundColor() const;
        void setUsesInactiveTextBackgroundColor(bool);
        bool usesInactiveTextBackgroundColor() const;
        
        bool paintingDisabled() const;
        void setPaintingDisabled(bool);
        
        bool updatingControlTints() const;
        void setUpdatingControlTints(bool);

        void beginTransparencyLayer(float opacity);
        void endTransparencyLayer();

        void setShadow(const IntSize&, int blur, const Color&);
        void clearShadow();

        void initFocusRing(int width, int offset);
        void addFocusRingRect(const IntRect&);
        void drawFocusRing(const Color&);
        void clearFocusRing();

        void setLineWidth(float);
        void setLineCap(LineCap);
        void setLineJoin(LineJoin);
        void setMiterLimit(float);

        void setAlpha(float);

        void setCompositeOperation(CompositeOperator);

        void clip(const Path&);

        void scale(const FloatSize&);
        void rotate(float angleInRadians);
        void translate(const FloatSize&);

#if SVG_SUPPORT
        KRenderingDeviceContext* createRenderingDeviceContext();
#endif

#if WIN32
        GraphicsContext(HDC); // FIXME: To be removed.
#endif

    private:
        void savePlatformState();
        void restorePlatformState();

        int focusRingWidth() const;
        int focusRingOffset() const;
        const Vector<IntRect>& focusRingRects() const;

        static GraphicsContextPrivate* createGraphicsContextPrivate();
        static void destroyGraphicsContextPrivate(GraphicsContextPrivate*);

        GraphicsContextPrivate* m_common;
        GraphicsContextPlatformPrivate* m_data;
    };

}

#endif
