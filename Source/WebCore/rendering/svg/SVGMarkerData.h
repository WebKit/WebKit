/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "FloatConversion.h"
#include "Path.h"

namespace WebCore {

class RenderSVGResourceMarker;

enum SVGMarkerType {
    StartMarker,
    MidMarker,
    EndMarker
};

struct MarkerPosition {
    MarkerPosition(SVGMarkerType useType, const FloatPoint& useOrigin, float useAngle)
        : type(useType)
        , origin(useOrigin)
        , angle(useAngle)
    {
    }

    SVGMarkerType type;
    FloatPoint origin;
    float angle;
};

class SVGMarkerData {
public:
    SVGMarkerData(Vector<MarkerPosition>& positions, bool reverseStart)
        : m_positions(positions)
        , m_elementIndex(0)
        , m_reverseStart(reverseStart)
    {
    }

    static void updateFromPathElement(SVGMarkerData& markerData, const PathElement& element)
    {
        // First update the outslope for the previous element.
        markerData.updateOutslope(element.points[0]);

        // Record the marker for the previous element.
        if (markerData.m_elementIndex > 0) {
            SVGMarkerType markerType = markerData.m_elementIndex == 1 ? StartMarker : MidMarker;
            markerData.m_positions.append(MarkerPosition(markerType, markerData.m_origin, markerData.currentAngle(markerType)));
        }

        // Update our marker data for this element.
        markerData.updateMarkerDataForPathElement(element);
        ++markerData.m_elementIndex;
    }

    void pathIsDone()
    {
        m_positions.append(MarkerPosition(EndMarker, m_origin, currentAngle(EndMarker)));
    }

private:
    float currentAngle(SVGMarkerType type) const
    {
        // For details of this calculation, see: http://www.w3.org/TR/SVG/single-page.html#painting-MarkerElement
        FloatPoint inSlope(m_inslopePoints[1] - m_inslopePoints[0]);
        FloatPoint outSlope(m_outslopePoints[1] - m_outslopePoints[0]);

        double inAngle = rad2deg(inSlope.slopeAngleRadians());
        double outAngle = rad2deg(outSlope.slopeAngleRadians());

        switch (type) {
        case StartMarker:
            if (m_reverseStart)
                return narrowPrecisionToFloat(outAngle - 180);
            return narrowPrecisionToFloat(outAngle);
        case MidMarker:
            // WK193015: Prevent bugs due to angles being non-continuous.
            if (fabs(inAngle - outAngle) > 180)
                inAngle += 360;
            return narrowPrecisionToFloat((inAngle + outAngle) / 2);
        case EndMarker:
            return narrowPrecisionToFloat(inAngle);
        }

        ASSERT_NOT_REACHED();
        return 0;
    }

    void updateOutslope(const FloatPoint& point)
    {
        m_outslopePoints[0] = m_origin;
        m_outslopePoints[1] = point;
    }

    void updateMarkerDataForPathElement(const PathElement& element)
    {
        auto& points = element.points;

        switch (element.type) {
        case PathElement::Type::AddQuadCurveToPoint:
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=33115 (PathElement::Type::AddQuadCurveToPoint not handled for <marker>)
            m_origin = points[1];
            break;
        case PathElement::Type::AddCurveToPoint:
            m_inslopePoints[0] = points[1];
            m_inslopePoints[1] = points[2];
            m_origin = points[2];
            break;
        case PathElement::Type::MoveToPoint:
            m_subpathStart = points[0];
            FALLTHROUGH;
        case PathElement::Type::AddLineToPoint:
            updateInslope(points[0]);
            m_origin = points[0];
            break;
        case PathElement::Type::CloseSubpath:
            updateInslope(points[0]);
            m_origin = m_subpathStart;
            m_subpathStart = FloatPoint();
        }
    }

    void updateInslope(const FloatPoint& point)
    {
        m_inslopePoints[0] = m_origin;
        m_inslopePoints[1] = point;
    }

    Vector<MarkerPosition>& m_positions;
    unsigned m_elementIndex;
    FloatPoint m_origin;
    FloatPoint m_subpathStart;
    FloatPoint m_inslopePoints[2];
    FloatPoint m_outslopePoints[2];
    bool m_reverseStart;
};

} // namespace WebCore
