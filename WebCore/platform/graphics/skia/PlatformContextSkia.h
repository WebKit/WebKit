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
#include "Noncopyable.h"

#include "SkDashPathEffect.h"
#include "SkDrawLooper.h"
#include "SkPaint.h"
#include "SkPath.h"
#include "skia/ext/platform_canvas.h"

#include <wtf/Vector.h>

namespace WebCore {

enum CompositeOperator;
class DrawingBuffer;
class GLES2Canvas;
class GraphicsContext3D;
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
class PlatformContextSkia : public Noncopyable {
public:
    // For printing, there shouldn't be any canvas. canvas can be NULL. If you
    // supply a NULL canvas, you can also call setCanvas later.
    PlatformContextSkia(skia::PlatformCanvas*);
    ~PlatformContextSkia();

    // Sets the canvas associated with this context. Use when supplying NULL
    // to the constructor.
    void setCanvas(skia::PlatformCanvas*);

    // If false we're rendering to a GraphicsContext for a web page, if false
    // we're not (as is the case when rendering to a canvas object).
    // If this is true the contents have not been marked up with the magic
    // color and all text drawing needs to go to a layer so that the alpha is
    // correctly updated.
    void setDrawingToImageBuffer(bool);
    bool isDrawingToImageBuffer() const;

    void save();
    void restore();

    // Begins a layer that is clipped to the image |imageBuffer| at the location
    // |rect|. This layer is implicitly restored when the next restore is
    // invoked.
    // NOTE: |imageBuffer| may be deleted before the |restore| is invoked.
    void beginLayerClippedToImage(const FloatRect&, const ImageBuffer*);
    void clipPathAntiAliased(const SkPath&);

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
    void setFillRule(SkPath::FillType);
    void setXfermodeMode(SkXfermode::Mode);
    void setFillColor(SkColor);
    void setFillShader(SkShader*);
    void setStrokeStyle(StrokeStyle);
    void setStrokeColor(SkColor);
    void setStrokeThickness(float thickness);
    void setStrokeShader(SkShader*);
    void setTextDrawingMode(int mode);
    void setUseAntialiasing(bool enable);
    void setDashPathEffect(SkDashPathEffect*);

    SkDrawLooper* getDrawLooper() const;
    StrokeStyle getStrokeStyle() const;
    float getStrokeThickness() const;
    int getTextDrawingMode() const;
    float getAlpha() const;

    void beginPath();
    void addPath(const SkPath&);
    SkPath currentPathInLocalCoordinates() const;

    void canvasClipPath(const SkPath&);

    // Returns the fill color. The returned color has it's alpha adjusted
    // by the current alpha.
    SkColor effectiveFillColor() const;

    // Returns the stroke color. The returned color has it's alpha adjusted
    // by the current alpha.
    SkColor effectiveStrokeColor() const;

    skia::PlatformCanvas* canvas() { return m_canvas; }

    InterpolationQuality interpolationQuality() const;
    void setInterpolationQuality(InterpolationQuality interpolationQuality);

    // FIXME: This should be pushed down to GraphicsContext.
    void drawRect(SkRect rect);

    // FIXME: I'm still unsure how I will serialize this call.
    void paintSkPaint(const SkRect&, const SkPaint&);

    const SkBitmap* bitmap() const;

    // Returns the canvas used for painting, NOT guaranteed to be non-NULL.
    //
    // Warning: This function is deprecated so the users are reminded that they
    // should use this layer of indirection instead of using the canvas
    // directly. This is to help with the eventual serialization.
    skia::PlatformCanvas* canvas() const;

    // Returns if the context is a printing context instead of a display
    // context. Bitmap shouldn't be resampled when printing to keep the best
    // possible quality.
    bool isPrinting();

    void getImageResamplingHint(IntSize* srcSize, FloatSize* dstSize) const;
    void setImageResamplingHint(const IntSize& srcSize, const FloatSize& dstSize);
    void clearImageResamplingHint();
    bool hasImageResamplingHint() const;

    bool canAccelerate() const;
    bool useGPU() { return m_useGPU; }
    void setSharedGraphicsContext3D(SharedGraphicsContext3D*, DrawingBuffer*, const IntSize&);
    GLES2Canvas* gpuCanvas() const { return m_gpuCanvas.get(); }

    // Call these before making a call that manipulates the underlying
    // skia::PlatformCanvas or WebCore::GLES2Canvas
    void prepareForSoftwareDraw() const;
    void prepareForHardwareDraw() const;
    // Call to force the skia::PlatformCanvas to contain all rendering results.
    void syncSoftwareCanvas() const;
    void markDirtyRect(const IntRect& rect);

private:
    // Used when restoring and the state has an image clip. Only shows the pixels in
    // m_canvas that are also in imageBuffer.
    void applyClipFromImage(const FloatRect&, const SkBitmap&);
    void applyAntiAliasedClipPaths(WTF::Vector<SkPath>& paths);

    void uploadSoftwareToHardware(CompositeOperator) const;
    void readbackHardwareToSoftware() const;

    // Defines drawing style.
    struct State;

    // NULL indicates painting is disabled. Never delete this object.
    skia::PlatformCanvas* m_canvas;

    // States stack. Enables local drawing state change with save()/restore()
    // calls.
    WTF::Vector<State> m_stateStack;
    // Pointer to the current drawing state. This is a cached value of
    // mStateStack.back().
    State* m_state;

    // Current path in global coordinates.
    SkPath m_path;

    // Stores image sizes for a hint to compute image resampling modes.
    // Values are used in ImageSkia.cpp
    IntSize m_imageResamplingHintSrcSize;
    FloatSize m_imageResamplingHintDstSize;
    bool m_drawingToImageBuffer;
    bool m_useGPU;
    OwnPtr<GLES2Canvas> m_gpuCanvas;
    mutable enum { None, Software, Mixed, Hardware } m_backingStoreState;
    mutable RefPtr<Texture> m_uploadTexture;
    mutable IntRect m_softwareDirtyRect;
};

}
#endif // PlatformContextSkia_h
