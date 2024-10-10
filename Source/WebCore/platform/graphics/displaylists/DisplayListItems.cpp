/*
 * Copyright (C) 2015-2023 Apple Inc. All rights reserved.
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
#include "DisplayListItems.h"

#include "DecomposedGlyphs.h"
#include "DisplayListReplayer.h"
#include "Filter.h"
#include "FontCascade.h"
#include "ImageBuffer.h"
#include "MediaPlayer.h"
#include "SharedBuffer.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace DisplayList {

void Save::apply(GraphicsContext& context) const
{
    context.save();
}

void Restore::apply(GraphicsContext& context) const
{
    context.restore();
}

void Translate::apply(GraphicsContext& context) const
{
    context.translate(m_x, m_y);
}

void Translate::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("x", x());
    ts.dumpProperty("y", y());
}

void Rotate::apply(GraphicsContext& context) const
{
    context.rotate(m_angle);
}

void Rotate::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("angle", angle());
}

void Scale::apply(GraphicsContext& context) const
{
    context.scale(m_size);
}

void Scale::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("size", amount());
}

void SetCTM::apply(GraphicsContext& context) const
{
    context.setCTM(m_transform);
}

void SetCTM::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("set-ctm", transform());
}

void ConcatenateCTM::apply(GraphicsContext& context) const
{
    context.concatCTM(m_transform);
}

void ConcatenateCTM::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("ctm", transform());
}

void SetInlineFillColor::apply(GraphicsContext& context) const
{
    context.setFillColor(color());
}

void SetInlineFillColor::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("color", color());
}

void SetInlineStroke::apply(GraphicsContext& context) const
{
    if (auto color = this->color())
        context.setStrokeColor(*color);
    if (auto thickness = this->thickness())
        context.setStrokeThickness(*thickness);
}

void SetInlineStroke::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    if (auto color = this->color())
        ts.dumpProperty("color", *color);
    if (auto thickness = this->thickness())
        ts.dumpProperty("thickness", *thickness);
}

SetState::SetState(const GraphicsContextState& state)
    : m_state(state)
{
}

void SetState::apply(GraphicsContext& context) const
{
    context.mergeLastChanges(m_state);
}

void SetState::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts << state();
}

void SetLineCap::apply(GraphicsContext& context) const
{
    context.setLineCap(m_lineCap);
}

void SetLineCap::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("line-cap", lineCap());
}

void SetLineDash::apply(GraphicsContext& context) const
{
    context.setLineDash(m_dashArray, m_dashOffset);
}

void SetLineDash::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("dash-array", dashArray());
    ts.dumpProperty("dash-offset", dashOffset());
}

void SetLineJoin::apply(GraphicsContext& context) const
{
    context.setLineJoin(m_lineJoin);
}

void SetLineJoin::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("line-join", lineJoin());
}

void SetMiterLimit::apply(GraphicsContext& context) const
{
    context.setMiterLimit(m_miterLimit);
}

void SetMiterLimit::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("mitre-limit", miterLimit());
}

void ClearDropShadow::apply(GraphicsContext& context) const
{
    context.clearDropShadow();
}

void Clip::apply(GraphicsContext& context) const
{
    context.clip(m_rect);
}

void Clip::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect", rect());
}

void ClipRoundedRect::apply(GraphicsContext& context) const
{
    context.clipRoundedRect(m_rect);
}

void ClipRoundedRect::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect", rect());
}

void ClipOut::apply(GraphicsContext& context) const
{
    context.clipOut(m_rect);
}

void ClipOut::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect", rect());
}

void ClipOutRoundedRect::apply(GraphicsContext& context) const
{
    context.clipOutRoundedRect(m_rect);
}

void ClipOutRoundedRect::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect", rect());
}

void ClipToImageBuffer::apply(GraphicsContext& context, WebCore::ImageBuffer& imageBuffer) const
{
    context.clipToImageBuffer(imageBuffer, m_destinationRect);
}

void ClipToImageBuffer::dump(TextStream& ts, OptionSet<AsTextFlag> flags) const
{
    if (flags.contains(AsTextFlag::IncludeResourceIdentifiers))
        ts.dumpProperty("image-buffer-identifier", imageBufferIdentifier());
    ts.dumpProperty("dest-rect", destinationRect());
}

void ClipOutToPath::apply(GraphicsContext& context) const
{
    context.clipOut(m_path);
}

void ClipOutToPath::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("path", path());
}

void ClipPath::apply(GraphicsContext& context) const
{
    context.clipPath(m_path, m_windRule);
}

void ClipPath::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("path", path());
    ts.dumpProperty("wind-rule", windRule());
}

void ResetClip::apply(GraphicsContext& context) const
{
    context.resetClip();
}

DrawFilteredImageBuffer::DrawFilteredImageBuffer(std::optional<RenderingResourceIdentifier> sourceImageIdentifier, const FloatRect& sourceImageRect, Filter& filter)
    : m_sourceImageIdentifier(sourceImageIdentifier)
    , m_sourceImageRect(sourceImageRect)
    , m_filter(filter)
{
}

NO_RETURN_DUE_TO_ASSERT void DrawFilteredImageBuffer::apply(GraphicsContext&) const
{
    ASSERT_NOT_REACHED();
}

void DrawFilteredImageBuffer::apply(GraphicsContext& context, ImageBuffer* sourceImage, FilterResults& results) const
{
    context.drawFilteredImageBuffer(sourceImage, m_sourceImageRect, m_filter, results);
}

void DrawFilteredImageBuffer::dump(TextStream& ts, OptionSet<AsTextFlag> flags) const
{
    if (flags.contains(AsTextFlag::IncludeResourceIdentifiers))
        ts.dumpProperty("source-image-identifier", sourceImageIdentifier());
    ts.dumpProperty("source-image-rect", sourceImageRect());
}

DrawGlyphs::DrawGlyphs(RenderingResourceIdentifier fontIdentifier, PositionedGlyphs&& positionedGlyphs)
    : m_fontIdentifier(fontIdentifier)
    , m_positionedGlyphs(WTFMove(positionedGlyphs))
{
}

DrawGlyphs::DrawGlyphs(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned count, const FloatPoint& localAnchor, FontSmoothingMode smoothingMode)
    : m_fontIdentifier(font.renderingResourceIdentifier())
    , m_positionedGlyphs { Vector(std::span { glyphs, count }), Vector(std::span { advances, count }), localAnchor, smoothingMode }
{
}

void DrawGlyphs::apply(GraphicsContext& context, const Font& font) const
{
    return context.drawGlyphs(font, m_positionedGlyphs.glyphs.data(), m_positionedGlyphs.advances.data(), m_positionedGlyphs.glyphs.size(), anchorPoint(), m_positionedGlyphs.smoothingMode);
}

void DrawGlyphs::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    // FIXME: dump more stuff.
    ts.dumpProperty("local-anchor", localAnchor());
    ts.dumpProperty("anchor-point", anchorPoint());
    ts.dumpProperty("font-smoothing-mode", fontSmoothingMode());
    ts.dumpProperty("length", glyphs().size());
}

void DrawDecomposedGlyphs::apply(GraphicsContext& context, const Font& font, const DecomposedGlyphs& decomposedGlyphs) const
{
    return context.drawDecomposedGlyphs(font, decomposedGlyphs);
}

void DrawDecomposedGlyphs::dump(TextStream& ts, OptionSet<AsTextFlag> flags) const
{
    if (flags.contains(AsTextFlag::IncludeResourceIdentifiers)) {
        ts.dumpProperty("font-identifier", fontIdentifier());
        ts.dumpProperty("draw-glyphs-data-identifier", decomposedGlyphsIdentifier());
    }
}

DrawDisplayListItems::DrawDisplayListItems(const Vector<Item>& items, const FloatPoint& destination)
    : m_items(items)
    , m_destination(destination)
{
}

DrawDisplayListItems::DrawDisplayListItems(Vector<Item>&& items, const FloatPoint& destination)
    : m_items(WTFMove(items))
    , m_destination(destination)
{
}

void DrawDisplayListItems::apply(GraphicsContext& context, const ResourceHeap& resourceHeap, ControlFactory& controlFactory) const
{
    context.drawDisplayListItems(m_items, resourceHeap, controlFactory, m_destination);
}

NO_RETURN_DUE_TO_ASSERT void DrawDisplayListItems::apply(GraphicsContext&) const
{
    ASSERT_NOT_REACHED();
}

void DrawDisplayListItems::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts << items();
    ts.dumpProperty("destination", destination());
}

void DrawImageBuffer::apply(GraphicsContext& context, WebCore::ImageBuffer& imageBuffer) const
{
    context.drawImageBuffer(imageBuffer, m_destinationRect, m_srcRect, m_options);
}

void DrawImageBuffer::dump(TextStream& ts, OptionSet<AsTextFlag> flags) const
{
    if (flags.contains(AsTextFlag::IncludeResourceIdentifiers))
        ts.dumpProperty("image-buffer-identifier", imageBufferIdentifier());
    ts.dumpProperty("source-rect", source());
    ts.dumpProperty("dest-rect", destinationRect());
}

void DrawNativeImage::apply(GraphicsContext& context, NativeImage& image) const
{
    context.drawNativeImageInternal(image, m_destinationRect, m_srcRect, m_options);
}

void DrawNativeImage::dump(TextStream& ts, OptionSet<AsTextFlag> flags) const
{
    if (flags.contains(AsTextFlag::IncludeResourceIdentifiers))
        ts.dumpProperty("image-identifier", imageIdentifier());
    ts.dumpProperty("source-rect", source());
    ts.dumpProperty("dest-rect", destinationRect());
}

void DrawSystemImage::apply(GraphicsContext& context) const
{
    context.drawSystemImage(m_systemImage, m_destinationRect);
}

void DrawSystemImage::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    // FIXME: dump more stuff.
    ts.dumpProperty("destination", destinationRect());
}

DrawPattern::DrawPattern(RenderingResourceIdentifier imageIdentifier, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
    : m_imageIdentifier(imageIdentifier)
    , m_destination(destRect)
    , m_tileRect(tileRect)
    , m_patternTransform(patternTransform)
    , m_phase(phase)
    , m_spacing(spacing)
    , m_options(options)
{
}

void DrawPattern::apply(GraphicsContext& context, SourceImage& sourceImage) const
{
    if (auto image = sourceImage.nativeImageIfExists()) {
        context.drawPattern(*image, m_destination, m_tileRect, m_patternTransform, m_phase, m_spacing, m_options);
        return;
    }

    if (auto imageBuffer = sourceImage.imageBufferIfExists()) {
        context.drawPattern(*imageBuffer, m_destination, m_tileRect, m_patternTransform, m_phase, m_spacing, m_options);
        return;
    }

    ASSERT_NOT_REACHED();
}

void DrawPattern::dump(TextStream& ts, OptionSet<AsTextFlag> flags) const
{
    if (flags.contains(AsTextFlag::IncludeResourceIdentifiers))
        ts.dumpProperty("image-identifier", imageIdentifier());
    ts.dumpProperty("pattern-transform", patternTransform());
    ts.dumpProperty("tile-rect", tileRect());
    ts.dumpProperty("dest-rect", destRect());
    ts.dumpProperty("phase", phase());
    ts.dumpProperty("spacing", spacing());
}

void DrawRect::apply(GraphicsContext& context) const
{
    context.drawRect(m_rect, m_borderThickness);
}

void DrawRect::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect", rect());
    ts.dumpProperty("border-thickness", borderThickness());
}

void DrawLine::apply(GraphicsContext& context) const
{
    context.drawLine(m_point1, m_point2);
}

void DrawLine::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("point-1", point1());
    ts.dumpProperty("point-2", point2());
}

DrawLinesForText::DrawLinesForText(const FloatPoint& point, const DashArray& widths, float thickness, bool printing, bool doubleLines, StrokeStyle style)
    : m_point(point)
    , m_widths(widths)
    , m_thickness(thickness)
    , m_printing(printing)
    , m_doubleLines(doubleLines)
    , m_style(style)
{
}

void DrawLinesForText::apply(GraphicsContext& context) const
{
    context.drawLinesForText(m_point, m_thickness, m_widths, m_printing, m_doubleLines, m_style);
}

void DrawLinesForText::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("point", point());
    ts.dumpProperty("thickness", thickness());
    ts.dumpProperty("double", doubleLines());
    ts.dumpProperty("widths", widths());
    ts.dumpProperty("is-printing", isPrinting());
    ts.dumpProperty("double", doubleLines());
}

void DrawDotsForDocumentMarker::apply(GraphicsContext& context) const
{
    context.drawDotsForDocumentMarker(m_rect, m_style);
}

void DrawDotsForDocumentMarker::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect", rect());
}

void DrawEllipse::apply(GraphicsContext& context) const
{
    context.drawEllipse(m_rect);
}

void DrawEllipse::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect", rect());
}

void DrawPath::apply(GraphicsContext& context) const
{
    context.drawPath(m_path);
}

void DrawPath::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("path", path());
}

void DrawFocusRingPath::apply(GraphicsContext& context) const
{
    context.drawFocusRing(m_path, m_outlineWidth, m_color);
}

void DrawFocusRingPath::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("path", path());
    ts.dumpProperty("outline-width", outlineWidth());
    ts.dumpProperty("color", color());
}

void DrawFocusRingRects::apply(GraphicsContext& context) const
{
    context.drawFocusRing(m_rects, m_outlineOffset, m_outlineWidth, m_color);
}

void DrawFocusRingRects::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rects", rects());
    ts.dumpProperty("outline-offset", outlineOffset());
    ts.dumpProperty("outline-width", outlineWidth());
    ts.dumpProperty("color", color());
}

void FillRect::apply(GraphicsContext& context) const
{
    context.fillRect(m_rect, m_requiresClipToRect);
}

void FillRect::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect", rect());
    ts.dumpProperty("requiresClipToRect", m_requiresClipToRect == GraphicsContext::RequiresClipToRect::Yes);
}

void FillRectWithColor::apply(GraphicsContext& context) const
{
    context.fillRect(m_rect, m_color);
}

void FillRectWithColor::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect", rect());
    ts.dumpProperty("color", color());
}

FillRectWithGradient::FillRectWithGradient(const FloatRect& rect, Gradient& gradient)
    : m_rect(rect)
    , m_gradient(gradient)
{
}

FillRectWithGradient::FillRectWithGradient(FloatRect&& rect, Ref<Gradient>&& gradient)
    : m_rect(WTFMove(rect))
    , m_gradient(WTFMove(gradient))
{
}

void FillRectWithGradient::apply(GraphicsContext& context) const
{
    context.fillRect(m_rect, m_gradient);
}

void FillRectWithGradient::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    // FIXME: log gradient.
    ts.dumpProperty("rect", rect());
}

FillRectWithGradientAndSpaceTransform::FillRectWithGradientAndSpaceTransform(const FloatRect& rect, Gradient& gradient, const AffineTransform& gradientSpaceTransform, GraphicsContext::RequiresClipToRect requiresClipToRect)
    : m_rect(rect)
    , m_gradient(gradient)
    , m_gradientSpaceTransform(gradientSpaceTransform)
    , m_requiresClipToRect(requiresClipToRect)
{
}

FillRectWithGradientAndSpaceTransform::FillRectWithGradientAndSpaceTransform(FloatRect&& rect, Ref<Gradient>&& gradient, AffineTransform&& gradientSpaceTransform, GraphicsContext::RequiresClipToRect requiresClipToRect)
    : m_rect(WTFMove(rect))
    , m_gradient(WTFMove(gradient))
    , m_gradientSpaceTransform(WTFMove(gradientSpaceTransform))
    , m_requiresClipToRect(requiresClipToRect)
{
}

void FillRectWithGradientAndSpaceTransform::apply(GraphicsContext& context) const
{
    context.fillRect(m_rect, m_gradient, m_gradientSpaceTransform);
}

void FillRectWithGradientAndSpaceTransform::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    // FIXME: log gradient.
    ts.dumpProperty("rect", rect());
    ts.dumpProperty("gradient-space-transform", gradientSpaceTransform());
    ts.dumpProperty("requiresClipToRect", m_requiresClipToRect == GraphicsContext::RequiresClipToRect::Yes);
}

void FillCompositedRect::apply(GraphicsContext& context) const
{
    context.fillRect(m_rect, m_color, m_op, m_blendMode);
}

void FillCompositedRect::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect", rect());
    ts.dumpProperty("color", color());
    ts.dumpProperty("composite-operation", compositeOperator());
    ts.dumpProperty("blend-mode", blendMode());
}

void FillRoundedRect::apply(GraphicsContext& context) const
{
    context.fillRoundedRect(m_rect, m_color, m_blendMode);
}

void FillRoundedRect::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect", roundedRect());
    ts.dumpProperty("color", color());
    ts.dumpProperty("blend-mode", blendMode());
}

void FillRectWithRoundedHole::apply(GraphicsContext& context) const
{
    context.fillRectWithRoundedHole(m_rect, m_roundedHoleRect, m_color);
}

void FillRectWithRoundedHole::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect", rect());
    ts.dumpProperty("rounded-hole-rect", roundedHoleRect());
    ts.dumpProperty("color", color());
}

#if ENABLE(INLINE_PATH_DATA)

void FillLine::apply(GraphicsContext& context) const
{
    context.fillPath(path());
}

void FillLine::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("path", path());
}

void FillArc::apply(GraphicsContext& context) const
{
    context.fillPath(path());
}

void FillArc::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("path", path());
}

void FillClosedArc::apply(GraphicsContext& context) const
{
    context.fillPath(path());
}

void FillClosedArc::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("path", path());
}

void FillQuadCurve::apply(GraphicsContext& context) const
{
    context.fillPath(path());
}

void FillQuadCurve::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("path", path());
}

void FillBezierCurve::apply(GraphicsContext& context) const
{
    context.fillPath(path());
}

void FillBezierCurve::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("path", path());
}

#endif // ENABLE(INLINE_PATH_DATA)

void FillPathSegment::apply(GraphicsContext& context) const
{
    context.fillPath(path());
}

void FillPathSegment::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("path", path());
}

void FillPath::apply(GraphicsContext& context) const
{
    context.fillPath(m_path);
}

void FillPath::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("path", path());
}

void FillEllipse::apply(GraphicsContext& context) const
{
    context.fillEllipse(m_rect);
}

void FillEllipse::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect", rect());
}

void StrokeRect::apply(GraphicsContext& context) const
{
    context.strokeRect(m_rect, m_lineWidth);
}

void StrokeRect::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect", rect());
    ts.dumpProperty("line-width", lineWidth());
}

void StrokePath::apply(GraphicsContext& context) const
{
    context.strokePath(m_path);
}

void StrokePath::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("path", path());
}

void StrokePathSegment::apply(GraphicsContext& context) const
{
    context.strokePath(path());
}

void StrokePathSegment::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("path", path());
}

void StrokeEllipse::apply(GraphicsContext& context) const
{
    context.strokeEllipse(m_rect);
}

void StrokeEllipse::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect", rect());
}

void StrokeLine::apply(GraphicsContext& context) const
{
#if ENABLE(INLINE_PATH_DATA)
    auto path = Path({ PathSegment { PathDataLine { { start() }, { end() } } } });
#else
    Path path;
    path.moveTo(start());
    path.addLineTo(end());
#endif
    context.strokePath(path);
}

void StrokeLine::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("start", start());
    ts.dumpProperty("end", end());
}

#if ENABLE(INLINE_PATH_DATA)

void StrokeArc::apply(GraphicsContext& context) const
{
    context.strokePath(path());
}

void StrokeArc::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("path", path());
}

void StrokeClosedArc::apply(GraphicsContext& context) const
{
    context.strokePath(path());
}

void StrokeClosedArc::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("path", path());
}

void StrokeQuadCurve::apply(GraphicsContext& context) const
{
    context.strokePath(path());
}

void StrokeQuadCurve::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("path", path());
}

void StrokeBezierCurve::apply(GraphicsContext& context) const
{
    context.strokePath(path());
}

void StrokeBezierCurve::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("path", path());
}

#endif // ENABLE(INLINE_PATH_DATA)

void ClearRect::apply(GraphicsContext& context) const
{
    context.clearRect(m_rect);
}

void ClearRect::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect", rect());
}

DrawControlPart::DrawControlPart(ControlPart& part, const FloatRoundedRect& borderRect, float deviceScaleFactor, const ControlStyle& style)
    : m_part(part)
    , m_borderRect(borderRect)
    , m_deviceScaleFactor(deviceScaleFactor)
    , m_style(style)
{
}

void DrawControlPart::apply(GraphicsContext& context, ControlFactory& controlFactory) const
{
    m_part->setOverrideControlFactory(&controlFactory);
    context.drawControlPart(m_part, m_borderRect, m_deviceScaleFactor, m_style);
    m_part->setOverrideControlFactory(nullptr);
}

void DrawControlPart::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("type", type());
    ts.dumpProperty("border-rect", borderRect());
    ts.dumpProperty("device-scale-factor", deviceScaleFactor());
    ts.dumpProperty("style", style());
}

void BeginTransparencyLayer::apply(GraphicsContext& context) const
{
    context.beginTransparencyLayer(m_opacity);
}

void BeginTransparencyLayerWithCompositeMode::apply(GraphicsContext& context) const
{
    context.beginTransparencyLayer(m_compositeMode.operation, m_compositeMode.blendMode);
}

void BeginTransparencyLayer::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("opacity", opacity());
}

void BeginTransparencyLayerWithCompositeMode::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("composite-operator", compositeMode().operation);
    ts.dumpProperty("blend-mode", compositeMode().blendMode);
}

void EndTransparencyLayer::apply(GraphicsContext& context) const
{
    if (context.isInTransparencyLayer())
        context.endTransparencyLayer();
}

#if USE(CG)

void ApplyStrokePattern::apply(GraphicsContext& context) const
{
    context.applyStrokePattern();
}

void ApplyFillPattern::apply(GraphicsContext& context) const
{
    context.applyFillPattern();
}
#endif

void ApplyDeviceScaleFactor::apply(GraphicsContext& context) const
{
    context.applyDeviceScaleFactor(m_scaleFactor);
}

void ApplyDeviceScaleFactor::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("scale-factor", scaleFactor());
}

} // namespace DisplayList
} // namespace WebCore
