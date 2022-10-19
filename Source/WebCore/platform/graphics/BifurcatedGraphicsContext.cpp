/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#if ASSERT_ENABLED
#define VERIFY_STATE_SYNCHRONIZATION() do { \
    verifyStateSynchronization(); \
} while (0)
#else
#define VERIFY_STATE_SYNCHRONIZATION() ((void)0)
#endif

namespace WebCore {

BifurcatedGraphicsContext::BifurcatedGraphicsContext(GraphicsContext& primaryContext, GraphicsContext& secondaryContext)
    : m_primaryContext(primaryContext)
    , m_secondaryContext(secondaryContext)
{
    VERIFY_STATE_SYNCHRONIZATION();
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

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::restore()
{
    GraphicsContext::restore();
    m_primaryContext.restore();
    m_secondaryContext.restore();

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::drawRect(const FloatRect& rect, float borderThickness)
{
    m_primaryContext.drawRect(rect, borderThickness);
    m_secondaryContext.drawRect(rect, borderThickness);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::drawLine(const FloatPoint& from, const FloatPoint& to)
{
    m_primaryContext.drawLine(from, to);
    m_secondaryContext.drawLine(from, to);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::drawEllipse(const FloatRect& rect)
{
    m_primaryContext.drawEllipse(rect);
    m_secondaryContext.drawEllipse(rect);

    VERIFY_STATE_SYNCHRONIZATION();
}

#if USE(CG)
void BifurcatedGraphicsContext::applyStrokePattern()
{
    m_primaryContext.applyStrokePattern();
    m_secondaryContext.applyStrokePattern();

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::applyFillPattern()
{
    m_primaryContext.applyFillPattern();
    m_secondaryContext.applyFillPattern();

    VERIFY_STATE_SYNCHRONIZATION();
}
#endif

void BifurcatedGraphicsContext::drawPath(const Path& path)
{
    m_primaryContext.drawPath(path);
    m_secondaryContext.drawPath(path);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::fillPath(const Path& path)
{
    m_primaryContext.fillPath(path);
    m_secondaryContext.fillPath(path);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::strokePath(const Path& path)
{
    m_primaryContext.strokePath(path);
    m_secondaryContext.strokePath(path);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::beginTransparencyLayer(float opacity)
{
    GraphicsContext::beginTransparencyLayer(opacity);
    m_primaryContext.beginTransparencyLayer(opacity);
    m_secondaryContext.beginTransparencyLayer(opacity);
    m_state.didBeginTransparencyLayer();

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::endTransparencyLayer()
{
    GraphicsContext::endTransparencyLayer();
    m_primaryContext.endTransparencyLayer();
    m_secondaryContext.endTransparencyLayer();
    m_state.didEndTransparencyLayer(m_primaryContext.alpha());

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::applyDeviceScaleFactor(float factor)
{
    m_primaryContext.applyDeviceScaleFactor(factor);
    m_secondaryContext.applyDeviceScaleFactor(factor);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::fillRect(const FloatRect& rect)
{
    m_primaryContext.fillRect(rect);
    m_secondaryContext.fillRect(rect);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::fillRect(const FloatRect& rect, const Color& color)
{
    m_primaryContext.fillRect(rect, color);
    m_secondaryContext.fillRect(rect, color);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::fillRoundedRectImpl(const FloatRoundedRect& rect, const Color& color)
{
    m_primaryContext.fillRoundedRectImpl(rect, color);
    m_secondaryContext.fillRoundedRectImpl(rect, color);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    m_primaryContext.fillRectWithRoundedHole(rect, roundedHoleRect, color);
    m_secondaryContext.fillRectWithRoundedHole(rect, roundedHoleRect, color);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::clearRect(const FloatRect& rect)
{
    m_primaryContext.clearRect(rect);
    m_secondaryContext.clearRect(rect);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::strokeRect(const FloatRect& rect, float lineWidth)
{
    m_primaryContext.strokeRect(rect, lineWidth);
    m_secondaryContext.strokeRect(rect, lineWidth);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::fillEllipse(const FloatRect& ellipse)
{
    m_primaryContext.fillEllipse(ellipse);
    m_secondaryContext.fillEllipse(ellipse);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::strokeEllipse(const FloatRect& ellipse)
{
    m_primaryContext.strokeEllipse(ellipse);
    m_secondaryContext.strokeEllipse(ellipse);

    VERIFY_STATE_SYNCHRONIZATION();
}

#if USE(CG)
void BifurcatedGraphicsContext::setIsCALayerContext(bool isCALayerContext)
{
    m_primaryContext.setIsCALayerContext(isCALayerContext);
    m_secondaryContext.setIsCALayerContext(isCALayerContext);

    VERIFY_STATE_SYNCHRONIZATION();
}

bool BifurcatedGraphicsContext::isCALayerContext() const
{
    return m_primaryContext.isCALayerContext();
}

void BifurcatedGraphicsContext::setIsAcceleratedContext(bool isAcceleratedContext)
{
    m_primaryContext.setIsAcceleratedContext(isAcceleratedContext);
    m_secondaryContext.setIsAcceleratedContext(isAcceleratedContext);

    VERIFY_STATE_SYNCHRONIZATION();
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

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::clipOut(const FloatRect& rect)
{
    m_primaryContext.clipOut(rect);
    m_secondaryContext.clipOut(rect);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::clipOut(const Path& path)
{
    m_primaryContext.clipOut(path);
    m_secondaryContext.clipOut(path);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::clipPath(const Path& path, WindRule windRule)
{
    m_primaryContext.clipPath(path, windRule);
    m_secondaryContext.clipPath(path, windRule);

    VERIFY_STATE_SYNCHRONIZATION();
}

IntRect BifurcatedGraphicsContext::clipBounds() const
{
    return m_primaryContext.clipBounds();
}

void BifurcatedGraphicsContext::setLineCap(LineCap lineCap)
{
    m_primaryContext.setLineCap(lineCap);
    m_secondaryContext.setLineCap(lineCap);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::setLineDash(const DashArray& dashArray, float dashOffset)
{
    m_primaryContext.setLineDash(dashArray, dashOffset);
    m_secondaryContext.setLineDash(dashArray, dashOffset);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::setLineJoin(LineJoin lineJoin)
{
    m_primaryContext.setLineJoin(lineJoin);
    m_secondaryContext.setLineJoin(lineJoin);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::setMiterLimit(float miterLimit)
{
    m_primaryContext.setMiterLimit(miterLimit);
    m_secondaryContext.setMiterLimit(miterLimit);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::drawNativeImage(NativeImage& nativeImage, const FloatSize& selfSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    m_primaryContext.drawNativeImage(nativeImage, selfSize, destRect, srcRect, options);
    m_secondaryContext.drawNativeImage(nativeImage, selfSize, destRect, srcRect, options);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::drawSystemImage(SystemImage& systemImage, const FloatRect& destinationRect)
{
    m_primaryContext.drawSystemImage(systemImage, destinationRect);
    m_secondaryContext.drawSystemImage(systemImage, destinationRect);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::drawPattern(NativeImage& nativeImage, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    m_primaryContext.drawPattern(nativeImage, destRect, tileRect, patternTransform, phase, spacing, options);
    m_secondaryContext.drawPattern(nativeImage, destRect, tileRect, patternTransform, phase, spacing, options);

    VERIFY_STATE_SYNCHRONIZATION();
}

ImageDrawResult BifurcatedGraphicsContext::drawImage(Image& image, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions& options)
{
    auto result = m_primaryContext.drawImage(image, destination, source, options);
    m_secondaryContext.drawImage(image, destination, source, options);

    VERIFY_STATE_SYNCHRONIZATION();

    return result;
}

ImageDrawResult BifurcatedGraphicsContext::drawTiledImage(Image& image, const FloatRect& destination, const FloatPoint& source, const FloatSize& tileSize, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    auto result = m_primaryContext.drawTiledImage(image, destination, source, tileSize, spacing, options);
    m_secondaryContext.drawTiledImage(image, destination, source, tileSize, spacing, options);

    VERIFY_STATE_SYNCHRONIZATION();

    return result;
}

ImageDrawResult BifurcatedGraphicsContext::drawTiledImage(Image& image, const FloatRect& destination, const FloatRect& source, const FloatSize& tileScaleFactor, Image::TileRule hRule, Image::TileRule vRule, const ImagePaintingOptions& options)
{
    auto result = m_primaryContext.drawTiledImage(image, destination, source, tileScaleFactor, hRule, vRule, options);
    m_secondaryContext.drawTiledImage(image, destination, source, tileScaleFactor, hRule, vRule, options);
    
    VERIFY_STATE_SYNCHRONIZATION();

    return result;
}

#if ENABLE(VIDEO)
void BifurcatedGraphicsContext::paintFrameForMedia(MediaPlayer& player, const FloatRect& destination)
{
    m_primaryContext.paintFrameForMedia(player, destination);
    m_secondaryContext.paintFrameForMedia(player, destination);

    VERIFY_STATE_SYNCHRONIZATION();
}
#endif // ENABLE(VIDEO)

void BifurcatedGraphicsContext::scale(const FloatSize& scale)
{
    m_primaryContext.scale(scale);
    m_secondaryContext.scale(scale);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::rotate(float angleInRadians)
{
    m_primaryContext.rotate(angleInRadians);
    m_secondaryContext.rotate(angleInRadians);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::translate(float x, float y)
{
    m_primaryContext.translate(x, y);
    m_secondaryContext.translate(x, y);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::concatCTM(const AffineTransform& transform)
{
    m_primaryContext.concatCTM(transform);
    m_secondaryContext.concatCTM(transform);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::setCTM(const AffineTransform& transform)
{
    m_primaryContext.setCTM(transform);
    m_secondaryContext.setCTM(transform);

    VERIFY_STATE_SYNCHRONIZATION();
}

AffineTransform BifurcatedGraphicsContext::getCTM(IncludeDeviceScale includeDeviceScale) const
{
    return m_primaryContext.getCTM(includeDeviceScale);
}

void BifurcatedGraphicsContext::drawFocusRing(const Vector<FloatRect>& rects, float width, float offset, const Color& color)
{
    m_primaryContext.drawFocusRing(rects, width, offset, color);
    m_secondaryContext.drawFocusRing(rects, width, offset, color);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::drawFocusRing(const Path& path, float width, float offset, const Color& color)
{
    m_primaryContext.drawFocusRing(path, width, offset, color);
    m_secondaryContext.drawFocusRing(path, width, offset, color);

    VERIFY_STATE_SYNCHRONIZATION();
}

#if PLATFORM(MAC)
void BifurcatedGraphicsContext::drawFocusRing(const Path& path, double timeOffset, bool& needsRedraw, const Color& color)
{
    m_primaryContext.drawFocusRing(path, timeOffset, needsRedraw, color);
    m_secondaryContext.drawFocusRing(path, timeOffset, needsRedraw, color);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::drawFocusRing(const Vector<FloatRect>& rects, double timeOffset, bool& needsRedraw, const Color& color)
{
    m_primaryContext.drawFocusRing(rects, timeOffset, needsRedraw, color);
    m_secondaryContext.drawFocusRing(rects, timeOffset, needsRedraw, color);

    VERIFY_STATE_SYNCHRONIZATION();
}
#endif

FloatSize BifurcatedGraphicsContext::drawText(const FontCascade& cascade, const TextRun& run, const FloatPoint& point, unsigned from, std::optional<unsigned> to)
{
    auto size = m_primaryContext.drawText(cascade, run, point, from, to);
    m_secondaryContext.drawText(cascade, run, point, from, to);
    
    VERIFY_STATE_SYNCHRONIZATION();

    return size;
}

void BifurcatedGraphicsContext::drawGlyphs(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned numGlyphs, const FloatPoint& point, FontSmoothingMode fontSmoothingMode)
{
    m_primaryContext.drawGlyphs(font, glyphs, advances, numGlyphs, point, fontSmoothingMode);
    m_secondaryContext.drawGlyphs(font, glyphs, advances, numGlyphs, point, fontSmoothingMode);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::drawDecomposedGlyphs(const Font& font, const DecomposedGlyphs& decomposedGlyphs)
{
    m_primaryContext.drawDecomposedGlyphs(font, decomposedGlyphs);
    m_secondaryContext.drawDecomposedGlyphs(font, decomposedGlyphs);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::drawEmphasisMarks(const FontCascade& cascade, const TextRun& run, const AtomString& mark, const FloatPoint& point, unsigned from, std::optional<unsigned> to)
{
    m_primaryContext.drawEmphasisMarks(cascade, run, mark, point, from, to);
    m_secondaryContext.drawEmphasisMarks(cascade, run, mark, point, from, to);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::drawBidiText(const FontCascade& cascade, const TextRun& run, const FloatPoint& point, FontCascade::CustomFontNotReadyAction customFontNotReadyAction)
{
    m_primaryContext.drawBidiText(cascade, run, point, customFontNotReadyAction);
    m_secondaryContext.drawBidiText(cascade, run, point, customFontNotReadyAction);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::drawLinesForText(const FloatPoint& point, float thickness, const DashArray& widths, bool printing, bool doubleLines, StrokeStyle strokeStyle)
{
    m_primaryContext.drawLinesForText(point, thickness, widths, printing, doubleLines, strokeStyle);
    m_secondaryContext.drawLinesForText(point, thickness, widths, printing, doubleLines, strokeStyle);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::drawDotsForDocumentMarker(const FloatRect& rect, DocumentMarkerLineStyle markerStyle)
{
    m_primaryContext.drawDotsForDocumentMarker(rect, markerStyle);
    m_secondaryContext.drawDotsForDocumentMarker(rect, markerStyle);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::setURLForRect(const URL& url, const FloatRect& rect)
{
    m_primaryContext.setURLForRect(url, rect);
    m_secondaryContext.setURLForRect(url, rect);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::setDestinationForRect(const String& name, const FloatRect& rect)
{
    m_primaryContext.setDestinationForRect(name, rect);
    m_secondaryContext.setDestinationForRect(name, rect);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::addDestinationAtPoint(const String& name, const FloatPoint& point)
{
    m_primaryContext.addDestinationAtPoint(name, point);
    m_secondaryContext.addDestinationAtPoint(name, point);

    VERIFY_STATE_SYNCHRONIZATION();
}

bool BifurcatedGraphicsContext::supportsInternalLinks() const
{
    return m_primaryContext.supportsInternalLinks();
}

void BifurcatedGraphicsContext::didUpdateState(GraphicsContextState& state)
{
    // This calls mergeLastChanges() instead of didUpdateState() so that changes
    // are also applied to each context's GraphicsContextState, so that code
    // internal to the child contexts that reads from the state gets the right values.
    m_primaryContext.mergeLastChanges(state);
    m_secondaryContext.mergeLastChanges(state);
    state.didApplyChanges();

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::verifyStateSynchronization()
{
    auto primaryContextCTM = m_primaryContext.getCTM();

    // The two contexts' CTMs must begin and remain in sync, otherwise `setCTM(getCTM())`
    // will cause further painting to the secondary context to be mistransformed.
    auto secondaryContextCTM = m_secondaryContext.getCTM();
    if (!m_hasLoggedAboutDesynchronizedState && !primaryContextCTM.isEssentiallyEqualToAsFloats(secondaryContextCTM)) {
        ALWAYS_LOG_WITH_STREAM(stream << "BifurcatedGraphicsContext(" << this << ") CTM is out of sync: " << primaryContextCTM << " != " << secondaryContextCTM);
        m_hasLoggedAboutDesynchronizedState = true;
    }
}

#if OS(WINDOWS) && !USE(CAIRO)
GraphicsContextPlatformPrivate* BifurcatedGraphicsContext::deprecatedPrivateContext() const
{
    return m_primaryContext.deprecatedPrivateContext();
}
#endif

} // namespace WebCore

