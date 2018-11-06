/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "FontCascade.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace DisplayList {

// Should match RenderTheme::platformFocusRingWidth()
static const float platformFocusRingWidth = 3;

#if !defined(NDEBUG) || !LOG_DISABLED
WTF::CString Item::description() const
{
    TextStream ts;
    ts << *this;
    return ts.release().utf8();
}
#endif

size_t Item::sizeInBytes(const Item& item)
{
    switch (item.type()) {
    case ItemType::Save:
        return sizeof(downcast<Save>(item));
    case ItemType::Restore:
        return sizeof(downcast<Restore>(item));
    case ItemType::Translate:
        return sizeof(downcast<Translate>(item));
    case ItemType::Rotate:
        return sizeof(downcast<Rotate>(item));
    case ItemType::Scale:
        return sizeof(downcast<Scale>(item));
    case ItemType::ConcatenateCTM:
        return sizeof(downcast<ConcatenateCTM>(item));
    case ItemType::SetState:
        return sizeof(downcast<SetState>(item));
    case ItemType::SetLineCap:
        return sizeof(downcast<SetLineCap>(item));
    case ItemType::SetLineDash:
        return sizeof(downcast<SetLineDash>(item));
    case ItemType::SetLineJoin:
        return sizeof(downcast<SetLineJoin>(item));
    case ItemType::SetMiterLimit:
        return sizeof(downcast<SetMiterLimit>(item));
    case ItemType::ClearShadow:
        return sizeof(downcast<ClearShadow>(item));
    case ItemType::Clip:
        return sizeof(downcast<Clip>(item));
    case ItemType::ClipOut:
        return sizeof(downcast<ClipOut>(item));
    case ItemType::ClipOutToPath:
        return sizeof(downcast<ClipOutToPath>(item));
    case ItemType::ClipPath:
        return sizeof(downcast<ClipPath>(item));
    case ItemType::DrawGlyphs:
        return sizeof(downcast<DrawGlyphs>(item));
    case ItemType::DrawImage:
        return sizeof(downcast<DrawImage>(item));
    case ItemType::DrawTiledImage:
        return sizeof(downcast<DrawTiledImage>(item));
    case ItemType::DrawTiledScaledImage:
        return sizeof(downcast<DrawTiledScaledImage>(item));
#if USE(CG) || USE(CAIRO)
    case ItemType::DrawNativeImage:
        return sizeof(downcast<DrawNativeImage>(item));
#endif
    case ItemType::DrawPattern:
        return sizeof(downcast<DrawPattern>(item));
    case ItemType::DrawRect:
        return sizeof(downcast<DrawRect>(item));
    case ItemType::DrawLine:
        return sizeof(downcast<DrawLine>(item));
    case ItemType::DrawLinesForText:
        return sizeof(downcast<DrawLinesForText>(item));
    case ItemType::DrawDotsForDocumentMarker:
        return sizeof(downcast<DrawDotsForDocumentMarker>(item));
    case ItemType::DrawEllipse:
        return sizeof(downcast<DrawEllipse>(item));
    case ItemType::DrawPath:
        return sizeof(downcast<DrawPath>(item));
    case ItemType::DrawFocusRingPath:
        return sizeof(downcast<DrawFocusRingPath>(item));
    case ItemType::DrawFocusRingRects:
        return sizeof(downcast<DrawFocusRingRects>(item));
    case ItemType::FillRect:
        return sizeof(downcast<FillRect>(item));
    case ItemType::FillRectWithColor:
        return sizeof(downcast<FillRectWithColor>(item));
    case ItemType::FillRectWithGradient:
        return sizeof(downcast<FillRectWithGradient>(item));
    case ItemType::FillCompositedRect:
        return sizeof(downcast<FillCompositedRect>(item));
    case ItemType::FillRoundedRect:
        return sizeof(downcast<FillRoundedRect>(item));
    case ItemType::FillRectWithRoundedHole:
        return sizeof(downcast<FillRectWithRoundedHole>(item));
    case ItemType::FillPath:
        return sizeof(downcast<FillPath>(item));
    case ItemType::FillEllipse:
        return sizeof(downcast<FillEllipse>(item));
    case ItemType::StrokeRect:
        return sizeof(downcast<StrokeRect>(item));
    case ItemType::StrokePath:
        return sizeof(downcast<StrokePath>(item));
    case ItemType::StrokeEllipse:
        return sizeof(downcast<StrokeEllipse>(item));
    case ItemType::ClearRect:
        return sizeof(downcast<ClearRect>(item));
    case ItemType::BeginTransparencyLayer:
        return sizeof(downcast<BeginTransparencyLayer>(item));
    case ItemType::EndTransparencyLayer:
        return sizeof(downcast<EndTransparencyLayer>(item));
#if USE(CG)
    case ItemType::ApplyStrokePattern:
        return sizeof(downcast<ApplyStrokePattern>(item));
    case ItemType::ApplyFillPattern:
        return sizeof(downcast<ApplyFillPattern>(item));
#endif
    case ItemType::ApplyDeviceScaleFactor:
        return sizeof(downcast<ApplyDeviceScaleFactor>(item));
    }
    return 0;
}

static TextStream& operator<<(TextStream& ts, const DrawingItem& item)
{
    ts.startGroup();
    ts << "extent ";
    if (item.extentKnown())
        ts << item.extent();
    else
        ts << "unknown";
    
    ts.endGroup();
    return ts;
}

void Save::apply(GraphicsContext& context) const
{
    context.save();
}

static TextStream& operator<<(TextStream& ts, const Save& item)
{
    ts.dumpProperty("restore-index", item.restoreIndex());
    return ts;
}

void Restore::apply(GraphicsContext& context) const
{
    context.restore();
}

void Translate::apply(GraphicsContext& context) const
{
    context.translate(m_x, m_y);
}

static TextStream& operator<<(TextStream& ts, const Translate& item)
{
    ts.dumpProperty("x", item.x());
    ts.dumpProperty("y", item.y());

    return ts;
}

void Rotate::apply(GraphicsContext& context) const
{
    context.rotate(m_angle);
}

static TextStream& operator<<(TextStream& ts, const Rotate& item)
{
    ts.dumpProperty("angle", item.angle());

    return ts;
}

void Scale::apply(GraphicsContext& context) const
{
    context.scale(m_size);
}

static TextStream& operator<<(TextStream& ts, const Scale& item)
{
    ts.dumpProperty("size", item.amount());

    return ts;
}

ConcatenateCTM::ConcatenateCTM(const AffineTransform& transform)
    : Item(ItemType::ConcatenateCTM)
    , m_transform(transform)
{
}

void ConcatenateCTM::apply(GraphicsContext& context) const
{
    context.concatCTM(m_transform);
}

static TextStream& operator<<(TextStream& ts, const ConcatenateCTM& item)
{
    ts.dumpProperty("ctm", item.transform());

    return ts;
}

void SetState::apply(GraphicsContext& context) const
{
    m_state.apply(context);
}

void SetState::accumulate(const GraphicsContextState& state, GraphicsContextState::StateChangeFlags flags)
{
    m_state.accumulate(state, flags);
}

static TextStream& operator<<(TextStream& ts, const SetState& state)
{
    ts << state.state();
    return ts;
}

void SetLineCap::apply(GraphicsContext& context) const
{
    context.setLineCap(m_lineCap);
}

static TextStream& operator<<(TextStream& ts, const SetLineCap& lineCap)
{
    ts.dumpProperty("line-cap", lineCap.lineCap());
    return ts;
}

void SetLineDash::apply(GraphicsContext& context) const
{
    context.setLineDash(m_dashArray, m_dashOffset);
}

static TextStream& operator<<(TextStream& ts, const SetLineDash& lineDash)
{
    ts.dumpProperty("dash-array", lineDash.dashArray());
    ts.dumpProperty("dash-offset", lineDash.dashOffset());
    return ts;
}

void SetLineJoin::apply(GraphicsContext& context) const
{
    context.setLineJoin(m_lineJoin);
}

static TextStream& operator<<(TextStream& ts, const SetLineJoin& lineJoin)
{
    ts.dumpProperty("line-join", lineJoin.lineJoin());
    return ts;
}

void SetMiterLimit::apply(GraphicsContext& context) const
{
    context.setMiterLimit(m_miterLimit);
}

static TextStream& operator<<(TextStream& ts, const SetMiterLimit& miterLimit)
{
    ts.dumpProperty("mitre-limit", miterLimit.miterLimit());
    return ts;
}

void ClearShadow::apply(GraphicsContext& context) const
{
    context.clearShadow();
}

void Clip::apply(GraphicsContext& context) const
{
    context.clip(m_rect);
}

static TextStream& operator<<(TextStream& ts, const Clip& item)
{
    ts.dumpProperty("rect", item.rect());
    return ts;
}

void ClipOut::apply(GraphicsContext& context) const
{
    context.clipOut(m_rect);
}

static TextStream& operator<<(TextStream& ts, const ClipOut& item)
{
    ts.dumpProperty("rect", item.rect());
    return ts;
}

void ClipOutToPath::apply(GraphicsContext& context) const
{
    context.clipOut(m_path);
}

static TextStream& operator<<(TextStream& ts, const ClipOutToPath& item)
{
    ts.dumpProperty("path", item.path());
    return ts;
}

void ClipPath::apply(GraphicsContext& context) const
{
    context.clipPath(m_path, m_windRule);
}

static TextStream& operator<<(TextStream& ts, const ClipPath& item)
{
    ts.dumpProperty("path", item.path());
    ts.dumpProperty("wind-rule", item.windRule());
    return ts;
}

DrawGlyphs::DrawGlyphs(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned count, const FloatPoint& blockLocation, const FloatSize& localAnchor, FontSmoothingMode smoothingMode)
    : DrawingItem(ItemType::DrawGlyphs)
    , m_font(const_cast<Font&>(font))
    , m_blockLocation(blockLocation)
    , m_localAnchor(localAnchor)
    , m_smoothingMode(smoothingMode)
{
    m_glyphs.reserveInitialCapacity(count);
    m_advances.reserveInitialCapacity(count);
    for (unsigned i = 0; i < count; ++i) {
        m_glyphs.uncheckedAppend(glyphs[i]);
        m_advances.uncheckedAppend(advances[i]);
    }
    computeBounds();
}

inline GlyphBuffer DrawGlyphs::generateGlyphBuffer() const
{
    GlyphBuffer result;
    for (size_t i = 0; i < m_glyphs.size(); ++i) {
        result.add(m_glyphs[i], &m_font.get(), m_advances[i]);
    }
    return result;
}

void DrawGlyphs::apply(GraphicsContext& context) const
{
    context.drawGlyphs(m_font, generateGlyphBuffer(), 0, m_glyphs.size(), anchorPoint(), m_smoothingMode);
}

void DrawGlyphs::computeBounds()
{
    // FIXME: This code doesn't actually take the extents of the glyphs into consideration. It assumes that
    // the glyph lies entirely within its [(ascent + descent), advance] rect.
    float ascent = m_font->fontMetrics().floatAscent();
    float descent = m_font->fontMetrics().floatDescent();
    FloatPoint current = toFloatPoint(localAnchor());
    size_t numGlyphs = m_glyphs.size();
    for (size_t i = 0; i < numGlyphs; ++i) {
        GlyphBufferAdvance advance = m_advances[i];
        FloatRect glyphRect = FloatRect(current.x(), current.y() - ascent, advance.width(), ascent + descent);
        m_bounds.unite(glyphRect);

        current.move(advance.width(), advance.height());
    }
}

std::optional<FloatRect> DrawGlyphs::localBounds(const GraphicsContext&) const
{
    FloatRect localBounds = m_bounds;
    localBounds.move(m_blockLocation.x(), m_blockLocation.y());
    return localBounds;
}

static TextStream& operator<<(TextStream& ts, const DrawGlyphs& item)
{
    ts << static_cast<const DrawingItem&>(item);
    // FIXME: dump more stuff.
    ts.dumpProperty("block-location", item.blockLocation());
    ts.dumpProperty("local-anchor", item.localAnchor());
    ts.dumpProperty("anchor-point", item.anchorPoint());
    ts.dumpProperty("length", item.glyphs().size());

    return ts;
}

DrawImage::DrawImage(Image& image, const FloatRect& destination, const FloatRect& source, const ImagePaintingOptions& imagePaintingOptions)
    : DrawingItem(ItemType::DrawImage)
    , m_image(image)
    , m_destination(destination)
    , m_source(source)
    , m_imagePaintingOptions(imagePaintingOptions)
{
}

void DrawImage::apply(GraphicsContext& context) const
{
    context.drawImage(m_image.get(), m_destination, m_source, m_imagePaintingOptions);
}

static TextStream& operator<<(TextStream& ts, const DrawImage& item)
{
    ts << static_cast<const DrawingItem&>(item);
    ts.dumpProperty("image", item.image());
    ts.dumpProperty("source-rect", item.source());
    ts.dumpProperty("dest-rect", item.destination());
    return ts;
}

DrawTiledImage::DrawTiledImage(Image& image, const FloatRect& destination, const FloatPoint& source, const FloatSize& tileSize, const FloatSize& spacing, const ImagePaintingOptions& imagePaintingOptions)
    : DrawingItem(ItemType::DrawTiledImage)
    , m_image(image)
    , m_destination(destination)
    , m_source(source)
    , m_tileSize(tileSize)
    , m_spacing(spacing)
    , m_imagePaintingOptions(imagePaintingOptions)
{
}

void DrawTiledImage::apply(GraphicsContext& context) const
{
    context.drawTiledImage(m_image.get(), m_destination, m_source, m_tileSize, m_spacing, m_imagePaintingOptions);
}

static TextStream& operator<<(TextStream& ts, const DrawTiledImage& item)
{
    ts << static_cast<const DrawingItem&>(item);
    ts.dumpProperty("image", item.image());
    ts.dumpProperty("source-point", item.source());
    ts.dumpProperty("dest-rect", item.destination());
    ts.dumpProperty("tile-size", item.tileSize());
    ts.dumpProperty("spacing", item.spacing());
    return ts;
}

DrawTiledScaledImage::DrawTiledScaledImage(Image& image, const FloatRect& destination, const FloatRect& source, const FloatSize& tileScaleFactor, Image::TileRule hRule, Image::TileRule vRule, const ImagePaintingOptions& imagePaintingOptions)
    : DrawingItem(ItemType::DrawTiledScaledImage)
    , m_image(image)
    , m_destination(destination)
    , m_source(source)
    , m_tileScaleFactor(tileScaleFactor)
    , m_hRule(hRule)
    , m_vRule(vRule)
    , m_imagePaintingOptions(imagePaintingOptions)
{
}

void DrawTiledScaledImage::apply(GraphicsContext& context) const
{
    context.drawTiledImage(m_image.get(), m_destination, m_source, m_tileScaleFactor, m_hRule, m_vRule, m_imagePaintingOptions);
}

static TextStream& operator<<(TextStream& ts, const DrawTiledScaledImage& item)
{
    ts << static_cast<const DrawingItem&>(item);
    ts.dumpProperty("image", item.image());
    ts.dumpProperty("source-rect", item.source());
    ts.dumpProperty("dest-rect", item.destination());
    return ts;
}

#if USE(CG) || USE(CAIRO)
DrawNativeImage::DrawNativeImage(const NativeImagePtr& image, const FloatSize& imageSize, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator op, BlendMode blendMode, ImageOrientation orientation)
    : DrawingItem(ItemType::DrawNativeImage)
#if USE(CG)
    // FIXME: Need to store an image for Cairo.
    , m_image(image)
#endif
    , m_imageSize(imageSize)
    , m_destination(destRect)
    , m_srcRect(srcRect)
#if USE(CG)
    , m_op(op)
    , m_blendMode(blendMode)
#endif
    , m_orientation(orientation)
{
#if !USE(CG)
    UNUSED_PARAM(image);
    UNUSED_PARAM(op);
    UNUSED_PARAM(blendMode);
#endif
}

void DrawNativeImage::apply(GraphicsContext& context) const
{
#if USE(CG)
    context.drawNativeImage(m_image, m_imageSize, m_destination, m_srcRect, m_op, m_blendMode, m_orientation);
#else
    UNUSED_PARAM(context);
#endif
}

static TextStream& operator<<(TextStream& ts, const DrawNativeImage& item)
{
    ts << static_cast<const DrawingItem&>(item);
    // FIXME: dump more stuff.
    ts.dumpProperty("source-rect", item.source());
    ts.dumpProperty("dest-rect", item.destination());
    return ts;
}
#endif

DrawPattern::DrawPattern(Image& image, const FloatRect& destRect, const FloatRect& tileRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, CompositeOperator op, BlendMode blendMode)
    : DrawingItem(ItemType::DrawPattern)
    , m_image(image)
    , m_patternTransform(patternTransform)
    , m_tileRect(tileRect)
    , m_destination(destRect)
    , m_phase(phase)
    , m_spacing(spacing)
    , m_op(op)
    , m_blendMode(blendMode)
{
}

void DrawPattern::apply(GraphicsContext& context) const
{
    context.drawPattern(m_image.get(), m_destination, m_tileRect, m_patternTransform, m_phase, m_spacing, m_op, m_blendMode);
}

static TextStream& operator<<(TextStream& ts, const DrawPattern& item)
{
    ts << static_cast<const DrawingItem&>(item);
    ts.dumpProperty("image", item.image());
    ts.dumpProperty("pattern-transform", item.patternTransform());
    ts.dumpProperty("tile-rect", item.tileRect());
    ts.dumpProperty("dest-rect", item.destRect());
    ts.dumpProperty("phase", item.phase());
    ts.dumpProperty("spacing", item.spacing());
    return ts;
}

void DrawRect::apply(GraphicsContext& context) const
{
    context.drawRect(m_rect, m_borderThickness);
}

static TextStream& operator<<(TextStream& ts, const DrawRect& item)
{
    ts << static_cast<const DrawingItem&>(item);
    ts.dumpProperty("rect", item.rect());
    ts.dumpProperty("border-thickness", item.borderThickness());
    return ts;
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

static TextStream& operator<<(TextStream& ts, const DrawLine& item)
{
    ts << static_cast<const DrawingItem&>(item);
    ts.dumpProperty("point-1", item.point1());
    ts.dumpProperty("point-2", item.point2());
    return ts;
}

void DrawLinesForText::apply(GraphicsContext& context) const
{
    context.drawLinesForText(point(), m_thickness, m_widths, m_printing, m_doubleLines);
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

static TextStream& operator<<(TextStream& ts, const DrawLinesForText& item)
{
    ts << static_cast<const DrawingItem&>(item);
    ts.dumpProperty("block-location", item.blockLocation());
    ts.dumpProperty("local-anchor", item.localAnchor());
    ts.dumpProperty("point", item.point());
    ts.dumpProperty("thickness", item.thickness());
    ts.dumpProperty("double", item.doubleLines());
    ts.dumpProperty("widths", item.widths());
    ts.dumpProperty("is-printing", item.isPrinting());
    ts.dumpProperty("double", item.doubleLines());
    return ts;
}

void DrawDotsForDocumentMarker::apply(GraphicsContext& context) const
{
    context.drawDotsForDocumentMarker(m_rect, m_style);
}

std::optional<FloatRect> DrawDotsForDocumentMarker::localBounds(const GraphicsContext&) const
{
    return m_rect;
}

static TextStream& operator<<(TextStream& ts, const DrawDotsForDocumentMarker& item)
{
    ts << static_cast<const DrawingItem&>(item);
    ts.dumpProperty("rect", item.rect());
    return ts;
}

void DrawEllipse::apply(GraphicsContext& context) const
{
    context.drawEllipse(m_rect);
}

static TextStream& operator<<(TextStream& ts, const DrawEllipse& item)
{
    ts.dumpProperty("rect", item.rect());
    return ts;
}

void DrawPath::apply(GraphicsContext& context) const
{
#if USE(CG)
    context.drawPath(m_path);
#else
    UNUSED_PARAM(context);
#endif
}

static TextStream& operator<<(TextStream& ts, const DrawPath& item)
{
    ts << static_cast<const DrawingItem&>(item);
//    ts.dumpProperty("path", item.path()); // FIXME: add logging for paths.
    return ts;
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

static TextStream& operator<<(TextStream& ts, const DrawFocusRingPath& item)
{
    ts << static_cast<const DrawingItem&>(item);
//    ts.dumpProperty("path", item.path()); // FIXME: add logging for paths.
    ts.dumpProperty("width", item.width());
    ts.dumpProperty("offset", item.offset());
    ts.dumpProperty("color", item.color());
    return ts;
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

static TextStream& operator<<(TextStream& ts, const DrawFocusRingRects& item)
{
    ts << static_cast<const DrawingItem&>(item);
    ts.dumpProperty("rects", item.rects());
    ts.dumpProperty("width", item.width());
    ts.dumpProperty("offset", item.offset());
    ts.dumpProperty("color", item.color());
    return ts;
}

void FillRect::apply(GraphicsContext& context) const
{
    context.fillRect(m_rect);
}

static TextStream& operator<<(TextStream& ts, const FillRect& item)
{
    ts << static_cast<const DrawingItem&>(item);
    ts.dumpProperty("rect", item.rect());
    return ts;
}

void FillRectWithColor::apply(GraphicsContext& context) const
{
    context.fillRect(m_rect, m_color);
}

static TextStream& operator<<(TextStream& ts, const FillRectWithColor& item)
{
    ts << static_cast<const DrawingItem&>(item);
    ts.dumpProperty("rect", item.rect());
    ts.dumpProperty("color", item.color());
    return ts;
}

void FillRectWithGradient::apply(GraphicsContext& context) const
{
    context.fillRect(m_rect, m_gradient.get());
}

static TextStream& operator<<(TextStream& ts, const FillRectWithGradient& item)
{
    ts << static_cast<const DrawingItem&>(item);
    // FIXME: log gradient.
    ts.dumpProperty("rect", item.rect());
    return ts;
}

void FillCompositedRect::apply(GraphicsContext& context) const
{
    context.fillRect(m_rect, m_color, m_op, m_blendMode);
}

static TextStream& operator<<(TextStream& ts, const FillCompositedRect& item)
{
    ts << static_cast<const DrawingItem&>(item);
    ts.dumpProperty("rect", item.rect());
    ts.dumpProperty("color", item.color());
    ts.dumpProperty("composite-operation", item.compositeOperator());
    ts.dumpProperty("blend-mode", item.blendMode());
    return ts;
}

void FillRoundedRect::apply(GraphicsContext& context) const
{
    context.fillRoundedRect(m_rect, m_color, m_blendMode);
}

static TextStream& operator<<(TextStream& ts, const FillRoundedRect& item)
{
    ts << static_cast<const DrawingItem&>(item);
    ts.dumpProperty("rect", item.roundedRect());
    ts.dumpProperty("color", item.color());
    ts.dumpProperty("blend-mode", item.blendMode());
    return ts;
}

void FillRectWithRoundedHole::apply(GraphicsContext& context) const
{
    context.fillRectWithRoundedHole(m_rect, m_roundedHoleRect, m_color);
}

static TextStream& operator<<(TextStream& ts, const FillRectWithRoundedHole& item)
{
    ts << static_cast<const DrawingItem&>(item);
    ts.dumpProperty("rect", item.rect());
    ts.dumpProperty("rounded-hole-rect", item.roundedHoleRect());
    ts.dumpProperty("color", item.color());
    return ts;
}

void FillPath::apply(GraphicsContext& context) const
{
    context.fillPath(m_path);
}

static TextStream& operator<<(TextStream& ts, const FillPath& item)
{
    ts << static_cast<const DrawingItem&>(item);
    ts.dumpProperty("path", item.path());
    return ts;
}

void FillEllipse::apply(GraphicsContext& context) const
{
    context.fillEllipse(m_rect);
}

static TextStream& operator<<(TextStream& ts, const FillEllipse& item)
{
    ts << static_cast<const DrawingItem&>(item);
    ts.dumpProperty("rect", item.rect());
    return ts;
}

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

static TextStream& operator<<(TextStream& ts, const StrokeRect& item)
{
    ts << static_cast<const DrawingItem&>(item);
    ts.dumpProperty("rect", item.rect());
    ts.dumpProperty("line-width", item.lineWidth());
    return ts;
}

std::optional<FloatRect> StrokePath::localBounds(const GraphicsContext& context) const
{
    // FIXME: Need to take stroke thickness into account correctly, via CGPathByStrokingPath().
    float strokeThickness = context.strokeThickness();

    FloatRect bounds = m_path.boundingRect();
    bounds.expand(strokeThickness, strokeThickness);
    return bounds;
}

void StrokePath::apply(GraphicsContext& context) const
{
    context.strokePath(m_path);
}

static TextStream& operator<<(TextStream& ts, const StrokePath& item)
{
    ts << static_cast<const DrawingItem&>(item);
    ts.dumpProperty("path", item.path());
    return ts;
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

static TextStream& operator<<(TextStream& ts, const StrokeEllipse& item)
{
    ts << static_cast<const DrawingItem&>(item);
    ts.dumpProperty("rect", item.rect());
    return ts;
}

void ClearRect::apply(GraphicsContext& context) const
{
    context.clearRect(m_rect);
}

static TextStream& operator<<(TextStream& ts, const ClearRect& item)
{
    ts << static_cast<const DrawingItem&>(item);
    ts.dumpProperty("rect", item.rect());
    return ts;
}

void BeginTransparencyLayer::apply(GraphicsContext& context) const
{
    context.beginTransparencyLayer(m_opacity);
}

static TextStream& operator<<(TextStream& ts, const BeginTransparencyLayer& item)
{
    ts << static_cast<const DrawingItem&>(item);
    ts.dumpProperty("opacity", item.opacity());
    return ts;
}

void EndTransparencyLayer::apply(GraphicsContext& context) const
{
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

static TextStream& operator<<(TextStream& ts, const ApplyDeviceScaleFactor& item)
{
    ts.dumpProperty("scale-factor", item.scaleFactor());
    return ts;
}

static TextStream& operator<<(TextStream& ts, const ItemType& type)
{
    switch (type) {
    case ItemType::Save: ts << "save"; break;
    case ItemType::Restore: ts << "restore"; break;
    case ItemType::Translate: ts << "translate"; break;
    case ItemType::Rotate: ts << "rotate"; break;
    case ItemType::Scale: ts << "scale"; break;
    case ItemType::ConcatenateCTM: ts << "concatentate-ctm"; break;
    case ItemType::SetState: ts << "set-state"; break;
    case ItemType::SetLineCap: ts << "set-line-cap"; break;
    case ItemType::SetLineDash: ts << "set-line-dash"; break;
    case ItemType::SetLineJoin: ts << "set-line-join"; break;
    case ItemType::SetMiterLimit: ts << "set-miter-limit"; break;
    case ItemType::Clip: ts << "clip"; break;
    case ItemType::ClipOut: ts << "clip-out"; break;
    case ItemType::ClipOutToPath: ts << "clip-out-to-path"; break;
    case ItemType::ClipPath: ts << "clip-path"; break;
    case ItemType::DrawGlyphs: ts << "draw-glyphs"; break;
    case ItemType::DrawImage: ts << "draw-image"; break;
    case ItemType::DrawTiledImage: ts << "draw-tiled-image"; break;
    case ItemType::DrawTiledScaledImage: ts << "draw-tiled-scaled-image"; break;
#if USE(CG) || USE(CAIRO)
    case ItemType::DrawNativeImage: ts << "draw-native-image"; break;
#endif
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
    case ItemType::FillPath: ts << "fill-path"; break;
    case ItemType::FillEllipse: ts << "fill-ellipse"; break;
    case ItemType::StrokeRect: ts << "stroke-rect"; break;
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

TextStream& operator<<(TextStream& ts, const Item& item)
{
    TextStream::GroupScope group(ts);
    ts << item.type();

    // FIXME: Make a macro which takes a macro for all these enumeration switches
    switch (item.type()) {
    case ItemType::Save:
        ts << downcast<Save>(item);
        break;
    case ItemType::Translate:
        ts << downcast<Translate>(item);
        break;
    case ItemType::Rotate:
        ts << downcast<Rotate>(item);
        break;
    case ItemType::Scale:
        ts << downcast<Scale>(item);
        break;
    case ItemType::ConcatenateCTM:
        ts << downcast<ConcatenateCTM>(item);
        break;
    case ItemType::SetState:
        ts << downcast<SetState>(item);
        break;
    case ItemType::SetLineCap:
        ts << downcast<SetLineCap>(item);
        break;
    case ItemType::SetLineDash:
        ts << downcast<SetLineDash>(item);
        break;
    case ItemType::SetLineJoin:
        ts << downcast<SetLineJoin>(item);
        break;
    case ItemType::SetMiterLimit:
        ts << downcast<SetMiterLimit>(item);
        break;
    case ItemType::Clip:
        ts << downcast<Clip>(item);
        break;
    case ItemType::ClipOut:
        ts << downcast<ClipOut>(item);
        break;
    case ItemType::ClipOutToPath:
        ts << downcast<ClipOutToPath>(item);
        break;
    case ItemType::ClipPath:
        ts << downcast<ClipPath>(item);
        break;
    case ItemType::DrawGlyphs:
        ts << downcast<DrawGlyphs>(item);
        break;
    case ItemType::DrawImage:
        ts << downcast<DrawImage>(item);
        break;
    case ItemType::DrawTiledImage:
        ts << downcast<DrawTiledImage>(item);
        break;
    case ItemType::DrawTiledScaledImage:
        ts << downcast<DrawTiledScaledImage>(item);
        break;
#if USE(CG) || USE(CAIRO)
    case ItemType::DrawNativeImage:
        ts << downcast<DrawNativeImage>(item);
        break;
#endif
    case ItemType::DrawPattern:
        ts << downcast<DrawPattern>(item);
        break;
    case ItemType::DrawRect:
        ts << downcast<DrawRect>(item);
        break;
    case ItemType::DrawLine:
        ts << downcast<DrawLine>(item);
        break;
    case ItemType::DrawLinesForText:
        ts << downcast<DrawLinesForText>(item);
        break;
    case ItemType::DrawDotsForDocumentMarker:
        ts << downcast<DrawDotsForDocumentMarker>(item);
        break;
    case ItemType::DrawEllipse:
        ts << downcast<DrawEllipse>(item);
        break;
    case ItemType::DrawPath:
        ts << downcast<DrawPath>(item);
        break;
    case ItemType::DrawFocusRingPath:
        ts << downcast<DrawFocusRingPath>(item);
        break;
    case ItemType::DrawFocusRingRects:
        ts << downcast<DrawFocusRingRects>(item);
        break;
    case ItemType::FillRect:
        ts << downcast<FillRect>(item);
        break;
    case ItemType::FillRectWithColor:
        ts << downcast<FillRectWithColor>(item);
        break;
    case ItemType::FillRectWithGradient:
        ts << downcast<FillRectWithGradient>(item);
        break;
    case ItemType::FillCompositedRect:
        ts << downcast<FillCompositedRect>(item);
        break;
    case ItemType::FillRoundedRect:
        ts << downcast<FillRoundedRect>(item);
        break;
    case ItemType::FillRectWithRoundedHole:
        ts << downcast<FillRectWithRoundedHole>(item);
        break;
    case ItemType::FillPath:
        ts << downcast<FillPath>(item);
        break;
    case ItemType::FillEllipse:
        ts << downcast<FillEllipse>(item);
        break;
    case ItemType::StrokeRect:
        ts << downcast<StrokeRect>(item);
        break;
    case ItemType::StrokePath:
        ts << downcast<StrokePath>(item);
        break;
    case ItemType::StrokeEllipse:
        ts << downcast<StrokeEllipse>(item);
        break;
    case ItemType::ClearRect:
        ts << downcast<ClearRect>(item);
        break;
    case ItemType::BeginTransparencyLayer:
        ts << downcast<BeginTransparencyLayer>(item);
        break;
    case ItemType::ApplyDeviceScaleFactor:
        ts << downcast<ApplyDeviceScaleFactor>(item);
        break;

    // Items with no additional data.
    case ItemType::Restore:
    case ItemType::EndTransparencyLayer:
#if USE(CG)
    case ItemType::ApplyStrokePattern:
    case ItemType::ApplyFillPattern:
#endif
    case ItemType::ClearShadow:
        break;
    }
    return ts;
}

}
}
