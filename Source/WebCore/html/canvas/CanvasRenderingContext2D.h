/*
 * Copyright (C) 2006, 2007, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CanvasRenderingContext2D_h
#define CanvasRenderingContext2D_h

#include "AffineTransform.h"
#include "CanvasPathMethods.h"
#include "CanvasRenderingContext.h"
#include "CanvasStyle.h"
#include "Color.h"
#include "FloatSize.h"
#include "FontCascade.h"
#include "GraphicsContext.h"
#include "GraphicsTypes.h"
#include "ImageBuffer.h"
#include "Path.h"
#include "PlatformLayer.h"
#include "TextFlags.h"
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CanvasGradient;
class CanvasPattern;
class DOMPath;
class FloatRect;
class GraphicsContext;
class HTMLCanvasElement;
class HTMLImageElement;
class HTMLVideoElement;
class ImageData;
class TextMetrics;

typedef int ExceptionCode;

class CanvasRenderingContext2D final : public CanvasRenderingContext, public CanvasPathMethods {
public:
    CanvasRenderingContext2D(HTMLCanvasElement*, bool usesCSSCompatibilityParseMode, bool usesDashboardCompatibilityMode);
    virtual ~CanvasRenderingContext2D();

    const CanvasStyle& strokeStyle() const { return state().strokeStyle; }
    void setStrokeStyle(CanvasStyle);

    const CanvasStyle& fillStyle() const { return state().fillStyle; }
    void setFillStyle(CanvasStyle);

    float lineWidth() const;
    void setLineWidth(float);

    String lineCap() const;
    void setLineCap(const String&);

    String lineJoin() const;
    void setLineJoin(const String&);

    float miterLimit() const;
    void setMiterLimit(float);

    const Vector<float>& getLineDash() const;
    void setLineDash(const Vector<float>&);
    void setWebkitLineDash(const Vector<float>&);

    float lineDashOffset() const;
    void setLineDashOffset(float);
    float webkitLineDashOffset() const;
    void setWebkitLineDashOffset(float);

    float shadowOffsetX() const;
    void setShadowOffsetX(float);

    float shadowOffsetY() const;
    void setShadowOffsetY(float);

    float shadowBlur() const;
    void setShadowBlur(float);

    String shadowColor() const;
    void setShadowColor(const String&);

    float globalAlpha() const;
    void setGlobalAlpha(float);

    String globalCompositeOperation() const;
    void setGlobalCompositeOperation(const String&);

    void save() { ++m_unrealizedSaveCount; }
    void restore();

    void scale(float sx, float sy);
    void rotate(float angleInRadians);
    void translate(float tx, float ty);
    void transform(float m11, float m12, float m21, float m22, float dx, float dy);
    void setTransform(float m11, float m12, float m21, float m22, float dx, float dy);

    void setStrokeColor(const String& color);
    void setStrokeColor(float grayLevel);
    void setStrokeColor(const String& color, float alpha);
    void setStrokeColor(float grayLevel, float alpha);
    void setStrokeColor(float r, float g, float b, float a);
    void setStrokeColor(float c, float m, float y, float k, float a);

    void setFillColor(const String& color);
    void setFillColor(float grayLevel);
    void setFillColor(const String& color, float alpha);
    void setFillColor(float grayLevel, float alpha);
    void setFillColor(float r, float g, float b, float a);
    void setFillColor(float c, float m, float y, float k, float a);

    void beginPath();

    void fill(const String& winding = ASCIILiteral("nonzero"));
    void stroke();
    void clip(const String& winding = ASCIILiteral("nonzero"));

    void fill(DOMPath*, const String& winding = ASCIILiteral("nonzero"));
    void stroke(DOMPath*);
    void clip(DOMPath*, const String& winding = ASCIILiteral("nonzero"));

    bool isPointInPath(const float x, const float y, const String& winding = ASCIILiteral("nonzero"));
    bool isPointInStroke(const float x, const float y);

    bool isPointInPath(DOMPath*, const float x, const float y, const String& winding = ASCIILiteral("nonzero"));
    bool isPointInStroke(DOMPath*, const float x, const float y);

    void clearRect(float x, float y, float width, float height);
    void fillRect(float x, float y, float width, float height);
    void strokeRect(float x, float y, float width, float height);

    void setShadow(float width, float height, float blur);
    void setShadow(float width, float height, float blur, const String& color);
    void setShadow(float width, float height, float blur, float grayLevel);
    void setShadow(float width, float height, float blur, const String& color, float alpha);
    void setShadow(float width, float height, float blur, float grayLevel, float alpha);
    void setShadow(float width, float height, float blur, float r, float g, float b, float a);
    void setShadow(float width, float height, float blur, float c, float m, float y, float k, float a);

    void clearShadow();

    void drawImage(HTMLImageElement*, float x, float y, ExceptionCode&);
    void drawImage(HTMLImageElement*, float x, float y, float width, float height, ExceptionCode&);
    void drawImage(HTMLImageElement*, float sx, float sy, float sw, float sh, float dx, float dy, float dw, float dh, ExceptionCode&);
    void drawImage(HTMLImageElement*, const FloatRect& srcRect, const FloatRect& dstRect, ExceptionCode&);
    void drawImage(HTMLCanvasElement*, float x, float y, ExceptionCode&);
    void drawImage(HTMLCanvasElement*, float x, float y, float width, float height, ExceptionCode&);
    void drawImage(HTMLCanvasElement*, float sx, float sy, float sw, float sh, float dx, float dy, float dw, float dh, ExceptionCode&);
    void drawImage(HTMLCanvasElement*, const FloatRect& srcRect, const FloatRect& dstRect, ExceptionCode&);
    void drawImage(HTMLImageElement*, const FloatRect& srcRect, const FloatRect& dstRect, const CompositeOperator&, const BlendMode&, ExceptionCode&);
#if ENABLE(VIDEO)
    void drawImage(HTMLVideoElement*, float x, float y, ExceptionCode&);
    void drawImage(HTMLVideoElement*, float x, float y, float width, float height, ExceptionCode&);
    void drawImage(HTMLVideoElement*, float sx, float sy, float sw, float sh, float dx, float dy, float dw, float dh, ExceptionCode&);
    void drawImage(HTMLVideoElement*, const FloatRect& srcRect, const FloatRect& dstRect, ExceptionCode&);
#endif

    void drawImageFromRect(HTMLImageElement*, float sx = 0, float sy = 0, float sw = 0, float sh = 0,
                           float dx = 0, float dy = 0, float dw = 0, float dh = 0, const String& compositeOperation = emptyString());

    void setAlpha(float);

    void setCompositeOperation(const String&);

    RefPtr<CanvasGradient> createLinearGradient(float x0, float y0, float x1, float y1, ExceptionCode&);
    RefPtr<CanvasGradient> createRadialGradient(float x0, float y0, float r0, float x1, float y1, float r1, ExceptionCode&);
    RefPtr<CanvasPattern> createPattern(HTMLImageElement*, const String& repetitionType, ExceptionCode&);
    RefPtr<CanvasPattern> createPattern(HTMLCanvasElement*, const String& repetitionType, ExceptionCode&);

    RefPtr<ImageData> createImageData(RefPtr<ImageData>&&, ExceptionCode&) const;
    RefPtr<ImageData> createImageData(float width, float height, ExceptionCode&) const;
    RefPtr<ImageData> getImageData(float sx, float sy, float sw, float sh, ExceptionCode&) const;
    RefPtr<ImageData> webkitGetImageDataHD(float sx, float sy, float sw, float sh, ExceptionCode&) const;
    void putImageData(ImageData*, float dx, float dy, ExceptionCode&);
    void putImageData(ImageData*, float dx, float dy, float dirtyX, float dirtyY, float dirtyWidth, float dirtyHeight, ExceptionCode&);
    void webkitPutImageDataHD(ImageData*, float dx, float dy, ExceptionCode&);
    void webkitPutImageDataHD(ImageData*, float dx, float dy, float dirtyX, float dirtyY, float dirtyWidth, float dirtyHeight, ExceptionCode&);

    void drawFocusIfNeeded(Element*);
    void drawFocusIfNeeded(DOMPath*, Element*);

    float webkitBackingStorePixelRatio() const { return 1; }

    void reset();

    String font() const;
    void setFont(const String&);

    String textAlign() const;
    void setTextAlign(const String&);

    String textBaseline() const;
    void setTextBaseline(const String&);

    String direction() const;
    void setDirection(const String&);

    void fillText(const String& text, float x, float y);
    void fillText(const String& text, float x, float y, float maxWidth);
    void strokeText(const String& text, float x, float y);
    void strokeText(const String& text, float x, float y, float maxWidth);
    Ref<TextMetrics> measureText(const String& text);

    LineCap getLineCap() const { return state().lineCap; }
    LineJoin getLineJoin() const { return state().lineJoin; }

    bool imageSmoothingEnabled() const;
    void setImageSmoothingEnabled(bool);

    String imageSmoothingQuality() const;
    void setImageSmoothingQuality(const String&);

    enum class SmoothingQuality {
        Low,
        Medium,
        High
    };

    bool usesDisplayListDrawing() const { return m_usesDisplayListDrawing; };
    void setUsesDisplayListDrawing(bool flag) { m_usesDisplayListDrawing = flag; };

    bool tracksDisplayListReplay() const { return m_tracksDisplayListReplay; }
    void setTracksDisplayListReplay(bool);

    String displayListAsText(DisplayList::AsTextFlags) const;
    String replayDisplayListAsText(DisplayList::AsTextFlags) const;

private:
    enum class Direction {
        Inherit,
        RTL,
        LTR
    };

    class FontProxy : public FontSelectorClient {
    public:
        FontProxy() = default;
        virtual ~FontProxy();
        FontProxy(const FontProxy&);
        FontProxy& operator=(const FontProxy&);

        bool realized() const { return m_font.fontSelector(); }
        void initialize(FontSelector&, RenderStyle&);
        FontMetrics fontMetrics() const;
        const FontCascadeDescription& fontDescription() const;
        float width(const TextRun&) const;
        void drawBidiText(GraphicsContext&, const TextRun&, const FloatPoint&, FontCascade::CustomFontNotReadyAction) const;

    private:
        void update(FontSelector&);
        virtual void fontsNeedUpdate(FontSelector&) override;

        FontCascade m_font;
    };

    struct State final {
        State();

        State(const State&);
        State& operator=(const State&);

        String unparsedStrokeColor;
        String unparsedFillColor;
        CanvasStyle strokeStyle;
        CanvasStyle fillStyle;
        float lineWidth;
        LineCap lineCap;
        LineJoin lineJoin;
        float miterLimit;
        FloatSize shadowOffset;
        float shadowBlur;
        RGBA32 shadowColor;
        float globalAlpha;
        CompositeOperator globalComposite;
        BlendMode globalBlend;
        AffineTransform transform;
        bool hasInvertibleTransform;
        Vector<float> lineDash;
        float lineDashOffset;
        bool imageSmoothingEnabled;
        SmoothingQuality imageSmoothingQuality;

        // Text state.
        TextAlign textAlign;
        TextBaseline textBaseline;
        Direction direction;

        String unparsedFont;
        FontProxy font;
    };

    enum CanvasDidDrawOption {
        CanvasDidDrawApplyNone = 0,
        CanvasDidDrawApplyTransform = 1,
        CanvasDidDrawApplyShadow = 1 << 1,
        CanvasDidDrawApplyClip = 1 << 2,
        CanvasDidDrawApplyAll = 0xffffffff
    };

    State& modifiableState() { ASSERT(!m_unrealizedSaveCount); return m_stateStack.last(); }
    const State& state() const { return m_stateStack.last(); }

    void applyLineDash() const;
    void setShadow(const FloatSize& offset, float blur, RGBA32 color);
    void applyShadow();
    bool shouldDrawShadows() const;

    void didDraw(const FloatRect&, unsigned options = CanvasDidDrawApplyAll);
    void didDrawEntireCanvas();

    void paintRenderingResultsToCanvas() override;

    GraphicsContext* drawingContext() const;

    void unwindStateStack();
    void realizeSaves()
    {
        if (m_unrealizedSaveCount)
            realizeSavesLoop();
    }
    void realizeSavesLoop();

    void applyStrokePattern();
    void applyFillPattern();

    void drawTextInternal(const String& text, float x, float y, bool fill, float maxWidth = 0, bool useMaxWidth = false);

    // The relationship between FontCascade and CanvasRenderingContext2D::FontProxy must hold certain invariants.
    // Therefore, all font operations must pass through the State.
    const FontProxy& fontProxy();

#if ENABLE(DASHBOARD_SUPPORT)
    void clearPathForDashboardBackwardCompatibilityMode();
#endif

    void beginCompositeLayer();
    void endCompositeLayer();

    void fillInternal(const Path&, const String& winding);
    void strokeInternal(const Path&);
    void clipInternal(const Path&, const String& winding);

    bool isPointInPathInternal(const Path&, float x, float y, const String& winding);
    bool isPointInStrokeInternal(const Path&, float x, float y);

    void drawFocusIfNeededInternal(const Path&, Element*);

    void clearCanvas();
    Path transformAreaToDevice(const Path&) const;
    Path transformAreaToDevice(const FloatRect&) const;
    bool rectContainsCanvas(const FloatRect&) const;

    template<class T> IntRect calculateCompositingBufferRect(const T&, IntSize*);
    std::unique_ptr<ImageBuffer> createCompositingBuffer(const IntRect&);
    void compositeBuffer(ImageBuffer&, const IntRect&, CompositeOperator);

    void inflateStrokeRect(FloatRect&) const;

    template<class T> void fullCanvasCompositedDrawImage(T&, const FloatRect&, const FloatRect&, CompositeOperator);

    void prepareGradientForDashboard(CanvasGradient& gradient) const;

    RefPtr<ImageData> getImageData(ImageBuffer::CoordinateSystem, float sx, float sy, float sw, float sh, ExceptionCode&) const;
    void putImageData(ImageData*, ImageBuffer::CoordinateSystem, float dx, float dy, float dirtyX, float dirtyY, float dirtyWidth, float dirtyHeight, ExceptionCode&);

    virtual bool is2d() const override { return true; }
    virtual bool isAccelerated() const override;

    virtual bool hasInvertibleTransform() const override { return state().hasInvertibleTransform; }
    TextDirection toTextDirection(Direction, RenderStyle** computedStyle = nullptr) const;

#if ENABLE(ACCELERATED_2D_CANVAS)
    virtual PlatformLayer* platformLayer() const override;
#endif

    Vector<State, 1> m_stateStack;
    unsigned m_unrealizedSaveCount { 0 };
    bool m_usesCSSCompatibilityParseMode;
#if ENABLE(DASHBOARD_SUPPORT)
    bool m_usesDashboardCompatibilityMode;
#endif

    bool m_usesDisplayListDrawing { false };
    bool m_tracksDisplayListReplay { false };
    mutable std::unique_ptr<struct DisplayListDrawingContext> m_recordingContext;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CANVASRENDERINGCONTEXT(WebCore::CanvasRenderingContext2D, is2d())

#endif
