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
#include "DisplayListRecorderImpl.h"

#include "DisplayList.h"
#include "DisplayListItems.h"
#include "Filter.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "Logging.h"
#include "MediaPlayer.h"
#include "NotImplemented.h"
#include "SourceImage.h"
#include <wtf/MathExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace DisplayList {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RecorderImpl);

RecorderImpl::RecorderImpl(const GraphicsContextState& state, const FloatRect& initialClip, const AffineTransform& initialCTM, const DestinationColorSpace& colorSpace, DrawGlyphsMode drawGlyphsMode)
    : Recorder(state, initialClip, initialCTM, colorSpace, drawGlyphsMode)
{
    LOG_WITH_STREAM(DisplayLists, stream << "\nRecording with clip " << initialClip);
}

RecorderImpl::~RecorderImpl()
{
    ASSERT(stateStack().size() == 1); // If this fires, it indicates mismatched save/restore.
}

Ref<const DisplayList> RecorderImpl::takeDisplayList()
{
    appendStateChangeItemIfNecessary();
    m_items.shrinkToFit();
    return DisplayList::create(WTFMove(m_items));
}

Ref<const DisplayList> RecorderImpl::copyDisplayList()
{
    appendStateChangeItemIfNecessary();
    return DisplayList::create(Vector(m_items));
}

void RecorderImpl::save(GraphicsContextState::Purpose purpose)
{
    updateStateForSave(purpose);
    m_items.append(Save());
}

void RecorderImpl::restore(GraphicsContextState::Purpose purpose)
{
    if (stateStack().size() <= 1) {
        LOG_ERROR("ERROR void RecorderImpl::restore() stack is empty");
        return;
    }

    if (!updateStateForRestore(purpose))
        return;
    m_items.append(Restore());
}

void RecorderImpl::translate(float x, float y)
{
    if (!updateStateForTranslate(x, y))
        return;
    m_items.append(Translate(x, y));
}

void RecorderImpl::rotate(float angle)
{
    if (!updateStateForRotate(angle))
        return;
    m_items.append(Rotate(angle));
}

void RecorderImpl::scale(const FloatSize& scale)
{
    if (!updateStateForScale(scale))
        return;
    m_items.append(Scale(scale));
}

void RecorderImpl::setCTM(const AffineTransform& transform)
{
    updateStateForSetCTM(transform);
    m_items.append(SetCTM(transform));
}

void RecorderImpl::concatCTM(const AffineTransform& transform)
{
    if (!updateStateForConcatCTM(transform))
        return;
    m_items.append(ConcatenateCTM(transform));
}

void RecorderImpl::setLineCap(LineCap lineCap)
{
    m_items.append(SetLineCap(lineCap));
}

void RecorderImpl::setLineDash(const DashArray& array, float dashOffset)
{
    m_items.append(SetLineDash(array, dashOffset));
}

void RecorderImpl::setLineJoin(LineJoin join)
{
    m_items.append(SetLineJoin(join));
}

void RecorderImpl::setMiterLimit(float limit)
{
    m_items.append(SetMiterLimit(limit));
}

void RecorderImpl::resetClip()
{
    updateStateForResetClip();
    m_items.append(ResetClip());
    clip(initialClip());
}

void RecorderImpl::clip(const FloatRect& clipRect)
{
    updateStateForClip(clipRect);
    m_items.append(Clip(clipRect));
}

void RecorderImpl::clipRoundedRect(const FloatRoundedRect& clipRect)
{
    updateStateForClipRoundedRect(clipRect);
    m_items.append(ClipRoundedRect(clipRect));
}

void RecorderImpl::clipOut(const FloatRect& clipRect)
{
    updateStateForClipOut(clipRect);
    m_items.append(ClipOut(clipRect));
}

void RecorderImpl::clipOutRoundedRect(const FloatRoundedRect& clipRect)
{
    updateStateForClipOutRoundedRect(clipRect);
    m_items.append(ClipOutRoundedRect(clipRect));
}

void RecorderImpl::clipToImageBuffer(ImageBuffer& imageBuffer, const FloatRect& destinationRect)
{
    updateStateForClipToImageBuffer(destinationRect);
    m_items.append(ClipToImageBuffer(imageBuffer, destinationRect));
}

void RecorderImpl::clipOut(const Path& path)
{
    updateStateForClipOut(path);
    m_items.append(ClipOutToPath(path));
}

void RecorderImpl::clipPath(const Path& path, WindRule rule)
{
    updateStateForClipPath(path);
    m_items.append(ClipPath(path, rule));
}

void RecorderImpl::drawFilteredImageBuffer(ImageBuffer* sourceImage, const FloatRect& sourceImageRect, Filter& filter, FilterResults&)
{
    appendStateChangeItemIfNecessary();
    m_items.append(DrawFilteredImageBuffer(sourceImage, sourceImageRect, filter));
}

void RecorderImpl::drawGlyphs(const Font& font, std::span<const GlyphBufferGlyph> glyphs, std::span<const GlyphBufferAdvance> advances, const FloatPoint& localAnchor, FontSmoothingMode smoothingMode)
{
    if (decomposeDrawGlyphsIfNeeded(font, glyphs, advances, localAnchor, smoothingMode))
        return;

#if USE(SKIA)
    if (drawGlyphsMode() == Recorder::DrawGlyphsMode::TextBlob) {
        m_items.append(DrawTextBlob(Ref { font }, Vector(glyphs), Vector(advances), localAnchor, smoothingMode));
        return;
    }
#endif

    drawGlyphsImmediate(font, glyphs, advances, localAnchor, smoothingMode);
}

void RecorderImpl::drawGlyphsImmediate(const Font& font, std::span<const GlyphBufferGlyph> glyphs, std::span<const GlyphBufferAdvance> advances, const FloatPoint& localAnchor, FontSmoothingMode smoothingMode)
{
    appendStateChangeItemIfNecessary();
    m_items.append(DrawGlyphs(Ref { font }, Vector(glyphs), Vector(advances), localAnchor, smoothingMode));
}

void RecorderImpl::drawDisplayList(const DisplayList& displayList, ControlFactory&)
{
    appendStateChangeItemIfNecessary();
    m_items.append(DrawDisplayList(Ref { displayList }));
}

void RecorderImpl::drawImageBuffer(ImageBuffer& imageBuffer, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    appendStateChangeItemIfNecessary();
    m_items.append(DrawImageBuffer(imageBuffer, destRect, srcRect, options));
}

void RecorderImpl::drawNativeImage(NativeImage& image, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    appendStateChangeItemIfNecessary();
    m_items.append(DrawNativeImage(image, destRect, srcRect, options));
}

void RecorderImpl::drawSystemImage(SystemImage& systemImage, const FloatRect& destinationRect)
{
    appendStateChangeItemIfNecessary();
    m_items.append(DrawSystemImage(systemImage, destinationRect));
}

void RecorderImpl::drawPattern(NativeImage& image, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    appendStateChangeItemIfNecessary();
    m_items.append(DrawPatternNativeImage(image, destRect, tileRect, patternTransform, phase, spacing, options));
}

void RecorderImpl::drawPattern(ImageBuffer& imageBuffer, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    appendStateChangeItemIfNecessary();
    m_items.append(DrawPatternImageBuffer(imageBuffer, destRect, tileRect, patternTransform, phase, spacing, options));
}

void RecorderImpl::beginTransparencyLayer(float opacity)
{
    updateStateForBeginTransparencyLayer(opacity);
    m_items.append(BeginTransparencyLayer(opacity));
}

void RecorderImpl::beginTransparencyLayer(CompositeOperator compositeOperator, BlendMode blendMode)
{
    updateStateForBeginTransparencyLayer(compositeOperator, blendMode);
    m_items.append(BeginTransparencyLayerWithCompositeMode({ compositeOperator, blendMode }));
}

void RecorderImpl::endTransparencyLayer()
{
    if (updateStateForEndTransparencyLayer())
        m_items.append(EndTransparencyLayer());
}

void RecorderImpl::drawRect(const FloatRect& rect, float lineWidth)
{
    appendStateChangeItemIfNecessary();
    m_items.append(DrawRect(rect, lineWidth));
}

void RecorderImpl::drawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    appendStateChangeItemIfNecessary();
    m_items.append(DrawLine(point1, point2));
}

void RecorderImpl::drawLinesForText(const FloatPoint& point, float thickness, std::span<const FloatSegment> lineSegments, bool printing, bool doubleLines, StrokeStyle style)
{
    appendStateChangeItemIfNecessary();
    m_items.append(DrawLinesForText(point, lineSegments, thickness, printing, doubleLines, style));
}

void RecorderImpl::drawDotsForDocumentMarker(const FloatRect& rect, DocumentMarkerLineStyle style)
{
    appendStateChangeItemIfNecessary();
    m_items.append(DrawDotsForDocumentMarker(rect, style));
}

void RecorderImpl::drawEllipse(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary();
    m_items.append(DrawEllipse(rect));
}

void RecorderImpl::drawPath(const Path& path)
{
    appendStateChangeItemIfNecessary();
    m_items.append(DrawPath(path));
}

void RecorderImpl::drawFocusRing(const Path& path, float outlineWidth, const Color& color)
{
    appendStateChangeItemIfNecessary();
    m_items.append(DrawFocusRingPath(path, outlineWidth, color));
}

void RecorderImpl::drawFocusRing(const Vector<FloatRect>& rects, float outlineOffset, float outlineWidth, const Color& color)
{
    appendStateChangeItemIfNecessary();
    m_items.append(DrawFocusRingRects(rects, outlineOffset, outlineWidth, color));
}

void RecorderImpl::fillRect(const FloatRect& rect, RequiresClipToRect requiresClipToRect)
{
    appendStateChangeItemIfNecessary();
    m_items.append(FillRect(rect, requiresClipToRect));
}

void RecorderImpl::fillRect(const FloatRect& rect, const Color& color)
{
    appendStateChangeItemIfNecessary();
    m_items.append(FillRectWithColor(rect, color));
}

void RecorderImpl::fillRect(const FloatRect& rect, Gradient& gradient)
{
    appendStateChangeItemIfNecessary();
    m_items.append(FillRectWithGradient(rect, gradient));
}

void RecorderImpl::fillRect(const FloatRect& rect, Gradient& gradient, const AffineTransform& gradientSpaceTransform, RequiresClipToRect requiresClipToRect)
{
    appendStateChangeItemIfNecessary();
    m_items.append(FillRectWithGradientAndSpaceTransform(rect, gradient, gradientSpaceTransform, requiresClipToRect));
}

void RecorderImpl::fillRect(const FloatRect& rect, const Color& color, CompositeOperator op, BlendMode mode)
{
    appendStateChangeItemIfNecessary();
    m_items.append(FillCompositedRect(rect, color, op, mode));
}

void RecorderImpl::fillRoundedRect(const FloatRoundedRect& rect, const Color& color, BlendMode mode)
{
    appendStateChangeItemIfNecessary();
    m_items.append(FillRoundedRect(rect, color, mode));
}

void RecorderImpl::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedRect, const Color& color)
{
    appendStateChangeItemIfNecessary();
    m_items.append(FillRectWithRoundedHole(rect, roundedRect, color));
}

void RecorderImpl::fillPath(const Path& path)
{
    appendStateChangeItemIfNecessary();
    m_items.append(FillPath(path));
}

void RecorderImpl::fillEllipse(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary();
    m_items.append(FillEllipse(rect));
}

#if ENABLE(VIDEO)
void RecorderImpl::drawVideoFrame(const VideoFrame&, const FloatRect&, ImageOrientation, bool)
{
    appendStateChangeItemIfNecessary();
    // FIXME: TODO
}
#endif // ENABLE(VIDEO)

void RecorderImpl::strokeRect(const FloatRect& rect, float width)
{
    appendStateChangeItemIfNecessary();
    m_items.append(StrokeRect(rect, width));
}

void RecorderImpl::strokePath(const Path& path)
{
    appendStateChangeItemIfNecessary();
    m_items.append(StrokePath(path));
}

void RecorderImpl::strokeEllipse(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary();
    m_items.append(StrokeEllipse(rect));
}

void RecorderImpl::clearRect(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary();
    m_items.append(ClearRect(rect));
}

void RecorderImpl::drawControlPart(ControlPart& part, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    appendStateChangeItemIfNecessary();
    m_items.append(DrawControlPart(part, borderRect, deviceScaleFactor, style));
}

#if USE(CG)

void RecorderImpl::applyStrokePattern()
{
    appendStateChangeItemIfNecessary();
    m_items.append(ApplyStrokePattern());
}

void RecorderImpl::applyFillPattern()
{
    appendStateChangeItemIfNecessary();
    m_items.append(ApplyFillPattern());
}

#endif // USE(CG)

void RecorderImpl::applyDeviceScaleFactor(float scaleFactor)
{
    updateStateForApplyDeviceScaleFactor(scaleFactor);
    m_items.append(ApplyDeviceScaleFactor(scaleFactor));
}

void RecorderImpl::beginPage(const FloatRect& pageRect)
{
    appendStateChangeItemIfNecessary();
    m_items.append(BeginPage({ pageRect }));
}

void RecorderImpl::endPage()
{
    appendStateChangeItemIfNecessary();
    m_items.append(EndPage());
}

void RecorderImpl::setURLForRect(const URL& link, const FloatRect& destRect)
{
    appendStateChangeItemIfNecessary();
    m_items.append(SetURLForRect(link, destRect));
}

void RecorderImpl::appendStateChangeItemIfNecessary()
{
    auto& state = currentState().state;
    auto changes = state.changes();
    if (!changes)
        return;

    auto recordFullItem = [&] {
        m_items.append(SetState(state));
        state.didApplyChanges();
        currentState().lastDrawingState = state;
    };

    if (!changes.containsOnly({ GraphicsContextState::Change::FillBrush, GraphicsContextState::Change::StrokeBrush, GraphicsContextState::Change::StrokeThickness })) {
        recordFullItem();
        return;
    }
    std::optional<PackedColor::RGBA> fillColor;
    if (changes.contains(GraphicsContextState::Change::FillBrush)) {
        fillColor = state.fillBrush().packedColor();
        if (!fillColor) {
            recordFullItem();
            return;
        }
    }
    std::optional<PackedColor::RGBA> strokeColor;
    if (changes.contains(GraphicsContextState::Change::StrokeBrush)) {
        strokeColor = state.strokeBrush().packedColor();
        if (!strokeColor) {
            recordFullItem();
            return;
        }
    }
    std::optional<float> strokeThickness;
    if (changes.contains(GraphicsContextState::Change::StrokeThickness))
        strokeThickness = state.strokeThickness();

    if (fillColor)
        m_items.append(SetInlineFillColor(*fillColor));
    if (strokeColor || strokeThickness)
        m_items.append(SetInlineStroke(strokeColor, strokeThickness));

    state.didApplyChanges();
    currentState().lastDrawingState = state;
}

void RecorderImpl::drawPlaceholder(Function<void(GraphicsContext&)>&& function)
{
    m_items.append(DrawPlaceholder(WTFMove(function)));
}

} // namespace DisplayList
} // namespace WebCore
