/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2006 Apple Inc.
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2011 Renata Hodovan <reni@webkit.org>
 * Copyright (C) 2011 University of Szeged
 * Copyright (C) 2020, 2021, 2022 Igalia S.L.
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
 */

#pragma once

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "RenderSVGShape.h"

namespace WebCore {

class RenderSVGPath final : public RenderSVGShape {
    WTF_MAKE_ISO_ALLOCATED(RenderSVGPath);
public:
    RenderSVGPath(SVGGraphicsElement&, RenderStyle&&);
    virtual ~RenderSVGPath();

    FloatRect computeMarkerBoundingBox(const SVGBoundingBoxComputation::DecorationOptions&) const;

    void updateMarkerPositions();

private:
    ASCIILiteral renderName() const override { return "RenderSVGPath"_s; }

    void updateShapeFromElement() override;
    FloatRect adjustStrokeBoundingBoxForZeroLengthLinecaps(RepaintRectCalculation, FloatRect strokeBoundingBox) const override;

    void strokeShape(GraphicsContext&) const override;
    bool shapeDependentStrokeContains(const FloatPoint&, PointCoordinateSpace = GlobalCoordinateSpace) override;

    bool shouldStrokeZeroLengthSubpath() const;
    Path* zeroLengthLinecapPath(const FloatPoint&) const;
    FloatRect zeroLengthSubpathRect(const FloatPoint&, float) const;
    void updateZeroLengthSubpaths();
    void strokeZeroLengthSubpaths(GraphicsContext&) const;

    bool shouldGenerateMarkerPositions() const;
    void drawMarkers(PaintInfo&) override;

    bool isRenderingDisabled() const override;

    Vector<FloatPoint> m_zeroLengthLinecapLocations;
    Vector<MarkerPosition> m_markerPositions;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGPath, isRenderSVGPath())

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
