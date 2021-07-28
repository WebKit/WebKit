/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "Logging.h"
#include "MediaPlayer.h"
#include "NotImplemented.h"
#include <wtf/MathExtras.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace DisplayList {

Recorder::Recorder(DisplayList& displayList, const GraphicsContextState& state, const FloatRect& initialClip, const AffineTransform& initialCTM, Delegate* delegate, DrawGlyphsRecorder::DrawGlyphsDeconstruction drawGlyphsDeconstruction)
    : m_displayList(displayList)
    , m_delegate(delegate)
    , m_isNested(false)
    , m_drawGlyphsRecorder(*this, drawGlyphsDeconstruction)
{
    LOG_WITH_STREAM(DisplayLists, stream << "\nRecording with clip " << initialClip);
    m_stateStack.append({ state, initialCTM, initialClip });
}

Recorder::Recorder(Recorder& parent, const GraphicsContextState& state, const FloatRect& initialClip, const AffineTransform& initialCTM)
    : m_displayList(parent.m_displayList)
    , m_delegate(parent.m_delegate)
    , m_isNested(true)
    , m_drawGlyphsRecorder(*this, parent.m_drawGlyphsRecorder.drawGlyphsDeconstruction())
{
    m_stateStack.append({ state, initialCTM, initialClip });
}

Recorder::~Recorder()
{
    ASSERT(m_stateStack.size() == 1); // If this fires, it indicates mismatched save/restore.
    if (!m_isNested)
        LOG(DisplayLists, "Recorded display list:\n%s", m_displayList.description().data());
}

void Recorder::getPixelBuffer(const PixelBufferFormat& outputFormat, const IntRect& sourceRect)
{
    append<GetPixelBuffer>(outputFormat, sourceRect);
}

void Recorder::putPixelBuffer(const PixelBuffer& pixelBuffer, const IntRect& srcRect, const IntPoint& destPoint, AlphaPremultiplication destFormat)
{
    append<PutPixelBuffer>(pixelBuffer, srcRect, destPoint, destFormat);
}

static bool containsOnlyInlineStateChanges(const GraphicsContextStateChange& changes, GraphicsContextState::StateChangeFlags changeFlags)
{
    static constexpr GraphicsContextState::StateChangeFlags inlineStateChangeFlags {
        GraphicsContextState::StrokeThicknessChange,
        GraphicsContextState::StrokeColorChange,
        GraphicsContextState::FillColorChange,
        GraphicsContextState::FillGradientChange,
    };

    if (changeFlags != (changeFlags & inlineStateChangeFlags))
        return false;

    if (changeFlags.contains(GraphicsContextState::StrokeColorChange) && !changes.m_state.strokeColor.tryGetAsSRGBABytes())
        return false;

    if (changeFlags.contains(GraphicsContextState::FillColorChange) && !changes.m_state.fillColor.tryGetAsSRGBABytes())
        return false;

    if (changeFlags.contains(GraphicsContextState::FillGradientChange)
        && (!changes.m_state.fillGradient || !SetInlineFillGradient::isInline(*changes.m_state.fillGradient)))
        return false;

    return true;
}

void Recorder::recordNativeImageUse(NativeImage& image)
{
    if (m_delegate)
        m_delegate->recordNativeImageUse(image);
    m_displayList.cacheNativeImage(image);
}

void Recorder::appendStateChangeItem(const GraphicsContextStateChange& changes, GraphicsContextState::StateChangeFlags changeFlags)
{
    if (!containsOnlyInlineStateChanges(changes, changeFlags)) {
        if (auto pattern = changes.m_state.strokePattern)
            recordNativeImageUse(pattern->tileImage());
        if (auto pattern = changes.m_state.fillPattern)
            recordNativeImageUse(pattern->tileImage());
        append<SetState>(changes.m_state, changeFlags);
        return;
    }

    if (changeFlags.contains(GraphicsContextState::StrokeColorChange))
        append<SetInlineStrokeColor>(*changes.m_state.strokeColor.tryGetAsSRGBABytes());

    if (changeFlags.contains(GraphicsContextState::StrokeThicknessChange))
        append<SetStrokeThickness>(changes.m_state.strokeThickness);

    if (changeFlags.contains(GraphicsContextState::FillColorChange))
        append<SetInlineFillColor>(*changes.m_state.fillColor.tryGetAsSRGBABytes());

    if (changeFlags.contains(GraphicsContextState::FillGradientChange))
        append<SetInlineFillGradient>(*changes.m_state.fillGradient, changes.m_state.fillGradientSpaceTransform);
}

bool Recorder::canAppendItemOfType(ItemType type) const
{
    return !m_delegate || m_delegate->canAppendItemOfType(type);
}

void Recorder::appendStateChangeItemIfNecessary()
{
    auto& stateChanges = currentState().stateChange;
    auto changesFromLastState = stateChanges.changesFromState(currentState().lastDrawingState);
    if (!changesFromLastState)
        return;

    LOG_WITH_STREAM(DisplayLists, stream << "pre-drawing, saving state " << GraphicsContextStateChange(stateChanges.m_state, changesFromLastState));
    appendStateChangeItem(stateChanges, changesFromLastState);
    stateChanges.m_changeFlags = { };
    currentState().lastDrawingState = stateChanges.m_state;
}

void Recorder::updateState(const GraphicsContextState& state, GraphicsContextState::StateChangeFlags flags)
{
    currentState().stateChange.accumulate(state, flags);
}

bool Recorder::canDrawImageBuffer(const ImageBuffer& imageBuffer) const
{
    return !m_delegate || m_delegate->isCachedImageBuffer(imageBuffer);
}

RenderingMode Recorder::renderingMode() const
{
    return m_delegate ? m_delegate->renderingMode() : RenderingMode::Unaccelerated;
}

void Recorder::setLineCap(LineCap lineCap)
{
    append<SetLineCap>(lineCap);
}

void Recorder::setLineDash(const DashArray& dashArray, float dashOffset)
{
    append<SetLineDash>(dashArray, dashOffset);
}

void Recorder::setLineJoin(LineJoin lineJoin)
{
    append<SetLineJoin>(lineJoin);
}

void Recorder::setMiterLimit(float miterLimit)
{
    append<SetMiterLimit>(miterLimit);
}

void Recorder::drawGlyphs(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned numGlyphs, const FloatPoint& startPoint, FontSmoothingMode smoothingMode)
{
    m_drawGlyphsRecorder.drawGlyphs(font, glyphs, advances, numGlyphs, startPoint, smoothingMode);
}

void Recorder::appendDrawGlyphsItemWithCachedFont(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned count, const FloatPoint& localAnchor, FontSmoothingMode smoothingMode)
{
    if (m_delegate)
        m_delegate->recordFontUse(const_cast<Font&>(font));
    m_displayList.cacheFont(const_cast<Font&>(font));
    append<DrawGlyphs>(font, glyphs, advances, count, localAnchor, smoothingMode);
}

void Recorder::drawImageBuffer(ImageBuffer& imageBuffer, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    if (!canDrawImageBuffer(imageBuffer)) {
        GraphicsContext::drawImageBuffer(imageBuffer, destRect, srcRect, options);
        return;
    }
    if (m_delegate)
        m_delegate->recordImageBufferUse(imageBuffer);
    m_displayList.cacheImageBuffer(imageBuffer);
    append<DrawImageBuffer>(imageBuffer.renderingResourceIdentifier(), destRect, srcRect, options);
}

void Recorder::drawNativeImage(NativeImage& image, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    recordNativeImageUse(image);
    append<DrawNativeImage>(image.renderingResourceIdentifier(), imageSize, destRect, srcRect, options);
}

void Recorder::drawPattern(NativeImage& image, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    recordNativeImageUse(image);
    append<DrawPattern>(image.renderingResourceIdentifier(), imageSize, destRect, tileRect, patternTransform, phase, spacing, options);
}

void Recorder::save()
{
    append<Save>();
    m_stateStack.append(m_stateStack.last().cloneForSave());
}

void Recorder::restore()
{
    if (!m_stateStack.size())
        return;

    m_stateStack.removeLast();
    append<Restore>();
}

void Recorder::translate(float x, float y)
{
    currentState().translate(x, y);
    append<Translate>(x, y);
}

void Recorder::rotate(float angleInRadians)
{
    currentState().rotate(angleInRadians);
    append<Rotate>(angleInRadians);
}

void Recorder::scale(const FloatSize& size)
{
    currentState().scale(size);
    append<Scale>(size);
}

void Recorder::concatCTM(const AffineTransform& transform)
{
    if (transform.isIdentity())
        return;

    currentState().concatCTM(transform);
    append<ConcatenateCTM>(transform);
}

void Recorder::setCTM(const AffineTransform& transform)
{
    currentState().setCTM(transform);
    append<SetCTM>(transform);
}

AffineTransform Recorder::getCTM(GraphicsContext::IncludeDeviceScale) const
{
    // FIXME: Respect the given value of IncludeDeviceScale.
    return currentState().ctm;
}

void Recorder::beginTransparencyLayer(float opacity)
{
    append<BeginTransparencyLayer>(opacity);
    m_stateStack.append(m_stateStack.last().cloneForTransparencyLayer());
}

void Recorder::endTransparencyLayer()
{
    append<EndTransparencyLayer>();
    m_stateStack.removeLast();
}

void Recorder::drawRect(const FloatRect& rect, float borderThickness)
{
    append<DrawRect>(rect, borderThickness);
}

void Recorder::drawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    append<DrawLine>(point1, point2);
}

void Recorder::drawLinesForText(const FloatPoint& point, float thickness, const DashArray& widths, bool printing, bool doubleLines, StrokeStyle)
{
    append<DrawLinesForText>(FloatPoint(), toFloatSize(point), thickness, widths, printing, doubleLines);
}

void Recorder::drawDotsForDocumentMarker(const FloatRect& rect, DocumentMarkerLineStyle style)
{
    append<DrawDotsForDocumentMarker>(rect, style);
}

void Recorder::drawEllipse(const FloatRect& rect)
{
    append<DrawEllipse>(rect);
}

void Recorder::drawPath(const Path& path)
{
    append<DrawPath>(path);
}

void Recorder::drawFocusRing(const Path& path, float width, float offset, const Color& color)
{
    append<DrawFocusRingPath>(path, width, offset, color);
}

void Recorder::drawFocusRing(const Vector<FloatRect>& rects, float width, float offset, const Color& color)
{
    append<DrawFocusRingRects>(rects, width, offset, color);
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
    append<FillRect>(rect);
}

void Recorder::fillRect(const FloatRect& rect, const Color& color)
{
    append<FillRectWithColor>(rect, color);
}

void Recorder::fillRect(const FloatRect& rect, Gradient& gradient)
{
    append<FillRectWithGradient>(rect, gradient);
}

void Recorder::fillRect(const FloatRect& rect, const Color& color, CompositeOperator op, BlendMode blendMode)
{
    append<FillCompositedRect>(rect, color, op, blendMode);
}

void Recorder::fillRoundedRect(const FloatRoundedRect& rect, const Color& color, BlendMode blendMode)
{
    append<FillRoundedRect>(rect, color, blendMode);
}

void Recorder::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedHoleRect, const Color& color)
{
    append<FillRectWithRoundedHole>(rect, roundedHoleRect, color);
}

void Recorder::fillPath(const Path& path)
{
#if ENABLE(INLINE_PATH_DATA)
    if (path.hasInlineData()) {
        if (path.hasInlineData<LineData>())
            append<FillLine>(path.inlineData<LineData>());
        else if (path.hasInlineData<ArcData>())
            append<FillArc>(path.inlineData<ArcData>());
        else if (path.hasInlineData<QuadCurveData>())
            append<FillQuadCurve>(path.inlineData<QuadCurveData>());
        else if (path.hasInlineData<BezierCurveData>())
            append<FillBezierCurve>(path.inlineData<BezierCurveData>());
        return;
    }
#endif
    append<FillPath>(path);
}

void Recorder::fillEllipse(const FloatRect& rect)
{
    append<FillEllipse>(rect);
}

void Recorder::strokeRect(const FloatRect& rect, float lineWidth)
{
    append<StrokeRect>(rect, lineWidth);
}

void Recorder::strokePath(const Path& path)
{
#if ENABLE(INLINE_PATH_DATA)
    if (path.hasInlineData()) {
        if (path.hasInlineData<LineData>())
            append<StrokeLine>(path.inlineData<LineData>());
        else if (path.hasInlineData<ArcData>())
            append<StrokeArc>(path.inlineData<ArcData>());
        else if (path.hasInlineData<QuadCurveData>())
            append<StrokeQuadCurve>(path.inlineData<QuadCurveData>());
        else if (path.hasInlineData<BezierCurveData>())
            append<StrokeBezierCurve>(path.inlineData<BezierCurveData>());
        return;
    }
#endif
    append<StrokePath>(path);
}

void Recorder::strokeEllipse(const FloatRect& rect)
{
    append<StrokeEllipse>(rect);
}

void Recorder::clearRect(const FloatRect& rect)
{
    append<ClearRect>(rect);
}

#if USE(CG)
void Recorder::applyStrokePattern()
{
    append<ApplyStrokePattern>();
}

void Recorder::applyFillPattern()
{
    append<ApplyFillPattern>();
}
#endif

void Recorder::clip(const FloatRect& rect)
{
    currentState().clipBounds.intersect(rect);
    append<Clip>(rect);
}

void Recorder::clipOut(const FloatRect& rect)
{
    append<ClipOut>(rect);
}

void Recorder::clipOut(const Path& path)
{
    append<ClipOutToPath>(path);
}

void Recorder::clipPath(const Path& path, WindRule windRule)
{
    currentState().clipBounds.intersect(path.fastBoundingRect());
    append<ClipPath>(path, windRule);
}

IntRect Recorder::clipBounds() const
{
    return enclosingIntRect(currentState().clipBounds);
}

void Recorder::clipToImageBuffer(ImageBuffer& imageBuffer, const FloatRect& destRect)
{
    if (m_delegate)
        m_delegate->recordImageBufferUse(imageBuffer);
    m_displayList.cacheImageBuffer(imageBuffer);
    append<ClipToImageBuffer>(imageBuffer.renderingResourceIdentifier(), destRect);
}

GraphicsContext::ClipToDrawingCommandsResult Recorder::clipToDrawingCommands(const FloatRect& destination, const DestinationColorSpace& colorSpace, Function<void(GraphicsContext&)>&& drawingFunction)
{
    auto initialClip = FloatRect(FloatPoint(), destination.size());

    // The initial CTM matches ImageBuffer's initial CTM.
    AffineTransform transform = getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);
    FloatSize scaleFactor(transform.xScale(), transform.yScale());
    auto scaledSize = expandedIntSize(destination.size() * scaleFactor);
    AffineTransform initialCTM;
    initialCTM.scale(1, -1);
    initialCTM.translate(0, -scaledSize.height());
    initialCTM.scale(scaledSize / destination.size());

    Recorder nestedContext(*this, GraphicsContextState(), initialClip, initialCTM);
    append<BeginClipToDrawingCommands>(destination, colorSpace);
    drawingFunction(nestedContext);
    append<EndClipToDrawingCommands>(destination);

    return ClipToDrawingCommandsResult::Success;
}

#if ENABLE(VIDEO)
void Recorder::paintFrameForMedia(MediaPlayer& player, const FloatRect& destination)
{
    if (!player.identifier()) {
        GraphicsContext::paintFrameForMedia(player, destination);
        return;
    }
    ASSERT(player.identifier());
    append<PaintFrameForMedia>(player, destination);
}
#endif

void Recorder::applyDeviceScaleFactor(float deviceScaleFactor)
{
    // FIXME: this changes the baseCTM, which will invalidate all of our cached extents.
    // Assert that it's only called early on?
    append<ApplyDeviceScaleFactor>(deviceScaleFactor);
}

FloatRect Recorder::roundToDevicePixels(const FloatRect& rect, GraphicsContext::RoundingMode)
{
    WTFLogAlways("GraphicsContext::roundToDevicePixels() is not yet compatible with DisplayList::Recorder.");
    return rect;
}

// FIXME: share with ShadowData
static inline float shadowPaintingExtent(float blurRadius)
{
    // Blurring uses a Gaussian function whose std. deviation is m_radius/2, and which in theory
    // extends to infinity. In 8-bit contexts, however, rounding causes the effect to become
    // undetectable at around 1.4x the radius.
    const float radiusExtentMultiplier = 1.4;
    return ceilf(blurRadius * radiusExtentMultiplier);
}

FloatRect Recorder::extentFromLocalBounds(const FloatRect& rect) const
{
    FloatRect bounds = rect;
    const ContextState& state = currentState();

    FloatSize shadowOffset;
    float shadowRadius;
    Color shadowColor;
    if (getShadow(shadowOffset, shadowRadius, shadowColor)) {
        FloatRect shadowExtent = bounds;
        shadowExtent.move(shadowOffset);
        shadowExtent.inflate(shadowPaintingExtent(shadowRadius));
        bounds.unite(shadowExtent);
    }
    
    FloatRect clippedExtent = intersection(state.clipBounds, bounds);
    return state.ctm.mapRect(clippedExtent);
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
    clipBounds.move(-x, -y);
}

void Recorder::ContextState::rotate(float angleInRadians)
{
    double angleInDegrees = rad2deg(static_cast<double>(angleInRadians));
    ctm.rotate(angleInDegrees);
    
    AffineTransform rotation;
    rotation.rotate(angleInDegrees);

    if (std::optional<AffineTransform> inverse = rotation.inverse())
        clipBounds = inverse.value().mapRect(clipBounds);
}

void Recorder::ContextState::scale(const FloatSize& size)
{
    ctm.scale(size);
    clipBounds.scale(1 / size.width(), 1 / size.height());
}

void Recorder::ContextState::setCTM(const AffineTransform& matrix)
{
    std::optional<AffineTransform> inverseTransformForClipBounds;
    if (auto originalCTMInverse = ctm.inverse())
        inverseTransformForClipBounds = originalCTMInverse->multiply(matrix).inverse();

    ctm = matrix;

    if (inverseTransformForClipBounds)
        clipBounds = inverseTransformForClipBounds->mapRect(clipBounds);
}

void Recorder::ContextState::concatCTM(const AffineTransform& matrix)
{
    ctm *= matrix;

    if (std::optional<AffineTransform> inverse = matrix.inverse())
        clipBounds = inverse.value().mapRect(clipBounds);
}

} // namespace DisplayList
} // namespace WebCore
