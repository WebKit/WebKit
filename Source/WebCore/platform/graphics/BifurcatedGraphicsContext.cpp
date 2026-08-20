/*
 * Copyright (C) 2021-2024 Apple Inc. All rights reserved.
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
#include <wtf/TZoneMallocInlines.h>

#if ASSERT_ENABLED
#define VERIFY_STATE_SYNCHRONIZATION() do { \
    verifyStateSynchronization(); \
} while (0)
#else
#define VERIFY_STATE_SYNCHRONIZATION() ((void)0)
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(BifurcatedGraphicsContext);

BifurcatedGraphicsContext::BifurcatedGraphicsContext(GraphicsContext& primaryContext, GraphicsContext& secondaryContext)
    : m_primaryContext(primaryContext)
    , m_secondaryContext(secondaryContext)
{
    VERIFY_STATE_SYNCHRONIZATION();
}

BifurcatedGraphicsContext::~BifurcatedGraphicsContext() = default;

bool BifurcatedGraphicsContext::hasPlatformContext() const
{
    return m_primaryContext.hasPlatformContext();
}

PlatformGraphicsContext* BifurcatedGraphicsContext::platformContext() const
{
    const_cast<BifurcatedGraphicsContext&>(*this).updatePlatformContextStateIfNeeded();
    return m_primaryContext.platformContext();
}

const DestinationColorSpace& BifurcatedGraphicsContext::colorSpace() const
{
    return m_primaryContext.colorSpace();
}

void BifurcatedGraphicsContext::save(GraphicsContextState::Purpose purpose)
{
    // FIXME: Consider not using the BifurcatedGraphicsContext's state stack at all,
    // and making all of the state getters and setters virtual.
    updatePlatformContextStateIfNeeded();
    GraphicsContext::save(purpose);
    m_primaryContext.save(purpose);
    m_secondaryContext.save(purpose);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::restore(GraphicsContextState::Purpose purpose)
{
    GraphicsContext::restore(purpose);
    m_primaryContext.restore(purpose);
    m_secondaryContext.restore(purpose);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::drawRect(const FloatRect& rect, float borderThickness)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.drawRect(rect, borderThickness);
    m_secondaryContext.drawRect(rect, borderThickness);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::drawLine(const FloatPoint& from, const FloatPoint& to)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.drawLine(from, to);
    m_secondaryContext.drawLine(from, to);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::drawEllipse(const FloatRect& rect)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.drawEllipse(rect);
    m_secondaryContext.drawEllipse(rect);

    VERIFY_STATE_SYNCHRONIZATION();
}

#if USE(CG)
void BifurcatedGraphicsContext::applyStrokePattern()
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.applyStrokePattern();
    m_secondaryContext.applyStrokePattern();

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::applyFillPattern()
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.applyFillPattern();
    m_secondaryContext.applyFillPattern();

    VERIFY_STATE_SYNCHRONIZATION();
}
#endif

void BifurcatedGraphicsContext::drawPath(const Path& path)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.drawPath(path);
    m_secondaryContext.drawPath(path);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::fillPath(const Path& path)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.fillPath(path);
    m_secondaryContext.fillPath(path);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::strokePath(const Path& path)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.strokePath(path);
    m_secondaryContext.strokePath(path);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::beginTransparencyLayer(float opacity)
{
    updatePlatformContextStateIfNeeded();
    GraphicsContext::beginTransparencyLayer(opacity);
    m_primaryContext.beginTransparencyLayer(opacity);
    m_secondaryContext.beginTransparencyLayer(opacity);

    GraphicsContext::save(GraphicsContextState::Purpose::TransparencyLayer);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::beginTransparencyLayer(CompositeOperator compositeOperator, BlendMode blendMode)
{
    updatePlatformContextStateIfNeeded();
    GraphicsContext::beginTransparencyLayer(compositeOperator, blendMode);
    m_primaryContext.beginTransparencyLayer(compositeOperator, blendMode);
    m_secondaryContext.beginTransparencyLayer(compositeOperator, blendMode);

    GraphicsContext::save(GraphicsContextState::Purpose::TransparencyLayer);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::endTransparencyLayer()
{
    updatePlatformContextStateIfNeeded();
    GraphicsContext::endTransparencyLayer();
    m_primaryContext.endTransparencyLayer();
    m_secondaryContext.endTransparencyLayer();

    GraphicsContext::restore(GraphicsContextState::Purpose::TransparencyLayer);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::applyDeviceScaleFactor(float factor)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.applyDeviceScaleFactor(factor);
    m_secondaryContext.applyDeviceScaleFactor(factor);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::fillRect(const FloatRect& rect, RequiresClipToRect requiresClipToRect)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.fillRect(rect, requiresClipToRect);
    m_secondaryContext.fillRect(rect, requiresClipToRect);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::fillRect(const FloatRect& rect, const Color& color)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.fillRect(rect, color);
    m_secondaryContext.fillRect(rect, color);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::fillRect(const FloatRect& rect, Gradient& gradient)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.fillRect(rect, gradient);
    m_secondaryContext.fillRect(rect, gradient);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::fillRect(const FloatRect& rect, Gradient& gradient, const AffineTransform& gradientSpaceTransform, RequiresClipToRect requiresClipToRect)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.fillRect(rect, gradient, gradientSpaceTransform, requiresClipToRect);
    m_secondaryContext.fillRect(rect, gradient, gradientSpaceTransform, requiresClipToRect);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::fillRoundedRectImpl(const FloatRoundedRect& rect, const Color& color)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.fillRoundedRectImpl(rect, color);
    m_secondaryContext.fillRoundedRectImpl(rect, color);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.fillRectWithRoundedHole(rect, roundedHoleRect, color);
    m_secondaryContext.fillRectWithRoundedHole(rect, roundedHoleRect, color);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::clearRect(const FloatRect& rect)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.clearRect(rect);
    m_secondaryContext.clearRect(rect);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::strokeRect(const FloatRect& rect, float lineWidth)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.strokeRect(rect, lineWidth);
    m_secondaryContext.strokeRect(rect, lineWidth);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::fillEllipse(const FloatRect& ellipse)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.fillEllipse(ellipse);
    m_secondaryContext.fillEllipse(ellipse);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::strokeEllipse(const FloatRect& ellipse)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.strokeEllipse(ellipse);
    m_secondaryContext.strokeEllipse(ellipse);

    VERIFY_STATE_SYNCHRONIZATION();
}

#if USE(CG)

bool BifurcatedGraphicsContext::isCALayerContext() const
{
    return m_primaryContext.isCALayerContext();
}

#endif

RenderingMode BifurcatedGraphicsContext::renderingMode() const
{
    return m_primaryContext.renderingMode();
}

void BifurcatedGraphicsContext::clip(const FloatRect& rect)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.clip(rect);
    m_secondaryContext.clip(rect);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::clipRoundedRect(const FloatRoundedRect& rect)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.clipRoundedRect(rect);
    m_secondaryContext.clipRoundedRect(rect);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::clipOut(const FloatRect& rect)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.clipOut(rect);
    m_secondaryContext.clipOut(rect);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::clipOutRoundedRect(const FloatRoundedRect& rect)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.clipOutRoundedRect(rect);
    m_secondaryContext.clipOutRoundedRect(rect);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::clipOut(const Path& path)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.clipOut(path);
    m_secondaryContext.clipOut(path);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::clipPath(const Path& path, WindRule windRule)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.clipPath(path, windRule);
    m_secondaryContext.clipPath(path, windRule);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::clipToImageBuffer(ImageBuffer& imageBuffer, const FloatRect& destRect)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.clipToImageBuffer(imageBuffer, destRect);
    m_secondaryContext.clipToImageBuffer(imageBuffer, destRect);

    VERIFY_STATE_SYNCHRONIZATION();
}

IntRect BifurcatedGraphicsContext::clipBounds() const
{
    return m_primaryContext.clipBounds();
}

void BifurcatedGraphicsContext::resetClip()
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.resetClip();
    m_secondaryContext.resetClip();

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::setLineCap(LineCap lineCap)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.setLineCap(lineCap);
    m_secondaryContext.setLineCap(lineCap);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::setLineDash(const DashArray& dashArray, float dashOffset)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.setLineDash(dashArray, dashOffset);
    m_secondaryContext.setLineDash(dashArray, dashOffset);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::setLineJoin(LineJoin lineJoin)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.setLineJoin(lineJoin);
    m_secondaryContext.setLineJoin(lineJoin);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::setMiterLimit(float miterLimit)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.setMiterLimit(miterLimit);
    m_secondaryContext.setMiterLimit(miterLimit);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::drawNativeImage(const NativeImage& nativeImage, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.drawNativeImage(nativeImage, destRect, srcRect, options);
    m_secondaryContext.drawNativeImage(nativeImage, destRect, srcRect, options);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::drawSystemImage(SystemImage& systemImage, const FloatRect& destinationRect)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.drawSystemImage(systemImage, destinationRect);
    m_secondaryContext.drawSystemImage(systemImage, destinationRect);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::drawControlPart(ControlPart& part, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.drawControlPart(part, borderRect, deviceScaleFactor, style);
    m_secondaryContext.drawControlPart(part, borderRect, deviceScaleFactor, style);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::drawPattern(const NativeImage& nativeImage, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.drawPattern(nativeImage, destRect, tileRect, patternTransform, phase, spacing, options);
    m_secondaryContext.drawPattern(nativeImage, destRect, tileRect, patternTransform, phase, spacing, options);

    VERIFY_STATE_SYNCHRONIZATION();
}

ImageDrawResult BifurcatedGraphicsContext::drawImage(Image& image, const FloatRect& destination, const FloatRect& source, ImagePaintingOptions options)
{
    updatePlatformContextStateIfNeeded();
    auto result = m_primaryContext.drawImage(image, destination, source, options);
    m_secondaryContext.drawImage(image, destination, source, options);

    VERIFY_STATE_SYNCHRONIZATION();

    return result;
}

ImageDrawResult BifurcatedGraphicsContext::drawTiledImage(Image& image, const FloatRect& destination, const FloatPoint& source, const FloatSize& tileSize, const FloatSize& spacing, ImagePaintingOptions options)
{
    updatePlatformContextStateIfNeeded();
    auto result = m_primaryContext.drawTiledImage(image, destination, source, tileSize, spacing, options);
    m_secondaryContext.drawTiledImage(image, destination, source, tileSize, spacing, options);

    VERIFY_STATE_SYNCHRONIZATION();

    return result;
}

ImageDrawResult BifurcatedGraphicsContext::drawTiledImage(Image& image, const FloatRect& destination, const FloatRect& source, const FloatSize& tileScaleFactor, Image::TileRule hRule, Image::TileRule vRule, ImagePaintingOptions options)
{
    updatePlatformContextStateIfNeeded();
    auto result = m_primaryContext.drawTiledImage(image, destination, source, tileScaleFactor, hRule, vRule, options);
    m_secondaryContext.drawTiledImage(image, destination, source, tileScaleFactor, hRule, vRule, options);
    
    VERIFY_STATE_SYNCHRONIZATION();

    return result;
}

#if ENABLE(VIDEO)
void BifurcatedGraphicsContext::drawVideoFrame(const VideoFrame& videoFrame, const FloatRect& destination, WebCore::ImageOrientation orientation, bool shouldDiscardAlpha)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.drawVideoFrame(videoFrame, destination, orientation, shouldDiscardAlpha);
    m_secondaryContext.drawVideoFrame(videoFrame, destination, orientation, shouldDiscardAlpha);

    VERIFY_STATE_SYNCHRONIZATION();
}
#endif // ENABLE(VIDEO)

void BifurcatedGraphicsContext::scale(const FloatSize& scale)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.scale(scale);
    m_secondaryContext.scale(scale);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::rotate(float angleInRadians)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.rotate(angleInRadians);
    m_secondaryContext.rotate(angleInRadians);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::translate(float x, float y)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.translate(x, y);
    m_secondaryContext.translate(x, y);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::concatCTM(const AffineTransform& transform)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.concatCTM(transform);
    m_secondaryContext.concatCTM(transform);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::setCTM(const AffineTransform& transform)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.setCTM(transform);
    m_secondaryContext.setCTM(transform);

    VERIFY_STATE_SYNCHRONIZATION();
}

AffineTransform BifurcatedGraphicsContext::getCTM(IncludeDeviceScale includeDeviceScale) const
{
    return m_primaryContext.getCTM(includeDeviceScale);
}

void BifurcatedGraphicsContext::drawFocusRing(const Path& path, float outlineWidth, const Color& color, float zoomFactor)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.drawFocusRing(path, outlineWidth, color, zoomFactor);
    m_secondaryContext.drawFocusRing(path, outlineWidth, color, zoomFactor);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::drawFocusRing(const Vector<FloatRect>& rects, float outlineWidth, const Color& color, float zoomFactor)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.drawFocusRing(rects, outlineWidth, color, zoomFactor);
    m_secondaryContext.drawFocusRing(rects, outlineWidth, color, zoomFactor);

    VERIFY_STATE_SYNCHRONIZATION();
}

FloatSize BifurcatedGraphicsContext::drawText(const FontCascade& cascade, const TextRun& run, const FloatPoint& point, unsigned from, std::optional<unsigned> to)
{
    updatePlatformContextStateIfNeeded();
    auto size = m_primaryContext.drawText(cascade, run, point, from, to);
    m_secondaryContext.drawText(cascade, run, point, from, to);
    
    VERIFY_STATE_SYNCHRONIZATION();

    return size;
}

void BifurcatedGraphicsContext::drawGlyphs(const Font& font, std::span<const GlyphBufferGlyph> glyphs, std::span<const GlyphBufferAdvance> advances, const FloatPoint& point, FontSmoothingMode fontSmoothingMode)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.drawGlyphs(font, glyphs, advances, point, fontSmoothingMode);
    m_secondaryContext.drawGlyphs(font, glyphs, advances, point, fontSmoothingMode);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::drawEmphasisMarks(const FontCascade& cascade, const TextRun& run, const AtomString& mark, const FloatPoint& point, unsigned from, std::optional<unsigned> to)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.drawEmphasisMarks(cascade, run, mark, point, from, to);
    m_secondaryContext.drawEmphasisMarks(cascade, run, mark, point, from, to);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::drawBidiText(const FontCascade& cascade, const TextRun& run, const FloatPoint& point, FontCascade::CustomFontNotReadyAction customFontNotReadyAction)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.drawBidiText(cascade, run, point, customFontNotReadyAction);
    m_secondaryContext.drawBidiText(cascade, run, point, customFontNotReadyAction);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::drawLinesForText(const FloatPoint& point, float thickness, std::span<const FloatSegment> lineSegments, bool printing, bool doubleLines, StrokeStyle strokeStyle)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.drawLinesForText(point, thickness, lineSegments, printing, doubleLines, strokeStyle);
    m_secondaryContext.drawLinesForText(point, thickness, lineSegments, printing, doubleLines, strokeStyle);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::drawDotsForDocumentMarker(const FloatRect& rect, DocumentMarkerLineStyle markerStyle)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.drawDotsForDocumentMarker(rect, markerStyle);
    m_secondaryContext.drawDotsForDocumentMarker(rect, markerStyle);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::beginPage(const FloatRect& pageRect)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.beginPage(pageRect);
    m_secondaryContext.beginPage(pageRect);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::endPage()
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.endPage();
    m_secondaryContext.endPage();

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::setURLForRect(const URL& url, const FloatRect& rect)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.setURLForRect(url, rect);
    m_secondaryContext.setURLForRect(url, rect);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::setDestinationForRect(const String& name, const FloatRect& rect)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.setDestinationForRect(name, rect);
    m_secondaryContext.setDestinationForRect(name, rect);

    VERIFY_STATE_SYNCHRONIZATION();
}

void BifurcatedGraphicsContext::addDestinationAtPoint(const String& name, const FloatPoint& point)
{
    updatePlatformContextStateIfNeeded();
    m_primaryContext.addDestinationAtPoint(name, point);
    m_secondaryContext.addDestinationAtPoint(name, point);

    VERIFY_STATE_SYNCHRONIZATION();
}

bool BifurcatedGraphicsContext::supportsInternalLinks() const
{
    return m_primaryContext.supportsInternalLinks();
}

void BifurcatedGraphicsContext::updatePlatformContextStateIfNeeded()
{
    if (!m_state.changes())
        return;

    // This calls mergeLastChanges() so that the changes are also applied to each context's
    // GraphicsContextState, so that code internal to the child contexts that reads from the
    // state gets the right values. The child contexts apply them to their platform contexts
    // when they need to.
    m_primaryContext.mergeLastChanges(m_state);
    m_secondaryContext.mergeLastChanges(m_state);
    m_state.didApplyChanges();

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

} // namespace WebCore

