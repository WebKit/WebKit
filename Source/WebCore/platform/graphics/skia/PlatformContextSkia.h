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

#ifndef PlatformContextSkia_h
#define PlatformContextSkia_h

#include "GraphicsContext.h"
#include "OpaqueRegionSkia.h"

#include "SkCanvas.h"
#include "SkDashPathEffect.h"
#include "SkDevice.h"
#include "SkDrawLooper.h"
#include "SkPaint.h"
#include "SkPath.h"
#include "SkRRect.h"

#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace WebCore {

enum CompositeOperator;
class Texture;

// This class holds the platform-specific state for GraphicsContext. We put
// most of our Skia wrappers on this class. In theory, a lot of this stuff could
// be moved to GraphicsContext directly, except that some code external to this
// would like to poke at our graphics layer as well (like the Image and Font
// stuff, which needs some amount of our wrappers and state around SkCanvas).
//
// So in general, this class uses just Skia types except when there's no easy
// conversion. GraphicsContext is responsible for converting the WebKit types to
// Skia types and setting up the eventual call to the Skia functions.
//
// This class then keeps track of all the current Skia state. WebKit expects
// that the graphics state that is pushed and popped by save() and restore()
// includes things like colors and pen styles. Skia does this differently, where
// push and pop only includes transforms and bitmaps, and the application is
// responsible for managing the painting state which is store in separate
// SkPaint objects. This class provides the adaptor that allows the painting
// state to be pushed and popped along with the bitmap.
class PlatformContextSkia {
    WTF_MAKE_NONCOPYABLE(PlatformContextSkia);
public:
    // For printing, there shouldn't be any canvas. canvas can be NULL. If you
    // supply a NULL canvas, you can also call setCanvas later.
    PlatformContextSkia(SkCanvas*);
    ~PlatformContextSkia();

    // Sets the graphics context associated with this context.
    // GraphicsContextSkia calls this from its constructor.
    void setGraphicsContext(const GraphicsContext* gc) { m_gc = gc; }

    // Sets the canvas associated with this context. Use when supplying NULL
    // to the constructor.
    void setCanvas(SkCanvas*);

    SkDevice* createCompatibleDevice(const IntSize&, bool hasAlpha);

    // If false we're rendering to a GraphicsContext for a web page, if false
    // we're not (as is the case when rendering to a canvas object).
    // If this is true the contents have not been marked up with the magic
    // color and all text drawing needs to go to a layer so that the alpha is
    // correctly updated.
    void setDrawingToImageBuffer(bool);
    bool isDrawingToImageBuffer() const;

    void save();
    void restore();

    void saveLayer(const SkRect* bounds, const SkPaint*, SkCanvas::SaveFlags = SkCanvas::kARGB_ClipLayer_SaveFlag);
    void restoreLayer();

    // Begins a layer that is clipped to the image |imageBuffer| at the location
    // |rect|. This layer is implicitly restored when the next restore is
    // invoked.
    // NOTE: |imageBuffer| may be deleted before the |restore| is invoked.
    void beginLayerClippedToImage(const FloatRect&, const ImageBuffer*);

    // Sets up the common flags on a paint for antialiasing, effects, etc.
    // This is implicitly called by setupPaintFill and setupPaintStroke, but
    // you may wish to call it directly sometimes if you don't want that other
    // behavior.
    void setupPaintCommon(SkPaint*) const;

    // Sets up the paint for the current fill style.
    void setupPaintForFilling(SkPaint*) const;

    // Sets up the paint for stroking. Returns an int representing the width of
    // the pen, or 1 if the pen's width is 0 if a non-zero length is provided,
    // the number of dashes/dots on a dashed/dotted line will be adjusted to
    // start and end that length with a dash/dot.
    float setupPaintForStroking(SkPaint*, SkRect*, int length) const;

    // State setting functions.
    void setDrawLooper(SkDrawLooper*);  // Note: takes an additional ref.
    void setMiterLimit(float);
    void setAlpha(float);
    void setLineCap(SkPaint::Cap);
    void setLineJoin(SkPaint::Join);
    void setXfermodeMode(SkXfermode::Mode);
    void setFillColor(SkColor);
    void setStrokeStyle(StrokeStyle);
    void setStrokeColor(SkColor);
    void setStrokeThickness(float thickness);
    void setTextDrawingMode(TextDrawingModeFlags mode);
    void setUseAntialiasing(bool enable);
    void setDashPathEffect(SkDashPathEffect*);

    SkDrawLooper* getDrawLooper() const;
    StrokeStyle getStrokeStyle() const;
    float getStrokeThickness() const;
    TextDrawingModeFlags getTextDrawingMode() const;
    float getAlpha() const;
    int getNormalizedAlpha() const;
    SkXfermode::Mode getXfermodeMode() const;

    // Returns the fill color. The returned color has it's alpha adjusted
    // by the current alpha.
    SkColor effectiveFillColor() const;

    // Returns the stroke color. The returned color has it's alpha adjusted
    // by the current alpha.
    SkColor effectiveStrokeColor() const;

    // Returns the canvas used for painting, NOT guaranteed to be non-null.
    // Accessing the backing canvas this way flushes all queued save ops,
    // so it should be avoided. Use the corresponding PlatformContextSkia
    // draw/matrix/clip methods instead.
    SkCanvas* canvas();
    const SkCanvas* canvas() const { return m_canvas; }

    InterpolationQuality interpolationQuality() const;
    void setInterpolationQuality(InterpolationQuality interpolationQuality);

    // FIXME: This should be pushed down to GraphicsContext.
    void drawRect(SkRect rect);

    // Returns if the context is a printing context instead of a display
    // context. Bitmap shouldn't be resampled when printing to keep the best
    // possible quality.
    bool printing() const { return m_printing; }
    void setPrinting(bool p) { m_printing = p; }

    // Returns if the context allows rendering of fonts using native platform
    // APIs. If false is returned font rendering is performed using the skia
    // text drawing APIs.
    // if USE(SKIA_TEXT) is enabled, this always returns false
    bool isNativeFontRenderingAllowed();

    bool isAccelerated() const { return m_accelerated; }
    void setAccelerated(bool accelerated) { m_accelerated = accelerated; }

    // True if this context is deferring draw calls to be executed later.
    // We need to know this for context-to-context draws, in order to know if
    // the source bitmap needs to be copied.
    bool isDeferred() const { return m_deferred; }
    void setDeferred(bool deferred) { m_deferred = deferred; }

    float deviceScaleFactor() const { return m_deviceScaleFactor; }
    void setDeviceScaleFactor(float scale) { m_deviceScaleFactor = scale; }

    void setTrackOpaqueRegion(bool track) { m_trackOpaqueRegion = track; }

    // This will be an empty region unless tracking is enabled.
    const OpaqueRegionSkia& opaqueRegion() const { return m_opaqueRegion; }

    // After drawing directly to the context's canvas, use this function to notify the context so
    // it can track the opaque region.
    // FIXME: this is still needed only because ImageSkia::paintSkBitmap() may need to notify for a
    //        smaller rect than the one drawn to, due to its clipping logic.
    void didDrawRect(const SkRect&, const SkPaint&, const SkBitmap* = 0);

    // Turn off LCD text for the paint if not supported on this context.
    void adjustTextRenderMode(SkPaint*);
    bool couldUseLCDRenderedText();

    enum AntiAliasingMode {
        NotAntiAliased,
        AntiAliased
    };
    enum AccessMode {
        ReadOnly,
        ReadWrite
    };

    // SkCanvas wrappers.
    const SkBitmap* bitmap() const;
    const SkBitmap& layerBitmap(AccessMode = ReadOnly) const;
    bool readPixels(SkBitmap*, int x, int y,
        SkCanvas::Config8888 = SkCanvas::kNative_Premul_Config8888);
    void writePixels(const SkBitmap&, int x, int y,
        SkCanvas::Config8888 = SkCanvas::kNative_Premul_Config8888);
    bool isDrawingToLayer() const;
    bool isVector() const;

    bool clipPath(const SkPath&, AntiAliasingMode = NotAntiAliased,
        SkRegion::Op = SkRegion::kIntersect_Op);
    bool clipRect(const SkRect&, AntiAliasingMode = NotAntiAliased,
        SkRegion::Op = SkRegion::kIntersect_Op);
    bool getClipBounds(SkRect*) const;

    void setMatrix(const SkMatrix&);
    const SkMatrix& getTotalMatrix() const;
    bool concat(const SkMatrix&);
    bool rotate(SkScalar degrees);
    bool scale(SkScalar sx, SkScalar sy);
    bool translate(SkScalar dx, SkScalar dy);

    void drawBitmap(const SkBitmap&, SkScalar, SkScalar, const SkPaint* = 0);
    void drawBitmapRect(const SkBitmap&, const SkIRect*, const SkRect&, const SkPaint* = 0);
    void drawOval(const SkRect&, const SkPaint&);
    void drawPath(const SkPath&, const SkPaint&);
    void drawPoints(SkCanvas::PointMode, size_t count, const SkPoint pts[], const SkPaint&);
    void drawRect(const SkRect&, const SkPaint&);
    void drawIRect(const SkIRect&, const SkPaint&);
    void drawRRect(const SkRRect&, const SkPaint&);
    void drawPosText(const void* text, size_t byteLength, const SkPoint pos[], const SkPaint&);
    void drawPosTextH(const void* text, size_t byteLength, const SkScalar xpos[], SkScalar constY,
        const SkPaint&);
    void drawTextOnPath(const void* text, size_t byteLength, const SkPath&,
        const SkMatrix*, const SkPaint&);

#if defined(SK_SUPPORT_HINTING_SCALE_FACTOR)
    void setHintingScaleFactor(SkScalar factor) { m_hintingScaleFactor = factor; }
    SkScalar hintingScaleFactor() const { return m_hintingScaleFactor; }
#endif

private:
    // Used when restoring and the state has an image clip. Only shows the pixels in
    // m_canvas that are also in imageBuffer.
    // The clipping rectangle is given in absolute coordinates.
    void applyClipFromImage(const SkRect&, const SkBitmap&);

    // common code between setupPaintFor[Filling,Stroking]
    void setupShader(SkPaint*, Gradient*, Pattern*, SkColor) const;

    void realizeSave(SkCanvas::SaveFlags);

    // Defines drawing style.
    struct State;

    struct DeferredSaveState;

    // NULL indicates painting is disabled. Never delete this object.
    SkCanvas* m_canvas;
    const GraphicsContext* m_gc;

    // States stack. Enables local drawing state change with save()/restore()
    // calls.
    WTF::Vector<State> m_stateStack;
    // Pointer to the current drawing state. This is a cached value of
    // mStateStack.back().
    State* m_state;

    WTF::Vector<DeferredSaveState> m_saveStateStack;

    // Currently pending save flags.
    // FIXME: While defined as a bitmask of SkCanvas::SaveFlags, this is mostly used as a bool.
    //        It will come in handy when adding granular save() support (clip vs. matrix vs. paint).
    unsigned m_deferredSaveFlags;

    // Tracks the region painted opaque via the GraphicsContext.
    OpaqueRegionSkia m_opaqueRegion;
    bool m_trackOpaqueRegion;

    bool m_printing;
    bool m_accelerated;
    bool m_deferred;
    bool m_drawingToImageBuffer;
    float m_deviceScaleFactor;
#if defined(SK_SUPPORT_HINTING_SCALE_FACTOR)
    SkScalar m_hintingScaleFactor;
#endif
};

inline void PlatformContextSkia::realizeSave(SkCanvas::SaveFlags flags)
{
    if (m_deferredSaveFlags & flags) {
        m_canvas->save((SkCanvas::SaveFlags)m_deferredSaveFlags);
        m_deferredSaveFlags = 0;
    }
}

inline SkCanvas* PlatformContextSkia::canvas()
{
    // Flush any pending saves.
    realizeSave(SkCanvas::kMatrixClip_SaveFlag);

    return m_canvas;
}

inline const SkBitmap& PlatformContextSkia::layerBitmap(AccessMode access) const
{
    return m_canvas->getTopDevice()->accessBitmap(access == ReadWrite);
}

inline bool PlatformContextSkia::readPixels(SkBitmap* bitmap, int x, int y,
    SkCanvas::Config8888 config8888)
{
    return m_canvas->readPixels(bitmap, x, y, config8888);
}

inline void PlatformContextSkia::writePixels(const SkBitmap& bitmap, int x, int y,
    SkCanvas::Config8888 config8888)
{
    m_canvas->writePixels(bitmap, x, y, config8888);

    if (m_trackOpaqueRegion) {
        SkRect rect = SkRect::MakeXYWH(x, y, bitmap.width(), bitmap.height());
        SkPaint paint;

        paint.setXfermodeMode(SkXfermode::kSrc_Mode);
        m_opaqueRegion.didDrawRect(this, rect, paint, &bitmap);
    }
}

inline bool PlatformContextSkia::isDrawingToLayer() const
{
    return m_canvas->isDrawingToLayer();
}

inline bool PlatformContextSkia::isVector() const
{
    return m_canvas->getTopDevice()->getDeviceCapabilities() & SkDevice::kVector_Capability;
}

inline bool PlatformContextSkia::clipPath(const SkPath& path, AntiAliasingMode aa, SkRegion::Op op)
{
    realizeSave(SkCanvas::kClip_SaveFlag);

    return m_canvas->clipPath(path, op, aa == AntiAliased);
}

inline bool PlatformContextSkia::clipRect(const SkRect& rect, AntiAliasingMode aa, SkRegion::Op op)
{
    realizeSave(SkCanvas::kClip_SaveFlag);

    return m_canvas->clipRect(rect, op, aa == AntiAliased);
}

inline bool PlatformContextSkia::getClipBounds(SkRect* bounds) const
{
    return m_canvas->getClipBounds(bounds);
}

inline void PlatformContextSkia::setMatrix(const SkMatrix& matrix)
{
    realizeSave(SkCanvas::kMatrix_SaveFlag);

    m_canvas->setMatrix(matrix);
}

inline const SkMatrix& PlatformContextSkia::getTotalMatrix() const
{
    return m_canvas->getTotalMatrix();
}

inline bool PlatformContextSkia::concat(const SkMatrix& matrix)
{
    realizeSave(SkCanvas::kMatrix_SaveFlag);

    return m_canvas->concat(matrix);
}

inline bool PlatformContextSkia::rotate(SkScalar degrees)
{
    realizeSave(SkCanvas::kMatrix_SaveFlag);

    return m_canvas->rotate(degrees);
}

inline bool PlatformContextSkia::scale(SkScalar sx, SkScalar sy)
{
    realizeSave(SkCanvas::kMatrix_SaveFlag);

    return m_canvas->scale(sx, sy);
}

inline bool PlatformContextSkia::translate(SkScalar dx, SkScalar dy)
{
    realizeSave(SkCanvas::kMatrix_SaveFlag);

    return m_canvas->translate(dx, dy);
}

inline void PlatformContextSkia::drawBitmap(const SkBitmap& bitmap, SkScalar left, SkScalar top,
    const SkPaint* paint)
{
    m_canvas->drawBitmap(bitmap, left, top, paint);

    if (m_trackOpaqueRegion) {
        SkRect rect = SkRect::MakeXYWH(left, top, bitmap.width(), bitmap.height());
        m_opaqueRegion.didDrawRect(this, rect, *paint, &bitmap);
    }
}

inline void PlatformContextSkia::drawBitmapRect(const SkBitmap& bitmap, const SkIRect* isrc,
    const SkRect& dst, const SkPaint* paint)
{
    m_canvas->drawBitmapRect(bitmap, isrc, dst, paint);

    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawRect(this, dst, *paint, &bitmap);
}

inline void PlatformContextSkia::drawOval(const SkRect& oval, const SkPaint& paint)
{
    m_canvas->drawOval(oval, paint);

    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawBounded(this, oval, paint);
}

inline void PlatformContextSkia::drawPath(const SkPath& path, const SkPaint& paint)
{
    m_canvas->drawPath(path, paint);

    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawPath(this, path, paint);
}

inline void PlatformContextSkia::drawPoints(SkCanvas::PointMode mode, size_t count,
    const SkPoint pts[], const SkPaint& paint)
{
    m_canvas->drawPoints(mode, count, pts, paint);

    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawPoints(this, mode, count, pts, paint);
}

inline void PlatformContextSkia::drawRect(const SkRect& rect, const SkPaint& paint)
{
    m_canvas->drawRect(rect, paint);

    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawRect(this, rect, paint, 0);
}

inline void PlatformContextSkia::drawIRect(const SkIRect& rect, const SkPaint& paint)
{
    m_canvas->drawIRect(rect, paint);

    if (m_trackOpaqueRegion) {
        SkRect r = SkRect::MakeFromIRect(rect);
        m_opaqueRegion.didDrawRect(this, r, paint, 0);
    }
}

inline void PlatformContextSkia::drawRRect(const SkRRect& rect, const SkPaint& paint)
{
    m_canvas->drawRRect(rect, paint);

    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawRect(this, rect.getBounds(), paint, 0);
}

inline void PlatformContextSkia::drawPosText(const void* text, size_t byteLength,
    const SkPoint pos[], const SkPaint& paint)
{
    m_canvas->drawPosText(text, byteLength, pos, paint);

    // FIXME: compute bounds for positioned text.
    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawUnbounded(this, paint, OpaqueRegionSkia::FillOrStroke);
}

inline void PlatformContextSkia::drawPosTextH(const void* text, size_t byteLength,
    const SkScalar xpos[], SkScalar constY, const SkPaint& paint)
{
    m_canvas->drawPosTextH(text, byteLength, xpos, constY, paint);

    // FIXME: compute bounds for positioned text.
    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawUnbounded(this, paint, OpaqueRegionSkia::FillOrStroke);
}

inline void PlatformContextSkia::drawTextOnPath(const void* text, size_t byteLength,
    const SkPath& path, const SkMatrix* matrix, const SkPaint& paint)
{
    m_canvas->drawTextOnPath(text, byteLength, path, matrix, paint);

    // FIXME: compute bounds for positioned text.
    if (m_trackOpaqueRegion)
        m_opaqueRegion.didDrawUnbounded(this, paint, OpaqueRegionSkia::FillOrStroke);
}

}
#endif // PlatformContextSkia_h
