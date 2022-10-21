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

#if USE(SYSTEM_PREVIEW)
#include "ARKitBadgeSystemImage.h"
#endif

namespace WebCore {
namespace DisplayList {

Recorder::Recorder(const GraphicsContextState& state, const FloatRect& initialClip, const AffineTransform& initialCTM, DrawGlyphsMode drawGlyphsMode)
    : GraphicsContext(state)
    , m_initialScale(initialCTM.xScale())
    , m_drawGlyphsMode(drawGlyphsMode)
{
    ASSERT(!state.changes());
    m_stateStack.append({ state, initialCTM, initialCTM.mapRect(initialClip) });
}

Recorder::~Recorder()
{
    ASSERT(m_stateStack.size() == 1); // If this fires, it indicates mismatched save/restore.
}

void Recorder::appendStateChangeItem(const GraphicsContextState& state)
{
    ASSERT(state.changes());
    
    if (state.containsOnlyInlineChanges()) {
        if (state.changes().contains(GraphicsContextState::Change::FillBrush))
            recordSetInlineFillColor(*fillColor().tryGetAsSRGBABytes());

        if (state.changes().contains(GraphicsContextState::Change::StrokeBrush))
            recordSetInlineStrokeColor(*strokeColor().tryGetAsSRGBABytes());

        if (state.changes().contains(GraphicsContextState::Change::StrokeThickness))
            recordSetStrokeThickness(strokeThickness());

        return;
    }

    if (state.changes().contains(GraphicsContextState::Change::FillBrush)) {
        if (auto pattern = fillPattern())
            recordResourceUse(pattern->tileImage());
    }

    if (state.changes().contains(GraphicsContextState::Change::StrokeBrush)) {
        if (auto pattern = strokePattern())
            recordResourceUse(pattern->tileImage());
    }

    recordSetState(state);
}

void Recorder::appendStateChangeItemIfNecessary()
{
    // FIXME: This is currently invoked in an ad-hoc manner when recording drawing items. We should consider either
    // splitting GraphicsContext state changes into individual display list items, or refactoring the code such that
    // this method is automatically invoked when recording a drawing item.
    auto& state = currentState().state;
    if (!state.changes())
        return;

    LOG_WITH_STREAM(DisplayLists, stream << "pre-drawing, saving state " << state);
    appendStateChangeItem(state);
    state.didApplyChanges();
    currentState().lastDrawingState = state;
}

const GraphicsContextState& Recorder::state() const
{
    return currentState().state;
}

void Recorder::didUpdateState(GraphicsContextState& state)
{
    currentState().state.mergeLastChanges(state, currentState().lastDrawingState);
    state.didApplyChanges();
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

bool Recorder::shouldDeconstructDrawGlyphs() const
{
    switch (m_drawGlyphsMode) {
    case DrawGlyphsMode::Normal:
        return false;
    case DrawGlyphsMode::DeconstructUsingDrawGlyphsCommands:
    case DrawGlyphsMode::DeconstructUsingDrawDecomposedGlyphsCommands:
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void Recorder::drawGlyphs(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned numGlyphs, const FloatPoint& startPoint, FontSmoothingMode smoothingMode)
{
    if (shouldDeconstructDrawGlyphs()) {
        if (!m_drawGlyphsRecorder)
            m_drawGlyphsRecorder = makeUnique<DrawGlyphsRecorder>(*this, m_initialScale);
        m_drawGlyphsRecorder->drawGlyphs(font, glyphs, advances, numGlyphs, startPoint, smoothingMode);
        return;
    }

    drawGlyphsAndCacheResources(font, glyphs, advances, numGlyphs, startPoint, smoothingMode);
}

void Recorder::drawDecomposedGlyphs(const Font& font, const DecomposedGlyphs& decomposedGlyphs)
{
    appendStateChangeItemIfNecessary();
    recordResourceUse(const_cast<Font&>(font));
    recordResourceUse(const_cast<DecomposedGlyphs&>(decomposedGlyphs));
    recordDrawDecomposedGlyphs(font, decomposedGlyphs);
}

void Recorder::drawGlyphsAndCacheResources(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned numGlyphs, const FloatPoint& localAnchor, FontSmoothingMode smoothingMode)
{
    appendStateChangeItemIfNecessary();
    recordResourceUse(const_cast<Font&>(font));

    if (m_drawGlyphsMode == DrawGlyphsMode::DeconstructUsingDrawDecomposedGlyphsCommands) {
        auto decomposedGlyphs = DecomposedGlyphs::create(glyphs, advances, numGlyphs, localAnchor, smoothingMode);
        recordResourceUse(decomposedGlyphs.get());
        recordDrawDecomposedGlyphs(font, decomposedGlyphs.get());
        return;
    }

    recordDrawGlyphs(font, glyphs, advances, numGlyphs, localAnchor, smoothingMode);
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

void Recorder::drawSystemImage(SystemImage& systemImage, const FloatRect& destinationRect)
{
#if USE(SYSTEM_PREVIEW)
    if (is<ARKitBadgeSystemImage>(systemImage)) {
        if (auto image = downcast<ARKitBadgeSystemImage>(systemImage).image()) {
            auto nativeImage = image->nativeImage();
            if (!nativeImage)
                return;
            recordResourceUse(*nativeImage);
        }
    }
#endif
    recordDrawSystemImage(systemImage, destinationRect);
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
    GraphicsContext::save();
    recordSave();
    m_stateStack.append(m_stateStack.last());
}

void Recorder::restore()
{
    GraphicsContext::restore();

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
    GraphicsContext::beginTransparencyLayer(opacity);

    appendStateChangeItemIfNecessary();
    recordBeginTransparencyLayer(opacity);
    m_stateStack.append(m_stateStack.last().cloneForTransparencyLayer());
    
    m_state.didBeginTransparencyLayer();
}

void Recorder::endTransparencyLayer()
{
    GraphicsContext::endTransparencyLayer();

    appendStateChangeItemIfNecessary();
    recordEndTransparencyLayer();
    m_stateStack.removeLast();

    m_state.didEndTransparencyLayer(currentState().state.alpha());
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

void Recorder::drawLinesForText(const FloatPoint& point, float thickness, const DashArray& widths, bool printing, bool doubleLines, StrokeStyle style)
{
    appendStateChangeItemIfNecessary();
    recordDrawLinesForText(FloatPoint(), toFloatSize(point), thickness, widths, printing, doubleLines, style);
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
#if ENABLE(INLINE_PATH_DATA)
    auto& state = currentState().state;
    if (state.changes() && state.containsOnlyInlineChanges() && !state.changes().contains(GraphicsContextState::Change::FillBrush) && path.hasInlineData() && path.hasInlineData<LineData>()) {
        recordStrokeLineWithColorAndThickness(*strokeColor().tryGetAsSRGBABytes(), strokeThickness(), path.inlineData<LineData>());
        state.didApplyChanges();
        currentState().lastDrawingState = state;
        return;
    }

    appendStateChangeItemIfNecessary();

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
#else
    appendStateChangeItemIfNecessary();
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

RefPtr<ImageBuffer> Recorder::createImageBuffer(const FloatSize& size, float resolutionScale, const DestinationColorSpace& colorSpace, std::optional<RenderingMode> renderingMode, std::optional<RenderingMethod> renderingMethod) const
{
    return GraphicsContext::createImageBuffer(size, resolutionScale, colorSpace, renderingMode, renderingMethod.value_or(RenderingMethod::DisplayList));
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

#if ENABLE(WEB_CODECS)
void Recorder::paintVideoFrame(VideoFrame& frame, const FloatRect& destination, bool shouldDiscardAlpha)
{
    if (!frame.isRemoteProxy()) {
        GraphicsContext::paintVideoFrame(frame, destination, shouldDiscardAlpha);
        return;
    }
    // FIXME: Implement recording.
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
