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

#include "Brush.h"
#include "Image.h"
#include "Pen.h"
#include "TextDirection.h"
#include <kxmlcore/Noncopyable.h>

#if __APPLE__
typedef struct CGContext* CGContextRef;
#endif

#if WIN32
typedef struct HDC__ *HDC;
typedef struct _cairo cairo_t;
#endif

class DeprecatedString;
class QChar;

namespace WebCore {

    class Font;
    struct GraphicsContextPrivate;
    class GraphicsContextPlatformPrivate;
    class IntPoint;
    class IntPointArray;

#if SVG_SUPPORT
    class KRenderingDevice;
    class KRenderingDeviceContext;
#endif

    class GraphicsContext : Noncopyable {
    public:
        GraphicsContext();
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
#endif
        GraphicsContext(bool forPrinting);
        ~GraphicsContext();
       
        const Font& font() const;
        void setFont(const Font&);
        
        const Pen& pen() const;
        void setPen(const Pen&);
        void setPen(Pen::PenStyle);
        void setPen(RGBA32);
        
        const Brush& brush() const;
        void setBrush(const Brush&);
        void setBrush(Brush::BrushStyle);
        void setBrush(RGBA32);

        void save();
        void restore();
        
        void drawRect(const IntRect&);
        void drawLine(const IntPoint&, const IntPoint&);
        void drawEllipse(const IntRect&);
        void drawArc(int, int, int, int, int, int);
        void drawConvexPolygon(const IntPointArray&);

        void fillRect(const IntRect&, const Brush&);

        void drawImageAtPoint(Image*, const IntPoint&, Image::CompositeOperator = Image::CompositeSourceOver);
        void drawImageInRect(Image*, const IntRect&, Image::CompositeOperator = Image::CompositeSourceOver);

        void drawImage(Image*, const IntPoint&, 
            int sx = 0, int sy = 0, int sw = -1, int sh = -1,
            Image::CompositeOperator = Image::CompositeSourceOver, 
            void* nativeData = 0);
        void drawImage(Image*, const IntRect&,
            int sx = 0, int sy = 0, int sw = -1, int sh = -1,
            Image::CompositeOperator = Image::CompositeSourceOver, 
            void* nativeData = 0);
        void drawImage(Image*, const FloatRect&,
            float sx = 0, float sy = 0, float sw = -1, float sh = -1,
            Image::CompositeOperator = Image::CompositeSourceOver, 
            void* nativeData = 0);
        void drawTiledImage(Image*, const IntRect& dest, int sx = 0, int sy = 0, void* nativeData = 0);
        void drawScaledAndTiledImage(Image*, const IntRect&, int, int, int, int, 
            Image::TileRule hRule = Image::StretchTile, Image::TileRule vRule = Image::StretchTile,
            void* nativeData = 0);

        void addClip(const IntRect&);
        void addRoundedRectClip(const IntRect&, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight);

        void drawText(int x, int y, int alignmentFlags, const DeprecatedString&);
        void drawHighlightForText(int x, int y, int h, int tabWidth, int xpos,
            const QChar*, int slen, int pos, int len, int toAdd,
            TextDirection d, bool visuallyOrdered,
            int from, int to, const Color& backgroundColor);
        void drawText(int x, int y, int tabWidth, int xpos,
            const QChar*, int slen, int pos, int len, int toAdd,
            TextDirection d = LTR, bool visuallyOrdered = false, int from = -1, int to = -1);
        void drawLineForText(int x, int y, int yOffset, int width);
        void drawLineForMisspelling(int x, int y, int width);
        int misspellingLineThickness() const;

        Color selectedTextBackgroundColor() const;
        void setUsesInactiveTextBackgroundColor(bool);
        bool usesInactiveTextBackgroundColor() const;
        
        bool paintingDisabled() const;
        void setPaintingDisabled(bool);
        
        bool updatingControlTints() const;
        void setUpdatingControlTints(bool);

        void beginTransparencyLayer(float opacity);
        void endTransparencyLayer();

        void setShadow(int x, int y, int blur, const Color&);
        void clearShadow();

        void initFocusRing(int width, int offset);
        void addFocusRingRect(const IntRect&);
        void drawFocusRing(const Color&);
        void clearFocusRing();
    
#if __APPLE__
        static CGContextRef currentCGContext();
        static int getCompositeOperation(CGContextRef);
        static void setCompositeOperation(CGContextRef, const DeprecatedString& operation);
        static void setCompositeOperation(CGContextRef, int operation);
#endif

#if __APPLE__
        CGContextRef platformContext() const { return currentCGContext(); }
#endif
#if WIN32
        cairo_t* platformContext() const;
#endif

#if SVG_SUPPORT
        KRenderingDeviceContext* createRenderingDeviceContext();
        static KRenderingDevice* renderingDevice();
#endif

        bool printing() const;

private:
        void setColorFromBrush();
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
