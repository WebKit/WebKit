/*
 * Copyright (C) 2012 Google, Inc.
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

#include "Path.h"
#include <wtf/Forward.h>

namespace WebCore {

class SVGSubpathData {
public:
    SVGSubpathData(Vector<FloatPoint>& zeroLengthSubpathLocations)
        : m_zeroLengthSubpathLocations(zeroLengthSubpathLocations)
    {
    }

    static void updateFromPathElement(SVGSubpathData& subpathFinder, const PathElement& element)
    {
        switch (element.type) {
        case PathElement::Type::MoveToPoint:
            if (subpathFinder.m_pathIsZeroLength && !subpathFinder.m_haveSeenMoveOnly)
                subpathFinder.m_zeroLengthSubpathLocations.append(subpathFinder.m_lastPoint);
            subpathFinder.m_lastPoint = subpathFinder.m_movePoint = element.points[0];
            subpathFinder.m_haveSeenMoveOnly = true;
            subpathFinder.m_pathIsZeroLength = true;
            break;
        case PathElement::Type::AddLineToPoint:
            if (subpathFinder.m_lastPoint != element.points[0]) {
                subpathFinder.m_pathIsZeroLength = false;
                subpathFinder.m_lastPoint = element.points[0];
            }
            subpathFinder.m_haveSeenMoveOnly = false;
            break;
        case PathElement::Type::AddQuadCurveToPoint:
            if (subpathFinder.m_lastPoint != element.points[0] || element.points[0] != element.points[1]) {
                subpathFinder.m_pathIsZeroLength = false;
                subpathFinder.m_lastPoint = element.points[1];
            }
            subpathFinder.m_haveSeenMoveOnly = false;
            break;
        case PathElement::Type::AddCurveToPoint:
            if (subpathFinder.m_lastPoint != element.points[0] || element.points[0] != element.points[1] || element.points[1] != element.points[2]) {
                subpathFinder.m_pathIsZeroLength = false;
                subpathFinder.m_lastPoint = element.points[2];
            }
            subpathFinder.m_haveSeenMoveOnly = false;
            break;
        case PathElement::Type::CloseSubpath:
            if (subpathFinder.m_pathIsZeroLength)
                subpathFinder.m_zeroLengthSubpathLocations.append(subpathFinder.m_lastPoint);
            subpathFinder.m_haveSeenMoveOnly = true; // This is an implicit move for the next element
            subpathFinder.m_pathIsZeroLength = true; // A new sub-path also starts here
            subpathFinder.m_lastPoint = subpathFinder.m_movePoint;
            break;
        }
    }

    void pathIsDone()
    {
        if (m_pathIsZeroLength && !m_haveSeenMoveOnly)
            m_zeroLengthSubpathLocations.append(m_lastPoint);
    }

private:
    Vector<FloatPoint>& m_zeroLengthSubpathLocations;
    FloatPoint m_lastPoint;
    FloatPoint m_movePoint;
    bool m_haveSeenMoveOnly { false };
    bool m_pathIsZeroLength { false };
};

} // namespace WebCore
