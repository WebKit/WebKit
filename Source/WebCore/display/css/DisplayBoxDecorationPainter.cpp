/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DisplayBoxDecorationPainter.h"

#include "BorderEdge.h" // For BoxSideSet.
#include "Color.h"
#include "DisplayBoxDecorationData.h"
#include "DisplayBoxModelBox.h"
#include "DisplayBoxRareGeometry.h"
#include "DisplayPaintingContext.h"
#include "DisplayStyle.h"
#include "DisplayTree.h"
#include "FillLayer.h"
#include "GeometryUtilities.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "LayoutPoint.h"
#include "ShadowData.h"

namespace WebCore {
namespace Display {

class BorderPainter {
public:
    BorderPainter(const RectEdges<BorderEdge>& edges, const FloatRoundedRect& borderRect, BackgroundBleedAvoidance bleedAvoidance, bool includeLeftEdge, bool includeRightEdge)
        : m_edges(edges)
        , m_borderRect(borderRect)
        , m_bleedAvoidance(bleedAvoidance)
        , m_includeLeftEdge(includeLeftEdge)
        , m_includeRightEdge(includeRightEdge)
    {
    }
    
    void paintBorders(PaintingContext&) const;

private:
    static bool edgesShareColor(const BorderEdge& firstEdge, const BorderEdge& secondEdge)
    {
        return firstEdge.color() == secondEdge.color();
    }

    static bool borderStyleFillsBorderArea(BorderStyle style)
    {
        return !(style == BorderStyle::Dotted || style == BorderStyle::Dashed || style == BorderStyle::Double);
    }

    static bool borderStyleHasInnerDetail(BorderStyle style)
    {
        return style == BorderStyle::Groove || style == BorderStyle::Ridge || style == BorderStyle::Double;
    }

    static bool styleRequiresClipPolygon(BorderStyle style)
    {
        return style == BorderStyle::Dotted || style == BorderStyle::Dashed; // These are drawn with a stroke, so we have to clip to get corner miters.
    }

    static bool borderStyleIsDottedOrDashed(BorderStyle style)
    {
        return style == BorderStyle::Dotted || style == BorderStyle::Dashed;
    }

    static bool borderWillArcInnerEdge(const FloatSize& firstRadius, const FloatSize& secondRadius)
    {
        return !firstRadius.isEmpty() || !secondRadius.isEmpty();
    }

    static bool borderStyleHasUnmatchedColorsAtCorner(BorderStyle, BoxSide, BoxSide adjacentSide);
    static bool borderStylesRequireMitre(BoxSide, BoxSide adjacentSide, BorderStyle, BorderStyle adjacentStyle);
    static FloatRoundedRect calculateAdjustedInnerBorder(const FloatRoundedRect& innerBorder, BoxSide);
    static void calculateBorderStyleColor(BorderStyle, BoxSide, Color&);

    FloatRect borderInnerRectAdjustedForBleedAvoidance(const PaintingContext&) const;

    bool colorsMatchAtCorner(BoxSide, BoxSide adjacentSide) const;
    bool colorNeedsAntiAliasAtCorner(BoxSide, BoxSide adjacentSide) const;
    bool willBeOverdrawn(BoxSide, BoxSide adjacentSide) const;
    bool joinRequiresMitre(BoxSide, BoxSide adjacentSide, bool allowOverdraw) const;

    void clipBorderSidePolygon(PaintingContext&, const FloatRoundedRect& outerBorder, const FloatRoundedRect& innerBorder, BoxSide, bool firstEdgeMatches, bool secondEdgeMatches) const;

    void drawLineForBoxSide(PaintingContext&, const FloatRect&, BoxSide, Color, BorderStyle, float adjacentWidth1, float adjacentWidth2, bool antialias) const;
    void drawBoxSideFromPath(PaintingContext&, const FloatRect& borderRect, const Path& borderPath, float thickness, float drawThickness, BoxSide, Color, BorderStyle) const;

    void paintOneBorderSide(PaintingContext&, const FloatRoundedRect& outerBorder, const FloatRoundedRect& innerBorder, const FloatRect& sideRect, BoxSide, const Path*, bool antialias, const Color* overrideColor) const;
    void paintBorderSides(PaintingContext&, const FloatRoundedRect& outerBorder, const FloatRoundedRect& innerBorder, FloatSize innerBorderBleedAdjustment, BoxSideSet, bool antialias, const Color* overrideColor = nullptr) const;
    void paintTranslucentBorderSides(PaintingContext&, const FloatRoundedRect& outerBorder, const FloatRoundedRect& innerBorder, FloatSize innerBorderBleedAdjustment, BoxSideSet, bool antialias) const;

    const RectEdges<BorderEdge>& m_edges;
    const FloatRoundedRect m_borderRect;
    const BackgroundBleedAvoidance m_bleedAvoidance;
    const bool m_includeLeftEdge;
    const bool m_includeRightEdge;
};

// BorderStyle::Outset darkens the bottom and right (and maybe lightens the top and left)
// BorderStyle::Inset darkens the top and left (and maybe lightens the bottom and right)
bool BorderPainter::borderStyleHasUnmatchedColorsAtCorner(BorderStyle style, BoxSide side, BoxSide adjacentSide)
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

bool BorderPainter::colorsMatchAtCorner(BoxSide side, BoxSide adjacentSide) const
{
    auto& edge = m_edges.at(side);
    auto& adjacentEdge = m_edges.at(adjacentSide);

    if (edge.shouldRender() != adjacentEdge.shouldRender())
        return false;

    if (!edgesShareColor(edge, adjacentEdge))
        return false;

    return !borderStyleHasUnmatchedColorsAtCorner(edge.style(), side, adjacentSide);
}

bool BorderPainter::colorNeedsAntiAliasAtCorner(BoxSide side, BoxSide adjacentSide) const
{
    auto& edge = m_edges.at(side);
    auto& adjacentEdge = m_edges.at(adjacentSide);

    if (edge.color().isOpaque())
        return false;

    if (edge.shouldRender() != adjacentEdge.shouldRender())
        return false;

    if (!edgesShareColor(edge, adjacentEdge))
        return true;

    return borderStyleHasUnmatchedColorsAtCorner(edge.style(), side, adjacentSide);
}

void BorderPainter::calculateBorderStyleColor(BorderStyle style, BoxSide side, Color& color)
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
            color = color.darkened();
    } else {
        if (color.luminance() < baseLightColorLuminance)
            color = color.lightened();
    }
}

// This assumes that we draw in order: top, bottom, left, right.
bool BorderPainter::willBeOverdrawn(BoxSide side, BoxSide adjacentSide) const
{
    switch (side) {
    case BoxSide::Top:
    case BoxSide::Bottom: {
        auto& edge = m_edges.at(side);
        auto& adjacentEdge = m_edges.at(adjacentSide);

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

bool BorderPainter::borderStylesRequireMitre(BoxSide side, BoxSide adjacentSide, BorderStyle style, BorderStyle adjacentStyle)
{
    if (style == BorderStyle::Double || adjacentStyle == BorderStyle::Double || adjacentStyle == BorderStyle::Groove || adjacentStyle == BorderStyle::Ridge)
        return true;

    if (borderStyleIsDottedOrDashed(style) != borderStyleIsDottedOrDashed(adjacentStyle))
        return true;

    if (style != adjacentStyle)
        return true;

    return borderStyleHasUnmatchedColorsAtCorner(style, side, adjacentSide);
}

FloatRoundedRect BorderPainter::calculateAdjustedInnerBorder(const FloatRoundedRect& innerBorder, BoxSide side)
{
    // Expand the inner border as necessary to make it a rounded rect (i.e. radii contained within each edge).
    // This function relies on the fact we only get radii not contained within each edge if one of the radii
    // for an edge is zero, so we can shift the arc towards the zero radius corner.
    auto newRadii = innerBorder.radii();
    auto newRect = innerBorder.rect();

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

    return FloatRoundedRect { newRect, newRadii };
}

bool BorderPainter::joinRequiresMitre(BoxSide side, BoxSide adjacentSide, bool allowOverdraw) const
{
    auto& edge = m_edges.at(side);
    auto& adjacentEdge = m_edges.at(adjacentSide);

    if ((edge.isTransparent() && adjacentEdge.isTransparent()) || !adjacentEdge.isPresent())
        return false;

    if (allowOverdraw && willBeOverdrawn(side, adjacentSide))
        return false;

    if (!edgesShareColor(edge, adjacentEdge))
        return true;

    if (borderStylesRequireMitre(side, adjacentSide, edge.style(), adjacentEdge.style()))
        return true;
    
    return false;
}

void BorderPainter::drawBoxSideFromPath(PaintingContext& paintingContext, const FloatRect& borderRect, const Path& borderPath, float thickness, float drawThickness, BoxSide side, Color color, BorderStyle borderStyle) const
{
    if (thickness <= 0)
        return;

    if (borderStyle == BorderStyle::Double && thickness < 3)
        borderStyle = BorderStyle::Solid;

    switch (borderStyle) {
    case BorderStyle::None:
    case BorderStyle::Hidden:
        return;
    case BorderStyle::Dotted:
    case BorderStyle::Dashed: {
        paintingContext.context.setStrokeColor(color);

        // The stroke is doubled here because the provided path is the
        // outside edge of the border so half the stroke is clipped off.
        // The extra multiplier is so that the clipping mask can antialias
        // the edges to prevent jaggies.
        paintingContext.context.setStrokeThickness(drawThickness * 2 * 1.1f);
        paintingContext.context.setStrokeStyle(borderStyle == BorderStyle::Dashed ? StrokeStyle::DashedStroke : StrokeStyle::DottedStroke);

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

            DashArray lineDash;
            lineDash.append(dashLength);
            lineDash.append(gapLength);
            paintingContext.context.setLineDash(lineDash, dashLength);
        }
        
        // FIXME: stroking the border path causes issues with tight corners:
        // https://bugs.webkit.org/show_bug.cgi?id=58711
        // Also, to get the best appearance we should stroke a path between the two borders.
        paintingContext.context.strokePath(borderPath);
        return;
    }
    case BorderStyle::Double: {
        // Get the inner border rects for both the outer border line and the inner border line
        // FIXME: Rect edges.
        float outerBorderTopWidth = m_edges.top().outerWidth();
        float innerBorderTopWidth = m_edges.top().innerWidth();

        float outerBorderRightWidth = m_edges.right().outerWidth();
        float innerBorderRightWidth = m_edges.right().innerWidth();

        float outerBorderBottomWidth = m_edges.bottom().outerWidth();
        float innerBorderBottomWidth = m_edges.bottom().innerWidth();

        float outerBorderLeftWidth = m_edges.left().outerWidth();
        float innerBorderLeftWidth = m_edges.left().innerWidth();

        // Draw inner border line
        {
            GraphicsContextStateSaver stateSaver(paintingContext.context);
            auto innerClip = roundedInsetBorderForRect(borderRect, m_borderRect.radii(), { innerBorderTopWidth, innerBorderRightWidth, innerBorderBottomWidth, innerBorderLeftWidth }, m_includeLeftEdge, m_includeRightEdge);
            
            paintingContext.context.clipRoundedRect(innerClip);
            drawBoxSideFromPath(paintingContext, borderRect, borderPath, thickness, drawThickness, side, color, BorderStyle::Solid);
        }

        // Draw outer border line
        {
            GraphicsContextStateSaver stateSaver(paintingContext.context);
            auto outerRect = borderRect;
            if (m_bleedAvoidance == BackgroundBleedAvoidance::UseTransparencyLayer) {
                outerRect.inflate(1);
                ++outerBorderTopWidth;
                ++outerBorderBottomWidth;
                ++outerBorderLeftWidth;
                ++outerBorderRightWidth;
            }

            auto outerClip = roundedInsetBorderForRect(outerRect, m_borderRect.radii(), { outerBorderTopWidth, outerBorderRightWidth, outerBorderBottomWidth, outerBorderLeftWidth }, m_includeLeftEdge, m_includeRightEdge);
            paintingContext.context.clipOutRoundedRect(outerClip);
            drawBoxSideFromPath(paintingContext, borderRect, borderPath, thickness, drawThickness, side, color, BorderStyle::Solid);
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
        drawBoxSideFromPath(paintingContext, borderRect, borderPath, thickness, drawThickness, side, color, s1);

        // Paint inner only
        GraphicsContextStateSaver stateSaver(paintingContext.context);
        // FIXME: Should have pixel snapped these at display tree generation time.
        // FIXME: RectEdges
        float topWidth { m_edges.top().widthForPainting() / 2 };
        float bottomWidth { m_edges.bottom().widthForPainting() / 2 };
        float leftWidth { m_edges.left().widthForPainting() / 2 };
        float rightWidth { m_edges.right().widthForPainting() / 2 };

        auto clipRect = roundedInsetBorderForRect(borderRect, m_borderRect.radii(), { topWidth, rightWidth, bottomWidth, leftWidth }, m_includeLeftEdge, m_includeRightEdge);

        paintingContext.context.clipRoundedRect(clipRect);
        drawBoxSideFromPath(paintingContext, borderRect, borderPath, thickness, drawThickness, side, color, s2);
        return;
    }
    case BorderStyle::Inset:
    case BorderStyle::Outset:
        calculateBorderStyleColor(borderStyle, side, color);
        break;
    default:
        break;
    }

    paintingContext.context.setStrokeStyle(StrokeStyle::NoStroke);
    paintingContext.context.setFillColor(color);
    paintingContext.context.drawRect(borderRect);
}

void BorderPainter::clipBorderSidePolygon(PaintingContext& paintingContext, const FloatRoundedRect& outerBorder, const FloatRoundedRect& innerBorder, BoxSide side, bool firstEdgeMatches, bool secondEdgeMatches) const
{
    const FloatRect& outerRect = outerBorder.rect();
    const FloatRect& innerRect = innerBorder.rect();

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
        bool wasAntialiased = paintingContext.context.shouldAntialias();
        paintingContext.context.setShouldAntialias(!firstEdgeMatches);
        paintingContext.context.clipPath(Path::polygonPathFromPoints(quad), WindRule::NonZero);
        paintingContext.context.setShouldAntialias(wasAntialiased);
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
    bool wasAntialiased = paintingContext.context.shouldAntialias();
    paintingContext.context.setShouldAntialias(!firstEdgeMatches);
    paintingContext.context.clipPath(Path::polygonPathFromPoints(firstQuad), WindRule::NonZero);

    Vector<FloatPoint> secondQuad = {
        quad[0],
        side == BoxSide::Top || side == BoxSide::Bottom ? FloatPoint(quad[0].x(), quad[1].y()) : FloatPoint(quad[1].x(), quad[0].y()),
        quad[1],
        quad[2],
        quad[3]
    };
    // Antialiasing affects the second side.
    paintingContext.context.setShouldAntialias(!secondEdgeMatches);
    paintingContext.context.clipPath(Path::polygonPathFromPoints(secondQuad), WindRule::NonZero);

    paintingContext.context.setShouldAntialias(wasAntialiased);
}

void BorderPainter::drawLineForBoxSide(PaintingContext& paintingContext, const FloatRect& rect, BoxSide side, Color color, BorderStyle borderStyle, float adjacentWidth1, float adjacentWidth2, bool antialias) const
{
    auto drawBorderRect = [&](const FloatRect& rect) {
        if (rect.isEmpty())
            return;
        paintingContext.context.drawRect(rect);
    };

    auto drawLineFor = [&](const FloatRect& rect, BoxSide side, BorderStyle borderStyle, const FloatSize& adjacent) {
        if (rect.isEmpty())
            return;
        drawLineForBoxSide(paintingContext, rect, side, color, borderStyle, adjacent.width(), adjacent.height(), antialias);
    };
    
    auto& edge = m_edges.at(side);

    const float deviceScaleFactor = paintingContext.deviceScaleFactor;
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

    switch (borderStyle) {
    case BorderStyle::None:
    case BorderStyle::Hidden:
        return;
    case BorderStyle::Dotted:
    case BorderStyle::Dashed: {
        bool wasAntialiased = paintingContext.context.shouldAntialias();
        StrokeStyle oldStrokeStyle = paintingContext.context.strokeStyle();
        paintingContext.context.setShouldAntialias(antialias);
        paintingContext.context.setStrokeColor(color);
        paintingContext.context.setStrokeThickness(thickness);
        paintingContext.context.setStrokeStyle(borderStyle == BorderStyle::Dashed ? StrokeStyle::DashedStroke : StrokeStyle::DottedStroke);
        paintingContext.context.drawLine({ x1, y1 }, { x2, y2 });
        paintingContext.context.setShouldAntialias(wasAntialiased);
        paintingContext.context.setStrokeStyle(oldStrokeStyle);
        break;
    }
    case BorderStyle::Double: {
        float thirdOfThickness = ceilToDevicePixel(thickness / 3, deviceScaleFactor);
        if (!adjacentWidth1 && !adjacentWidth2) {
            StrokeStyle oldStrokeStyle = paintingContext.context.strokeStyle();
            paintingContext.context.setStrokeStyle(StrokeStyle::NoStroke);
            paintingContext.context.setFillColor(color);

            bool wasAntialiased = paintingContext.context.shouldAntialias();
            paintingContext.context.setShouldAntialias(antialias);

            switch (side) {
            case BoxSide::Top:
                drawBorderRect({ x1, y1, length, edge.outerWidth() });
                drawBorderRect({ x1, y2 - edge.innerWidth(), length, edge.innerWidth() });
                break;
            case BoxSide::Bottom:
                drawBorderRect({ x1, y2 - edge.outerWidth(), length, edge.outerWidth() });
                drawBorderRect({ x1, y1, length, edge.innerWidth() });
                break;
            case BoxSide::Left:
                drawBorderRect({ x1, y1, edge.outerWidth(), length });
                drawBorderRect({ x2 - edge.innerWidth(), y1, edge.innerWidth(), length });
                break;
            case BoxSide::Right:
                drawBorderRect({ x2 - edge.outerWidth(), y1, edge.outerWidth(), length });
                drawBorderRect({ x1, y1, edge.innerWidth(), length });
                break;
            }

            paintingContext.context.setShouldAntialias(wasAntialiased);
            paintingContext.context.setStrokeStyle(oldStrokeStyle);
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
        calculateBorderStyleColor(borderStyle, side, color);
        FALLTHROUGH;
    case BorderStyle::Solid: {
        StrokeStyle oldStrokeStyle = paintingContext.context.strokeStyle();
        ASSERT(x2 >= x1);
        ASSERT(y2 >= y1);
        if (!adjacentWidth1 && !adjacentWidth2) {
            paintingContext.context.setStrokeStyle(StrokeStyle::NoStroke);
            paintingContext.context.setFillColor(color);
            bool wasAntialiased = paintingContext.context.shouldAntialias();
            paintingContext.context.setShouldAntialias(antialias);
            drawBorderRect(snapRectToDevicePixels(LayoutRect(x1, y1, x2 - x1, y2 - y1), deviceScaleFactor));
            paintingContext.context.setShouldAntialias(wasAntialiased);
            paintingContext.context.setStrokeStyle(oldStrokeStyle);
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

        paintingContext.context.setStrokeStyle(StrokeStyle::NoStroke);
        paintingContext.context.setFillColor(color);
        bool wasAntialiased = paintingContext.context.shouldAntialias();
        paintingContext.context.setShouldAntialias(antialias);
        paintingContext.context.fillPath(Path::polygonPathFromPoints(quad));
        paintingContext.context.setShouldAntialias(wasAntialiased);

        paintingContext.context.setStrokeStyle(oldStrokeStyle);
        break;
    }
    }
}

void BorderPainter::paintOneBorderSide(PaintingContext& paintingContext, const FloatRoundedRect& outerBorder, const FloatRoundedRect& innerBorder, const FloatRect& sideRect, BoxSide side, const Path* path, bool antialias, const Color* overrideColor) const
{
    auto& edgeToRender = m_edges.at(side);
    ASSERT(edgeToRender.widthForPainting());
    
    auto adjacentSides = adjacentSidesForSide(side);
    
    auto& adjacentEdge1 = m_edges.at(adjacentSides.first);
    auto& adjacentEdge2 = m_edges.at(adjacentSides.second);

    bool mitreAdjacentSide1 = joinRequiresMitre(side, adjacentSides.first, !antialias);
    bool mitreAdjacentSide2 = joinRequiresMitre(side, adjacentSides.second, !antialias);
    
    bool adjacentSide1StylesMatch = colorsMatchAtCorner(side, adjacentSides.first);
    bool adjacentSide2StylesMatch = colorsMatchAtCorner(side, adjacentSides.second);

    const Color& colorToPaint = overrideColor ? *overrideColor : edgeToRender.color();

    if (path) {
        GraphicsContextStateSaver stateSaver(paintingContext.context);

        clipBorderSidePolygon(paintingContext, outerBorder, innerBorder, side, adjacentSide1StylesMatch, adjacentSide2StylesMatch);

        if (!innerBorder.isRenderable())
            paintingContext.context.clipOutRoundedRect(FloatRoundedRect(calculateAdjustedInnerBorder(innerBorder, side)));

        float thickness = std::max(std::max(edgeToRender.widthForPainting(), adjacentEdge1.widthForPainting()), adjacentEdge2.widthForPainting());
        drawBoxSideFromPath(paintingContext, outerBorder.rect(), *path, edgeToRender.widthForPainting(), thickness, side, colorToPaint, edgeToRender.style());
        return;
    }

    bool clipForStyle = styleRequiresClipPolygon(edgeToRender.style()) && (mitreAdjacentSide1 || mitreAdjacentSide2);
    bool clipAdjacentSide1 = colorNeedsAntiAliasAtCorner(side, adjacentSides.first) && mitreAdjacentSide1;
    bool clipAdjacentSide2 = colorNeedsAntiAliasAtCorner(side, adjacentSides.second) && mitreAdjacentSide2;
    bool shouldClip = clipForStyle || clipAdjacentSide1 || clipAdjacentSide2;
    
    GraphicsContextStateSaver clipStateSaver(paintingContext.context, shouldClip);
    if (shouldClip) {
        bool aliasAdjacentSide1 = clipAdjacentSide1 || (clipForStyle && mitreAdjacentSide1);
        bool aliasAdjacentSide2 = clipAdjacentSide2 || (clipForStyle && mitreAdjacentSide2);
        clipBorderSidePolygon(paintingContext, outerBorder, innerBorder, side, !aliasAdjacentSide1, !aliasAdjacentSide2);
        // Since we clipped, no need to draw with a mitre.
        mitreAdjacentSide1 = false;
        mitreAdjacentSide2 = false;
    }
    drawLineForBoxSide(paintingContext, sideRect, side, colorToPaint, edgeToRender.style(), mitreAdjacentSide1 ? adjacentEdge1.widthForPainting() : 0, mitreAdjacentSide2 ? adjacentEdge2.widthForPainting() : 0, antialias);
}

void BorderPainter::paintBorderSides(PaintingContext& paintingContext, const FloatRoundedRect& outerBorder, const FloatRoundedRect& innerBorder, FloatSize innerBorderBleedAdjustment, BoxSideSet edgeSet, bool antialias, const Color* overrideColor) const
{
    bool renderRadii = outerBorder.isRounded();

    Path roundedPath;
    if (renderRadii)
        roundedPath.addRoundedRect(outerBorder);

    auto paintOneSide = [&](BoxSide side) {
        auto& edge = m_edges.at(side);
        if (!edge.shouldRender() || !edgeSet.contains(edgeFlagForSide(side)))
            return;

        auto sideRect = outerBorder.rect();
        FloatSize firstRadius;
        FloatSize secondRadius;

        switch (side) {
        case BoxSide::Top:
            sideRect.setHeight(edge.widthForPainting() + innerBorderBleedAdjustment.height());
            firstRadius = innerBorder.radii().topLeft();
            secondRadius = innerBorder.radii().topRight();
            break;
        case BoxSide::Right:
            sideRect.shiftXEdgeTo(sideRect.maxX() - edge.widthForPainting() - innerBorderBleedAdjustment.width());
            firstRadius = innerBorder.radii().bottomRight();
            secondRadius = innerBorder.radii().topRight();
            break;
        case BoxSide::Bottom:
            sideRect.shiftYEdgeTo(sideRect.maxY() - edge.widthForPainting() - innerBorderBleedAdjustment.height());
            firstRadius = innerBorder.radii().bottomLeft();
            secondRadius = innerBorder.radii().bottomRight();
            break;
        case BoxSide::Left:
            sideRect.setWidth(edge.widthForPainting() + innerBorderBleedAdjustment.width());
            firstRadius = innerBorder.radii().bottomLeft();
            secondRadius = innerBorder.radii().topLeft();
            break;
        }

        bool usePath = renderRadii && (borderStyleHasInnerDetail(edge.style()) || borderWillArcInnerEdge(firstRadius, secondRadius));
        paintOneBorderSide(paintingContext, outerBorder, innerBorder, sideRect, side, usePath ? &roundedPath : nullptr, antialias, overrideColor);
    };

    paintOneSide(BoxSide::Top);
    paintOneSide(BoxSide::Bottom);
    paintOneSide(BoxSide::Left);
    paintOneSide(BoxSide::Right);
}

void BorderPainter::paintTranslucentBorderSides(PaintingContext& paintingContext, const FloatRoundedRect& outerBorder, const FloatRoundedRect& innerBorder, FloatSize innerBorderBleedAdjustment, BoxSideSet edgesToDraw, bool antialias) const
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

            auto& edge = m_edges.at(side);
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
        {
            auto transparencyScope = TransparencyLayerScope { paintingContext.context, commonColor.alphaAsFloat(), useTransparencyLayer };
            if (useTransparencyLayer)
                commonColor = commonColor.opaqueColor();

            paintBorderSides(paintingContext, outerBorder, innerBorder, innerBorderBleedAdjustment, commonColorEdgeSet, antialias, &commonColor);
        }
        
        edgesToDraw.remove(commonColorEdgeSet);
    }
}

static FloatRect shrinkRectByOneDevicePixel(const PaintingContext& paintingContext, const FloatRect& rect)
{
    auto shrunkRect = rect;
    auto transform = paintingContext.context.getCTM();
    shrunkRect.inflateX(-ceilToDevicePixel(1.0f / transform.xScale(), paintingContext.deviceScaleFactor));
    shrunkRect.inflateY(-ceilToDevicePixel(1.0f / transform.yScale(), paintingContext.deviceScaleFactor));
    return shrunkRect;
}

FloatRect BorderPainter::borderInnerRectAdjustedForBleedAvoidance(const PaintingContext& paintingContext) const
{
    if (m_bleedAvoidance != BackgroundBleedAvoidance::BackgroundOverBorder)
        return m_borderRect.rect();

    // We shrink the rectangle by one device pixel on each side to make it fully overlap the anti-aliased background border
    return shrinkRectByOneDevicePixel(paintingContext, m_borderRect.rect());
}

void BorderPainter::paintBorders(PaintingContext& paintingContext) const
{
    bool haveAlphaColor = false;
    bool haveAllSolidEdges = true;
    bool haveAllDoubleEdges = true;
    int numEdgesVisible = 4;
    bool allEdgesShareColor = true;
    std::optional<BoxSide> firstVisibleSide;
    BoxSideSet edgesToDraw;

    for (auto side : allBoxSides) {
        auto& currEdge = m_edges.at(side);

        if (currEdge.shouldRender())
            edgesToDraw.add(edgeFlagForSide(side));

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
            firstVisibleSide = side;
        else if (currEdge.color() != m_edges.at(*firstVisibleSide).color())
            allEdgesShareColor = false;

        if (!currEdge.color().isOpaque())
            haveAlphaColor = true;
        
        if (currEdge.style() != BorderStyle::Solid)
            haveAllSolidEdges = false;

        if (currEdge.style() != BorderStyle::Double)
            haveAllDoubleEdges = false;
    }

    FloatRoundedRect innerBorderRect = roundedInsetBorderForRect(borderInnerRectAdjustedForBleedAvoidance(paintingContext), m_borderRect.radii(), borderWidths(m_edges), m_includeLeftEdge, m_includeRightEdge);

    // FIXME: If no corner intersects the clip region, we can pretend outerBorder is rectangular to improve performance.
    
    if ((haveAllSolidEdges || haveAllDoubleEdges) && allEdgesShareColor && innerBorderRect.isRenderable()) {
        // Fast path for drawing all solid edges and all unrounded double edges
        if (numEdgesVisible == 4 && (m_borderRect.isRounded() || haveAlphaColor) && (haveAllSolidEdges || (!m_borderRect.isRounded() && !innerBorderRect.isRounded()))) {
            Path path;
            
            if (m_borderRect.isRounded() && m_bleedAvoidance != BackgroundBleedAvoidance::UseTransparencyLayer)
                path.addRoundedRect(m_borderRect);
            else
                path.addRect(m_borderRect.rect());

            if (haveAllDoubleEdges) {
                auto innerThirdRect = m_borderRect.rect();
                auto outerThirdRect = m_borderRect.rect();

                for (auto side : allBoxSides) {
                    auto& edge = m_edges.at(side);
                    float outerWidth = edge.outerWidth();
                    float innerWidth = edge.innerWidth();
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

                auto outerThirdRoundedRect = m_borderRect;
                outerThirdRoundedRect.setRect(outerThirdRect);

                if (outerThirdRoundedRect.isRounded() && m_bleedAvoidance != BackgroundBleedAvoidance::UseTransparencyLayer)
                    path.addRoundedRect(outerThirdRoundedRect);
                else
                    path.addRect(outerThirdRoundedRect.rect());

                auto innerThirdRoundedRect = innerBorderRect;
                innerThirdRoundedRect.setRect(innerThirdRect);
                if (innerThirdRoundedRect.isRounded())
                    path.addRoundedRect(innerThirdRoundedRect);
                else
                    path.addRect(innerThirdRoundedRect.rect());
            }

            if (innerBorderRect.isRounded())
                path.addRoundedRect(innerBorderRect);
            else
                path.addRect(innerBorderRect.rect());
            
            paintingContext.context.setFillRule(WindRule::EvenOdd);
            paintingContext.context.setFillColor(m_edges.top().color());
            paintingContext.context.fillPath(path);
            return;
        }

        // Avoid creating transparent layers
        if (haveAllSolidEdges && numEdgesVisible != 4 && !m_borderRect.isRounded() && haveAlphaColor) {
            Path path;

            auto calculateSideRect = [&](const BorderEdge& edge, BoxSide side) {
                auto sideRect = m_borderRect.rect();
                auto width = edge.widthForPainting();

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
            };

            for (auto side : allBoxSides) {
                auto& edge = m_edges.at(side);
                if (edge.shouldRender()) {
                    auto sideRect = calculateSideRect(edge, side);
                    path.addRect(sideRect);
                }
            }

            paintingContext.context.setFillRule(WindRule::NonZero);
            paintingContext.context.setFillColor(m_edges.at(*firstVisibleSide).color());
            paintingContext.context.fillPath(path);
            return;
        }
    }

    bool clipToOuterBorder = m_borderRect.isRounded();
    GraphicsContextStateSaver stateSaver(paintingContext.context, clipToOuterBorder);
    if (clipToOuterBorder) {
        // Clip to the inner and outer radii rects.
        if (m_bleedAvoidance != BackgroundBleedAvoidance::UseTransparencyLayer)
            paintingContext.context.clipRoundedRect(m_borderRect);

        if (innerBorderRect.isRenderable())
            paintingContext.context.clipOutRoundedRect(innerBorderRect);
    }

    bool shouldAntialiasLines = !paintingContext.context.getCTM().isIdentityOrTranslationOrFlipped();

    // If only one edge visible antialiasing doesn't create seams
    bool antialias = shouldAntialiasLines || numEdgesVisible == 1;

    auto unadjustedInnerBorder = innerBorderRect;
    FloatSize innerBorderBleedAdjustment;
    if (m_bleedAvoidance == BackgroundBleedAvoidance::BackgroundOverBorder) {
        unadjustedInnerBorder = roundedInsetBorderForRect(m_borderRect.rect(), m_borderRect.radii(), borderWidths(m_edges), m_includeLeftEdge, m_includeRightEdge);
        innerBorderBleedAdjustment = innerBorderRect.rect().location() - unadjustedInnerBorder.rect().location();
    }

    if (haveAlphaColor)
        paintTranslucentBorderSides(paintingContext, m_borderRect, innerBorderRect, innerBorderBleedAdjustment, edgesToDraw, antialias); // FIXME.
    else
        paintBorderSides(paintingContext, m_borderRect, innerBorderRect, innerBorderBleedAdjustment, edgesToDraw, antialias);
}

BoxDecorationPainter::BoxDecorationPainter(const BoxModelBox& box, PaintingContext& paintingContext, bool includeLeftEdge, bool includeRightEdge)
    : m_box(box)
    , m_borderRect(box.borderRoundedRect())
    , m_bleedAvoidance(determineBackgroundBleedAvoidance(box, paintingContext))
    , m_includeLeftEdge(includeLeftEdge)
    , m_includeRightEdge(includeRightEdge)
{
}

void BoxDecorationPainter::paintBorders(PaintingContext& paintingContext) const
{
    auto* boxDecorationData = m_box.boxDecorationData();
    if (!boxDecorationData)
        return;

    if (boxDecorationData->hasBorderImage()) {
        // border-image is not affected by border-radius.
        // FIXME: Implement border-image painting.
        return;
    }

    auto outerBorder = borderRoundedRect();
    BorderPainter painter(boxDecorationData->borderEdges(), outerBorder, m_bleedAvoidance, m_includeLeftEdge, m_includeRightEdge);
    painter.paintBorders(paintingContext);
}

void BoxDecorationPainter::paintFillLayer(PaintingContext& paintingContext, const FillLayer& layer, const FillLayerImageGeometry& geometry) const
{
    GraphicsContextStateSaver stateSaver(paintingContext.context, false);

    auto clipRectForLayer = [](const BoxModelBox& box, const FillLayer& layer) -> UnadjustedAbsoluteFloatRect {
        switch (layer.clip()) {
        case FillBox::Border:
            return box.absoluteBorderBoxRect();
        case FillBox::Padding:
            return box.absolutePaddingBoxRect();
        case FillBox::Content:
            return box.absoluteContentBoxRect();
        case FillBox::Text:
        case FillBox::NoClip:
            break;
        }
        return { };
    };

    switch (layer.clip()) {
    case FillBox::Border:
    case FillBox::Padding:
    case FillBox::Content: {
        stateSaver.save();
        paintingContext.context.clip(clipRectForLayer(m_box, layer));
        break;
    }
    case FillBox::Text:
    case FillBox::NoClip:
        break;
    }

    // FIXME: Handle background compositing modes.

    auto* backgroundImage = layer.image();
    CompositeOperator op = CompositeOperator::SourceOver;

    if (geometry.destRect().isEmpty())
        return;

    auto image = backgroundImage->image(nullptr, geometry.tileSize());
    if (!image)
        return;

    // FIXME: call image->updateFromSettings().

    ImagePaintingOptions options = {
        op == CompositeOperator::SourceOver ? layer.composite() : op,
        layer.blendMode(),
        DecodingMode::Synchronous,
        ImageOrientation::FromImage,
        InterpolationQuality::Default
    };

    paintingContext.context.drawTiledImage(*image, geometry.destRect(), toFloatPoint(geometry.relativePhase()), geometry.tileSize(), geometry.spaceSize(), options);
}

void BoxDecorationPainter::paintBoxShadow(PaintingContext& paintingContext, ShadowStyle shadowStyle) const
{
    if (!m_box.style().boxShadow())
        return;

    auto borderRect = shadowStyle == ShadowStyle::Inset ? innerBorderRoundedRect() : borderRoundedRect();
    auto* boxDecorationData = m_box.boxDecorationData();
    bool hasBorderRadius = m_box.hasBorderRadius();

    bool hasOpaqueBackground = m_box.style().backgroundColor().isOpaque();

    auto paintNormalShadow = [&](const ShadowData& shadow) {
        // FIXME: Snapping here isn't ideal. It would be better to compute a rect which is border rect + offset + spread, and snap that at tree building time.
        auto shadowOffset = roundSizeToDevicePixels({ shadow.x().value(), shadow.y().value() }, paintingContext.deviceScaleFactor);
        float shadowPaintingExtent = ceilToDevicePixel(shadow.paintingExtent(), paintingContext.deviceScaleFactor);
        float shadowSpread = roundToDevicePixel(shadow.spread().value(), paintingContext.deviceScaleFactor);
        auto shadowRadius = shadow.radius();

        auto fillRect = borderRect;
        fillRect.inflate(shadowSpread);
        if (fillRect.isEmpty())
            return;

        auto shadowRect = borderRect.rect();
        shadowRect.inflate(shadowPaintingExtent + shadowSpread);
        shadowRect.move(shadowOffset);

        GraphicsContextStateSaver stateSaver(paintingContext.context);
        paintingContext.context.clip(shadowRect);

        // Move the fill just outside the clip, adding at least 1 pixel of separation so that the fill does not
        // bleed in (due to antialiasing) if the context is transformed.
        float xOffset = shadowRect.width() + std::max<float>(0, shadowOffset.width()) + shadowPaintingExtent + 2 * shadowSpread + 1.0f;
        auto extraOffset = FloatSize { std::ceil(xOffset), 0 };
        shadowOffset -= extraOffset;
        fillRect.move(extraOffset);

        auto rectToClipOut = borderRect;
        auto adjustedFillRect = fillRect;

        auto shadowRectOrigin = fillRect.rect().location() + shadowOffset;
        auto adjustedShadowOffset = shadowRectOrigin - adjustedFillRect.rect().location();

        paintingContext.context.setShadow(adjustedShadowOffset, shadowRadius.value(), shadow.color(), shadow.isWebkitBoxShadow() ? ShadowRadiusMode::Legacy : ShadowRadiusMode::Default);

        if (hasBorderRadius) {
            // If the box is opaque, it is unnecessary to clip it out. However, doing so saves time
            // when painting the shadow. On the other hand, it introduces subpixel gaps along the
            // corners. Those are avoided by insetting the clipping path by one pixel.
            if (hasOpaqueBackground)
                rectToClipOut.inflateWithRadii(-1.0f);

            if (!rectToClipOut.isEmpty())
                paintingContext.context.clipOutRoundedRect(rectToClipOut);

            auto influenceRect = FloatRoundedRect { shadowRect, borderRect.radii() };
            influenceRect.expandRadii(2 * shadowPaintingExtent + shadowSpread);

            // FIXME: Optimize for clipped-out corners
            adjustedFillRect.expandRadii(shadowSpread);
            if (!adjustedFillRect.isRenderable())
                adjustedFillRect.adjustRadii();
            paintingContext.context.fillRoundedRect(adjustedFillRect, Color::black);
        } else {
            // If the box is opaque, it is unnecessary to clip it out. However, doing so saves time
            // when painting the shadow. On the other hand, it introduces subpixel gaps along the
            // edges if they are not pixel-aligned. Those are avoided by insetting the clipping path
            // by one pixel.
            if (hasOpaqueBackground) {
                // FIXME: The function to decide on the policy based on the transform should be a named function.
                // FIXME: It's not clear if this check is right. What about integral scale factors?
                AffineTransform transform = paintingContext.context.getCTM();
                if (transform.a() != 1 || (transform.d() != 1 && transform.d() != -1) || transform.b() || transform.c())
                    rectToClipOut.inflate(-1.0f);
            }

            if (!rectToClipOut.isEmpty())
                paintingContext.context.clipOut(rectToClipOut.rect());

            paintingContext.context.fillRect(adjustedFillRect.rect(), Color::black);
        }
    };

    auto paintInsetShadow = [&](const ShadowData& shadow) {
        auto shadowOffset = roundSizeToDevicePixels({ shadow.x().value(), shadow.y().value() }, paintingContext.deviceScaleFactor);
        float shadowPaintingExtent = ceilToDevicePixel(shadow.paintingExtent(), paintingContext.deviceScaleFactor);
        float shadowSpread = roundToDevicePixel(shadow.spread().value(), paintingContext.deviceScaleFactor);
        auto shadowRadius = shadow.radius();

        auto holeRect = borderRect.rect();
        holeRect.inflate(-shadowSpread);

        if (!m_includeLeftEdge) {
            // FIXME: Need to take writing mode into account.
            holeRect.shiftXEdgeBy(-(std::max<float>(shadowOffset.width(), 0) + shadowPaintingExtent + shadowSpread));
        }

        if (!m_includeRightEdge) {
            // FIXME: Need to take writing mode into account.
            holeRect.setWidth(holeRect.width() - std::min<float>(shadowOffset.width(), 0) + shadowPaintingExtent + shadowSpread);
        }

        auto roundedHoleRect = FloatRoundedRect { holeRect, borderRect.radii() };
        if (shadowSpread && roundedHoleRect.isRounded()) {
            auto roundedRectCorrectingForSpread = [&]() {
                bool horizontal = true; // FIXME: Handle writing modes.
                auto borderWidth = borderWidths(boxDecorationData->borderEdges());

                float leftWidth { (!horizontal || m_includeLeftEdge) ? borderWidth.left() + shadowSpread : 0 };
                float rightWidth { (!horizontal || m_includeRightEdge) ? borderWidth.right() + shadowSpread : 0 };
                float topWidth { (horizontal || m_includeLeftEdge) ? borderWidth.top() + shadowSpread : 0 };
                float bottomWidth { (horizontal || m_includeRightEdge) ? borderWidth.bottom() + shadowSpread : 0 };

                return roundedInsetBorderForRect(m_borderRect.rect(), m_borderRect.radii(), { topWidth, rightWidth, bottomWidth, leftWidth }, m_includeLeftEdge, m_includeRightEdge);
            }();
            roundedHoleRect.setRadii(roundedRectCorrectingForSpread.radii());
        }

        if (roundedHoleRect.isEmpty()) {
            if (hasBorderRadius)
                paintingContext.context.fillRoundedRect(borderRect, shadow.color());
            else
                paintingContext.context.fillRect(borderRect.rect(), shadow.color());
            return;
        }

        auto areaCastingShadowInHole = [](const FloatRect& holeRect, float shadowExtent, float shadowSpread, FloatSize shadowOffset) {
            auto bounds(holeRect);
            
            bounds.inflate(shadowExtent);

            if (shadowSpread < 0)
                bounds.inflate(-shadowSpread);
            
            auto offsetBounds = bounds;
            offsetBounds.move(-shadowOffset);
            return unionRect(bounds, offsetBounds);
        };

        Color fillColor = shadow.color().opaqueColor();
        auto shadowCastingRect = areaCastingShadowInHole(borderRect.rect(), shadowPaintingExtent, shadowSpread, shadowOffset);

        GraphicsContextStateSaver stateSaver(paintingContext.context);
        if (hasBorderRadius)
            paintingContext.context.clipRoundedRect(borderRect);
        else
            paintingContext.context.clip(borderRect.rect());

        float xOffset = shadowCastingRect.width() + std::max<float>(0, shadowOffset.width()) + shadowPaintingExtent - 2 * shadowSpread + 1.0f;
        auto extraOffset = FloatSize { std::ceil(xOffset), 0 };

        paintingContext.context.translate(extraOffset);
        shadowOffset -= extraOffset;

        paintingContext.context.setShadow(shadowOffset, shadowRadius.value(), shadow.color(), shadow.isWebkitBoxShadow() ? ShadowRadiusMode::Legacy : ShadowRadiusMode::Default);
        paintingContext.context.fillRectWithRoundedHole(shadowCastingRect, roundedHoleRect, fillColor);
    };


    for (auto* shadow = m_box.style().boxShadow(); shadow; shadow = shadow->next()) {
        if (shadow->style() != shadowStyle)
            continue;

        LayoutSize shadowOffset(shadow->x().value(), shadow->y().value());
        if (shadowOffset.isZero() && shadow->radius().isZero() && shadow->spread().isZero())
            continue;

        if (shadow->style() == ShadowStyle::Normal)
            paintNormalShadow(*shadow);
        else
            paintInsetShadow(*shadow);
    }
}

void BoxDecorationPainter::paintBackgroundImages(PaintingContext& paintingContext) const
{
    const auto& style = m_box.style();

    Vector<const FillLayer*, 8> layers;

    for (auto* layer = style.backgroundLayers(); layer; layer = layer->next())
        layers.append(layer);

    auto* boxDecorationData = m_box.boxDecorationData();
    ASSERT(boxDecorationData);

    auto& layerGeometryList = boxDecorationData->backgroundImageGeometry();

    for (int i = layers.size() - 1; i >=0; --i) {
        const auto* layer = layers[i];
        const auto& geometry = layerGeometryList[i];
        paintFillLayer(paintingContext, *layer, geometry);
    }
}

FloatRoundedRect BoxDecorationPainter::innerBorderRoundedRect() const
{
    return m_box.innerBorderRoundedRect();
}

FloatRoundedRect BoxDecorationPainter::backgroundRoundedRectAdjustedForBleedAvoidance(const PaintingContext& paintingContext) const
{
    if (m_bleedAvoidance == BackgroundBleedAvoidance::ShrinkBackground) {
        // We shrink the rectangle by one device pixel on each side because the bleed is one pixel maximum.
        return roundedRectWithIncludedRadii(shrinkRectByOneDevicePixel(paintingContext, m_borderRect.rect()), m_borderRect.radii(), m_includeLeftEdge, m_includeRightEdge);
    }

    if (m_bleedAvoidance == BackgroundBleedAvoidance::BackgroundOverBorder)
        return innerBorderRoundedRect();

    return roundedRectWithIncludedRadii(m_borderRect.rect(), m_borderRect.radii(), m_includeLeftEdge, m_includeRightEdge);
}

void BoxDecorationPainter::paintBackground(PaintingContext& paintingContext) const
{
    auto borderBoxRect = m_box.absoluteBorderBoxRect();
    const auto& style = m_box.style();

    if (style.hasBackground()) {
        GraphicsContextStateSaver stateSaver(paintingContext.context, false);
        if (m_bleedAvoidance != BackgroundBleedAvoidance::UseTransparencyLayer && m_box.hasBorderRadius()) {
            stateSaver.save();
            auto outerBorder = backgroundRoundedRectAdjustedForBleedAvoidance(paintingContext);
            paintingContext.context.clipRoundedRect(outerBorder);
        }
    
        paintingContext.context.fillRect(borderBoxRect, style.backgroundColor());

        if (style.hasBackgroundImage())
            paintBackgroundImages(paintingContext);
    }
}

BackgroundBleedAvoidance BoxDecorationPainter::determineBackgroundBleedAvoidance(const BoxModelBox& box, PaintingContext& paintingContext)
{
    if (paintingContext.context.paintingDisabled())
        return BackgroundBleedAvoidance::None;

    auto& style = box.style();
    auto* boxDecorationData = box.boxDecorationData();
    
    auto hasBackgroundAndRoundedBorder = [&] {
        if (!boxDecorationData)
            return false;

        // FIXME: Consult border-image.
        return style.hasBackground() && boxDecorationData->hasBorder() && box.hasBorderRadius();
    };

    if (!hasBackgroundAndRoundedBorder())
        return BackgroundBleedAvoidance::None;

    auto ctm = paintingContext.context.getCTM();
    auto contextScaling = FloatSize { static_cast<float>(ctm.xScale()), static_cast<float>(ctm.yScale()) };
    if (boxDecorationData->borderObscuresBackgroundEdge(contextScaling))
        return BackgroundBleedAvoidance::ShrinkBackground;

    if (boxDecorationData->borderObscuresBackground() && style.backgroundHasOpaqueTopLayer())
        return BackgroundBleedAvoidance::BackgroundOverBorder;

    return BackgroundBleedAvoidance::UseTransparencyLayer;
}

void BoxDecorationPainter::paintBackgroundAndBorders(PaintingContext& paintingContext) const
{
    // FIXME: Table decoration painting is special.
    
    switch (m_bleedAvoidance) {
    case BackgroundBleedAvoidance::BackgroundOverBorder:
        paintBoxShadow(paintingContext, ShadowStyle::Normal);
        paintBorders(paintingContext);
        paintBackground(paintingContext);
        paintBoxShadow(paintingContext, ShadowStyle::Inset);
        break;

    case BackgroundBleedAvoidance::UseTransparencyLayer: {
        paintBoxShadow(paintingContext, ShadowStyle::Normal);
        GraphicsContextStateSaver stateSaver(paintingContext.context);
        auto outerBorder = borderRoundedRect();
        paintingContext.context.clipRoundedRect(outerBorder);
        paintingContext.context.beginTransparencyLayer(1);

        paintBackground(paintingContext);
        paintBoxShadow(paintingContext, ShadowStyle::Inset);
        paintBorders(paintingContext);

        paintingContext.context.endTransparencyLayer();
        break;
    }
    
    case BackgroundBleedAvoidance::ShrinkBackground:
    case BackgroundBleedAvoidance::None:
        paintBoxShadow(paintingContext, ShadowStyle::Normal);
        paintBackground(paintingContext);
        paintBoxShadow(paintingContext, ShadowStyle::Inset);
        paintBorders(paintingContext);
        break;
    }
}

} // namespace Display
} // namespace WebCore

