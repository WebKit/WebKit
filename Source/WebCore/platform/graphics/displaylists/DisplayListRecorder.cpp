/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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
#include "DisplayListRecorder.h"

#include "DisplayList.h"
#include "DisplayListDrawingContext.h"
#include "DisplayListItems.h"
#include "FEImage.h"
#include "Filter.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "Logging.h"
#include "MediaPlayer.h"
#include "NotImplemented.h"
#include <wtf/MathExtras.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace DisplayList {

Recorder::Recorder(const GraphicsContextState& state, const FloatRect& initialClip, const AffineTransform& initialCTM, DrawGlyphsRecorder::DeconstructDrawGlyphs deconstructDrawGlyphs)
    : m_drawGlyphsRecorder(*this, deconstructDrawGlyphs)
{
    m_stateStack.append({ state, initialCTM, initialCTM.mapRect(initialClip) });
}

Recorder::Recorder(Recorder& parent, const GraphicsContextState& state, const FloatRect& initialClip, const AffineTransform& initialCTM)
    : m_drawGlyphsRecorder(*this, parent.m_drawGlyphsRecorder.deconstructDrawGlyphs())
{
    m_stateStack.append({ state, initialCTM, initialCTM.mapRect(initialClip) });
}

Recorder::~Recorder()
{
    ASSERT(m_stateStack.size() == 1); // If this fires, it indicates mismatched save/restore.
}

static bool containsOnlyInlineStateChanges(const GraphicsContextStateChange& changes, GraphicsContextState::StateChangeFlags changeFlags)
{
    static constexpr GraphicsContextState::StateChangeFlags inlineStateChangeFlags {
        GraphicsContextState::StrokeThicknessChange,
        GraphicsContextState::StrokeColorChange,
        GraphicsContextState::FillColorChange,
    };

    if (changeFlags != (changeFlags & inlineStateChangeFlags))
        return false;

    if (changeFlags.contains(GraphicsContextState::StrokeColorChange) && !changes.m_state.strokeColor.tryGetAsSRGBABytes())
        return false;

    if (changeFlags.contains(GraphicsContextState::FillColorChange) && !changes.m_state.fillColor.tryGetAsSRGBABytes())
        return false;

    return true;
}

void Recorder::appendStateChangeItem(const GraphicsContextStateChange& changes, GraphicsContextState::StateChangeFlags changeFlags)
{
    if (!containsOnlyInlineStateChanges(changes, changeFlags)) {
        if (auto pattern = changes.m_state.strokePattern)
            recordResourceUse(pattern->tileImage());
        if (auto pattern = changes.m_state.fillPattern)
            recordResourceUse(pattern->tileImage());
        recordSetState(changes.m_state, changeFlags);
        return;
    }

    if (changeFlags.contains(GraphicsContextState::StrokeColorChange))
        recordSetInlineStrokeColor(*changes.m_state.strokeColor.tryGetAsSRGBABytes());

    if (changeFlags.contains(GraphicsContextState::StrokeThicknessChange))
        recordSetStrokeThickness(changes.m_state.strokeThickness);

    if (changeFlags.contains(GraphicsContextState::FillColorChange))
        recordSetInlineFillColor(*changes.m_state.fillColor.tryGetAsSRGBABytes());
}

void Recorder::appendStateChangeItemIfNecessary()
{
    // FIXME: This is currently invoked in an ad-hoc manner when recording drawing items. We should consider either
    // splitting GraphicsContext state changes into individual display list items, or refactoring the code such that
    // this method is automatically invoked when recording a drawing item.
    auto& stateChanges = currentState().stateChange;
    auto changesFromLastState = stateChanges.changesFromState(currentState().lastDrawingState);
    if (!changesFromLastState)
        return;

    LOG_WITH_STREAM(DisplayLists, stream << "pre-drawing, saving state " << GraphicsContextStateChange(stateChanges.m_state, changesFromLastState));
    appendStateChangeItem(stateChanges, changesFromLastState);
    stateChanges.m_changeFlags = { };
    currentState().lastDrawingState = stateChanges.m_state;
}

const GraphicsContextState& Recorder::state() const
{
    return currentState().stateChange.m_state;
}

void Recorder::didUpdateState(const GraphicsContextState& state, GraphicsContextState::StateChangeFlags flags)
{
    currentState().stateChange.accumulate(state, flags);
}

void Recorder::setLineCap(LineCap lineCap)
{
    recordSetLineCap(lineCap);
}

void Recorder::setLineDash(const DashArray& dashArray, float dashOffset)
{
    recordSetLineDash(dashArray, dashOffset);
}

void Recorder::setLineJoin(LineJoin lineJoin)
{
    recordSetLineJoin(lineJoin);
}

void Recorder::setMiterLimit(float miterLimit)
{
    recordSetMiterLimit(miterLimit);
}

void Recorder::drawFilteredImageBuffer(ImageBuffer* sourceImage, const FloatRect& sourceImageRect, Filter& filter, FilterResults& results)
{
    appendStateChangeItemIfNecessary();

    for (auto& effect : filter.effectsOfType(FilterEffect::Type::FEImage)) {
        auto& feImage = downcast<FEImage>(effect.get());
        if (!recordResourceUse(feImage.sourceImage())) {
            GraphicsContext::drawFilteredImageBuffer(sourceImage, sourceImageRect, filter, results);
            return;
        }
    }

    if (!sourceImage) {
        recordDrawFilteredImageBuffer(nullptr, sourceImageRect, filter);
        return;
    }

    if (!recordResourceUse(*sourceImage)) {
        GraphicsContext::drawFilteredImageBuffer(sourceImage, sourceImageRect, filter, results);
        return;
    }

    recordDrawFilteredImageBuffer(sourceImage, sourceImageRect, filter);
}

void Recorder::drawGlyphs(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned numGlyphs, const FloatPoint& startPoint, FontSmoothingMode smoothingMode)
{
    m_drawGlyphsRecorder.drawGlyphs(font, glyphs, advances, numGlyphs, startPoint, smoothingMode);
}

void Recorder::drawGlyphsAndCacheFont(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned count, const FloatPoint& localAnchor, FontSmoothingMode smoothingMode)
{
    appendStateChangeItemIfNecessary();
    recordResourceUse(const_cast<Font&>(font));
    recordDrawGlyphs(font, glyphs, advances, count, localAnchor, smoothingMode);
}

void Recorder::drawImageBuffer(ImageBuffer& imageBuffer, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    appendStateChangeItemIfNecessary();

    if (!recordResourceUse(imageBuffer)) {
        GraphicsContext::drawImageBuffer(imageBuffer, destRect, srcRect, options);
        return;
    }

    recordDrawImageBuffer(imageBuffer, destRect, srcRect, options);
}

void Recorder::drawNativeImage(NativeImage& image, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    appendStateChangeItemIfNecessary();
    recordResourceUse(image);
    recordDrawNativeImage(image.renderingResourceIdentifier(), imageSize, destRect, srcRect, options);
}

void Recorder::drawPattern(NativeImage& image, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    appendStateChangeItemIfNecessary();
    recordResourceUse(image);
    recordDrawPattern(image.renderingResourceIdentifier(), destRect, tileRect, patternTransform, phase, spacing, options);
}

void Recorder::drawPattern(ImageBuffer& imageBuffer, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    appendStateChangeItemIfNecessary();

    if (!recordResourceUse(imageBuffer)) {
        GraphicsContext::drawPattern(imageBuffer, destRect, tileRect, patternTransform, phase, spacing, options);
        return;
    }

    recordDrawPattern(imageBuffer.renderingResourceIdentifier(), destRect, tileRect, patternTransform, phase, spacing, options);
}

void Recorder::save()
{
    recordSave();
    m_stateStack.append(m_stateStack.last().cloneForSave());
}

void Recorder::restore()
{
    if (!m_stateStack.size())
        return;

    m_stateStack.removeLast();
    recordRestore();
}

void Recorder::translate(float x, float y)
{
    currentState().translate(x, y);
    recordTranslate(x, y);
}

void Recorder::rotate(float angleInRadians)
{
    currentState().rotate(angleInRadians);
    recordRotate(angleInRadians);
}

void Recorder::scale(const FloatSize& size)
{
    currentState().scale(size);
    recordScale(size);
}

void Recorder::concatCTM(const AffineTransform& transform)
{
    if (transform.isIdentity())
        return;

    currentState().concatCTM(transform);
    recordConcatenateCTM(transform);
}

void Recorder::setCTM(const AffineTransform& transform)
{
    currentState().setCTM(transform);
    recordSetCTM(transform);
}

AffineTransform Recorder::getCTM(GraphicsContext::IncludeDeviceScale) const
{
    return currentState().ctm;
}

void Recorder::beginTransparencyLayer(float opacity)
{
    appendStateChangeItemIfNecessary();
    recordBeginTransparencyLayer(opacity);
    m_stateStack.append(m_stateStack.last().cloneForTransparencyLayer());
}

void Recorder::endTransparencyLayer()
{
    appendStateChangeItemIfNecessary();
    recordEndTransparencyLayer();
    m_stateStack.removeLast();
}

void Recorder::drawRect(const FloatRect& rect, float borderThickness)
{
    appendStateChangeItemIfNecessary();
    recordDrawRect(rect, borderThickness);
}

void Recorder::drawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    appendStateChangeItemIfNecessary();
    recordDrawLine(point1, point2);
}

void Recorder::drawLinesForText(const FloatPoint& point, float thickness, const DashArray& widths, bool printing, bool doubleLines, StrokeStyle)
{
    appendStateChangeItemIfNecessary();
    recordDrawLinesForText(FloatPoint(), toFloatSize(point), thickness, widths, printing, doubleLines);
}

void Recorder::drawDotsForDocumentMarker(const FloatRect& rect, DocumentMarkerLineStyle style)
{
    appendStateChangeItemIfNecessary();
    recordDrawDotsForDocumentMarker(rect, style);
}

void Recorder::drawEllipse(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary();
    recordDrawEllipse(rect);
}

void Recorder::drawPath(const Path& path)
{
    appendStateChangeItemIfNecessary();
    recordDrawPath(path);
}

void Recorder::drawFocusRing(const Path& path, float width, float offset, const Color& color)
{
    appendStateChangeItemIfNecessary();
    recordDrawFocusRingPath(path, width, offset, color);
}

void Recorder::drawFocusRing(const Vector<FloatRect>& rects, float width, float offset, const Color& color)
{
    appendStateChangeItemIfNecessary();
    recordDrawFocusRingRects(rects, width, offset, color);
}

#if PLATFORM(MAC)
void Recorder::drawFocusRing(const Path&, double, bool&, const Color&)
{
    notImplemented();
}

void Recorder::drawFocusRing(const Vector<FloatRect>&, double, bool&, const Color&)
{
    notImplemented();
}
#endif

void Recorder::fillRect(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary();
    recordFillRect(rect);
}

void Recorder::fillRect(const FloatRect& rect, const Color& color)
{
    appendStateChangeItemIfNecessary();
    recordFillRectWithColor(rect, color);
}

void Recorder::fillRect(const FloatRect& rect, Gradient& gradient)
{
    appendStateChangeItemIfNecessary();
    recordFillRectWithGradient(rect, gradient);
}

void Recorder::fillRect(const FloatRect& rect, const Color& color, CompositeOperator op, BlendMode blendMode)
{
    appendStateChangeItemIfNecessary();
    recordFillCompositedRect(rect, color, op, blendMode);
}

void Recorder::fillRoundedRect(const FloatRoundedRect& rect, const Color& color, BlendMode blendMode)
{
    appendStateChangeItemIfNecessary();
    recordFillRoundedRect(rect, color, blendMode);
}

void Recorder::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    appendStateChangeItemIfNecessary();
    recordFillRectWithRoundedHole(rect, roundedHoleRect, color);
}

void Recorder::fillPath(const Path& path)
{
    appendStateChangeItemIfNecessary();
#if ENABLE(INLINE_PATH_DATA)
    if (path.hasInlineData()) {
        if (path.hasInlineData<LineData>())
            recordFillLine(path.inlineData<LineData>());
        else if (path.hasInlineData<ArcData>())
            recordFillArc(path.inlineData<ArcData>());
        else if (path.hasInlineData<QuadCurveData>())
            recordFillQuadCurve(path.inlineData<QuadCurveData>());
        else if (path.hasInlineData<BezierCurveData>())
            recordFillBezierCurve(path.inlineData<BezierCurveData>());
        return;
    }
#endif
    recordFillPath(path);
}

void Recorder::fillEllipse(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary();
    recordFillEllipse(rect);
}

void Recorder::strokeRect(const FloatRect& rect, float lineWidth)
{
    appendStateChangeItemIfNecessary();
    recordStrokeRect(rect, lineWidth);
}

void Recorder::strokePath(const Path& path)
{
    appendStateChangeItemIfNecessary();
#if ENABLE(INLINE_PATH_DATA)
    if (path.hasInlineData()) {
        if (path.hasInlineData<LineData>())
            recordStrokeLine(path.inlineData<LineData>());
        else if (path.hasInlineData<ArcData>())
            recordStrokeArc(path.inlineData<ArcData>());
        else if (path.hasInlineData<QuadCurveData>())
            recordStrokeQuadCurve(path.inlineData<QuadCurveData>());
        else if (path.hasInlineData<BezierCurveData>())
            recordStrokeBezierCurve(path.inlineData<BezierCurveData>());
        return;
    }
#endif
    recordStrokePath(path);
}

void Recorder::strokeEllipse(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary();
    recordStrokeEllipse(rect);
}

void Recorder::clearRect(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary();
    recordClearRect(rect);
}

#if USE(CG)
void Recorder::applyStrokePattern()
{
    appendStateChangeItemIfNecessary();
    recordApplyStrokePattern();
}

void Recorder::applyFillPattern()
{
    appendStateChangeItemIfNecessary();
    recordApplyFillPattern();
}
#endif

void Recorder::clip(const FloatRect& rect)
{
    currentState().clipBounds.intersect(currentState().ctm.mapRect(rect));
    recordClip(rect);
}

void Recorder::clipOut(const FloatRect& rect)
{
    recordClipOut(rect);
}

void Recorder::clipOut(const Path& path)
{
    recordClipOutToPath(path);
}

void Recorder::clipPath(const Path& path, WindRule windRule)
{
    currentState().clipBounds.intersect(currentState().ctm.mapRect(path.fastBoundingRect()));
    recordClipPath(path, windRule);
}

IntRect Recorder::clipBounds() const
{
    if (auto inverse = currentState().ctm.inverse())
        return enclosingIntRect(inverse->mapRect(currentState().clipBounds));

    // If the CTM is not invertible, return the original rect.
    // This matches CGRectApplyInverseAffineTransform behavior.
    return enclosingIntRect(currentState().clipBounds);
}

void Recorder::clipToImageBuffer(ImageBuffer& imageBuffer, const FloatRect& destRect)
{
    recordResourceUse(imageBuffer);
    recordClipToImageBuffer(imageBuffer, destRect);
}

RefPtr<ImageBuffer> Recorder::createImageBuffer(const FloatSize& size, const DestinationColorSpace& colorSpace, RenderingMode renderingMode, RenderingMethod renderingMethod) const
{
    if (renderingMethod == RenderingMethod::Default)
        renderingMethod = RenderingMethod::DisplayList;

    return GraphicsContext::createImageBuffer(size, colorSpace, renderingMode, renderingMethod);
}

#if ENABLE(VIDEO)
void Recorder::paintFrameForMedia(MediaPlayer& player, const FloatRect& destination)
{
    if (!player.identifier()) {
        GraphicsContext::paintFrameForMedia(player, destination);
        return;
    }
    ASSERT(player.identifier());
    recordPaintFrameForMedia(player, destination);
}
#endif

void Recorder::applyDeviceScaleFactor(float deviceScaleFactor)
{
    // We modify the state directly here instead of calling GraphicsContext::scale()
    // because the recorded item will scale() when replayed.
    currentState().scale({ deviceScaleFactor, deviceScaleFactor });

    // FIXME: this changes the baseCTM, which will invalidate all of our cached extents.
    // Assert that it's only called early on?
    recordApplyDeviceScaleFactor(deviceScaleFactor);
}

FloatRect Recorder::roundToDevicePixels(const FloatRect& rect, GraphicsContext::RoundingMode)
{
    WTFLogAlways("GraphicsContext::roundToDevicePixels() is not yet compatible with DisplayList::Recorder.");
    return rect;
}

const Recorder::ContextState& Recorder::currentState() const
{
    ASSERT(m_stateStack.size());
    return m_stateStack.last();
}

Recorder::ContextState& Recorder::currentState()
{
    ASSERT(m_stateStack.size());
    return m_stateStack.last();
}

const AffineTransform& Recorder::ctm() const
{
    return currentState().ctm;
}

void Recorder::ContextState::translate(float x, float y)
{
    ctm.translate(x, y);
}

void Recorder::ContextState::rotate(float angleInRadians)
{
    double angleInDegrees = rad2deg(static_cast<double>(angleInRadians));
    ctm.rotate(angleInDegrees);
    
    AffineTransform rotation;
    rotation.rotate(angleInDegrees);
}

void Recorder::ContextState::scale(const FloatSize& size)
{
    ctm.scale(size);
}

void Recorder::ContextState::setCTM(const AffineTransform& matrix)
{
    ctm = matrix;
}

void Recorder::ContextState::concatCTM(const AffineTransform& matrix)
{
    ctm *= matrix;
}

} // namespace DisplayList
} // namespace WebCore
