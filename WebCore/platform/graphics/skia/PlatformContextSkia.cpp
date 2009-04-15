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

// State -----------------------------------------------------------------------

// Encapsulates the additional painting state information we store for each
// pushed graphics state.
struct PlatformContextSkia::State {
    State();
    State(const State&);
    ~State();

    // Common shader state.
    float m_alpha;
    SkPorterDuff::Mode m_porterDuffMode;
    SkShader* m_gradient;
    SkShader* m_pattern;
    bool m_useAntialiasing;
    SkDrawLooper* m_looper;

    // Fill.
    SkColor m_fillColor;

    // Stroke.
    WebCore::StrokeStyle m_strokeStyle;
    SkColor m_strokeColor;
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

#if defined(__linux__) || PLATFORM(WIN_OS)
    // If non-empty, the current State is clipped to this image.
    SkBitmap m_imageBufferClip;
    // If m_imageBufferClip is non-empty, this is the region the image is clipped to.
    WebCore::FloatRect m_clip;
#endif

private:
    // Not supported.
    void operator=(const State&);
};

// Note: Keep theses default values in sync with GraphicsContextState.
PlatformContextSkia::State::State()
    : m_alpha(1)
    , m_porterDuffMode(SkPorterDuff::kSrcOver_Mode)
    , m_gradient(0)
    , m_pattern(0)
    , m_useAntialiasing(true)
    , m_looper(0)
    , m_fillColor(0xFF000000)
    , m_strokeStyle(WebCore::SolidStroke)
    , m_strokeColor(WebCore::Color::black)
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
    , m_porterDuffMode(other.m_porterDuffMode)
    , m_gradient(other.m_gradient)
    , m_pattern(other.m_pattern)
    , m_useAntialiasing(other.m_useAntialiasing)
    , m_looper(other.m_looper)
    , m_fillColor(other.m_fillColor)
    , m_strokeStyle(other.m_strokeStyle)
    , m_strokeColor(other.m_strokeColor)
    , m_strokeThickness(other.m_strokeThickness)
    , m_dashRatio(other.m_dashRatio)
    , m_miterLimit(other.m_miterLimit)
    , m_lineCap(other.m_lineCap)
    , m_lineJoin(other.m_lineJoin)
    , m_dash(other.m_dash)
    , m_textDrawingMode(other.m_textDrawingMode)
#if defined(__linux__) || PLATFORM(WIN_OS)
    , m_imageBufferClip(other.m_imageBufferClip)
    , m_clip(other.m_clip)
#endif
{
    // Up the ref count of these. saveRef does nothing if 'this' is NULL.
    m_looper->safeRef();
    m_dash->safeRef();
    m_gradient->safeRef();
    m_pattern->safeRef();
}

PlatformContextSkia::State::~State()
{
    m_looper->safeUnref();
    m_dash->safeUnref();
    m_gradient->safeUnref();
    m_pattern->safeUnref();
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
    , m_stateStack(sizeof(State))
#if PLATFORM(WIN_OS)
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

#if PLATFORM(WIN_OS)
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

#if defined(__linux__) || PLATFORM(WIN_OS)
    // The clip image only needs to be applied once. Reset the image so that we
    // don't attempt to clip multiple times.
    m_state->m_imageBufferClip.reset();
#endif

    // Save our native canvas.
    canvas()->save();
}

#if defined(__linux__) || PLATFORM(WIN_OS)
void PlatformContextSkia::beginLayerClippedToImage(const WebCore::FloatRect& rect,
                                                   const WebCore::ImageBuffer* imageBuffer)
{
    // Skia doesn't support clipping to an image, so we create a layer. The next
    // time restore is invoked the layer and |imageBuffer| are combined to
    // create the resulting image.
    m_state->m_clip = rect;
    SkRect bounds = { SkFloatToScalar(rect.x()), SkFloatToScalar(rect.y()),
                      SkFloatToScalar(rect.right()), SkFloatToScalar(rect.bottom()) };
                      
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

void PlatformContextSkia::restore()
{
#if defined(__linux__) || PLATFORM(WIN_OS)
    if (!m_state->m_imageBufferClip.empty()) {
        applyClipFromImage(m_state->m_clip, m_state->m_imageBufferClip);
        canvas()->restore();
    }
#endif

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
        if (oldFillColor != m_state->m_strokeColor)
            setFillColor(m_state->m_strokeColor);
        setupPaintForFilling(&paint);
        SkRect topBorder = { rect.fLeft, rect.fTop, rect.fRight, rect.fTop + 1 };
        canvas()->drawRect(topBorder, paint);
        SkRect bottomBorder = { rect.fLeft, rect.fBottom - 1, rect.fRight, rect.fBottom };
        canvas()->drawRect(bottomBorder, paint);
        SkRect leftBorder = { rect.fLeft, rect.fTop + 1, rect.fLeft + 1, rect.fBottom - 1 };
        canvas()->drawRect(leftBorder, paint);
        SkRect rightBorder = { rect.fRight - 1, rect.fTop + 1, rect.fRight, rect.fBottom - 1 };
        canvas()->drawRect(rightBorder, paint);
        if (oldFillColor != m_state->m_strokeColor)
            setFillColor(oldFillColor);
    }
}

void PlatformContextSkia::setupPaintCommon(SkPaint* paint) const
{
#ifdef SK_DEBUGx
    {
        SkPaint defaultPaint;
        SkASSERT(*paint == defaultPaint);
    }
#endif

    paint->setAntiAlias(m_state->m_useAntialiasing);
    paint->setPorterDuffXfermode(m_state->m_porterDuffMode);
    paint->setLooper(m_state->m_looper);

    if (m_state->m_gradient)
        paint->setShader(m_state->m_gradient);
    else if (m_state->m_pattern)
        paint->setShader(m_state->m_pattern);
}

void PlatformContextSkia::setupPaintForFilling(SkPaint* paint) const
{
    setupPaintCommon(paint);
    paint->setColor(m_state->applyAlpha(m_state->m_fillColor));
}

float PlatformContextSkia::setupPaintForStroking(SkPaint* paint, SkRect* rect, int length) const
{
    setupPaintCommon(paint);
    float width = m_state->m_strokeThickness;

    paint->setColor(m_state->applyAlpha(m_state->m_strokeColor));
    paint->setStyle(SkPaint::kStroke_Style);
    paint->setStrokeWidth(SkFloatToScalar(width));
    paint->setStrokeCap(m_state->m_lineCap);
    paint->setStrokeJoin(m_state->m_lineJoin);
    paint->setStrokeMiter(SkFloatToScalar(m_state->m_miterLimit));

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
            SkScalar dashLength;
            if (length) {
                // Determine about how many dashes or dots we should have.
                float roundedWidth = roundf(width);
                int numDashes = roundedWidth ? (length / roundedWidth) : length;
                if (!(numDashes & 1))
                    numDashes++;    // Make it odd so we end on a dash/dot.
                // Use the number of dashes to determine the length of a
                // dash/dot, which will be approximately width
                dashLength = SkScalarDiv(SkIntToScalar(length), SkIntToScalar(numDashes));
            } else
                dashLength = SkFloatToScalar(width);
            SkScalar intervals[2] = { dashLength, dashLength };
            paint->setPathEffect(new SkDashPathEffect(intervals, 2, 0))->unref();
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

void PlatformContextSkia::setPorterDuffMode(SkPorterDuff::Mode pdm)
{
    m_state->m_porterDuffMode = pdm;
}

void PlatformContextSkia::setFillColor(SkColor color)
{
    m_state->m_fillColor = color;
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
}

float PlatformContextSkia::getStrokeThickness() const
{
    return m_state->m_strokeThickness;
}

void PlatformContextSkia::setStrokeThickness(float thickness)
{
    m_state->m_strokeThickness = thickness;
}

int PlatformContextSkia::getTextDrawingMode() const
{
    return m_state->m_textDrawingMode;
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
    matrix.invert(&inverseMatrix);
    localPath.transform(inverseMatrix);
    return localPath;
}

void PlatformContextSkia::setFillRule(SkPath::FillType fr)
{
    m_path.setFillType(fr);
}

void PlatformContextSkia::setGradient(SkShader* gradient)
{
    if (gradient != m_state->m_gradient) {
        m_state->m_gradient->safeUnref();
        m_state->m_gradient = gradient;
    }
}

void PlatformContextSkia::setPattern(SkShader* pattern)
{
    if (pattern != m_state->m_pattern) {
        m_state->m_pattern->safeUnref();
        m_state->m_pattern = pattern;
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

#if defined(__linux__) || PLATFORM(WIN_OS)
void PlatformContextSkia::applyClipFromImage(const WebCore::FloatRect& rect, const SkBitmap& imageBuffer)
{
    // NOTE: this assumes the image mask contains opaque black for the portions that are to be shown, as such we
    // only look at the alpha when compositing. I'm not 100% sure this is what WebKit expects for image clipping.
    SkPaint paint;
    paint.setPorterDuffXfermode(SkPorterDuff::kDstIn_Mode);
    m_canvas->drawBitmap(imageBuffer, SkFloatToScalar(rect.x()), SkFloatToScalar(rect.y()), &paint);
}
#endif
