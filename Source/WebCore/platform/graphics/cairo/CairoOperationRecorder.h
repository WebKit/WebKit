/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018 Igalia S.L.
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if USE(CAIRO)
#include "CairoPaintingOperation.h"
#include "GraphicsContext.h"

namespace WebCore {
namespace Cairo {

class OperationRecorder final : public WebCore::GraphicsContext {
public:
    OperationRecorder(PaintingOperations&);

private:
    bool hasPlatformContext() const override { return false; }
    PlatformGraphicsContext* platformContext() const override { return nullptr; }

    void didUpdateState(WebCore::GraphicsContextState&) override;

    void setLineCap(WebCore::LineCap) override;
    void setLineDash(const WebCore::DashArray&, float) override;
    void setLineJoin(WebCore::LineJoin) override;
    void setMiterLimit(float) override;

    void fillRect(const WebCore::FloatRect&, WebCore::GraphicsContext::RequiresClipToRect = WebCore::GraphicsContext::RequiresClipToRect::Yes) override;
    void fillRect(const WebCore::FloatRect&, const WebCore::Color&) override;
    void fillRect(const WebCore::FloatRect&, WebCore::Gradient&) override;
    void fillRect(const WebCore::FloatRect&, WebCore::Gradient&, const WebCore::AffineTransform&, WebCore::GraphicsContext::RequiresClipToRect = WebCore::GraphicsContext::RequiresClipToRect::Yes) override;
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

    void drawGlyphs(const WebCore::Font&, std::span<const WebCore::GlyphBufferGlyph>, std::span<const WebCore::GlyphBufferAdvance>, const WebCore::FloatPoint&, WebCore::FontSmoothingMode) override;

    void drawImageBuffer(WebCore::ImageBuffer&, const WebCore::FloatRect& destination, const WebCore::FloatRect& source, WebCore::ImagePaintingOptions) override;
    void drawFilteredImageBuffer(WebCore::ImageBuffer*, const WebCore::FloatRect&, WebCore::Filter&, WebCore::FilterResults&) override;
    void drawNativeImage(WebCore::NativeImage&, const WebCore::FloatRect&, const WebCore::FloatRect&, WebCore::ImagePaintingOptions) override;
    void drawPattern(WebCore::NativeImage&, const WebCore::FloatRect&, const WebCore::FloatRect&, const WebCore::AffineTransform&, const WebCore::FloatPoint&, const WebCore::FloatSize&, WebCore::ImagePaintingOptions) override;

    void drawRect(const WebCore::FloatRect&, float) override;
    void drawLine(const WebCore::FloatPoint&, const WebCore::FloatPoint&) override;
    void drawLinesForText(const WebCore::FloatPoint&, float thickness, std::span<const WebCore::FloatSegment>, bool, bool, WebCore::StrokeStyle) override;
    void drawDotsForDocumentMarker(const WebCore::FloatRect&, WebCore::DocumentMarkerLineStyle) override;
    void drawEllipse(const WebCore::FloatRect&) override;

    void drawFocusRing(const WebCore::Path&, float outlineWidth, const WebCore::Color&) override;
    void drawFocusRing(const Vector<WebCore::FloatRect>&, float outlineOffset, float outlineWidth, const WebCore::Color&) override;

    void save(WebCore::GraphicsContextState::Purpose = WebCore::GraphicsContextState::Purpose::SaveRestore) override;
    void restore(WebCore::GraphicsContextState::Purpose = WebCore::GraphicsContextState::Purpose::SaveRestore) override;

    void translate(float, float) override;
    void rotate(float angleInRadians) override;
    void scale(const WebCore::FloatSize&) override;
    void concatCTM(const WebCore::AffineTransform&) override;
    void setCTM(const WebCore::AffineTransform&) override;
    WebCore::AffineTransform getCTM(WebCore::GraphicsContext::IncludeDeviceScale) const override;

    void beginTransparencyLayer(float) override;
    void beginTransparencyLayer(WebCore::CompositeOperator, WebCore::BlendMode) override;
    void endTransparencyLayer() override;

    void resetClip() override;
    void clip(const WebCore::FloatRect&) override;
    void clipOut(const WebCore::FloatRect&) override;
    void clipOut(const WebCore::Path&) override;
    void clipPath(const WebCore::Path&, WebCore::WindRule) override;
    WebCore::IntRect clipBounds() const override;
    void clipToImageBuffer(WebCore::ImageBuffer&, const WebCore::FloatRect&) override;
#if ENABLE(VIDEO)
    void drawVideoFrame(const WebCore::VideoFrame&, const WebCore::FloatRect& destination, WebCore::ImageOrientation, bool shouldDiscardAlpha) override;
#endif

    void applyDeviceScaleFactor(float) override;

    void append(std::unique_ptr<PaintingOperation>&&);
    PaintingOperations& m_commandList;

    struct State {
        WebCore::AffineTransform ctm;
        WebCore::AffineTransform ctmInverse;
        WebCore::FloatRect clipBounds;
    };
    Vector<State, 32> m_stateStack;
};

} // namespace Cairo
} // namespace WebCore

#endif // USE(CAIRO)
