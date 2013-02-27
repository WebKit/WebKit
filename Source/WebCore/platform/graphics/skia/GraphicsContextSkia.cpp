/*
 * Copyright (c) 2006, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GraphicsContext.h"

#include "AffineTransform.h"
#include "Color.h"
#include "FloatRect.h"
#include "Gradient.h"
#include "ImageBuffer.h"
#include "IntRect.h"
#include "KURL.h"
#include "NativeImageSkia.h"
#include "NotImplemented.h"
#include "PlatformContextSkia.h"

#include "SkAnnotation.h"
#include "SkBitmap.h"
#include "SkBlurMaskFilter.h"
#include "SkColorFilter.h"
#include "SkCornerPathEffect.h"
#include "SkData.h"
#include "SkLayerDrawLooper.h"
#include "SkRRect.h"
#include "SkShader.h"
#include "SkiaUtils.h"
#include "skia/ext/platform_canvas.h"

#include <math.h>
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>
#include <wtf/UnusedParam.h>

#if PLATFORM(CHROMIUM) && OS(DARWIN)
#include <ApplicationServices/ApplicationServices.h>
#endif

using namespace std;

namespace WebCore {

namespace {
// Local helper functions ------------------------------------------------------

// Return value % max, but account for value possibly being negative.
inline int fastMod(int value, int max)
{
    bool isNeg = false;
    if (value < 0) {
        value = -value;
        isNeg = true;
    }
    if (value >= max)
        value %= max;
    if (isNeg)
        value = -value;
    return value;
}

void addCornerArc(SkPath* path, const SkRect& rect, const IntSize& size, int startAngle)
{
    SkIRect ir;
    int rx = SkMin32(SkScalarRound(rect.width()), size.width());
    int ry = SkMin32(SkScalarRound(rect.height()), size.height());

    ir.set(-rx, -ry, rx, ry);
    switch (startAngle) {
    case 0:
        ir.offset(rect.fRight - ir.fRight, rect.fBottom - ir.fBottom);
        break;
    case 90:
        ir.offset(rect.fLeft - ir.fLeft, rect.fBottom - ir.fBottom);
        break;
    case 180:
        ir.offset(rect.fLeft - ir.fLeft, rect.fTop - ir.fTop);
        break;
    case 270:
        ir.offset(rect.fRight - ir.fRight, rect.fTop - ir.fTop);
        break;
    default:
        ASSERT(0);
    }

    SkRect r;
    r.set(ir);
    path->arcTo(r, SkIntToScalar(startAngle), SkIntToScalar(90), false);
}

void draw2xMarker(SkBitmap* bitmap, int index)
{

    static const SkPMColor lineColors[2] = {
        SkPreMultiplyARGB(0xFF, 0xFF, 0x00, 0x00), // Opaque red.
        SkPreMultiplyARGB(0xFF, 0xC0, 0xC0, 0xC0), // Opaque gray.
    };
    static const SkPMColor antiColors1[2] = {
        SkPreMultiplyARGB(0xB0, 0xFF, 0x00, 0x00), // Semitransparent red
        SkPreMultiplyARGB(0xB0, 0xC0, 0xC0, 0xC0), // Semitransparent gray
    };
    static const SkPMColor antiColors2[2] = {
        SkPreMultiplyARGB(0x60, 0xFF, 0x00, 0x00), // More transparent red
        SkPreMultiplyARGB(0x60, 0xC0, 0xC0, 0xC0), // More transparent gray
    };

    const SkPMColor lineColor = lineColors[index];
    const SkPMColor antiColor1 = antiColors1[index];
    const SkPMColor antiColor2 = antiColors2[index];

    uint32_t* row1 = bitmap->getAddr32(0, 0);
    uint32_t* row2 = bitmap->getAddr32(0, 1);
    uint32_t* row3 = bitmap->getAddr32(0, 2);
    uint32_t* row4 = bitmap->getAddr32(0, 3);

    // Pattern: X0o   o0X0o   o0
    //          XX0o o0XXX0o o0X
    //           o0XXX0o o0XXX0o
    //            o0X0o   o0X0o
    const SkPMColor row1Color[] = { lineColor, antiColor1, antiColor2, 0,          0,         0,          antiColor2, antiColor1 };
    const SkPMColor row2Color[] = { lineColor, lineColor,  antiColor1, antiColor2, 0,         antiColor2, antiColor1, lineColor };
    const SkPMColor row3Color[] = { 0,         antiColor2, antiColor1, lineColor,  lineColor, lineColor,  antiColor1, antiColor2 };
    const SkPMColor row4Color[] = { 0,         0,          antiColor2, antiColor1, lineColor, antiColor1, antiColor2, 0 };

    for (int x = 0; x < bitmap->width() + 8; x += 8) {
        int count = min(bitmap->width() - x, 8);
        if (count > 0) {
            memcpy(row1 + x, row1Color, count * sizeof(SkPMColor));
            memcpy(row2 + x, row2Color, count * sizeof(SkPMColor));
            memcpy(row3 + x, row3Color, count * sizeof(SkPMColor));
            memcpy(row4 + x, row4Color, count * sizeof(SkPMColor));
        }
    }
}

void draw1xMarker(SkBitmap* bitmap, int index)
{
    static const uint32_t lineColors[2] = {
        0xFF << SK_A32_SHIFT | 0xFF << SK_R32_SHIFT, // Opaque red.
        0xFF << SK_A32_SHIFT | 0xC0 << SK_R32_SHIFT | 0xC0 << SK_G32_SHIFT | 0xC0 << SK_B32_SHIFT, // Opaque gray.
    };
    static const uint32_t antiColors[2] = {
        0x60 << SK_A32_SHIFT | 0x60 << SK_R32_SHIFT, // Semitransparent red
        0xFF << SK_A32_SHIFT | 0xC0 << SK_R32_SHIFT | 0xC0 << SK_G32_SHIFT | 0xC0 << SK_B32_SHIFT, // Semitransparent gray
    };

    const uint32_t lineColor = lineColors[index];
    const uint32_t antiColor = antiColors[index];

    // Pattern: X o   o X o   o X
    //            o X o   o X o
    uint32_t* row1 = bitmap->getAddr32(0, 0);
    uint32_t* row2 = bitmap->getAddr32(0, 1);
    for (int x = 0; x < bitmap->width(); x++) {
        switch (x % 4) {
        case 0:
            row1[x] = lineColor;
            break;
        case 1:
            row1[x] = antiColor;
            row2[x] = antiColor;
            break;
        case 2:
            row2[x] = lineColor;
            break;
        case 3:
            row1[x] = antiColor;
            row2[x] = antiColor;
            break;
        }
    }
}

inline void setRadii(SkVector* radii, IntSize topLeft, IntSize topRight, IntSize bottomRight, IntSize bottomLeft)
{
    radii[SkRRect::kUpperLeft_Corner].set(SkIntToScalar(topLeft.width()),
        SkIntToScalar(topLeft.height()));
    radii[SkRRect::kUpperRight_Corner].set(SkIntToScalar(topRight.width()),
        SkIntToScalar(topRight.height()));
    radii[SkRRect::kLowerRight_Corner].set(SkIntToScalar(bottomRight.width()),
        SkIntToScalar(bottomRight.height()));
    radii[SkRRect::kLowerLeft_Corner].set(SkIntToScalar(bottomLeft.width()),
        SkIntToScalar(bottomLeft.height()));
}

} // namespace

// -----------------------------------------------------------------------------

// This may be called with a NULL pointer to create a graphics context that has
// no painting.
void GraphicsContext::platformInit(PlatformGraphicsContext* gc)
{
    if (gc)
        gc->setGraphicsContext(this);

    // the caller owns the gc
    m_data = gc;
    setPaintingDisabled(!gc || !gc->canvas());
}

void GraphicsContext::platformDestroy()
{
}

PlatformGraphicsContext* GraphicsContext::platformContext() const
{
    ASSERT(!paintingDisabled());
    return m_data;
}

// State saving ----------------------------------------------------------------

void GraphicsContext::savePlatformState()
{
    if (paintingDisabled())
        return;

    // Save our private State.
    platformContext()->save();
}

void GraphicsContext::restorePlatformState()
{
    if (paintingDisabled())
        return;

    // Restore our private State.
    platformContext()->restore();
}

void GraphicsContext::beginPlatformTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;

    // We need the "alpha" layer flag here because the base layer is opaque
    // (the surface of the page) but layers on top may have transparent parts.
    // Without explicitly setting the alpha flag, the layer will inherit the
    // opaque setting of the base and some things won't work properly.
    SkCanvas::SaveFlags saveFlags = static_cast<SkCanvas::SaveFlags>(SkCanvas::kHasAlphaLayer_SaveFlag | SkCanvas::kFullColorLayer_SaveFlag);

    SkPaint layerPaint;
    layerPaint.setAlpha(static_cast<unsigned char>(opacity * 255));
    layerPaint.setXfermodeMode(platformContext()->getXfermodeMode());

    platformContext()->saveLayer(0, &layerPaint, saveFlags);
}

void GraphicsContext::endPlatformTransparencyLayer()
{
    if (paintingDisabled())
        return;
    platformContext()->restoreLayer();
}

bool GraphicsContext::supportsTransparencyLayers()
{
    return true;
}

// Graphics primitives ---------------------------------------------------------

void GraphicsContext::clearPlatformShadow()
{
    if (paintingDisabled())
        return;
    platformContext()->setDrawLooper(0);
}

void GraphicsContext::clearRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    SkRect r = rect;
    SkPaint paint;
    platformContext()->setupPaintForFilling(&paint);
    paint.setXfermodeMode(SkXfermode::kClear_Mode);
    platformContext()->drawRect(r, paint);
}

void GraphicsContext::clip(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    platformContext()->clipRect(rect);
}

void GraphicsContext::clip(const Path& path, WindRule clipRule)
{
    if (paintingDisabled() || path.isEmpty())
        return;

    clipPath(path, clipRule);
}

void GraphicsContext::clipRoundedRect(const RoundedRect& rect)
{
    if (paintingDisabled())
        return;

    SkVector radii[4];
    RoundedRect::Radii wkRadii = rect.radii();
    setRadii(radii, wkRadii.topLeft(), wkRadii.topRight(), wkRadii.bottomRight(), wkRadii.bottomLeft());

    SkRRect r;
    r.setRectRadii(rect.rect(), radii);

    platformContext()->clipRRect(r, PlatformContextSkia::AntiAliased);
}

void GraphicsContext::canvasClip(const Path& pathToClip, WindRule clipRule)
{
    if (paintingDisabled())
        return;

    const SkPath* path = pathToClip.platformPath();
    SkPath::FillType ftype = (clipRule == RULE_EVENODD) ? SkPath::kEvenOdd_FillType : SkPath::kWinding_FillType;
    SkPath storage;

    if (path && (path->getFillType() != ftype)) {
        storage = *path;
        storage.setFillType(ftype);
        path = &storage;
    }

    platformContext()->clipPath(path ? *path : SkPath());
}

void GraphicsContext::clipOut(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    platformContext()->clipRect(rect, PlatformContextSkia::NotAntiAliased, SkRegion::kDifference_Op);
}

void GraphicsContext::clipOut(const Path& p)
{
    if (paintingDisabled())
        return;

    // We must make a copy of the path, to mark it as inverse-filled.
    SkPath path(p.isNull() ? SkPath() : *p.platformPath());
    path.toggleInverseFillType();
    platformContext()->clipPath(path, PlatformContextSkia::AntiAliased);
}

void GraphicsContext::clipPath(const Path& pathToClip, WindRule clipRule)
{
    if (paintingDisabled())
        return;

    const SkPath* path = pathToClip.platformPath();
    SkPath::FillType ftype = (clipRule == RULE_EVENODD) ? SkPath::kEvenOdd_FillType : SkPath::kWinding_FillType;
    SkPath storage;
    if (!path)
        path = &storage;
    else if (path->getFillType() != ftype) {
        storage = *path;
        storage.setFillType(ftype);
        path = &storage;
    }
    platformContext()->clipPath(*path, PlatformContextSkia::AntiAliased);
}

void GraphicsContext::concatCTM(const AffineTransform& affine)
{
    if (paintingDisabled())
        return;

    platformContext()->concat(affine);
}

void GraphicsContext::setCTM(const AffineTransform& affine)
{
    if (paintingDisabled())
        return;

    platformContext()->setMatrix(affine);
}

static void setPathFromConvexPoints(SkPath* path, size_t numPoints, const FloatPoint* points)
{
    path->incReserve(numPoints);
    path->moveTo(WebCoreFloatToSkScalar(points[0].x()),
                 WebCoreFloatToSkScalar(points[0].y()));
    for (size_t i = 1; i < numPoints; ++i) {
        path->lineTo(WebCoreFloatToSkScalar(points[i].x()),
                     WebCoreFloatToSkScalar(points[i].y()));
    }

    /*  The code used to just blindly call this
            path->setIsConvex(true);
        But webkit can sometimes send us non-convex 4-point values, so we mark the path's
        convexity as unknown, so it will get computed by skia at draw time.
        See crbug.com 108605
    */
    SkPath::Convexity convexity = SkPath::kConvex_Convexity;
    if (numPoints == 4)
        convexity = SkPath::kUnknown_Convexity;
    path->setConvexity(convexity);
}

void GraphicsContext::drawConvexPolygon(size_t numPoints,
                                        const FloatPoint* points,
                                        bool shouldAntialias)
{
    if (paintingDisabled())
        return;

    if (numPoints <= 1)
        return;

    SkPath path;
    setPathFromConvexPoints(&path, numPoints, points);

    SkPaint paint;
    platformContext()->setupPaintForFilling(&paint);
    paint.setAntiAlias(shouldAntialias);
    platformContext()->drawPath(path, paint);

    if (strokeStyle() != NoStroke) {
        paint.reset();
        platformContext()->setupPaintForStroking(&paint, 0, 0);
        platformContext()->drawPath(path, paint);
    }
}

void GraphicsContext::clipConvexPolygon(size_t numPoints, const FloatPoint* points, bool antialiased)
{
    if (paintingDisabled())
        return;

    if (numPoints <= 1)
        return;

    SkPath path;
    setPathFromConvexPoints(&path, numPoints, points);
    platformContext()->clipPath(path, antialiased
        ? PlatformContextSkia::AntiAliased
        : PlatformContextSkia::NotAntiAliased);
}

// This method is only used to draw the little circles used in lists.
void GraphicsContext::drawEllipse(const IntRect& elipseRect)
{
    if (paintingDisabled())
        return;

    SkRect rect = elipseRect;
    SkPaint paint;
    platformContext()->setupPaintForFilling(&paint);
    platformContext()->drawOval(rect, paint);

    if (strokeStyle() != NoStroke) {
        paint.reset();
        platformContext()->setupPaintForStroking(&paint, &rect, 0);
        platformContext()->drawOval(rect, paint);
    }
}

void GraphicsContext::drawFocusRing(const Path& path, int width, int offset, const Color& color)
{
    // FIXME: implement
}

static inline void drawOuterPath(PlatformContextSkia* context, const SkPath& path, SkPaint& paint, int width)
{
#if PLATFORM(CHROMIUM) && OS(DARWIN)
    paint.setAlpha(64);
    paint.setStrokeWidth(width);
    paint.setPathEffect(new SkCornerPathEffect((width - 1) * 0.5f))->unref();
#else
    paint.setStrokeWidth(1);
    paint.setPathEffect(new SkCornerPathEffect(1))->unref();
#endif
    context->drawPath(path, paint);
}

static inline void drawInnerPath(PlatformContextSkia* context, const SkPath& path, SkPaint& paint, int width)
{
#if PLATFORM(CHROMIUM) && OS(DARWIN)
    paint.setAlpha(128);
    paint.setStrokeWidth(width * 0.5f);
    context->drawPath(path, paint);
#endif
}

static inline int getFocusRingOutset(int offset)
{
#if PLATFORM(CHROMIUM) && OS(DARWIN)
    return offset + 2;
#else
    return 0;
#endif
}

void GraphicsContext::drawFocusRing(const Vector<IntRect>& rects, int width, int offset, const Color& color)
{
    if (paintingDisabled())
        return;

    unsigned rectCount = rects.size();
    if (!rectCount)
        return;

    SkRegion focusRingRegion;
    const int focusRingOutset = getFocusRingOutset(offset);
    for (unsigned i = 0; i < rectCount; i++) {
        SkIRect r = rects[i];
        r.inset(-focusRingOutset, -focusRingOutset);
        focusRingRegion.op(r, SkRegion::kUnion_Op);
    }

    SkPath path;
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);

    paint.setColor(color.rgb());
    focusRingRegion.getBoundaryPath(&path);
    drawOuterPath(platformContext(), path, paint, width);
    drawInnerPath(platformContext(), path, paint, width);
}

// This is only used to draw borders.
void GraphicsContext::drawLine(const IntPoint& point1, const IntPoint& point2)
{
    if (paintingDisabled())
        return;

    StrokeStyle penStyle = strokeStyle();
    if (penStyle == NoStroke)
        return;

    SkPaint paint;
    FloatPoint p1 = point1;
    FloatPoint p2 = point2;
    bool isVerticalLine = (p1.x() == p2.x());
    int width = roundf(strokeThickness());

    // We know these are vertical or horizontal lines, so the length will just
    // be the sum of the displacement component vectors give or take 1 -
    // probably worth the speed up of no square root, which also won't be exact.
    FloatSize disp = p2 - p1;
    int length = SkScalarRound(disp.width() + disp.height());
    platformContext()->setupPaintForStroking(&paint, 0, length);

    if (strokeStyle() == DottedStroke || strokeStyle() == DashedStroke) {
        // Do a rect fill of our endpoints.  This ensures we always have the
        // appearance of being a border.  We then draw the actual dotted/dashed line.

        SkRect r1, r2;
        r1.set(p1.x(), p1.y(), p1.x() + width, p1.y() + width);
        r2.set(p2.x(), p2.y(), p2.x() + width, p2.y() + width);

        if (isVerticalLine) {
            r1.offset(-width / 2, 0);
            r2.offset(-width / 2, -width);
        } else {
            r1.offset(0, -width / 2);
            r2.offset(-width, -width / 2);
        }
        SkPaint fillPaint;
        fillPaint.setColor(paint.getColor());
        platformContext()->drawRect(r1, fillPaint);
        platformContext()->drawRect(r2, fillPaint);
    }

    adjustLineToPixelBoundaries(p1, p2, width, penStyle);
    SkPoint pts[2] = { (SkPoint)p1, (SkPoint)p2 };

    platformContext()->drawPoints(SkCanvas::kLines_PointMode, 2, pts, paint);
}

void GraphicsContext::drawLineForDocumentMarker(const FloatPoint& pt, float width, DocumentMarkerLineStyle style)
{
    if (paintingDisabled())
        return;

    int deviceScaleFactor = SkScalarRoundToInt(WebCoreFloatToSkScalar(platformContext()->deviceScaleFactor()));
    ASSERT(deviceScaleFactor == 1 || deviceScaleFactor == 2);

    // Create the pattern we'll use to draw the underline.
    int index = style == DocumentMarkerGrammarLineStyle ? 1 : 0;
    static SkBitmap* misspellBitmap1x[2] = { 0, 0 };
    static SkBitmap* misspellBitmap2x[2] = { 0, 0 };
    SkBitmap** misspellBitmap = deviceScaleFactor == 2 ? misspellBitmap2x : misspellBitmap1x;
    if (!misspellBitmap[index]) {
#if PLATFORM(CHROMIUM) && OS(DARWIN)
        // Match the artwork used by the Mac.
        const int rowPixels = 4 * deviceScaleFactor;
        const int colPixels = 3 * deviceScaleFactor;
        misspellBitmap[index] = new SkBitmap;
        misspellBitmap[index]->setConfig(SkBitmap::kARGB_8888_Config,
                                         rowPixels, colPixels);
        misspellBitmap[index]->allocPixels();

        misspellBitmap[index]->eraseARGB(0, 0, 0, 0);
        const uint32_t transparentColor = 0x00000000;

        if (deviceScaleFactor == 1) {
            const uint32_t colors[2][6] = {
                { 0x2A2A0600, 0x57571000,  0xA8A81B00, 0xBFBF1F00,  0x70701200, 0xE0E02400 },
                { 0x2A001503, 0x57002A08,  0xA800540D, 0xBF005F0F,  0x70003809, 0xE0007012 }
            };

            // Pattern: a b a   a b a
            //          c d c   c d c
            //          e f e   e f e
            for (int x = 0; x < colPixels; ++x) {
                uint32_t* row = misspellBitmap[index]->getAddr32(0, x);
                row[0] = colors[index][x * 2];
                row[1] = colors[index][x * 2 + 1];
                row[2] = colors[index][x * 2];
                row[3] = transparentColor;
            }
        } else if (deviceScaleFactor == 2) {
            const uint32_t colors[2][18] = {
                { 0x0a090101, 0x33320806, 0x55540f0a,  0x37360906, 0x6e6c120c, 0x6e6c120c,  0x7674140d, 0x8d8b1810, 0x8d8b1810,
                  0x96941a11, 0xb3b01f15, 0xb3b01f15,  0x6d6b130c, 0xd9d62619, 0xd9d62619,  0x19180402, 0x7c7a150e, 0xcecb2418 },
                { 0x0a000400, 0x33031b06, 0x55062f0b,  0x37041e06, 0x6e083d0d, 0x6e083d0d,  0x7608410e, 0x8d094e11, 0x8d094e11,
                  0x960a5313, 0xb30d6417, 0xb30d6417,  0x6d073c0d, 0xd90f781c, 0xd90f781c,  0x19010d03, 0x7c094510, 0xce0f731a }
            };

            // Pattern: a b c c b a
            //          d e f f e d
            //          g h j j h g
            //          k l m m l k
            //          n o p p o n
            //          q r s s r q
            for (int x = 0; x < colPixels; ++x) {
                uint32_t* row = misspellBitmap[index]->getAddr32(0, x);
                row[0] = colors[index][x * 3];
                row[1] = colors[index][x * 3 + 1];
                row[2] = colors[index][x * 3 + 2];
                row[3] = colors[index][x * 3 + 2];
                row[4] = colors[index][x * 3 + 1];
                row[5] = colors[index][x * 3];
                row[6] = transparentColor;
                row[7] = transparentColor;
            }
        } else
            ASSERT_NOT_REACHED();
#else
        // We use a 2-pixel-high misspelling indicator because that seems to be
        // what WebKit is designed for, and how much room there is in a typical
        // page for it.
        const int rowPixels = 32 * deviceScaleFactor; // Must be multiple of 4 for pattern below.
        const int colPixels = 2 * deviceScaleFactor;
        misspellBitmap[index] = new SkBitmap;
        misspellBitmap[index]->setConfig(SkBitmap::kARGB_8888_Config, rowPixels, colPixels);
        misspellBitmap[index]->allocPixels();

        misspellBitmap[index]->eraseARGB(0, 0, 0, 0);
        if (deviceScaleFactor == 1)
            draw1xMarker(misspellBitmap[index], index);
        else if (deviceScaleFactor == 2)
            draw2xMarker(misspellBitmap[index], index);
        else
            ASSERT_NOT_REACHED();
#endif
    }

#if PLATFORM(CHROMIUM) && OS(DARWIN)
    SkScalar originX = WebCoreFloatToSkScalar(pt.x()) * deviceScaleFactor;
    SkScalar originY = WebCoreFloatToSkScalar(pt.y()) * deviceScaleFactor;

    // Make sure to draw only complete dots.
    int rowPixels = misspellBitmap[index]->width();
    float widthMod = fmodf(width * deviceScaleFactor, rowPixels);
    if (rowPixels - widthMod > deviceScaleFactor)
        width -= widthMod / deviceScaleFactor;
#else
    SkScalar originX = WebCoreFloatToSkScalar(pt.x());

    // Offset it vertically by 1 so that there's some space under the text.
    SkScalar originY = WebCoreFloatToSkScalar(pt.y()) + 1;
    originX *= deviceScaleFactor;
    originY *= deviceScaleFactor;
#endif

    // Make a shader for the bitmap with an origin of the box we'll draw. This
    // shader is refcounted and will have an initial refcount of 1.
    SkShader* shader = SkShader::CreateBitmapShader(
        *misspellBitmap[index], SkShader::kRepeat_TileMode,
        SkShader::kRepeat_TileMode);
    SkMatrix matrix;
    matrix.setTranslate(originX, originY);
    shader->setLocalMatrix(matrix);

    // Assign the shader to the paint & release our reference. The paint will
    // now own the shader and the shader will be destroyed when the paint goes
    // out of scope.
    SkPaint paint;
    paint.setShader(shader)->unref();

    SkRect rect;
    rect.set(originX, originY, originX + WebCoreFloatToSkScalar(width) * deviceScaleFactor, originY + SkIntToScalar(misspellBitmap[index]->height()));

    if (deviceScaleFactor == 2) {
        platformContext()->save();
        platformContext()->scale(SK_ScalarHalf, SK_ScalarHalf);
    }
    platformContext()->drawRect(rect, paint);
    if (deviceScaleFactor == 2)
        platformContext()->restore();
}

void GraphicsContext::drawLineForText(const FloatPoint& pt,
                                      float width,
                                      bool printing)
{
    if (paintingDisabled())
        return;

    if (width <= 0)
        return;

    int thickness = SkMax32(static_cast<int>(strokeThickness()), 1);
    SkRect r;
    r.fLeft = WebCoreFloatToSkScalar(pt.x());
    // Avoid anti-aliasing lines. Currently, these are always horizontal.
    r.fTop = WebCoreFloatToSkScalar(floorf(pt.y()));
    r.fRight = r.fLeft + WebCoreFloatToSkScalar(width);
    r.fBottom = r.fTop + SkIntToScalar(thickness);

    SkPaint paint;
    platformContext()->setupPaintForFilling(&paint);
    // Text lines are drawn using the stroke color.
    paint.setColor(platformContext()->effectiveStrokeColor());
    platformContext()->drawRect(r, paint);
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    ASSERT(!rect.isEmpty());
    if (rect.isEmpty())
        return;

    platformContext()->drawRect(rect);
}

void GraphicsContext::fillPath(const Path& pathToFill)
{
    if (paintingDisabled() || pathToFill.isEmpty())
        return;

    const GraphicsContextState& state = m_state;
    SkPath::FillType ftype = state.fillRule == RULE_EVENODD ?
        SkPath::kEvenOdd_FillType : SkPath::kWinding_FillType;

    const SkPath* path = pathToFill.platformPath();
    SkPath storage;
    if (path->getFillType() != ftype) {
        storage = *path;
        storage.setFillType(ftype);
        path = &storage;
    }

    SkPaint paint;
    platformContext()->setupPaintForFilling(&paint);

    platformContext()->drawPath(*path, paint);
}

void GraphicsContext::fillRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    SkRect r = rect;

    SkPaint paint;
    platformContext()->setupPaintForFilling(&paint);
    platformContext()->drawRect(r, paint);
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color, ColorSpace colorSpace)
{
    if (paintingDisabled())
        return;

    SkRect r = rect;
    SkPaint paint;
    platformContext()->setupPaintCommon(&paint);
    paint.setColor(color.rgb());
    platformContext()->drawRect(r, paint);
}

void GraphicsContext::fillRoundedRect(const IntRect& rect,
                                      const IntSize& topLeft,
                                      const IntSize& topRight,
                                      const IntSize& bottomLeft,
                                      const IntSize& bottomRight,
                                      const Color& color,
                                      ColorSpace colorSpace)
{
    if (paintingDisabled())
        return;

    if (topLeft.width() + topRight.width() > rect.width()
            || bottomLeft.width() + bottomRight.width() > rect.width()
            || topLeft.height() + bottomLeft.height() > rect.height()
            || topRight.height() + bottomRight.height() > rect.height()) {
        // Not all the radii fit, return a rect. This matches the behavior of
        // Path::createRoundedRectangle. Without this we attempt to draw a round
        // shadow for a square box.
        fillRect(rect, color, colorSpace);
        return;
    }

    SkVector radii[4];
    setRadii(radii, topLeft, topRight, bottomRight, bottomLeft);

    SkRRect rr;
    rr.setRectRadii(rect, radii);

    SkPaint paint;
    platformContext()->setupPaintForFilling(&paint);
    paint.setColor(color.rgb());

    platformContext()->drawRRect(rr, paint);
}

AffineTransform GraphicsContext::getCTM(IncludeDeviceScale) const
{
    if (paintingDisabled())
        return AffineTransform();

    const SkMatrix& m = platformContext()->getTotalMatrix();
    return AffineTransform(SkScalarToDouble(m.getScaleX()),
                           SkScalarToDouble(m.getSkewY()),
                           SkScalarToDouble(m.getSkewX()),
                           SkScalarToDouble(m.getScaleY()),
                           SkScalarToDouble(m.getTranslateX()),
                           SkScalarToDouble(m.getTranslateY()));
}

FloatRect GraphicsContext::roundToDevicePixels(const FloatRect& rect, RoundingMode)
{
    return rect;
}

void GraphicsContext::scale(const FloatSize& size)
{
    if (paintingDisabled())
        return;

    platformContext()->scale(WebCoreFloatToSkScalar(size.width()),
        WebCoreFloatToSkScalar(size.height()));
}

void GraphicsContext::setAlpha(float alpha)
{
    if (paintingDisabled())
        return;

    platformContext()->setAlpha(alpha);
}

void GraphicsContext::setPlatformCompositeOperation(CompositeOperator op, BlendMode)
{
    if (paintingDisabled())
        return;

    platformContext()->setXfermodeMode(WebCoreCompositeToSkiaComposite(op));
}

InterpolationQuality GraphicsContext::imageInterpolationQuality() const
{
    return platformContext()->interpolationQuality();
}

void GraphicsContext::setImageInterpolationQuality(InterpolationQuality q)
{
    platformContext()->setInterpolationQuality(q);
}

void GraphicsContext::setLineCap(LineCap cap)
{
    if (paintingDisabled())
        return;
    switch (cap) {
    case ButtCap:
        platformContext()->setLineCap(SkPaint::kButt_Cap);
        break;
    case RoundCap:
        platformContext()->setLineCap(SkPaint::kRound_Cap);
        break;
    case SquareCap:
        platformContext()->setLineCap(SkPaint::kSquare_Cap);
        break;
    default:
        ASSERT(0);
        break;
    }
}

void GraphicsContext::setLineDash(const DashArray& dashes, float dashOffset)
{
    if (paintingDisabled())
        return;

    // FIXME: This is lifted directly off SkiaSupport, lines 49-74
    // so it is not guaranteed to work correctly.
    size_t dashLength = dashes.size();
    if (!dashLength) {
        // If no dash is set, revert to solid stroke
        // FIXME: do we need to set NoStroke in some cases?
        platformContext()->setStrokeStyle(SolidStroke);
        platformContext()->setDashPathEffect(0);
        return;
    }

    size_t count = !(dashLength % 2) ? dashLength : dashLength * 2;
    SkScalar* intervals = new SkScalar[count];

    for (unsigned int i = 0; i < count; i++)
        intervals[i] = dashes[i % dashLength];

    platformContext()->setDashPathEffect(new SkDashPathEffect(intervals, count, dashOffset));

    delete[] intervals;
}

void GraphicsContext::setLineJoin(LineJoin join)
{
    if (paintingDisabled())
        return;
    switch (join) {
    case MiterJoin:
        platformContext()->setLineJoin(SkPaint::kMiter_Join);
        break;
    case RoundJoin:
        platformContext()->setLineJoin(SkPaint::kRound_Join);
        break;
    case BevelJoin:
        platformContext()->setLineJoin(SkPaint::kBevel_Join);
        break;
    default:
        ASSERT(0);
        break;
    }
}

void GraphicsContext::setMiterLimit(float limit)
{
    if (paintingDisabled())
        return;
    platformContext()->setMiterLimit(limit);
}

void GraphicsContext::setPlatformFillColor(const Color& color, ColorSpace colorSpace)
{
    if (paintingDisabled())
        return;

    platformContext()->setFillColor(color.rgb());
}

void GraphicsContext::setPlatformShadow(const FloatSize& size,
                                        float blurFloat,
                                        const Color& color,
                                        ColorSpace colorSpace)
{
    if (paintingDisabled())
        return;

    // Detect when there's no effective shadow and clear the looper.
    if (!size.width() && !size.height() && !blurFloat) {
        platformContext()->setDrawLooper(0);
        return;
    }

    double width = size.width();
    double height = size.height();
    double blur = blurFloat;

    uint32_t mfFlags = SkBlurMaskFilter::kHighQuality_BlurFlag;

    if (m_state.shadowsIgnoreTransforms)  {
        // Currently only the GraphicsContext associated with the
        // CanvasRenderingContext for HTMLCanvasElement have shadows ignore
        // Transforms. So with this flag set, we know this state is associated
        // with a CanvasRenderingContext.
        mfFlags |= SkBlurMaskFilter::kIgnoreTransform_BlurFlag;

        // CG uses natural orientation for Y axis, but the HTML5 canvas spec
        // does not.
        // So we now flip the height since it was flipped in
        // CanvasRenderingContext in order to work with CG.
        height = -height;
    }

    SkColor c;
    if (color.isValid())
        c = color.rgb();
    else
        c = SkColorSetARGB(0xFF/3, 0, 0, 0);    // "std" apple shadow color.

    // TODO(tc): Should we have a max value for the blur?  CG clamps at 1000.0
    // for perf reasons.

    SkLayerDrawLooper* dl = new SkLayerDrawLooper;
    SkAutoUnref aur(dl);

    // top layer, we just draw unchanged
    dl->addLayer();

    // lower layer contains our offset, blur, and colorfilter
    SkLayerDrawLooper::LayerInfo info;

    // Since CSS box-shadow ignores the original alpha, we used to default to kSrc_Mode here and
    // only switch to kDst_Mode for Canvas. But that precaution is not really necessary because
    // RenderBoxModelObject performs a dedicated shadow fill with opaque black to ensure
    // cross-platform consistent results.
    info.fColorMode = SkXfermode::kDst_Mode;
    info.fPaintBits |= SkLayerDrawLooper::kMaskFilter_Bit; // our blur
    info.fPaintBits |= SkLayerDrawLooper::kColorFilter_Bit;
    info.fOffset.set(width, height);
    info.fPostTranslate = m_state.shadowsIgnoreTransforms;

    SkMaskFilter* mf = SkBlurMaskFilter::Create(blur / 2, SkBlurMaskFilter::kNormal_BlurStyle, mfFlags);

    SkColorFilter* cf = SkColorFilter::CreateModeFilter(c, SkXfermode::kSrcIn_Mode);

    SkPaint* paint = dl->addLayer(info);
    SkSafeUnref(paint->setMaskFilter(mf));
    SkSafeUnref(paint->setColorFilter(cf));

    // dl is now built, just install it
    platformContext()->setDrawLooper(dl);
}

void GraphicsContext::setPlatformStrokeColor(const Color& strokecolor, ColorSpace colorSpace)
{
    if (paintingDisabled())
        return;

    platformContext()->setStrokeColor(strokecolor.rgb());
}

void GraphicsContext::setPlatformStrokeStyle(StrokeStyle stroke)
{
    if (paintingDisabled())
        return;

    platformContext()->setStrokeStyle(stroke);
}

void GraphicsContext::setPlatformStrokeThickness(float thickness)
{
    if (paintingDisabled())
        return;

    platformContext()->setStrokeThickness(thickness);
}

void GraphicsContext::setPlatformTextDrawingMode(TextDrawingModeFlags mode)
{
    if (paintingDisabled())
        return;

    platformContext()->setTextDrawingMode(mode);
}

void GraphicsContext::setURLForRect(const KURL& link, const IntRect& destRect)
{
    if (paintingDisabled())
        return;

    SkAutoDataUnref url(SkData::NewWithCString(link.string().utf8().data()));
    SkAnnotateRectWithURL(platformContext()->canvas(), destRect, url.get());
}

void GraphicsContext::setPlatformShouldAntialias(bool enable)
{
    if (paintingDisabled())
        return;

    platformContext()->setUseAntialiasing(enable);
}

void GraphicsContext::strokeArc(const IntRect& r, int startAngle, int angleSpan)
{
    if (paintingDisabled())
        return;

    SkPaint paint;
    SkRect oval = r;
    if (strokeStyle() == NoStroke) {
        // Stroke using the fill color.
        // TODO(brettw) is this really correct? It seems unreasonable.
        platformContext()->setupPaintForFilling(&paint);
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(WebCoreFloatToSkScalar(strokeThickness()));
    } else
        platformContext()->setupPaintForStroking(&paint, 0, 0);

    // We do this before converting to scalar, so we don't overflow SkFixed.
    startAngle = fastMod(startAngle, 360);
    angleSpan = fastMod(angleSpan, 360);

    SkPath path;
    path.addArc(oval, SkIntToScalar(-startAngle), SkIntToScalar(-angleSpan));
    platformContext()->drawPath(path, paint);
}

void GraphicsContext::strokePath(const Path& pathToStroke)
{
    if (paintingDisabled() || pathToStroke.isEmpty())
        return;

    const SkPath& path = *pathToStroke.platformPath();
    SkPaint paint;
    platformContext()->setupPaintForStroking(&paint, 0, 0);
    platformContext()->drawPath(path, paint);
}

void GraphicsContext::strokeRect(const FloatRect& rect, float lineWidth)
{
    if (paintingDisabled())
        return;

    SkPaint paint;
    platformContext()->setupPaintForStroking(&paint, 0, 0);
    paint.setStrokeWidth(WebCoreFloatToSkScalar(lineWidth));
    // strokerect has special rules for CSS when the rect is degenerate:
    // if width==0 && height==0, do nothing
    // if width==0 || height==0, then just draw line for the other dimension
    SkRect r(rect);
    bool validW = r.width() > 0;
    bool validH = r.height() > 0;
    if (validW && validH) {
        platformContext()->drawRect(r, paint);
    } else if (validW || validH) {
        // we are expected to respect the lineJoin, so we can't just call
        // drawLine -- we have to create a path that doubles back on itself.
        SkPath path;
        path.moveTo(r.fLeft, r.fTop);
        path.lineTo(r.fRight, r.fBottom);
        path.close();
        platformContext()->drawPath(path, paint);
    }
}

void GraphicsContext::rotate(float angleInRadians)
{
    if (paintingDisabled())
        return;

    platformContext()->rotate(WebCoreFloatToSkScalar(angleInRadians * (180.0f / 3.14159265f)));
}

void GraphicsContext::translate(float w, float h)
{
    if (paintingDisabled())
        return;

    platformContext()->translate(WebCoreFloatToSkScalar(w), WebCoreFloatToSkScalar(h));
}

bool GraphicsContext::isAcceleratedContext() const
{
    return platformContext()->isAccelerated();
}

#if PLATFORM(CHROMIUM) && OS(DARWIN)
CGColorSpaceRef deviceRGBColorSpaceRef()
{
    static CGColorSpaceRef deviceSpace = CGColorSpaceCreateDeviceRGB();
    return deviceSpace;
}
#endif

void GraphicsContext::platformFillEllipse(const FloatRect& ellipse)
{
    if (paintingDisabled())
        return;

    SkRect rect = ellipse;
    SkPaint paint;
    platformContext()->setupPaintForFilling(&paint);
    platformContext()->drawOval(rect, paint);
}

void GraphicsContext::platformStrokeEllipse(const FloatRect& ellipse)
{
    if (paintingDisabled())
        return;

    SkRect rect(ellipse);
    SkPaint paint;
    platformContext()->setupPaintForStroking(&paint, 0, 0);
    platformContext()->drawOval(rect, paint);
}

}  // namespace WebCore
