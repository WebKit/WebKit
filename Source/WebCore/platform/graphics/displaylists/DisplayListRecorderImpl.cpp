/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

void RecorderImpl::recordSetInlineFillColor(PackedColor::RGBA inlineColor)
{
    append(SetInlineFillColor(inlineColor));
}

void RecorderImpl::recordSetInlineStroke(SetInlineStroke&& strokeItem)
{
    append(strokeItem);
}

void RecorderImpl::recordSetState(const GraphicsContextState& state)
{
    append(SetState(state));
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

void RecorderImpl::recordClearDropShadow()
{
    append(ClearDropShadow());
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

void RecorderImpl::recordDrawGlyphs(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned count, const FloatPoint& localAnchor, FontSmoothingMode mode)
{
    append(DrawGlyphs(font, glyphs, advances, count, localAnchor, mode));
}

void RecorderImpl::recordDrawDecomposedGlyphs(const Font& font, const DecomposedGlyphs& decomposedGlyphs)
{
    append(DrawDecomposedGlyphs(font.renderingResourceIdentifier(), decomposedGlyphs.renderingResourceIdentifier()));
}

void RecorderImpl::recordDrawDisplayListItems(const Vector<Item>& items, const FloatPoint& destination)
{
    append(DrawDisplayListItems(items, destination));
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

void RecorderImpl::drawLinesForText(const FloatPoint& point, float thickness, const DashArray& widths, bool printing, bool doubleLines, StrokeStyle style)
{
    appendStateChangeItemIfNecessary();
    append(DrawLinesForText(point, widths, thickness, printing, doubleLines, style));
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

#if ENABLE(INLINE_PATH_DATA)

void RecorderImpl::recordFillLine(const PathDataLine& line)
{
    append(FillLine(line));
}

void RecorderImpl::recordFillArc(const PathArc& arc)
{
    append(FillArc(arc));
}

void RecorderImpl::recordFillClosedArc(const PathClosedArc& closedArc)
{
    append(FillClosedArc(closedArc));
}

void RecorderImpl::recordFillQuadCurve(const PathDataQuadCurve& curve)
{
    append(FillQuadCurve(curve));
}

void RecorderImpl::recordFillBezierCurve(const PathDataBezierCurve& curve)
{
    append(FillBezierCurve(curve));
}

#endif // ENABLE(INLINE_PATH_DATA)

void RecorderImpl::recordFillPathSegment(const PathSegment& segment)
{
    append(FillPathSegment(segment));
}

void RecorderImpl::recordFillPath(const Path& path)
{
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

#if ENABLE(INLINE_PATH_DATA)

void RecorderImpl::recordStrokeLine(const PathDataLine& line)
{
    append(StrokeLine(line));
}

void RecorderImpl::recordStrokeLineWithColorAndThickness(const PathDataLine& line, SetInlineStroke&& strokeItem)
{
    append(strokeItem);
    append(StrokePathSegment(PathSegment { line }));
}

void RecorderImpl::recordStrokeArc(const PathArc& arc)
{
    append(StrokeArc(arc));
}

void RecorderImpl::recordStrokeClosedArc(const PathClosedArc& closedArc)
{
    append(StrokeClosedArc(closedArc));
}

void RecorderImpl::recordStrokeQuadCurve(const PathDataQuadCurve& curve)
{
    append(StrokeQuadCurve(curve));
}

void RecorderImpl::recordStrokeBezierCurve(const PathDataBezierCurve& curve)
{
    append(StrokeBezierCurve(curve));
}

#endif // ENABLE(INLINE_PATH_DATA)

void RecorderImpl::recordStrokePathSegment(const PathSegment& segment)
{
    append(StrokePathSegment(segment));
}

void RecorderImpl::recordStrokePath(const Path& path)
{
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

bool RecorderImpl::recordResourceUse(Font& font)
{
    m_displayList.cacheFont(font);
    return true;
}

bool RecorderImpl::recordResourceUse(DecomposedGlyphs& decomposedGlyphs)
{
    m_displayList.cacheDecomposedGlyphs(decomposedGlyphs);
    return true;
}

bool RecorderImpl::recordResourceUse(Gradient& gradient)
{
    m_displayList.cacheGradient(gradient);
    return true;
}

bool RecorderImpl::recordResourceUse(Filter& filter)
{
    m_displayList.cacheFilter(filter);
    return true;
}

} // namespace DisplayList
} // namespace WebCore
