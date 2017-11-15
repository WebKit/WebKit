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

#include "config.h"
#include "GraphicsContextImplCairo.h"

#if USE(CAIRO)

namespace WebCore {

GraphicsContextImplCairo::GraphicsContextImplCairo(GraphicsContext& context, PlatformContextCairo& platformContext)
    : GraphicsContextImpl(context, FloatRect { }, AffineTransform { })
    , m_platformContext(platformContext)
{
}

GraphicsContextImplCairo::~GraphicsContextImplCairo() = default;

void GraphicsContextImplCairo::updateState(const GraphicsContextState&, GraphicsContextState::StateChangeFlags)
{
}

void GraphicsContextImplCairo::clearShadow()
{
}

void GraphicsContextImplCairo::setLineCap(LineCap)
{
}

void GraphicsContextImplCairo::setLineDash(const DashArray&, float)
{
}

void GraphicsContextImplCairo::setLineJoin(LineJoin)
{
}

void GraphicsContextImplCairo::setMiterLimit(float)
{
}

void GraphicsContextImplCairo::fillRect(const FloatRect&)
{
}

void GraphicsContextImplCairo::fillRect(const FloatRect&, const Color&)
{
}

void GraphicsContextImplCairo::fillRect(const FloatRect&, Gradient&)
{
}

void GraphicsContextImplCairo::fillRect(const FloatRect&, const Color&, CompositeOperator, BlendMode)
{
}

void GraphicsContextImplCairo::fillRoundedRect(const FloatRoundedRect&, const Color&, BlendMode)
{
}

void GraphicsContextImplCairo::fillRectWithRoundedHole(const FloatRect&, const FloatRoundedRect&, const Color&)
{
}

void GraphicsContextImplCairo::fillPath(const Path&)
{
}

void GraphicsContextImplCairo::fillEllipse(const FloatRect&)
{
}

void GraphicsContextImplCairo::strokeRect(const FloatRect&, float)
{
}

void GraphicsContextImplCairo::strokePath(const Path&)
{
}

void GraphicsContextImplCairo::strokeEllipse(const FloatRect&)
{
}

void GraphicsContextImplCairo::clearRect(const FloatRect&)
{
}

void GraphicsContextImplCairo::drawGlyphs(const Font&, const GlyphBuffer&, unsigned, unsigned, const FloatPoint&, FontSmoothingMode)
{
}

void GraphicsContextImplCairo::drawImage(Image&, const FloatRect&, const FloatRect&, const ImagePaintingOptions&)
{
}

void GraphicsContextImplCairo::drawTiledImage(Image&, const FloatRect&, const FloatPoint&, const FloatSize&, const FloatSize&, const ImagePaintingOptions&)
{
}

void GraphicsContextImplCairo::drawTiledImage(Image&, const FloatRect&, const FloatRect&, const FloatSize&, Image::TileRule, Image::TileRule, const ImagePaintingOptions&)
{
}

void GraphicsContextImplCairo::drawNativeImage(const NativeImagePtr&, const FloatSize&, const FloatRect&, const FloatRect&, CompositeOperator, BlendMode, ImageOrientation)
{
}

void GraphicsContextImplCairo::drawPattern(Image&, const FloatRect&, const FloatRect&, const AffineTransform&, const FloatPoint&, const FloatSize&, CompositeOperator, BlendMode)
{
}

void GraphicsContextImplCairo::drawRect(const FloatRect&, float)
{
}

void GraphicsContextImplCairo::drawLine(const FloatPoint&, const FloatPoint&)
{
}

void GraphicsContextImplCairo::drawLinesForText(const FloatPoint&, const DashArray&, bool, bool, float)
{
}

void GraphicsContextImplCairo::drawLineForDocumentMarker(const FloatPoint&, float, GraphicsContext::DocumentMarkerLineStyle)
{
}

void GraphicsContextImplCairo::drawEllipse(const FloatRect&)
{
}

void GraphicsContextImplCairo::drawPath(const Path&)
{
}

void GraphicsContextImplCairo::drawFocusRing(const Path&, float, float, const Color&)
{
}

void GraphicsContextImplCairo::drawFocusRing(const Vector<FloatRect>&, float, float, const Color&)
{
}

void GraphicsContextImplCairo::save()
{
}

void GraphicsContextImplCairo::restore()
{
}

void GraphicsContextImplCairo::translate(float, float)
{
}

void GraphicsContextImplCairo::rotate(float)
{
}

void GraphicsContextImplCairo::scale(const FloatSize&)
{
}

void GraphicsContextImplCairo::concatCTM(const AffineTransform&)
{
}

void GraphicsContextImplCairo::beginTransparencyLayer(float)
{
}

void GraphicsContextImplCairo::endTransparencyLayer()
{
}

void GraphicsContextImplCairo::clip(const FloatRect&)
{
}

void GraphicsContextImplCairo::clipOut(const FloatRect&)
{
}

void GraphicsContextImplCairo::clipOut(const Path&)
{
}

void GraphicsContextImplCairo::clipPath(const Path&, WindRule)
{
}

void GraphicsContextImplCairo::applyDeviceScaleFactor(float)
{
}

} // namespace WebCore

#endif // USE(CAIRO)
