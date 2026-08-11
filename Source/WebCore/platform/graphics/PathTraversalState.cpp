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
#include "PathTraversalState.h"

#include "PathCurveSubdivision.h"
#include <wtf/MathExtras.h>

namespace WebCore {

// FIXME: A possible speed-up would be to check up front whether approximateDistance() plus the
// current total distance already exceeds the desired distance, and skip subdividing when it does not.
template<class CurveType>
static float curveLength(const PathTraversalState& traversalState, const CurveType& originalCurve, FloatPoint& previous, FloatPoint& current)
{
    bool isVectorAtLength = traversalState.action() == PathTraversalState::Action::VectorAtLength;
    float totalLength = 0;

    forEachFlattenedCurveLeaf(originalCurve, [&](const CurveType& curve, float length, bool isLastLeaf) {
        totalLength += length;
        previous = curve.start;
        current = curve.end;
        ASSERT_UNUSED(isLastLeaf, !isLastLeaf || curve.end == originalCurve.end);
        return !isVectorAtLength || traversalState.totalLength() + totalLength <= traversalState.desiredLength();
    });

    return totalLength;
}

PathTraversalState::PathTraversalState(Action action, float desiredLength)
    : m_action(action)
    , m_desiredLength(desiredLength)
{
    ASSERT(action != Action::TotalLength || !desiredLength);
}

void PathTraversalState::closeSubpath()
{
    m_totalLength += distanceLine(m_current, m_start);
    m_current = m_start;
}

void PathTraversalState::moveTo(const FloatPoint& point)
{
    m_previous = m_current = m_start = point;
}

void PathTraversalState::lineTo(const FloatPoint& point)
{
    m_totalLength += distanceLine(m_current, point);
    m_current = point;
}

void PathTraversalState::quadraticBezierTo(const FloatPoint& newControl, const FloatPoint& newEnd)
{
    m_totalLength += curveLength<QuadraticBezier>(*this, QuadraticBezier(m_current, newControl, newEnd), m_previous, m_current);
}

void PathTraversalState::cubicBezierTo(const FloatPoint& newControl1, const FloatPoint& newControl2, const FloatPoint& newEnd)
{
    m_totalLength += curveLength<CubicBezier>(*this, CubicBezier(m_current, newControl1, newControl2, newEnd), m_previous, m_current);
}

bool PathTraversalState::finalizeAppendPathElement()
{
    if (m_action == Action::TotalLength)
        return false;

    if (m_action == Action::SegmentAtLength) {
        if (m_totalLength >= m_desiredLength)
            m_success = true;
        return m_success;
    }

    ASSERT(m_action == Action::VectorAtLength);

    if (m_totalLength >= m_desiredLength) {
        float slope = FloatPoint(m_current - m_previous).slopeAngleRadians();
        float offset = m_desiredLength - m_totalLength;
        m_current.move(offset * cosf(slope), offset * sinf(slope));

        if (!m_isZeroVector && !m_desiredLength)
            m_isZeroVector = true;
        else {
            m_success = true;
            m_normalAngle = rad2deg(slope);
        }
    }

    m_previous = m_current;
    return m_success;
}

bool PathTraversalState::appendPathElement(PathElement::Type type, std::span<const FloatPoint> points)
{
    switch (type) {
    case PathElement::Type::MoveToPoint:
        moveTo(points[0]);
        break;
    case PathElement::Type::AddLineToPoint:
        lineTo(points[0]);
        break;
    case PathElement::Type::AddQuadCurveToPoint:
        quadraticBezierTo(points[0], points[1]);
        break;
    case PathElement::Type::AddCurveToPoint:
        cubicBezierTo(points[0], points[1], points[2]);
        break;
    case PathElement::Type::CloseSubpath:
        closeSubpath();
        break;
    }
    
    return finalizeAppendPathElement();
}

bool PathTraversalState::processPathElement(PathElement::Type type, std::span<const FloatPoint> points)
{
    if (m_success)
        return true;

    if (m_isZeroVector) {
        PathTraversalState traversalState(*this);
        m_success = traversalState.appendPathElement(type, points);
        m_normalAngle = traversalState.m_normalAngle;
        return m_success;
    }

    return appendPathElement(type, points);
}

}
