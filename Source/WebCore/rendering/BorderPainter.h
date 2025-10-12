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

#include "GraphicsTypes.h"
#include "RenderBoxModelObject.h"
#include "RenderElement.h"

namespace WebCore {

class BorderShape;

class BorderPainter {
public:
    BorderPainter(const RenderElement&, const PaintInfo&);

    void paintBorder(const LayoutRect&, const RenderStyle&, BleedAvoidance = BleedAvoidance::None, RectEdges<bool> closedEdges = { true }) const;
    void paintOutline(const LayoutRect&) const;
    void paintOutline(const LayoutPoint& paintOffset, const Vector<LayoutRect>& lineRects) const;

    bool paintNinePieceImage(const LayoutRect&, const RenderStyle&, const Style::BorderImage&, CompositeOperator = CompositeOperator::SourceOver) const;
    bool paintNinePieceImage(const LayoutRect&, const RenderStyle&, const Style::MaskBorder&, CompositeOperator = CompositeOperator::SourceOver) const;

    static void drawLineForBoxSide(GraphicsContext&, const Document&, const FloatRect&, BoxSide, Color, BorderStyle, float adjacentWidth1, float adjacentWidth2, bool antialias = false);

    static std::optional<Path> pathForBorderArea(const LayoutRect&, const RenderStyle&, float deviceScaleFactor, RectEdges<bool> closedEdges = { true });

    static bool shouldAntialiasLines(GraphicsContext&);

private:
    struct Sides;
    void paintSides(const BorderShape&, const Sides&) const;

    template<typename T>
    bool paintNinePieceImageImpl(const LayoutRect&, const RenderStyle&, const T&, CompositeOperator = CompositeOperator::SourceOver) const;

    void paintTranslucentBorderSides(const BorderShape&, const Sides&, BoxSideSet edgesToDraw, bool antialias) const;
    void paintBorderSides(const BorderShape&, const Sides&, BoxSideSet edgesToDraw, bool antialias, const Color* overrideColor = nullptr) const;

    void paintOneBorderSide(const BorderShape&, const Sides&, const LayoutRect& sideRect, BoxSide, BoxSide adjacentSide1, BoxSide adjacentSide2, const Path*, bool antialias, const Color* overrideColor) const;

    void drawBoxSideFromPath(const BorderShape&, const Path& borderPath, const BorderEdges&, float thickness, float drawThickness, BoxSide, Color, BorderStyle, BleedAvoidance) const;
    void clipBorderSidePolygon(const BorderShape&, BoxSide, bool firstEdgeMatches, bool secondEdgeMatches) const;

    LayoutRect borderRectAdjustedForBleedAvoidance(const LayoutRect&, BleedAvoidance) const;

    static Color calculateBorderStyleColor(const BorderStyle&, const BoxSide&, const Color&);

    const Document& document() const;

    CheckedRef<const RenderElement> m_renderer;
    const PaintInfo& m_paintInfo;
};

LayoutRect shrinkRectByOneDevicePixel(const GraphicsContext&, const LayoutRect&, float devicePixelRatio);

}
