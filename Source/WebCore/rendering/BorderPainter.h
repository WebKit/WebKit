/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2022 Apple Inc. All rights reserved.
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

#pragma once

#include "RenderBoxModelObject.h"
#include "RenderElement.h"

namespace WebCore {

class BorderPainter {
public:
    BorderPainter(const RenderElement&, const PaintInfo&);

    void paintBorder(const LayoutRect&, const RenderStyle&, BackgroundBleedAvoidance = BackgroundBleedNone, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true);
    void paintOutline(const LayoutRect&);
    void paintOutline(const LayoutPoint& paintOffset, const Vector<LayoutRect>& lineRects);

    bool paintNinePieceImage(const LayoutRect&, const RenderStyle&, const NinePieceImage&, CompositeOperator = CompositeOperator::SourceOver);
    struct Sides {
        RoundedRect outerBorder;
        RoundedRect innerBorder;
        RoundedRect unadjustedInnerBorder;
        std::optional<BorderData::Radii> radii { };
        const BorderEdges& edges;
        bool haveAllSolidEdges { true };
        BackgroundBleedAvoidance bleedAvoidance { BackgroundBleedNone };
        bool includeLogicalLeftEdge { true };
        bool includeLogicalRightEdge { true };
        bool appliedClipAlready { false };
        bool isHorizontal { true };
    };
    void paintSides(const Sides&);

    static void drawLineForBoxSide(GraphicsContext&, const Document&, const FloatRect&, BoxSide, Color, BorderStyle, float adjacentWidth1, float adjacentWidth2, bool antialias = false);

    static bool allCornersClippedOut(const RoundedRect&, const LayoutRect&);
    static bool shouldAntialiasLines(GraphicsContext&);
    static Color calculateBorderStyleColor(const BorderStyle&, const BoxSide&, const Color&);

private:
    void paintTranslucentBorderSides(const RoundedRect& outerBorder, const RoundedRect& innerBorder, const IntPoint& innerBorderAdjustment,
        const BorderEdges&, BoxSideSet edgesToDraw, std::optional<BorderData::Radii>, BackgroundBleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias, bool isHorizontal);
    void paintBorderSides(const RoundedRect& outerBorder, const RoundedRect& innerBorder, const IntPoint& innerBorderAdjustment, const BorderEdges&, BoxSideSet edgeSet, std::optional<BorderData::Radii>, BackgroundBleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias, bool isHorizontal, const Color* overrideColor = nullptr);
    void paintOneBorderSide(const RoundedRect& outerBorder, const RoundedRect& innerBorder,
        const LayoutRect& sideRect, BoxSide, BoxSide adjacentSide1, BoxSide adjacentSide2, const BorderEdges&, std::optional<BorderData::Radii>, const Path*, BackgroundBleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias, bool isHorizontal, const Color* overrideColor);
    void drawBoxSideFromPath(const LayoutRect& borderRect, const Path& borderPath, const BorderEdges&, std::optional<BorderData::Radii>, float thickness, float drawThickness, BoxSide, Color, BorderStyle, BackgroundBleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool isHorizontal);
    void clipBorderSidePolygon(const RoundedRect& outerBorder, const RoundedRect& innerBorder, BoxSide, bool firstEdgeMatches, bool secondEdgeMatches);

    LayoutRect borderInnerRectAdjustedForBleedAvoidance(const LayoutRect&, BackgroundBleedAvoidance) const;

    const Document& document() const;

    const RenderElement& m_renderer;
    const PaintInfo& m_paintInfo;
};

LayoutRect shrinkRectByOneDevicePixel(const GraphicsContext&, const LayoutRect&, float devicePixelRatio);

}
