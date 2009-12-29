/*
    Copyright (C) Research In Motion Limited 2009. All rights reserved.
                  2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2008 Rob Buis <buis@kde.org>
                  2005, 2007 Eric Seidel <eric@webkit.org>
                  2009 Google, Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "SVGMarkerLayoutInfo.h"

#include "FloatConversion.h"
#include "SVGResourceMarker.h"

namespace WebCore {

SVGMarkerLayoutInfo::SVGMarkerLayoutInfo()
    : m_startMarker(0)
    , m_midMarker(0)
    , m_endMarker(0)
    , m_elementIndex(0)
    , m_markerData(MarkerData::Unknown, 0)
    , m_strokeWidth(0)
{
}

SVGMarkerLayoutInfo::~SVGMarkerLayoutInfo()
{
}

void SVGMarkerLayoutInfo::initialize(SVGResourceMarker* startMarker, SVGResourceMarker* midMarker, SVGResourceMarker* endMarker, float strokeWidth)
{
    m_startMarker = startMarker;
    m_midMarker = midMarker;
    m_endMarker = endMarker;
    m_strokeWidth = strokeWidth;
}

static inline void updateMarkerDataForElement(MarkerData& previousMarkerData, const PathElement* element)
{
    FloatPoint* points = element->points;
    
    switch (element->type) {
    case PathElementAddQuadCurveToPoint:
        // TODO
        previousMarkerData.origin = points[1];
        break;
    case PathElementAddCurveToPoint:
        previousMarkerData.inslopePoints[0] = points[1];
        previousMarkerData.inslopePoints[1] = points[2];
        previousMarkerData.origin = points[2];
        break;
    case PathElementMoveToPoint:
        previousMarkerData.subpathStart = points[0];
    case PathElementAddLineToPoint:
        previousMarkerData.inslopePoints[0] = previousMarkerData.origin;
        previousMarkerData.inslopePoints[1] = points[0];
        previousMarkerData.origin = points[0];
        break;
    case PathElementCloseSubpath:
        previousMarkerData.inslopePoints[0] = previousMarkerData.origin;
        previousMarkerData.inslopePoints[1] = points[0];
        previousMarkerData.origin = previousMarkerData.subpathStart;
        previousMarkerData.subpathStart = FloatPoint();
    }
}

void recordMarkerData(SVGMarkerLayoutInfo& info)
{
    MarkerData& data = info.m_markerData;
    if (!data.marker)
        return;

    FloatPoint inslopeChange = data.inslopePoints[1] - FloatSize(data.inslopePoints[0].x(), data.inslopePoints[0].y());
    FloatPoint outslopeChange = data.outslopePoints[1] - FloatSize(data.outslopePoints[0].x(), data.outslopePoints[0].y());

    double inslope = rad2deg(atan2(inslopeChange.y(), inslopeChange.x()));
    double outslope = rad2deg(atan2(outslopeChange.y(), outslopeChange.x()));

    double angle = 0.0;
    switch (data.type) {
    case MarkerData::Start:
        angle = outslope;
        break;
    case MarkerData::Mid:
        angle = (inslope + outslope) / 2;
        break;
    case MarkerData::End:
        angle = inslope;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    TransformationMatrix transform = data.marker->markerTransformation(data.origin, narrowPrecisionToFloat(angle), info.m_strokeWidth);
    info.m_layout.append(MarkerLayout(data.marker, transform));
}

void processStartAndMidMarkers(void* info, const PathElement* element)
{
    SVGMarkerLayoutInfo& data = *reinterpret_cast<SVGMarkerLayoutInfo*>(info);
    FloatPoint* points = element->points;

    // First update the outslope for the previous element
    data.m_markerData.outslopePoints[0] = data.m_markerData.origin;
    data.m_markerData.outslopePoints[1] = points[0];

    // Draw the marker for the previous element
    if (data.m_elementIndex != 0)
        recordMarkerData(data);

    // Update our marker data for this element
    updateMarkerDataForElement(data.m_markerData, element);

    if (data.m_elementIndex == 1) {
        // After drawing the start marker, switch to drawing mid markers
        data.m_markerData.marker = data.m_midMarker;
        data.m_markerData.type = MarkerData::Mid;
    }

    ++data.m_elementIndex;
}

FloatRect SVGMarkerLayoutInfo::calculateBoundaries(const Path& path)
{
    m_layout.clear();
    m_elementIndex = 0;
    m_markerData = MarkerData(MarkerData::Start, m_startMarker);
    path.apply(this, processStartAndMidMarkers);

    m_markerData.marker = m_endMarker;
    m_markerData.type = MarkerData::End;
    recordMarkerData(*this);

    if (m_layout.isEmpty())
        return FloatRect();

    FloatRect bounds;

    if (m_startMarker)
        bounds.unite(m_startMarker->markerBoundaries());

    if (m_midMarker)
        bounds.unite(m_midMarker->markerBoundaries());

    if (m_endMarker)
        bounds.unite(m_endMarker->markerBoundaries());

    return bounds;
}

void SVGMarkerLayoutInfo::drawMarkers(RenderObject::PaintInfo& paintInfo)
{
    if (m_layout.isEmpty())
        return;

    Vector<MarkerLayout>::iterator it = m_layout.begin();
    Vector<MarkerLayout>::iterator end = m_layout.end();

    for (; it != end; ++it) {
        MarkerLayout& layout = *it;
        layout.marker->draw(paintInfo, layout.matrix);
    }
}

}

#endif // ENABLE(SVG)
