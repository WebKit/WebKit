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
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace DisplayList {

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

void RecorderImpl::recordSave()
{
    append(Save());
}

void RecorderImpl::recordRestore()
{
    append(Restore());
}

void RecorderImpl::recordTranslate(float x, float y)
{
    append(Translate(x, y));
}

void RecorderImpl::recordRotate(float angle)
{
    append(Rotate(angle));
}

void RecorderImpl::recordScale(const FloatSize& scale)
{
    append(Scale(scale));
}

void RecorderImpl::recordSetCTM(const AffineTransform& transform)
{
    append(SetCTM(transform));
}

void RecorderImpl::recordConcatenateCTM(const AffineTransform& transform)
{
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

void RecorderImpl::recordSetLineCap(LineCap lineCap)
{
    append(SetLineCap(lineCap));
}

void RecorderImpl::recordSetLineDash(const DashArray& array, float dashOffset)
{
    append(SetLineDash(array, dashOffset));
}

void RecorderImpl::recordSetLineJoin(LineJoin join)
{
    append(SetLineJoin(join));
}

void RecorderImpl::recordSetMiterLimit(float limit)
{
    append(SetMiterLimit(limit));
}

void RecorderImpl::recordClearDropShadow()
{
    append(ClearDropShadow());
}

void RecorderImpl::recordResetClip()
{
    append(ResetClip());
}

void RecorderImpl::recordClip(const FloatRect& clipRect)
{
    append(Clip(clipRect));
}

void RecorderImpl::recordClipRoundedRect(const FloatRoundedRect& clipRect)
{
    append(ClipRoundedRect(clipRect));
}

void RecorderImpl::recordClipOut(const FloatRect& clipRect)
{
    append(ClipOut(clipRect));
}

void RecorderImpl::recordClipOutRoundedRect(const FloatRoundedRect& clipRect)
{
    append(ClipOutRoundedRect(clipRect));
}

void RecorderImpl::recordClipToImageBuffer(ImageBuffer& imageBuffer, const FloatRect& destinationRect)
{
    append(ClipToImageBuffer(imageBuffer.renderingResourceIdentifier(), destinationRect));
}

void RecorderImpl::recordClipOutToPath(const Path& path)
{
    append(ClipOutToPath(path));
}

void RecorderImpl::recordClipPath(const Path& path, WindRule rule)
{
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

void RecorderImpl::recordBeginTransparencyLayer(float opacity)
{
    append(BeginTransparencyLayer(opacity));
}

void RecorderImpl::recordEndTransparencyLayer()
{
    append(EndTransparencyLayer());
}

void RecorderImpl::recordDrawRect(const FloatRect& rect, float lineWidth)
{
    append(DrawRect(rect, lineWidth));
}

void RecorderImpl::recordDrawLine(const FloatPoint& point1, const FloatPoint& point2)
{
    append(DrawLine(point1, point2));
}

void RecorderImpl::recordDrawLinesForText(const FloatPoint& blockLocation, const FloatSize& localAnchor, float thickness, const DashArray& widths, bool printing, bool doubleLines, StrokeStyle style)
{
    append(DrawLinesForText(blockLocation, localAnchor, widths, thickness, printing, doubleLines, style));
}

void RecorderImpl::recordDrawDotsForDocumentMarker(const FloatRect& rect, const DocumentMarkerLineStyle& style)
{
    append(DrawDotsForDocumentMarker(rect, style));
}

void RecorderImpl::recordDrawEllipse(const FloatRect& rect)
{
    append(DrawEllipse(rect));
}

void RecorderImpl::recordDrawPath(const Path& path)
{
    append(DrawPath(path));
}

void RecorderImpl::recordDrawFocusRingPath(const Path& path, float outlineWidth, const Color& color)
{
    append(DrawFocusRingPath(path, outlineWidth, color));
}

void RecorderImpl::recordDrawFocusRingRects(const Vector<FloatRect>& rects, float outlineOffset, float outlineWidth, const Color& color)
{
    append(DrawFocusRingRects(rects, outlineOffset, outlineWidth, color));
}

void RecorderImpl::recordFillRect(const FloatRect& rect)
{
    append(FillRect(rect));
}

void RecorderImpl::recordFillRectWithColor(const FloatRect& rect, const Color& color)
{
    append(FillRectWithColor(rect, color));
}

void RecorderImpl::recordFillRectWithGradient(const FloatRect& rect, Gradient& gradient)
{
    append(FillRectWithGradient(rect, gradient));
}

void RecorderImpl::recordFillRectWithGradientAndSpaceTransform(const FloatRect& rect, Gradient& gradient, const AffineTransform& gradientSpaceTransform)
{
    append(FillRectWithGradientAndSpaceTransform(rect, gradient, gradientSpaceTransform));
}

void RecorderImpl::recordFillCompositedRect(const FloatRect& rect, const Color& color, CompositeOperator op, BlendMode mode)
{
    append(FillCompositedRect(rect, color, op, mode));
}

void RecorderImpl::recordFillRoundedRect(const FloatRoundedRect& rect, const Color& color, BlendMode mode)
{
    append(FillRoundedRect(rect, color, mode));
}

void RecorderImpl::recordFillRectWithRoundedHole(const FloatRect& rect, const FloatRoundedRect& roundedRect, const Color& color)
{
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

void RecorderImpl::recordFillEllipse(const FloatRect& rect)
{
    append(FillEllipse(rect));
}

#if ENABLE(VIDEO)
void RecorderImpl::recordPaintFrameForMedia(MediaPlayer& player, const FloatRect& destination)
{
    append(PaintFrameForMedia(player.identifier(), destination));
}

void RecorderImpl::recordPaintVideoFrame(VideoFrame&, const FloatRect&, bool /* shouldDiscardAlpha */)
{
    // FIXME: TODO
}
#endif // ENABLE(VIDEO)

void RecorderImpl::recordStrokeRect(const FloatRect& rect, float width)
{
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

void RecorderImpl::recordStrokeEllipse(const FloatRect& rect)
{
    append(StrokeEllipse(rect));
}

void RecorderImpl::recordClearRect(const FloatRect& rect)
{
    append(ClearRect(rect));
}

void RecorderImpl::recordDrawControlPart(ControlPart& part, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
{
    append(DrawControlPart(part, borderRect, deviceScaleFactor, style));
}

#if USE(CG)

void RecorderImpl::recordApplyStrokePattern()
{
    append(ApplyStrokePattern());
}

void RecorderImpl::recordApplyFillPattern()
{
    append(ApplyFillPattern());
}

#endif // USE(CG)

void RecorderImpl::recordApplyDeviceScaleFactor(float scaleFactor)
{
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
