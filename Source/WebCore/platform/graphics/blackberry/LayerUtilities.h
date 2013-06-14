/*
 * Copyright (C) 2013 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef LayerUtilities_h
#define LayerUtilities_h

#if USE(ACCELERATED_COMPOSITING)

#include "FloatPoint.h"
#include "FloatPoint3D.h"
#include "FloatQuad.h"
#include "FloatSize.h"
#include "LayerCompositingThread.h"
#include "TransformationMatrix.h"

#include <algorithm>
#include <wtf/Vector.h>

namespace WebCore {

// Specifies a clip plane with normal n and containing point p_0
// as p * n + d = 0, d = -p_0 * n. The asterisk is dot product.
class LayerClipPlane {
public:
    LayerClipPlane(FloatPoint3D n, float d)
        : m_n(n)
        , m_d(d)
    {
    }

    inline bool isPointInside(const FloatPoint3D& p) const
    {
        return p * m_n + m_d > 0;
    }

    inline FloatPoint3D computeIntersection(const FloatPoint3D& p1, const FloatPoint3D& p2) const
    {
        float u = (-m_d - p1 * m_n) / ((p2 - p1) * m_n);
        return p1 + u * (p2 - p1);
    }

protected:
    FloatPoint3D m_n;
    float m_d;
};

// Sutherland - Hodgman, inner loop
template<typename Point, size_t inlineCapacity, typename ClipPrimitive>
inline Vector<Point, inlineCapacity> intersect(const Vector<Point, inlineCapacity>& inputList, const ClipPrimitive& clipPrimitive)
{
    Vector<Point, inlineCapacity> outputList;
    Point s;
    if (!inputList.isEmpty())
        s = inputList.last();
    for (typename Vector<Point, inlineCapacity>::const_iterator eIterator = inputList.begin(); eIterator != inputList.end(); ++eIterator) {
        const Point& e = *eIterator;
        if (clipPrimitive.isPointInside(e)) {
            if (!clipPrimitive.isPointInside(s))
                outputList.append(clipPrimitive.computeIntersection(s, e));
            outputList.append(e);
        } else if (clipPrimitive.isPointInside(s))
            outputList.append(clipPrimitive.computeIntersection(s, e));
        s = e;
    }
    return outputList;
}

template<size_t inlineCapacity>
inline FloatRect boundingBox(const Vector<FloatPoint, inlineCapacity>& points)
{
    if (points.isEmpty())
        return FloatRect();
    float xmin, xmax, ymin, ymax;
    xmin = ymin = std::numeric_limits<float>::infinity();
    xmax = ymax = -std::numeric_limits<float>::infinity();
    for (size_t i = 0; i < points.size(); ++i) {
        const FloatPoint& p = points[i];
        if (p.x() < xmin)
            xmin = p.x();
        if (p.x() > xmax)
            xmax = p.x();
        if (p.y() < ymin)
            ymin = p.y();
        if (p.y() > ymax)
            ymax = p.y();
    }
    return FloatRect(xmin, ymin, xmax - xmin, ymax - ymin);
}

inline FloatPoint3D multVecMatrix(const TransformationMatrix& matrix, const FloatPoint3D& p, float& w)
{
    FloatPoint3D result(
        matrix.m41() + p.x() * matrix.m11() + p.y() * matrix.m21() + p.z() * matrix.m31(),
        matrix.m42() + p.x() * matrix.m12() + p.y() * matrix.m22() + p.z() * matrix.m32(),
        matrix.m43() + p.x() * matrix.m13() + p.y() * matrix.m23() + p.z() * matrix.m33());
    w = matrix.m44() + p.x() * matrix.m14() + p.y() * matrix.m24() + p.z() * matrix.m34();
    return result;
}

template<typename Point, size_t inlineCapacity>
inline Vector<Point, inlineCapacity> toVector(const FloatQuad& quad)
{
    Vector<Point, inlineCapacity> result;
    result.append(quad.p1());
    result.append(quad.p2());
    result.append(quad.p3());
    result.append(quad.p4());
    return result;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif // LayerUtilities_h
