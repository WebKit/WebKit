/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "BifurcatedGraphicsContext.h"

namespace WebCore {

BifurcatedGraphicsContext::BifurcatedGraphicsContext(GraphicsContext& primaryContext, GraphicsContext& secondaryContext)
    : m_primaryContext(primaryContext)
    , m_secondaryContext(secondaryContext)
{
}

BifurcatedGraphicsContext::~BifurcatedGraphicsContext()
{
}

bool BifurcatedGraphicsContext::hasPlatformContext() const
{
    return m_primaryContext.hasPlatformContext();
}

PlatformGraphicsContext* BifurcatedGraphicsContext::platformContext() const
{
    return m_primaryContext.platformContext();
}

void BifurcatedGraphicsContext::save()
{
    // FIXME: Consider not using the BifurcatedGraphicsContext's state stack at all,
    // and making all of the state getters and setters virtual.
    GraphicsContext::save();
    m_primaryContext.save();
    m_secondaryContext.save();
}

void BifurcatedGraphicsContext::restore()
{
    GraphicsContext::restore();
    m_primaryContext.restore();
    m_secondaryContext.restore();
}

void BifurcatedGraphicsContext::drawRect(const FloatRect& rect, float borderThickness)
{
    m_primaryContext.drawRect(rect, borderThickness);
    m_secondaryContext.drawRect(rect, borderThickness);
}

void BifurcatedGraphicsContext::drawLine(const FloatPoint& from, const FloatPoint& to)
{
    m_primaryContext.drawLine(from, to);
    m_secondaryContext.drawLine(from, to);
}

void BifurcatedGraphicsContext::drawEllipse(const FloatRect& rect)
{
    m_primaryContext.drawEllipse(rect);
    m_secondaryContext.drawEllipse(rect);
}

#if USE(CG) || USE(DIRECT2D)
void BifurcatedGraphicsContext::applyStrokePattern()
{
    m_primaryContext.applyStrokePattern();
    m_secondaryContext.applyStrokePattern();
}

void BifurcatedGraphicsContext::applyFillPattern()
{
    m_primaryContext.applyFillPattern();
    m_secondaryContext.applyFillPattern();
}
#endif

void BifurcatedGraphicsContext::drawPath(const Path& path)
{
    m_primaryContext.drawPath(path);
    m_secondaryContext.drawPath(path);
}

void BifurcatedGraphicsContext::fillPath(const Path& path)
{
    m_primaryContext.fillPath(path);
    m_secondaryContext.fillPath(path);
}

void BifurcatedGraphicsContext::strokePath(const Path& path)
{
    m_primaryContext.strokePath(path);
    m_secondaryContext.strokePath(path);
}

void BifurcatedGraphicsContext::beginTransparencyLayer(float opacity)
{
    m_primaryContext.beginTransparencyLayer(opacity);
    m_secondaryContext.beginTransparencyLayer(opacity);
}

void BifurcatedGraphicsContext::endTransparencyLayer()
{
    m_primaryContext.endTransparencyLayer();
    m_secondaryContext.endTransparencyLayer();
}

void BifurcatedGraphicsContext::applyDeviceScaleFactor(float factor)
{
    m_primaryContext.applyDeviceScaleFactor(factor);
    m_secondaryContext.applyDeviceScaleFactor(factor);
}

void BifurcatedGraphicsContext::fillRect(const FloatRect& rect)
{
    m_primaryContext.fillRect(rect);
    m_secondaryContext.fillRect(rect);
}

void BifurcatedGraphicsContext::fillRect(const FloatRect& rect, const Color& color)
{
    m_primaryContext.fillRect(rect, color);
    m_secondaryContext.fillRect(rect, color);
}

void BifurcatedGraphicsContext::fillRoundedRectImpl(const FloatRoundedRect& rect, const Color& color)
{
    m_primaryContext.fillRoundedRectImpl(rect, color);
    m_secondaryContext.fillRoundedRectImpl(rect, color);
}

void BifurcatedGraphicsContext::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    m_primaryContext.fillRectWithRoundedHole(rect, roundedHoleRect, color);
    m_secondaryContext.fillRectWithRoundedHole(rect, roundedHoleRect, color);
}

void BifurcatedGraphicsContext::clearRect(const FloatRect& rect)
{
    m_primaryContext.clearRect(rect);
    m_secondaryContext.clearRect(rect);
}

void BifurcatedGraphicsContext::strokeRect(const FloatRect& rect, float lineWidth)
{
    m_primaryContext.strokeRect(rect, lineWidth);
    m_secondaryContext.strokeRect(rect, lineWidth);
}

void BifurcatedGraphicsContext::fillEllipse(const FloatRect& ellipse)
{
    m_primaryContext.fillEllipse(ellipse);
    m_secondaryContext.fillEllipse(ellipse);
}

void BifurcatedGraphicsContext::strokeEllipse(const FloatRect& ellipse)
{
    m_primaryContext.strokeEllipse(ellipse);
    m_secondaryContext.strokeEllipse(ellipse);
}

#if USE(CG) || USE(DIRECT2D)
void BifurcatedGraphicsContext::setIsCALayerContext(bool isCALayerContext)
{
    m_primaryContext.setIsCALayerContext(isCALayerContext);
    m_secondaryContext.setIsCALayerContext(isCALayerContext);
}

bool BifurcatedGraphicsContext::isCALayerContext() const
{
    return m_primaryContext.isCALayerContext();
}

void BifurcatedGraphicsContext::setIsAcceleratedContext(bool isAcceleratedContext)
{
    m_primaryContext.setIsAcceleratedContext(isAcceleratedContext);
    m_secondaryContext.setIsAcceleratedContext(isAcceleratedContext);
}
#endif

RenderingMode BifurcatedGraphicsContext::renderingMode() const
{
    return m_primaryContext.renderingMode();
}

void BifurcatedGraphicsContext::clip(const FloatRect& rect)
{
    m_primaryContext.clip(rect);
    m_secondaryContext.clip(rect);
}

void BifurcatedGraphicsContext::clipOut(const FloatRect& rect)
{
    m_primaryContext.clipOut(rect);
    m_secondaryContext.clipOut(rect);
}

void BifurcatedGraphicsContext::clipOut(const Path& path)
{
    m_primaryContext.clipOut(path);
    m_secondaryContext.clipOut(path);
}

void BifurcatedGraphicsContext::clipPath(const Path& path, WindRule windRule)
{
    m_primaryContext.clipPath(path, windRule);
    m_secondaryContext.clipPath(path, windRule);
}

IntRect BifurcatedGraphicsContext::clipBounds() const
{
    return m_primaryContext.clipBounds();
}

void BifurcatedGraphicsContext::setLineCap(LineCap lineCap)
{
    m_primaryContext.setLineCap(lineCap);
    m_secondaryContext.setLineCap(lineCap);
}

void BifurcatedGraphicsContext::setLineDash(const DashArray& dashArray, float dashOffset)
{
    m_primaryContext.setLineDash(dashArray, dashOffset);
    m_secondaryContext.setLineDash(dashArray, dashOffset);
}

void BifurcatedGraphicsContext::setLineJoin(LineJoin lineJoin)
{
    m_primaryContext.setLineJoin(lineJoin);
    m_secondaryContext.setLineJoin(lineJoin);
}

void BifurcatedGraphicsContext::setMiterLimit(float miterLimit)
{
    m_primaryContext.setMiterLimit(miterLimit);
    m_secondaryContext.setMiterLimit(miterLimit);
}

void BifurcatedGraphicsContext::drawNativeImage(NativeImage& nativeImage, const FloatSize& selfSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    m_primaryContext.drawNativeImage(nativeImage, selfSize, destRect, srcRect, options);
    m_secondaryContext.drawNativeImage(nativeImage, selfSize, destRect, srcRect, options);
}

void BifurcatedGraphicsContext::drawPattern(NativeImage& nativeImage, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    m_primaryContext.drawPattern(nativeImage, imageSize, destRect, tileRect, patternTransform, phase, spacing, options);
    m_secondaryContext.drawPattern(nativeImage, imageSize, destRect, tileRect, patternTransform, phase, spacing, options);
}

#if ENABLE(VIDEO)
void BifurcatedGraphicsContext::paintFrameForMedia(MediaPlayer& player, const FloatRect& destination)
{
    m_primaryContext.paintFrameForMedia(player, destination);
    m_secondaryContext.paintFrameForMedia(player, destination);
}
#endif // ENABLE(VIDEO)

void BifurcatedGraphicsContext::scale(const FloatSize& scale)
{
    m_primaryContext.scale(scale);
    m_secondaryContext.scale(scale);
}

void BifurcatedGraphicsContext::rotate(float angleInRadians)
{
    m_primaryContext.rotate(angleInRadians);
    m_secondaryContext.rotate(angleInRadians);
}

void BifurcatedGraphicsContext::translate(float x, float y)
{
    m_primaryContext.translate(x, y);
    m_secondaryContext.translate(x, y);
}

void BifurcatedGraphicsContext::concatCTM(const AffineTransform& transform)
{
    m_primaryContext.concatCTM(transform);
    m_secondaryContext.concatCTM(transform);
}

void BifurcatedGraphicsContext::setCTM(const AffineTransform& transform)
{
    m_primaryContext.setCTM(transform);
    m_secondaryContext.setCTM(transform);
}

AffineTransform BifurcatedGraphicsContext::getCTM(IncludeDeviceScale includeDeviceScale) const
{
    return m_primaryContext.getCTM(includeDeviceScale);
}

FloatRect BifurcatedGraphicsContext::roundToDevicePixels(const FloatRect& rect, RoundingMode roundingMode)
{
    return m_primaryContext.roundToDevicePixels(rect, roundingMode);
}

void BifurcatedGraphicsContext::drawFocusRing(const Vector<FloatRect>& rects, float width, float offset, const Color& color)
{
    m_primaryContext.drawFocusRing(rects, width, offset, color);
    m_secondaryContext.drawFocusRing(rects, width, offset, color);
}

void BifurcatedGraphicsContext::drawFocusRing(const Path& path, float width, float offset, const Color& color)
{
    m_primaryContext.drawFocusRing(path, width, offset, color);
    m_secondaryContext.drawFocusRing(path, width, offset, color);
}

#if PLATFORM(MAC)
void BifurcatedGraphicsContext::drawFocusRing(const Path& path, double timeOffset, bool& needsRedraw, const Color& color)
{
    m_primaryContext.drawFocusRing(path, timeOffset, needsRedraw, color);
    m_secondaryContext.drawFocusRing(path, timeOffset, needsRedraw, color);
}

void BifurcatedGraphicsContext::drawFocusRing(const Vector<FloatRect>& rects, double timeOffset, bool& needsRedraw, const Color& color)
{
    m_primaryContext.drawFocusRing(rects, timeOffset, needsRedraw, color);
    m_secondaryContext.drawFocusRing(rects, timeOffset, needsRedraw, color);
}
#endif

FloatSize BifurcatedGraphicsContext::drawText(const FontCascade& cascade, const TextRun& run, const FloatPoint& point, unsigned from, std::optional<unsigned> to)
{
    auto size = m_primaryContext.drawText(cascade, run, point, from, to);
    m_secondaryContext.drawText(cascade, run, point, from, to);
    return size;
}

void BifurcatedGraphicsContext::drawGlyphs(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned numGlyphs, const FloatPoint& point, FontSmoothingMode fontSmoothingMode)
{
    m_primaryContext.drawGlyphs(font, glyphs, advances, numGlyphs, point, fontSmoothingMode);
    m_secondaryContext.drawGlyphs(font, glyphs, advances, numGlyphs, point, fontSmoothingMode);
}

void BifurcatedGraphicsContext::drawEmphasisMarks(const FontCascade& cascade, const TextRun& run, const AtomString& mark, const FloatPoint& point, unsigned from, std::optional<unsigned> to)
{
    m_primaryContext.drawEmphasisMarks(cascade, run, mark, point, from, to);
    m_secondaryContext.drawEmphasisMarks(cascade, run, mark, point, from, to);
}

void BifurcatedGraphicsContext::drawBidiText(const FontCascade& cascade, const TextRun& run, const FloatPoint& point, FontCascade::CustomFontNotReadyAction customFontNotReadyAction)
{
    m_primaryContext.drawBidiText(cascade, run, point, customFontNotReadyAction);
    m_secondaryContext.drawBidiText(cascade, run, point, customFontNotReadyAction);
}

void BifurcatedGraphicsContext::drawLinesForText(const FloatPoint& point, float thickness, const DashArray& widths, bool printing, bool doubleLines, StrokeStyle strokeStyle)
{
    m_primaryContext.drawLinesForText(point, thickness, widths, printing, doubleLines, strokeStyle);
    m_secondaryContext.drawLinesForText(point, thickness, widths, printing, doubleLines, strokeStyle);
}

void BifurcatedGraphicsContext::drawDotsForDocumentMarker(const FloatRect& rect, DocumentMarkerLineStyle markerStyle)
{
    m_primaryContext.drawDotsForDocumentMarker(rect, markerStyle);
    m_secondaryContext.drawDotsForDocumentMarker(rect, markerStyle);
}

void BifurcatedGraphicsContext::setURLForRect(const URL& url, const FloatRect& rect)
{
    m_primaryContext.setURLForRect(url, rect);
    m_secondaryContext.setURLForRect(url, rect);
}

void BifurcatedGraphicsContext::setDestinationForRect(const String& name, const FloatRect& rect)
{
    m_primaryContext.setDestinationForRect(name, rect);
    m_secondaryContext.setDestinationForRect(name, rect);
}

void BifurcatedGraphicsContext::addDestinationAtPoint(const String& name, const FloatPoint& point)
{
    m_primaryContext.addDestinationAtPoint(name, point);
    m_secondaryContext.addDestinationAtPoint(name, point);
}

bool BifurcatedGraphicsContext::supportsInternalLinks() const
{
    return m_primaryContext.supportsInternalLinks();
}

void BifurcatedGraphicsContext::updateState(const GraphicsContextState& state, GraphicsContextState::StateChangeFlags flags)
{
    m_primaryContext.updateState(state, flags);
    m_secondaryContext.updateState(state, flags);
}

#if OS(WINDOWS) && !USE(CAIRO)
GraphicsContextPlatformPrivate* BifurcatedGraphicsContext::deprecatedPrivateContext() const
{
    return m_primaryContext.deprecatedPrivateContext();
}
#endif

} // namespace WebCore

