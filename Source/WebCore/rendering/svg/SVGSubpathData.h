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
#include <wtf/StdLibExtras.h>

namespace WebCore {

class SVGSubpathData {
public:
    SVGSubpathData(Vector<FloatPoint>& zeroLengthSubpathLocations)
        : m_zeroLengthSubpathLocations(zeroLengthSubpathLocations)
    {
    }

    static void updateFromPathElement(SVGSubpathData& subpathFinder, const PathElement& element)
    {
        WTF::switchOn(element,
            [&](const PathMoveTo& moveTo) {
                if (subpathFinder.m_pathIsZeroLength && !subpathFinder.m_haveSeenMoveOnly)
                    subpathFinder.m_zeroLengthSubpathLocations.append(subpathFinder.m_lastPoint);
                subpathFinder.m_lastPoint = subpathFinder.m_movePoint = moveTo.point;
                subpathFinder.m_haveSeenMoveOnly = true;
                subpathFinder.m_pathIsZeroLength = true;
            },
            [&](const PathLineTo& lineTo) {
                if (subpathFinder.m_lastPoint != lineTo.point) {
                    subpathFinder.m_pathIsZeroLength = false;
                    subpathFinder.m_lastPoint = lineTo.point;
                }
                subpathFinder.m_haveSeenMoveOnly = false;
            },
            [&](const PathQuadCurveTo& quadTo) {
                if (subpathFinder.m_lastPoint != quadTo.controlPoint || quadTo.controlPoint != quadTo.endPoint) {
                    subpathFinder.m_pathIsZeroLength = false;
                    subpathFinder.m_lastPoint = quadTo.endPoint;
                }
                subpathFinder.m_haveSeenMoveOnly = false;
            },
            [&](const PathBezierCurveTo& bezierTo) {
                if (subpathFinder.m_lastPoint != bezierTo.controlPoint1 || bezierTo.controlPoint1 != bezierTo.controlPoint2 || bezierTo.controlPoint2 != bezierTo.endPoint) {
                    subpathFinder.m_pathIsZeroLength = false;
                    subpathFinder.m_lastPoint = bezierTo.endPoint;
                }
                subpathFinder.m_haveSeenMoveOnly = false;
            },
            [&](const PathCloseSubpath&) {
                if (subpathFinder.m_pathIsZeroLength)
                    subpathFinder.m_zeroLengthSubpathLocations.append(subpathFinder.m_lastPoint);
                subpathFinder.m_haveSeenMoveOnly = true; // This is an implicit move for the next element
                subpathFinder.m_pathIsZeroLength = true; // A new sub-path also starts here
                subpathFinder.m_lastPoint = subpathFinder.m_movePoint;
            });
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
