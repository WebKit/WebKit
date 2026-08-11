/*
 * Copyright (C) 2006, 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "PathArcLengthMapper.h"

#include "PathCurveSubdivision.h"
#include <wtf/MathExtras.h>

namespace WebCore {

void PathArcLengthMapper::addVertex(const FloatPoint& point, float segmentLength)
{
    m_totalLength += segmentLength;
    m_vertices.append({ point, m_totalLength });
    m_current = point;
}

void PathArcLengthMapper::appendPathElement(PathElement::Type type, std::span<const FloatPoint> points)
{
    // Record each leaf endpoint with its control-polygon length, matching how curveLength() accumulates.
    auto appendCurveLeaves = [&](const auto& curve) {
        forEachFlattenedCurveLeaf(curve, [&](const auto& leaf, float length, bool) {
            addVertex(leaf.end, length);
            return true;
        });
    };

    switch (type) {
    case PathElement::Type::MoveToPoint:
        // Zero-length vertex so interpolation never spans the gap between subpaths.
        m_subpathStart = points[0];
        addVertex(points[0], 0);
        break;
    case PathElement::Type::AddLineToPoint:
        addVertex(points[0], distanceLine(m_current, points[0]));
        break;
    case PathElement::Type::AddQuadCurveToPoint:
        appendCurveLeaves(QuadraticBezier(m_current, points[0], points[1]));
        break;
    case PathElement::Type::AddCurveToPoint:
        appendCurveLeaves(CubicBezier(m_current, points[0], points[1], points[2]));
        break;
    case PathElement::Type::CloseSubpath:
        addVertex(m_subpathStart, distanceLine(m_current, m_subpathStart));
        break;
    }
}

auto PathArcLengthMapper::positionAtLength(float length) const -> Position
{
    if (m_vertices.isEmpty())
        return { };
    if (m_vertices.size() == 1)
        return { m_vertices[0].point, 0 };

    length = clampTo<float>(length, 0, m_totalLength);

    // Smallest vertex index (>= 1) whose accumulated length reaches `length`.
    size_t low = 1;
    size_t high = m_vertices.size() - 1;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (m_vertices[mid].accumulatedLength < length)
            low = mid + 1;
        else
            high = mid;
    }

    const auto& end = m_vertices[low];
    const auto& start = m_vertices[low - 1];

    float angleInRadians = FloatPoint(end.point - start.point).slopeAngleRadians();
    float offset = length - end.accumulatedLength; // <= 0: step back from the leaf's end point.

    FloatPoint point = end.point;
    point.move(offset * cosf(angleInRadians), offset * sinf(angleInRadians));

    return { point, rad2deg(angleInRadians) };
}

} // namespace WebCore
