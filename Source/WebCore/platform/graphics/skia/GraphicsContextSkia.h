/*
 * Copyright (C) 2024, 2025 Igalia S.L.
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

#if USE(SKIA)

#include "GLFence.h"
#include "GraphicsContext.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkCanvas.h>
#include <skia/core/SkImage.h>
#include <skia/effects/SkDashPathEffect.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>

class SkSurface;

namespace WebCore {

using SkiaImageToFenceMap = HashMap<const SkImage*, std::unique_ptr<GLFence>>;

class WEBCORE_EXPORT GraphicsContextSkia final : public GraphicsContext {
public:
    GraphicsContextSkia(SkCanvas&, RenderingMode, RenderingPurpose, CompletionHandler<void()>&& = nullptr);
    virtual ~GraphicsContextSkia();

    bool hasPlatformContext() const final;
    SkCanvas* platformContext() const final;

    const DestinationColorSpace& colorSpace() const final;

    void beginRecording();
    SkiaImageToFenceMap endRecording();

    void didUpdateState(GraphicsContextState&) final;
    void didUpdateSingleState(GraphicsContextState&, GraphicsContextState::ChangeIndex) final;

    void setLineCap(LineCap) final;
    void setLineDash(const DashArray&, float) final;
    void setLineJoin(LineJoin) final;
    void setMiterLimit(float) final;

    using GraphicsContext::fillRect;
    void fillRect(const FloatRect&, RequiresClipToRect = RequiresClipToRect::Yes) final;
    void fillRect(const FloatRect&, const Color&) final;
    void fillRect(const FloatRect&, Gradient&, const AffineTransform&, RequiresClipToRect = RequiresClipToRect::Yes) final;
    void fillRoundedRectImpl(const FloatRoundedRect&, const Color&) final;
    void fillRectWithRoundedHole(const FloatRect&, const FloatRoundedRect&, const Color&) final;
    void fillPath(const Path&) final;
    void strokeRect(const FloatRect&, float) final;
    void strokePath(const Path&) final;
    void clearRect(const FloatRect&) final;

    void drawNativeImageInternal(NativeImage&, const FloatRect&, const FloatRect&, ImagePaintingOptions) final;
    void drawPattern(NativeImage&, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions) final;
    void drawRect(const FloatRect&, float) final;
    void drawLine(const FloatPoint&, const FloatPoint&) final;
    void drawLinesForText(const FloatPoint&, float thickness, std::span<const FloatSegment>, bool isPrinting, bool doubleLines, StrokeStyle) final;
    void drawDotsForDocumentMarker(const FloatRect&, DocumentMarkerLineStyle) final;
    void drawEllipse(const FloatRect&) final;

    void drawFocusRing(const Path&, float outlineWidth, const Color&) final;
    void drawFocusRing(const Vector<FloatRect>&, float outlineOffset, float outlineWidth, const Color&) final;

    void save(GraphicsContextState::Purpose = GraphicsContextState::Purpose::SaveRestore) final;
    void restore(GraphicsContextState::Purpose = GraphicsContextState::Purpose::SaveRestore) final;

    void translate(float, float) final;
    void rotate(float) final;
    using GraphicsContext::scale;
    void scale(const FloatSize&) final;
    void concatCTM(const AffineTransform&) final;
    void setCTM(const AffineTransform&) final;
    AffineTransform getCTM(GraphicsContext::IncludeDeviceScale) const final;

    void beginTransparencyLayer(float) final;
    void beginTransparencyLayer(CompositeOperator, BlendMode) final;
    void endTransparencyLayer() final;

    void resetClip() final;
    void clip(const FloatRect&) final;
    void clipOut(const FloatRect&) final;
    void clipOut(const Path&) final;
    void clipPath(const Path&, WindRule) final;
    IntRect clipBounds() const final;
    void clipToImageBuffer(ImageBuffer&, const FloatRect&) final;

    RenderingMode renderingMode() const final;

    SkPaint createFillPaint() const;
    SkPaint createStrokePaint() const;

    void drawSkiaText(const sk_sp<SkTextBlob>&, SkScalar, SkScalar, bool, bool);

    static std::unique_ptr<GLFence> createAcceleratedRenderingFenceIfNeeded(SkSurface*);
    static std::unique_ptr<GLFence> createAcceleratedRenderingFenceIfNeeded(const sk_sp<SkImage>&);

private:
    enum class ContextMode : bool {
        PaintingMode,
        RecordingMode
    };

    bool makeGLContextCurrentIfNeeded() const;
    void trackAcceleratedRenderingFenceIfNeeded(const sk_sp<SkImage>&);
    void trackAcceleratedRenderingFenceIfNeeded(SkPaint&);

    void setupFillSource(SkPaint&);
    void setupStrokeSource(SkPaint&);

    enum class ShadowStyle : uint8_t { Outset, Inset };
    sk_sp<SkImageFilter> createDropShadowFilterIfNeeded(ShadowStyle) const;
    bool drawOutsetShadow(SkPaint&, Function<void(const SkPaint&)>&&);

    void drawSkiaRect(const SkRect&, SkPaint&);
    void drawSkiaPath(const SkPath&, SkPaint&);

    class SkiaState {
    public:
        SkiaState() = default;

        struct {
            SkScalar miter { SkFloatToScalar(4) };
            SkPaint::Cap cap { SkPaint::kButt_Cap };
            SkPaint::Join join { SkPaint::kMiter_Join };
            sk_sp<SkPathEffect> dash;
        } m_stroke;
    };

    struct LayerState {
        std::optional<CompositeMode> compositeMode;
    };

    SkCanvas& m_canvas;
    ContextMode m_contextMode { ContextMode::PaintingMode };
    RenderingMode m_renderingMode { RenderingMode::Accelerated };
    RenderingPurpose m_renderingPurpose { RenderingPurpose::Unspecified };
    CompletionHandler<void()> m_destroyNotify;
    SkiaState m_skiaState;
    Vector<SkiaState, 1> m_skiaStateStack;
    Vector<LayerState, 1> m_layerStateStack;
    SkiaImageToFenceMap m_imageToFenceMap;
    const DestinationColorSpace m_colorSpace;
};

} // namespace WebCore

#endif // USE(SKIA)
