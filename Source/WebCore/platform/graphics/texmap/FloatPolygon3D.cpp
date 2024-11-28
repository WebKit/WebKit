/*
 * Copyright (C) 2024 Jani Hautakangas <jani@kodegood.com>
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
#include "FloatPolygon3D.h"

#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FloatPolygon3D);

FloatPolygon3D::FloatPolygon3D(const FloatRect& rect, const TransformationMatrix& transform)
{
    m_vertices.append(transform.mapPoint(FloatPoint3D(rect.minXMinYCorner())));
    m_vertices.append(transform.mapPoint(FloatPoint3D(rect.maxXMinYCorner())));
    m_vertices.append(transform.mapPoint(FloatPoint3D(rect.maxXMaxYCorner())));
    m_vertices.append(transform.mapPoint(FloatPoint3D(rect.minXMaxYCorner())));

    if (auto inverse = transform.inverse())
        m_normal = inverse->transpose().mapPoint(m_normal);
    else {
        FloatPoint3D edge1(m_vertices[1].x() - m_vertices[0].x(), m_vertices[1].y() - m_vertices[0].y(), m_vertices[1].z() - m_vertices[0].z());
        FloatPoint3D edge2(m_vertices[2].x() - m_vertices[0].x(), m_vertices[2].y() - m_vertices[0].y(), m_vertices[2].z() - m_vertices[0].z());
        m_normal = edge1.cross(edge2);
    }
    m_normal.normalize();
}

FloatPolygon3D::FloatPolygon3D(Vector<FloatPoint3D>&& vertices, const FloatPoint3D& normal)
    : m_vertices(WTFMove(vertices))
    , m_normal(normal)
{
}

// Splits the polygon into two parts relative to the given plane.
// Algorithm:
// - For each edge of the polygon:
//   - Compute the signed distances of the edge's vertices from the plane.
//   - If both vertices are on the same side of the plane, add the starting vertex to the corresponding side's list.
//   - If the edge crosses the plane, compute the intersection point:
//     - t = di / (di - dj)
//     - intersectionPoint = vi + t * (vj - vi)
//     - Add the starting vertex and the intersection point to the appropriate lists.
// - Construct two new polygons from the collected vertices.
std::pair<FloatPolygon3D, FloatPolygon3D> FloatPolygon3D::split(const FloatPlane3D& plane) const
{
    Vector<FloatPoint3D> positiveVertices;
    Vector<FloatPoint3D> negativeVertices;

    const float epsilon = std::numeric_limits<float>::epsilon(); // Tolerance for floating point comparisons

    unsigned numberOfVertices = m_vertices.size();
    for (unsigned i = 0; i < numberOfVertices; ++i) {
        const FloatPoint3D& vi = m_vertices[i];
        const FloatPoint3D& vj = m_vertices[(i + 1) % numberOfVertices];
        float di = plane.distanceToPoint(vi);
        float dj = plane.distanceToPoint(vj);

        bool viPos = di > epsilon;
        bool viNeg = di < -epsilon;

        if (viPos) {
            positiveVertices.append(vi);

            if (dj < -epsilon) {
                // Edge crosses from positive to negative
                float t = di / (di - dj);
                FloatPoint3D intersectionPoint = vi + (vj - vi) * t;
                positiveVertices.append(intersectionPoint);
                negativeVertices.append(intersectionPoint);
            }

        } else if (viNeg) {
            negativeVertices.append(vi);

            if (dj > epsilon) {
                // Edge crosses from negative to positive
                float t = di / (di - dj);
                FloatPoint3D intersectionPoint = vi + (vj - vi) * t;
                negativeVertices.append(intersectionPoint);
                positiveVertices.append(intersectionPoint);
            }

        } else { // vi is approximately on the plane
            positiveVertices.append(vi);
            negativeVertices.append(vi);
        }
    }

    // Create new polygons for each side
    return { { WTFMove(negativeVertices), m_normal }, { WTFMove(positiveVertices), m_normal } };
}

} // namespace WebCore
