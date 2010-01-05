/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
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
#include "ImageBuffer.h"
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#include "SkiaUtils.h"

#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"

#include "SkBitmap.h"
#include "SkColorPriv.h"
#include "SkShader.h"
#include "SkDashPathEffect.h"

#include <wtf/MathExtras.h>
#include <wtf/Vector.h>

namespace WebCore 
{
extern bool isPathSkiaSafe(const SkMatrix& transform, const SkPath& path);
}

// State -----------------------------------------------------------------------

// Encapsulates the additional painting state information we store for each
// pushed graphics state.
struct PlatformContextSkia::State {
    State();
    State(const State&);
    ~State();

    // Common shader state.
    float m_alpha;
    SkXfermode::Mode m_xferMode;
    bool m_useAntialiasing;
    SkDrawLooper* m_looper;

    // Fill.
    SkColor m_fillColor;
    SkShader* m_fillShader;

    // Stroke.
    WebCore::StrokeStyle m_strokeStyle;
    SkColor m_strokeColor;
    SkShader* m_strokeShader;
    float m_strokeThickness;
    int m_dashRatio;  // Ratio of the length of a dash to its width.
    float m_miterLimit;
    SkPaint::Cap m_lineCap;
    SkPaint::Join m_lineJoin;
    SkDashPathEffect* m_dash;

    // Text. (See cTextFill & friends in GraphicsContext.h.)
    int m_textDrawingMode;

    // Helper function for applying the state's alpha value to the given input
    // color to produce a new output color.
    SkColor applyAlpha(SkColor) const;

#if OS(LINUX) || OS(WINDOWS)
    // If non-empty, the current State is clipped to this image.
    SkBitmap m_imageBufferClip;
    // If m_imageBufferClip is non-empty, this is the region the image is clipped to.
    WebCore::FloatRect m_clip;
#endif

    // This is a list of clipping paths which are currently active, in the
    // order in which they were pushed.
    WTF::Vector<SkPath> m_antiAliasClipPaths;

private:
    // Not supported.
    void operator=(const State&);
};

// Note: Keep theses default values in sync with GraphicsContextState.
PlatformContextSkia::State::State()
    : m_alpha(1)
    , m_xferMode(SkXfermode::kSrcOver_Mode)
    , m_useAntialiasing(true)
    , m_looper(0)
    , m_fillColor(0xFF000000)
    , m_fillShader(0)
    , m_strokeStyle(WebCore::SolidStroke)
    , m_strokeColor(WebCore::Color::black)
    , m_strokeShader(0)
    , m_strokeThickness(0)
    , m_dashRatio(3)
    , m_miterLimit(4)
    , m_lineCap(SkPaint::kDefault_Cap)
    , m_lineJoin(SkPaint::kDefault_Join)
    , m_dash(0)
    , m_textDrawingMode(WebCore::cTextFill)
{
}

PlatformContextSkia::State::State(const State& other)
    : m_alpha(other.m_alpha)
    , m_xferMode(other.m_xferMode)
    , m_useAntialiasing(other.m_useAntialiasing)
    , m_looper(other.m_looper)
    , m_fillColor(other.m_fillColor)
    , m_fillShader(other.m_fillShader)
    , m_strokeStyle(other.m_strokeStyle)
    , m_strokeColor(other.m_strokeColor)
    , m_strokeShader(other.m_strokeShader)
    , m_strokeThickness(other.m_strokeThickness)
    , m_dashRatio(other.m_dashRatio)
    , m_miterLimit(other.m_miterLimit)
    , m_lineCap(other.m_lineCap)
    , m_lineJoin(other.m_lineJoin)
    , m_dash(other.m_dash)
    , m_textDrawingMode(other.m_textDrawingMode)
#if OS(LINUX) || OS(WINDOWS)
    , m_imageBufferClip(other.m_imageBufferClip)
    , m_clip(other.m_clip)
#endif
{
    // Up the ref count of these. saveRef does nothing if 'this' is NULL.
    m_looper->safeRef();
    m_dash->safeRef();
    m_fillShader->safeRef();
    m_strokeShader->safeRef();
}

PlatformContextSkia::State::~State()
{
    m_looper->safeUnref();
    m_dash->safeUnref();
    m_fillShader->safeUnref();
    m_strokeShader->safeUnref();
}

SkColor PlatformContextSkia::State::applyAlpha(SkColor c) const
{
    int s = roundf(m_alpha * 256);
    if (s >= 256)
        return c;
    if (s < 0)
        return 0;

    int a = SkAlphaMul(SkColorGetA(c), s);
    return (c & 0x00FFFFFF) | (a << 24);
}

// PlatformContextSkia ---------------------------------------------------------

// Danger: canvas can be NULL.
PlatformContextSkia::PlatformContextSkia(skia::PlatformCanvas* canvas)
    : m_canvas(canvas)
#if OS(WINDOWS)
    , m_drawingToImageBuffer(false)
#endif
{
    m_stateStack.append(State());
    m_state = &m_stateStack.last();
}

PlatformContextSkia::~PlatformContextSkia()
{
}

void PlatformContextSkia::setCanvas(skia::PlatformCanvas* canvas)
{
    m_canvas = canvas;
}

#if OS(WINDOWS)
void PlatformContextSkia::setDrawingToImageBuffer(bool value)
{
    m_drawingToImageBuffer = value;
}

bool PlatformContextSkia::isDrawingToImageBuffer() const
{
    return m_drawingToImageBuffer;
}
#endif

void PlatformContextSkia::save()
{
    m_stateStack.append(*m_state);
    m_state = &m_stateStack.last();

#if OS(LINUX) || OS(WINDOWS)
    // The clip image only needs to be applied once. Reset the image so that we
    // don't attempt to clip multiple times.
    m_state->m_imageBufferClip.reset();
#endif

    // Save our native canvas.
    canvas()->save();
}

#if OS(LINUX) || OS(WINDOWS)
void PlatformContextSkia::beginLayerClippedToImage(const WebCore::FloatRect& rect,
                                                   const WebCore::ImageBuffer* imageBuffer)
{
    // Skia doesn't support clipping to an image, so we create a layer. The next
    // time restore is invoked the layer and |imageBuffer| are combined to
    // create the resulting image.
    m_state->m_clip = rect;
    SkRect bounds = { SkFloatToScalar(rect.x()), SkFloatToScalar(rect.y()),
                      SkFloatToScalar(rect.right()), SkFloatToScalar(rect.bottom()) };

    canvas()->clipRect(bounds);
    canvas()->saveLayerAlpha(&bounds, 255,
                             static_cast<SkCanvas::SaveFlags>(SkCanvas::kHasAlphaLayer_SaveFlag | SkCanvas::kFullColorLayer_SaveFlag));
    // Copy off the image as |imageBuffer| may be deleted before restore is invoked.
    const SkBitmap* bitmap = imageBuffer->context()->platformContext()->bitmap();
    if (!bitmap->pixelRef()) {
        // The bitmap owns it's pixels. This happens when we've allocated the
        // pixels in some way and assigned them directly to the bitmap (as
        // happens when we allocate a DIB). In this case the assignment operator
        // does not copy the pixels, rather the copied bitmap ends up
        // referencing the same pixels. As the pixels may not live as long as we
        // need it to, we copy the image.
        bitmap->copyTo(&m_state->m_imageBufferClip, SkBitmap::kARGB_8888_Config);
    } else {
        // If there is a pixel ref, we can safely use the assignment operator.
        m_state->m_imageBufferClip = *bitmap;
    }
}
#endif

void PlatformContextSkia::clipPathAntiAliased(const SkPath& clipPath)
{
    // If we are currently tracking any anti-alias clip paths, then we already
    // have a layer in place and don't need to add another.
    bool haveLayerOutstanding = m_state->m_antiAliasClipPaths.size();

    // See comments in applyAntiAliasedClipPaths about how this works.
    m_state->m_antiAliasClipPaths.append(clipPath);

    if (!haveLayerOutstanding) {
        SkRect bounds = clipPath.getBounds();
        canvas()->saveLayerAlpha(&bounds, 255, static_cast<SkCanvas::SaveFlags>(SkCanvas::kHasAlphaLayer_SaveFlag | SkCanvas::kFullColorLayer_SaveFlag | SkCanvas::kClipToLayer_SaveFlag));
    }
}

void PlatformContextSkia::restore()
{
#if OS(LINUX) || OS(WINDOWS)
    if (!m_state->m_imageBufferClip.empty()) {
        applyClipFromImage(m_state->m_clip, m_state->m_imageBufferClip);
        canvas()->restore();
    }
#endif

    if (!m_state->m_antiAliasClipPaths.isEmpty())
        applyAntiAliasedClipPaths(m_state->m_antiAliasClipPaths);

    m_stateStack.removeLast();
    m_state = &m_stateStack.last();

    // Restore our native canvas.
    canvas()->restore();
}

void PlatformContextSkia::drawRect(SkRect rect)
{
    SkPaint paint;
    int fillcolorNotTransparent = m_state->m_fillColor & 0xFF000000;
    if (fillcolorNotTransparent) {
        setupPaintForFilling(&paint);
        canvas()->drawRect(rect, paint);
    }

    if (m_state->m_strokeStyle != WebCore::NoStroke &&
        (m_state->m_strokeColor & 0xFF000000)) {
        // We do a fill of four rects to simulate the stroke of a border.
        SkColor oldFillColor = m_state->m_fillColor;

        // setFillColor() will set the shader to NULL, so save a ref to it now. 
        SkShader* oldFillShader = m_state->m_fillShader;
        oldFillShader->safeRef();
        setFillColor(m_state->m_strokeColor);
        paint.reset();
        setupPaintForFilling(&paint);
        SkRect topBorder = { rect.fLeft, rect.fTop, rect.fRight, rect.fTop + 1 };
        canvas()->drawRect(topBorder, paint);
        SkRect bottomBorder = { rect.fLeft, rect.fBottom - 1, rect.fRight, rect.fBottom };
        canvas()->drawRect(bottomBorder, paint);
        SkRect leftBorder = { rect.fLeft, rect.fTop + 1, rect.fLeft + 1, rect.fBottom - 1 };
        canvas()->drawRect(leftBorder, paint);
        SkRect rightBorder = { rect.fRight - 1, rect.fTop + 1, rect.fRight, rect.fBottom - 1 };
        canvas()->drawRect(rightBorder, paint);
        setFillColor(oldFillColor);
        setFillShader(oldFillShader);
        oldFillShader->safeUnref();
    }
}

void PlatformContextSkia::setupPaintCommon(SkPaint* paint) const
{
#if defined(SK_DEBUG)
    {
        SkPaint defaultPaint;
        SkASSERT(*paint == defaultPaint);
    }
#endif

    paint->setAntiAlias(m_state->m_useAntialiasing);
    paint->setXfermodeMode(m_state->m_xferMode);
    paint->setLooper(m_state->m_looper);
}

void PlatformContextSkia::setupPaintForFilling(SkPaint* paint) const
{
    setupPaintCommon(paint);
    paint->setColor(m_state->applyAlpha(m_state->m_fillColor));
    paint->setShader(m_state->m_fillShader);
}

static SkScalar scalarBound(SkScalar v, SkScalar min, SkScalar max)
{
    if (v < min)
        return min;
    if (v > max)
        return max;
    return v;
}

float PlatformContextSkia::setupPaintForStroking(SkPaint* paint, SkRect* rect, int length) const
{
    setupPaintCommon(paint);
    float width = m_state->m_strokeThickness;

    paint->setColor(m_state->applyAlpha(m_state->m_strokeColor));
    paint->setShader(m_state->m_strokeShader);
    paint->setStyle(SkPaint::kStroke_Style);
    // The limits here (512 and 256) were made up but are hopefully large
    // enough to be reasonable. They are, empirically, small enough not to
    // cause overflows in Skia.
    paint->setStrokeWidth(scalarBound(SkFloatToScalar(width), 0, 512));
    paint->setStrokeCap(m_state->m_lineCap);
    paint->setStrokeJoin(m_state->m_lineJoin);
    paint->setStrokeMiter(scalarBound(SkFloatToScalar(m_state->m_miterLimit), 0, 256));

    if (m_state->m_dash)
        paint->setPathEffect(m_state->m_dash);
    else {
        switch (m_state->m_strokeStyle) {
        case WebCore::NoStroke:
        case WebCore::SolidStroke:
            break;
        case WebCore::DashedStroke:
            width = m_state->m_dashRatio * width;
            // Fall through.
        case WebCore::DottedStroke:
            // Truncate the width, since we don't want fuzzy dots or dashes.
            int dashLength = static_cast<int>(width);
            // Subtract off the endcaps, since they're rendered separately.
            int distance = length - 2 * static_cast<int>(m_state->m_strokeThickness);
            int phase = 1;
            if (dashLength > 1) {
                // Determine how many dashes or dots we should have.
                int numDashes = distance / dashLength;
                int remainder = distance % dashLength;
                // Adjust the phase to center the dashes within the line.
                if (numDashes % 2 == 0) {
                    // Even:  shift right half a dash, minus half the remainder
                    phase = (dashLength - remainder) / 2;
                } else {
                    // Odd:  shift right a full dash, minus half the remainder
                    phase = dashLength - remainder / 2;
                }
            }
            SkScalar dashLengthSk = SkIntToScalar(dashLength);
            SkScalar intervals[2] = { dashLengthSk, dashLengthSk };
            paint->setPathEffect(new SkDashPathEffect(intervals, 2, SkIntToScalar(phase)))->unref();
        }
    }

    return width;
}

void PlatformContextSkia::setDrawLooper(SkDrawLooper* dl)
{
    SkRefCnt_SafeAssign(m_state->m_looper, dl);
}

void PlatformContextSkia::setMiterLimit(float ml)
{
    m_state->m_miterLimit = ml;
}

void PlatformContextSkia::setAlpha(float alpha)
{
    m_state->m_alpha = alpha;
}

void PlatformContextSkia::setLineCap(SkPaint::Cap lc)
{
    m_state->m_lineCap = lc;
}

void PlatformContextSkia::setLineJoin(SkPaint::Join lj)
{
    m_state->m_lineJoin = lj;
}

void PlatformContextSkia::setXfermodeMode(SkXfermode::Mode pdm)
{
    m_state->m_xferMode = pdm;
}

void PlatformContextSkia::setFillColor(SkColor color)
{
    m_state->m_fillColor = color;
    setFillShader(NULL);
}

SkDrawLooper* PlatformContextSkia::getDrawLooper() const
{
    return m_state->m_looper;
}

WebCore::StrokeStyle PlatformContextSkia::getStrokeStyle() const
{
    return m_state->m_strokeStyle;
}

void PlatformContextSkia::setStrokeStyle(WebCore::StrokeStyle strokeStyle)
{
    m_state->m_strokeStyle = strokeStyle;
}

void PlatformContextSkia::setStrokeColor(SkColor strokeColor)
{
    m_state->m_strokeColor = strokeColor;
    setStrokeShader(NULL);
}

float PlatformContextSkia::getStrokeThickness() const
{
    return m_state->m_strokeThickness;
}

void PlatformContextSkia::setStrokeThickness(float thickness)
{
    m_state->m_strokeThickness = thickness;
}

void PlatformContextSkia::setStrokeShader(SkShader* strokeShader)
{
    if (strokeShader != m_state->m_strokeShader) {
        m_state->m_strokeShader->safeUnref();
        m_state->m_strokeShader = strokeShader;
        m_state->m_strokeShader->safeRef();
    }
}

int PlatformContextSkia::getTextDrawingMode() const
{
    return m_state->m_textDrawingMode;
}

float PlatformContextSkia::getAlpha() const
{
    return m_state->m_alpha;
}

void PlatformContextSkia::setTextDrawingMode(int mode)
{
  // cTextClip is never used, so we assert that it isn't set:
  // https://bugs.webkit.org/show_bug.cgi?id=21898
  ASSERT((mode & WebCore::cTextClip) == 0);
  m_state->m_textDrawingMode = mode;
}

void PlatformContextSkia::setUseAntialiasing(bool enable)
{
    m_state->m_useAntialiasing = enable;
}

SkColor PlatformContextSkia::effectiveFillColor() const
{
    return m_state->applyAlpha(m_state->m_fillColor);
}

SkColor PlatformContextSkia::effectiveStrokeColor() const
{
    return m_state->applyAlpha(m_state->m_strokeColor);
}

void PlatformContextSkia::beginPath()
{
    m_path.reset();
}

void PlatformContextSkia::addPath(const SkPath& path)
{
    m_path.addPath(path, m_canvas->getTotalMatrix());
}

SkPath PlatformContextSkia::currentPathInLocalCoordinates() const
{
    SkPath localPath = m_path;
    const SkMatrix& matrix = m_canvas->getTotalMatrix();
    SkMatrix inverseMatrix;
    if (!matrix.invert(&inverseMatrix))
        return SkPath();
    localPath.transform(inverseMatrix);
    return localPath;
}

void PlatformContextSkia::setFillRule(SkPath::FillType fr)
{
    m_path.setFillType(fr);
}

void PlatformContextSkia::setFillShader(SkShader* fillShader)
{
    if (fillShader != m_state->m_fillShader) {
        m_state->m_fillShader->safeUnref();
        m_state->m_fillShader = fillShader;
        m_state->m_fillShader->safeRef();
    }
}

void PlatformContextSkia::setDashPathEffect(SkDashPathEffect* dash)
{
    if (dash != m_state->m_dash) {
        m_state->m_dash->safeUnref();
        m_state->m_dash = dash;
    }
}

void PlatformContextSkia::paintSkPaint(const SkRect& rect,
                                       const SkPaint& paint)
{
    m_canvas->drawRect(rect, paint);
}

const SkBitmap* PlatformContextSkia::bitmap() const
{
    return &m_canvas->getDevice()->accessBitmap(false);
}

bool PlatformContextSkia::isPrinting()
{
    return m_canvas->getTopPlatformDevice().IsVectorial();
}

#if OS(LINUX) || OS(WINDOWS)
void PlatformContextSkia::applyClipFromImage(const WebCore::FloatRect& rect, const SkBitmap& imageBuffer)
{
    // NOTE: this assumes the image mask contains opaque black for the portions that are to be shown, as such we
    // only look at the alpha when compositing. I'm not 100% sure this is what WebKit expects for image clipping.
    SkPaint paint;
    paint.setXfermodeMode(SkXfermode::kDstIn_Mode);
    m_canvas->drawBitmap(imageBuffer, SkFloatToScalar(rect.x()), SkFloatToScalar(rect.y()), &paint);
}
#endif

void PlatformContextSkia::applyAntiAliasedClipPaths(WTF::Vector<SkPath>& paths)
{
    // Anti-aliased clipping:
    //
    // Skia's clipping is 1-bit only. Consider what would happen if it were 8-bit:
    // We have a square canvas, filled with white and we declare a circular
    // clipping path. Then we fill twice with a black rectangle. The fractional
    // pixels would first get the correct color (white * alpha + black * (1 -
    // alpha)), but the second fill would apply the alpha to the already
    // modified color and the result would be too dark.
    //
    // This, anti-aliased clipping needs to be performed after the drawing has
    // been done. In order to do this, we create a new layer of the canvas in
    // clipPathAntiAliased and store the clipping path. All drawing is done to
    // the layer's bitmap while it's in effect. When WebKit calls restore() to
    // undo the clipping, this function is called.
    //
    // Here, we walk the list of clipping paths backwards and, for each, we
    // clear outside of the clipping path. We only need a single extra layer
    // for any number of clipping paths.
    //
    // When we call restore on the SkCanvas, the layer's bitmap is composed
    // into the layer below and we end up with correct, anti-aliased clipping.

    SkPaint paint;
    paint.setXfermodeMode(SkXfermode::kClear_Mode);
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kFill_Style);

    for (size_t i = paths.size() - 1; i < paths.size(); --i) {
        paths[i].setFillType(SkPath::kInverseWinding_FillType);
        m_canvas->drawPath(paths[i], paint);
    }

    m_canvas->restore();
}
