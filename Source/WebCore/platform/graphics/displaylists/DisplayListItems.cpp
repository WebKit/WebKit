/*
 * Copyright (C) 2015-2025 Apple Inc. All rights reserved.
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

#include "DisplayList.h"
#include "Filter.h"
#include "FilterResults.h"
#include "FontCascade.h"
#include "ImageBuffer.h"
#include "MediaPlayer.h"
#include "SharedBuffer.h"
#include <wtf/text/TextStream.h>

#if USE(SKIA)
#include "GraphicsContextSkia.h"
#endif

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
    ts.dumpProperty("x"_s, x());
    ts.dumpProperty("y"_s, y());
}

void Rotate::apply(GraphicsContext& context) const
{
    context.rotate(m_angle);
}

void Rotate::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("angle"_s, angle());
}

void Scale::apply(GraphicsContext& context) const
{
    context.scale(m_size);
}

void Scale::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("size"_s, amount());
}

void SetCTM::apply(GraphicsContext& context) const
{
    context.setCTM(m_transform);
}

void SetCTM::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("set-ctm"_s, transform());
}

void ConcatenateCTM::apply(GraphicsContext& context) const
{
    context.concatCTM(m_transform);
}

void ConcatenateCTM::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("ctm"_s, transform());
}

void SetInlineFillColor::apply(GraphicsContext& context) const
{
    context.setFillColor(color());
}

void SetInlineFillColor::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("color"_s, color());
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
        ts.dumpProperty("color"_s, *color);
    if (auto thickness = this->thickness())
        ts.dumpProperty("thickness"_s, *thickness);
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
    ts.dumpProperty("line-cap"_s, lineCap());
}

void SetLineDash::apply(GraphicsContext& context) const
{
    context.setLineDash(m_dashArray, m_dashOffset);
}

void SetLineDash::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("dash-array"_s, dashArray());
    ts.dumpProperty("dash-offset"_s, dashOffset());
}

void SetLineJoin::apply(GraphicsContext& context) const
{
    context.setLineJoin(m_lineJoin);
}

void SetLineJoin::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("line-join"_s, lineJoin());
}

void SetMiterLimit::apply(GraphicsContext& context) const
{
    context.setMiterLimit(m_miterLimit);
}

void SetMiterLimit::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("mitre-limit"_s, miterLimit());
}

void Clip::apply(GraphicsContext& context) const
{
    context.clip(m_rect);
}

void Clip::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect"_s, rect());
}

void ClipRoundedRect::apply(GraphicsContext& context) const
{
    context.clipRoundedRect(m_rect);
}

void ClipRoundedRect::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect"_s, rect());
}

void ClipOut::apply(GraphicsContext& context) const
{
    context.clipOut(m_rect);
}

void ClipOut::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect"_s, rect());
}

void ClipOutRoundedRect::apply(GraphicsContext& context) const
{
    context.clipOutRoundedRect(m_rect);
}

void ClipOutRoundedRect::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect"_s, rect());
}

void ClipToImageBuffer::apply(GraphicsContext& context) const
{
    context.clipToImageBuffer(m_imageBuffer, m_destinationRect);
}

void ClipToImageBuffer::dump(TextStream& ts, OptionSet<AsTextFlag> flags) const
{
    if (flags.contains(AsTextFlag::IncludeResourceIdentifiers))
        ts.dumpProperty("image-buffer-identifier"_s, m_imageBuffer->renderingResourceIdentifier());
    ts.dumpProperty("dest-rect"_s, destinationRect());
}

void ClipOutToPath::apply(GraphicsContext& context) const
{
    context.clipOut(m_path);
}

void ClipOutToPath::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("path"_s, path());
}

void ClipPath::apply(GraphicsContext& context) const
{
    context.clipPath(m_path, m_windRule);
}

void ClipPath::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("path"_s, path());
    ts.dumpProperty("wind-rule"_s, windRule());
}

void ResetClip::apply(GraphicsContext& context) const
{
    context.resetClip();
}

void DrawFilteredImageBuffer::apply(GraphicsContext& context) const
{
    FilterResults results;
    context.drawFilteredImageBuffer(m_sourceImage.get(), m_sourceImageRect, m_filter, results);
}

void DrawFilteredImageBuffer::dump(TextStream& ts, OptionSet<AsTextFlag> flags) const
{
    if (flags.contains(AsTextFlag::IncludeResourceIdentifiers)) {
        if (m_sourceImage)
            ts.dumpProperty("source-image-identifier"_s, m_sourceImage->renderingResourceIdentifier());
    }
    ts.dumpProperty("source-image-rect"_s, sourceImageRect());
}

void DrawGlyphs::apply(GraphicsContext& context) const
{
    context.drawGlyphs(m_font, m_glyphs.span(), m_advances.span(), m_localAnchor, m_fontSmoothingMode);
}

void DrawGlyphs::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    // FIXME: dump more stuff.
    ts.dumpProperty("local-anchor"_s, localAnchor());
    ts.dumpProperty("font-smoothing-mode"_s, fontSmoothingMode());
    ts.dumpProperty("length"_s, length());
}

#if USE(SKIA)
void DrawTextBlob::apply(GraphicsContext& context) const
{
    if (m_textBlob)
        static_cast<GraphicsContextSkia*>(&context)->drawSkiaText(m_textBlob, SkFloatToScalar(m_localAnchor.x()), SkFloatToScalar(m_localAnchor.y()), m_enableAntialiasing, m_isVertical);
}

void DrawTextBlob::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("local-anchor"_s, localAnchor());
    ts.dumpProperty("font-smoothing-mode"_s, fontSmoothingMode());
    ts.dumpProperty("length"_s, length());
}
#endif // USE(SKIA)

DrawDisplayList::DrawDisplayList(Ref<const DisplayList>&& displayList)
    : m_displayList(WTFMove(displayList))
{
}

DrawDisplayList::~DrawDisplayList() = default;

Ref<const DisplayList> DrawDisplayList::displayList() const
{
    return m_displayList;
}

void DrawDisplayList::apply(GraphicsContext& context) const
{
    return context.drawDisplayList(m_displayList);
}

void DrawDisplayList::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    Ref displayList = this->displayList();
    ts.dumpProperty("display-list"_s, displayList);
}

DrawPlaceholder::DrawPlaceholder(Function<void(GraphicsContext&)>&& function)
    : m_function(FunctionHolder::create(WTFMove(function)))
{
}

DrawPlaceholder::~DrawPlaceholder() = default;

void DrawPlaceholder::apply(GraphicsContext& context) const
{
    return (*m_function)(context);
}

void DrawPlaceholder::dump(TextStream&, OptionSet<AsTextFlag>) const
{
}

void DrawImageBuffer::apply(GraphicsContext& context) const
{
    context.drawImageBuffer(m_imageBuffer, m_destinationRect, m_srcRect, m_options);
}

void DrawImageBuffer::dump(TextStream& ts, OptionSet<AsTextFlag> flags) const
{
    if (flags.contains(AsTextFlag::IncludeResourceIdentifiers))
        ts.dumpProperty("image-buffer-identifier"_s, m_imageBuffer->renderingResourceIdentifier());
    ts.dumpProperty("source-rect"_s, source());
    ts.dumpProperty("dest-rect"_s, destinationRect());
}

void DrawNativeImage::apply(GraphicsContext& context) const
{
    context.drawNativeImage(m_image, m_destinationRect, m_srcRect, m_options);
}

void DrawNativeImage::dump(TextStream& ts, OptionSet<AsTextFlag> flags) const
{
    if (flags.contains(AsTextFlag::IncludeResourceIdentifiers))
        ts.dumpProperty("image-identifier"_s, m_image->renderingResourceIdentifier());
    ts.dumpProperty("source-rect"_s, source());
    ts.dumpProperty("dest-rect"_s, destinationRect());
}

void DrawSystemImage::apply(GraphicsContext& context) const
{
    context.drawSystemImage(m_systemImage, m_destinationRect);
}

void DrawSystemImage::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    // FIXME: dump more stuff.
    ts.dumpProperty("destination"_s, destinationRect());
}

void DrawPatternNativeImage::apply(GraphicsContext& context) const
{
    context.drawPattern(m_image, m_destination, m_tileRect, m_patternTransform, m_phase, m_spacing, m_options);
}

void DrawPatternNativeImage::dump(TextStream& ts, OptionSet<AsTextFlag> flags) const
{
    if (flags.contains(AsTextFlag::IncludeResourceIdentifiers))
        ts.dumpProperty("image-identifier"_s, m_image->renderingResourceIdentifier());
    ts.dumpProperty("pattern-transform"_s, patternTransform());
    ts.dumpProperty("tile-rect"_s, tileRect());
    ts.dumpProperty("dest-rect"_s, destRect());
    ts.dumpProperty("phase"_s, phase());
    ts.dumpProperty("spacing"_s, spacing());
}

void DrawPatternImageBuffer::apply(GraphicsContext& context) const
{
    context.drawPattern(m_imageBuffer, m_destination, m_tileRect, m_patternTransform, m_phase, m_spacing, m_options);
}

void DrawPatternImageBuffer::dump(TextStream& ts, OptionSet<AsTextFlag> flags) const
{
    if (flags.contains(AsTextFlag::IncludeResourceIdentifiers))
        ts.dumpProperty("image-identifier"_s, m_imageBuffer->renderingResourceIdentifier());
    ts.dumpProperty("pattern-transform"_s, patternTransform());
    ts.dumpProperty("tile-rect"_s, tileRect());
    ts.dumpProperty("dest-rect"_s, destRect());
    ts.dumpProperty("phase"_s, phase());
    ts.dumpProperty("spacing"_s, spacing());
}

void DrawRect::apply(GraphicsContext& context) const
{
    context.drawRect(m_rect, m_borderThickness);
}

void DrawRect::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect"_s, rect());
    ts.dumpProperty("border-thickness"_s, borderThickness());
}

void DrawLine::apply(GraphicsContext& context) const
{
    context.drawLine(m_point1, m_point2);
}

void DrawLine::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("point-1"_s, point1());
    ts.dumpProperty("point-2"_s, point2());
}

DrawLinesForText::DrawLinesForText(const FloatPoint& point, std::span<const FloatSegment> lineSegments, float thickness, bool printing, bool doubleLines, StrokeStyle style)
    : m_point(point)
    , m_lineSegments(lineSegments)
    , m_thickness(thickness)
    , m_printing(printing)
    , m_doubleLines(doubleLines)
    , m_style(style)
{
}

void DrawLinesForText::apply(GraphicsContext& context) const
{
    context.drawLinesForText(m_point, m_thickness, m_lineSegments, m_printing, m_doubleLines, m_style);
}

void DrawLinesForText::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("point"_s, point());
    ts.dumpProperty("thickness"_s, thickness());
    ts.dumpProperty("double"_s, doubleLines());
    ts.dumpProperty("lineSegments"_s, lineSegments());
    ts.dumpProperty("is-printing"_s, isPrinting());
    ts.dumpProperty("double"_s, doubleLines());
}

void DrawDotsForDocumentMarker::apply(GraphicsContext& context) const
{
    context.drawDotsForDocumentMarker(m_rect, m_style);
}

void DrawDotsForDocumentMarker::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect"_s, rect());
}

void DrawEllipse::apply(GraphicsContext& context) const
{
    context.drawEllipse(m_rect);
}

void DrawEllipse::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect"_s, rect());
}

void DrawPath::apply(GraphicsContext& context) const
{
    context.drawPath(m_path);
}

void DrawPath::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("path"_s, path());
}

void DrawFocusRingPath::apply(GraphicsContext& context) const
{
    context.drawFocusRing(m_path, m_outlineWidth, m_color);
}

void DrawFocusRingPath::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("path"_s, path());
    ts.dumpProperty("outline-width"_s, outlineWidth());
    ts.dumpProperty("color"_s, color());
}

void DrawFocusRingRects::apply(GraphicsContext& context) const
{
    context.drawFocusRing(m_rects, m_outlineOffset, m_outlineWidth, m_color);
}

void DrawFocusRingRects::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rects"_s, rects());
    ts.dumpProperty("outline-offset"_s, outlineOffset());
    ts.dumpProperty("outline-width"_s, outlineWidth());
    ts.dumpProperty("color"_s, color());
}

void FillRect::apply(GraphicsContext& context) const
{
    context.fillRect(m_rect, m_requiresClipToRect);
}

void FillRect::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect"_s, rect());
    ts.dumpProperty("requiresClipToRect"_s, m_requiresClipToRect == GraphicsContext::RequiresClipToRect::Yes);
}

void FillRectWithColor::apply(GraphicsContext& context) const
{
    context.fillRect(m_rect, m_color);
}

void FillRectWithColor::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect"_s, rect());
    ts.dumpProperty("color"_s, color());
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
    ts.dumpProperty("rect"_s, rect());
    ts.dumpProperty("gradient"_s, m_gradient);
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
    context.fillRect(m_rect, m_gradient, m_gradientSpaceTransform, m_requiresClipToRect);
}

void FillRectWithGradientAndSpaceTransform::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    // FIXME: log gradient.
    ts.dumpProperty("rect"_s, rect());
    ts.dumpProperty("gradient-space-transform"_s, gradientSpaceTransform());
    ts.dumpProperty("requiresClipToRect"_s, m_requiresClipToRect == GraphicsContext::RequiresClipToRect::Yes);
}

void FillCompositedRect::apply(GraphicsContext& context) const
{
    context.fillRect(m_rect, m_color, m_op, m_blendMode);
}

void FillCompositedRect::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect"_s, rect());
    ts.dumpProperty("color"_s, color());
    ts.dumpProperty("composite-operation"_s, compositeOperator());
    ts.dumpProperty("blend-mode"_s, blendMode());
}

void FillRoundedRect::apply(GraphicsContext& context) const
{
    context.fillRoundedRect(m_rect, m_color, m_blendMode);
}

void FillRoundedRect::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect"_s, roundedRect());
    ts.dumpProperty("color"_s, color());
    ts.dumpProperty("blend-mode"_s, blendMode());
}

void FillRectWithRoundedHole::apply(GraphicsContext& context) const
{
    context.fillRectWithRoundedHole(m_rect, m_roundedHoleRect, m_color);
}

void FillRectWithRoundedHole::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect"_s, rect());
    ts.dumpProperty("rounded-hole-rect"_s, roundedHoleRect());
    ts.dumpProperty("color"_s, color());
}

void FillPath::apply(GraphicsContext& context) const
{
    context.fillPath(m_path);
}

void FillPath::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("path"_s, path());
}

void FillEllipse::apply(GraphicsContext& context) const
{
    context.fillEllipse(m_rect);
}

void FillEllipse::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect"_s, rect());
}

void StrokeRect::apply(GraphicsContext& context) const
{
    context.strokeRect(m_rect, m_lineWidth);
}

void StrokeRect::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect"_s, rect());
    ts.dumpProperty("line-width"_s, lineWidth());
}

void StrokePath::apply(GraphicsContext& context) const
{
    context.strokePath(m_path);
}

void StrokePath::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("path"_s, path());
}

void StrokeEllipse::apply(GraphicsContext& context) const
{
    context.strokeEllipse(m_rect);
}

void StrokeEllipse::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect"_s, rect());
}

void ClearRect::apply(GraphicsContext& context) const
{
    context.clearRect(m_rect);
}

void ClearRect::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("rect"_s, rect());
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
    ts.dumpProperty("type"_s, type());
    ts.dumpProperty("border-rect"_s, borderRect());
    ts.dumpProperty("device-scale-factor"_s, deviceScaleFactor());
    ts.dumpProperty("style"_s, style());
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
    ts.dumpProperty("opacity"_s, opacity());
}

void BeginTransparencyLayerWithCompositeMode::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("composite-operator"_s, compositeMode().operation);
    ts.dumpProperty("blend-mode"_s, compositeMode().blendMode);
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
    ts.dumpProperty("scale-factor"_s, scaleFactor());
}

void BeginPage::apply(GraphicsContext& context) const
{
    context.beginPage(m_pageRect);
}

void BeginPage::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("page-rect"_s, pageRect());
}

void EndPage::apply(GraphicsContext& context) const
{
    context.endPage();
}

void SetURLForRect::apply(GraphicsContext& context) const
{
    context.setURLForRect(m_link, m_destRect);
}

void SetURLForRect::dump(TextStream& ts, OptionSet<AsTextFlag>) const
{
    ts.dumpProperty("link"_s, link());
    ts.dumpProperty("dest_rect"_s, destRect());
}

} // namespace DisplayList
} // namespace WebCore
