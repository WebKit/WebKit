/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "GraphicsContext.h"
#include "NicosiaPaintingOperation.h"

namespace Nicosia {

class CairoOperationRecorder final : public WebCore::GraphicsContext {
public:
    CairoOperationRecorder(PaintingOperations&);

private:
    bool hasPlatformContext() const override { return false; }
    PlatformGraphicsContext* platformContext() const override { return nullptr; }

    void updateState(const WebCore::GraphicsContextState&, WebCore::GraphicsContextState::StateChangeFlags) override;

    void setLineCap(WebCore::LineCap) override;
    void setLineDash(const WebCore::DashArray&, float) override;
    void setLineJoin(WebCore::LineJoin) override;
    void setMiterLimit(float) override;

    void fillRect(const WebCore::FloatRect&) override;
    void fillRect(const WebCore::FloatRect&, const WebCore::Color&) override;
    void fillRect(const WebCore::FloatRect&, WebCore::Gradient&) override;
    void fillRect(const WebCore::FloatRect&, const WebCore::Color&, WebCore::CompositeOperator, WebCore::BlendMode) override;
    void fillRoundedRectImpl(const WebCore::FloatRoundedRect&, const WebCore::Color&) override { ASSERT_NOT_REACHED(); }
    void fillRoundedRect(const WebCore::FloatRoundedRect&, const WebCore::Color&, WebCore::BlendMode) override;
    void fillRectWithRoundedHole(const WebCore::FloatRect&, const WebCore::FloatRoundedRect&, const WebCore::Color&) override;
    void fillPath(const WebCore::Path&) override;
    void fillEllipse(const WebCore::FloatRect&) override;
    void strokeRect(const WebCore::FloatRect&, float) override;
    void strokePath(const WebCore::Path&) override;
    void strokeEllipse(const WebCore::FloatRect&) override;
    void clearRect(const WebCore::FloatRect&) override;

    void drawGlyphs(const WebCore::Font&, const WebCore::GlyphBufferGlyph*, const WebCore::GlyphBufferAdvance*, unsigned numGlyphs, const WebCore::FloatPoint&, WebCore::FontSmoothingMode) override;

    void drawImageBuffer(WebCore::ImageBuffer&, const WebCore::FloatRect& destination, const WebCore::FloatRect& source, const WebCore::ImagePaintingOptions&) override;
    void drawNativeImage(WebCore::NativeImage&, const WebCore::FloatSize&, const WebCore::FloatRect&, const WebCore::FloatRect&, const WebCore::ImagePaintingOptions&) override;
    void drawPattern(WebCore::NativeImage&, const WebCore::FloatSize&, const WebCore::FloatRect&, const WebCore::FloatRect&, const WebCore::AffineTransform&, const WebCore::FloatPoint&, const WebCore::FloatSize&, const WebCore::ImagePaintingOptions&) override;

    void drawRect(const WebCore::FloatRect&, float) override;
    void drawLine(const WebCore::FloatPoint&, const WebCore::FloatPoint&) override;
    void drawLinesForText(const WebCore::FloatPoint&, float thickness, const WebCore::DashArray&, bool, bool, WebCore::StrokeStyle) override;
    void drawDotsForDocumentMarker(const WebCore::FloatRect&, WebCore::DocumentMarkerLineStyle) override;
    void drawEllipse(const WebCore::FloatRect&) override;

    void drawFocusRing(const WebCore::Path&, float, float, const WebCore::Color&) override;
    void drawFocusRing(const Vector<WebCore::FloatRect>&, float, float, const WebCore::Color&) override;

    void save() override;
    void restore() override;

    void translate(float, float) override;
    void rotate(float angleInRadians) override;
    void scale(const WebCore::FloatSize&) override;
    void concatCTM(const WebCore::AffineTransform&) override;
    void setCTM(const WebCore::AffineTransform&) override;
    WebCore::AffineTransform getCTM(WebCore::GraphicsContext::IncludeDeviceScale) const override;

    void beginTransparencyLayer(float) override;
    void endTransparencyLayer() override;

    void clip(const WebCore::FloatRect&) override;
    void clipOut(const WebCore::FloatRect&) override;
    void clipOut(const WebCore::Path&) override;
    void clipPath(const WebCore::Path&, WebCore::WindRule) override;
    WebCore::IntRect clipBounds() const override;
    void clipToImageBuffer(WebCore::ImageBuffer&, const WebCore::FloatRect&) override;
    WebCore::GraphicsContext::ClipToDrawingCommandsResult clipToDrawingCommands(const WebCore::FloatRect& destination, const WebCore::DestinationColorSpace&, Function<void(WebCore::GraphicsContext&)>&&) override;
#if ENABLE(VIDEO)
    void paintFrameForMedia(WebCore::MediaPlayer&, const WebCore::FloatRect& destination) override;
#endif

    void applyDeviceScaleFactor(float) override;

    WebCore::FloatRect roundToDevicePixels(const WebCore::FloatRect&, WebCore::GraphicsContext::RoundingMode) override;

    void append(std::unique_ptr<PaintingOperation>&&);
    PaintingOperations& m_commandList;

    struct State {
        WebCore::AffineTransform ctm;
        WebCore::AffineTransform ctmInverse;
        WebCore::FloatRect clipBounds;
    };
    Vector<State, 32> m_stateStack;
};

} // namespace Nicosia
