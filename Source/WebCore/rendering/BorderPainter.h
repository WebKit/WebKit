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

namespace WebCore {

class BorderPainter {
public:
    BorderPainter(const RenderBoxModelObject&, const PaintInfo&);

    void paintBorder(const LayoutRect&, const RenderStyle&, BackgroundBleedAvoidance = BackgroundBleedNone, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true);
    bool paintNinePieceImage(const LayoutRect&, const RenderStyle&, const NinePieceImage&, CompositeOperator = CompositeOperator::SourceOver);

    static bool allCornersClippedOut(const RoundedRect&, const LayoutRect&);
    static bool shouldAntialiasLines(GraphicsContext&);
    static Color calculateBorderStyleColor(const BorderStyle&, const BoxSide&, const Color&);

private:
    void paintTranslucentBorderSides(const RenderStyle&, const RoundedRect& outerBorder, const RoundedRect& innerBorder, const IntPoint& innerBorderAdjustment,
        const BorderEdges&, BoxSideSet edgesToDraw, BackgroundBleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias);
    void paintBorderSides(const RenderStyle&, const RoundedRect& outerBorder, const RoundedRect& innerBorder, const IntPoint& innerBorderAdjustment, const BorderEdges&, BoxSideSet edgeSet, BackgroundBleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias, const Color* overrideColor = nullptr);
    void paintOneBorderSide(const RenderStyle&, const RoundedRect& outerBorder, const RoundedRect& innerBorder,
        const LayoutRect& sideRect, BoxSide, BoxSide adjacentSide1, BoxSide adjacentSide2, const BorderEdges&, const Path*, BackgroundBleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias, const Color* overrideColor);
    void drawBoxSideFromPath(const LayoutRect& borderRect, const Path& borderPath, const BorderEdges&, float thickness, float drawThickness, BoxSide, const RenderStyle&, Color, BorderStyle, BackgroundBleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge);
    void clipBorderSidePolygon(const RoundedRect& outerBorder, const RoundedRect& innerBorder, BoxSide, bool firstEdgeMatches, bool secondEdgeMatches);

    LayoutRect borderInnerRectAdjustedForBleedAvoidance(const LayoutRect&, BackgroundBleedAvoidance) const;

    const Document& document() const;

    const RenderBoxModelObject& m_renderer;
    const PaintInfo& m_paintInfo;
};

LayoutRect shrinkRectByOneDevicePixel(const GraphicsContext&, const LayoutRect&, float devicePixelRatio);

}
