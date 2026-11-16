/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/FloatRect.h>
#include <WebCore/IntRect.h>
#include <wtf/Forward.h>

#include <wtf/Vector.h>

namespace WebCore {

class FloatQuad;

NODELETE float euclidianDistance(const FloatSize&);
NODELETE WEBCORE_EXPORT float euclidianDistance(const FloatPoint&, const FloatPoint&);

NODELETE float dotProduct(const FloatSize&, const FloatSize&);

// Which side of the directed line lineStart->lineEnd a point lies on: > 0 to the left of the direction, < 0 to the right, 0 on the line.
NODELETE float signedDistanceToLine(const FloatPoint&, const FloatPoint& lineStart, const FloatPoint& lineEnd);

// Find where two infinite lines intersect, each given by a pair of points lying on it. Returns std::nullopt if the lines are parallel.
NODELETE WEBCORE_EXPORT std::optional<FloatPoint> findIntersection(const FloatPoint& p1, const FloatPoint& p2, const FloatPoint& d1, const FloatPoint& d2);

NODELETE std::optional<FloatPoint> findLineIntersection(const FloatPoint& firstOrigin, const FloatSize& firstDirection, const FloatPoint& secondOrigin, const FloatSize& secondDirection, float parallelTolerance = 1e-6f);

// Find where the segment crosses the infinite line through lineA and lineB. Returns std::nullopt if they are parallel or the crossing lies beyond the segment's ends.
NODELETE std::optional<FloatPoint> findSegmentLineIntersection(const FloatPoint& segmentStart, const FloatPoint& segmentEnd, const FloatPoint& lineA, const FloatPoint& lineB);

NODELETE WEBCORE_EXPORT IntRect unionRect(const Vector<IntRect>&);
NODELETE WEBCORE_EXPORT IntRect unionRectIgnoringZeroRects(const Vector<IntRect>&);
NODELETE WEBCORE_EXPORT FloatRect unionRect(const Vector<FloatRect>&);
NODELETE WEBCORE_EXPORT FloatRect unionRectIgnoringZeroRects(const Vector<FloatRect>&);

// Map point from srcRect to an equivalent point in destRect.
NODELETE FloatPoint mapPoint(FloatPoint, const FloatRect& srcRect, const FloatRect& destRect);

// Map rect from srcRect to an equivalent rect in destRect.
NODELETE WEBCORE_EXPORT FloatRect mapRect(const FloatRect&, const FloatRect& srcRect, const FloatRect& destRect);

NODELETE WEBCORE_EXPORT FloatRect largestRectWithAspectRatioInsideRect(float aspectRatio, const FloatRect&);
NODELETE WEBCORE_EXPORT FloatRect smallestRectWithAspectRatioAroundRect(float aspectRatio, const FloatRect&);

// Given a natural size and a subrect of it, compute the rect that natural-sized content
// would occupy such that subrect maps exactly onto destRect. Used by object-view-box rendering.
NODELETE FloatRect fullRectFromSubrectAndSize(const FloatSize& naturalSize, const FloatRect& subrect, const FloatRect& destRect);

NODELETE FloatSize sizeWithAreaAndAspectRatio(float area, float aspectRatio);

// Compute a rect that encloses all points covered by the given rect if it were rotated a full turn around (0,0).
NODELETE FloatRect boundsOfRotatingRect(const FloatRect&);

NODELETE bool ellipseContainsPoint(const FloatPoint& center, const FloatSize& radii, const FloatPoint&);
NODELETE float eccentricAngle(FloatPoint, FloatPoint center, float radiusX, float radiusY);

NODELETE FloatPoint midPoint(const FloatPoint&, const FloatPoint&);

// The point a fraction `t` of the way from `from` to `to`: from + t * (to - from). (t = 0.5 is midPoint.)
NODELETE FloatPoint linearInterpolation(const FloatPoint& from, const FloatPoint& to, float);

// -------------
// |   h\  |s  |
// |     \a|   |
// |      \|   |
// |       *   |
// |     (x,y) |
// -------------
// Given a box and a ray (described by an offset from the top left corner of the box and angle from vertical in degrees), compute
// the length from the starting position to the intersection of the ray with the box. Given the above diagram, we are
// trying to calculate h, with lengthOfPointToSideOfIntersection computing the length of s, and angleOfPointToSideOfIntersection
// computing a.
NODELETE double lengthOfRayIntersectionWithBoundingBox(const FloatRect& boundingRect, const std::pair<const FloatPoint&, float> ray);

// Given a box and a ray (described by an offset from the top left corner of the box and angle from vertical in degrees),
// compute the closest length from the starting position to the side that the ray intersects with.
NODELETE double lengthOfPointToSideOfIntersection(const FloatRect& boundingRect, const std::pair<const FloatPoint&, float> ray);

// Given a box and a ray (described by an offset from the top left corner of the box and angle from vertical in degrees)
// compute the acute angle between the ray and the line segment from the starting point to the closest point on the
// side that the ray intersects with.
NODELETE float angleOfPointToSideOfIntersection(const FloatRect& boundingRect, const std::pair<const FloatPoint&, float> ray);

// Given a box and an offset from the top left corner, calculate the distance of the point from each side
NODELETE RectEdges<double> distanceOfPointToSidesOfRect(const FloatRect&, const FloatPoint&);

NODELETE float distanceToClosestSide(FloatPoint, FloatSize);
NODELETE float distanceToFarthestSide(FloatPoint, FloatSize);
NODELETE float distanceToClosestCorner(FloatPoint, FloatSize);
NODELETE float distanceToFarthestCorner(FloatPoint, FloatSize);

// Given a box and an offset from the top left corner, construct a coordinate system with this offset as the origin,
// and return the vertices of the box in this coordinate system
NODELETE std::array<FloatPoint, 4> verticesForBox(const FloatRect&, const FloatPoint);

NODELETE float toPositiveAngle(float angle);
NODELETE float toRelatedAcuteAngle(float angle);

NODELETE float normalizeAngleInRadians(float radians);

NODELETE WEBCORE_EXPORT FloatRect scaledRectAtOrigin(const FloatRect& sourceRect, float scale, const FloatPoint& origin);

struct RotatedRect {
    FloatPoint center;
    FloatSize size;
    float angleInRadians;
};

NODELETE WEBCORE_EXPORT RotatedRect rotatedBoundingRectWithMinimumAngleOfRotation(const FloatQuad&, std::optional<float> minRotationInRadians = std::nullopt);

static inline float min3(float a, float b, float c)
{
    return std::min(std::min(a, b), c);
}

static inline float max3(float a, float b, float c)
{
    return std::max(std::max(a, b), c);
}

static inline float min4(float a, float b, float c, float d)
{
    return std::min(std::min(a, b), std::min(c, d));
}

static inline float max4(float a, float b, float c, float d)
{
    return std::max(std::max(a, b), std::max(c, d));
}

}
