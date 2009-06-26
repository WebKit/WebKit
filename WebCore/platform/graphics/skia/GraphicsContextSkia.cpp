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

#include "GraphicsContextPlatformPrivate.h"
#include "GraphicsContextPrivate.h"
#include "Color.h"
#include "FloatRect.h"
#include "Gradient.h"
#include "ImageBuffer.h"
#include "IntRect.h"
#include "NativeImageSkia.h"
#include "NotImplemented.h"
#include "PlatformContextSkia.h"
#include "TransformationMatrix.h"

#include "SkBitmap.h"
#include "SkBlurDrawLooper.h"
#include "SkCornerPathEffect.h"
#include "skia/ext/platform_canvas.h"
#include "SkiaUtils.h"
#include "SkShader.h"

#include <math.h>
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>

using namespace std;

namespace WebCore {

namespace {

// "Seatbelt" functions ------------------------------------------------------
//
// These functions check certain graphics primitives for being "safe".
// Skia has historically crashed when sent crazy data. These functions do
// additional checking to prevent crashes.
//
// Ideally, all of these would be fixed in the graphics layer and we would not
// have to do any checking. You can uncomment the ENSURE_VALUE_SAFETY_FOR_SKIA
// flag to check the graphics layer.
#define ENSURE_VALUE_SAFETY_FOR_SKIA

static bool isCoordinateSkiaSafe(float coord)
{
#ifdef ENSURE_VALUE_SAFETY_FOR_SKIA
    // First check for valid floats.
#if defined(_MSC_VER)
    if (!_finite(coord))
#else
    if (!finite(coord))
#endif
        return false;

    // Skia uses 16.16 fixed point and 26.6 fixed point in various places. If
    // the transformed point exceeds 15 bits, we just declare that it's
    // unreasonable to catch both of these cases.
    static const int maxPointMagnitude = 32767;
    if (coord > maxPointMagnitude || coord < -maxPointMagnitude)
        return false;

    return true;
#else
    return true;
#endif
}

static bool isPointSkiaSafe(const SkMatrix& transform, const SkPoint& pt)
{
#ifdef ENSURE_VALUE_SAFETY_FOR_SKIA
    // Now check for points that will overflow. We check the *transformed*
    // points since this is what will be rasterized.
    SkPoint xPt;
    transform.mapPoints(&xPt, &pt, 1);
    return isCoordinateSkiaSafe(xPt.fX) && isCoordinateSkiaSafe(xPt.fY);
#else
    return true;
#endif
}

static bool isRectSkiaSafe(const SkMatrix& transform, const SkRect& rc)
{
#ifdef ENSURE_VALUE_SAFETY_FOR_SKIA
    SkPoint topleft = {rc.fLeft, rc.fTop};
    SkPoint bottomright = {rc.fRight, rc.fBottom};
    return isPointSkiaSafe(transform, topleft) && isPointSkiaSafe(transform, bottomright);
#else
    return true;
#endif
}

bool isPathSkiaSafe(const SkMatrix& transform, const SkPath& path)
{
#ifdef ENSURE_VALUE_SAFETY_FOR_SKIA
    SkPoint current_points[4];
    SkPath::Iter iter(path, false);
    for (SkPath::Verb verb = iter.next(current_points);
         verb != SkPath::kDone_Verb;
         verb = iter.next(current_points)) {
        switch (verb) {
        case SkPath::kMove_Verb:
            // This move will be duplicated in the next verb, so we can ignore.
            break;
        case SkPath::kLine_Verb:
            // iter.next returns 2 points.
            if (!isPointSkiaSafe(transform, current_points[0])
                || !isPointSkiaSafe(transform, current_points[1]))
                return false;
            break;
        case SkPath::kQuad_Verb:
            // iter.next returns 3 points.
            if (!isPointSkiaSafe(transform, current_points[0])
                || !isPointSkiaSafe(transform, current_points[1])
                || !isPointSkiaSafe(transform, current_points[2]))
                return false;
            break;
        case SkPath::kCubic_Verb:
            // iter.next returns 4 points.
            if (!isPointSkiaSafe(transform, current_points[0])
                || !isPointSkiaSafe(transform, current_points[1])
                || !isPointSkiaSafe(transform, current_points[2])
                || !isPointSkiaSafe(transform, current_points[3]))
                return false;
            break;
        case SkPath::kClose_Verb:
        case SkPath::kDone_Verb:
        default:
            break;
        }
    }
    return true;
#else
    return true;
#endif
}

// Local helper functions ------------------------------------------------------

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

inline int fastMod(int value, int max)
{
    int sign = SkExtractSign(value);

    value = SkApplySign(value, sign);
    if (value >= max)
        value %= max;
    return SkApplySign(value, sign);
}

inline float square(float n)
{
    return n * n;
}

}  // namespace

// -----------------------------------------------------------------------------

// This may be called with a NULL pointer to create a graphics context that has
// no painting.
GraphicsContext::GraphicsContext(PlatformGraphicsContext* gc)
    : m_common(createGraphicsContextPrivate())
    , m_data(new GraphicsContextPlatformPrivate(gc))
{
    setPaintingDisabled(!gc || !platformContext()->canvas());
}

GraphicsContext::~GraphicsContext()
{
    delete m_data;
    this->destroyGraphicsContextPrivate(m_common);
}

PlatformGraphicsContext* GraphicsContext::platformContext() const
{
    ASSERT(!paintingDisabled());
    return m_data->context();
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

void GraphicsContext::beginTransparencyLayer(float opacity)
{
    if (paintingDisabled())
        return;

    // We need the "alpha" layer flag here because the base layer is opaque
    // (the surface of the page) but layers on top may have transparent parts.
    // Without explicitly setting the alpha flag, the layer will inherit the
    // opaque setting of the base and some things won't work properly.
    platformContext()->canvas()->saveLayerAlpha(
        0,
        static_cast<unsigned char>(opacity * 255),
        static_cast<SkCanvas::SaveFlags>(SkCanvas::kHasAlphaLayer_SaveFlag |
                                         SkCanvas::kFullColorLayer_SaveFlag));
}

void GraphicsContext::endTransparencyLayer()
{
    if (paintingDisabled())
        return;
    platformContext()->canvas()->restore();
}

// Graphics primitives ---------------------------------------------------------

void GraphicsContext::addInnerRoundedRectClip(const IntRect& rect, int thickness)
{
    if (paintingDisabled())
        return;

    SkRect r(rect);
    if (!isRectSkiaSafe(getCTM(), r))
        return;

    SkPath path;
    path.addOval(r, SkPath::kCW_Direction);
    // only perform the inset if we won't invert r
    if (2 * thickness < rect.width() && 2 * thickness < rect.height()) {
        r.inset(SkIntToScalar(thickness) ,SkIntToScalar(thickness));
        path.addOval(r, SkPath::kCCW_Direction);
    }
    platformContext()->canvas()->clipPath(path);
}

void GraphicsContext::addPath(const Path& path)
{
    if (paintingDisabled())
        return;
    platformContext()->addPath(*path.platformPath());
}

void GraphicsContext::beginPath()
{
    if (paintingDisabled())
        return;
    platformContext()->beginPath();
}

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
    if (!isRectSkiaSafe(getCTM(), r))
        ClipRectToCanvas(*platformContext()->canvas(), r, &r);

    SkPaint paint;
    platformContext()->setupPaintForFilling(&paint);
    paint.setXfermodeMode(SkXfermode::kClear_Mode);
    platformContext()->canvas()->drawRect(r, paint);
}

void GraphicsContext::clip(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    SkRect r(rect);
    if (!isRectSkiaSafe(getCTM(), r))
        return;

    platformContext()->canvas()->clipRect(r);
}

void GraphicsContext::clip(const Path& path)
{
    if (paintingDisabled())
        return;

    const SkPath& p = *path.platformPath();
    if (!isPathSkiaSafe(getCTM(), p))
        return;

    platformContext()->canvas()->clipPath(p);
}

void GraphicsContext::clipOut(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    SkRect r(rect);
    if (!isRectSkiaSafe(getCTM(), r))
        return;

    platformContext()->canvas()->clipRect(r, SkRegion::kDifference_Op);
}

void GraphicsContext::clipOut(const Path& p)
{
    if (paintingDisabled())
        return;

    const SkPath& path = *p.platformPath();
    if (!isPathSkiaSafe(getCTM(), path))
        return;

    platformContext()->canvas()->clipPath(path, SkRegion::kDifference_Op);
}

void GraphicsContext::clipOutEllipseInRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    SkRect oval(rect);
    if (!isRectSkiaSafe(getCTM(), oval))
        return;

    SkPath path;
    path.addOval(oval, SkPath::kCCW_Direction);
    platformContext()->canvas()->clipPath(path, SkRegion::kDifference_Op);
}

void GraphicsContext::clipPath(WindRule clipRule)
{
    if (paintingDisabled())
        return;

    SkPath path = platformContext()->currentPathInLocalCoordinates();
    path.setFillType(clipRule == RULE_EVENODD ? SkPath::kEvenOdd_FillType : SkPath::kWinding_FillType);
    platformContext()->canvas()->clipPath(path);
}

void GraphicsContext::clipToImageBuffer(const FloatRect& rect,
                                        const ImageBuffer* imageBuffer)
{
    if (paintingDisabled())
        return;

#if defined(__linux__) || PLATFORM(WIN_OS)
    platformContext()->beginLayerClippedToImage(rect, imageBuffer);
#endif
}

void GraphicsContext::concatCTM(const TransformationMatrix& xform)
{
    if (paintingDisabled())
        return;
    platformContext()->canvas()->concat(xform);
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

    path.incReserve(numPoints);
    path.moveTo(WebCoreFloatToSkScalar(points[0].x()),
                WebCoreFloatToSkScalar(points[0].y()));
    for (size_t i = 1; i < numPoints; i++) {
        path.lineTo(WebCoreFloatToSkScalar(points[i].x()),
                    WebCoreFloatToSkScalar(points[i].y()));
    }

    if (!isPathSkiaSafe(getCTM(), path))
        return;

    SkPaint paint;
    platformContext()->setupPaintForFilling(&paint);
    platformContext()->canvas()->drawPath(path, paint);

    if (strokeStyle() != NoStroke) {
        paint.reset();
        platformContext()->setupPaintForStroking(&paint, 0, 0);
        platformContext()->canvas()->drawPath(path, paint);
    }
}

// This method is only used to draw the little circles used in lists.
void GraphicsContext::drawEllipse(const IntRect& elipseRect)
{
    if (paintingDisabled())
        return;

    SkRect rect = elipseRect;
    if (!isRectSkiaSafe(getCTM(), rect))
        return;

    SkPaint paint;
    platformContext()->setupPaintForFilling(&paint);
    platformContext()->canvas()->drawOval(rect, paint);

    if (strokeStyle() != NoStroke) {
        paint.reset();
        platformContext()->setupPaintForStroking(&paint, &rect, 0);
        platformContext()->canvas()->drawOval(rect, paint);
    }
}

void GraphicsContext::drawFocusRing(const Color& color)
{
    if (paintingDisabled())
        return;

    const Vector<IntRect>& rects = focusRingRects();
    unsigned rectCount = rects.size();
    if (0 == rectCount)
        return;

    SkRegion focusRingRegion;
    const SkScalar focusRingOutset = WebCoreFloatToSkScalar(0.5);
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
    paint.setStrokeWidth(focusRingOutset * 2);
    paint.setPathEffect(new SkCornerPathEffect(focusRingOutset * 2))->unref();
    focusRingRegion.getBoundaryPath(&path);
    platformContext()->canvas()->drawPath(path, paint);
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
    SkPoint pts[2] = { (SkPoint)point1, (SkPoint)point2 };
    if (!isPointSkiaSafe(getCTM(), pts[0]) || !isPointSkiaSafe(getCTM(), pts[1]))
        return;

    // We know these are vertical or horizontal lines, so the length will just
    // be the sum of the displacement component vectors give or take 1 -
    // probably worth the speed up of no square root, which also won't be exact.
    SkPoint disp = pts[1] - pts[0];
    int length = SkScalarRound(disp.fX + disp.fY);
    int width = roundf(
        platformContext()->setupPaintForStroking(&paint, 0, length));

    // "Borrowed" this comment and idea from GraphicsContextCG.cpp
    // For odd widths, we add in 0.5 to the appropriate x/y so that the float
    // arithmetic works out.  For example, with a border width of 3, KHTML will
    // pass us (y1+y2)/2, e.g., (50+53)/2 = 103/2 = 51 when we want 51.5.  It is
    // always true that an even width gave us a perfect position, but an odd
    // width gave us a position that is off by exactly 0.5.
    bool isVerticalLine = pts[0].fX == pts[1].fX;

    if (width & 1) {  // Odd.
        if (isVerticalLine) {
            pts[0].fX = pts[0].fX + SK_ScalarHalf;
            pts[1].fX = pts[0].fX;
        } else {  // Horizontal line
            pts[0].fY = pts[0].fY + SK_ScalarHalf;
            pts[1].fY = pts[0].fY;
        }
    }
    platformContext()->canvas()->drawPoints(SkCanvas::kLines_PointMode, 2, pts, paint);
}

void GraphicsContext::drawLineForMisspellingOrBadGrammar(const IntPoint& pt,
                                                         int width,
                                                         bool grammar)
{
    if (paintingDisabled())
        return;

    // Create the pattern we'll use to draw the underline.
    static SkBitmap* misspellBitmap = 0;
    if (!misspellBitmap) {
        // We use a 2-pixel-high misspelling indicator because that seems to be
        // what WebKit is designed for, and how much room there is in a typical
        // page for it.
        const int rowPixels = 32;  // Must be multiple of 4 for pattern below.
        const int colPixels = 2;
        misspellBitmap = new SkBitmap;
        misspellBitmap->setConfig(SkBitmap::kARGB_8888_Config,
                                   rowPixels, colPixels);
        misspellBitmap->allocPixels();

        misspellBitmap->eraseARGB(0, 0, 0, 0);
        const uint32_t lineColor = 0xFFFF0000;  // Opaque red.
        const uint32_t antiColor = 0x60600000;  // Semitransparent red.

        // Pattern:  X o   o X o   o X
        //             o X o   o X o
        uint32_t* row1 = misspellBitmap->getAddr32(0, 0);
        uint32_t* row2 = misspellBitmap->getAddr32(0, 1);
        for (int x = 0; x < rowPixels; x++) {
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

    // Offset it vertically by 1 so that there's some space under the text.
    SkScalar originX = SkIntToScalar(pt.x());
    SkScalar originY = SkIntToScalar(pt.y()) + 1;

    // Make a shader for the bitmap with an origin of the box we'll draw. This
    // shader is refcounted and will have an initial refcount of 1.
    SkShader* shader = SkShader::CreateBitmapShader(
        *misspellBitmap, SkShader::kRepeat_TileMode,
        SkShader::kRepeat_TileMode);
    SkMatrix matrix;
    matrix.reset();
    matrix.postTranslate(originX, originY);
    shader->setLocalMatrix(matrix);

    // Assign the shader to the paint & release our reference. The paint will
    // now own the shader and the shader will be destroyed when the paint goes
    // out of scope.
    SkPaint paint;
    paint.setShader(shader);
    shader->unref();

    SkRect rect;
    rect.set(originX,
             originY,
             originX + SkIntToScalar(width),
             originY + SkIntToScalar(misspellBitmap->height()));
    platformContext()->canvas()->drawRect(rect, paint);
}

void GraphicsContext::drawLineForText(const IntPoint& pt,
                                      int width,
                                      bool printing)
{
    if (paintingDisabled())
        return;

    if (width <= 0)
        return;

    int thickness = SkMax32(static_cast<int>(strokeThickness()), 1);
    SkRect r;
    r.fLeft = SkIntToScalar(pt.x());
    r.fTop = SkIntToScalar(pt.y());
    r.fRight = r.fLeft + SkIntToScalar(width);
    r.fBottom = r.fTop + SkIntToScalar(thickness);

    SkPaint paint;
    platformContext()->setupPaintForFilling(&paint);
    // Text lines are drawn using the stroke color.
    paint.setColor(platformContext()->effectiveStrokeColor());
    platformContext()->canvas()->drawRect(r, paint);
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    SkRect r = rect;
    if (!isRectSkiaSafe(getCTM(), r)) {
        // See the fillRect below.
        ClipRectToCanvas(*platformContext()->canvas(), r, &r);
    }

    platformContext()->drawRect(r);
}

void GraphicsContext::fillPath()
{
    if (paintingDisabled())
        return;

    SkPath path = platformContext()->currentPathInLocalCoordinates();
    if (!isPathSkiaSafe(getCTM(), path))
      return;

    const GraphicsContextState& state = m_common->state;
    ColorSpace colorSpace = state.fillColorSpace;

    path.setFillType(state.fillRule == RULE_EVENODD ?
        SkPath::kEvenOdd_FillType : SkPath::kWinding_FillType);

    SkPaint paint;
    platformContext()->setupPaintForFilling(&paint);

    if (colorSpace == PatternColorSpace) {
        SkShader* pat = state.fillPattern->createPlatformPattern(getCTM());
        paint.setShader(pat);
        pat->unref();
    } else if (colorSpace == GradientColorSpace)
        paint.setShader(state.fillGradient->platformGradient());

    platformContext()->canvas()->drawPath(path, paint);
}

void GraphicsContext::fillRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;

    SkRect r = rect;
    if (!isRectSkiaSafe(getCTM(), r)) {
        // See the other version of fillRect below.
        ClipRectToCanvas(*platformContext()->canvas(), r, &r);
    }

    const GraphicsContextState& state = m_common->state;
    ColorSpace colorSpace = state.fillColorSpace;

    SkPaint paint;
    platformContext()->setupPaintForFilling(&paint);

    if (colorSpace == PatternColorSpace) {
        SkShader* pat = state.fillPattern->createPlatformPattern(getCTM());
        paint.setShader(pat);
        pat->unref();
    } else if (colorSpace == GradientColorSpace)
        paint.setShader(state.fillGradient->platformGradient());

    platformContext()->canvas()->drawRect(r, paint);
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color)
{
    if (paintingDisabled())
        return;

    SkRect r = rect;
    if (!isRectSkiaSafe(getCTM(), r)) {
        // Special case when the rectangle overflows fixed point. This is a
        // workaround to fix bug 1212844. When the input rectangle is very
        // large, it can overflow Skia's internal fixed point rect. This
        // should be fixable in Skia (since the output bitmap isn't that
        // large), but until that is fixed, we try to handle it ourselves.
        //
        // We manually clip the rectangle to the current clip rect. This
        // will prevent overflow. The rectangle will be transformed to the
        // canvas' coordinate space before it is converted to fixed point
        // so we are guaranteed not to overflow after doing this.
        ClipRectToCanvas(*platformContext()->canvas(), r, &r);
    }

    SkPaint paint;
    platformContext()->setupPaintCommon(&paint);
    paint.setColor(color.rgb());
    platformContext()->canvas()->drawRect(r, paint);
}

void GraphicsContext::fillRoundedRect(const IntRect& rect,
                                      const IntSize& topLeft,
                                      const IntSize& topRight,
                                      const IntSize& bottomLeft,
                                      const IntSize& bottomRight,
                                      const Color& color)
{
    if (paintingDisabled())
        return;

    SkRect r = rect;
    if (!isRectSkiaSafe(getCTM(), r))
        // See fillRect().
        ClipRectToCanvas(*platformContext()->canvas(), r, &r);

    if (topLeft.width() + topRight.width() > rect.width()
            || bottomLeft.width() + bottomRight.width() > rect.width()
            || topLeft.height() + bottomLeft.height() > rect.height()
            || topRight.height() + bottomRight.height() > rect.height()) {
        // Not all the radii fit, return a rect. This matches the behavior of
        // Path::createRoundedRectangle. Without this we attempt to draw a round
        // shadow for a square box.
        fillRect(rect, color);
        return;
    }

    SkPath path;
    addCornerArc(&path, r, topRight, 270);
    addCornerArc(&path, r, bottomRight, 0);
    addCornerArc(&path, r, bottomLeft, 90);
    addCornerArc(&path, r, topLeft, 180);

    SkPaint paint;
    platformContext()->setupPaintForFilling(&paint);
    platformContext()->canvas()->drawPath(path, paint);
}

TransformationMatrix GraphicsContext::getCTM() const
{
    const SkMatrix& m = platformContext()->canvas()->getTotalMatrix();
    return TransformationMatrix(SkScalarToDouble(m.getScaleX()),      // a
                                SkScalarToDouble(m.getSkewY()),       // b
                                SkScalarToDouble(m.getSkewX()),       // c
                                SkScalarToDouble(m.getScaleY()),      // d
                                SkScalarToDouble(m.getTranslateX()),  // e
                                SkScalarToDouble(m.getTranslateY())); // f
}

FloatRect GraphicsContext::roundToDevicePixels(const FloatRect& rect)
{
    // This logic is copied from GraphicsContextCG, eseidel 5/05/08

    // It is not enough just to round to pixels in device space. The rotation
    // part of the affine transform matrix to device space can mess with this
    // conversion if we have a rotating image like the hands of the world clock
    // widget. We just need the scale, so we get the affine transform matrix and
    // extract the scale.

    const SkMatrix& deviceMatrix = platformContext()->canvas()->getTotalMatrix();
    if (deviceMatrix.isIdentity())
        return rect;

    float deviceScaleX = sqrtf(square(deviceMatrix.getScaleX())
        + square(deviceMatrix.getSkewY()));
    float deviceScaleY = sqrtf(square(deviceMatrix.getSkewX())
        + square(deviceMatrix.getScaleY()));

    FloatPoint deviceOrigin(rect.x() * deviceScaleX, rect.y() * deviceScaleY);
    FloatPoint deviceLowerRight((rect.x() + rect.width()) * deviceScaleX,
        (rect.y() + rect.height()) * deviceScaleY);

    deviceOrigin.setX(roundf(deviceOrigin.x()));
    deviceOrigin.setY(roundf(deviceOrigin.y()));
    deviceLowerRight.setX(roundf(deviceLowerRight.x()));
    deviceLowerRight.setY(roundf(deviceLowerRight.y()));

    // Don't let the height or width round to 0 unless either was originally 0
    if (deviceOrigin.y() == deviceLowerRight.y() && rect.height() != 0)
        deviceLowerRight.move(0, 1);
    if (deviceOrigin.x() == deviceLowerRight.x() && rect.width() != 0)
        deviceLowerRight.move(1, 0);

    FloatPoint roundedOrigin(deviceOrigin.x() / deviceScaleX,
        deviceOrigin.y() / deviceScaleY);
    FloatPoint roundedLowerRight(deviceLowerRight.x() / deviceScaleX,
        deviceLowerRight.y() / deviceScaleY);
    return FloatRect(roundedOrigin, roundedLowerRight - roundedOrigin);
}

void GraphicsContext::scale(const FloatSize& size)
{
    if (paintingDisabled())
        return;
    platformContext()->canvas()->scale(WebCoreFloatToSkScalar(size.width()),
        WebCoreFloatToSkScalar(size.height()));
}

void GraphicsContext::setAlpha(float alpha)
{
    if (paintingDisabled())
        return;
    platformContext()->setAlpha(alpha);
}

void GraphicsContext::setCompositeOperation(CompositeOperator op)
{
    if (paintingDisabled())
        return;
    platformContext()->setXfermodeMode(WebCoreCompositeToSkiaComposite(op));
}

void GraphicsContext::setImageInterpolationQuality(InterpolationQuality)
{
    notImplemented();
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

    size_t count = (dashLength % 2) == 0 ? dashLength : dashLength * 2;
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

void GraphicsContext::setPlatformFillColor(const Color& color)
{
    if (paintingDisabled())
        return;
    platformContext()->setFillColor(color.rgb());
}

void GraphicsContext::setPlatformShadow(const IntSize& size,
                                        int blurInt,
                                        const Color& color)
{
    if (paintingDisabled())
        return;

    // Detect when there's no effective shadow and clear the looper.
    if (size.width() == 0 && size.height() == 0 && blurInt == 0) {
        platformContext()->setDrawLooper(NULL);
        return;
    }

    double width = size.width();
    double height = size.height();
    double blur = blurInt;

    // TODO(tc): This still does not address the issue that shadows
    // within canvas elements should ignore transforms.
    if (m_common->state.shadowsIgnoreTransforms)  {
        // Currently only the GraphicsContext associated with the
        // CanvasRenderingContext for HTMLCanvasElement have shadows ignore
        // Transforms. So with this flag set, we know this state is associated
        // with a CanvasRenderingContext.
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
    SkDrawLooper* dl = new SkBlurDrawLooper(blur / 2, width, height, c);
    platformContext()->setDrawLooper(dl);
    dl->unref();
}

void GraphicsContext::setPlatformStrokeColor(const Color& strokecolor)
{
    if (paintingDisabled())
        return;

    platformContext()->setStrokeColor(strokecolor.rgb());
}

void GraphicsContext::setPlatformStrokeStyle(const StrokeStyle& stroke)
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

void GraphicsContext::setPlatformTextDrawingMode(int mode)
{
    if (paintingDisabled())
        return;

    platformContext()->setTextDrawingMode(mode);
}

void GraphicsContext::setURLForRect(const KURL& link, const IntRect& destRect)
{
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
    if (!isPathSkiaSafe(getCTM(), path))
        return;
    platformContext()->canvas()->drawPath(path, paint);
}

void GraphicsContext::strokePath()
{
    if (paintingDisabled())
        return;

    SkPath path = platformContext()->currentPathInLocalCoordinates();
    if (!isPathSkiaSafe(getCTM(), path))
        return;

    const GraphicsContextState& state = m_common->state;
    ColorSpace colorSpace = state.strokeColorSpace;

    SkPaint paint;
    platformContext()->setupPaintForStroking(&paint, 0, 0);

    if (colorSpace == PatternColorSpace) {
        SkShader* pat = state.strokePattern->createPlatformPattern(getCTM());
        paint.setShader(pat);
        pat->unref();
    } else if (colorSpace == GradientColorSpace)
        paint.setShader(state.strokeGradient->platformGradient());

    platformContext()->canvas()->drawPath(path, paint);
}

void GraphicsContext::strokeRect(const FloatRect& rect, float lineWidth)
{
    if (paintingDisabled())
        return;

    if (!isRectSkiaSafe(getCTM(), rect))
        return;

    const GraphicsContextState& state = m_common->state;
    ColorSpace colorSpace = state.strokeColorSpace;

    SkPaint paint;
    platformContext()->setupPaintForStroking(&paint, 0, 0);
    paint.setStrokeWidth(WebCoreFloatToSkScalar(lineWidth));

    if (colorSpace == PatternColorSpace) {
        SkShader* pat = state.strokePattern->createPlatformPattern(getCTM());
        paint.setShader(pat);
        pat->unref();
    } else if (colorSpace == GradientColorSpace)
        paint.setShader(state.strokeGradient->platformGradient());

    platformContext()->canvas()->drawRect(rect, paint);
}

void GraphicsContext::rotate(float angleInRadians)
{
    if (paintingDisabled())
        return;

    platformContext()->canvas()->rotate(WebCoreFloatToSkScalar(
        angleInRadians * (180.0f / 3.14159265f)));
}

void GraphicsContext::translate(float w, float h)
{
    if (paintingDisabled())
        return;

    platformContext()->canvas()->translate(WebCoreFloatToSkScalar(w),
                                           WebCoreFloatToSkScalar(h));
}

}  // namespace WebCore
