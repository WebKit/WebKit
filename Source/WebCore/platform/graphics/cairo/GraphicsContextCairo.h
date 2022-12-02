/*
 * Copyright (C) 2017 Metrological Group B.V.
 * Copyright (C) 2017 Igalia S.L.
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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

#include "GraphicsContext.h"

typedef struct _cairo cairo_t;
typedef struct _cairo_surface cairo_surface_t;

namespace WebCore {

class WEBCORE_EXPORT GraphicsContextCairo final : public GraphicsContext {
public:
    explicit GraphicsContextCairo(RefPtr<cairo_t>&&);
    explicit GraphicsContextCairo(cairo_surface_t*);

#if PLATFORM(WIN)
    GraphicsContextCairo(HDC, bool hasAlpha = false); // FIXME: To be removed.
    explicit GraphicsContextCairo(GraphicsContextCairo*);
#endif

    virtual ~GraphicsContextCairo();

    bool hasPlatformContext() const final;
    GraphicsContextCairo* platformContext() const final;

    void didUpdateState(GraphicsContextState&);

    void setLineCap(LineCap) final;
    void setLineDash(const DashArray&, float) final;
    void setLineJoin(LineJoin) final;
    void setMiterLimit(float) final;

    using GraphicsContext::fillRect;
    void fillRect(const FloatRect&) final;
    void fillRect(const FloatRect&, const Color&) final;
    void fillRoundedRectImpl(const FloatRoundedRect&, const Color&) final;
    void fillRectWithRoundedHole(const FloatRect&, const FloatRoundedRect&, const Color&) final;
    void fillPath(const Path&) final;
    void strokeRect(const FloatRect&, float) final;
    void strokePath(const Path&) final;
    void clearRect(const FloatRect&) final;

    void drawGlyphs(const Font&, const GlyphBufferGlyph*, const GlyphBufferAdvance*, unsigned numGlyphs, const FloatPoint&, FontSmoothingMode) final;
    void drawDecomposedGlyphs(const Font&, const DecomposedGlyphs&) final;

    void drawNativeImage(NativeImage&, const FloatSize&, const FloatRect&, const FloatRect&, const ImagePaintingOptions&) final;
    void drawPattern(NativeImage&, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions&) final;

    void drawRect(const FloatRect&, float) final;
    void drawLine(const FloatPoint&, const FloatPoint&) final;
    void drawLinesForText(const FloatPoint&, float thickness, const DashArray&, bool, bool, StrokeStyle) final;
    void drawDotsForDocumentMarker(const FloatRect&, DocumentMarkerLineStyle) final;
    void drawEllipse(const FloatRect&) final;

    void drawFocusRing(const Path&, float, float, const Color&) final;
    void drawFocusRing(const Vector<FloatRect>&, float, float, const Color&) final;

    void save() final;
    void restore() final;

    void translate(float, float) final;
    void rotate(float) final;
    using GraphicsContext::scale;
    void scale(const FloatSize&) final;
    void concatCTM(const AffineTransform&) final;
    void setCTM(const AffineTransform&) final;
    AffineTransform getCTM(GraphicsContext::IncludeDeviceScale) const final;

    void beginTransparencyLayer(float) final;
    void endTransparencyLayer() final;

    void clip(const FloatRect&) final;
    void clipOut(const FloatRect&) final;
    void clipOut(const Path&) final;
    void clipPath(const Path&, WindRule) final;
    IntRect clipBounds() const final;
    void clipToImageBuffer(ImageBuffer&, const FloatRect&) final;
    
    RenderingMode renderingMode() const final;

    cairo_t* cr() const;
    Vector<float>& layers();
    void pushImageMask(cairo_surface_t*, const FloatRect&);

private:
    RefPtr<cairo_t> m_cr;

    class CairoState;
    CairoState* m_cairoState;
    Vector<CairoState> m_cairoStateStack;

    // Transparency layers.
    Vector<float> m_layers;
};

} // namespace WebCore

#endif // USE(CAIRO)
