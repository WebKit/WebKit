/*
 * Copyright (C) 2023 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PathSegmentData.h"

#include "AffineTransform.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

FloatPoint PathMoveTo::calculateEndPoint(const FloatPoint&, FloatPoint& lastMoveToPoint) const
{
    lastMoveToPoint = point;
    return lastMoveToPoint;
}

std::optional<FloatPoint> PathMoveTo::tryGetEndPointWithoutContext() const
{
    FloatPoint lastMoveToPoint;
    return calculateEndPoint({ }, lastMoveToPoint);
}

void PathMoveTo::extendFastBoundingRect(const FloatPoint&, const FloatPoint&, FloatRect&) const
{
}

void PathMoveTo::extendBoundingRect(const FloatPoint&, const FloatPoint&, FloatRect&) const
{
}

void PathMoveTo::applyElements(const PathElementApplier& applier) const
{
    applier({ PathElement::Type::MoveToPoint, { point } });
}

void PathMoveTo::transform(const AffineTransform& transform)
{
    point = transform.mapPoint(point);
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const PathMoveTo& data)
{
    ts << "move to " << data.point;
    return ts;
}

FloatPoint PathLineTo::calculateEndPoint(const FloatPoint&, FloatPoint&) const
{
    return point;
}

std::optional<FloatPoint> PathLineTo::tryGetEndPointWithoutContext() const
{
    FloatPoint lastMoveToPoint;
    return calculateEndPoint({ }, lastMoveToPoint);
}

void PathLineTo::extendFastBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const
{
    extendBoundingRect(currentPoint, lastMoveToPoint, boundingRect);
}

void PathLineTo::extendBoundingRect(const FloatPoint& currentPoint, const FloatPoint&, FloatRect& boundingRect) const
{
    boundingRect.extend(currentPoint);
    boundingRect.extend(point);
}

void PathLineTo::applyElements(const PathElementApplier& applier) const
{
    applier({ PathElement::Type::AddLineToPoint, { point } });
}

void PathLineTo::transform(const AffineTransform& transform)
{
    point = transform.mapPoint(point);
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const PathLineTo& data)
{
    ts << "add line to " << data.point;
    return ts;
}

FloatPoint PathQuadCurveTo::calculateEndPoint(const FloatPoint&, FloatPoint&) const
{
    return endPoint;
}

std::optional<FloatPoint> PathQuadCurveTo::tryGetEndPointWithoutContext() const
{
    FloatPoint lastMoveToPoint;
    return calculateEndPoint({ }, lastMoveToPoint);
}

void PathQuadCurveTo::extendFastBoundingRect(const FloatPoint& currentPoint, const FloatPoint&, FloatRect& boundingRect) const
{
    boundingRect.extend(currentPoint);
    boundingRect.extend(controlPoint);
    boundingRect.extend(endPoint);
}

static float calculateQuadratic(float t, float p0, float p1, float p2)
{
    // Fallback to a value which won't affect the calculations.
    if (!(t >= 0 && t <= 1))
        return p0;

    float s = 1 - t;

    // B(t) = (1 - t)^2 P0 + 2 (1 - t)t P1 + t^2 P2, 0 <= t <= 1
    return (s * s * p0) + (2 * s * t * p1) + (t * t * p2);
}

static float calculateQuadraticExtremity(float p0, float p1, float p2)
{
    // B(t)  = (1 - t)^2 P0 + 2 (1 - t)t P1 + t^2 P2, 0 <= t <= 1
    // B'(t) = 2(1 - t) (P1 - P0) + 2t (P2 - P1)
    //       = 2 (P1 - P0) + 2t (P0 - 2P1 + P2)
    //
    // Solve for B'(t) = 0
    //
    //     t = (P0 - P1) / (P0 - 2P1 + P2)
    //
    float t = (p0 - p1) / (p0 - 2 * p1 + p2);

    return calculateQuadratic(t, p0, p1, p2);
}

static FloatPoint calculateQuadraticExtremity(const FloatPoint& currentPoint, const FloatPoint& controlPoint, const FloatPoint& endPoint)
{
    float x = calculateQuadraticExtremity(currentPoint.x(), controlPoint.x(), endPoint.x());
    float y = calculateQuadraticExtremity(currentPoint.y(), controlPoint.y(), endPoint.y());
    return { x, y };
}

void PathQuadCurveTo::extendBoundingRect(const FloatPoint& currentPoint, const FloatPoint&, FloatRect& boundingRect) const
{
    auto extremity = calculateQuadraticExtremity(currentPoint, controlPoint, endPoint);
    boundingRect.extend(currentPoint);
    boundingRect.extend(extremity);
    boundingRect.extend(endPoint);
}

void PathQuadCurveTo::applyElements(const PathElementApplier& applier) const
{
    applier({ PathElement::Type::AddQuadCurveToPoint, { controlPoint, endPoint } });
}

void PathQuadCurveTo::transform(const AffineTransform& transform)
{
    controlPoint = transform.mapPoint(controlPoint);
    endPoint = transform.mapPoint(endPoint);
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const PathQuadCurveTo& data)
{
    ts << "add quad curve to " << data.controlPoint << " " << data.endPoint;
    return ts;
}

FloatPoint PathBezierCurveTo::calculateEndPoint(const FloatPoint&, FloatPoint&) const
{
    return endPoint;
}

std::optional<FloatPoint> PathBezierCurveTo::tryGetEndPointWithoutContext() const
{
    FloatPoint lastMoveToPoint;
    return calculateEndPoint({ }, lastMoveToPoint);
}

void PathBezierCurveTo::extendFastBoundingRect(const FloatPoint& currentPoint, const FloatPoint&, FloatRect& boundingRect) const
{
    boundingRect.extend(currentPoint);
    boundingRect.extend(controlPoint1);
    boundingRect.extend(controlPoint2);
    boundingRect.extend(endPoint);
}

static float calculateBezier(float t, float p0, float p1, float p2, float p3)
{
    // Fallback to a value which won't affect the calculations.
    if (!(t >= 0 && t <= 1))
        return p0;

    float s = (1 - t);

    // B(t)  = (1 - t)^3 P0 + 3 (1 - t)^2 t P1 + 3 (1 -t) t^2 P2 + t^3 p3, 0 <= t <= 1
    return (s * s * s * p0) + (3 * s * s * t * p1) + (3 * s * t * t * p2) + (t * t * t * p3);
}

static std::pair<float, float> calculateBezierExtremities(float p0, float p1, float p2, float p3)
{
    // B(t)  = (1 - t)^3 P0 + 3 (1 - t)^2 t P1 + 3 (1 -t) t^2 P2 + t^3 p3, 0 <= t <= 1
    // B'(t) = 3(1 - t)^2 (P1 - P0) + 6(1 - t)t (P2 - P1) + 3t^2 (P3 - P2)
    //
    // Let i = P1 - P0
    //     j = P2 - P1
    //     k = P3 - P2
    //
    // B'(t) = 3i(1 - t)^2 + 6j(1 - t)t + 3kt^2
    //       = (3i - 6j + 3k)t^2 + (-6i + 6j)t + 3i
    //
    // Let a = 3i - 6j + 3k
    //     b = -6i + 6j
    //     c = 3i
    //
    // B'(t) = at^2 + bt + c
    //
    // Solve for B'(t) = 0
    //
    //     t = (-b (+/-) sqrt(b^2 - 4ac))) / 2a
    //
    float i = p1 - p0;
    float j = p2 - p1;
    float k = p3 - p2;

    float a = 3 * i - 6 * j + 3 * k;
    float b = 6 * j - 6 * i;
    float c = 3 * i;

    if (abs(a) < 0.00001) {
        float t = -c / b;
        float s = calculateBezier(t, p0, p1, p2, p3);
        return std::make_pair(s, s);
    }

    float sqrtPart = b * b - 4 * a * c;
    if (sqrtPart < 0)
        return std::make_pair(p0, p0);

    float t1 = (-b + sqrt(sqrtPart)) / (2 * a);
    float t2 = (-b - sqrt(sqrtPart)) / (2 * a);

    float s1 = calculateBezier(t1, p0, p1, p2, p3);
    float s2 = calculateBezier(t2, p0, p1, p2, p3);

    return std::make_pair(s1, s2);
}

static std::pair<FloatPoint, FloatPoint> calculateBezierExtremities(const FloatPoint& currentPoint, const FloatPoint& controlPoint1, const FloatPoint& controlPoint2, const FloatPoint& endPoint)
{
    auto x = calculateBezierExtremities(currentPoint.x(), controlPoint1.x(), controlPoint2.x(), endPoint.x());
    auto y = calculateBezierExtremities(currentPoint.y(), controlPoint1.y(), controlPoint2.y(), endPoint.y());

    return std::make_pair(FloatPoint(x.first, y.first), FloatPoint(x.second, y.second));
}

void PathBezierCurveTo::extendBoundingRect(const FloatPoint& currentPoint, const FloatPoint&, FloatRect& boundingRect) const
{
    auto bezierExtremities = calculateBezierExtremities(currentPoint, controlPoint1, controlPoint2, endPoint);
    boundingRect.extend(currentPoint);
    boundingRect.extend(bezierExtremities.first);
    boundingRect.extend(bezierExtremities.second);
    boundingRect.extend(endPoint);
}

void PathBezierCurveTo::applyElements(const PathElementApplier& applier) const
{
    applier({ PathElement::Type::AddCurveToPoint, { controlPoint1, controlPoint2, endPoint } });
}

void PathBezierCurveTo::transform(const AffineTransform& transform)
{
    controlPoint1 = transform.mapPoint(controlPoint1);
    controlPoint2 = transform.mapPoint(controlPoint2);
    endPoint = transform.mapPoint(endPoint);
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const PathBezierCurveTo& data)
{
    ts << "add curve to " << data.controlPoint1 << " " << data.controlPoint2 << " " << data.endPoint;
    return ts;
}

static float angleOfLine(const FloatPoint& p1, const FloatPoint& p2)
{
    if (abs(p1.x() - p2.x()) < 0.00001)
        return p1.y() - p2.y() >= 0 ? piFloat / 2 : 3 * piFloat / 2;
    return atan2(p1.y() - p2.y(), p1.x() - p2.x());
}

static FloatPoint calculateArcToEndPoint(const FloatPoint& currentPoint, const FloatPoint& controlPoint1, const FloatPoint& controlPoint2, float radius)
{
    float angle1 = angleOfLine(currentPoint, controlPoint1);
    float angle2 = angleOfLine(controlPoint1, controlPoint2);
    float angleBteweenLines = angle2 - angle1;

    if (abs(angleBteweenLines) < 0.00001 || abs(angleBteweenLines) >= piFloat / 2)
        return controlPoint1;

    float adjacent = abs(radius / tan(angleBteweenLines / 2));

    float x = controlPoint1.x() + adjacent * cos(angle2);
    float y = controlPoint1.y() - adjacent * sin(angle2);
    return { x, y };
}

FloatPoint PathArcTo::calculateEndPoint(const FloatPoint& currentPoint, FloatPoint&) const
{
    return calculateArcToEndPoint(currentPoint, controlPoint1, controlPoint2, radius);
}

std::optional<FloatPoint> PathArcTo::tryGetEndPointWithoutContext() const
{
    return std::nullopt;
}

void PathArcTo::extendFastBoundingRect(const FloatPoint& currentPoint, const FloatPoint&, FloatRect& boundingRect) const
{
    boundingRect.extend(currentPoint);
    boundingRect.extend(controlPoint1);
    boundingRect.extend(controlPoint2);
}

void PathArcTo::extendBoundingRect(const FloatPoint& currentPoint, const FloatPoint&, FloatRect& boundingRect) const
{
    boundingRect.extend(currentPoint);
    boundingRect.extend(controlPoint1);
    boundingRect.extend(calculateArcToEndPoint(currentPoint, controlPoint1, controlPoint2, radius));
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const PathArcTo& data)
{
    ts << "add arc to " << data.controlPoint1 << " " << data.controlPoint2 << " " << data.radius;
    return ts;
}

FloatPoint PathArc::calculateEndPoint(const FloatPoint&, FloatPoint& lastMoveToPoint) const
{
    lastMoveToPoint = center + FloatSize { radius * cos(startAngle), - radius * sin(startAngle) };
    return center + FloatSize { radius * cos(endAngle), - radius * sin(endAngle) };
}

std::optional<FloatPoint> PathArc::tryGetEndPointWithoutContext() const
{
    FloatPoint lastMoveToPoint;
    return calculateEndPoint({ }, lastMoveToPoint);
}

void PathArc::extendFastBoundingRect(const FloatPoint&, const FloatPoint&, FloatRect& boundingRect) const
{
    auto minXMinYCorner = center - FloatSize { radius, radius };
    auto maxXMaxYCorner = center + FloatSize { radius, radius };
    boundingRect.extend(minXMinYCorner);
    boundingRect.extend(maxXMaxYCorner);
}

static float angleInClockwise(float angle, RotationDirection direction)
{
    return direction == RotationDirection::Clockwise ? angle : angle - radiansPerTurnFloat;
}

void PathArc::extendBoundingRect(const FloatPoint&, const FloatPoint&, FloatRect& boundingRect) const
{
    auto circleRect = FloatRect { center - FloatSize { radius, radius }, center + FloatSize { radius, radius } };

    if (endAngle - startAngle >= radiansPerTurnFloat) {
        boundingRect.extend(circleRect.minXMinYCorner());
        boundingRect.extend(circleRect.maxXMaxYCorner());
        return;
    }

    float x1 = center.x() + radius * cos(startAngle);
    float y1 = center.y() + radius * sin(startAngle);

    float x2 = center.x() + radius * cos(endAngle);
    float y2 = center.y() + radius * sin(endAngle);

    float startAngle = this->startAngle;
    float endAngle = this->endAngle;

    if (x1 > x2)
        std::swap(x1, x2);

    if (y1 > y2)
        std::swap(y1, y2);

    if (direction == RotationDirection::Counterclockwise) {
        std::swap(startAngle, endAngle);
        startAngle = angleInClockwise(startAngle, direction);
    }

    if (isInRange(float(0), startAngle, endAngle))
        x2 = circleRect.maxX();

    if (isInRange(angleInClockwise(piFloat / 2, direction), startAngle, endAngle))
        y2 = circleRect.maxY();

    if (isInRange(angleInClockwise(piFloat, direction), startAngle, endAngle))
        x1 = circleRect.x();

    if (isInRange(angleInClockwise(3 * piFloat / 2, direction), startAngle, endAngle))
        y1 = circleRect.y();

    boundingRect.extend({ x1, y1 });
    boundingRect.extend({ x2, y2 });
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const PathArc& data)
{
    ts << "add arc " << data.center << " " << data.radius  << " " << data.startAngle  << " " << data.endAngle  << " " << data.direction;
    return ts;
}

FloatPoint PathClosedArc::calculateEndPoint(const FloatPoint&, FloatPoint& lastMoveToPoint) const
{
    lastMoveToPoint = arc.center + FloatSize { arc.radius * cos(arc.startAngle), - arc.radius * sin(arc.startAngle) };
    return lastMoveToPoint;
}

std::optional<FloatPoint> PathClosedArc::tryGetEndPointWithoutContext() const
{
    FloatPoint lastMoveToPoint;
    return calculateEndPoint({ }, lastMoveToPoint);
}

void PathClosedArc::extendFastBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const
{
    arc.extendFastBoundingRect(currentPoint, lastMoveToPoint, boundingRect);
}

void PathClosedArc::extendBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const
{
    arc.extendBoundingRect(currentPoint, lastMoveToPoint, boundingRect);
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const PathClosedArc& data)
{
    ts << "add closed arc " << data.arc.center << " " << data.arc.radius << " " << data.arc.startAngle  << " " << data.arc.endAngle  << " " << data.arc.direction;
    return ts;
}

FloatPoint PathEllipse::calculateEndPoint(const FloatPoint&, FloatPoint& lastMoveToPoint) const
{
    lastMoveToPoint = center + FloatSize { radiusX * cos(startAngle), - radiusY * sin(startAngle) };
    auto endPoint = center + FloatSize { radiusX * cos(endAngle), - radiusY * sin(endAngle) };
    if (!rotation)
        return endPoint;

    auto rotation = AffineTransform::makeRotation(deg2rad(this->rotation));
    lastMoveToPoint = rotation.mapPoint(lastMoveToPoint);
    return rotation.mapPoint(endPoint);
}

std::optional<FloatPoint> PathEllipse::tryGetEndPointWithoutContext() const
{
    FloatPoint lastMoveToPoint;
    return calculateEndPoint({ }, lastMoveToPoint);
}

void PathEllipse::extendFastBoundingRect(const FloatPoint&, const FloatPoint&, FloatRect& boundingRect) const
{
    auto minXMinYCorner = center - FloatSize { radiusX, radiusY };
    auto maxXMaxYCorner = center + FloatSize { radiusX, radiusY };

    if (!rotation) {
        boundingRect.extend(minXMinYCorner);
        boundingRect.extend(maxXMaxYCorner);
        return;
    }

    auto rect = FloatRect { minXMinYCorner, maxXMaxYCorner };
    auto rotation = AffineTransform::makeRotation(deg2rad(this->rotation));
    rect = rotation.mapRect(rect);

    boundingRect.extend(rect.minXMinYCorner());
    boundingRect.extend(rect.maxXMaxYCorner());
}

void PathEllipse::extendBoundingRect(const FloatPoint& currentPoint, const FloatPoint&, FloatRect& boundingRect) const
{
    // FIXME: Implement this function.
    boundingRect.extend(currentPoint);
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const PathEllipse& data)
{
    ts << "add ellipse " << data.center << " " << data.radiusX << " " << data.radiusY  << " " << data.rotation << " " << data.startAngle  << " " << data.endAngle  << " " << data.direction;
    return ts;
}

FloatPoint PathEllipseInRect::calculateEndPoint(const FloatPoint&, FloatPoint& lastMoveToPoint) const
{
    lastMoveToPoint = rect.center() + FloatSize { rect.width() / 2, 0 };
    return lastMoveToPoint;
}

std::optional<FloatPoint> PathEllipseInRect::tryGetEndPointWithoutContext() const
{
    FloatPoint lastMoveToPoint;
    return calculateEndPoint({ }, lastMoveToPoint);
}

void PathEllipseInRect::extendFastBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const
{
    extendBoundingRect(currentPoint, lastMoveToPoint, boundingRect);
}

void PathEllipseInRect::extendBoundingRect(const FloatPoint&, const FloatPoint&, FloatRect& boundingRect) const
{
    boundingRect.extend(rect.minXMinYCorner());
    boundingRect.extend(rect.maxXMaxYCorner());
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const PathEllipseInRect& data)
{
    ts << "add ellipse in rect " << data.rect;
    return ts;
}

FloatPoint PathRect::calculateEndPoint(const FloatPoint&, FloatPoint& lastMoveToPoint) const
{
    lastMoveToPoint = rect.location();
    return lastMoveToPoint;
}

std::optional<FloatPoint> PathRect::tryGetEndPointWithoutContext() const
{
    FloatPoint lastMoveToPoint;
    return calculateEndPoint({ }, lastMoveToPoint);
}

void PathRect::extendFastBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const
{
    extendBoundingRect(currentPoint, lastMoveToPoint, boundingRect);
}

void PathRect::extendBoundingRect(const FloatPoint&, const FloatPoint&, FloatRect& boundingRect) const
{
    boundingRect.extend(rect.minXMinYCorner());
    boundingRect.extend(rect.maxXMaxYCorner());
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const PathRect& data)
{
    ts << "add rect " << data.rect;
    return ts;
}

FloatPoint PathRoundedRect::calculateEndPoint(const FloatPoint&, FloatPoint& lastMoveToPoint) const
{
    lastMoveToPoint = roundedRect.rect().location();
    return lastMoveToPoint;
}

std::optional<FloatPoint> PathRoundedRect::tryGetEndPointWithoutContext() const
{
    FloatPoint lastMoveToPoint;
    return calculateEndPoint({ }, lastMoveToPoint);
}

void PathRoundedRect::extendFastBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const
{
    extendBoundingRect(currentPoint, lastMoveToPoint, boundingRect);
}

void PathRoundedRect::extendBoundingRect(const FloatPoint&, const FloatPoint&, FloatRect& boundingRect) const
{
    boundingRect.extend(roundedRect.rect().minXMinYCorner());
    boundingRect.extend(roundedRect.rect().maxXMaxYCorner());
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const PathRoundedRect& data)
{
    ts << "add rounded rect " << data.roundedRect;
    return ts;
}

FloatPoint PathDataLine::calculateEndPoint(const FloatPoint&, FloatPoint& lastMoveToPoint) const
{
    lastMoveToPoint = start;
    return end;
}

std::optional<FloatPoint> PathDataLine::tryGetEndPointWithoutContext() const
{
    FloatPoint lastMoveToPoint;
    return calculateEndPoint({ }, lastMoveToPoint);
}

void PathDataLine::extendFastBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const
{
    extendBoundingRect(currentPoint, lastMoveToPoint, boundingRect);
}

void PathDataLine::extendBoundingRect(const FloatPoint&, const FloatPoint&, FloatRect& boundingRect) const
{
    boundingRect.extend(start);
    boundingRect.extend(end);
}

void PathDataLine::applyElements(const PathElementApplier& applier) const
{
    applier({ PathElement::Type::MoveToPoint, { start } });
    applier({ PathElement::Type::AddLineToPoint, { end } });
}

void PathDataLine::transform(const AffineTransform& transform)
{
    start = transform.mapPoint(start);
    end = transform.mapPoint(end);
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const PathDataLine& data)
{
    ts << "move to " << data.start;
    ts << ", ";
    ts << "add line to " << data.end;
    return ts;
}

FloatPoint PathDataQuadCurve::calculateEndPoint(const FloatPoint&, FloatPoint& lastMoveToPoint) const
{
    lastMoveToPoint = start;
    return endPoint;
}

std::optional<FloatPoint> PathDataQuadCurve::tryGetEndPointWithoutContext() const
{
    FloatPoint lastMoveToPoint;
    return calculateEndPoint({ }, lastMoveToPoint);
}

void PathDataQuadCurve::extendFastBoundingRect(const FloatPoint&, const FloatPoint&, FloatRect& boundingRect) const
{
    boundingRect.extend(start);
    boundingRect.extend(controlPoint);
    boundingRect.extend(endPoint);
}

void PathDataQuadCurve::extendBoundingRect(const FloatPoint&, const FloatPoint&, FloatRect& boundingRect) const
{
    auto extremity = calculateQuadraticExtremity(start, controlPoint, endPoint);
    boundingRect.extend(start);
    boundingRect.extend(extremity);
    boundingRect.extend(endPoint);
}

void PathDataQuadCurve::applyElements(const PathElementApplier& applier) const
{
    applier({ PathElement::Type::MoveToPoint, { start } });
    applier({ PathElement::Type::AddQuadCurveToPoint, { controlPoint, endPoint } });
}

void PathDataQuadCurve::transform(const AffineTransform& transform)
{
    start = transform.mapPoint(start);
    controlPoint = transform.mapPoint(controlPoint);
    endPoint = transform.mapPoint(endPoint);
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const PathDataQuadCurve& data)
{
    ts << "move to " << data.start;
    ts << ", ";
    ts << "add quad curve to " << data.controlPoint << " " << data.endPoint;
    return ts;
}

FloatPoint PathDataBezierCurve::calculateEndPoint(const FloatPoint&, FloatPoint& lastMoveToPoint) const
{
    lastMoveToPoint = start;
    return endPoint;
}

std::optional<FloatPoint> PathDataBezierCurve::tryGetEndPointWithoutContext() const
{
    FloatPoint lastMoveToPoint;
    return calculateEndPoint({ }, lastMoveToPoint);
}

void PathDataBezierCurve::extendFastBoundingRect(const FloatPoint&, const FloatPoint&, FloatRect& boundingRect) const
{
    boundingRect.extend(start);
    boundingRect.extend(controlPoint1);
    boundingRect.extend(controlPoint2);
    boundingRect.extend(endPoint);
}

void PathDataBezierCurve::extendBoundingRect(const FloatPoint&, const FloatPoint&, FloatRect& boundingRect) const
{
    auto bezierExtremities = calculateBezierExtremities(start, controlPoint1, controlPoint2, endPoint);
    boundingRect.extend(start);
    boundingRect.extend(bezierExtremities.first);
    boundingRect.extend(bezierExtremities.second);
    boundingRect.extend(endPoint);
}

void PathDataBezierCurve::applyElements(const PathElementApplier& applier) const
{
    applier({ PathElement::Type::MoveToPoint, { start } });
    applier({ PathElement::Type::AddCurveToPoint, { controlPoint1, controlPoint2, endPoint } });
}

void PathDataBezierCurve::transform(const AffineTransform& transform)
{
    start = transform.mapPoint(start);
    controlPoint1 = transform.mapPoint(controlPoint1);
    controlPoint2 = transform.mapPoint(controlPoint2);
    endPoint = transform.mapPoint(endPoint);
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const PathDataBezierCurve& data)
{
    ts << "move to " << data.start;
    ts << ", ";
    ts << "add curve to " << data.controlPoint1 << " " << data.controlPoint2 << " " << data.endPoint;
    return ts;
}

FloatPoint PathDataArc::calculateEndPoint(const FloatPoint&, FloatPoint& lastMoveToPoint) const
{
    lastMoveToPoint = start;
    return calculateArcToEndPoint(start, controlPoint1, controlPoint2, radius);
}

std::optional<FloatPoint> PathDataArc::tryGetEndPointWithoutContext() const
{
    FloatPoint lastMoveToPoint;
    return calculateEndPoint({ }, lastMoveToPoint);
}

void PathDataArc::extendFastBoundingRect(const FloatPoint&, const FloatPoint&, FloatRect& boundingRect) const
{
    boundingRect.extend(start);
    boundingRect.extend(controlPoint1);
    boundingRect.extend(controlPoint2);
}

void PathDataArc::extendBoundingRect(const FloatPoint&, const FloatPoint&, FloatRect& boundingRect) const
{
    boundingRect.extend(start);
    boundingRect.extend(controlPoint1);
    boundingRect.extend(calculateArcToEndPoint(start, controlPoint1, controlPoint2, radius));
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const PathDataArc& data)
{
    ts << "move to " << data.start;
    ts << ", ";
    ts << "add arc to " << data.controlPoint1 << " " << data.controlPoint2 << " " << data.radius;
    return ts;
}

FloatPoint PathCloseSubpath::calculateEndPoint(const FloatPoint&, FloatPoint& lastMoveToPoint) const
{
    return lastMoveToPoint;
}

std::optional<FloatPoint> PathCloseSubpath::tryGetEndPointWithoutContext() const
{
    return std::nullopt;
}

void PathCloseSubpath::extendFastBoundingRect(const FloatPoint&, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const
{
    boundingRect.extend(lastMoveToPoint);
}

void PathCloseSubpath::extendBoundingRect(const FloatPoint&, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const
{
    boundingRect.extend(lastMoveToPoint);
}

void PathCloseSubpath::applyElements(const PathElementApplier& applier) const
{
    applier({ PathElement::Type::CloseSubpath, { } });
}

void PathCloseSubpath::transform(const AffineTransform&)
{
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const PathCloseSubpath&)
{
    ts << "close subpath";
    return ts;
}

} // namespace WebCore
