/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2005, 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2009 Jeff Schiller <codedread@gmail.com>
 * Copyright (C) 2011 Renata Hodovan <reni@webkit.org>
 * Copyright (C) 2011 University of Szeged
 * Copyright (C) 2020, 2021, 2022, 2023, 2024 Igalia S.L.
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

#include "config.h"
#include "RenderSVGPath.h"

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "Gradient.h"
#include "ReferencedSVGResources.h"
#include "RenderLayer.h"
#include "RenderSVGResourceMarkerInlines.h"
#include "RenderSVGShapeInlines.h"
#include "RenderStyleInlines.h"
#include "SVGMarkerElement.h"
#include "SVGPathElement.h"
#include "SVGSubpathData.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGPath);

RenderSVGPath::RenderSVGPath(SVGGraphicsElement& element, RenderStyle&& style)
    : RenderSVGShape(Type::SVGPath, element, WTFMove(style))
{
    ASSERT(isRenderSVGPath());
}

RenderSVGPath::~RenderSVGPath() = default;

void RenderSVGPath::updateShapeFromElement()
{
    clearPath();
    m_shapeType = ShapeType::Empty;
    m_fillBoundingBox = ensurePath().boundingRect();
    m_strokeBoundingBox = std::nullopt;
    m_approximateStrokeBoundingBox = std::nullopt;
    updateMarkerPositions();
    updateZeroLengthSubpaths();

    ASSERT(hasPath());
    if (path().isEmpty())
        return;
    if (path().definitelySingleLine())
        m_shapeType = ShapeType::Line;
    else
        m_shapeType = ShapeType::Path;
}

FloatRect RenderSVGPath::adjustStrokeBoundingBoxForZeroLengthLinecaps(RepaintRectCalculation, FloatRect strokeBoundingBox) const
{
    if (style().svgStyle().hasStroke()) {
        // FIXME: zero-length subpaths do not respect vector-effect = non-scaling-stroke.
        float strokeWidth = this->strokeWidth();
        for (size_t i = 0; i < m_zeroLengthLinecapLocations.size(); ++i)
            strokeBoundingBox.unite(zeroLengthSubpathRect(m_zeroLengthLinecapLocations[i], strokeWidth));
    }

    return strokeBoundingBox;
}

static void useStrokeStyleToFill(GraphicsContext& context)
{
    if (auto gradient = context.strokeGradient())
        context.setFillGradient(*gradient, context.strokeGradientSpaceTransform());
    else if (Pattern* pattern = context.strokePattern())
        context.setFillPattern(*pattern);
    else
        context.setFillColor(context.strokeColor());
}

void RenderSVGPath::strokeShape(GraphicsContext& context) const
{
    if (!style().hasVisibleStroke())
        return;

    // This happens only if the layout was never been called for this element.
    if (!hasPath())
        return;

    RenderSVGShape::strokeShape(context);
    strokeZeroLengthSubpaths(context);
}

bool RenderSVGPath::shapeDependentStrokeContains(const FloatPoint& point, PointCoordinateSpace pointCoordinateSpace)
{
    if (RenderSVGShape::shapeDependentStrokeContains(point, pointCoordinateSpace))
        return true;

    for (size_t i = 0; i < m_zeroLengthLinecapLocations.size(); ++i) {
        ASSERT(style().svgStyle().hasStroke());
        float strokeWidth = this->strokeWidth();
        if (style().capStyle() == LineCap::Square) {
            if (zeroLengthSubpathRect(m_zeroLengthLinecapLocations[i], strokeWidth).contains(point))
                return true;
        } else {
            ASSERT(style().capStyle() == LineCap::Round);
            FloatPoint radiusVector(point.x() - m_zeroLengthLinecapLocations[i].x(), point.y() -  m_zeroLengthLinecapLocations[i].y());
            if (radiusVector.lengthSquared() < strokeWidth * strokeWidth * .25f)
                return true;
        }
    }
    return false;
}

bool RenderSVGPath::shouldStrokeZeroLengthSubpath() const
{
    // Spec(11.4): Any zero length subpath shall not be stroked if the "stroke-linecap" property has a value of butt
    // but shall be stroked if the "stroke-linecap" property has a value of round or square
    return style().svgStyle().hasStroke() && style().capStyle() != LineCap::Butt;
}

Path* RenderSVGPath::zeroLengthLinecapPath(const FloatPoint& linecapPosition) const
{
    static NeverDestroyed<Path> tempPath;

    tempPath.get().clear();
    if (style().capStyle() == LineCap::Square)
        tempPath.get().addRect(zeroLengthSubpathRect(linecapPosition, this->strokeWidth()));
    else
        tempPath.get().addEllipseInRect(zeroLengthSubpathRect(linecapPosition, this->strokeWidth()));

    return &tempPath.get();
}

FloatRect RenderSVGPath::zeroLengthSubpathRect(const FloatPoint& linecapPosition, float strokeWidth) const
{
    return FloatRect(linecapPosition.x() - strokeWidth / 2, linecapPosition.y() - strokeWidth / 2, strokeWidth, strokeWidth);
}

void RenderSVGPath::updateZeroLengthSubpaths()
{
    m_zeroLengthLinecapLocations.clear();

    if (!strokeWidth() || !shouldStrokeZeroLengthSubpath())
        return;

    SVGSubpathData subpathData(m_zeroLengthLinecapLocations);
    path().applyElements([&subpathData](const PathElement& pathElement) {
        SVGSubpathData::updateFromPathElement(subpathData, pathElement);
    });
    subpathData.pathIsDone();
}

void RenderSVGPath::strokeZeroLengthSubpaths(GraphicsContext& context) const
{
    if (m_zeroLengthLinecapLocations.isEmpty())
        return;

    AffineTransform nonScalingTransform;
    if (hasNonScalingStroke())
        nonScalingTransform = nonScalingStrokeTransform();

    GraphicsContextStateSaver stateSaver(context, true);
    useStrokeStyleToFill(context);
    for (size_t i = 0; i < m_zeroLengthLinecapLocations.size(); ++i) {
        auto usePath = zeroLengthLinecapPath(m_zeroLengthLinecapLocations[i]);
        if (hasNonScalingStroke())
            usePath = nonScalingStrokePath(usePath, nonScalingTransform);
        context.fillPath(*usePath);
    }
}

static inline RenderSVGResourceMarker* markerForType(SVGMarkerType type, RenderSVGResourceMarker* markerStart, RenderSVGResourceMarker* markerMid, RenderSVGResourceMarker* markerEnd)
{
    switch (type) {
    case StartMarker:
        return markerStart;
    case MidMarker:
        return markerMid;
    case EndMarker:
        return markerEnd;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

bool RenderSVGPath::shouldGenerateMarkerPositions() const
{
    if (style().svgStyle().hasMarkers() && graphicsElement().supportsMarkers())
        return svgMarkerStartResourceFromStyle() || svgMarkerMidResourceFromStyle() || svgMarkerEndResourceFromStyle();
    return false;
}

void RenderSVGPath::drawMarkers(PaintInfo& paintInfo)
{
    if (m_markerPositions.isEmpty())
        return;

    if (SVGHitTestCycleDetectionScope::isVisiting(*this))
        return;

    SVGHitTestCycleDetectionScope paintScope(*this);

    auto* markerStart = svgMarkerStartResourceFromStyle();
    auto* markerMid = svgMarkerMidResourceFromStyle();
    auto* markerEnd = svgMarkerEndResourceFromStyle();
    if (!markerStart && !markerMid && !markerEnd)
        return;

    float strokeWidth = this->strokeWidth();
    for (auto& markerPosition : m_markerPositions) {
        if (auto* marker = markerForType(markerPosition.type, markerStart, markerMid, markerEnd); marker && marker->hasLayer()) {
            auto& context = paintInfo.context();
            GraphicsContextStateSaver stateSaver(context);

            auto contentTransform = marker->markerTransformation(markerPosition.origin, markerPosition.angle, strokeWidth);
            marker->layer()->paintSVGResourceLayer(context, contentTransform);
        }
    }
}

FloatRect RenderSVGPath::computeMarkerBoundingBox(const SVGBoundingBoxComputation::DecorationOptions& options) const
{
    if (m_markerPositions.isEmpty())
        return { };

    if (SVGHitTestCycleDetectionScope::isVisiting(*this))
        return { };

    SVGHitTestCycleDetectionScope queryScope(*this);

    auto* markerStart = svgMarkerStartResourceFromStyle();
    auto* markerMid = svgMarkerMidResourceFromStyle();
    auto* markerEnd = svgMarkerEndResourceFromStyle();
    if (!markerStart && !markerMid && !markerEnd)
        return { };

    FloatRect boundaries;
    for (auto& markerPosition : m_markerPositions) {
        if (auto* marker = markerForType(markerPosition.type, markerStart, markerMid, markerEnd))
            boundaries.unite(marker->computeMarkerBoundingBox(options, marker->markerTransformation(markerPosition.origin, markerPosition.angle, strokeWidth())));
    }

    return boundaries;
}

void RenderSVGPath::updateMarkerPositions()
{
    m_markerPositions.clear();

    if (!shouldGenerateMarkerPositions())
        return;

    ASSERT(hasPath());
    auto* markerStart = svgMarkerStartResourceFromStyle();

    SVGMarkerData markerData(m_markerPositions, markerStart ? markerStart->hasReverseStart() : false);
    path().applyElements([&markerData](const PathElement& pathElement) {
        SVGMarkerData::updateFromPathElement(markerData, pathElement);
    });
    markerData.pathIsDone();
}

bool RenderSVGPath::isRenderingDisabled() const
{
    // For a polygon, polyline or path, rendering is disabled if there is no path data.
    // No path data is possible in the case of a missing or empty 'd' or 'points' attribute.
    return !hasPath() || path().isEmpty();
}

}

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
