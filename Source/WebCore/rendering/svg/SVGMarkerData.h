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

    void updateFromPathElement(const PathElement& element)
    {
        // First update the outslope for the previous element.
        m_outslopePoints[0] = m_origin;
        m_outslopePoints[1] = WTF::switchOn(element,
            [](const PathMoveTo& moveTo) {
                return moveTo.point;
            },
            [](const PathLineTo& lineTo) {
                return lineTo.point;
            },
            [](const PathQuadCurveTo& quadTo) {
                return quadTo.controlPoint;
            },
            [](const PathBezierCurveTo& bezierTo) {
                return bezierTo.controlPoint1;
            },
            [](const PathCloseSubpath&) {
                return FloatPoint { };
            });
        // Record the marker for the previous element.
        if (m_elementIndex > 0) {
            SVGMarkerType markerType = m_elementIndex == 1 ? StartMarker : MidMarker;
            m_positions.append(MarkerPosition(markerType, m_origin, currentAngle(markerType)));
        }

        // Update our marker data for this element.
        updateMarkerDataForPathElement(element);
        ++m_elementIndex;
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
            if (std::abs(inAngle - outAngle) > 180)
                inAngle += 360;
            return narrowPrecisionToFloat((inAngle + outAngle) / 2);
        case EndMarker:
            return narrowPrecisionToFloat(inAngle);
        }

        ASSERT_NOT_REACHED();
        return 0;
    }

    void updateMarkerDataForPathElement(const PathElement& element)
    {
        WTF::switchOn(element,
            [&](const PathQuadCurveTo& quadTo) {
                // FIXME: https://bugs.webkit.org/show_bug.cgi?id=33115 (PathElement::Type::AddQuadCurveToPoint not handled for <marker>)
                m_origin = quadTo.endPoint;
            },
            [&](const PathBezierCurveTo& bezierTo) {
                m_inslopePoints[0] = bezierTo.controlPoint2;
                m_inslopePoints[1] = bezierTo.endPoint;
                m_origin = bezierTo.endPoint;
            },
            [&](const PathMoveTo& moveTo) {
                m_inslopePoints[0] = m_origin;
                m_inslopePoints[1] = moveTo.point;
                m_origin = moveTo.point;
                m_subpathStart = moveTo.point;
            },
            [&](const PathLineTo& lineTo) {
                m_inslopePoints[0] = m_origin;
                m_inslopePoints[1] = lineTo.point;
                m_origin = lineTo.point;
            },
            [&](const PathCloseSubpath&) {
                m_inslopePoints[0] = m_origin;
                m_inslopePoints[1] = { };
                m_origin = m_subpathStart;
                m_subpathStart = { };
            });
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
