/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "GraphicsContext.h"

namespace WebCore {

class GraphicsContextImpl {
    WTF_MAKE_NONCOPYABLE(GraphicsContextImpl);
public:
    GraphicsContextImpl(GraphicsContext&, const FloatRect& initialClip, const AffineTransform&);
    virtual ~GraphicsContextImpl();

    GraphicsContext& graphicsContext() const { return m_graphicsContext; }

    virtual bool hasPlatformContext() const = 0;
    virtual PlatformGraphicsContext* platformContext() const = 0;

    virtual void updateState(const GraphicsContextState&, GraphicsContextState::StateChangeFlags) = 0;
    virtual void clearShadow() = 0;

    virtual void setLineCap(LineCap) = 0;
    virtual void setLineDash(const DashArray&, float dashOffset) = 0;
    virtual void setLineJoin(LineJoin) = 0;
    virtual void setMiterLimit(float) = 0;

    virtual void fillRect(const FloatRect&) = 0;
    virtual void fillRect(const FloatRect&, const Color&) = 0;
    virtual void fillRect(const FloatRect&, Gradient&) = 0;
    virtual void fillRect(const FloatRect&, const Color&, CompositeOperator, BlendMode) = 0;
    virtual void fillRoundedRect(const FloatRoundedRect&, const Color&, BlendMode) = 0;
    virtual void fillRectWithRoundedHole(const FloatRect&, const FloatRoundedRect& roundedHoleRect, const Color&) = 0;
    virtual void fillPath(const Path&) = 0;
    virtual void fillEllipse(const FloatRect&) = 0;
    virtual void strokeRect(const FloatRect&, float lineWidth) = 0;
    virtual void strokePath(const Path&) = 0;
    virtual void strokeEllipse(const FloatRect&) = 0;
    virtual void clearRect(const FloatRect&) = 0;

#if USE(CG)
    virtual void applyStrokePattern() = 0;
    virtual void applyFillPattern() = 0;
#endif

    virtual void drawGlyphs(const Font&, const GlyphBuffer&, unsigned from, unsigned numGlyphs, const FloatPoint& anchorPoint, FontSmoothingMode) = 0;

    virtual ImageDrawResult drawImage(Image&, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions&) = 0;
    virtual ImageDrawResult drawTiledImage(Image&, const FloatRect& destination, const FloatPoint& source, const FloatSize& tileSize, const FloatSize& spacing, const ImagePaintingOptions&) = 0;
    virtual ImageDrawResult drawTiledImage(Image&, const FloatRect& destination, const FloatRect& source, const FloatSize& tileScaleFactor, Image::TileRule hRule, Image::TileRule vRule, const ImagePaintingOptions&) = 0;
#if USE(CG) || USE(CAIRO)
    virtual void drawNativeImage(const NativeImagePtr&, const FloatSize& selfSize, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator, BlendMode, ImageOrientation) = 0;
#endif
    virtual void drawPattern(Image&, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform&, const FloatPoint& phase, const FloatSize& spacing, CompositeOperator, BlendMode = BlendMode::Normal) = 0;

    virtual void drawRect(const FloatRect&, float borderThickness) = 0;
    virtual void drawLine(const FloatPoint&, const FloatPoint&) = 0;
    virtual void drawLinesForText(const FloatPoint&, const DashArray& widths, bool printing, bool doubleLines, float strokeThickness) = 0;
    virtual void drawDotsForDocumentMarker(const FloatRect&, DocumentMarkerLineStyle) = 0;
    virtual void drawEllipse(const FloatRect&) = 0;
    virtual void drawPath(const Path&) = 0;

    virtual void drawFocusRing(const Path&, float width, float offset, const Color&) = 0;
    virtual void drawFocusRing(const Vector<FloatRect>&, float width, float offset, const Color&) = 0;

    virtual void save() = 0;
    virtual void restore() = 0;

    virtual void translate(float x, float y) = 0;
    virtual void rotate(float angleInRadians) = 0;
    virtual void scale(const FloatSize&) = 0;
    virtual void concatCTM(const AffineTransform&) = 0;
    virtual void setCTM(const AffineTransform&) = 0;
    virtual AffineTransform getCTM(GraphicsContext::IncludeDeviceScale) = 0;

    virtual void beginTransparencyLayer(float opacity) = 0;
    virtual void endTransparencyLayer() = 0;

    virtual void clip(const FloatRect&) = 0;
    virtual void clipOut(const FloatRect&) = 0;
    virtual void clipOut(const Path&) = 0;
    virtual void clipPath(const Path&, WindRule) = 0;
    virtual IntRect clipBounds() = 0;
    virtual void clipToImageBuffer(ImageBuffer&, const FloatRect&) = 0;
    
    virtual void applyDeviceScaleFactor(float) = 0;

    virtual FloatRect roundToDevicePixels(const FloatRect&, GraphicsContext::RoundingMode) = 0;

protected:
    static ImageDrawResult drawImageImpl(GraphicsContext&, Image&, const FloatRect&, const FloatRect&, const ImagePaintingOptions&);
    static ImageDrawResult drawTiledImageImpl(GraphicsContext&, Image&, const FloatRect&, const FloatPoint&, const FloatSize&, const FloatSize&, const ImagePaintingOptions&);
    static ImageDrawResult drawTiledImageImpl(GraphicsContext&, Image&, const FloatRect&, const FloatRect&, const FloatSize&, Image::TileRule, Image::TileRule, const ImagePaintingOptions&);

private:
    friend class GraphicsContext;
    GraphicsContext& m_graphicsContext;
};

} // namespace WebCore
