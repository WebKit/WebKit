/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "cc/CCMathUtil.h"

#include "FloatPoint.h"
#include "FloatQuad.h"
#include "IntRect.h"
#include "TransformationMatrix.h"

namespace WebCore {

struct HomogeneousCoordinate {
    HomogeneousCoordinate(double newX, double newY, double newZ, double newW)
        : x(newX)
        , y(newY)
        , z(newZ)
        , w(newW)
    {
    }

    bool shouldBeClipped() const
    {
        return w <= 0;
    }

    FloatPoint cartesianPoint2d() const
    {
        if (w == 1)
            return FloatPoint(x, y);

        // For now, because this code is used privately only by CCMathUtil, it should never be called when w == 0, and we do not yet need to handle that case.
        ASSERT(w);
        double invW = 1.0 / w;
        return FloatPoint(x * invW, y * invW);
    }

    double x;
    double y;
    double z;
    double w;
};

static HomogeneousCoordinate projectPoint(const TransformationMatrix& transform, const FloatPoint& p)
{
    // In this case, the layer we are trying to project onto is perpendicular to ray
    // (point p and z-axis direction) that we are trying to project. This happens when the
    // layer is rotated so that it is infinitesimally thin, or when it is co-planar with
    // the camera origin -- i.e. when the layer is invisible anyway.
    if (!transform.m33())
        return HomogeneousCoordinate(0, 0, 0, 1);

    double x = p.x();
    double y = p.y();
    double z = -(transform.m13() * x + transform.m23() * y + transform.m43()) / transform.m33();
    // implicit definition of w = 1;

    double outX = x * transform.m11() + y * transform.m21() + z * transform.m31() + transform.m41();
    double outY = x * transform.m12() + y * transform.m22() + z * transform.m32() + transform.m42();
    double outZ = x * transform.m13() + y * transform.m23() + z * transform.m33() + transform.m43();
    double outW = x * transform.m14() + y * transform.m24() + z * transform.m34() + transform.m44();

    return HomogeneousCoordinate(outX, outY, outZ, outW);
}

static HomogeneousCoordinate mapPoint(const TransformationMatrix& transform, const FloatPoint& p)
{
    double x = p.x();
    double y = p.y();
    // implicit definition of z = 0;
    // implicit definition of w = 1;

    double outX = x * transform.m11() + y * transform.m21() + transform.m41();
    double outY = x * transform.m12() + y * transform.m22() + transform.m42();
    double outZ = x * transform.m13() + y * transform.m23() + transform.m43();
    double outW = x * transform.m14() + y * transform.m24() + transform.m44();

    return HomogeneousCoordinate(outX, outY, outZ, outW);
}

static HomogeneousCoordinate computeClippedPointForEdge(const HomogeneousCoordinate& h1, const HomogeneousCoordinate& h2)
{
    // Points h1 and h2 form a line in 4d, and any point on that line can be represented
    // as an interpolation between h1 and h2:
    //    p = (1-t) h1 + (t) h2
    //
    // We want to compute point p such that p.w == epsilon, where epsilon is a small
    // non-zero number. (but the smaller the number is, the higher the risk of overflow)
    // To do this, we solve for t in the following equation:
    //    p.w = epsilon = (1-t) * h1.w + (t) * h2.w
    //
    // Once paramter t is known, the rest of p can be computed via p = (1-t) h1 + (t) h2.

    // Technically this is a special case of the following assertion, but its a good idea to keep it an explicit sanity check here.
    ASSERT(h2.w != h1.w);
    // Exactly one of h1 or h2 (but not both) must be on the negative side of the w plane when this is called.
    ASSERT(h1.shouldBeClipped() ^ h2.shouldBeClipped());

    double w = 0.00001; // or any positive non-zero small epsilon

    double t = (w - h1.w) / (h2.w - h1.w);

    double x = (1-t) * h1.x + t * h2.x;
    double y = (1-t) * h1.y + t * h2.y;
    double z = (1-t) * h1.z + t * h2.z;

    return HomogeneousCoordinate(x, y, z, w);
}

static inline void expandBoundsToIncludePoint(float& xmin, float& xmax, float& ymin, float& ymax, const FloatPoint& p)
{
    xmin = std::min(p.x(), xmin);
    xmax = std::max(p.x(), xmax);
    ymin = std::min(p.y(), ymin);
    ymax = std::max(p.y(), ymax);
}

static FloatRect computeEnclosingRect(const HomogeneousCoordinate& h1, const HomogeneousCoordinate& h2, const HomogeneousCoordinate& h3, const HomogeneousCoordinate& h4)
{
    // This function performs clipping as necessary and computes the enclosing 2d
    // FloatRect of the vertices. Doing these two steps simultaneously allows us to avoid
    // the overhead of storing an unknown number of clipped vertices.

    // If no vertices on the quad are clipped, then we can simply return the enclosing rect directly.
    bool somethingClipped = h1.shouldBeClipped() || h2.shouldBeClipped() || h3.shouldBeClipped() || h4.shouldBeClipped();
    if (!somethingClipped) {
        FloatQuad mappedQuad = FloatQuad(h1.cartesianPoint2d(), h2.cartesianPoint2d(), h3.cartesianPoint2d(), h4.cartesianPoint2d());
        return mappedQuad.boundingBox();
    }

    bool everythingClipped = h1.shouldBeClipped() && h2.shouldBeClipped() && h3.shouldBeClipped() && h4.shouldBeClipped();
    if (everythingClipped)
        return FloatRect();

    float xmin = std::numeric_limits<float>::max();
    float xmax = std::numeric_limits<float>::min();
    float ymin = std::numeric_limits<float>::max();
    float ymax = std::numeric_limits<float>::min();

    if (!h1.shouldBeClipped())
        expandBoundsToIncludePoint(xmin, xmax, ymin, ymax, h1.cartesianPoint2d());

    if (h1.shouldBeClipped() ^ h2.shouldBeClipped())
        expandBoundsToIncludePoint(xmin, xmax, ymin, ymax, computeClippedPointForEdge(h1, h2).cartesianPoint2d());

    if (!h2.shouldBeClipped())
        expandBoundsToIncludePoint(xmin, xmax, ymin, ymax, h2.cartesianPoint2d());

    if (h2.shouldBeClipped() ^ h3.shouldBeClipped())
        expandBoundsToIncludePoint(xmin, xmax, ymin, ymax, computeClippedPointForEdge(h2, h3).cartesianPoint2d());

    if (!h3.shouldBeClipped())
        expandBoundsToIncludePoint(xmin, xmax, ymin, ymax, h3.cartesianPoint2d());

    if (h3.shouldBeClipped() ^ h4.shouldBeClipped())
        expandBoundsToIncludePoint(xmin, xmax, ymin, ymax, computeClippedPointForEdge(h3, h4).cartesianPoint2d());

    if (!h4.shouldBeClipped())
        expandBoundsToIncludePoint(xmin, xmax, ymin, ymax, h4.cartesianPoint2d());

    if (h4.shouldBeClipped() ^ h1.shouldBeClipped())
        expandBoundsToIncludePoint(xmin, xmax, ymin, ymax, computeClippedPointForEdge(h4, h1).cartesianPoint2d());

    return FloatRect(FloatPoint(xmin, ymin), FloatSize(xmax - xmin, ymax - ymin));
}

static inline void addVertexToClippedQuad(const FloatPoint& newVertex, FloatPoint clippedQuad[8], int& numVerticesInClippedQuad)
{
    clippedQuad[numVerticesInClippedQuad] = newVertex;
    numVerticesInClippedQuad++;
}

IntRect CCMathUtil::mapClippedRect(const TransformationMatrix& transform, const IntRect& srcRect)
{
    return enclosingIntRect(mapClippedRect(transform, FloatRect(srcRect)));
}

FloatRect CCMathUtil::mapClippedRect(const TransformationMatrix& transform, const FloatRect& srcRect)
{
    if (transform.isIdentityOrTranslation()) {
        FloatRect mappedRect(srcRect);
        mappedRect.move(static_cast<float>(transform.m41()), static_cast<float>(transform.m42()));
        return mappedRect;
    }

    // Apply the transform, but retain the result in homogeneous coordinates.
    FloatQuad q = FloatQuad(FloatRect(srcRect));
    HomogeneousCoordinate h1 = mapPoint(transform, q.p1());
    HomogeneousCoordinate h2 = mapPoint(transform, q.p2());
    HomogeneousCoordinate h3 = mapPoint(transform, q.p3());
    HomogeneousCoordinate h4 = mapPoint(transform, q.p4());

    return computeEnclosingRect(h1, h2, h3, h4);
}

FloatRect CCMathUtil::projectClippedRect(const TransformationMatrix& transform, const FloatRect& srcRect)
{
    // Perform the projection, but retain the result in homogeneous coordinates.
    FloatQuad q = FloatQuad(FloatRect(srcRect));
    HomogeneousCoordinate h1 = projectPoint(transform, q.p1());
    HomogeneousCoordinate h2 = projectPoint(transform, q.p2());
    HomogeneousCoordinate h3 = projectPoint(transform, q.p3());
    HomogeneousCoordinate h4 = projectPoint(transform, q.p4());

    return computeEnclosingRect(h1, h2, h3, h4);
}

void CCMathUtil::mapClippedQuad(const TransformationMatrix& transform, const FloatQuad& srcQuad, FloatPoint clippedQuad[8], int& numVerticesInClippedQuad)
{
    HomogeneousCoordinate h1 = mapPoint(transform, srcQuad.p1());
    HomogeneousCoordinate h2 = mapPoint(transform, srcQuad.p2());
    HomogeneousCoordinate h3 = mapPoint(transform, srcQuad.p3());
    HomogeneousCoordinate h4 = mapPoint(transform, srcQuad.p4());

    // The order of adding the vertices to the array is chosen so that clockwise / counter-clockwise orientation is retained.

    numVerticesInClippedQuad = 0;

    if (!h1.shouldBeClipped())
        addVertexToClippedQuad(h1.cartesianPoint2d(), clippedQuad, numVerticesInClippedQuad);

    if (h1.shouldBeClipped() ^ h2.shouldBeClipped())
        addVertexToClippedQuad(computeClippedPointForEdge(h1, h2).cartesianPoint2d(), clippedQuad, numVerticesInClippedQuad);

    if (!h2.shouldBeClipped())
        addVertexToClippedQuad(h2.cartesianPoint2d(), clippedQuad, numVerticesInClippedQuad);

    if (h2.shouldBeClipped() ^ h3.shouldBeClipped())
        addVertexToClippedQuad(computeClippedPointForEdge(h2, h3).cartesianPoint2d(), clippedQuad, numVerticesInClippedQuad);

    if (!h3.shouldBeClipped())
        addVertexToClippedQuad(h3.cartesianPoint2d(), clippedQuad, numVerticesInClippedQuad);

    if (h3.shouldBeClipped() ^ h4.shouldBeClipped())
        addVertexToClippedQuad(computeClippedPointForEdge(h3, h4).cartesianPoint2d(), clippedQuad, numVerticesInClippedQuad);

    if (!h4.shouldBeClipped())
        addVertexToClippedQuad(h4.cartesianPoint2d(), clippedQuad, numVerticesInClippedQuad);

    if (h4.shouldBeClipped() ^ h1.shouldBeClipped())
        addVertexToClippedQuad(computeClippedPointForEdge(h4, h1).cartesianPoint2d(), clippedQuad, numVerticesInClippedQuad);

    ASSERT(numVerticesInClippedQuad <= 8);
}

FloatRect CCMathUtil::computeEnclosingRectOfVertices(FloatPoint vertices[], int numVertices)
{
    if (numVertices < 2)
        return FloatRect();

    float xmin = std::numeric_limits<float>::max();
    float xmax = std::numeric_limits<float>::min();
    float ymin = std::numeric_limits<float>::max();
    float ymax = std::numeric_limits<float>::min();

    for (int i = 0; i < numVertices; ++i)
        expandBoundsToIncludePoint(xmin, xmax, ymin, ymax, vertices[i]);

    return FloatRect(FloatPoint(xmin, ymin), FloatSize(xmax - xmin, ymax - ymin));
}

FloatQuad CCMathUtil::mapQuad(const TransformationMatrix& transform, const FloatQuad& q, bool& clipped)
{
    if (transform.isIdentityOrTranslation()) {
        FloatQuad mappedQuad(q);
        mappedQuad.move(static_cast<float>(transform.m41()), static_cast<float>(transform.m42()));
        clipped = false;
        return mappedQuad;
    }

    HomogeneousCoordinate h1 = mapPoint(transform, q.p1());
    HomogeneousCoordinate h2 = mapPoint(transform, q.p2());
    HomogeneousCoordinate h3 = mapPoint(transform, q.p3());
    HomogeneousCoordinate h4 = mapPoint(transform, q.p4());

    clipped = h1.shouldBeClipped() || h2.shouldBeClipped() || h3.shouldBeClipped() || h4.shouldBeClipped();

    // Result will be invalid if clipped == true. But, compute it anyway just in case, to emulate existing behavior.
    return FloatQuad(h1.cartesianPoint2d(), h2.cartesianPoint2d(), h3.cartesianPoint2d(), h4.cartesianPoint2d());
}

FloatQuad CCMathUtil::projectQuad(const TransformationMatrix& transform, const FloatQuad& q, bool& clipped)
{
    FloatQuad projectedQuad;
    bool clippedPoint;
    projectedQuad.setP1(transform.projectPoint(q.p1(), &clippedPoint));
    clipped = clippedPoint;
    projectedQuad.setP2(transform.projectPoint(q.p2(), &clippedPoint));
    clipped |= clippedPoint;
    projectedQuad.setP3(transform.projectPoint(q.p3(), &clippedPoint));
    clipped |= clippedPoint;
    projectedQuad.setP4(transform.projectPoint(q.p4(), &clippedPoint));
    clipped |= clippedPoint;

    return projectedQuad;
}

} // namespace WebCore
