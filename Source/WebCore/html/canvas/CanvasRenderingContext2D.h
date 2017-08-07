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

#pragma once

#include "AffineTransform.h"
#include "CanvasPath.h"
#include "CanvasRenderingContext.h"
#include "CanvasStyle.h"
#include "Color.h"
#include "FloatSize.h"
#include "FontCascade.h"
#include "FontSelectorClient.h"
#include "GraphicsContext.h"
#include "GraphicsTypes.h"
#include "ImageBuffer.h"
#include "Path.h"
#include "PlatformLayer.h"
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CanvasGradient;
class CanvasPattern;
class DOMMatrix;
class DOMPath;
class FloatRect;
class GraphicsContext;
class HTMLCanvasElement;
class HTMLImageElement;
class HTMLVideoElement;
class ImageData;
class TextMetrics;

struct DOMMatrixInit;

#if ENABLE(VIDEO)
using CanvasImageSource = Variant<RefPtr<HTMLImageElement>, RefPtr<HTMLVideoElement>, RefPtr<HTMLCanvasElement>>;
#else
using CanvasImageSource = Variant<RefPtr<HTMLImageElement>, RefPtr<HTMLCanvasElement>>;
#endif

class CanvasRenderingContext2D final : public CanvasRenderingContext, public CanvasPath {
public:
    CanvasRenderingContext2D(HTMLCanvasElement&, bool usesCSSCompatibilityParseMode, bool usesDashboardCompatibilityMode);
    virtual ~CanvasRenderingContext2D();

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
    const Vector<float>& webkitLineDash() const { return getLineDash(); }
    void setWebkitLineDash(const Vector<float>&);

    float lineDashOffset() const;
    void setLineDashOffset(float);

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

    // This is a no-op in a direct-2d canvas.
    void commit() { }

    void scale(float sx, float sy);
    void rotate(float angleInRadians);
    void translate(float tx, float ty);
    void transform(float m11, float m12, float m21, float m22, float dx, float dy);

    Ref<DOMMatrix> getTransform() const;
    void setTransform(float m11, float m12, float m21, float m22, float dx, float dy);
    ExceptionOr<void> setTransform(DOMMatrixInit&&);
    void resetTransform();

    void setStrokeColor(const String& color, std::optional<float> alpha = std::nullopt);
    void setStrokeColor(float grayLevel, float alpha = 1.0);
    void setStrokeColor(float r, float g, float b, float a);
    void setStrokeColor(float c, float m, float y, float k, float a);

    void setFillColor(const String& color, std::optional<float> alpha = std::nullopt);
    void setFillColor(float grayLevel, float alpha = 1.0f);
    void setFillColor(float r, float g, float b, float a);
    void setFillColor(float c, float m, float y, float k, float a);

    void beginPath();

    enum class WindingRule { Nonzero, Evenodd };
    static String stringForWindingRule(WindingRule);

    void fill(WindingRule = WindingRule::Nonzero);
    void stroke();
    void clip(WindingRule = WindingRule::Nonzero);

    void fill(DOMPath&, WindingRule = WindingRule::Nonzero);
    void stroke(DOMPath&);
    void clip(DOMPath&, WindingRule = WindingRule::Nonzero);

    bool isPointInPath(float x, float y, WindingRule = WindingRule::Nonzero);
    bool isPointInStroke(float x, float y);

    bool isPointInPath(DOMPath&, float x, float y, WindingRule = WindingRule::Nonzero);
    bool isPointInStroke(DOMPath&, float x, float y);

    void clearRect(float x, float y, float width, float height);
    void fillRect(float x, float y, float width, float height);
    void strokeRect(float x, float y, float width, float height);

    void setShadow(float width, float height, float blur, const String& color = String(), std::optional<float> alpha = std::nullopt);
    void setShadow(float width, float height, float blur, float grayLevel, float alpha = 1.0);
    void setShadow(float width, float height, float blur, float r, float g, float b, float a);
    void setShadow(float width, float height, float blur, float c, float m, float y, float k, float a);

    void clearShadow();

    ExceptionOr<void> drawImage(CanvasImageSource&&, float dx, float dy);
    ExceptionOr<void> drawImage(CanvasImageSource&&, float dx, float dy, float dw, float dh);
    ExceptionOr<void> drawImage(CanvasImageSource&&, float sx, float sy, float sw, float sh, float dx, float dy, float dw, float dh);

    void drawImageFromRect(HTMLImageElement&, float sx = 0, float sy = 0, float sw = 0, float sh = 0, float dx = 0, float dy = 0, float dw = 0, float dh = 0, const String& compositeOperation = emptyString());

    void setAlpha(float);

    void setCompositeOperation(const String&);

    using Style = Variant<String, RefPtr<CanvasGradient>, RefPtr<CanvasPattern>>;
    Style strokeStyle() const;
    void setStrokeStyle(Style&&);
    Style fillStyle() const;
    void setFillStyle(Style&&);

    ExceptionOr<Ref<CanvasGradient>> createLinearGradient(float x0, float y0, float x1, float y1);
    ExceptionOr<Ref<CanvasGradient>> createRadialGradient(float x0, float y0, float r0, float x1, float y1, float r1);
    ExceptionOr<RefPtr<CanvasPattern>> createPattern(CanvasImageSource&&, const String& repetition);

    ExceptionOr<RefPtr<ImageData>> createImageData(ImageData*) const;
    ExceptionOr<RefPtr<ImageData>> createImageData(float width, float height) const;
    ExceptionOr<RefPtr<ImageData>> getImageData(float sx, float sy, float sw, float sh) const;
    ExceptionOr<RefPtr<ImageData>> webkitGetImageDataHD(float sx, float sy, float sw, float sh) const;
    void putImageData(ImageData&, float dx, float dy);
    void putImageData(ImageData&, float dx, float dy, float dirtyX, float dirtyY, float dirtyWidth, float dirtyHeight);
    void webkitPutImageDataHD(ImageData&, float dx, float dy);
    void webkitPutImageDataHD(ImageData&, float dx, float dy, float dirtyX, float dirtyY, float dirtyWidth, float dirtyHeight);

    void drawFocusIfNeeded(Element&);
    void drawFocusIfNeeded(DOMPath&, Element&);

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

    void fillText(const String& text, float x, float y, std::optional<float> maxWidth = std::nullopt);
    void strokeText(const String& text, float x, float y, std::optional<float> maxWidth = std::nullopt);
    Ref<TextMetrics> measureText(const String& text);

    LineCap getLineCap() const { return state().lineCap; }
    LineJoin getLineJoin() const { return state().lineJoin; }

    bool imageSmoothingEnabled() const;
    void setImageSmoothingEnabled(bool);

    enum class ImageSmoothingQuality { Low, Medium, High };
    static String stringForImageSmoothingQuality(ImageSmoothingQuality);

    ImageSmoothingQuality imageSmoothingQuality() const;
    void setImageSmoothingQuality(ImageSmoothingQuality);

    void setPath(DOMPath&);
    Ref<DOMPath> getPath() const;

    bool usesDisplayListDrawing() const { return m_usesDisplayListDrawing; };
    void setUsesDisplayListDrawing(bool flag) { m_usesDisplayListDrawing = flag; };

    bool tracksDisplayListReplay() const { return m_tracksDisplayListReplay; }
    void setTracksDisplayListReplay(bool);

    String displayListAsText(DisplayList::AsTextFlags) const;
    String replayDisplayListAsText(DisplayList::AsTextFlags) const;

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
        void initialize(FontSelector&, const RenderStyle&);
        const FontMetrics& fontMetrics() const;
        const FontCascadeDescription& fontDescription() const;
        float width(const TextRun&, GlyphOverflow* = 0) const;
        void drawBidiText(GraphicsContext&, const TextRun&, const FloatPoint&, FontCascade::CustomFontNotReadyAction) const;

    private:
        void update(FontSelector&);
        void fontsNeedUpdate(FontSelector&) override;

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
        Color shadowColor;
        float globalAlpha;
        CompositeOperator globalComposite;
        BlendMode globalBlend;
        AffineTransform transform;
        bool hasInvertibleTransform;
        Vector<float> lineDash;
        float lineDashOffset;
        bool imageSmoothingEnabled;
        ImageSmoothingQuality imageSmoothingQuality;

        // Text state.
        TextAlign textAlign;
        TextBaseline textBaseline;
        Direction direction;

        String unparsedFont;
        FontProxy font;
    };

    const State& state() const { return m_stateStack.last(); }

private:
    enum CanvasDidDrawOption {
        CanvasDidDrawApplyNone = 0,
        CanvasDidDrawApplyTransform = 1,
        CanvasDidDrawApplyShadow = 1 << 1,
        CanvasDidDrawApplyClip = 1 << 2,
        CanvasDidDrawApplyAll = 0xffffffff
    };

    State& modifiableState() { ASSERT(!m_unrealizedSaveCount || m_stateStack.size() >= MaxSaveCount); return m_stateStack.last(); }

    void applyLineDash() const;
    void setShadow(const FloatSize& offset, float blur, const Color&);
    void applyShadow();
    bool shouldDrawShadows() const;

    void didDraw(const FloatRect&, unsigned options = CanvasDidDrawApplyAll);
    void didDrawEntireCanvas();

    void paintRenderingResultsToCanvas() override;

    GraphicsContext* drawingContext() const;

    void unwindStateStack();
    void realizeSaves();
    void realizeSavesLoop();

    void applyStrokePattern();
    void applyFillPattern();

    void setStrokeStyle(CanvasStyle);
    void setFillStyle(CanvasStyle);

    ExceptionOr<RefPtr<CanvasPattern>> createPattern(HTMLImageElement&, bool repeatX, bool repeatY);
    ExceptionOr<RefPtr<CanvasPattern>> createPattern(HTMLCanvasElement&, bool repeatX, bool repeatY);
#if ENABLE(VIDEO)
    ExceptionOr<RefPtr<CanvasPattern>> createPattern(HTMLVideoElement&, bool repeatX, bool repeatY);
#endif

    ExceptionOr<void> drawImage(HTMLImageElement&, const FloatRect& srcRect, const FloatRect& dstRect);
    ExceptionOr<void> drawImage(HTMLImageElement&, const FloatRect& srcRect, const FloatRect& dstRect, const CompositeOperator&, const BlendMode&);
    ExceptionOr<void> drawImage(HTMLCanvasElement&, const FloatRect& srcRect, const FloatRect& dstRect);
#if ENABLE(VIDEO)
    ExceptionOr<void> drawImage(HTMLVideoElement&, const FloatRect& srcRect, const FloatRect& dstRect);
#endif

    void drawTextInternal(const String& text, float x, float y, bool fill, std::optional<float> maxWidth = std::nullopt);

    // The relationship between FontCascade and CanvasRenderingContext2D::FontProxy must hold certain invariants.
    // Therefore, all font operations must pass through the State.
    const FontProxy& fontProxy();

    void clearPathForDashboardBackwardCompatibilityMode();

    void beginCompositeLayer();
    void endCompositeLayer();

    void fillInternal(const Path&, WindingRule);
    void strokeInternal(const Path&);
    void clipInternal(const Path&, WindingRule);

    bool isPointInPathInternal(const Path&, float x, float y, WindingRule);
    bool isPointInStrokeInternal(const Path&, float x, float y);

    void drawFocusIfNeededInternal(const Path&, Element&);

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

    ExceptionOr<RefPtr<ImageData>> getImageData(ImageBuffer::CoordinateSystem, float sx, float sy, float sw, float sh) const;
    void putImageData(ImageData&, ImageBuffer::CoordinateSystem, float dx, float dy, float dirtyX, float dirtyY, float dirtyWidth, float dirtyHeight);

    bool is2d() const override { return true; }
    bool isAccelerated() const override;

    bool hasInvertibleTransform() const override { return state().hasInvertibleTransform; }
    TextDirection toTextDirection(Direction, const RenderStyle** computedStyle = nullptr) const;

    FloatPoint textOffset(float width, TextDirection);

#if ENABLE(ACCELERATED_2D_CANVAS)
    PlatformLayer* platformLayer() const override;
#endif

    static const unsigned MaxSaveCount = 1024 * 16;
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
