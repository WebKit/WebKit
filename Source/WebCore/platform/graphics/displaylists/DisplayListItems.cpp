/*
 * Copyright (C) 2015-2022 Apple Inc. All rights reserved.
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

#include "DisplayListReplayer.h"
#include "Filter.h"
#include "FontCascade.h"
#include "ImageBuffer.h"
#include "MediaPlayer.h"
#include "SharedBuffer.h"
#include <wtf/text/TextStream.h>

#if USE(CG)
#include "GraphicsContextPlatformPrivateCG.h"
#endif

namespace WebCore {
namespace DisplayList {

// Should match RenderTheme::platformFocusRingWidth()
static const float platformFocusRingWidth = 3;

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

void Rotate::apply(GraphicsContext& context) const
{
    context.rotate(m_angle);
}

void Scale::apply(GraphicsContext& context) const
{
    context.scale(m_size);
}

void SetCTM::apply(GraphicsContext& context) const
{
    context.setCTM(m_transform);
}

void ConcatenateCTM::apply(GraphicsContext& context) const
{
    context.concatCTM(m_transform);
}

void SetInlineFillColor::apply(GraphicsContext& context) const
{
    context.setFillColor(color());
}

void SetInlineStrokeColor::apply(GraphicsContext& context) const
{
    context.setStrokeColor(color());
}

void SetStrokeThickness::apply(GraphicsContext& context) const
{
    context.setStrokeThickness(m_thickness);
}

SetState::SetState(const GraphicsContextState& state)
    : m_state(state)
{
}

void SetState::apply(GraphicsContext& context)
{
    context.updateState(m_state);
}

void SetLineCap::apply(GraphicsContext& context) const
{
    context.setLineCap(m_lineCap);
}

void SetLineDash::apply(GraphicsContext& context) const
{
    context.setLineDash(m_dashArray, m_dashOffset);
}

void SetLineJoin::apply(GraphicsContext& context) const
{
    context.setLineJoin(m_lineJoin);
}

void SetMiterLimit::apply(GraphicsContext& context) const
{
    context.setMiterLimit(m_miterLimit);
}

void ClearShadow::apply(GraphicsContext& context) const
{
    context.clearShadow();
}

void Clip::apply(GraphicsContext& context) const
{
    context.clip(m_rect);
}

void ClipOut::apply(GraphicsContext& context) const
{
    context.clipOut(m_rect);
}

NO_RETURN_DUE_TO_ASSERT void ClipToImageBuffer::apply(GraphicsContext&) const
{
    ASSERT_NOT_REACHED();
}

void ClipToImageBuffer::apply(GraphicsContext& context, WebCore::ImageBuffer& imageBuffer) const
{
    context.clipToImageBuffer(imageBuffer, m_destinationRect);
}

void ClipOutToPath::apply(GraphicsContext& context) const
{
    context.clipOut(m_path);
}

void ClipPath::apply(GraphicsContext& context) const
{
    context.clipPath(m_path, m_windRule);
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

void DrawFilteredImageBuffer::apply(GraphicsContext& context, ImageBuffer* sourceImage, FilterResults& results)
{
    context.drawFilteredImageBuffer(sourceImage, m_sourceImageRect, m_filter, results);
}

DrawGlyphs::DrawGlyphs(RenderingResourceIdentifier fontIdentifier, Vector<GlyphBufferGlyph, 128>&& glyphs, Vector<GlyphBufferAdvance, 128>&& advances, const FloatRect& bounds, const FloatPoint& localAnchor, FontSmoothingMode smoothingMode)
    : m_fontIdentifier(fontIdentifier)
    , m_glyphs(WTFMove(glyphs))
    , m_advances(WTFMove(advances))
    , m_bounds(bounds)
    , m_localAnchor(localAnchor)
    , m_smoothingMode(smoothingMode)
{
}

DrawGlyphs::DrawGlyphs(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned count, const FloatPoint& localAnchor, FontSmoothingMode smoothingMode)
    : m_fontIdentifier(font.renderingResourceIdentifier())
    , m_localAnchor(localAnchor)
    , m_smoothingMode(smoothingMode)
{
    m_glyphs.reserveInitialCapacity(count);
    m_advances.reserveInitialCapacity(count);
    for (unsigned i = 0; i < count; ++i) {
        m_glyphs.uncheckedAppend(glyphs[i]);
        m_advances.uncheckedAppend(advances[i]);
    }
    computeBounds(font);
}

void DrawGlyphs::apply(GraphicsContext& context, const Font& font) const
{
    context.drawGlyphs(font, m_glyphs.data(), m_advances.data(), m_glyphs.size(), anchorPoint(), m_smoothingMode);
}

void DrawGlyphs::computeBounds(const Font& font)
{
    // FIXME: This code doesn't actually take the extents of the glyphs into consideration. It assumes that
    // the glyph lies entirely within its [(ascent + descent), advance] rect.
    float ascent = font.fontMetrics().floatAscent();
    float descent = font.fontMetrics().floatDescent();
    FloatPoint current = localAnchor();
    size_t numGlyphs = m_glyphs.size();
    for (size_t i = 0; i < numGlyphs; ++i) {
        GlyphBufferAdvance advance = m_advances[i];
        FloatRect glyphRect = FloatRect(current.x(), current.y() - ascent, width(advance), ascent + descent);
        m_bounds.unite(glyphRect);

        current.move(width(advance), height(advance));
    }
}

NO_RETURN_DUE_TO_ASSERT void DrawImageBuffer::apply(GraphicsContext&) const
{
    ASSERT_NOT_REACHED();
}

void DrawImageBuffer::apply(GraphicsContext& context, WebCore::ImageBuffer& imageBuffer) const
{
    context.drawImageBuffer(imageBuffer, m_destinationRect, m_srcRect, m_options);
}

NO_RETURN_DUE_TO_ASSERT void DrawNativeImage::apply(GraphicsContext&) const
{
    ASSERT_NOT_REACHED();
}

void DrawNativeImage::apply(GraphicsContext& context, NativeImage& image) const
{
    context.drawNativeImage(image, m_imageSize, m_destinationRect, m_srcRect, m_options);
}

void DrawSystemImage::apply(GraphicsContext& context) const
{
    context.drawSystemImage(m_systemImage, m_destinationRect);
}

DrawPattern::DrawPattern(RenderingResourceIdentifier imageIdentifier, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
    : m_imageIdentifier(imageIdentifier)
    , m_destination(destRect)
    , m_tileRect(tileRect)
    , m_patternTransform(patternTransform)
    , m_phase(phase)
    , m_spacing(spacing)
    , m_options(options)
{
}

NO_RETURN_DUE_TO_ASSERT void DrawPattern::apply(GraphicsContext&) const
{
    ASSERT_NOT_REACHED();
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

void DrawRect::apply(GraphicsContext& context) const
{
    context.drawRect(m_rect, m_borderThickness);
}

std::optional<FloatRect> DrawLine::localBounds(const GraphicsContext&) const
{
    FloatRect bounds;
    bounds.fitToPoints(m_point1, m_point2);
    return bounds;
}

void DrawLine::apply(GraphicsContext& context) const
{
    context.drawLine(m_point1, m_point2);
}

DrawLinesForText::DrawLinesForText(const FloatPoint& blockLocation, const FloatSize& localAnchor, float thickness, const DashArray& widths, bool printing, bool doubleLines, StrokeStyle style)
    : m_blockLocation(blockLocation)
    , m_localAnchor(localAnchor)
    , m_widths(widths)
    , m_thickness(thickness)
    , m_printing(printing)
    , m_doubleLines(doubleLines)
    , m_style(style)
{
}

void DrawLinesForText::apply(GraphicsContext& context) const
{
    context.drawLinesForText(point(), m_thickness, m_widths, m_printing, m_doubleLines, m_style);
}

std::optional<FloatRect> DrawLinesForText::localBounds(const GraphicsContext&) const
{
    // This function needs to return a value equal to or enclosing what GraphicsContext::computeLineBoundsAndAntialiasingModeForText() returns.

    if (!m_widths.size())
        return FloatRect();

    FloatRect result(point(), FloatSize(m_widths.last(), m_thickness));
    result.inflate(1); // Account for pixel snapping. FIXME: This isn't perfect, as it doesn't take the CTM into account.
    return result;
}

void DrawDotsForDocumentMarker::apply(GraphicsContext& context) const
{
    context.drawDotsForDocumentMarker(m_rect, {
        static_cast<DocumentMarkerLineStyle::Mode>(m_styleMode),
        m_styleShouldUseDarkAppearance,
    });
}

std::optional<FloatRect> DrawDotsForDocumentMarker::localBounds(const GraphicsContext&) const
{
    return m_rect;
}

void DrawEllipse::apply(GraphicsContext& context) const
{
    context.drawEllipse(m_rect);
}

void DrawPath::apply(GraphicsContext& context) const
{
    context.drawPath(m_path);
}

void DrawFocusRingPath::apply(GraphicsContext& context) const
{
    context.drawFocusRing(m_path, m_width, m_offset, m_color);
}

std::optional<FloatRect> DrawFocusRingPath::localBounds(const GraphicsContext&) const
{
    FloatRect result = m_path.fastBoundingRect();
    result.inflate(platformFocusRingWidth);
    return result;
}

DrawFocusRingRects::DrawFocusRingRects(const Vector<FloatRect>& rects, float width, float offset, const Color& color)
    : m_rects(rects)
    , m_width(width)
    , m_offset(offset)
    , m_color(color)
{
}

void DrawFocusRingRects::apply(GraphicsContext& context) const
{
    context.drawFocusRing(m_rects, m_width, m_offset, m_color);
}

std::optional<FloatRect> DrawFocusRingRects::localBounds(const GraphicsContext&) const
{
    FloatRect result;
    for (auto& rect : m_rects)
        result.unite(rect);
    result.inflate(platformFocusRingWidth);
    return result;
}

void FillRect::apply(GraphicsContext& context) const
{
    context.fillRect(m_rect);
}

void FillRectWithColor::apply(GraphicsContext& context) const
{
    context.fillRect(m_rect, m_color);
}

FillRectWithGradient::FillRectWithGradient(const FloatRect& rect, Gradient& gradient)
    : m_rect(rect)
    , m_gradient(gradient)
{
}

void FillRectWithGradient::apply(GraphicsContext& context) const
{
    context.fillRect(m_rect, m_gradient.get());
}

void FillCompositedRect::apply(GraphicsContext& context) const
{
    context.fillRect(m_rect, m_color, m_op, m_blendMode);
}

void FillRoundedRect::apply(GraphicsContext& context) const
{
    context.fillRoundedRect(m_rect, m_color, m_blendMode);
}

void FillRectWithRoundedHole::apply(GraphicsContext& context) const
{
    context.fillRectWithRoundedHole(m_rect, m_roundedHoleRect, m_color);
}

#if ENABLE(INLINE_PATH_DATA)

void FillLine::apply(GraphicsContext& context) const
{
    context.fillPath(path());
}

void FillArc::apply(GraphicsContext& context) const
{
    context.fillPath(path());
}

void FillQuadCurve::apply(GraphicsContext& context) const
{
    context.fillPath(path());
}

void FillBezierCurve::apply(GraphicsContext& context) const
{
    context.fillPath(path());
}

#endif // ENABLE(INLINE_PATH_DATA)

void FillPath::apply(GraphicsContext& context) const
{
    context.fillPath(m_path);
}

void FillEllipse::apply(GraphicsContext& context) const
{
    context.fillEllipse(m_rect);
}

#if ENABLE(VIDEO)
PaintFrameForMedia::PaintFrameForMedia(MediaPlayer& player, const FloatRect& destination)
    : m_identifier(player.identifier())
    , m_destination(destination)
{
}

NO_RETURN_DUE_TO_ASSERT void PaintFrameForMedia::apply(GraphicsContext&) const
{
    // Should be handled by the delegate.
    ASSERT_NOT_REACHED();
}

#endif

std::optional<FloatRect> StrokeRect::localBounds(const GraphicsContext&) const
{
    FloatRect bounds = m_rect;
    bounds.expand(m_lineWidth, m_lineWidth);
    return bounds;
}

void StrokeRect::apply(GraphicsContext& context) const
{
    context.strokeRect(m_rect, m_lineWidth);
}

std::optional<FloatRect> StrokePath::localBounds(const GraphicsContext& context) const
{
    // FIXME: Need to take stroke thickness into account correctly, via CGPathByStrokingPath().
    float strokeThickness = context.strokeThickness();

    FloatRect bounds = m_path.fastBoundingRect();
    bounds.expand(strokeThickness, strokeThickness);
    return bounds;
}

void StrokePath::apply(GraphicsContext& context) const
{
    context.strokePath(m_path);
}

std::optional<FloatRect> StrokeEllipse::localBounds(const GraphicsContext& context) const
{
    float strokeThickness = context.strokeThickness();

    FloatRect bounds = m_rect;
    bounds.expand(strokeThickness, strokeThickness);
    return bounds;
}

void StrokeEllipse::apply(GraphicsContext& context) const
{
    context.strokeEllipse(m_rect);
}

std::optional<FloatRect> StrokeLine::localBounds(const GraphicsContext& context) const
{
    float strokeThickness = context.strokeThickness();

    FloatRect bounds;
    bounds.fitToPoints(start(), end());
    bounds.expand(strokeThickness, strokeThickness);
    return bounds;
}

void StrokeLine::apply(GraphicsContext& context) const
{
#if ENABLE(INLINE_PATH_DATA)
    auto path = Path::from(InlinePathData { LineData { start(), end() } });
#else
    Path path;
    path.moveTo(start());
    path.addLineTo(end());
#endif
    context.strokePath(path);
}

#if ENABLE(INLINE_PATH_DATA)

std::optional<FloatRect> StrokeArc::localBounds(const GraphicsContext& context) const
{
    // FIXME: Need to take stroke thickness into account correctly, via CGPathByStrokingPath().
    float strokeThickness = context.strokeThickness();

    auto bounds = path().fastBoundingRect();
    bounds.expand(strokeThickness, strokeThickness);
    return bounds;
}

void StrokeArc::apply(GraphicsContext& context) const
{
    context.strokePath(path());
}

std::optional<FloatRect> StrokeQuadCurve::localBounds(const GraphicsContext& context) const
{
    // FIXME: Need to take stroke thickness into account correctly, via CGPathByStrokingPath().
    float strokeThickness = context.strokeThickness();

    auto bounds = path().fastBoundingRect();
    bounds.expand(strokeThickness, strokeThickness);
    return bounds;
}

void StrokeQuadCurve::apply(GraphicsContext& context) const
{
    context.strokePath(path());
}

std::optional<FloatRect> StrokeBezierCurve::localBounds(const GraphicsContext& context) const
{
    // FIXME: Need to take stroke thickness into account correctly, via CGPathByStrokingPath().
    float strokeThickness = context.strokeThickness();

    auto bounds = path().fastBoundingRect();
    bounds.expand(strokeThickness, strokeThickness);
    return bounds;
}

void StrokeBezierCurve::apply(GraphicsContext& context) const
{
    context.strokePath(path());
}

#endif // ENABLE(INLINE_PATH_DATA)

void ClearRect::apply(GraphicsContext& context) const
{
    context.clearRect(m_rect);
}

void BeginTransparencyLayer::apply(GraphicsContext& context) const
{
    context.beginTransparencyLayer(m_opacity);
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

#if !LOG_DISABLED
static TextStream& operator<<(TextStream& ts, ItemType type)
{
    switch (type) {
    case ItemType::Save: ts << "save"; break;
    case ItemType::Restore: ts << "restore"; break;
    case ItemType::Translate: ts << "translate"; break;
    case ItemType::Rotate: ts << "rotate"; break;
    case ItemType::Scale: ts << "scale"; break;
    case ItemType::SetCTM: ts << "set-ctm"; break;
    case ItemType::ConcatenateCTM: ts << "concatentate-ctm"; break;
    case ItemType::SetInlineFillColor: ts << "set-inline-fill-color"; break;
    case ItemType::SetInlineStrokeColor: ts << "set-inline-stroke-color"; break;
    case ItemType::SetStrokeThickness: ts << "set-stroke-thickness"; break;
    case ItemType::SetState: ts << "set-state"; break;
    case ItemType::SetLineCap: ts << "set-line-cap"; break;
    case ItemType::SetLineDash: ts << "set-line-dash"; break;
    case ItemType::SetLineJoin: ts << "set-line-join"; break;
    case ItemType::SetMiterLimit: ts << "set-miter-limit"; break;
    case ItemType::Clip: ts << "clip"; break;
    case ItemType::ClipOut: ts << "clip-out"; break;
    case ItemType::ClipToImageBuffer: ts << "clip-to-image-buffer"; break;
    case ItemType::ClipOutToPath: ts << "clip-out-to-path"; break;
    case ItemType::ClipPath: ts << "clip-path"; break;
    case ItemType::DrawFilteredImageBuffer: ts << "draw-filtered-image-buffer"; break;
    case ItemType::DrawGlyphs: ts << "draw-glyphs"; break;
    case ItemType::DrawImageBuffer: ts << "draw-image-buffer"; break;
    case ItemType::DrawNativeImage: ts << "draw-native-image"; break;
    case ItemType::DrawSystemImage: ts << "draw-system-image"; break;
    case ItemType::DrawPattern: ts << "draw-pattern"; break;
    case ItemType::DrawRect: ts << "draw-rect"; break;
    case ItemType::DrawLine: ts << "draw-line"; break;
    case ItemType::DrawLinesForText: ts << "draw-lines-for-text"; break;
    case ItemType::DrawDotsForDocumentMarker: ts << "draw-dots-for-document-marker"; break;
    case ItemType::DrawEllipse: ts << "draw-ellipse"; break;
    case ItemType::DrawPath: ts << "draw-path"; break;
    case ItemType::DrawFocusRingPath: ts << "draw-focus-ring-path"; break;
    case ItemType::DrawFocusRingRects: ts << "draw-focus-ring-rects"; break;
    case ItemType::FillRect: ts << "fill-rect"; break;
    case ItemType::FillRectWithColor: ts << "fill-rect-with-color"; break;
    case ItemType::FillRectWithGradient: ts << "fill-rect-with-gradient"; break;
    case ItemType::FillCompositedRect: ts << "fill-composited-rect"; break;
    case ItemType::FillRoundedRect: ts << "fill-rounded-rect"; break;
    case ItemType::FillRectWithRoundedHole: ts << "fill-rect-with-rounded-hole"; break;
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::FillLine: ts << "fill-line"; break;
    case ItemType::FillArc: ts << "fill-arc"; break;
    case ItemType::FillQuadCurve: ts << "fill-quad-curve"; break;
    case ItemType::FillBezierCurve: ts << "fill-bezier-curve"; break;
#endif
    case ItemType::FillPath: ts << "fill-path"; break;
    case ItemType::FillEllipse: ts << "fill-ellipse"; break;
#if ENABLE(VIDEO)
    case ItemType::PaintFrameForMedia: ts << "paint-frame-for-media"; break;
#endif
    case ItemType::StrokeRect: ts << "stroke-rect"; break;
    case ItemType::StrokeLine: ts << "stroke-line"; break;
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::StrokeArc: ts << "stroke-arc"; break;
    case ItemType::StrokeQuadCurve: ts << "stroke-quad-curve"; break;
    case ItemType::StrokeBezierCurve: ts << "stroke-bezier-curve"; break;
#endif
    case ItemType::StrokePath: ts << "stroke-path"; break;
    case ItemType::StrokeEllipse: ts << "stroke-ellipse"; break;
    case ItemType::ClearRect: ts << "clear-rect"; break;
    case ItemType::BeginTransparencyLayer: ts << "begin-transparency-layer"; break;
    case ItemType::EndTransparencyLayer: ts << "end-transparency-layer"; break;
#if USE(CG)
    case ItemType::ApplyStrokePattern: ts << "apply-stroke-pattern"; break;
    case ItemType::ApplyFillPattern: ts << "apply-fill-pattern"; break;
#endif
    case ItemType::ApplyDeviceScaleFactor: ts << "apply-device-scale-factor"; break;
    case ItemType::ClearShadow: ts << "clear-shadow"; break;
    }
    return ts;
}

void dumpItem(TextStream& ts, const Translate& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("x", item.x());
    ts.dumpProperty("y", item.y());
}

void dumpItem(TextStream& ts, const Rotate& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("angle", item.angle());
}

void dumpItem(TextStream& ts, const Scale& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("size", item.amount());
}

void dumpItem(TextStream& ts, const SetCTM& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("set-ctm", item.transform());
}

void dumpItem(TextStream& ts, const ConcatenateCTM& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("ctm", item.transform());
}

void dumpItem(TextStream& ts, const SetInlineFillColor& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("color", item.color());
}

void dumpItem(TextStream& ts, const SetInlineStrokeColor& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("color", item.color());
}

void dumpItem(TextStream& ts, const SetStrokeThickness& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("thickness", item.thickness());
}

void dumpItem(TextStream& ts, const SetState& item, OptionSet<AsTextFlag>)
{
    ts << item.state();
}

void dumpItem(TextStream& ts, const SetLineCap& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("line-cap", item.lineCap());
}

void dumpItem(TextStream& ts, const SetLineDash& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("dash-array", item.dashArray());
    ts.dumpProperty("dash-offset", item.dashOffset());
}

void dumpItem(TextStream& ts, const SetLineJoin& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("line-join", item.lineJoin());
}

void dumpItem(TextStream& ts, const SetMiterLimit& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("mitre-limit", item.miterLimit());
}

void dumpItem(TextStream& ts, const Clip& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("rect", item.rect());
}

void dumpItem(TextStream& ts, const ClipOut& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("rect", item.rect());
}

void dumpItem(TextStream& ts, const ClipToImageBuffer& item, OptionSet<AsTextFlag> flags)
{
    if (!flags.contains(AsTextFlag::IncludesResourceIdentifiers))
        ts.dumpProperty("image-buffer-identifier", item.imageBufferIdentifier());
    ts.dumpProperty("dest-rect", item.destinationRect());
}

void dumpItem(TextStream& ts, const ClipOutToPath& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("path", item.path());
}

void dumpItem(TextStream& ts, const ClipPath& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("path", item.path());
    ts.dumpProperty("wind-rule", item.windRule());
}

void dumpItem(TextStream& ts, const DrawFilteredImageBuffer& item, OptionSet<AsTextFlag> flags)
{
    if (!flags.contains(AsTextFlag::IncludesResourceIdentifiers))
        ts.dumpProperty("source-image-identifier", item.sourceImageIdentifier());
    ts.dumpProperty("source-image-rect", item.sourceImageRect());
}

void dumpItem(TextStream& ts, const DrawGlyphs& item, OptionSet<AsTextFlag>)
{
    // FIXME: dump more stuff.
    ts.dumpProperty("local-anchor", item.localAnchor());
    ts.dumpProperty("anchor-point", item.anchorPoint());
    ts.dumpProperty("length", item.glyphs().size());

}

void dumpItem(TextStream& ts, const DrawImageBuffer& item, OptionSet<AsTextFlag> flags)
{
    if (!flags.contains(AsTextFlag::IncludesResourceIdentifiers))
        ts.dumpProperty("image-buffer-identifier", item.imageBufferIdentifier());
    ts.dumpProperty("source-rect", item.source());
    ts.dumpProperty("dest-rect", item.destinationRect());
}

void dumpItem(TextStream& ts, const DrawNativeImage& item, OptionSet<AsTextFlag> flags)
{
    if (!flags.contains(AsTextFlag::IncludesResourceIdentifiers))
        ts.dumpProperty("image-identifier", item.imageIdentifier());
    ts.dumpProperty("source-rect", item.source());
    ts.dumpProperty("dest-rect", item.destinationRect());
}

void dumpItem(TextStream& ts, const DrawSystemImage& item, OptionSet<AsTextFlag>)
{
    // FIXME: dump more stuff.
    ts.dumpProperty("destination", item.destinationRect());
}

void dumpItem(TextStream& ts, const DrawPattern& item, OptionSet<AsTextFlag> flags)
{
    if (!flags.contains(AsTextFlag::IncludesResourceIdentifiers))
        ts.dumpProperty("image-identifier", item.imageIdentifier());
    ts.dumpProperty("pattern-transform", item.patternTransform());
    ts.dumpProperty("tile-rect", item.tileRect());
    ts.dumpProperty("dest-rect", item.destRect());
    ts.dumpProperty("phase", item.phase());
    ts.dumpProperty("spacing", item.spacing());
}

void dumpItem(TextStream& ts, const DrawRect& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("rect", item.rect());
    ts.dumpProperty("border-thickness", item.borderThickness());
}

void dumpItem(TextStream& ts, const DrawLine& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("point-1", item.point1());
    ts.dumpProperty("point-2", item.point2());
}

void dumpItem(TextStream& ts, const DrawLinesForText& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("block-location", item.blockLocation());
    ts.dumpProperty("local-anchor", item.localAnchor());
    ts.dumpProperty("point", item.point());
    ts.dumpProperty("thickness", item.thickness());
    ts.dumpProperty("double", item.doubleLines());
    ts.dumpProperty("widths", item.widths());
    ts.dumpProperty("is-printing", item.isPrinting());
    ts.dumpProperty("double", item.doubleLines());
}

void dumpItem(TextStream& ts, const DrawDotsForDocumentMarker& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("rect", item.rect());
}

void dumpItem(TextStream& ts, const DrawEllipse& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("rect", item.rect());
}

void dumpItem(TextStream& ts, const DrawPath& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("path", item.path());
}

void dumpItem(TextStream& ts, const DrawFocusRingPath& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("path", item.path());
    ts.dumpProperty("width", item.width());
    ts.dumpProperty("offset", item.offset());
    ts.dumpProperty("color", item.color());
}

void dumpItem(TextStream& ts, const DrawFocusRingRects& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("rects", item.rects());
    ts.dumpProperty("width", item.width());
    ts.dumpProperty("offset", item.offset());
    ts.dumpProperty("color", item.color());
}

void dumpItem(TextStream& ts, const FillRect& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("rect", item.rect());
}

void dumpItem(TextStream& ts, const FillRectWithColor& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("rect", item.rect());
    ts.dumpProperty("color", item.color());
}

void dumpItem(TextStream& ts, const FillRectWithGradient& item, OptionSet<AsTextFlag>)
{
    // FIXME: log gradient.
    ts.dumpProperty("rect", item.rect());
}

void dumpItem(TextStream& ts, const FillCompositedRect& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("rect", item.rect());
    ts.dumpProperty("color", item.color());
    ts.dumpProperty("composite-operation", item.compositeOperator());
    ts.dumpProperty("blend-mode", item.blendMode());
}

void dumpItem(TextStream& ts, const FillRoundedRect& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("rect", item.roundedRect());
    ts.dumpProperty("color", item.color());
    ts.dumpProperty("blend-mode", item.blendMode());
}

void dumpItem(TextStream& ts, const FillRectWithRoundedHole& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("rect", item.rect());
    ts.dumpProperty("rounded-hole-rect", item.roundedHoleRect());
    ts.dumpProperty("color", item.color());
}

#if ENABLE(INLINE_PATH_DATA)

void dumpItem(TextStream& ts, const FillLine& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("path", item.path());
}

void dumpItem(TextStream& ts, const FillArc& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("path", item.path());
}

void dumpItem(TextStream& ts, const FillQuadCurve& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("path", item.path());
}

void dumpItem(TextStream& ts, const FillBezierCurve& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("path", item.path());
}

void dumpItem(TextStream& ts, const StrokeArc& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("path", item.path());
}

void dumpItem(TextStream& ts, const StrokeQuadCurve& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("path", item.path());
}

void dumpItem(TextStream& ts, const StrokeBezierCurve& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("path", item.path());
}

#endif // ENABLE(INLINE_PATH_DATA)

void dumpItem(TextStream& ts, const FillPath& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("path", item.path());
}

void dumpItem(TextStream& ts, const FillEllipse& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("rect", item.rect());
}

#if ENABLE(VIDEO)

void dumpItem(TextStream& ts, const PaintFrameForMedia& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("destination", item.destination());
}

#endif // ENABLE(VIDEO)


void dumpItem(TextStream& ts, const StrokeRect& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("rect", item.rect());
    ts.dumpProperty("line-width", item.lineWidth());
}

void dumpItem(TextStream& ts, const StrokePath& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("path", item.path());
}

void dumpItem(TextStream& ts, const StrokeEllipse& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("rect", item.rect());
}

void dumpItem(TextStream& ts, const StrokeLine& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("start", item.start());
    ts.dumpProperty("end", item.end());
}

void dumpItem(TextStream& ts, const ClearRect& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("rect", item.rect());
}

void dumpItem(TextStream& ts, const BeginTransparencyLayer& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("opacity", item.opacity());
}

void dumpItem(TextStream& ts, const ApplyDeviceScaleFactor& item, OptionSet<AsTextFlag>)
{
    ts.dumpProperty("scale-factor", item.scaleFactor());
}

void dumpItemHandle(TextStream& ts, const ItemHandle& item, OptionSet<AsTextFlag> flags)
{
    ts << item.type();

    switch (item.type()) {
    case ItemType::Translate:
        dumpItem(ts, item.get<Translate>(), flags);
        break;
    case ItemType::Rotate:
        dumpItem(ts, item.get<Rotate>(), flags);
        break;
    case ItemType::Scale:
        dumpItem(ts, item.get<Scale>(), flags);
        break;
    case ItemType::SetCTM:
        dumpItem(ts, item.get<SetCTM>(), flags);
        break;
    case ItemType::ConcatenateCTM:
        dumpItem(ts, item.get<ConcatenateCTM>(), flags);
        break;
    case ItemType::SetInlineFillColor:
        dumpItem(ts, item.get<SetInlineFillColor>(), flags);
        break;
    case ItemType::SetInlineStrokeColor:
        dumpItem(ts, item.get<SetInlineStrokeColor>(), flags);
        break;
    case ItemType::SetStrokeThickness:
        dumpItem(ts, item.get<SetStrokeThickness>(), flags);
        break;
    case ItemType::SetState:
        dumpItem(ts, item.get<SetState>(), flags);
        break;
    case ItemType::SetLineCap:
        dumpItem(ts, item.get<SetLineCap>(), flags);
        break;
    case ItemType::SetLineDash:
        dumpItem(ts, item.get<SetLineDash>(), flags);
        break;
    case ItemType::SetLineJoin:
        dumpItem(ts, item.get<SetLineJoin>(), flags);
        break;
    case ItemType::SetMiterLimit:
        dumpItem(ts, item.get<SetMiterLimit>(), flags);
        break;
    case ItemType::Clip:
        dumpItem(ts, item.get<Clip>(), flags);
        break;
    case ItemType::ClipOut:
        dumpItem(ts, item.get<ClipOut>(), flags);
        break;
    case ItemType::ClipToImageBuffer:
        dumpItem(ts, item.get<ClipToImageBuffer>(), flags);
        break;
    case ItemType::ClipOutToPath:
        dumpItem(ts, item.get<ClipOutToPath>(), flags);
        break;
    case ItemType::ClipPath:
        dumpItem(ts, item.get<ClipPath>(), flags);
        break;
    case ItemType::DrawFilteredImageBuffer:
        dumpItem(ts, item.get<DrawFilteredImageBuffer>(), flags);
        break;
    case ItemType::DrawGlyphs:
        dumpItem(ts, item.get<DrawGlyphs>(), flags);
        break;
    case ItemType::DrawImageBuffer:
        dumpItem(ts, item.get<DrawImageBuffer>(), flags);
        break;
    case ItemType::DrawNativeImage:
        dumpItem(ts, item.get<DrawNativeImage>(), flags);
        break;
    case ItemType::DrawSystemImage:
        dumpItem(ts, item.get<DrawSystemImage>(), flags);
        break;
    case ItemType::DrawPattern:
        dumpItem(ts, item.get<DrawPattern>(), flags);
        break;
    case ItemType::DrawRect:
        dumpItem(ts, item.get<DrawRect>(), flags);
        break;
    case ItemType::DrawLine:
        dumpItem(ts, item.get<DrawLine>(), flags);
        break;
    case ItemType::DrawLinesForText:
        dumpItem(ts, item.get<DrawLinesForText>(), flags);
        break;
    case ItemType::DrawDotsForDocumentMarker:
        dumpItem(ts, item.get<DrawDotsForDocumentMarker>(), flags);
        break;
    case ItemType::DrawEllipse:
        dumpItem(ts, item.get<DrawEllipse>(), flags);
        break;
    case ItemType::DrawPath:
        dumpItem(ts, item.get<DrawPath>(), flags);
        break;
    case ItemType::DrawFocusRingPath:
        dumpItem(ts, item.get<DrawFocusRingPath>(), flags);
        break;
    case ItemType::DrawFocusRingRects:
        dumpItem(ts, item.get<DrawFocusRingRects>(), flags);
        break;
    case ItemType::FillRect:
        dumpItem(ts, item.get<FillRect>(), flags);
        break;
    case ItemType::FillRectWithColor:
        dumpItem(ts, item.get<FillRectWithColor>(), flags);
        break;
    case ItemType::FillRectWithGradient:
        dumpItem(ts, item.get<FillRectWithGradient>(), flags);
        break;
    case ItemType::FillCompositedRect:
        dumpItem(ts, item.get<FillCompositedRect>(), flags);
        break;
    case ItemType::FillRoundedRect:
        dumpItem(ts, item.get<FillRoundedRect>(), flags);
        break;
    case ItemType::FillRectWithRoundedHole:
        dumpItem(ts, item.get<FillRectWithRoundedHole>(), flags);
        break;
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::FillLine:
        dumpItem(ts, item.get<FillLine>(), flags);
        break;
    case ItemType::FillArc:
        dumpItem(ts, item.get<FillArc>(), flags);
        break;
    case ItemType::FillQuadCurve:
        dumpItem(ts, item.get<FillQuadCurve>(), flags);
        break;
    case ItemType::FillBezierCurve:
        dumpItem(ts, item.get<FillBezierCurve>(), flags);
        break;
#endif
    case ItemType::FillPath:
        dumpItem(ts, item.get<FillPath>(), flags);
        break;
    case ItemType::FillEllipse:
        dumpItem(ts, item.get<FillEllipse>(), flags);
        break;
#if ENABLE(VIDEO)
    case ItemType::PaintFrameForMedia:
        dumpItem(ts, item.get<PaintFrameForMedia>(), flags);
        break;
#endif
    case ItemType::StrokeRect:
        dumpItem(ts, item.get<StrokeRect>(), flags);
        break;
    case ItemType::StrokeLine:
        dumpItem(ts, item.get<StrokeLine>(), flags);
        break;
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::StrokeArc:
        dumpItem(ts, item.get<StrokeArc>(), flags);
        break;
    case ItemType::StrokeQuadCurve:
        dumpItem(ts, item.get<StrokeQuadCurve>(), flags);
        break;
    case ItemType::StrokeBezierCurve:
        dumpItem(ts, item.get<StrokeBezierCurve>(), flags);
        break;
#endif
    case ItemType::StrokePath:
        dumpItem(ts, item.get<StrokePath>(), flags);
        break;
    case ItemType::StrokeEllipse:
        dumpItem(ts, item.get<StrokeEllipse>(), flags);
        break;
    case ItemType::ClearRect:
        dumpItem(ts, item.get<ClearRect>(), flags);
        break;
    case ItemType::BeginTransparencyLayer:
        dumpItem(ts, item.get<BeginTransparencyLayer>(), flags);
        break;
    case ItemType::ApplyDeviceScaleFactor:
        dumpItem(ts, item.get<ApplyDeviceScaleFactor>(), flags);
        break;
    // Items with no additional data.
    case ItemType::Save:
    case ItemType::Restore:
    case ItemType::EndTransparencyLayer:
#if USE(CG)
    case ItemType::ApplyStrokePattern:
    case ItemType::ApplyFillPattern:
#endif
    case ItemType::ClearShadow:
        break;
    }
}
#endif

} // namespace DisplayList
} // namespace WebCore
