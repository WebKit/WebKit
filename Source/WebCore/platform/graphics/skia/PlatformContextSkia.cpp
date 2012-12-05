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

#include "PlatformContextSkia.h"

#include "Extensions3D.h"
#include "GraphicsContext.h"
#include "GraphicsContext3D.h"
#include "ImageBuffer.h"
#include "NativeImageSkia.h"
#include "SkiaUtils.h"
#include "Texture.h"
#include "TilingData.h"

#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"

#include "SkBitmap.h"
#include "SkColorPriv.h"
#include "SkDashPathEffect.h"
#include "SkShader.h"

#include <wtf/MathExtras.h>
#include <wtf/Vector.h>

#if PLATFORM(CHROMIUM)
#include "TraceEvent.h"
#endif

namespace WebCore {

struct PlatformContextSkia::DeferredSaveState {
    DeferredSaveState(unsigned mask, int count) : m_flags(mask), m_restoreCount(count) { }

    unsigned m_flags;
    int m_restoreCount;
};

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

    // Stroke.
    StrokeStyle m_strokeStyle;
    SkColor m_strokeColor;
    float m_strokeThickness;
    int m_dashRatio;  // Ratio of the length of a dash to its width.
    float m_miterLimit;
    SkPaint::Cap m_lineCap;
    SkPaint::Join m_lineJoin;
    SkDashPathEffect* m_dash;

    // Text. (See TextModeFill & friends in GraphicsContext.h.)
    TextDrawingModeFlags m_textDrawingMode;

    // Helper function for applying the state's alpha value to the given input
    // color to produce a new output color.
    SkColor applyAlpha(SkColor) const;

    // If non-empty, the current State is clipped to this image.
    SkBitmap m_imageBufferClip;
    // If m_imageBufferClip is non-empty, this is the region the image is clipped to.
    SkRect m_clip;

    InterpolationQuality m_interpolationQuality;

    PlatformContextSkia::State cloneInheritedProperties();
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
    , m_strokeStyle(SolidStroke)
    , m_strokeColor(Color::black)
    , m_strokeThickness(0)
    , m_dashRatio(3)
    , m_miterLimit(4)
    , m_lineCap(SkPaint::kDefault_Cap)
    , m_lineJoin(SkPaint::kDefault_Join)
    , m_dash(0)
    , m_textDrawingMode(TextModeFill)
#if USE(LOW_QUALITY_IMAGE_INTERPOLATION)
    , m_interpolationQuality(InterpolationLow)
#else
    , m_interpolationQuality(InterpolationHigh)
#endif
{
}

PlatformContextSkia::State::State(const State& other)
    : m_alpha(other.m_alpha)
    , m_xferMode(other.m_xferMode)
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
    , m_imageBufferClip(other.m_imageBufferClip)
    , m_clip(other.m_clip)
    , m_interpolationQuality(other.m_interpolationQuality)
{
    // Up the ref count of these. SkSafeRef does nothing if its argument is 0.
    SkSafeRef(m_looper);
    SkSafeRef(m_dash);
}

PlatformContextSkia::State::~State()
{
    SkSafeUnref(m_looper);
    SkSafeUnref(m_dash);
}

// Returns a new State with all of this object's inherited properties copied.
PlatformContextSkia::State PlatformContextSkia::State::cloneInheritedProperties()
{
    return PlatformContextSkia::State(*this);
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
PlatformContextSkia::PlatformContextSkia(SkCanvas* canvas)
    : m_canvas(canvas)
    , m_deferredSaveFlags(0)
    , m_trackOpaqueRegion(false)
    , m_printing(false)
    , m_accelerated(false)
    , m_deferred(false)
    , m_drawingToImageBuffer(false)
    , m_deviceScaleFactor(1)
#if defined(SK_SUPPORT_HINTING_SCALE_FACTOR)
    , m_hintingScaleFactor(SK_Scalar1)
#endif
{
    m_stateStack.append(State());
    m_state = &m_stateStack.last();

    // will be assigned in setGraphicsContext()
    m_gc = 0;
}

PlatformContextSkia::~PlatformContextSkia()
{
}

void PlatformContextSkia::setCanvas(SkCanvas* canvas)
{
    m_canvas = canvas;
}

void PlatformContextSkia::setDrawingToImageBuffer(bool value)
{
    m_drawingToImageBuffer = value;
}

SkDevice* PlatformContextSkia::createCompatibleDevice(const IntSize& size, bool hasAlpha)
{
    return m_canvas->createCompatibleDevice(bitmap()->config(), size.width(), size.height(), !hasAlpha);
}

bool PlatformContextSkia::isDrawingToImageBuffer() const
{
    return m_drawingToImageBuffer;
}

void PlatformContextSkia::save()
{
    m_stateStack.append(m_state->cloneInheritedProperties());
    m_state = &m_stateStack.last();

    // The clip image only needs to be applied once. Reset the image so that we
    // don't attempt to clip multiple times.
    m_state->m_imageBufferClip.reset();

    m_saveStateStack.append(DeferredSaveState(m_deferredSaveFlags, m_canvas->getSaveCount()));
    m_deferredSaveFlags |= SkCanvas::kMatrixClip_SaveFlag;
}

void PlatformContextSkia::saveLayer(const SkRect* bounds, const SkPaint* paint, SkCanvas::SaveFlags saveFlags)
{
    realizeSave(SkCanvas::kMatrixClip_SaveFlag);

    m_canvas->saveLayer(bounds, paint, saveFlags);
    if (bounds)
        m_canvas->clipRect(*bounds);
    if (m_trackOpaqueRegion)
        m_opaqueRegion.pushCanvasLayer(paint);
}

void PlatformContextSkia::restoreLayer()
{
    m_canvas->restore();
    if (m_trackOpaqueRegion)
        m_opaqueRegion.popCanvasLayer(this);
}

void PlatformContextSkia::beginLayerClippedToImage(const FloatRect& rect,
                                                   const ImageBuffer* imageBuffer)
{
    SkRect bounds = { SkFloatToScalar(rect.x()), SkFloatToScalar(rect.y()),
                      SkFloatToScalar(rect.maxX()), SkFloatToScalar(rect.maxY()) };

    if (imageBuffer->internalSize().isEmpty()) {
        clipRect(bounds);
        return;
    }

    // Skia doesn't support clipping to an image, so we create a layer. The next
    // time restore is invoked the layer and |imageBuffer| are combined to
    // create the resulting image.

    m_state->m_clip = bounds;
    // Get the absolute coordinates of the stored clipping rectangle to make it
    // independent of any transform changes.
    getTotalMatrix().mapRect(&m_state->m_clip);

    SkCanvas::SaveFlags saveFlags = static_cast<SkCanvas::SaveFlags>(SkCanvas::kHasAlphaLayer_SaveFlag | SkCanvas::kFullColorLayer_SaveFlag);
    saveLayer(&bounds, 0, saveFlags);

    const SkBitmap* bitmap = imageBuffer->context()->platformContext()->bitmap();

    if (m_trackOpaqueRegion) {
        SkRect opaqueRect = bitmap->isOpaque() ? m_state->m_clip : SkRect::MakeEmpty();
        m_opaqueRegion.setImageMask(opaqueRect);
    }

    // Copy off the image as |imageBuffer| may be deleted before restore is invoked.
    if (bitmap->isImmutable())
        m_state->m_imageBufferClip = *bitmap;
    else {
        // We need to make a deep-copy of the pixels themselves, so they don't
        // change on us between now and when we want to apply them in restore()
        bitmap->copyTo(&m_state->m_imageBufferClip, SkBitmap::kARGB_8888_Config);
    }
}

void PlatformContextSkia::restore()
{
    if (!m_state->m_imageBufferClip.empty()) {
        applyClipFromImage(m_state->m_clip, m_state->m_imageBufferClip);
        m_canvas->restore();
    }

    m_stateStack.removeLast();
    m_state = &m_stateStack.last();

    DeferredSaveState savedState = m_saveStateStack.last();
    m_saveStateStack.removeLast();
    m_deferredSaveFlags = savedState.m_flags;
    m_canvas->restoreToCount(savedState.m_restoreCount);
}

void PlatformContextSkia::drawRect(SkRect rect)
{
    SkPaint paint;
    int fillcolorNotTransparent = m_state->m_fillColor & 0xFF000000;
    if (fillcolorNotTransparent) {
        setupPaintForFilling(&paint);
        drawRect(rect, paint);
    }

    if (m_state->m_strokeStyle != NoStroke
        && (m_state->m_strokeColor & 0xFF000000)) {
        // We do a fill of four rects to simulate the stroke of a border.
        paint.reset();
        setupPaintForFilling(&paint);
        // need to jam in the strokeColor
        paint.setColor(this->effectiveStrokeColor());

        SkRect topBorder = { rect.fLeft, rect.fTop, rect.fRight, rect.fTop + 1 };
        drawRect(topBorder, paint);
        SkRect bottomBorder = { rect.fLeft, rect.fBottom - 1, rect.fRight, rect.fBottom };
        drawRect(bottomBorder, paint);
        SkRect leftBorder = { rect.fLeft, rect.fTop + 1, rect.fLeft + 1, rect.fBottom - 1 };
        drawRect(leftBorder, paint);
        SkRect rightBorder = { rect.fRight - 1, rect.fTop + 1, rect.fRight, rect.fBottom - 1 };
        drawRect(rightBorder, paint);
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
#if defined(SK_SUPPORT_HINTING_SCALE_FACTOR)
    paint->setHintingScaleFactor(m_hintingScaleFactor);
#endif
}

void PlatformContextSkia::setupShader(SkPaint* paint, Gradient* grad, Pattern* pat, SkColor color) const
{
    SkShader* shader = 0;

    if (grad) {
        shader = grad->platformGradient();
        color = SK_ColorBLACK;
    } else if (pat) {
        shader = pat->platformPattern(m_gc->getCTM());
        color = SK_ColorBLACK;
        paint->setFilterBitmap(interpolationQuality() != InterpolationNone);
    }

    paint->setColor(m_state->applyAlpha(color));
    paint->setShader(shader);
}

void PlatformContextSkia::setupPaintForFilling(SkPaint* paint) const
{
    setupPaintCommon(paint);

    const GraphicsContextState& state = m_gc->state();
    setupShader(paint, state.fillGradient.get(), state.fillPattern.get(), m_state->m_fillColor);
}

float PlatformContextSkia::setupPaintForStroking(SkPaint* paint, SkRect* rect, int length) const
{
    setupPaintCommon(paint);

    const GraphicsContextState& state = m_gc->state();
    setupShader(paint, state.strokeGradient.get(), state.strokePattern.get(), m_state->m_strokeColor);

    float width = m_state->m_strokeThickness;

    paint->setStyle(SkPaint::kStroke_Style);
    paint->setStrokeWidth(SkFloatToScalar(width));
    paint->setStrokeCap(m_state->m_lineCap);
    paint->setStrokeJoin(m_state->m_lineJoin);
    paint->setStrokeMiter(SkFloatToScalar(m_state->m_miterLimit));

    if (m_state->m_dash)
        paint->setPathEffect(m_state->m_dash);
    else {
        switch (m_state->m_strokeStyle) {
        case NoStroke:
        case SolidStroke:
#if ENABLE(CSS3_TEXT)
        case DoubleStroke:
        case WavyStroke: // FIXME: https://bugs.webkit.org/show_bug.cgi?id=93509 - Needs platform support.
#endif // CSS3_TEXT
            break;
        case DashedStroke:
            width = m_state->m_dashRatio * width;
            // Fall through.
        case DottedStroke:
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
}

SkDrawLooper* PlatformContextSkia::getDrawLooper() const
{
    return m_state->m_looper;
}

StrokeStyle PlatformContextSkia::getStrokeStyle() const
{
    return m_state->m_strokeStyle;
}

void PlatformContextSkia::setStrokeStyle(StrokeStyle strokeStyle)
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

TextDrawingModeFlags PlatformContextSkia::getTextDrawingMode() const
{
    return m_state->m_textDrawingMode;
}

float PlatformContextSkia::getAlpha() const
{
    return m_state->m_alpha;
}

int PlatformContextSkia::getNormalizedAlpha() const
{
    int alpha = roundf(m_state->m_alpha * 256);
    if (alpha > 255)
        alpha = 255;
    else if (alpha < 0)
        alpha = 0;
    return alpha;
}

SkXfermode::Mode PlatformContextSkia::getXfermodeMode() const
{
    return m_state->m_xferMode;
}

void PlatformContextSkia::setTextDrawingMode(TextDrawingModeFlags mode)
{
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

InterpolationQuality PlatformContextSkia::interpolationQuality() const
{
    return m_state->m_interpolationQuality;
}

void PlatformContextSkia::setInterpolationQuality(InterpolationQuality interpolationQuality)
{
    m_state->m_interpolationQuality = interpolationQuality;
}

void PlatformContextSkia::setDashPathEffect(SkDashPathEffect* dash)
{
    if (dash != m_state->m_dash) {
        SkSafeUnref(m_state->m_dash);
        m_state->m_dash = dash;
    }
}

const SkBitmap* PlatformContextSkia::bitmap() const
{
#if PLATFORM(CHROMIUM)
    TRACE_EVENT0("skia", "PlatformContextSkia::bitmap");
#endif
    return &m_canvas->getDevice()->accessBitmap(false);
}

bool PlatformContextSkia::isNativeFontRenderingAllowed()
{
#if USE(SKIA_TEXT)
    return false;
#else
    if (isAccelerated())
        return false;
    return skia::SupportsPlatformPaint(m_canvas);
#endif
}

void PlatformContextSkia::applyClipFromImage(const SkRect& rect, const SkBitmap& imageBuffer)
{
    // NOTE: this assumes the image mask contains opaque black for the portions that are to be shown, as such we
    // only look at the alpha when compositing. I'm not 100% sure this is what WebKit expects for image clipping.
    SkPaint paint;
    paint.setXfermodeMode(SkXfermode::kDstIn_Mode);
    realizeSave(SkCanvas::kMatrixClip_SaveFlag);
    m_canvas->save(SkCanvas::kMatrix_SaveFlag);
    m_canvas->resetMatrix();
    m_canvas->drawBitmapRect(imageBuffer, 0, rect, &paint);
    m_canvas->restore();
}

void PlatformContextSkia::didDrawRect(const SkRect& rect, const SkPaint& paint, const SkBitmap* bitmap)
{
    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawRect(this, rect, paint, bitmap);
}

void PlatformContextSkia::adjustTextRenderMode(SkPaint* paint)
{
    if (!paint->isLCDRenderText())
        return;

    paint->setLCDRenderText(couldUseLCDRenderedText());
}

bool PlatformContextSkia::couldUseLCDRenderedText()
{
    // Our layers only have a single alpha channel. This means that subpixel
    // rendered text cannot be composited correctly when the layer is
    // collapsed. Therefore, subpixel text is disabled when we are drawing
    // onto a layer.
    if (isDrawingToLayer())
        return false;

    // If this text is not in an image buffer and so won't be externally
    // composited, then subpixel antialiasing is fine.
    return !isDrawingToImageBuffer();
}

} // namespace WebCore
