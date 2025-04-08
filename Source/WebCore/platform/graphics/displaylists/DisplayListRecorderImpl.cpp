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
#include "DisplayListDrawingContext.h"
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

RecorderImpl::RecorderImpl(DisplayList& displayList, const GraphicsContextState& state, const FloatRect& initialClip, const AffineTransform& initialCTM, const DestinationColorSpace& colorSpace, DrawGlyphsMode drawGlyphsMode)
    : Recorder(state, initialClip, initialCTM, colorSpace, drawGlyphsMode)
    , m_displayList(displayList)
{
    LOG_WITH_STREAM(DisplayLists, stream << "\nRecording with clip " << initialClip);
}

RecorderImpl::~RecorderImpl()
{
    ASSERT(stateStack().size() == 1); // If this fires, it indicates mismatched save/restore.
}

void RecorderImpl::save(GraphicsContextState::Purpose purpose)
{
    updateStateForSave(purpose);
    append(Save());
}

void RecorderImpl::restore(GraphicsContextState::Purpose purpose)
{
    if (!updateStateForRestore(purpose))
        return;
    append(Restore());
}

void RecorderImpl::translate(float x, float y)
{
    if (!updateStateForTranslate(x, y))
        return;
    append(Translate(x, y));
}

void RecorderImpl::rotate(float angle)
{
    if (!updateStateForRotate(angle))
        return;
    append(Rotate(angle));
}

void RecorderImpl::scale(const FloatSize& scale)
{
    if (!updateStateForScale(scale))
        return;
    append(Scale(scale));
}

void RecorderImpl::setCTM(const AffineTransform& transform)
{
    updateStateForSetCTM(transform);
    append(SetCTM(transform));
}

void RecorderImpl::concatCTM(const AffineTransform& transform)
{
    if (!updateStateForConcatCTM(transform))
        return;
    append(ConcatenateCTM(transform));
}

void RecorderImpl::setLineCap(LineCap lineCap)
{
    append(SetLineCap(lineCap));
}

void RecorderImpl::setLineDash(const DashArray& array, float dashOffset)
{
    append(SetLineDash(array, dashOffset));
}

void RecorderImpl::setLineJoin(LineJoin join)
{
    append(SetLineJoin(join));
}

void RecorderImpl::setMiterLimit(float limit)
{
    append(SetMiterLimit(limit));
}

void RecorderImpl::resetClip()
{
    updateStateForResetClip();
    append(ResetClip());
    clip(initialClip());
}

void RecorderImpl::clip(const FloatRect& clipRect)
{
    updateStateForClip(clipRect);
    append(Clip(clipRect));
}

void RecorderImpl::clipRoundedRect(const FloatRoundedRect& clipRect)
{
    updateStateForClipRoundedRect(clipRect);
    append(ClipRoundedRect(clipRect));
}

void RecorderImpl::clipOut(const FloatRect& clipRect)
{
    updateStateForClipOut(clipRect);
    append(ClipOut(clipRect));
}

void RecorderImpl::clipOutRoundedRect(const FloatRoundedRect& clipRect)
{
    updateStateForClipOutRoundedRect(clipRect);
    append(ClipOutRoundedRect(clipRect));
}

void RecorderImpl::recordClipToImageBuffer(ImageBuffer& imageBuffer, const FloatRect& destinationRect)
{
    append(ClipToImageBuffer(imageBuffer.renderingResourceIdentifier(), destinationRect));
}

void RecorderImpl::clipOut(const Path& path)
{
    updateStateForClipOut(path);
    append(ClipOutToPath(path));
}

void RecorderImpl::clipPath(const Path& path, WindRule rule)
{
    updateStateForClipPath(path);
    append(ClipPath(path, rule));
}

void RecorderImpl::recordDrawFilteredImageBuffer(ImageBuffer* sourceImage, const FloatRect& sourceImageRect, Filter& filter)
{
    std::optional<RenderingResourceIdentifier> identifier;
    if (sourceImage)
        identifier = sourceImage->renderingResourceIdentifier();
    append(DrawFilteredImageBuffer(WTFMove(identifier), sourceImageRect, filter));
}

void RecorderImpl::drawGlyphsImmediate(const Font& font, std::span<const GlyphBufferGlyph> glyphs, std::span<const GlyphBufferAdvance> advances, const FloatPoint& localAnchor, FontSmoothingMode smoothingMode)
{
    appendStateChangeItemIfNecessary();
    append(DrawGlyphs(Ref { font }, Vector(glyphs), Vector(advances), localAnchor, smoothingMode));
}

void RecorderImpl::drawDecomposedGlyphs(const Font& font, const DecomposedGlyphs& decomposedGlyphs)
{
    appendStateChangeItemIfNecessary();
    append(DrawDecomposedGlyphs(Ref { font }, Ref { decomposedGlyphs }));
}

void RecorderImpl::recordDrawImageBuffer(ImageBuffer& imageBuffer, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    append(DrawImageBuffer(imageBuffer.renderingResourceIdentifier(), destRect, srcRect, options));
}

void RecorderImpl::recordDrawNativeImage(RenderingResourceIdentifier imageIdentifier, const FloatRect& destRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    append(DrawNativeImage(imageIdentifier, destRect, srcRect, options));
}

void RecorderImpl::recordDrawSystemImage(SystemImage& systemImage, const FloatRect& destinationRect)
{
    append(DrawSystemImage(systemImage, destinationRect));
}

void RecorderImpl::recordDrawPattern(RenderingResourceIdentifier imageIdentifier, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& transform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    append(DrawPattern(imageIdentifier, destRect, tileRect, transform, phase, spacing, options));
}

void RecorderImpl::beginTransparencyLayer(float opacity)
{
    updateStateForBeginTransparencyLayer(opacity);
    append(BeginTransparencyLayer(opacity));
}

void RecorderImpl::beginTransparencyLayer(CompositeOperator compositeOperator, BlendMode blendMode)
{
    updateStateForBeginTransparencyLayer(compositeOperator, blendMode);
    append(BeginTransparencyLayerWithCompositeMode({ compositeOperator, blendMode }));
}

void RecorderImpl::endTransparencyLayer()
{
    updateStateForEndTransparencyLayer();
    append(EndTransparencyLayer());
}

void RecorderImpl::drawRect(const FloatRect& rect, float lineWidth)
{
    appendStateChangeItemIfNecessary();
    append(DrawRect(rect, lineWidth));
}

void RecorderImpl::drawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    appendStateChangeItemIfNecessary();
    append(DrawLine(point1, point2));
}

void RecorderImpl::drawLinesForText(const FloatPoint& point, float thickness, std::span<const FloatSegment> lineSegments, bool printing, bool doubleLines, StrokeStyle style)
{
    appendStateChangeItemIfNecessary();
    append(DrawLinesForText(point, lineSegments, thickness, printing, doubleLines, style));
}

void RecorderImpl::drawDotsForDocumentMarker(const FloatRect& rect, DocumentMarkerLineStyle style)
{
    appendStateChangeItemIfNecessary();
    append(DrawDotsForDocumentMarker(rect, style));
}

void RecorderImpl::drawEllipse(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary();
    append(DrawEllipse(rect));
}

void RecorderImpl::drawPath(const Path& path)
{
    appendStateChangeItemIfNecessary();
    append(DrawPath(path));
}

void RecorderImpl::drawFocusRing(const Path& path, float outlineWidth, const Color& color)
{
    appendStateChangeItemIfNecessary();
    append(DrawFocusRingPath(path, outlineWidth, color));
}

void RecorderImpl::drawFocusRing(const Vector<FloatRect>& rects, float outlineOffset, float outlineWidth, const Color& color)
{
    appendStateChangeItemIfNecessary();
    append(DrawFocusRingRects(rects, outlineOffset, outlineWidth, color));
}

void RecorderImpl::fillRect(const FloatRect& rect, RequiresClipToRect requiresClipToRect)
{
    appendStateChangeItemIfNecessary();
    append(FillRect(rect, requiresClipToRect));
}

void RecorderImpl::fillRect(const FloatRect& rect, const Color& color)
{
    appendStateChangeItemIfNecessary();
    append(FillRectWithColor(rect, color));
}

void RecorderImpl::fillRect(const FloatRect& rect, Gradient& gradient)
{
    appendStateChangeItemIfNecessary();
    append(FillRectWithGradient(rect, gradient));
}

void RecorderImpl::fillRect(const FloatRect& rect, Gradient& gradient, const AffineTransform& gradientSpaceTransform, RequiresClipToRect requiresClipToRect)
{
    appendStateChangeItemIfNecessary();
    append(FillRectWithGradientAndSpaceTransform(rect, gradient, gradientSpaceTransform, requiresClipToRect));
}

void RecorderImpl::fillRect(const FloatRect& rect, const Color& color, CompositeOperator op, BlendMode mode)
{
    appendStateChangeItemIfNecessary();
    append(FillCompositedRect(rect, color, op, mode));
}

void RecorderImpl::fillRoundedRect(const FloatRoundedRect& rect, const Color& color, BlendMode mode)
{
    appendStateChangeItemIfNecessary();
    append(FillRoundedRect(rect, color, mode));
}

void RecorderImpl::fillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedRect, const Color& color)
{
    appendStateChangeItemIfNecessary();
    append(FillRectWithRoundedHole(rect, roundedRect, color));
}

void RecorderImpl::fillPath(const Path& path)
{
    appendStateChangeItemIfNecessary();
    append(FillPath(path));
}

void RecorderImpl::fillEllipse(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary();
    append(FillEllipse(rect));
}

#if ENABLE(VIDEO)
void RecorderImpl::drawVideoFrame(VideoFrame&, const FloatRect&, ImageOrientation, bool)
{
    appendStateChangeItemIfNecessary();
    // FIXME: TODO
}
#endif // ENABLE(VIDEO)

void RecorderImpl::strokeRect(const FloatRect& rect, float width)
{
    appendStateChangeItemIfNecessary();
    append(StrokeRect(rect, width));
}

void RecorderImpl::strokePath(const Path& path)
{
    appendStateChangeItemIfNecessary();
    append(StrokePath(path));
}

void RecorderImpl::strokeEllipse(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary();
    append(StrokeEllipse(rect));
}

void RecorderImpl::clearRect(const FloatRect& rect)
{
    appendStateChangeItemIfNecessary();
    append(ClearRect(rect));
}

void RecorderImpl::drawControlPart(ControlPart& part, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    appendStateChangeItemIfNecessary();
    append(DrawControlPart(part, borderRect, deviceScaleFactor, style));
}

#if USE(CG)

void RecorderImpl::applyStrokePattern()
{
    appendStateChangeItemIfNecessary();
    append(ApplyStrokePattern());
}

void RecorderImpl::applyFillPattern()
{
    appendStateChangeItemIfNecessary();
    append(ApplyFillPattern());
}

#endif // USE(CG)

void RecorderImpl::applyDeviceScaleFactor(float scaleFactor)
{
    updateStateForApplyDeviceScaleFactor(scaleFactor);
    append(ApplyDeviceScaleFactor(scaleFactor));
}

void RecorderImpl::beginPage(const IntSize& pageSize)
{
    appendStateChangeItemIfNecessary();
    append(BeginPage({ pageSize }));
}

void RecorderImpl::endPage()
{
    appendStateChangeItemIfNecessary();
    append(EndPage());
}

void RecorderImpl::setURLForRect(const URL& link, const FloatRect& destRect)
{
    appendStateChangeItemIfNecessary();
    append(SetURLForRect(link, destRect));
}

bool RecorderImpl::recordResourceUse(NativeImage& nativeImage)
{
    m_displayList.cacheNativeImage(nativeImage);
    return true;
}

bool RecorderImpl::recordResourceUse(ImageBuffer& imageBuffer)
{
    m_displayList.cacheImageBuffer(imageBuffer);
    return true;
}

bool RecorderImpl::recordResourceUse(const SourceImage& image)
{
    if (auto imageBuffer = image.imageBufferIfExists())
        return recordResourceUse(*imageBuffer);

    if (auto nativeImage = image.nativeImageIfExists())
        return recordResourceUse(*nativeImage);

    return true;
}

void RecorderImpl::appendStateChangeItemIfNecessary()
{
    auto& state = currentState().state;
    if (!state.changes())
        return;

    auto recordFullItem = [&] {
        append(SetState(state));
        state.didApplyChanges();
        currentState().lastDrawingState = state;
    };
    auto changes = state.changes();

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
        append(SetInlineFillColor(*fillColor));
    if (strokeColor || strokeThickness)
        append(SetInlineStroke(strokeColor, strokeThickness));

    state.didApplyChanges();
    currentState().lastDrawingState = state;
}

} // namespace DisplayList
} // namespace WebCore
