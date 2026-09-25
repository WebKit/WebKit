/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Polygon4D.h"

#include "FloatRect.h"
#include "TransformationMatrix.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <span>

namespace WebCore {

static std::pair<Polygon4D::Vertices, Polygon4D::Vertices> splitPolygon(std::span<const Point4D> polygon, const Point4D& plane)
{
    // Splits a convex polygon along the plane into the parts on its negative and positive side, as when
    // building a BSP tree. The signed distance is linear in the undivided coordinates, so an edge crosses
    // the plane where it interpolates to zero, and the tolerance scales with w.
    // See https://en.wikipedia.org/wiki/Binary_space_partitioning.

    Polygon4D::Vertices negativeVertices;
    Polygon4D::Vertices positiveVertices;

    ASSERT(!polygon.empty());
    const double firstDistance = signedDistanceToPlane(plane, polygon[0]);
    double currentDistance = firstDistance;

    for (size_t i = 0; i < polygon.size(); ++i) {
        const bool isLast = i + 1 == polygon.size();
        const auto& current = polygon[i];
        const auto& next = isLast ? polygon[0] : polygon[i + 1];
        const double nextDistance = isLast ? firstDistance : signedDistanceToPlane(plane, next);

        const double currentTolerance = Polygon4D::planeTolerance * current.w;
        const double nextTolerance = Polygon4D::planeTolerance * next.w;

        // Unlike cur + t * (next - cur), this yields exactly w = 0 on the camera plane.
        auto intersectEdgeWithPlane = [&] {
            auto point = (currentDistance * next - nextDistance * current) / (currentDistance - nextDistance);
            point.w = std::abs(point.w);
            return point;
        };

        if (currentDistance > currentTolerance) {
            positiveVertices.append(current);
            if (nextDistance < -nextTolerance) {
                auto point = intersectEdgeWithPlane();
                positiveVertices.append(point);
                negativeVertices.append(point);
            }
        } else if (currentDistance < -currentTolerance) {
            negativeVertices.append(current);
            if (nextDistance > nextTolerance) {
                auto point = intersectEdgeWithPlane();
                negativeVertices.append(point);
                positiveVertices.append(point);
            }
        } else {
            positiveVertices.append(current);
            negativeVertices.append(current);
        }

        currentDistance = nextDistance;
    }

    return { WTF::move(negativeVertices), WTF::move(positiveVertices) };
}

auto Polygon4D::clipToFrontOfCamera(const FloatRect& rect, const TransformationMatrix& transform) -> Vertices
{
    auto mapCorner = [&](const FloatPoint& corner) {
        Point4D point { corner.x(), corner.y(), 0, 1 };
        transform.map4ComponentPoint(point.x, point.y, point.z, point.w);
        return point;
    };

    const std::array<Point4D, 4> corners {
        mapCorner(rect.minXMinYCorner()),
        mapCorner(rect.maxXMinYCorner()),
        mapCorner(rect.maxXMaxYCorner()),
        mapCorner(rect.minXMaxYCorner())
    };

    if (std::ranges::all_of(corners, [](const auto& corner) { return corner.w > 0; }))
        return { std::span { corners } };

    return splitPolygon(corners, { 0, 0, 0, 1 }).second;
}

auto Polygon4D::clipToPlane(std::span<const Point4D> vertices, const Point4D& plane) -> Vertices
{
    if (vertices.empty())
        return { };

    return splitPolygon(vertices, plane).second;
}

Polygon4D::Polygon4D(const FloatRect& rect, const TransformationMatrix& transform)
    : m_vertices(clipToFrontOfCamera(rect, transform))
{
    if (m_vertices.size() < 3) {
        m_vertices.clear();
        return;
    }

    // The layer lies in the plane z = 0. The transform maps it to the plane through the mapped x axis,
    // y axis and origin, which are rows 1, 2 and 4 of the transform. The plane coefficients are the
    // cross product of these rows, which is the third column of the adjugate. Unlike the inverse, the
    // adjugate exists for every transform. See https://en.wikipedia.org/wiki/Adjugate_matrix.
    auto determinant = [](double a1, double a2, double a3, double b1, double b2, double b3, double c1, double c2, double c3) {
        return a1 * (b2 * c3 - b3 * c2) - b1 * (a2 * c3 - a3 * c2) + c1 * (a2 * b3 - a3 * b2);
    };
    const Point4D a { transform.m11(), transform.m12(), transform.m13(), transform.m14() };
    const Point4D b { transform.m21(), transform.m22(), transform.m23(), transform.m24() };
    const Point4D c { transform.m41(), transform.m42(), transform.m43(), transform.m44() };
    m_plane = {
        determinant(a.y, a.z, a.w, b.y, b.z, b.w, c.y, c.z, c.w),
        -determinant(a.x, a.z, a.w, b.x, b.z, b.w, c.x, c.z, c.w),
        determinant(a.x, a.y, a.w, b.x, b.y, b.w, c.x, c.y, c.w),
        -determinant(a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z)
    };

    const double length = std::hypot(m_plane.x, m_plane.y, m_plane.z);
    if (!length) {
        m_vertices.clear();
        return;
    }

    // The adjugate is the inverse times the determinant. Flip the plane when the determinant is
    // negative, so that its positive side stays the front of the layer, where its z axis points.
    // The determinant is the signed distance of row 3, the mapped z axis, to the plane.
    const Point4D zAxis { transform.m31(), transform.m32(), transform.m33(), transform.m34() };
    m_plane = ((signedDistanceToPlane(m_plane, zAxis) < 0 ? -1 : 1) / length) * m_plane;
}

Polygon4D::Polygon4D(Vertices&& vertices, const Point4D& plane)
    : m_vertices(WTF::move(vertices))
    , m_plane(plane)
{
}

std::pair<Polygon4D, Polygon4D> Polygon4D::split(const Point4D& plane) const
{
    auto [negativeVertices, positiveVertices] = splitPolygon(m_vertices, plane);
    return { { WTF::move(negativeVertices), m_plane }, { WTF::move(positiveVertices), m_plane } };
}

} // namespace WebCore
