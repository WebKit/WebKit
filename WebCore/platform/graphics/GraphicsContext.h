/*
 * Copyright (C) 2003, 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008-2009 Torch Mobile, Inc.
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

#include "ColorSpace.h"
#include "DashArray.h"
#include "FloatRect.h"
#include "Image.h"
#include "IntRect.h"
#include "Path.h"
#include "TextDirection.h"
#include <wtf/Noncopyable.h>
#include <wtf/Platform.h>

#if PLATFORM(CG)
typedef struct CGContext PlatformGraphicsContext;
#elif PLATFORM(CAIRO)
typedef struct _cairo PlatformGraphicsContext;
#elif PLATFORM(QT)
QT_BEGIN_NAMESPACE
class QPainter;
QT_END_NAMESPACE
typedef QPainter PlatformGraphicsContext;
#elif PLATFORM(WX)
class wxGCDC;
class wxWindowDC;

// wxGraphicsContext allows us to support Path, etc.
// but on some platforms, e.g. Linux, it requires fairly
// new software.
#if USE(WXGC)
// On OS X, wxGCDC is just a typedef for wxDC, so use wxDC explicitly to make
// the linker happy.
#ifdef __APPLE__
    class wxDC;
    typedef wxDC PlatformGraphicsContext;
#else
    typedef wxGCDC PlatformGraphicsContext;
#endif
#else
    typedef wxWindowDC PlatformGraphicsContext;
#endif
#elif PLATFORM(SKIA)
typedef class PlatformContextSkia PlatformGraphicsContext;
#elif PLATFORM(HAIKU)
class BView;
typedef BView PlatformGraphicsContext;
struct pattern;
#elif PLATFORM(WINCE)
typedef struct HDC__ PlatformGraphicsContext;
#else
typedef void PlatformGraphicsContext;
#endif

#if PLATFORM(GTK)
typedef struct _GdkDrawable GdkDrawable;
typedef struct _GdkEventExpose GdkEventExpose;
#endif

#if PLATFORM(WIN)
typedef struct HDC__* HDC;
#if !PLATFORM(CG)
// UInt8 is defined in CoreFoundation/CFBase.h
typedef unsigned char UInt8;
#endif
#endif

#if PLATFORM(QT) && defined(Q_WS_WIN)
#include <windows.h>
#endif

namespace WebCore {

#if PLATFORM(WINCE) && !PLATFORM(QT)
    class SharedBitmap;
    class SimpleFontData;
    class GlyphBuffer;
#endif

    const int cMisspellingLineThickness = 3;
    const int cMisspellingLinePatternWidth = 4;
    const int cMisspellingLinePatternGapWidth = 1;

    class TransformationMatrix;
    class Font;
    class Generator;
    class Gradient;
    class GraphicsContextPrivate;
    class GraphicsContextPlatformPrivate;
    class ImageBuffer;
    class KURL;
    class Path;
    class Pattern;
    class TextRun;

    // These bits can be ORed together for a total of 8 possible text drawing modes.
    const int cTextInvisible = 0;
    const int cTextFill = 1;
    const int cTextStroke = 2;
    const int cTextClip = 4;

    enum StrokeStyle {
        NoStroke,
        SolidStroke,
        DottedStroke,
        DashedStroke
    };

    enum FillOrStrokeType {
        SolidColorType,
        PatternType,
        GradientType
    };

    enum InterpolationQuality {
        InterpolationDefault,
        InterpolationNone,
        InterpolationLow,
        InterpolationMedium,
        InterpolationHigh
    };

    class GraphicsContext : public Noncopyable {
    public:
        GraphicsContext(PlatformGraphicsContext*);
        ~GraphicsContext();

#if !PLATFORM(WINCE) || PLATFORM(QT)
        PlatformGraphicsContext* platformContext() const;
#endif

        float strokeThickness() const;
        void setStrokeThickness(float);
        StrokeStyle strokeStyle() const;
        void setStrokeStyle(const StrokeStyle& style);
        Color strokeColor() const;
        ColorSpace strokeColorSpace() const;
        void setStrokeColor(const Color&, ColorSpace);

        void setStrokePattern(PassRefPtr<Pattern>);
        Pattern* strokePattern() const;

        void setStrokeGradient(PassRefPtr<Gradient>);
        Gradient* strokeGradient() const;

        WindRule fillRule() const;
        void setFillRule(WindRule);
        Color fillColor() const;
        ColorSpace fillColorSpace() const;
        void setFillColor(const Color&, ColorSpace);

        void setFillPattern(PassRefPtr<Pattern>);
        Pattern* fillPattern() const;

        void setFillGradient(PassRefPtr<Gradient>);
        Gradient* fillGradient() const;

        void setShadowsIgnoreTransforms(bool);

        void setShouldAntialias(bool);
        bool shouldAntialias() const;

#if PLATFORM(CG)
        void applyStrokePattern();
        void applyFillPattern();
#endif

        void save();
        void restore();

        // These draw methods will do both stroking and filling.
        // FIXME: ...except drawRect(), which fills properly but always strokes
        // using a 1-pixel stroke inset from the rect borders (of the correct
        // stroke color).
        void drawRect(const IntRect&);
        void drawLine(const IntPoint&, const IntPoint&);
        void drawEllipse(const IntRect&);
        void drawConvexPolygon(size_t numPoints, const FloatPoint*, bool shouldAntialias = false);

        void drawPath();
        void fillPath();
        void strokePath();

        // Arc drawing (used by border-radius in CSS) just supports stroking at the moment.
        void strokeArc(const IntRect&, int startAngle, int angleSpan);

        void fillRect(const FloatRect&);
        void fillRect(const FloatRect&, const Color&, ColorSpace);
        void fillRect(const FloatRect&, Generator&);
        void fillRoundedRect(const IntRect&, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight, const Color&, ColorSpace);

        void clearRect(const FloatRect&);

        void strokeRect(const FloatRect&);
        void strokeRect(const FloatRect&, float lineWidth);

        void drawImage(Image*, const IntPoint&, CompositeOperator = CompositeSourceOver);
        void drawImage(Image*, const IntRect&, CompositeOperator = CompositeSourceOver, bool useLowQualityScale = false);
        void drawImage(Image*, const IntPoint& destPoint, const IntRect& srcRect, CompositeOperator = CompositeSourceOver);
        void drawImage(Image*, const IntRect& destRect, const IntRect& srcRect, CompositeOperator = CompositeSourceOver, bool useLowQualityScale = false);
        void drawImage(Image*, const FloatRect& destRect, const FloatRect& srcRect = FloatRect(0, 0, -1, -1),
                       CompositeOperator = CompositeSourceOver, bool useLowQualityScale = false);
        void drawTiledImage(Image*, const IntRect& destRect, const IntPoint& srcPoint, const IntSize& tileSize,
                       CompositeOperator = CompositeSourceOver);
        void drawTiledImage(Image*, const IntRect& destRect, const IntRect& srcRect,
                            Image::TileRule hRule = Image::StretchTile, Image::TileRule vRule = Image::StretchTile,
                            CompositeOperator = CompositeSourceOver);

        void setImageInterpolationQuality(InterpolationQuality);
        InterpolationQuality imageInterpolationQuality() const;

        void clip(const FloatRect&);
        void addRoundedRectClip(const IntRect&, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight);
        void addInnerRoundedRectClip(const IntRect&, int thickness);
        void clipOut(const IntRect&);
        void clipOutEllipseInRect(const IntRect&);
        void clipOutRoundedRect(const IntRect&, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight);
        void clipPath(WindRule);
        void clipToImageBuffer(const FloatRect&, const ImageBuffer*);

        int textDrawingMode();
        void setTextDrawingMode(int);

        void drawText(const Font&, const TextRun&, const IntPoint&, int from = 0, int to = -1);
        void drawBidiText(const Font&, const TextRun&, const FloatPoint&);
        void drawHighlightForText(const Font&, const TextRun&, const IntPoint&, int h, const Color& backgroundColor, ColorSpace, int from = 0, int to = -1);

        FloatRect roundToDevicePixels(const FloatRect&);

        void drawLineForText(const IntPoint&, int width, bool printing);
        void drawLineForMisspellingOrBadGrammar(const IntPoint&, int width, bool grammar);

        bool paintingDisabled() const;
        void setPaintingDisabled(bool);

        bool updatingControlTints() const;
        void setUpdatingControlTints(bool);

        void beginTransparencyLayer(float opacity);
        void endTransparencyLayer();

        void setShadow(const IntSize&, int blur, const Color&, ColorSpace);
        bool getShadow(IntSize&, int&, Color&) const;
        void clearShadow();

        void initFocusRing(int width, int offset);
        void addFocusRingRect(const IntRect&);
        void drawFocusRing(const Color&);
        void clearFocusRing();
        IntRect focusRingBoundingRect();

        void setLineCap(LineCap);
        void setLineDash(const DashArray&, float dashOffset);
        void setLineJoin(LineJoin);
        void setMiterLimit(float);

        void setAlpha(float);
#if PLATFORM(CAIRO)
        float getAlpha();
#endif

        void setCompositeOperation(CompositeOperator);

        void beginPath();
        void addPath(const Path&);

        void clip(const Path&);

        // This clip function is used only by <canvas> code. It allows
        // implementations to handle clipping on the canvas differently since
        // the disipline is different.
        void canvasClip(const Path&);
        void clipOut(const Path&);

        void scale(const FloatSize&);
        void rotate(float angleInRadians);
        void translate(float x, float y);
        IntPoint origin();

        void setURLForRect(const KURL&, const IntRect&);

        void concatCTM(const TransformationMatrix&);
        TransformationMatrix getCTM() const;

#if PLATFORM(WINCE) && !PLATFORM(QT)
        void setBitmap(PassRefPtr<SharedBitmap>);
        const TransformationMatrix& affineTransform() const;
        TransformationMatrix& affineTransform();
        void resetAffineTransform();
        void fillRect(const FloatRect&, const Gradient*);
        void drawText(const SimpleFontData* fontData, const GlyphBuffer& glyphBuffer, int from, int numGlyphs, const FloatPoint& point);
        void drawFrameControl(const IntRect& rect, unsigned type, unsigned state);
        void drawFocusRect(const IntRect& rect);
        void paintTextField(const IntRect& rect, unsigned state);
        void drawBitmap(SharedBitmap*, const IntRect& dstRect, const IntRect& srcRect, CompositeOperator compositeOp);
        void drawBitmapPattern(SharedBitmap*, const FloatRect& tileRectIn, const TransformationMatrix& patternTransform, const FloatPoint& phase, CompositeOperator op, const FloatRect& destRect, const IntSize& origSourceSize);
        void drawIcon(HICON icon, const IntRect& dstRect, UINT flags);
        HDC getWindowsContext(const IntRect&, bool supportAlphaBlend = false, bool mayCreateBitmap = true); // The passed in rect is used to create a bitmap for compositing inside transparency layers.
        void releaseWindowsContext(HDC, const IntRect&, bool supportAlphaBlend = false, bool mayCreateBitmap = true);    // The passed in HDC should be the one handed back by getWindowsContext.
        void drawRoundCorner(bool newClip, RECT clipRect, RECT rectWin, HDC dc, int width, int height);
#elif PLATFORM(WIN)
        GraphicsContext(HDC, bool hasAlpha = false); // FIXME: To be removed.
        bool inTransparencyLayer() const;
        HDC getWindowsContext(const IntRect&, bool supportAlphaBlend = true, bool mayCreateBitmap = true); // The passed in rect is used to create a bitmap for compositing inside transparency layers.
        void releaseWindowsContext(HDC, const IntRect&, bool supportAlphaBlend = true, bool mayCreateBitmap = true);    // The passed in HDC should be the one handed back by getWindowsContext.

        // When set to true, child windows should be rendered into this context
        // rather than allowing them just to render to the screen. Defaults to
        // false.
        // FIXME: This is a layering violation. GraphicsContext shouldn't know
        // what a "window" is. It would be much more appropriate for this flag
        // to be passed as a parameter alongside the GraphicsContext, but doing
        // that would require lots of changes in cross-platform code that we
        // aren't sure we want to make.
        void setShouldIncludeChildWindows(bool);
        bool shouldIncludeChildWindows() const;

        class WindowsBitmap : public Noncopyable {
        public:
            WindowsBitmap(HDC, IntSize);
            ~WindowsBitmap();

            HDC hdc() const { return m_hdc; }
            UInt8* buffer() const { return m_bitmapBuffer; }
            unsigned bufferLength() const { return m_bitmapBufferLength; }
            IntSize size() const { return m_size; }
            unsigned bytesPerRow() const { return m_bytesPerRow; }

        private:
            HDC m_hdc;
            HBITMAP m_bitmap;
            UInt8* m_bitmapBuffer;
            unsigned m_bitmapBufferLength;
            IntSize m_size;
            unsigned m_bytesPerRow;
        };

        WindowsBitmap* createWindowsBitmap(IntSize);
        // The bitmap should be non-premultiplied.
        void drawWindowsBitmap(WindowsBitmap*, const IntPoint&);
#endif

#if PLATFORM(QT) && defined(Q_WS_WIN)
        HDC getWindowsContext(const IntRect&, bool supportAlphaBlend = true, bool mayCreateBitmap = true);
        void releaseWindowsContext(HDC, const IntRect&, bool supportAlphaBlend = true, bool mayCreateBitmap = true);
        bool shouldIncludeChildWindows() const { return false; }
#endif

#if PLATFORM(QT)
        bool inTransparencyLayer() const;
        PlatformPath* currentPath();
        QPen pen();
#endif

#if PLATFORM(GTK)
        void setGdkExposeEvent(GdkEventExpose*);
        GdkDrawable* gdkDrawable() const;
        GdkEventExpose* gdkExposeEvent() const;
#endif

#if PLATFORM(HAIKU)
        pattern getHaikuStrokeStyle();
#endif

    private:
        void savePlatformState();
        void restorePlatformState();

        void setPlatformTextDrawingMode(int);
        void setPlatformFont(const Font& font);

        void setPlatformStrokeColor(const Color&, ColorSpace);
        void setPlatformStrokeStyle(const StrokeStyle&);
        void setPlatformStrokeThickness(float);
        void setPlatformStrokeGradient(Gradient*);
        void setPlatformStrokePattern(Pattern*);

        void setPlatformFillColor(const Color&, ColorSpace);
        void setPlatformFillGradient(Gradient*);
        void setPlatformFillPattern(Pattern*);

        void setPlatformShouldAntialias(bool b);

        void setPlatformShadow(const IntSize&, int blur, const Color&, ColorSpace);
        void clearPlatformShadow();

        static void adjustLineToPixelBoundaries(FloatPoint& p1, FloatPoint& p2, float strokeWidth, const StrokeStyle&);

        int focusRingWidth() const;
        int focusRingOffset() const;
        const Vector<IntRect>& focusRingRects() const;

        static GraphicsContextPrivate* createGraphicsContextPrivate();
        static void destroyGraphicsContextPrivate(GraphicsContextPrivate*);

        GraphicsContextPrivate* m_common;
        GraphicsContextPlatformPrivate* m_data; // Deprecated; m_commmon can just be downcasted. To be removed.
    };

} // namespace WebCore

#endif // GraphicsContext_h

