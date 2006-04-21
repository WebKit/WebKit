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
#include <kxmlcore/Noncopyable.h>

#if __APPLE__
#if __OBJC__
@class NSGraphicsContext;
#else
class NSGraphicsContext;
#endif
typedef struct CGContext* CGContextRef;
#endif

#if WIN32
typedef struct HDC__* HDC;
typedef struct _cairo cairo_t;
#endif

class DeprecatedString;
class QChar;

namespace WebCore {

    class Font;
    class GraphicsContextPrivate;
    class GraphicsContextPlatformPrivate;
    class IntPoint;
    class IntPointArray;

#if SVG_SUPPORT
    class KRenderingDeviceContext;
#endif

#if __APPLE__
    CGContextRef currentCGContext();
    void setCompositeOperation(CGContextRef, CompositeOperator);
#endif

    class GraphicsContext : Noncopyable {
    public:
        ~GraphicsContext();
       
        const Font& font() const;
        void setFont(const Font&);
        
        const Pen& pen() const;
        void setPen(const Pen&);
        void setPen(Pen::PenStyle);
        void setPen(RGBA32);
        
        const RGBA32 fillColor() const;
        void setFillColor(RGBA32);

        void save();
        void restore();
        
        void drawRect(const IntRect&);
        void drawLine(const IntPoint&, const IntPoint&);
        void drawEllipse(const IntRect&);
        void drawArc(int, int, int, int, int, int);
        void drawConvexPolygon(const IntPointArray&);

        void fillRect(const IntRect&, const Color&);

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

        void drawText(const IntPoint&, int alignmentFlags, const DeprecatedString&);
        void drawHighlightForText(const IntPoint&, int h, int tabWidth, int xpos,
            const QChar*, int slen, int pos, int len, int toAdd,
            TextDirection, bool visuallyOrdered,
            int from, int to, const Color& backgroundColor);
        void drawText(const IntPoint&, int tabWidth, int xpos,
            const QChar*, int slen, int pos, int len, int toAdd,
            TextDirection = LTR, bool visuallyOrdered = false, int from = -1, int to = -1);
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

        bool printing() const;

#if SVG_SUPPORT
        KRenderingDeviceContext* createRenderingDeviceContext();
#endif

#if __APPLE__
        GraphicsContext(NSGraphicsContext*);
        GraphicsContext(CGContextRef, bool flipText, bool forPrinting);
        CGContextRef platformContext() const;
#endif

#if WIN32
        // It's possible to use GetDC to grab the current context from
        // an HWND; however, we currently require clients to pass in the
        // Device Context handle directly.  Printing will also
        // eventually require clients to pass some sort of printer-info
        // struct to that we can CreateDC the printer device correctly.
        GraphicsContext(HDC);

        // This is temporarily public to allow Spinneret to do double-buffering.
        // Long term, we should get the GraphicsContext from the FrameView
        // and then have a blit() method on the FrameView.
        GraphicsContext(cairo_t* context);

        cairo_t* platformContext() const;
#endif

    private:
        void setColorFromFillColor();
        void setColorFromPen();
        
        void savePlatformState();
        void restorePlatformState();
        
        int focusRingWidth() const;
        int focusRingOffset() const;
        const Vector<IntRect>& focusRingRects() const;
        
        static GraphicsContextPrivate* createGraphicsContextPrivate(bool isForPrinting = false);
        static void destroyGraphicsContextPrivate(GraphicsContextPrivate*);
        
        GraphicsContextPrivate* m_common;
        GraphicsContextPlatformPrivate* m_data;
    };

}

#endif
