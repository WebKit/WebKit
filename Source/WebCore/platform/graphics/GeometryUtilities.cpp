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

#include "config.h"
#include "GeometryUtilities.h"

#include "FloatQuad.h"
#include <wtf/MathExtras.h>
#include <wtf/Vector.h>

namespace WebCore {

float euclidianDistance(const FloatPoint& p1, const FloatPoint& p2)
{
    FloatSize delta = p1 - p2;
    return std::hypot(delta.width(), delta.height());
}

float findSlope(const FloatPoint& p1, const FloatPoint& p2, float& c)
{
    if (p2.x() == p1.x())
        return std::numeric_limits<float>::infinity();

    // y = mx + c
    float slope = (p2.y() - p1.y()) / (p2.x() - p1.x());
    c = p1.y() - slope * p1.x();
    return slope;
}

bool findIntersection(const FloatPoint& p1, const FloatPoint& p2, const FloatPoint& d1, const FloatPoint& d2, FloatPoint& intersection) 
{
    float pOffset = 0;
    float pSlope = findSlope(p1, p2, pOffset);

    float dOffset = 0;
    float dSlope = findSlope(d1, d2, dOffset);

    if (dSlope == pSlope)
        return false;
    
    if (pSlope == std::numeric_limits<float>::infinity()) {
        intersection.setX(p1.x());
        intersection.setY(dSlope * intersection.x() + dOffset);
        return true;
    }
    if (dSlope == std::numeric_limits<float>::infinity()) {
        intersection.setX(d1.x());
        intersection.setY(pSlope * intersection.x() + pOffset);
        return true;
    }
    
    // Find x at intersection, where ys overlap; x = (c' - c) / (m - m')
    intersection.setX((dOffset - pOffset) / (pSlope - dSlope));
    intersection.setY(pSlope * intersection.x() + pOffset);
    return true;
}

IntRect unionRect(const Vector<IntRect>& rects)
{
    IntRect result;
    for (auto& rect : rects)
        result.unite(rect);
    return result;
}

FloatRect unionRect(const Vector<FloatRect>& rects)
{
    FloatRect result;
    for (auto& rect : rects)
        result.unite(rect);
    return result;
}

FloatRect unionRectIgnoringZeroRects(const Vector<FloatRect>& rects)
{
    FloatRect result;
    for (auto& rect : rects)
        result.uniteIfNonZero(rect);
    return result;
}

FloatPoint mapPoint(FloatPoint p, const FloatRect& srcRect, const FloatRect& destRect)
{
    if (!srcRect.width() || !srcRect.height())
        return p;

    float widthScale = destRect.width() / srcRect.width();
    float heightScale = destRect.height() / srcRect.height();

    return {
        destRect.x() + (p.x() - srcRect.x()) * widthScale,
        destRect.y() + (p.y() - srcRect.y()) * heightScale
    };
}

FloatRect mapRect(const FloatRect& r, const FloatRect& srcRect, const FloatRect& destRect)
{
    if (!srcRect.width() || !srcRect.height())
        return FloatRect();

    float widthScale = destRect.width() / srcRect.width();
    float heightScale = destRect.height() / srcRect.height();
    return {
        destRect.x() + (r.x() - srcRect.x()) * widthScale,
        destRect.y() + (r.y() - srcRect.y()) * heightScale,
        r.width() * widthScale,
        r.height() * heightScale
    };
}

FloatRect largestRectWithAspectRatioInsideRect(float aspectRatio, const FloatRect& srcRect)
{
    FloatRect destRect = srcRect;

    if (aspectRatio > srcRect.size().aspectRatio()) {
        float dy = destRect.width() / aspectRatio - destRect.height();
        destRect.inflateY(dy / 2);
    } else {
        float dx = destRect.height() * aspectRatio - destRect.width();
        destRect.inflateX(dx / 2);
    }
    return destRect;
}

FloatRect boundsOfRotatingRect(const FloatRect& r)
{
    // Compute the furthest corner from the origin.
    float maxCornerDistance = euclidianDistance(FloatPoint(), r.minXMinYCorner());
    maxCornerDistance = std::max(maxCornerDistance, euclidianDistance(FloatPoint(), r.maxXMinYCorner()));
    maxCornerDistance = std::max(maxCornerDistance, euclidianDistance(FloatPoint(), r.minXMaxYCorner()));
    maxCornerDistance = std::max(maxCornerDistance, euclidianDistance(FloatPoint(), r.maxXMaxYCorner()));
    
    return FloatRect(-maxCornerDistance, -maxCornerDistance, 2 * maxCornerDistance, 2 * maxCornerDistance);
}

FloatRect smallestRectWithAspectRatioAroundRect(float aspectRatio, const FloatRect& srcRect)
{
    FloatRect destRect = srcRect;

    if (aspectRatio < srcRect.size().aspectRatio()) {
        float dy = destRect.width() / aspectRatio - destRect.height();
        destRect.inflateY(dy / 2);
    } else {
        float dx = destRect.height() * aspectRatio - destRect.width();
        destRect.inflateX(dx / 2);
    }
    return destRect;
}

FloatSize sizeWithAreaAndAspectRatio(float area, float aspectRatio)
{
    auto scaledWidth = std::sqrt(area * aspectRatio);
    return { scaledWidth, scaledWidth / aspectRatio };
}

bool ellipseContainsPoint(const FloatPoint& center, const FloatSize& radii, const FloatPoint& point)
{
    if (radii.width() <= 0 || radii.height() <= 0)
        return false;

    // First, offset the query point so that the ellipse is effectively centered at the origin.
    FloatPoint transformedPoint(point);
    transformedPoint.move(-center.x(), -center.y());

    // If the point lies outside of the bounding box determined by the radii of the ellipse, it can't possibly
    // be contained within the ellipse, so bail early.
    if (transformedPoint.x() < -radii.width() || transformedPoint.x() > radii.width() || transformedPoint.y() < -radii.height() || transformedPoint.y() > radii.height())
        return false;

    // Let (x, y) represent the translated point, and let (Rx, Ry) represent the radii of an ellipse centered at the origin.
    // (x, y) is contained within the ellipse if, after scaling the ellipse to be a unit circle, the identically scaled
    // point lies within that unit circle. In other words, the squared distance (x/Rx)^2 + (y/Ry)^2 of the transformed point
    // to the origin is no greater than 1. This is equivalent to checking whether or not the point (xRy, yRx) lies within a
    // circle of radius RxRy.
    transformedPoint.scale(radii.height(), radii.width());
    auto transformedRadius = radii.width() * radii.height();

    // We can bail early if |xRy| + |yRx| <= RxRy to avoid additional multiplications, since that means the Manhattan distance
    // of the transformed point is less than the radius, so the point must lie within the transformed circle.
    return std::abs(transformedPoint.x()) + std::abs(transformedPoint.y()) <= transformedRadius || transformedPoint.lengthSquared() <= transformedRadius * transformedRadius;
}

FloatPoint midPoint(const FloatPoint& first, const FloatPoint& second)
{
    return { (first.x() + second.x()) / 2, (first.y() + second.y()) / 2 };
}

static float dotProduct(const FloatSize& u, const FloatSize& v)
{
    return u.width() * v.width() + u.height() * v.height();
}

static float angleBetweenVectors(const FloatSize& u, const FloatSize& v)
{
    auto magnitudes = u.diagonalLength() * v.diagonalLength();
    return magnitudes ? acos(clampTo<float>(dotProduct(u, v) / magnitudes, -1, 1)) : 0;
}

RotatedRect rotatedBoundingRectWithMinimumAngleOfRotation(const FloatQuad& quad, std::optional<float> minRotationInRadians)
{
    constexpr auto twoPiFloat = 2 * piFloat;

    auto minRotationAmount = minRotationInRadians.value_or(std::numeric_limits<float>::epsilon());

    auto leftMidPoint = midPoint(quad.p1(), quad.p4());
    auto rightMidPoint = midPoint(quad.p2(), quad.p3());
    auto widthVector = rightMidPoint - leftMidPoint;

    auto midPointToMidPointDistance = widthVector.diagonalLength();
    int signOfWidthVectorHeight = widthVector.height() < 0 ? -1 : 1;
    auto angle = midPointToMidPointDistance ? signOfWidthVectorHeight * acos(widthVector.width() / midPointToMidPointDistance) : 0;
    if (angle < 0)
        angle += twoPiFloat;

    if (std::abs(angle) < minRotationAmount || std::abs(twoPiFloat - angle) < minRotationAmount) {
        auto boundingBox = quad.boundingBox();
        return { boundingBox.center(), boundingBox.size(), 0 };
    }

    auto heightVector = FloatSize { widthVector.height(), -widthVector.width() };
    auto leftPerpendicularAngle = angleBetweenVectors(heightVector, quad.p1() - leftMidPoint);
    auto rightPerpendicularAngle = angleBetweenVectors(heightVector, quad.p2() - rightMidPoint);

    auto leftHypotenuseLength = (leftMidPoint - quad.p1()).diagonalLength();
    auto rightHypotenuseLength = (rightMidPoint - quad.p2()).diagonalLength();

    auto leftMargin = leftHypotenuseLength * sin(leftPerpendicularAngle);
    auto rightMargin = rightHypotenuseLength * sin(rightPerpendicularAngle);
    auto width = midPointToMidPointDistance + leftMargin + rightMargin;
    auto height = 2 * std::max(leftHypotenuseLength * cos(leftPerpendicularAngle), rightHypotenuseLength * cos(rightPerpendicularAngle));

    auto leftMidToCenterDistance = (midPointToMidPointDistance + rightMargin - leftMargin) / 2;
    auto center = leftMidPoint + (widthVector * leftMidToCenterDistance / midPointToMidPointDistance);
    return { center, { width, height }, angle };
}

}
