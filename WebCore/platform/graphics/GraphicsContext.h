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
#include "Path.h"
#include "Pen.h"
#include "TextDirection.h"
#include <wtf/Noncopyable.h>
#include <wtf/Platform.h>

#if PLATFORM(CG)
typedef struct CGContext PlatformGraphicsContext;
#elif PLATFORM(CAIRO)
typedef struct _cairo PlatformGraphicsContext;
#elif PLATFORM(QT)
class QPainter;
typedef QPainter PlatformGraphicsContext;
#else
typedef void PlatformGraphicsContext;
#endif

#if PLATFORM(WIN)
typedef struct HDC__* HDC;
#endif

namespace WebCore {

    const int cMisspellingLineThickness = 3;
    const int cMisspellingLinePatternWidth = 4;
    const int cMisspellingLinePatternGapWidth = 1;

    class AffineTransform;
    class Font;
    class GraphicsContextPrivate;
    class GraphicsContextPlatformPrivate;
    class KURL;
    class Path;
    class TextRun;
    class TextStyle;

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
        void drawArc(const IntRect&, float thickness, int startAngle, int angleSpan);
        void drawConvexPolygon(size_t numPoints, const FloatPoint*, bool shouldAntialias = false);

        void fillRect(const IntRect&, const Color&);
        void fillRect(const FloatRect&, const Color&);
        void clearRect(const FloatRect&);
        void strokeRect(const FloatRect&, float lineWidth);

        void drawImage(Image*, const IntPoint&, CompositeOperator = CompositeSourceOver);
        void drawImage(Image*, const IntRect&, CompositeOperator = CompositeSourceOver);
        void drawImage(Image*, const IntPoint& destPoint, const IntRect& srcRect, CompositeOperator = CompositeSourceOver);
        void drawImage(Image*, const IntRect& destRect, const IntRect& srcRect, CompositeOperator = CompositeSourceOver);
        void drawImage(Image*, const FloatRect& destRect, const FloatRect& srcRect = FloatRect(0, 0, -1, -1),
            CompositeOperator = CompositeSourceOver);
        void drawTiledImage(Image*, const IntRect& destRect, const IntPoint& srcPoint, const IntSize& tileSize,
            CompositeOperator = CompositeSourceOver);
        void drawTiledImage(Image*, const IntRect& destRect, const IntRect& srcRect, 
            Image::TileRule hRule = Image::StretchTile, Image::TileRule vRule = Image::StretchTile,
            CompositeOperator = CompositeSourceOver);

        void clip(const IntRect&);
        void addRoundedRectClip(const IntRect&, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight);
        void addInnerRoundedRectClip(const IntRect&, int thickness);

        // Functions to work around bugs in focus ring clipping on Mac.
        void setFocusRingClip(const IntRect&);
        void clearFocusRingClip();

        void drawText(const TextRun&, const IntPoint&);
        void drawText(const TextRun&, const IntPoint&, const TextStyle&);
        void drawHighlightForText(const TextRun&, const IntPoint&, int h, const TextStyle&, const Color& backgroundColor);

        FloatRect roundToDevicePixels(const FloatRect&);
        
        void drawLineForText(const IntPoint&, int yOffset, int width, bool printing);
        void drawLineForMisspellingOrBadGrammar(const IntPoint&, int width, bool grammar);
        
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
        IntRect focusRingBoundingRect();

        void setLineWidth(float);
        void setLineCap(LineCap);
        void setLineJoin(LineJoin);
        void setMiterLimit(float);

        void setAlpha(float);

        void setCompositeOperation(CompositeOperator);

        void beginPath();
        void addPath(const Path& path);

        void clip(const Path&);

        void scale(const FloatSize&);
        void rotate(float angleInRadians);
        void translate(float x, float y);
        IntPoint origin();
        
        void setURLForRect(const KURL&, const IntRect&);

        void concatCTM(const AffineTransform&);

#if PLATFORM(WIN)
        GraphicsContext(HDC); // FIXME: To be removed.
        HDC getWindowsContext(bool supportAlphaBlend = false, const IntRect* = 0); // The passed in rect is used to create a bitmap for compositing inside transparency layers.
        void releaseWindowsContext(HDC, bool supportAlphaBlend = false, const IntRect* = 0);    // The passed in HDC should be the one handed back by getWindowsContext.
#endif

#if PLATFORM(QT)
        void setFillRule(WindRule);
        PlatformPath* currentPath();
#endif

    private:
        void savePlatformState();
        void restorePlatformState();
        void setPlatformPen(const Pen& pen);
        void setPlatformFillColor(const Color& fill);
        void setPlatformFont(const Font& font);

        int focusRingWidth() const;
        int focusRingOffset() const;
        const Vector<IntRect>& focusRingRects() const;

        static GraphicsContextPrivate* createGraphicsContextPrivate();
        static void destroyGraphicsContextPrivate(GraphicsContextPrivate*);

        GraphicsContextPrivate* m_common;
        GraphicsContextPlatformPrivate* m_data;
    };

#ifdef SVG_SUPPORT
    class SVGResourceImage;
    GraphicsContext* contextForImage(SVGResourceImage*);
#endif

} // namespace WebCore

#endif // GraphicsContext_h
