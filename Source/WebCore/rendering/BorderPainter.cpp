/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "BorderPainter.h"

#include "BorderEdge.h"
#include "CachedImage.h"
#include "FloatRoundedRect.h"
#include "GeometryUtilities.h"
#include "GraphicsContext.h"
#include "NinePieceImage.h"
#include "PaintInfo.h"
#include "PathUtilities.h"
#include "RenderBox.h"
#include "RenderTheme.h"

namespace WebCore {

BorderPainter::BorderPainter(const RenderElement& renderer, const PaintInfo& paintInfo)
    : m_renderer(renderer)
    , m_paintInfo(paintInfo)
{
}

bool BorderPainter::allCornersClippedOut(const RoundedRect& border, const LayoutRect& clipRect)
{
    LayoutRect boundingRect = border.rect();
    if (clipRect.contains(boundingRect))
        return false;

    RoundedRect::Radii radii = border.radii();

    LayoutRect topLeftRect(boundingRect.location(), radii.topLeft());
    if (clipRect.intersects(topLeftRect))
        return false;

    LayoutRect topRightRect(boundingRect.location(), radii.topRight());
    topRightRect.setX(boundingRect.maxX() - topRightRect.width());
    if (clipRect.intersects(topRightRect))
        return false;

    LayoutRect bottomLeftRect(boundingRect.location(), radii.bottomLeft());
    bottomLeftRect.setY(boundingRect.maxY() - bottomLeftRect.height());
    if (clipRect.intersects(bottomLeftRect))
        return false;

    LayoutRect bottomRightRect(boundingRect.location(), radii.bottomRight());
    bottomRightRect.setX(boundingRect.maxX() - bottomRightRect.width());
    bottomRightRect.setY(boundingRect.maxY() - bottomRightRect.height());
    if (clipRect.intersects(bottomRightRect))
        return false;

    return true;
}

static LayoutRect calculateSideRect(const RoundedRect& outerBorder, const BorderEdges& edges, BoxSide side)
{
    LayoutRect sideRect = outerBorder.rect();
    float width = edges.at(side).widthForPainting();

    switch (side) {
    case BoxSide::Top:
        sideRect.setHeight(width);
        break;
    case BoxSide::Right:
        sideRect.shiftXEdgeTo(sideRect.maxX() - width);
        break;
    case BoxSide::Bottom:
        sideRect.shiftYEdgeTo(sideRect.maxY() - width);
        break;
    case BoxSide::Left:
        sideRect.setWidth(width);
        break;
    }

    return sideRect;
}

LayoutRect shrinkRectByOneDevicePixel(const GraphicsContext& context, const LayoutRect& rect, float devicePixelRatio)
{
    LayoutRect shrunkRect = rect;
    AffineTransform transform = context.getCTM();
    shrunkRect.inflateX(-ceilToDevicePixel(1_lu / transform.xScale(), devicePixelRatio));
    shrunkRect.inflateY(-ceilToDevicePixel(1_lu / transform.yScale(), devicePixelRatio));
    return shrunkRect;
}

void BorderPainter::paintBorder(const LayoutRect& rect, const RenderStyle& style, BackgroundBleedAvoidance bleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    GraphicsContext& graphicsContext = m_paintInfo.context();

    if (graphicsContext.paintingDisabled())
        return;

    auto paintsBorderImage = [&](LayoutRect rect, const NinePieceImage& ninePieceImage) {
        auto* styleImage = ninePieceImage.image();
        if (!styleImage)
            return false;

        if (!styleImage->isLoaded())
            return false;

        if (!styleImage->canRender(&m_renderer, style.effectiveZoom()))
            return false;

        auto rectWithOutsets = rect;
        rectWithOutsets.expand(style.imageOutsets(ninePieceImage));
        return !rectWithOutsets.isEmpty();
    };

    if (rect.isEmpty() && !paintsBorderImage(rect, style.borderImage()))
        return;

    auto rectToClipOut = const_cast<RenderElement&>(m_renderer).paintRectToClipOutFromBorder(rect);
    bool appliedClipAlready = !rectToClipOut.isEmpty();
    GraphicsContextStateSaver stateSave(graphicsContext, appliedClipAlready);
    if (!rectToClipOut.isEmpty())
        graphicsContext.clipOut(snapRectToDevicePixels(rectToClipOut, document().deviceScaleFactor()));

    // border-image is not affected by border-radius.
    if (paintNinePieceImage(rect, style, style.borderImage()))
        return;

    auto edges = borderEdges(style, document().deviceScaleFactor(), includeLogicalLeftEdge, includeLogicalRightEdge);
    RoundedRect outerBorder = style.getRoundedBorderFor(rect, includeLogicalLeftEdge, includeLogicalRightEdge);
    RoundedRect innerBorder = style.getRoundedInnerBorderFor(borderInnerRectAdjustedForBleedAvoidance(rect, bleedAvoidance), includeLogicalLeftEdge, includeLogicalRightEdge);
    RoundedRect unadjustedInnerBorder = (bleedAvoidance == BackgroundBleedBackgroundOverBorder) ? style.getRoundedInnerBorderFor(rect, includeLogicalLeftEdge, includeLogicalRightEdge) : innerBorder;

    auto haveAllSolidEdges = true;
    for (auto side : allBoxSides) {
        auto& currEdge = edges.at(side);

        if (currEdge.presentButInvisible() || !currEdge.widthForPainting())
            continue;

        if (currEdge.style() != BorderStyle::Solid) {
            haveAllSolidEdges = false;
            break;
        }
    }

    if (haveAllSolidEdges && outerBorder.isRounded() && allCornersClippedOut(outerBorder, m_paintInfo.rect))
        outerBorder.setRadii(RoundedRect::Radii());

    paintSides({
        outerBorder,
        innerBorder,
        unadjustedInnerBorder,
        style.hasBorderRadius() ? std::make_optional(style.borderRadii()) : std::nullopt,
        edges,
        haveAllSolidEdges,
        bleedAvoidance,
        includeLogicalLeftEdge,
        includeLogicalRightEdge,
        appliedClipAlready,
        style.isHorizontalWritingMode()
    });
}

void BorderPainter::paintOutline(const LayoutRect& paintRect)
{
    auto& styleToUse = m_renderer.style();
    auto outlineWidth = floorToDevicePixel(styleToUse.outlineWidth(), document().deviceScaleFactor());
    auto outlineOffset = floorToDevicePixel(styleToUse.outlineOffset(), document().deviceScaleFactor());

    // Only paint the focus ring by hand if the theme isn't able to draw it.
    if (styleToUse.outlineStyleIsAuto() == OutlineIsAuto::On && !m_renderer.theme().supportsFocusRing(styleToUse)) {
        Vector<LayoutRect> focusRingRects;
        LayoutRect paintRectToUse { paintRect };
        if (is<RenderBox>(m_renderer))
            paintRectToUse = m_renderer.theme().adjustedPaintRect(downcast<RenderBox>(m_renderer), paintRectToUse);
        m_renderer.addFocusRingRects(focusRingRects, paintRectToUse.location(), m_paintInfo.paintContainer);
        m_renderer.paintFocusRing(m_paintInfo, styleToUse, focusRingRects);
    }

    if (m_renderer.hasOutlineAnnotation() && styleToUse.outlineStyleIsAuto() == OutlineIsAuto::Off && !m_renderer.theme().supportsFocusRing(styleToUse))
        m_renderer.addPDFURLRect(m_paintInfo, paintRect.location());

    if (styleToUse.outlineStyleIsAuto() == OutlineIsAuto::On || styleToUse.outlineStyle() == BorderStyle::None)
        return;

    // FIXME: This prevents outlines from painting inside the object. See bug 12042
    auto outer = paintRect;
    outer.inflate(outlineOffset + outlineWidth);
    if (outer.isEmpty())
        return;

    auto isHorizontal = styleToUse.isHorizontalWritingMode();
    auto hasBorderRadius = styleToUse.hasBorderRadius();
    auto includeLogicalLeftEdge = true;
    auto includeLogicalRightEdge = true;
    auto roundedBorderRectFor = [&] (auto& borderRect, auto borderOffset) {
        auto adjustedRadius = [&] (auto& radius, auto offset) {
            auto widthValue = radius.width.isAuto() ? 0 : intValueForLength(radius.width, paintRect.width());
            auto heightValue = radius.height.isAuto() ? 0 : intValueForLength(radius.height, paintRect.height());
            if (!widthValue && !heightValue)
                return LengthSize { { 0, LengthType::Fixed }, { 0, LengthType::Fixed } };
            if (!widthValue)
                return LengthSize { { 0, LengthType::Fixed }, { heightValue + offset, LengthType::Fixed } };
            return LengthSize { { widthValue + offset, LengthType::Fixed }, { heightValue + offset, LengthType::Fixed } };
        };

        auto borderRadii = std::optional<BorderData::Radii> { };
        if (hasBorderRadius) {
            borderRadii = BorderData::Radii {
                adjustedRadius(styleToUse.borderTopLeftRadius(), borderOffset),
                adjustedRadius(styleToUse.borderTopRightRadius(), borderOffset),
                adjustedRadius(styleToUse.borderBottomLeftRadius(), borderOffset),
                adjustedRadius(styleToUse.borderBottomRightRadius(), borderOffset)
            };
        }
        return RenderStyle::getRoundedInnerBorderFor(borderRect, { }, { }, { }, { }, borderRadii, isHorizontal, includeLogicalLeftEdge, includeLogicalRightEdge);
    };
    auto innerRectForOutline = paintRect;
    innerRectForOutline.inflate(outlineOffset);
    auto innerBorder = roundedBorderRectFor(innerRectForOutline, LayoutUnit { outlineOffset });
    auto outerBorder = roundedBorderRectFor(outer, LayoutUnit { outlineWidth + outlineOffset });
    auto bleedAvoidance = BackgroundBleedShrinkBackground;
    auto appliedClipAlready = false;
    auto haveAllSolidEdges = true;

    paintSides({
        outerBorder,
        innerBorder,
        innerBorder,
        hasBorderRadius ? std::make_optional(styleToUse.borderRadii()) : std::nullopt,
        borderEdgesForOutline(styleToUse, document().deviceScaleFactor()),
        haveAllSolidEdges,
        bleedAvoidance,
        includeLogicalLeftEdge,
        includeLogicalRightEdge,
        appliedClipAlready,
        isHorizontal
    });
}

void BorderPainter::paintOutline(const LayoutPoint& paintOffset, const Vector<LayoutRect>& lineRects)
{
    if (lineRects.size() == 1) {
        auto adjustedPaintRect = lineRects[0];
        adjustedPaintRect.moveBy(paintOffset);
        paintOutline(adjustedPaintRect);
        return;
    }

    auto& styleToUse = m_renderer.style();
    auto outlineOffset = styleToUse.outlineOffset();
    auto outlineWidth = styleToUse.outlineWidth();
    auto deviceScaleFactor = document().deviceScaleFactor();

    Vector<FloatRect> pixelSnappedRects;
    for (size_t index = 0; index < lineRects.size(); ++index) {
        auto rect = lineRects[index];

        rect.moveBy(paintOffset);
        rect.inflate(outlineOffset + outlineWidth / 2);
        pixelSnappedRects.append(snapRectToDevicePixels(rect, deviceScaleFactor));
    }
    auto path = PathUtilities::pathWithShrinkWrappedRectsForOutline(pixelSnappedRects, styleToUse.border(), outlineOffset, styleToUse.direction(), styleToUse.writingMode(), deviceScaleFactor);
    if (path.isEmpty()) {
        // Disjoint line spanning inline boxes.
        for (auto rect : lineRects) {
            rect.moveBy(paintOffset);
            paintOutline(rect);
        }
        return;
    }

    auto& graphicsContext = m_paintInfo.context();
    auto outlineColor = styleToUse.visitedDependentColorWithColorFilter(CSSPropertyOutlineColor);
    auto useTransparencyLayer = !outlineColor.isOpaque();
    if (useTransparencyLayer) {
        graphicsContext.beginTransparencyLayer(outlineColor.alphaAsFloat());
        outlineColor = outlineColor.opaqueColor();
    }

    graphicsContext.setStrokeColor(outlineColor);
    graphicsContext.setStrokeThickness(outlineWidth);
    graphicsContext.setStrokeStyle(SolidStroke);
    graphicsContext.strokePath(path);

    if (useTransparencyLayer)
        graphicsContext.endTransparencyLayer();
}

void BorderPainter::paintSides(const Sides& sides)
{
    GraphicsContext& graphicsContext = m_paintInfo.context();

    ASSERT(!graphicsContext.paintingDisabled());

    // If no borders intersects with the dirty area, we can skip the painting.
    if (sides.innerBorder.contains(m_paintInfo.rect))
        return;

    bool haveAlphaColor = false;
    bool haveAllDoubleEdges = true;
    int numEdgesVisible = 4;
    bool allEdgesShareColor = true;
    std::optional<BoxSide> firstVisibleSide;
    BoxSideSet edgesToDraw;

    for (auto boxSide : allBoxSides) {
        auto& currEdge = sides.edges.at(boxSide);

        if (currEdge.shouldRender())
            edgesToDraw.add(edgeFlagForSide(boxSide));

        if (currEdge.presentButInvisible()) {
            --numEdgesVisible;
            allEdgesShareColor = false;
            continue;
        }

        if (!currEdge.widthForPainting()) {
            --numEdgesVisible;
            continue;
        }

        if (!firstVisibleSide)
            firstVisibleSide = boxSide;
        else if (currEdge.color() != sides.edges.at(*firstVisibleSide).color())
            allEdgesShareColor = false;

        if (!currEdge.color().isOpaque())
            haveAlphaColor = true;

        if (currEdge.style() != BorderStyle::Double)
            haveAllDoubleEdges = false;
    }

    auto deviceScaleFactor = document().deviceScaleFactor();
    // isRenderable() check avoids issue described in https://bugs.webkit.org/show_bug.cgi?id=38787
    if ((sides.haveAllSolidEdges || haveAllDoubleEdges) && allEdgesShareColor && sides.innerBorder.isRenderable()) {
        // Fast path for drawing all solid edges and all unrounded double edges
        if (numEdgesVisible == 4 && (sides.outerBorder.isRounded() || haveAlphaColor)
            && (sides.haveAllSolidEdges || (!sides.outerBorder.isRounded() && !sides.innerBorder.isRounded()))) {
            Path path;

            FloatRoundedRect pixelSnappedOuterBorder = sides.outerBorder.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
            if (pixelSnappedOuterBorder.isRounded() && sides.bleedAvoidance != BackgroundBleedUseTransparencyLayer)
                path.addRoundedRect(pixelSnappedOuterBorder);
            else
                path.addRect(pixelSnappedOuterBorder.rect());

            if (haveAllDoubleEdges) {
                LayoutRect innerThirdRect = sides.outerBorder.rect();
                LayoutRect outerThirdRect = sides.outerBorder.rect();
                for (auto side : allBoxSides) {
                    LayoutUnit outerWidth;
                    LayoutUnit innerWidth;
                    sides.edges.at(side).getDoubleBorderStripeWidths(outerWidth, innerWidth);
                    switch (side) {
                    case BoxSide::Top:
                        innerThirdRect.shiftYEdgeTo(innerThirdRect.y() + innerWidth);
                        outerThirdRect.shiftYEdgeTo(outerThirdRect.y() + outerWidth);
                        break;
                    case BoxSide::Right:
                        innerThirdRect.setWidth(innerThirdRect.width() - innerWidth);
                        outerThirdRect.setWidth(outerThirdRect.width() - outerWidth);
                        break;
                    case BoxSide::Bottom:
                        innerThirdRect.setHeight(innerThirdRect.height() - innerWidth);
                        outerThirdRect.setHeight(outerThirdRect.height() - outerWidth);
                        break;
                    case BoxSide::Left:
                        innerThirdRect.shiftXEdgeTo(innerThirdRect.x() + innerWidth);
                        outerThirdRect.shiftXEdgeTo(outerThirdRect.x() + outerWidth);
                        break;
                    }
                }

                FloatRoundedRect pixelSnappedOuterThird = sides.outerBorder.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
                pixelSnappedOuterThird.setRect(snapRectToDevicePixels(outerThirdRect, deviceScaleFactor));

                if (pixelSnappedOuterThird.isRounded() && sides.bleedAvoidance != BackgroundBleedUseTransparencyLayer)
                    path.addRoundedRect(pixelSnappedOuterThird);
                else
                    path.addRect(pixelSnappedOuterThird.rect());

                FloatRoundedRect pixelSnappedInnerThird = sides.innerBorder.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
                pixelSnappedInnerThird.setRect(snapRectToDevicePixels(innerThirdRect, deviceScaleFactor));
                if (pixelSnappedInnerThird.isRounded() && sides.bleedAvoidance != BackgroundBleedUseTransparencyLayer)
                    path.addRoundedRect(pixelSnappedInnerThird);
                else
                    path.addRect(pixelSnappedInnerThird.rect());
            }

            FloatRoundedRect pixelSnappedInnerBorder = sides.innerBorder.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
            if (pixelSnappedInnerBorder.isRounded())
                path.addRoundedRect(pixelSnappedInnerBorder);
            else
                path.addRect(pixelSnappedInnerBorder.rect());

            graphicsContext.setFillRule(WindRule::EvenOdd);
            graphicsContext.setFillColor(sides.edges.at(*firstVisibleSide).color());
            graphicsContext.fillPath(path);
            return;
        }
        // Avoid creating transparent layers
        if (sides.haveAllSolidEdges && numEdgesVisible != 4 && !sides.outerBorder.isRounded() && haveAlphaColor) {
            Path path;

            for (auto side : allBoxSides) {
                if (sides.edges.at(side).shouldRender()) {
                    auto sideRect = calculateSideRect(sides.outerBorder, sides.edges, side);
                    path.addRect(sideRect); // FIXME: Need pixel snapping here.
                }
            }

            graphicsContext.setFillRule(WindRule::NonZero);
            graphicsContext.setFillColor(sides.edges.at(*firstVisibleSide).color());
            graphicsContext.fillPath(path);
            return;
        }
    }

    bool clipToOuterBorder = sides.outerBorder.isRounded();
    GraphicsContextStateSaver stateSaver(graphicsContext, clipToOuterBorder && !sides.appliedClipAlready);
    if (clipToOuterBorder) {
        // Clip to the inner and outer radii rects.
        if (sides.bleedAvoidance != BackgroundBleedUseTransparencyLayer)
            graphicsContext.clipRoundedRect(sides.outerBorder.pixelSnappedRoundedRectForPainting(deviceScaleFactor));
        // isRenderable() check avoids issue described in https://bugs.webkit.org/show_bug.cgi?id=38787
        // The inside will be clipped out later (in clipBorderSideForComplexInnerPath)
        if (sides.innerBorder.isRenderable())
            graphicsContext.clipOutRoundedRect(sides.innerBorder.pixelSnappedRoundedRectForPainting(deviceScaleFactor));
    }

    // If only one edge visible antialiasing doesn't create seams
    bool antialias = shouldAntialiasLines(graphicsContext) || numEdgesVisible == 1;
    IntPoint innerBorderAdjustment(sides.innerBorder.rect().x() - sides.unadjustedInnerBorder.rect().x(), sides.innerBorder.rect().y() - sides.unadjustedInnerBorder.rect().y());
    if (haveAlphaColor)
        paintTranslucentBorderSides(sides.outerBorder, sides.unadjustedInnerBorder, innerBorderAdjustment, sides.edges, edgesToDraw, sides.radii, sides.bleedAvoidance, sides.includeLogicalLeftEdge, sides.includeLogicalRightEdge, antialias, sides.isHorizontal);
    else
        paintBorderSides(sides.outerBorder, sides.unadjustedInnerBorder, innerBorderAdjustment, sides.edges, edgesToDraw, sides.radii, sides.bleedAvoidance, sides.includeLogicalLeftEdge, sides.includeLogicalRightEdge, antialias, sides.isHorizontal);
}

bool BorderPainter::paintNinePieceImage(const LayoutRect& rect, const RenderStyle& style, const NinePieceImage& ninePieceImage, CompositeOperator op)
{
    StyleImage* styleImage = ninePieceImage.image();
    if (!styleImage)
        return false;

    if (!styleImage->isLoaded())
        return true; // Never paint a nine-piece image incrementally, but don't paint the fallback borders either.

    if (!styleImage->canRender(&m_renderer, style.effectiveZoom()))
        return false;

    if (!is<RenderBoxModelObject>(m_renderer))
        return false;

    // FIXME: border-image is broken with full page zooming when tiling has to happen, since the tiling function
    // doesn't have any understanding of the zoom that is in effect on the tile.
    float deviceScaleFactor = document().deviceScaleFactor();

    LayoutRect rectWithOutsets = rect;
    rectWithOutsets.expand(style.imageOutsets(ninePieceImage));
    LayoutRect destination = LayoutRect(snapRectToDevicePixels(rectWithOutsets, deviceScaleFactor));

    auto source = downcast<RenderBoxModelObject>(m_renderer).calculateImageIntrinsicDimensions(styleImage, destination.size(), RenderBoxModelObject::DoNotScaleByEffectiveZoom);

    // If both values are ‘auto’ then the intrinsic width and/or height of the image should be used, if any.
    styleImage->setContainerContextForRenderer(m_renderer, source, style.effectiveZoom());

    ninePieceImage.paint(m_paintInfo.context(), &m_renderer, style, destination, source, deviceScaleFactor, op);
    return true;
}

void BorderPainter::paintTranslucentBorderSides(const RoundedRect& outerBorder, const RoundedRect& innerBorder, const IntPoint& innerBorderAdjustment,
    const BorderEdges& edges, BoxSideSet edgesToDraw, std::optional<BorderData::Radii> radii, BackgroundBleedAvoidance bleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias, bool isHorizontal)
{
    // willBeOverdrawn assumes that we draw in order: top, bottom, left, right.
    // This is different from BoxSide enum order.
    static constexpr std::array<BoxSide, 4> paintOrderSides = { BoxSide::Top, BoxSide::Bottom, BoxSide::Left, BoxSide::Right };

    while (edgesToDraw) {
        // Find undrawn edges sharing a color.
        Color commonColor;

        BoxSideSet commonColorEdgeSet;
        for (auto side : paintOrderSides) {
            if (!edgesToDraw.contains(edgeFlagForSide(side)))
                continue;

            auto& edge = edges.at(side);
            bool includeEdge;
            if (commonColorEdgeSet.isEmpty()) {
                commonColor = edge.color();
                includeEdge = true;
            } else
                includeEdge = edge.color() == commonColor;

            if (includeEdge)
                commonColorEdgeSet.add(edgeFlagForSide(side));
        }

        bool useTransparencyLayer = includesAdjacentEdges(commonColorEdgeSet) && !commonColor.isOpaque();
        if (useTransparencyLayer) {
            m_paintInfo.context().beginTransparencyLayer(commonColor.alphaAsFloat());
            commonColor = commonColor.opaqueColor();
        }

        paintBorderSides(outerBorder, innerBorder, innerBorderAdjustment, edges, commonColorEdgeSet, radii, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias, isHorizontal, &commonColor);

        if (useTransparencyLayer)
            m_paintInfo.context().endTransparencyLayer();

        edgesToDraw.remove(commonColorEdgeSet);
    }
}

static bool borderWillArcInnerEdge(const LayoutSize& firstRadius, const LayoutSize& secondRadius)
{
    return !firstRadius.isZero() || !secondRadius.isZero();
}

inline bool styleRequiresClipPolygon(BorderStyle style)
{
    return style == BorderStyle::Dotted || style == BorderStyle::Dashed; // These are drawn with a stroke, so we have to clip to get corner miters.
}

static bool borderStyleFillsBorderArea(BorderStyle style)
{
    return !(style == BorderStyle::Dotted || style == BorderStyle::Dashed || style == BorderStyle::Double);
}

static bool borderStyleHasInnerDetail(BorderStyle style)
{
    return style == BorderStyle::Groove || style == BorderStyle::Ridge || style == BorderStyle::Double;
}

static bool borderStyleIsDottedOrDashed(BorderStyle style)
{
    return style == BorderStyle::Dotted || style == BorderStyle::Dashed;
}

// BorderStyle::Outset darkens the bottom and right (and maybe lightens the top and left)
// BorderStyle::Inset darkens the top and left (and maybe lightens the bottom and right)
static inline bool borderStyleHasUnmatchedColorsAtCorner(BorderStyle style, BoxSide side, BoxSide adjacentSide)
{
    // These styles match at the top/left and bottom/right.
    if (style == BorderStyle::Inset || style == BorderStyle::Groove || style == BorderStyle::Ridge || style == BorderStyle::Outset) {
        BoxSideSet topRightSides = { BoxSideFlag::Top, BoxSideFlag::Right };
        BoxSideSet bottomLeftSides = { BoxSideFlag::Bottom, BoxSideFlag::Left };

        BoxSideSet usedSides { edgeFlagForSide(side), edgeFlagForSide(adjacentSide) };
        return usedSides == topRightSides || usedSides == bottomLeftSides;
    }
    return false;
}

static inline bool colorsMatchAtCorner(BoxSide side, BoxSide adjacentSide, const BorderEdges& edges)
{
    auto& edge = edges.at(side);
    auto& adjacentEdge = edges.at(adjacentSide);

    if (edge.shouldRender() != adjacentEdge.shouldRender())
        return false;

    if (!edgesShareColor(edge, adjacentEdge))
        return false;

    return !borderStyleHasUnmatchedColorsAtCorner(edge.style(), side, adjacentSide);
}


static inline bool colorNeedsAntiAliasAtCorner(BoxSide side, BoxSide adjacentSide, const BorderEdges& edges)
{
    auto& edge = edges.at(side);
    auto& adjacentEdge = edges.at(adjacentSide);

    if (edge.color().isOpaque())
        return false;

    if (edge.shouldRender() != adjacentEdge.shouldRender())
        return false;

    if (!edgesShareColor(edge, adjacentEdge))
        return true;

    return borderStyleHasUnmatchedColorsAtCorner(edge.style(), side, adjacentSide);
}

// This assumes that we draw in order: top, bottom, left, right.
static inline bool willBeOverdrawn(BoxSide side, BoxSide adjacentSide, const BorderEdges& edges)
{
    switch (side) {
    case BoxSide::Top:
    case BoxSide::Bottom: {
        auto& edge = edges.at(side);
        auto& adjacentEdge = edges.at(adjacentSide);

        if (adjacentEdge.presentButInvisible())
            return false;

        if (!edgesShareColor(edge, adjacentEdge) && !adjacentEdge.color().isOpaque())
            return false;

        if (!borderStyleFillsBorderArea(adjacentEdge.style()))
            return false;

        return true;
    }
    case BoxSide::Left:
    case BoxSide::Right:
        // These draw last, so are never overdrawn.
        return false;
    }
    return false;
}

static inline bool borderStylesRequireMitre(BoxSide side, BoxSide adjacentSide, BorderStyle style, BorderStyle adjacentStyle)
{
    if (style == BorderStyle::Double || adjacentStyle == BorderStyle::Double || adjacentStyle == BorderStyle::Groove || adjacentStyle == BorderStyle::Ridge)
        return true;

    if (borderStyleIsDottedOrDashed(style) != borderStyleIsDottedOrDashed(adjacentStyle))
        return true;

    if (style != adjacentStyle)
        return true;

    return borderStyleHasUnmatchedColorsAtCorner(style, side, adjacentSide);
}

static bool joinRequiresMitre(BoxSide side, BoxSide adjacentSide, const BorderEdges& edges, bool allowOverdraw)
{
    auto& edge = edges.at(side);
    auto& adjacentEdge = edges.at(adjacentSide);

    if ((edge.isTransparent() && adjacentEdge.isTransparent()) || !adjacentEdge.isPresent())
        return false;

    if (allowOverdraw && willBeOverdrawn(side, adjacentSide, edges))
        return false;

    if (!edgesShareColor(edge, adjacentEdge))
        return true;

    if (borderStylesRequireMitre(side, adjacentSide, edge.style(), adjacentEdge.style()))
        return true;

    return false;
}

static RoundedRect calculateAdjustedInnerBorder(const RoundedRect&innerBorder, BoxSide side)
{
    // Expand the inner border as necessary to make it a rounded rect (i.e. radii contained within each edge).
    // This function relies on the fact we only get radii not contained within each edge if one of the radii
    // for an edge is zero, so we can shift the arc towards the zero radius corner.
    RoundedRect::Radii newRadii = innerBorder.radii();
    LayoutRect newRect = innerBorder.rect();

    float overshoot;
    float maxRadii;

    switch (side) {
    case BoxSide::Top:
        overshoot = newRadii.topLeft().width() + newRadii.topRight().width() - newRect.width();
        if (overshoot > 0) {
            ASSERT(!(newRadii.topLeft().width() && newRadii.topRight().width()));
            newRect.setWidth(newRect.width() + overshoot);
            if (!newRadii.topLeft().width())
                newRect.move(-overshoot, 0);
        }
        newRadii.setBottomLeft({ });
        newRadii.setBottomRight({ });
        maxRadii = std::max(newRadii.topLeft().height(), newRadii.topRight().height());
        if (maxRadii > newRect.height())
            newRect.setHeight(maxRadii);
        break;

    case BoxSide::Bottom:
        overshoot = newRadii.bottomLeft().width() + newRadii.bottomRight().width() - newRect.width();
        if (overshoot > 0) {
            ASSERT(!(newRadii.bottomLeft().width() && newRadii.bottomRight().width()));
            newRect.setWidth(newRect.width() + overshoot);
            if (!newRadii.bottomLeft().width())
                newRect.move(-overshoot, 0);
        }
        newRadii.setTopLeft({ });
        newRadii.setTopRight({ });
        maxRadii = std::max(newRadii.bottomLeft().height(), newRadii.bottomRight().height());
        if (maxRadii > newRect.height()) {
            newRect.move(0, newRect.height() - maxRadii);
            newRect.setHeight(maxRadii);
        }
        break;

    case BoxSide::Left:
        overshoot = newRadii.topLeft().height() + newRadii.bottomLeft().height() - newRect.height();
        if (overshoot > 0) {
            ASSERT(!(newRadii.topLeft().height() && newRadii.bottomLeft().height()));
            newRect.setHeight(newRect.height() + overshoot);
            if (!newRadii.topLeft().height())
                newRect.move(0, -overshoot);
        }
        newRadii.setTopRight({ });
        newRadii.setBottomRight({ });
        maxRadii = std::max(newRadii.topLeft().width(), newRadii.bottomLeft().width());
        if (maxRadii > newRect.width())
            newRect.setWidth(maxRadii);
        break;

    case BoxSide::Right:
        overshoot = newRadii.topRight().height() + newRadii.bottomRight().height() - newRect.height();
        if (overshoot > 0) {
            ASSERT(!(newRadii.topRight().height() && newRadii.bottomRight().height()));
            newRect.setHeight(newRect.height() + overshoot);
            if (!newRadii.topRight().height())
                newRect.move(0, -overshoot);
        }
        newRadii.setTopLeft({ });
        newRadii.setBottomLeft({ });
        maxRadii = std::max(newRadii.topRight().width(), newRadii.bottomRight().width());
        if (maxRadii > newRect.width()) {
            newRect.move(newRect.width() - maxRadii, 0);
            newRect.setWidth(maxRadii);
        }
        break;
    }

    return RoundedRect(newRect, newRadii);
}

void BorderPainter::paintBorderSides(const RoundedRect& outerBorder, const RoundedRect& innerBorder,
    const IntPoint& innerBorderAdjustment, const BorderEdges& edges, BoxSideSet edgeSet, std::optional<BorderData::Radii> radii, BackgroundBleedAvoidance bleedAvoidance,
    bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias, bool isHorizontal, const Color* overrideColor)
{
    bool renderRadii = outerBorder.isRounded();

    Path roundedPath;
    if (renderRadii)
        roundedPath.addRoundedRect(outerBorder);

    // The inner border adjustment for bleed avoidance mode BackgroundBleedBackgroundOverBorder
    // is only applied to sideRect, which is okay since BackgroundBleedBackgroundOverBorder
    // is only to be used for solid borders and the shape of the border painted by drawBoxSideFromPath
    // only depends on sideRect when painting solid borders.

    auto paintOneSide = [&](BoxSide side, BoxSide adjacentSide1, BoxSide adjacentSide2) {
        auto& edge = edges.at(side);
        if (!edge.shouldRender() || !edgeSet.contains(edgeFlagForSide(side)))
            return;

        LayoutRect sideRect = outerBorder.rect();
        LayoutSize firstRadius;
        LayoutSize secondRadius;

        switch (side) {
        case BoxSide::Top:
            sideRect.setHeight(edge.widthForPainting() + innerBorderAdjustment.y());
            firstRadius = innerBorder.radii().topLeft();
            secondRadius = innerBorder.radii().topRight();
            break;
        case BoxSide::Right:
            sideRect.shiftXEdgeTo(sideRect.maxX() - edge.widthForPainting() - innerBorderAdjustment.x());
            firstRadius = innerBorder.radii().bottomRight();
            secondRadius = innerBorder.radii().topRight();
            break;
        case BoxSide::Bottom:
            sideRect.shiftYEdgeTo(sideRect.maxY() - edge.widthForPainting() - innerBorderAdjustment.y());
            firstRadius = innerBorder.radii().bottomLeft();
            secondRadius = innerBorder.radii().bottomRight();
            break;
        case BoxSide::Left:
            sideRect.setWidth(edge.widthForPainting() + innerBorderAdjustment.x());
            firstRadius = innerBorder.radii().bottomLeft();
            secondRadius = innerBorder.radii().topLeft();
            break;
        }

        bool usePath = renderRadii && (borderStyleHasInnerDetail(edge.style()) || borderWillArcInnerEdge(firstRadius, secondRadius));
        paintOneBorderSide(outerBorder, innerBorder, sideRect, side, adjacentSide1, adjacentSide2, edges, radii, usePath ? &roundedPath : nullptr, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias, isHorizontal, overrideColor);
    };

    paintOneSide(BoxSide::Top, BoxSide::Left, BoxSide::Right);
    paintOneSide(BoxSide::Bottom, BoxSide::Left, BoxSide::Right);
    paintOneSide(BoxSide::Left, BoxSide::Top, BoxSide::Bottom);
    paintOneSide(BoxSide::Right, BoxSide::Top, BoxSide::Bottom);
}

void BorderPainter::paintOneBorderSide(const RoundedRect& outerBorder, const RoundedRect& innerBorder,
    const LayoutRect& sideRect, BoxSide side, BoxSide adjacentSide1, BoxSide adjacentSide2, const BorderEdges& edges, std::optional<BorderData::Radii> radii, const Path* path,
    BackgroundBleedAvoidance bleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias, bool isHorizontal, const Color* overrideColor)
{
    auto& edgeToRender = edges.at(side);
    ASSERT(edgeToRender.widthForPainting());
    auto& adjacentEdge1 = edges.at(adjacentSide1);
    auto& adjacentEdge2 = edges.at(adjacentSide2);

    bool mitreAdjacentSide1 = joinRequiresMitre(side, adjacentSide1, edges, !antialias);
    bool mitreAdjacentSide2 = joinRequiresMitre(side, adjacentSide2, edges, !antialias);

    bool adjacentSide1StylesMatch = colorsMatchAtCorner(side, adjacentSide1, edges);
    bool adjacentSide2StylesMatch = colorsMatchAtCorner(side, adjacentSide2, edges);

    const Color& colorToPaint = overrideColor ? *overrideColor : edgeToRender.color();

    auto& graphicsContext = m_paintInfo.context();

    if (path) {
        GraphicsContextStateSaver stateSaver(graphicsContext);

        clipBorderSidePolygon(outerBorder, innerBorder, side, adjacentSide1StylesMatch, adjacentSide2StylesMatch);

        if (!innerBorder.isRenderable())
            graphicsContext.clipOutRoundedRect(FloatRoundedRect(calculateAdjustedInnerBorder(innerBorder, side)));

        float thickness = std::max(std::max(edgeToRender.widthForPainting(), adjacentEdge1.widthForPainting()), adjacentEdge2.widthForPainting());
        drawBoxSideFromPath(outerBorder.rect(), *path, edges, radii, edgeToRender.widthForPainting(), thickness, side,
            colorToPaint, edgeToRender.style(), bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, isHorizontal);
    } else {
        bool clipForStyle = styleRequiresClipPolygon(edgeToRender.style()) && (mitreAdjacentSide1 || mitreAdjacentSide2);
        bool clipAdjacentSide1 = colorNeedsAntiAliasAtCorner(side, adjacentSide1, edges) && mitreAdjacentSide1;
        bool clipAdjacentSide2 = colorNeedsAntiAliasAtCorner(side, adjacentSide2, edges) && mitreAdjacentSide2;
        bool shouldClip = clipForStyle || clipAdjacentSide1 || clipAdjacentSide2;

        GraphicsContextStateSaver clipStateSaver(graphicsContext, shouldClip);
        if (shouldClip) {
            bool aliasAdjacentSide1 = clipAdjacentSide1 || (clipForStyle && mitreAdjacentSide1);
            bool aliasAdjacentSide2 = clipAdjacentSide2 || (clipForStyle && mitreAdjacentSide2);
            clipBorderSidePolygon(outerBorder, innerBorder, side, !aliasAdjacentSide1, !aliasAdjacentSide2);
            // Since we clipped, no need to draw with a mitre.
            mitreAdjacentSide1 = false;
            mitreAdjacentSide2 = false;
        }
        drawLineForBoxSide(graphicsContext, document(), sideRect, side, colorToPaint, edgeToRender.style(), mitreAdjacentSide1 ? adjacentEdge1.widthForPainting() : 0, mitreAdjacentSide2 ? adjacentEdge2.widthForPainting() : 0, antialias);
    }
}

void BorderPainter::drawBoxSideFromPath(const LayoutRect& borderRect, const Path& borderPath, const BorderEdges& edges,
    std::optional<BorderData::Radii> radii, float thickness, float drawThickness, BoxSide side, Color color, BorderStyle borderStyle, BackgroundBleedAvoidance bleedAvoidance,
    bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool isHorizontal)
{
    if (thickness <= 0)
        return;

    auto& graphicsContext = m_paintInfo.context();

    if (borderStyle == BorderStyle::Double && thickness < 3)
        borderStyle = BorderStyle::Solid;

    switch (borderStyle) {
    case BorderStyle::None:
    case BorderStyle::Hidden:
        return;
    case BorderStyle::Dotted:
    case BorderStyle::Dashed: {
        graphicsContext.setStrokeColor(color);

        // The stroke is doubled here because the provided path is the
        // outside edge of the border so half the stroke is clipped off.
        // The extra multiplier is so that the clipping mask can antialias
        // the edges to prevent jaggies.
        graphicsContext.setStrokeThickness(drawThickness * 2 * 1.1f);
        graphicsContext.setStrokeStyle(borderStyle == BorderStyle::Dashed ? DashedStroke : DottedStroke);

        // If the number of dashes that fit in the path is odd and non-integral then we
        // will have an awkwardly-sized dash at the end of the path. To try to avoid that
        // here, we simply make the whitespace dashes ever so slightly bigger.
        // FIXME: This could be even better if we tried to manipulate the dash offset
        // and possibly the gapLength to get the corners dash-symmetrical.
        float dashLength = thickness * ((borderStyle == BorderStyle::Dashed) ? 3.0f : 1.0f);
        float gapLength = dashLength;
        float numberOfDashes = borderPath.length() / dashLength;
        // Don't try to show dashes if we have less than 2 dashes + 2 gaps.
        // FIXME: should do this test per side.
        if (numberOfDashes >= 4) {
            bool evenNumberOfFullDashes = !((int)numberOfDashes % 2);
            bool integralNumberOfDashes = !(numberOfDashes - (int)numberOfDashes);
            if (!evenNumberOfFullDashes && !integralNumberOfDashes) {
                float numberOfGaps = numberOfDashes / 2;
                gapLength += (dashLength  / numberOfGaps);
            }

            auto lineDash = DashArray::from(dashLength, gapLength);
            graphicsContext.setLineDash(WTFMove(lineDash), dashLength);
        }

        // FIXME: stroking the border path causes issues with tight corners:
        // https://bugs.webkit.org/show_bug.cgi?id=58711
        // Also, to get the best appearance we should stroke a path between the two borders.
        graphicsContext.strokePath(borderPath);
        return;
    }
    case BorderStyle::Double: {
        // Get the inner border rects for both the outer border line and the inner border line
        LayoutUnit outerBorderTopWidth;
        LayoutUnit innerBorderTopWidth;
        edges.top().getDoubleBorderStripeWidths(outerBorderTopWidth, innerBorderTopWidth);

        LayoutUnit outerBorderRightWidth;
        LayoutUnit innerBorderRightWidth;
        edges.right().getDoubleBorderStripeWidths(outerBorderRightWidth, innerBorderRightWidth);

        LayoutUnit outerBorderBottomWidth;
        LayoutUnit innerBorderBottomWidth;
        edges.bottom().getDoubleBorderStripeWidths(outerBorderBottomWidth, innerBorderBottomWidth);

        LayoutUnit outerBorderLeftWidth;
        LayoutUnit innerBorderLeftWidth;
        edges.left().getDoubleBorderStripeWidths(outerBorderLeftWidth, innerBorderLeftWidth);

        // Draw inner border line
        {
            GraphicsContextStateSaver stateSaver(graphicsContext);
            auto innerClip = RenderStyle::getRoundedInnerBorderFor(borderRect,
                innerBorderTopWidth, innerBorderBottomWidth, innerBorderLeftWidth, innerBorderRightWidth,
                radii, isHorizontal,
                includeLogicalLeftEdge, includeLogicalRightEdge);

            graphicsContext.clipRoundedRect(FloatRoundedRect(innerClip));
            drawBoxSideFromPath(borderRect, borderPath, edges, radii, thickness, drawThickness, side, color, BorderStyle::Solid, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, isHorizontal);
        }

        // Draw outer border line
        {
            GraphicsContextStateSaver stateSaver(graphicsContext);
            LayoutRect outerRect = borderRect;
            if (bleedAvoidance == BackgroundBleedUseTransparencyLayer) {
                outerRect.inflate(1);
                ++outerBorderTopWidth;
                ++outerBorderBottomWidth;
                ++outerBorderLeftWidth;
                ++outerBorderRightWidth;
            }

            auto outerClip = RenderStyle::getRoundedInnerBorderFor(outerRect,
                outerBorderTopWidth, outerBorderBottomWidth, outerBorderLeftWidth, outerBorderRightWidth,
                radii, isHorizontal,
                includeLogicalLeftEdge, includeLogicalRightEdge);
            graphicsContext.clipOutRoundedRect(FloatRoundedRect(outerClip));
            drawBoxSideFromPath(borderRect, borderPath, edges, radii,  thickness, drawThickness, side, color, BorderStyle::Solid, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, isHorizontal);
        }
        return;
    }
    case BorderStyle::Ridge:
    case BorderStyle::Groove:
    {
        BorderStyle s1;
        BorderStyle s2;
        if (borderStyle == BorderStyle::Groove) {
            s1 = BorderStyle::Inset;
            s2 = BorderStyle::Outset;
        } else {
            s1 = BorderStyle::Outset;
            s2 = BorderStyle::Inset;
        }

        // Paint full border
        drawBoxSideFromPath(borderRect, borderPath, edges, radii,  thickness, drawThickness, side, color, s1, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, isHorizontal);

        // Paint inner only
        GraphicsContextStateSaver stateSaver(graphicsContext);
        LayoutUnit topWidth { edges.top().widthForPainting() / 2 };
        LayoutUnit bottomWidth { edges.bottom().widthForPainting() / 2 };
        LayoutUnit leftWidth { edges.left().widthForPainting() / 2 };
        LayoutUnit rightWidth { edges.right().widthForPainting() / 2 };

        auto clipRect = RenderStyle::getRoundedInnerBorderFor(borderRect,
            topWidth, bottomWidth, leftWidth, rightWidth,
            radii, isHorizontal,
            includeLogicalLeftEdge, includeLogicalRightEdge);

        graphicsContext.clipRoundedRect(FloatRoundedRect(clipRect));
        drawBoxSideFromPath(borderRect, borderPath, edges, radii,  thickness, drawThickness, side, color, s2, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, isHorizontal);
        return;
    }
    case BorderStyle::Inset:
    case BorderStyle::Outset:
        color = calculateBorderStyleColor(borderStyle, side, color);
        break;
    default:
        break;
    }

    graphicsContext.setStrokeStyle(NoStroke);
    graphicsContext.setFillColor(color);
    graphicsContext.drawRect(snapRectToDevicePixels(borderRect, document().deviceScaleFactor()));
}

void BorderPainter::clipBorderSidePolygon(const RoundedRect& outerBorder, const RoundedRect& innerBorder, BoxSide side, bool firstEdgeMatches, bool secondEdgeMatches)
{
    auto& graphicsContext = m_paintInfo.context();

    float deviceScaleFactor = document().deviceScaleFactor();
    const FloatRect& outerRect = snapRectToDevicePixels(outerBorder.rect(), deviceScaleFactor);
    const FloatRect& innerRect = snapRectToDevicePixels(innerBorder.rect(), deviceScaleFactor);

    // For each side, create a quad that encompasses all parts of that side that may draw,
    // including areas inside the innerBorder.
    //
    //         0----------------3
    //       0  \              /  0
    //       |\  1----------- 2  /|
    //       | 1                1 |
    //       | |                | |
    //       | |                | |
    //       | 2                2 |
    //       |/  1------------2  \|
    //       3  /              \  3
    //         0----------------3
    //
    Vector<FloatPoint> quad;
    switch (side) {
    case BoxSide::Top:
        quad = { outerRect.minXMinYCorner(), innerRect.minXMinYCorner(), innerRect.maxXMinYCorner(), outerRect.maxXMinYCorner() };

        if (!innerBorder.radii().topLeft().isZero())
            findIntersection(outerRect.minXMinYCorner(), innerRect.minXMinYCorner(), innerRect.minXMaxYCorner(), innerRect.maxXMinYCorner(), quad[1]);

        if (!innerBorder.radii().topRight().isZero())
            findIntersection(outerRect.maxXMinYCorner(), innerRect.maxXMinYCorner(), innerRect.minXMinYCorner(), innerRect.maxXMaxYCorner(), quad[2]);
        break;

    case BoxSide::Left:
        quad = { outerRect.minXMinYCorner(), innerRect.minXMinYCorner(), innerRect.minXMaxYCorner(), outerRect.minXMaxYCorner() };

        if (!innerBorder.radii().topLeft().isZero())
            findIntersection(outerRect.minXMinYCorner(), innerRect.minXMinYCorner(), innerRect.minXMaxYCorner(), innerRect.maxXMinYCorner(), quad[1]);

        if (!innerBorder.radii().bottomLeft().isZero())
            findIntersection(outerRect.minXMaxYCorner(), innerRect.minXMaxYCorner(), innerRect.minXMinYCorner(), innerRect.maxXMaxYCorner(), quad[2]);
        break;

    case BoxSide::Bottom:
        quad = { outerRect.minXMaxYCorner(), innerRect.minXMaxYCorner(), innerRect.maxXMaxYCorner(), outerRect.maxXMaxYCorner() };

        if (!innerBorder.radii().bottomLeft().isZero())
            findIntersection(outerRect.minXMaxYCorner(), innerRect.minXMaxYCorner(), innerRect.minXMinYCorner(), innerRect.maxXMaxYCorner(), quad[1]);

        if (!innerBorder.radii().bottomRight().isZero())
            findIntersection(outerRect.maxXMaxYCorner(), innerRect.maxXMaxYCorner(), innerRect.maxXMinYCorner(), innerRect.minXMaxYCorner(), quad[2]);
        break;

    case BoxSide::Right:
        quad = { outerRect.maxXMinYCorner(), innerRect.maxXMinYCorner(), innerRect.maxXMaxYCorner(), outerRect.maxXMaxYCorner() };

        if (!innerBorder.radii().topRight().isZero())
            findIntersection(outerRect.maxXMinYCorner(), innerRect.maxXMinYCorner(), innerRect.minXMinYCorner(), innerRect.maxXMaxYCorner(), quad[1]);

        if (!innerBorder.radii().bottomRight().isZero())
            findIntersection(outerRect.maxXMaxYCorner(), innerRect.maxXMaxYCorner(), innerRect.maxXMinYCorner(), innerRect.minXMaxYCorner(), quad[2]);
        break;
    }

    // If the border matches both of its adjacent sides, don't anti-alias the clip, and
    // if neither side matches, anti-alias the clip.
    if (firstEdgeMatches == secondEdgeMatches) {
        bool wasAntialiased = graphicsContext.shouldAntialias();
        graphicsContext.setShouldAntialias(!firstEdgeMatches);
        graphicsContext.clipPath(Path::polygonPathFromPoints(quad), WindRule::NonZero);
        graphicsContext.setShouldAntialias(wasAntialiased);
        return;
    }

    // Square off the end which shouldn't be affected by antialiasing, and clip.
    Vector<FloatPoint> firstQuad = {
        quad[0],
        quad[1],
        quad[2],
        side == BoxSide::Top || side == BoxSide::Bottom ? FloatPoint(quad[3].x(), quad[2].y()) : FloatPoint(quad[2].x(), quad[3].y()),
        quad[3]
    };
    bool wasAntialiased = graphicsContext.shouldAntialias();
    graphicsContext.setShouldAntialias(!firstEdgeMatches);
    graphicsContext.clipPath(Path::polygonPathFromPoints(firstQuad), WindRule::NonZero);

    Vector<FloatPoint> secondQuad = {
        quad[0],
        side == BoxSide::Top || side == BoxSide::Bottom ? FloatPoint(quad[0].x(), quad[1].y()) : FloatPoint(quad[1].x(), quad[0].y()),
        quad[1],
        quad[2],
        quad[3]
    };
    // Antialiasing affects the second side.
    graphicsContext.setShouldAntialias(!secondEdgeMatches);
    graphicsContext.clipPath(Path::polygonPathFromPoints(secondQuad), WindRule::NonZero);

    graphicsContext.setShouldAntialias(wasAntialiased);
}

void BorderPainter::drawLineForBoxSide(GraphicsContext& graphicsContext, const Document& document, const FloatRect& rect, BoxSide side, Color color, BorderStyle borderStyle, float adjacentWidth1, float adjacentWidth2, bool antialias)
{
    auto drawBorderRect = [&graphicsContext](const FloatRect& rect)
    {
        if (rect.isEmpty())
            return;
        graphicsContext.drawRect(rect);
    };

    auto drawLineFor = [&graphicsContext, &document, color, antialias](const FloatRect& rect, BoxSide side, BorderStyle borderStyle, const FloatSize& adjacent)
    {
        if (rect.isEmpty())
            return;
        drawLineForBoxSide(graphicsContext, document, rect, side, color, borderStyle, adjacent.width(), adjacent.height(), antialias);
    };

    float x1 = rect.x();
    float x2 = rect.maxX();
    float y1 = rect.y();
    float y2 = rect.maxY();
    float thickness;
    float length;
    if (side == BoxSide::Top || side == BoxSide::Bottom) {
        thickness = y2 - y1;
        length = x2 - x1;
    } else {
        thickness = x2 - x1;
        length = y2 - y1;
    }
    // FIXME: We really would like this check to be an ASSERT as we don't want to draw empty borders. However
    // nothing guarantees that the following recursive calls to drawLineForBoxSide will have non-null dimensions.
    if (!thickness || !length)
        return;

    float deviceScaleFactor = document.deviceScaleFactor();
    if (borderStyle == BorderStyle::Double && (thickness * deviceScaleFactor) < 3)
        borderStyle = BorderStyle::Solid;

    switch (borderStyle) {
    case BorderStyle::None:
    case BorderStyle::Hidden:
        return;
    case BorderStyle::Dotted:
    case BorderStyle::Dashed: {
        bool wasAntialiased = graphicsContext.shouldAntialias();
        StrokeStyle oldStrokeStyle = graphicsContext.strokeStyle();
        graphicsContext.setShouldAntialias(antialias);
        graphicsContext.setStrokeColor(color);
        graphicsContext.setStrokeThickness(thickness);
        graphicsContext.setStrokeStyle(borderStyle == BorderStyle::Dashed ? DashedStroke : DottedStroke);
        graphicsContext.drawLine(roundPointToDevicePixels(LayoutPoint(x1, y1), deviceScaleFactor), roundPointToDevicePixels(LayoutPoint(x2, y2), deviceScaleFactor));
        graphicsContext.setShouldAntialias(wasAntialiased);
        graphicsContext.setStrokeStyle(oldStrokeStyle);
        break;
    }
    case BorderStyle::Double: {
        float thirdOfThickness = ceilToDevicePixel(thickness / 3, deviceScaleFactor);
        ASSERT(thirdOfThickness);

        if (!adjacentWidth1 && !adjacentWidth2) {
            StrokeStyle oldStrokeStyle = graphicsContext.strokeStyle();
            graphicsContext.setStrokeStyle(NoStroke);
            graphicsContext.setFillColor(color);

            bool wasAntialiased = graphicsContext.shouldAntialias();
            graphicsContext.setShouldAntialias(antialias);

            switch (side) {
            case BoxSide::Top:
            case BoxSide::Bottom:
                drawBorderRect(snapRectToDevicePixels(LayoutRect(x1, y1, length, thirdOfThickness), deviceScaleFactor));
                drawBorderRect(snapRectToDevicePixels(LayoutRect(x1, y2 - thirdOfThickness, length, thirdOfThickness), deviceScaleFactor));
                break;
            case BoxSide::Left:
            case BoxSide::Right:
                drawBorderRect(snapRectToDevicePixels(LayoutRect(x1, y1, thirdOfThickness, length), deviceScaleFactor));
                drawBorderRect(snapRectToDevicePixels(LayoutRect(x2 - thirdOfThickness, y1, thirdOfThickness, length), deviceScaleFactor));
                break;
            }

            graphicsContext.setShouldAntialias(wasAntialiased);
            graphicsContext.setStrokeStyle(oldStrokeStyle);
        } else {
            float adjacent1BigThird = ceilToDevicePixel(adjacentWidth1 / 3, deviceScaleFactor);
            float adjacent2BigThird = ceilToDevicePixel(adjacentWidth2 / 3, deviceScaleFactor);

            float offset1 = floorToDevicePixel(fabs(adjacentWidth1) * 2 / 3, deviceScaleFactor);
            float offset2 = floorToDevicePixel(fabs(adjacentWidth2) * 2 / 3, deviceScaleFactor);

            float mitreOffset1 = adjacentWidth1 < 0 ? offset1 : 0;
            float mitreOffset2 = adjacentWidth1 > 0 ? offset1 : 0;
            float mitreOffset3 = adjacentWidth2 < 0 ? offset2 : 0;
            float mitreOffset4 = adjacentWidth2 > 0 ? offset2 : 0;

            FloatRect paintBorderRect;
            switch (side) {
            case BoxSide::Top:
                paintBorderRect = snapRectToDevicePixels(LayoutRect(x1 + mitreOffset1, y1, (x2 - mitreOffset3) - (x1 + mitreOffset1), thirdOfThickness), deviceScaleFactor);
                drawLineFor(paintBorderRect, side, BorderStyle::Solid, FloatSize(adjacent1BigThird, adjacent2BigThird));

                paintBorderRect = snapRectToDevicePixels(LayoutRect(x1 + mitreOffset2, y2 - thirdOfThickness, (x2 - mitreOffset4) - (x1 + mitreOffset2), thirdOfThickness), deviceScaleFactor);
                drawLineFor(paintBorderRect, side, BorderStyle::Solid, FloatSize(adjacent1BigThird, adjacent2BigThird));
                break;
            case BoxSide::Left:
                paintBorderRect = snapRectToDevicePixels(LayoutRect(x1, y1 + mitreOffset1, thirdOfThickness, (y2 - mitreOffset3) - (y1 + mitreOffset1)), deviceScaleFactor);
                drawLineFor(paintBorderRect, side, BorderStyle::Solid, FloatSize(adjacent1BigThird, adjacent2BigThird));

                paintBorderRect = snapRectToDevicePixels(LayoutRect(x2 - thirdOfThickness, y1 + mitreOffset2, thirdOfThickness, (y2 - mitreOffset4) - (y1 + mitreOffset2)), deviceScaleFactor);
                drawLineFor(paintBorderRect, side, BorderStyle::Solid, FloatSize(adjacent1BigThird, adjacent2BigThird));
                break;
            case BoxSide::Bottom:
                paintBorderRect = snapRectToDevicePixels(LayoutRect(x1 + mitreOffset2, y1, (x2 - mitreOffset4) - (x1 + mitreOffset2), thirdOfThickness), deviceScaleFactor);
                drawLineFor(paintBorderRect, side, BorderStyle::Solid, FloatSize(adjacent1BigThird, adjacent2BigThird));

                paintBorderRect = snapRectToDevicePixels(LayoutRect(x1 + mitreOffset1, y2 - thirdOfThickness, (x2 - mitreOffset3) - (x1 + mitreOffset1), thirdOfThickness), deviceScaleFactor);
                drawLineFor(paintBorderRect, side, BorderStyle::Solid, FloatSize(adjacent1BigThird, adjacent2BigThird));
                break;
            case BoxSide::Right:
                paintBorderRect = snapRectToDevicePixels(LayoutRect(x1, y1 + mitreOffset2, thirdOfThickness, (y2 - mitreOffset4) - (y1 + mitreOffset2)), deviceScaleFactor);
                drawLineFor(paintBorderRect, side, BorderStyle::Solid, FloatSize(adjacent1BigThird, adjacent2BigThird));

                paintBorderRect = snapRectToDevicePixels(LayoutRect(x2 - thirdOfThickness, y1 + mitreOffset1, thirdOfThickness, (y2 - mitreOffset3) - (y1 + mitreOffset1)), deviceScaleFactor);
                drawLineFor(paintBorderRect, side, BorderStyle::Solid, FloatSize(adjacent1BigThird, adjacent2BigThird));
                break;
            default:
                break;
            }
        }
        break;
    }
    case BorderStyle::Ridge:
    case BorderStyle::Groove: {
        BorderStyle s1;
        BorderStyle s2;
        if (borderStyle == BorderStyle::Groove) {
            s1 = BorderStyle::Inset;
            s2 = BorderStyle::Outset;
        } else {
            s1 = BorderStyle::Outset;
            s2 = BorderStyle::Inset;
        }

        float adjacent1BigHalf = ceilToDevicePixel(adjacentWidth1 / 2, deviceScaleFactor);
        float adjacent2BigHalf = ceilToDevicePixel(adjacentWidth2 / 2, deviceScaleFactor);

        float adjacent1SmallHalf = floorToDevicePixel(adjacentWidth1 / 2, deviceScaleFactor);
        float adjacent2SmallHalf = floorToDevicePixel(adjacentWidth2 / 2, deviceScaleFactor);

        float offset1 = 0;
        float offset2 = 0;
        float offset3 = 0;
        float offset4 = 0;

        if (((side == BoxSide::Top || side == BoxSide::Left) && adjacentWidth1 < 0) || ((side == BoxSide::Bottom || side == BoxSide::Right) && adjacentWidth1 > 0))
            offset1 = floorToDevicePixel(adjacentWidth1 / 2, deviceScaleFactor);

        if (((side == BoxSide::Top || side == BoxSide::Left) && adjacentWidth2 < 0) || ((side == BoxSide::Bottom || side == BoxSide::Right) && adjacentWidth2 > 0))
            offset2 = ceilToDevicePixel(adjacentWidth2 / 2, deviceScaleFactor);

        if (((side == BoxSide::Top || side == BoxSide::Left) && adjacentWidth1 > 0) || ((side == BoxSide::Bottom || side == BoxSide::Right) && adjacentWidth1 < 0))
            offset3 = floorToDevicePixel(fabs(adjacentWidth1) / 2, deviceScaleFactor);

        if (((side == BoxSide::Top || side == BoxSide::Left) && adjacentWidth2 > 0) || ((side == BoxSide::Bottom || side == BoxSide::Right) && adjacentWidth2 < 0))
            offset4 = ceilToDevicePixel(adjacentWidth2 / 2, deviceScaleFactor);

        float adjustedX = ceilToDevicePixel((x1 + x2) / 2, deviceScaleFactor);
        float adjustedY = ceilToDevicePixel((y1 + y2) / 2, deviceScaleFactor);
        // Quads can't use the default snapping rect functions.
        x1 = roundToDevicePixel(x1, deviceScaleFactor);
        x2 = roundToDevicePixel(x2, deviceScaleFactor);
        y1 = roundToDevicePixel(y1, deviceScaleFactor);
        y2 = roundToDevicePixel(y2, deviceScaleFactor);

        switch (side) {
        case BoxSide::Top:
            drawLineFor(FloatRect(FloatPoint(x1 + offset1, y1), FloatPoint(x2 - offset2, adjustedY)), side, s1, FloatSize(adjacent1BigHalf, adjacent2BigHalf));
            drawLineFor(FloatRect(FloatPoint(x1 + offset3, adjustedY), FloatPoint(x2 - offset4, y2)), side, s2, FloatSize(adjacent1SmallHalf, adjacent2SmallHalf));
            break;
        case BoxSide::Left:
            drawLineFor(FloatRect(FloatPoint(x1, y1 + offset1), FloatPoint(adjustedX, y2 - offset2)), side, s1, FloatSize(adjacent1BigHalf, adjacent2BigHalf));
            drawLineFor(FloatRect(FloatPoint(adjustedX, y1 + offset3), FloatPoint(x2, y2 - offset4)), side, s2, FloatSize(adjacent1SmallHalf, adjacent2SmallHalf));
            break;
        case BoxSide::Bottom:
            drawLineFor(FloatRect(FloatPoint(x1 + offset1, y1), FloatPoint(x2 - offset2, adjustedY)), side, s2, FloatSize(adjacent1BigHalf, adjacent2BigHalf));
            drawLineFor(FloatRect(FloatPoint(x1 + offset3, adjustedY), FloatPoint(x2 - offset4, y2)), side, s1, FloatSize(adjacent1SmallHalf, adjacent2SmallHalf));
            break;
        case BoxSide::Right:
            drawLineFor(FloatRect(FloatPoint(x1, y1 + offset1), FloatPoint(adjustedX, y2 - offset2)), side, s2, FloatSize(adjacent1BigHalf, adjacent2BigHalf));
            drawLineFor(FloatRect(FloatPoint(adjustedX, y1 + offset3), FloatPoint(x2, y2 - offset4)), side, s1, FloatSize(adjacent1SmallHalf, adjacent2SmallHalf));
            break;
        }
        break;
    }
    case BorderStyle::Inset:
    case BorderStyle::Outset:
        color = calculateBorderStyleColor(borderStyle, side, color);
        FALLTHROUGH;
    case BorderStyle::Solid: {
        StrokeStyle oldStrokeStyle = graphicsContext.strokeStyle();
        ASSERT(x2 >= x1);
        ASSERT(y2 >= y1);
        if (!adjacentWidth1 && !adjacentWidth2) {
            graphicsContext.setStrokeStyle(NoStroke);
            graphicsContext.setFillColor(color);
            bool wasAntialiased = graphicsContext.shouldAntialias();
            graphicsContext.setShouldAntialias(antialias);
            drawBorderRect(snapRectToDevicePixels(LayoutRect(x1, y1, x2 - x1, y2 - y1), deviceScaleFactor));
            graphicsContext.setShouldAntialias(wasAntialiased);
            graphicsContext.setStrokeStyle(oldStrokeStyle);
            return;
        }

        // FIXME: These roundings should be replaced by ASSERT(device pixel positioned) when all the callers have transitioned to device pixels.
        x1 = roundToDevicePixel(x1, deviceScaleFactor);
        y1 = roundToDevicePixel(y1, deviceScaleFactor);
        x2 = roundToDevicePixel(x2, deviceScaleFactor);
        y2 = roundToDevicePixel(y2, deviceScaleFactor);

        Vector<FloatPoint> quad;
        switch (side) {
        case BoxSide::Top:
            quad = {
                { x1 + std::max<float>(-adjacentWidth1, 0), y1 },
                { x1 + std::max<float>(adjacentWidth1, 0), y2 },
                { x2 - std::max<float>(adjacentWidth2, 0), y2 },
                { x2 - std::max<float>(-adjacentWidth2, 0), y1 }
            };
            break;
        case BoxSide::Bottom:
            quad = {
                { x1 + std::max<float>(adjacentWidth1, 0), y1 },
                { x1 + std::max<float>(-adjacentWidth1, 0), y2 },
                { x2 - std::max<float>(-adjacentWidth2, 0), y2 },
                { x2 - std::max<float>(adjacentWidth2, 0), y1 }
            };
            break;
        case BoxSide::Left:
            quad = {
                { x1, y1 + std::max<float>(-adjacentWidth1, 0) },
                { x1, y2 - std::max<float>(-adjacentWidth2, 0) },
                { x2, y2 - std::max<float>(adjacentWidth2, 0) },
                { x2, y1 + std::max<float>(adjacentWidth1, 0) }
            };
            break;
        case BoxSide::Right:
            quad = {
                { x1, y1 + std::max<float>(adjacentWidth1, 0) },
                { x1, y2 - std::max<float>(adjacentWidth2, 0) },
                { x2, y2 - std::max<float>(-adjacentWidth2, 0) },
                { x2, y1 + std::max<float>(-adjacentWidth1, 0) }
            };
            break;
        }

        graphicsContext.setStrokeStyle(NoStroke);
        graphicsContext.setFillColor(color);
        bool wasAntialiased = graphicsContext.shouldAntialias();
        graphicsContext.setShouldAntialias(antialias);
        graphicsContext.fillPath(Path::polygonPathFromPoints(quad));
        graphicsContext.setShouldAntialias(wasAntialiased);

        graphicsContext.setStrokeStyle(oldStrokeStyle);
        break;
    }
    }
}

LayoutRect BorderPainter::borderInnerRectAdjustedForBleedAvoidance(const LayoutRect& rect, BackgroundBleedAvoidance bleedAvoidance) const
{
    if (bleedAvoidance != BackgroundBleedBackgroundOverBorder)
        return rect;

    // We shrink the rectangle by one device pixel on each side to make it fully overlap the anti-aliased background border
    return shrinkRectByOneDevicePixel(m_paintInfo.context(), rect, document().deviceScaleFactor());
}

bool BorderPainter::shouldAntialiasLines(GraphicsContext& context)
{
    // FIXME: We may want to not antialias when scaled by an integral value,
    // and we may want to antialias when translated by a non-integral value.
    return !context.getCTM().isIdentityOrTranslationOrFlipped();
}

Color BorderPainter::calculateBorderStyleColor(const BorderStyle& style, const BoxSide& side, const Color& color)
{
    ASSERT(style == BorderStyle::Inset || style == BorderStyle::Outset);

    // This values were derived empirically.
    constexpr float baseDarkColorLuminance { 0.014443844f }; // Luminance of SRGBA<uint8_t> { 32, 32, 32 }
    constexpr float baseLightColorLuminance { 0.83077f }; // Luminance of SRGBA<uint8_t> { 235, 235, 235 }

    enum Operation { Darken, Lighten };

    Operation operation = (side == BoxSide::Top || side == BoxSide::Left) == (style == BorderStyle::Inset) ? Darken : Lighten;

    // Here we will darken the border decoration color when needed. This will yield a similar behavior as in FF.
    if (operation == Darken) {
        if (color.luminance() > baseDarkColorLuminance)
            return color.darkened();
    } else {
        if (color.luminance() < baseLightColorLuminance)
            return color.lightened();
    }
    return color;
}

const Document& BorderPainter::document() const
{
    return m_renderer.document();
}

}
