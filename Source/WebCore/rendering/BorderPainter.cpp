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

#include "BorderData.h"
#include "BorderEdge.h"
#include "BorderShape.h"
#include "CachedImage.h"
#include "FloatRoundedRect.h"
#include "GeometryUtilities.h"
#include "GraphicsContext.h"
#include "HTMLSelectElement.h"
#include "InlineIteratorInlineBox.h"
#include "InlineIteratorLineBox.h"
#include "LegacyRenderSVGModelObject.h"
#include "NinePieceImagePainter.h"
#include "PaintInfo.h"
#include "PathUtilities.h"
#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderInline.h"
#include "RenderListBox.h"
#include "RenderObjectDocument.h"
#include "RenderStyleInlines.h"
#include "RenderSVGModelObject.h"
#include "RenderTheme.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include <numeric>

namespace WebCore {

static bool borderStyleFillsBorderArea(BorderStyle style)
{
    switch (style) {
    case BorderStyle::None:
    case BorderStyle::Hidden:
    case BorderStyle::Inset:
    case BorderStyle::Groove:
    case BorderStyle::Outset:
    case BorderStyle::Ridge:
    case BorderStyle::Solid:
        return true;
    case BorderStyle::Dotted:
    case BorderStyle::Dashed:
    case BorderStyle::Double:
        return false;
    }
    return true;
}

static bool styleRequiresClipPolygon(BorderStyle style)
{
    switch (style) {
    case BorderStyle::None:
    case BorderStyle::Hidden:
    case BorderStyle::Inset:
    case BorderStyle::Groove:
    case BorderStyle::Outset:
    case BorderStyle::Ridge:
    case BorderStyle::Solid:
    case BorderStyle::Double:
        return false;
    case BorderStyle::Dotted:
    case BorderStyle::Dashed:
        // These are drawn with a stroke, so we have to clip to get corner miters.
        return true;
    }

    return false;
}

static bool borderStyleHasInnerDetail(BorderStyle style)
{
    switch (style) {
    case BorderStyle::None:
    case BorderStyle::Hidden:
    case BorderStyle::Inset:
    case BorderStyle::Outset:
    case BorderStyle::Solid:
    case BorderStyle::Dotted:
    case BorderStyle::Dashed:
        return false;

    case BorderStyle::Groove:
    case BorderStyle::Ridge:
    case BorderStyle::Double:
        return true;
    }

    return false;
}

static bool borderStyleIsDottedOrDashed(BorderStyle style)
{
    return style == BorderStyle::Dotted || style == BorderStyle::Dashed;
}

static bool decorationHasAllSimpleEdges(const RectEdges<BorderEdge>& edges)
{
    for (auto side : allBoxSides) {
        auto& currEdge = edges.at(side);

        if (!currEdge.widthForPainting())
            continue;

        if (!borderStyleFillsBorderArea(currEdge.style()))
            return false;
    }
    return true;
}

BorderPainter::BorderPainter(const RenderElement& renderer, const PaintInfo& paintInfo)
    : m_renderer(renderer)
    , m_paintInfo(paintInfo)
{
}

std::optional<Path> BorderPainter::pathForBorderArea(const LayoutRect& rect, const RenderStyle& style, float deviceScaleFactor, RectEdges<bool> closedEdges)
{
    auto edges = borderEdges(style, deviceScaleFactor, closedEdges);
    if (!decorationHasAllSimpleEdges(edges))
        return std::nullopt;

    auto borderShape = BorderShape::shapeForBorderRect(style, rect, closedEdges);
    return borderShape.pathForBorderArea(deviceScaleFactor);
}

static LayoutRect calculateSideRect(const LayoutRect& outerBorderRect, const BorderEdges& edges, BoxSide side)
{
    auto sideRect = outerBorderRect;
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

static LayoutSize sizeForDevicePixel(const GraphicsContext& context, float devicePixelRatio)
{
    auto transform = context.getCTM();
    return {
        ceilToDevicePixel(1_lu / transform.xScale(), devicePixelRatio),
        ceilToDevicePixel(1_lu / transform.yScale(), devicePixelRatio)
    };
}

LayoutRect shrinkRectByOneDevicePixel(const GraphicsContext& context, const LayoutRect& rect, float devicePixelRatio)
{
    auto shrunkRect = rect;
    auto devicePixelUnits = sizeForDevicePixel(context, devicePixelRatio);
    shrunkRect.inflate(-devicePixelUnits);
    return shrunkRect;
}

bool BorderPainter::decorationHasAllSolidEdges(const RectEdges<BorderEdge>& edges)
{
    for (auto side : allBoxSides) {
        auto& currEdge = edges.at(side);

        if (currEdge.presentButInvisible() || !currEdge.widthForPainting())
            continue;

        if (currEdge.style() != BorderStyle::Solid)
            return false;
    }
    return true;
}

void BorderPainter::paintBorder(const LayoutRect& rect, const RenderStyle& style, BleedAvoidance bleedAvoidance, RectEdges<bool> closedEdges) const
{
    auto& graphicsContext = m_paintInfo.context();

    if (graphicsContext.paintingDisabled())
        return;

    auto paintsBorderImage = [&](LayoutRect rect, const Style::BorderImage& borderImage) {
        auto image = borderImage.source().tryImage();
        if (!image)
            return false;

        if (!image->value->isLoaded(m_renderer.ptr()))
            return false;

        if (!image->value->canRender(m_renderer.ptr(), style.usedZoom()))
            return false;

        auto rectWithOutsets = rect;
        rectWithOutsets.expand(style.imageOutsets(borderImage));
        return !rectWithOutsets.isEmpty();
    };

    if (rect.isEmpty() && !paintsBorderImage(rect, style.borderImage()))
        return;

    auto rectToClipOut = const_cast<RenderElement&>(m_renderer.get()).paintRectToClipOutFromBorder(rect);
    bool appliedClipAlready = !rectToClipOut.isEmpty();
    GraphicsContextStateSaver stateSave(graphicsContext, appliedClipAlready);
    if (!rectToClipOut.isEmpty())
        graphicsContext.clipOut(snapRectToDevicePixels(rectToClipOut, document().deviceScaleFactor()));

    // border-image is not affected by border-radius.
    if (paintNinePieceImage(rect, style, style.borderImage()))
        return;

    auto [shape, edges] = [&]() {
        switch (bleedAvoidance) {
        case BleedAvoidance::None:
        case BleedAvoidance::ShrinkBackground:
        case BleedAvoidance::UseTransparencyLayer:
            return std::tuple<BorderShape, BorderEdges> {
                BorderShape::shapeForBorderRect(style, rect, closedEdges),
                borderEdges(style, document().deviceScaleFactor(), closedEdges, { }, m_paintInfo.paintBehavior.contains(PaintBehavior::ForceBlackBorder))
            };

        case BleedAvoidance::BackgroundOverBorder: {
            // Shrink the inner edge so there's no gap between the border and the background, which will be painted atop.
            auto shrinkAmount = sizeForDevicePixel(m_paintInfo.context(), document().deviceScaleFactor());
            auto edges = borderEdges(style, document().deviceScaleFactor(), closedEdges, shrinkAmount, m_paintInfo.paintBehavior.contains(PaintBehavior::ForceBlackBorder));
            auto borderWidths = RectEdges<LayoutUnit> {
                edges.top().width(),
                edges.right().width(),
                edges.bottom().width(),
                edges.left().width()
            };

            return std::tuple<BorderShape, BorderEdges> {
                BorderShape::shapeForBorderRect(style, rect, borderWidths, closedEdges),
                WTFMove(edges)
            };
        }
        }

        return std::tuple<BorderShape, BorderEdges> { BorderShape({ }, { 0_lu }), { } };
    }();

    bool outerEdgeIsRectangular = !shape.isRounded() || shape.outerShapeContains(m_paintInfo.rect);
    bool innerEdgeIsRectangular = shape.innerShapeIsRectangular();

    paintSides(shape, {
        style.hasBorderRadius() ? std::make_optional(style.borderRadii()) : std::nullopt,
        edges,
        decorationHasAllSolidEdges(edges),
        outerEdgeIsRectangular,
        innerEdgeIsRectangular,
        bleedAvoidance,
        closedEdges,
        appliedClipAlready,
    });
}

void BorderPainter::paintSides(const BorderShape& borderShape, const Sides& sides) const
{
    GraphicsContext& graphicsContext = m_paintInfo.context();

    ASSERT(!graphicsContext.paintingDisabled());

    // If no borders intersects with the dirty area, we can skip the painting.
    if (borderShape.innerShapeContains(m_paintInfo.rect))
        return;

    auto deviceScaleFactor = document().deviceScaleFactor();
    bool haveAlphaColor = false;
    bool haveAllDoubleEdges = true;
    int numEdgesVisible = 4;
    bool allEdgesShareColor = true;
    std::optional<BoxSide> firstVisibleSide;
    BoxSideSet edgesToDraw;

    for (auto boxSide : allBoxSides) {
        auto& currEdge = sides.edges.at(boxSide);

        if (currEdge.shouldRender())
            edgesToDraw.add(boxSide);

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
        else if (!equalIgnoringSemanticColor(currEdge.color(), sides.edges.at(*firstVisibleSide).color()))
            allEdgesShareColor = false;

        if (!currEdge.color().isOpaque())
            haveAlphaColor = true;

        if (currEdge.style() != BorderStyle::Double)
            haveAllDoubleEdges = false;
    }

    auto drawUniformRoundedOrAlphaBorders = [&]() {
        ASSERT(numEdgesVisible == 4);
        ASSERT(allEdgesShareColor);
        ASSERT(sides.haveAllSolidEdges);
        ASSERT(!sides.outerEdgeIsRectangular || haveAlphaColor);

        auto path = borderShape.pathForOuterShape(deviceScaleFactor);
        borderShape.addInnerShapeToPath(path, deviceScaleFactor);

        graphicsContext.setFillRule(WindRule::EvenOdd);
        graphicsContext.setFillColor(sides.edges.at(*firstVisibleSide).color());
        graphicsContext.fillPath(path);
    };

    auto drawUniformDoubleBorders = [&]() {
        ASSERT(numEdgesVisible == 4);
        ASSERT(allEdgesShareColor);
        ASSERT(haveAllDoubleEdges);
        ASSERT(!sides.outerEdgeIsRectangular || haveAlphaColor);

        auto path = borderShape.pathForOuterShape(deviceScaleFactor);

        RectEdges<LayoutUnit> outerThirdInsets;
        RectEdges<LayoutUnit> innerThirdInsets;

        sides.edges.at(BoxSide::Top).getDoubleBorderStripeWidths(outerThirdInsets.top(), innerThirdInsets.top());
        sides.edges.at(BoxSide::Right).getDoubleBorderStripeWidths(outerThirdInsets.right(), innerThirdInsets.right());
        sides.edges.at(BoxSide::Bottom).getDoubleBorderStripeWidths(outerThirdInsets.bottom(), innerThirdInsets.bottom());
        sides.edges.at(BoxSide::Left).getDoubleBorderStripeWidths(outerThirdInsets.left(), innerThirdInsets.left());

        auto outerThirdShape = borderShape.shapeWithBorderWidths(outerThirdInsets);
        outerThirdShape.addInnerShapeToPath(path, deviceScaleFactor);

        auto innerThirdShape = borderShape.shapeWithBorderWidths(innerThirdInsets);
        innerThirdShape.addInnerShapeToPath(path, deviceScaleFactor);

        borderShape.addInnerShapeToPath(path, deviceScaleFactor);

        graphicsContext.setFillRule(WindRule::EvenOdd);
        graphicsContext.setFillColor(sides.edges.at(*firstVisibleSide).color());
        graphicsContext.fillPath(path);
    };

    if ((sides.haveAllSolidEdges || haveAllDoubleEdges) && allEdgesShareColor) {
        // Fast path for drawing all solid edges and all unrounded double edges which need path-based rendering because of rounding or alpha colors.
        if (numEdgesVisible == 4 && (!sides.outerEdgeIsRectangular || haveAlphaColor)) {
            if (sides.haveAllSolidEdges) {
                drawUniformRoundedOrAlphaBorders();
                return;
            }

            if (haveAllDoubleEdges) {
                drawUniformDoubleBorders();
                return;
            }
        }

        // Avoid creating transparent layers
        if (sides.haveAllSolidEdges && numEdgesVisible != 4 && sides.outerEdgeIsRectangular && haveAlphaColor) {
            auto outerBorderRect = borderShape.borderRect();
            Path path;
            for (auto side : allBoxSides) {
                if (sides.edges.at(side).shouldRender()) {
                    auto sideRect = calculateSideRect(outerBorderRect, sides.edges, side);
                    path.addRect(sideRect); // FIXME: Need pixel snapping here.
                }
            }

            graphicsContext.setFillRule(WindRule::NonZero);
            graphicsContext.setFillColor(sides.edges.at(*firstVisibleSide).color());
            graphicsContext.fillPath(path);
            return;
        }
    }

    bool clipToOuterBorder = !sides.outerEdgeIsRectangular;
    GraphicsContextStateSaver stateSaver(graphicsContext, clipToOuterBorder && !sides.appliedClipAlready);
    if (clipToOuterBorder) {
        // Clip to the inner and outer radii rects.
        if (sides.bleedAvoidance != BleedAvoidance::UseTransparencyLayer)
            borderShape.clipToOuterShape(graphicsContext, deviceScaleFactor);
        borderShape.clipOutInnerShape(graphicsContext, deviceScaleFactor);
    }

    // If only one edge visible antialiasing doesn't create seams
    bool antialias = shouldAntialiasLines(graphicsContext) || numEdgesVisible == 1;
    if (haveAlphaColor)
        paintTranslucentBorderSides(borderShape, sides, edgesToDraw, antialias);
    else
        paintBorderSides(borderShape, sides, edgesToDraw, antialias);
}

template<typename T>
bool BorderPainter::paintNinePieceImageImpl(const LayoutRect& rect, const RenderStyle& style, const T& ninePieceImage, CompositeOperator op) const
{
    auto image = ninePieceImage.source().tryStyleImage();
    if (!image)
        return false;

    if (!image->isLoaded(m_renderer.ptr()))
        return true; // Never paint a nine-piece image incrementally, but don't paint the fallback borders either.

    if (!image->canRender(m_renderer.ptr(), style.usedZoom()))
        return false;

    CheckedPtr modelObject = dynamicDowncast<RenderBoxModelObject>(m_renderer.get());
    if (!modelObject)
        return false;

    ImagePaintingOptions options = {
        op,
        ImageOrientation::Orientation::FromImage,
        m_paintInfo.paintBehavior.contains(PaintBehavior::DrawsHDRContent) ? DrawsHDRContent::Yes : DrawsHDRContent::No,
        style.dynamicRangeLimit().toPlatformDynamicRangeLimit()
    };

    // FIXME: border-image is broken with full page zooming when tiling has to happen, since the tiling function
    // doesn't have any understanding of the zoom that is in effect on the tile.
    float deviceScaleFactor = document().deviceScaleFactor();

    LayoutRect rectWithOutsets = rect;
    rectWithOutsets.expand(style.imageOutsets(ninePieceImage));
    LayoutRect destination = LayoutRect(snapRectToDevicePixels(rectWithOutsets, deviceScaleFactor));

    auto source = modelObject->calculateImageIntrinsicDimensions(image.get(), destination.size(), RenderBoxModelObject::ScaleByUsedZoom::No);

    // If both values are ‘auto’ then the intrinsic width and/or height of the image should be used, if any.
    image->setContainerContextForRenderer(m_renderer, source, style.usedZoom());

    NinePieceImagePainter::paint(ninePieceImage, m_paintInfo.context(), m_renderer.ptr(), style, destination, source, deviceScaleFactor, options);
    return true;
}

bool BorderPainter::paintNinePieceImage(const LayoutRect& rect, const RenderStyle& style, const Style::BorderImage& borderImage, CompositeOperator op) const
{
    return paintNinePieceImageImpl(rect, style, borderImage, op);
}

bool BorderPainter::paintNinePieceImage(const LayoutRect& rect, const RenderStyle& style, const Style::MaskBorder& maskBorder, CompositeOperator op) const
{
    return paintNinePieceImageImpl(rect, style, maskBorder, op);
}

void BorderPainter::paintTranslucentBorderSides(const BorderShape& borderShape, const Sides& sides, BoxSideSet edgesToDraw, bool antialias) const
{
    // willBeOverdrawn assumes that we draw in order: top, bottom, left, right.
    // This is different from BoxSide enum order.
    static constexpr std::array<BoxSide, 4> paintOrderSides = { BoxSide::Top, BoxSide::Bottom, BoxSide::Left, BoxSide::Right };

    while (edgesToDraw) {
        // Find undrawn edges sharing a color.
        Color commonColor;

        BoxSideSet commonColorEdgeSet;
        for (auto side : paintOrderSides) {
            if (!edgesToDraw.contains(side))
                continue;

            auto& edge = sides.edges.at(side);
            bool includeEdge;
            if (commonColorEdgeSet.isEmpty()) {
                commonColor = edge.color();
                includeEdge = true;
            } else
                includeEdge = equalIgnoringSemanticColor(edge.color(), commonColor);

            if (includeEdge)
                commonColorEdgeSet.add(side);
        }

        bool useTransparencyLayer = includesAdjacentEdges(commonColorEdgeSet) && !commonColor.isOpaque();
        if (useTransparencyLayer) {
            m_paintInfo.context().beginTransparencyLayer(commonColor.alphaAsFloat());
            commonColor = commonColor.opaqueColor();
        }

        paintBorderSides(borderShape, sides, commonColorEdgeSet, antialias, &commonColor);

        if (useTransparencyLayer)
            m_paintInfo.context().endTransparencyLayer();

        edgesToDraw.remove(commonColorEdgeSet);
    }
}

// BorderStyle::Outset darkens the bottom and right (and maybe lightens the top and left)
// BorderStyle::Inset darkens the top and left (and maybe lightens the bottom and right)
static inline bool borderStyleHasUnmatchedColorsAtCorner(BorderStyle style, BoxSide side, BoxSide adjacentSide)
{
    // These styles match at the top/left and bottom/right.
    if (style == BorderStyle::Inset || style == BorderStyle::Groove || style == BorderStyle::Ridge || style == BorderStyle::Outset) {
        BoxSideSet topRightSides = { BoxSide::Top, BoxSide::Right };
        BoxSideSet bottomLeftSides = { BoxSide::Bottom, BoxSide::Left };

        BoxSideSet usedSides { side, adjacentSide };
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

void BorderPainter::paintBorderSides(const BorderShape& borderShape, const Sides& sides, BoxSideSet edgeSet, bool antialias, const Color* overrideColor) const
{
    Path roundedPath;
    if (!sides.outerEdgeIsRectangular) {
        float deviceScaleFactor = document().deviceScaleFactor();
        roundedPath = borderShape.pathForOuterShape(deviceScaleFactor);
    }

    auto innerEdgeRadii = borderShape.innerEdgeRadii();

    // The inner border adjustment for bleed avoidance mode BleedAvoidance::BackgroundOverBorder
    // is only applied to sideRect, which is okay since BleedAvoidance::BackgroundOverBorder
    // is only to be used for solid borders and the shape of the border painted by drawBoxSideFromPath
    // only depends on sideRect when painting solid borders.

    auto paintOneSide = [&](BoxSide side, BoxSide adjacentSide1, BoxSide adjacentSide2) {
        auto& edge = sides.edges.at(side);
        if (!edge.shouldRender() || !edgeSet.contains(side))
            return;

        LayoutRect sideRect = borderShape.borderRect();
        LayoutSize firstRadius;
        LayoutSize secondRadius;

        switch (side) {
        case BoxSide::Top:
            sideRect.setHeight(borderShape.borderWidths().top());
            firstRadius = innerEdgeRadii.topLeft();
            secondRadius = innerEdgeRadii.topRight();
            break;
        case BoxSide::Right:
            sideRect.shiftXEdgeTo(sideRect.maxX() - borderShape.borderWidths().right());
            firstRadius = innerEdgeRadii.bottomRight();
            secondRadius = innerEdgeRadii.topRight();
            break;
        case BoxSide::Bottom:
            sideRect.shiftYEdgeTo(sideRect.maxY() - borderShape.borderWidths().bottom());
            firstRadius = innerEdgeRadii.bottomLeft();
            secondRadius = innerEdgeRadii.bottomRight();
            break;
        case BoxSide::Left:
            sideRect.setWidth(borderShape.borderWidths().left());
            firstRadius = innerEdgeRadii.bottomLeft();
            secondRadius = innerEdgeRadii.topLeft();
            break;
        }

        auto borderWillArcInnerEdge = [](LayoutSize firstRadius, LayoutSize secondRadius) {
            return !firstRadius.isEmpty() || !secondRadius.isEmpty();
        };

        bool usePath = !sides.outerEdgeIsRectangular && (borderStyleHasInnerDetail(edge.style()) || borderWillArcInnerEdge(firstRadius, secondRadius));
        paintOneBorderSide(borderShape, sides, sideRect, side, adjacentSide1, adjacentSide2, usePath ? &roundedPath : nullptr, antialias, overrideColor);
    };

    paintOneSide(BoxSide::Top, BoxSide::Left, BoxSide::Right);
    paintOneSide(BoxSide::Bottom, BoxSide::Left, BoxSide::Right);
    paintOneSide(BoxSide::Left, BoxSide::Top, BoxSide::Bottom);
    paintOneSide(BoxSide::Right, BoxSide::Top, BoxSide::Bottom);
}

void BorderPainter::paintOneBorderSide(const BorderShape& borderShape, const Sides& sides, const LayoutRect& sideRect, BoxSide side, BoxSide adjacentSide1, BoxSide adjacentSide2, const Path* path, bool antialias, const Color* overrideColor) const
{
    auto& edgeToRender = sides.edges.at(side);
    ASSERT(edgeToRender.widthForPainting());
    auto& adjacentEdge1 = sides.edges.at(adjacentSide1);
    auto& adjacentEdge2 = sides.edges.at(adjacentSide2);

    bool mitreAdjacentSide1 = joinRequiresMitre(side, adjacentSide1, sides.edges, !antialias);
    bool mitreAdjacentSide2 = joinRequiresMitre(side, adjacentSide2, sides.edges, !antialias);

    bool adjacentSide1StylesMatch = colorsMatchAtCorner(side, adjacentSide1, sides.edges);
    bool adjacentSide2StylesMatch = colorsMatchAtCorner(side, adjacentSide2, sides.edges);

    const Color& colorToPaint = overrideColor ? *overrideColor : edgeToRender.color();

    auto& graphicsContext = m_paintInfo.context();

    if (path) {
        GraphicsContextStateSaver stateSaver(graphicsContext);

        clipBorderSidePolygon(borderShape, side, adjacentSide1StylesMatch, adjacentSide2StylesMatch);

        float thickness = std::max(std::max(edgeToRender.widthForPainting(), adjacentEdge1.widthForPainting()), adjacentEdge2.widthForPainting());
        drawBoxSideFromPath(borderShape, *path, sides.edges, edgeToRender.widthForPainting(), thickness, side, colorToPaint, edgeToRender.style(), sides.bleedAvoidance);
    } else {
        bool clipForStyle = styleRequiresClipPolygon(edgeToRender.style()) && (mitreAdjacentSide1 || mitreAdjacentSide2);
        bool clipAdjacentSide1 = colorNeedsAntiAliasAtCorner(side, adjacentSide1, sides.edges) && mitreAdjacentSide1;
        bool clipAdjacentSide2 = colorNeedsAntiAliasAtCorner(side, adjacentSide2, sides.edges) && mitreAdjacentSide2;
        bool shouldClip = clipForStyle || clipAdjacentSide1 || clipAdjacentSide2;

        GraphicsContextStateSaver clipStateSaver(graphicsContext, shouldClip);
        if (shouldClip) {
            bool aliasAdjacentSide1 = clipAdjacentSide1 || (clipForStyle && mitreAdjacentSide1);
            bool aliasAdjacentSide2 = clipAdjacentSide2 || (clipForStyle && mitreAdjacentSide2);
            clipBorderSidePolygon(borderShape, side, !aliasAdjacentSide1, !aliasAdjacentSide2);
            // Since we clipped, no need to draw with a mitre.
            mitreAdjacentSide1 = false;
            mitreAdjacentSide2 = false;
        }
        drawLineForBoxSide(graphicsContext, document(), sideRect, side, colorToPaint, edgeToRender.style(), mitreAdjacentSide1 ? adjacentEdge1.widthForPainting() : 0, mitreAdjacentSide2 ? adjacentEdge2.widthForPainting() : 0, antialias);
    }
}

void BorderPainter::drawBoxSideFromPath(const BorderShape& borderShape, const Path& borderPath, const BorderEdges& edges, float thickness, float drawThickness, BoxSide side, Color color, BorderStyle borderStyle, BleedAvoidance bleedAvoidance) const
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
        graphicsContext.setStrokeStyle(borderStyle == BorderStyle::Dashed ? StrokeStyle::DashedStroke : StrokeStyle::DottedStroke);

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

            graphicsContext.setLineDash(DashArray { dashLength, gapLength }, dashLength);
        }

        // FIXME: stroking the border path causes issues with tight corners:
        // https://bugs.webkit.org/show_bug.cgi?id=58711
        // Also, to get the best appearance we should stroke a path between the two borders.
        graphicsContext.strokePath(borderPath);
        return;
    }
    case BorderStyle::Double: {
        RectEdges<LayoutUnit> outerThirdInsets;
        RectEdges<LayoutUnit> innerThirdInsets;

        edges.at(BoxSide::Top).getDoubleBorderStripeWidths(outerThirdInsets.top(), innerThirdInsets.top());
        edges.at(BoxSide::Right).getDoubleBorderStripeWidths(outerThirdInsets.right(), innerThirdInsets.right());
        edges.at(BoxSide::Bottom).getDoubleBorderStripeWidths(outerThirdInsets.bottom(), innerThirdInsets.bottom());
        edges.at(BoxSide::Left).getDoubleBorderStripeWidths(outerThirdInsets.left(), innerThirdInsets.left());

        // Draw inner border line
        {
            GraphicsContextStateSaver stateSaver(graphicsContext);

            auto innerThirdShape = borderShape.shapeWithBorderWidths(innerThirdInsets);
            innerThirdShape.clipToInnerShape(graphicsContext, document().deviceScaleFactor()); // FIXME: Cache document().deviceScaleFactor().

            drawBoxSideFromPath(borderShape, borderPath, edges, thickness, drawThickness, side, color, BorderStyle::Solid, bleedAvoidance);
        }

        // Draw outer border line
        {
            auto outerThirdShape = borderShape.shapeWithBorderWidths(outerThirdInsets);
            outerThirdShape.clipOutInnerShape(graphicsContext, document().deviceScaleFactor());

            drawBoxSideFromPath(borderShape, borderPath, edges, thickness, drawThickness, side, color, BorderStyle::Solid, bleedAvoidance);
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
        drawBoxSideFromPath(borderShape, borderPath, edges, thickness, drawThickness, side, color, s1, bleedAvoidance);

        // Paint inner only
        GraphicsContextStateSaver stateSaver(graphicsContext);

        RectEdges<LayoutUnit> midWidths = {
            LayoutUnit { edges.top().widthForPainting() / 2 },
            LayoutUnit { edges.right().widthForPainting() / 2 },
            LayoutUnit { edges.bottom().widthForPainting() / 2 },
            LayoutUnit { edges.left().widthForPainting() / 2 },
        };

        auto midBorderShape = borderShape.shapeWithBorderWidths(midWidths);
        midBorderShape.clipToInnerShape(graphicsContext, document().deviceScaleFactor());

        drawBoxSideFromPath(borderShape, borderPath, edges, thickness, drawThickness, side, color, s2, bleedAvoidance);
        return;
    }
    case BorderStyle::Inset:
    case BorderStyle::Outset:
        color = calculateBorderStyleColor(borderStyle, side, color);
        break;
    default:
        break;
    }

    graphicsContext.setStrokeStyle(StrokeStyle::NoStroke);
    graphicsContext.setFillColor(color);

    auto borderRect = borderShape.snappedOuterRect(document().deviceScaleFactor());
    graphicsContext.drawRect(borderRect);
}

void BorderPainter::clipBorderSidePolygon(const BorderShape& borderShape, BoxSide side, bool firstEdgeMatches, bool secondEdgeMatches) const
{
    auto& graphicsContext = m_paintInfo.context();

    float deviceScaleFactor = document().deviceScaleFactor();
    auto outerRect = borderShape.snappedOuterRect(deviceScaleFactor);

    auto innerRect = borderShape.snappedInnerRect(deviceScaleFactor);
    auto innerBorder = borderShape.deprecatedInnerRoundedRect();

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

        if (!Style::isZero(innerBorder.radii().topLeft()))
            findIntersection(outerRect.minXMinYCorner(), innerRect.minXMinYCorner(), innerRect.minXMaxYCorner(), innerRect.maxXMinYCorner(), quad[1]);

        if (!Style::isZero(innerBorder.radii().topRight()))
            findIntersection(outerRect.maxXMinYCorner(), innerRect.maxXMinYCorner(), innerRect.minXMinYCorner(), innerRect.maxXMaxYCorner(), quad[2]);
        break;

    case BoxSide::Left:
        quad = { outerRect.minXMinYCorner(), innerRect.minXMinYCorner(), innerRect.minXMaxYCorner(), outerRect.minXMaxYCorner() };

        if (!Style::isZero(innerBorder.radii().topLeft()))
            findIntersection(outerRect.minXMinYCorner(), innerRect.minXMinYCorner(), innerRect.minXMaxYCorner(), innerRect.maxXMinYCorner(), quad[1]);

        if (!Style::isZero(innerBorder.radii().bottomLeft()))
            findIntersection(outerRect.minXMaxYCorner(), innerRect.minXMaxYCorner(), innerRect.minXMinYCorner(), innerRect.maxXMaxYCorner(), quad[2]);
        break;

    case BoxSide::Bottom:
        quad = { outerRect.minXMaxYCorner(), innerRect.minXMaxYCorner(), innerRect.maxXMaxYCorner(), outerRect.maxXMaxYCorner() };

        if (!Style::isZero(innerBorder.radii().bottomLeft()))
            findIntersection(outerRect.minXMaxYCorner(), innerRect.minXMaxYCorner(), innerRect.minXMinYCorner(), innerRect.maxXMaxYCorner(), quad[1]);

        if (!Style::isZero(innerBorder.radii().bottomRight()))
            findIntersection(outerRect.maxXMaxYCorner(), innerRect.maxXMaxYCorner(), innerRect.maxXMinYCorner(), innerRect.minXMaxYCorner(), quad[2]);
        break;

    case BoxSide::Right:
        quad = { outerRect.maxXMinYCorner(), innerRect.maxXMinYCorner(), innerRect.maxXMaxYCorner(), outerRect.maxXMaxYCorner() };

        if (!Style::isZero(innerBorder.radii().topRight()))
            findIntersection(outerRect.maxXMinYCorner(), innerRect.maxXMinYCorner(), innerRect.minXMinYCorner(), innerRect.maxXMaxYCorner(), quad[1]);

        if (!Style::isZero(innerBorder.radii().bottomRight()))
            findIntersection(outerRect.maxXMaxYCorner(), innerRect.maxXMaxYCorner(), innerRect.maxXMinYCorner(), innerRect.minXMaxYCorner(), quad[2]);
        break;
    }

    // If the border matches both of its adjacent sides, don't anti-alias the clip, and
    // if neither side matches, anti-alias the clip.
    if (firstEdgeMatches == secondEdgeMatches) {
        bool wasAntialiased = graphicsContext.shouldAntialias();
        graphicsContext.setShouldAntialias(!firstEdgeMatches);
        graphicsContext.clipPath(Path(quad), WindRule::NonZero);
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
    graphicsContext.clipPath(Path(firstQuad), WindRule::NonZero);

    Vector<FloatPoint> secondQuad = {
        quad[0],
        side == BoxSide::Top || side == BoxSide::Bottom ? FloatPoint(quad[0].x(), quad[1].y()) : FloatPoint(quad[1].x(), quad[0].y()),
        quad[1],
        quad[2],
        quad[3]
    };
    // Antialiasing affects the second side.
    graphicsContext.setShouldAntialias(!secondEdgeMatches);
    graphicsContext.clipPath(Path(secondQuad), WindRule::NonZero);

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
        graphicsContext.setStrokeStyle(borderStyle == BorderStyle::Dashed ? StrokeStyle::DashedStroke : StrokeStyle::DottedStroke);
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
            graphicsContext.setStrokeStyle(StrokeStyle::NoStroke);
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

            float offset1 = floorToDevicePixel(std::abs(adjacentWidth1) * 2 / 3, deviceScaleFactor);
            float offset2 = floorToDevicePixel(std::abs(adjacentWidth2) * 2 / 3, deviceScaleFactor);

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
            offset3 = floorToDevicePixel(std::abs(adjacentWidth1) / 2, deviceScaleFactor);

        if (((side == BoxSide::Top || side == BoxSide::Left) && adjacentWidth2 > 0) || ((side == BoxSide::Bottom || side == BoxSide::Right) && adjacentWidth2 < 0))
            offset4 = ceilToDevicePixel(adjacentWidth2 / 2, deviceScaleFactor);

        float adjustedX = ceilToDevicePixel(std::midpoint(x1, x2), deviceScaleFactor);
        float adjustedY = ceilToDevicePixel(std::midpoint(y1, y2), deviceScaleFactor);
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
        [[fallthrough]];
    case BorderStyle::Solid: {
        StrokeStyle oldStrokeStyle = graphicsContext.strokeStyle();
        ASSERT(x2 >= x1);
        ASSERT(y2 >= y1);
        if (!adjacentWidth1 && !adjacentWidth2) {
            graphicsContext.setStrokeStyle(StrokeStyle::NoStroke);
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

        graphicsContext.setStrokeStyle(StrokeStyle::NoStroke);
        graphicsContext.setFillColor(color);
        bool wasAntialiased = graphicsContext.shouldAntialias();
        graphicsContext.setShouldAntialias(antialias);
        graphicsContext.fillPath(Path(quad));
        graphicsContext.setShouldAntialias(wasAntialiased);

        graphicsContext.setStrokeStyle(oldStrokeStyle);
        break;
    }
    }
}

LayoutRect BorderPainter::borderRectAdjustedForBleedAvoidance(const LayoutRect& rect, BleedAvoidance bleedAvoidance) const
{
    if (bleedAvoidance != BleedAvoidance::BackgroundOverBorder)
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

// This never changes the alpha of the color, so it's OK that callers don't check for PaintBehavior::ForceBlackBorder.
Color BorderPainter::calculateBorderStyleColor(const BorderStyle& style, const BoxSide& side, const Color& color)
{
    ASSERT(style == BorderStyle::Inset || style == BorderStyle::Outset);

    // This values were derived empirically.
    constexpr float baseDarkColorLuminance { 0.014443844f }; // Luminance of SRGBA<uint8_t> { 32, 32, 32 }
    constexpr float baseLightColorLuminance { 0.83077f }; // Luminance of SRGBA<uint8_t> { 235, 235, 235 }

    enum Operation { Darken, Lighten };

    Operation operation = (side == BoxSide::Top || side == BoxSide::Left) == (style == BorderStyle::Inset) ? Darken : Lighten;

    bool isVeryDarkColor = color.luminance() <= baseDarkColorLuminance;
    bool isVeryLightColor = color.luminance() > baseLightColorLuminance;

    // Special case very dark colors to give them extra contrast.
    if (isVeryDarkColor)
        return operation == Darken ? color.lightened() : color.lightened().lightened();

    // Here we will darken the border decoration color when needed.
    if (operation == Darken)
        return color.darkened();

    ASSERT(operation == Lighten);

    return isVeryLightColor ? color : color.lightened();
}

const Document& BorderPainter::document() const
{
    return m_renderer->document();
}

}
