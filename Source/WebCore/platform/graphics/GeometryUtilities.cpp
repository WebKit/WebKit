/*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

float euclidianDistance(const FloatSize& delta)
{
    return std::hypot(delta.width(), delta.height());
}

float euclidianDistance(const FloatPoint& p1, const FloatPoint& p2)
{
    return euclidianDistance(p1 - p2);
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
    float pxLength = p2.x() - p1.x();
    float pyLength = p2.y() - p1.y();

    float dxLength = d2.x() - d1.x();
    float dyLength = d2.y() - d1.y();

    float denom = pxLength * dyLength - pyLength * dxLength;
    if (!denom)
        return false;
    
    float param = ((d1.x() - p1.x()) * dyLength - (d1.y() - p1.y()) * dxLength) / denom;

    intersection.setX(p1.x() + param * pxLength);
    intersection.setY(p1.y() + param * pyLength);
    return true;
}

IntRect unionRect(const Vector<IntRect>& rects)
{
    IntRect result;
    for (auto& rect : rects)
        result.unite(rect);
    return result;
}

IntRect unionRectIgnoringZeroRects(const Vector<IntRect>& rects)
{
    IntRect result;
    for (auto& rect : rects)
        result.uniteIfNonZero(rect);
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

float toPositiveAngle(float angle)
{
    angle = fmod(angle, 360);
    while (angle < 0)
        angle += 360.0;
    return angle;
}

// Compute acute angle from vertical axis
float toRelatedAcuteAngle(float angle)
{
    angle = toPositiveAngle(angle);
    if (angle < 90)
        return angle;
    if (angle > 90 || angle < 180)
        return std::abs(180 - angle);
    return std::abs(360 - angle);
}

RectEdges<double> distanceOfPointToSidesOfRect(const FloatRect& box, const FloatPoint& position)
{
    // Compute distance to each side of the containing box
    double top = std::abs(position.y());
    double bottom = std::abs(position.y() - box.height());
    double left = std::abs(position.x());
    double right = std::abs(position.x() - box.width());
    return RectEdges<double>(top, right, bottom, left);
}

std::array<FloatPoint, 4> verticesForBox(const FloatRect& box, const FloatPoint position)
{
    return { FloatPoint(-position.x(), -position.y()),
        FloatPoint(box.width() - position.x(), -position.y()),
        FloatPoint(box.width() - position.x(), box.height() - position.y()),
        FloatPoint(-position.x(), box.height() - position.y()) };
}

double lengthOfRayIntersectionWithBoundingBox(const FloatRect& boundingRect, const std::pair<const FloatPoint&, float> ray)
{
    auto length = lengthOfPointToSideOfIntersection(boundingRect, ray);
    auto angleOfTriangle = angleOfPointToSideOfIntersection(boundingRect, ray);
    // Given a length and angle of a right triangle, calculate the hypotenuse, which corresponds to
    // the length from the given point to the intersecting point on the box
    return length / cos(deg2rad(angleOfTriangle));
}

// Get the side of box the ray intersects with
static BoxSide intersectionSide(const FloatRect& boundingRect, const std::pair<const FloatPoint&, float> ray)
{
    auto position = ray.first;
    auto angleInRadians = deg2rad(ray.second);
    auto distances = distanceOfPointToSidesOfRect(boundingRect, position);
    // Get possible intersection sides
    auto s1 = cos(angleInRadians) >= 0 ? distances.top() : distances.bottom();
    auto s2 = sin(angleInRadians) >= 0 ? distances.right() : distances.left();
    auto vertical = cos(angleInRadians) >= 0 ? BoxSide::Top : BoxSide::Bottom;
    auto horizontal = sin(angleInRadians) >= 0 ? BoxSide::Right : BoxSide::Left;
    auto acuteAngle = deg2rad(toRelatedAcuteAngle(ray.second));
    return sin(acuteAngle) * s1  > cos(acuteAngle) * s2 ? horizontal : vertical;
}

double lengthOfPointToSideOfIntersection(const FloatRect& boundingRect, const std::pair<const FloatPoint&, float> ray)
{
    auto position = ray.first;
    if (position.x() < 0 || position.x() > boundingRect.width() || position.y() < 0 || position.y() > boundingRect.height())
        return 0;
    auto distances = distanceOfPointToSidesOfRect(boundingRect, position);
    return distances.at(intersectionSide(boundingRect, ray));
}

float angleOfPointToSideOfIntersection(const FloatRect& boundingRect, const std::pair<const FloatPoint&, float> ray)
{
    auto angle = ray.second;
    auto side = intersectionSide(boundingRect, ray);
    angle = toRelatedAcuteAngle(toPositiveAngle(angle));
    return side == BoxSide::Top || side == BoxSide::Bottom ? angle : 90 - angle;
}

float normalizeAngleInRadians(float radians)
{
    float circles = radians / radiansPerTurnFloat;
    return radiansPerTurnFloat * (circles - floor(circles));
}

}
