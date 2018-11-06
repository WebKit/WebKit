/*
 * Copyright (C) 2017 Metrological Group B.V.
 * Copyright (C) 2017 Igalia S.L.
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

#if USE(CAIRO)

#include "GraphicsContextImpl.h"

typedef struct _cairo cairo_t;

namespace WebCore {

class PlatformContextCairo;

class GraphicsContextImplCairo final : public GraphicsContextImpl {
public:
    WEBCORE_EXPORT static GraphicsContext::GraphicsContextImplFactory createFactory(PlatformContextCairo&);
    WEBCORE_EXPORT static GraphicsContext::GraphicsContextImplFactory createFactory(cairo_t*);

    GraphicsContextImplCairo(GraphicsContext&, PlatformContextCairo&);
    GraphicsContextImplCairo(GraphicsContext&, cairo_t*);
    virtual ~GraphicsContextImplCairo();

    bool hasPlatformContext() const override;
    PlatformContextCairo* platformContext() const override;

    void updateState(const GraphicsContextState&, GraphicsContextState::StateChangeFlags) override;
    void clearShadow() override;

    void setLineCap(LineCap) override;
    void setLineDash(const DashArray&, float) override;
    void setLineJoin(LineJoin) override;
    void setMiterLimit(float) override;

    void fillRect(const FloatRect&) override;
    void fillRect(const FloatRect&, const Color&) override;
    void fillRect(const FloatRect&, Gradient&) override;
    void fillRect(const FloatRect&, const Color&, CompositeOperator, BlendMode) override;
    void fillRoundedRect(const FloatRoundedRect&, const Color&, BlendMode) override;
    void fillRectWithRoundedHole(const FloatRect&, const FloatRoundedRect&, const Color&) override;
    void fillPath(const Path&) override;
    void fillEllipse(const FloatRect&) override;
    void strokeRect(const FloatRect&, float) override;
    void strokePath(const Path&) override;
    void strokeEllipse(const FloatRect&) override;
    void clearRect(const FloatRect&) override;

    void drawGlyphs(const Font&, const GlyphBuffer&, unsigned, unsigned, const FloatPoint&, FontSmoothingMode) override;

    ImageDrawResult drawImage(Image&, const FloatRect&, const FloatRect&, const ImagePaintingOptions&) override;
    ImageDrawResult drawTiledImage(Image&, const FloatRect&, const FloatPoint&, const FloatSize&, const FloatSize&, const ImagePaintingOptions&) override;
    ImageDrawResult drawTiledImage(Image&, const FloatRect&, const FloatRect&, const FloatSize&, Image::TileRule, Image::TileRule, const ImagePaintingOptions&) override;
    void drawNativeImage(const NativeImagePtr&, const FloatSize&, const FloatRect&, const FloatRect&, CompositeOperator, BlendMode, ImageOrientation) override;
    void drawPattern(Image&, const FloatRect&, const FloatRect&, const AffineTransform&, const FloatPoint&, const FloatSize&, CompositeOperator, BlendMode = BlendMode::Normal) override;

    void drawRect(const FloatRect&, float) override;
    void drawLine(const FloatPoint&, const FloatPoint&) override;
    void drawLinesForText(const FloatPoint&, float thickness, const DashArray&, bool, bool) override;
    void drawDotsForDocumentMarker(const FloatRect&, DocumentMarkerLineStyle) override;
    void drawEllipse(const FloatRect&) override;
    void drawPath(const Path&) override;

    void drawFocusRing(const Path&, float, float, const Color&) override;
    void drawFocusRing(const Vector<FloatRect>&, float, float, const Color&) override;

    void save() override;
    void restore() override;

    void translate(float, float) override;
    void rotate(float) override;
    void scale(const FloatSize&) override;
    void concatCTM(const AffineTransform&) override;
    void setCTM(const AffineTransform&) override;
    AffineTransform getCTM(GraphicsContext::IncludeDeviceScale) override;

    void beginTransparencyLayer(float) override;
    void endTransparencyLayer() override;

    void clip(const FloatRect&) override;
    void clipOut(const FloatRect&) override;
    void clipOut(const Path&) override;
    void clipPath(const Path&, WindRule) override;
    IntRect clipBounds() override;
    void clipToImageBuffer(ImageBuffer&, const FloatRect&) override;
    
    void applyDeviceScaleFactor(float) override;

    FloatRect roundToDevicePixels(const FloatRect&, GraphicsContext::RoundingMode) override;

private:
    std::unique_ptr<PlatformContextCairo> m_ownedPlatformContext;
    PlatformContextCairo& m_platformContext;

    std::unique_ptr<GraphicsContextPlatformPrivate> m_private;
};

} // namespace WebCore

#endif // USE(CAIRO)
