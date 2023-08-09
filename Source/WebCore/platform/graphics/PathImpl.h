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

#pragma once

#include "FloatRoundedRect.h"
#include "PathElement.h"
#include "PathSegment.h"
#include <wtf/FastMalloc.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class PathImpl {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~PathImpl() = default;

    static constexpr float circleControlPoint()
    {
        // Approximation of control point positions on a bezier to simulate a quarter of a circle.
        // This is 1-kappa, where kappa = 4 * (sqrt(2) - 1) / 3
        return 0.447715;
    }

    virtual bool isPathStream() const { return false; }

    virtual UniqueRef<PathImpl> clone() const = 0;

    virtual bool operator==(const PathImpl&) const = 0;

    virtual void moveTo(const FloatPoint&) = 0;

    virtual void addLineTo(const FloatPoint&) = 0;
    virtual void addQuadCurveTo(const FloatPoint& controlPoint, const FloatPoint& endPoint) = 0;
    virtual void addBezierCurveTo(const FloatPoint& controlPoint1, const FloatPoint& controlPoint2, const FloatPoint& endPoint) = 0;
    virtual void addArcTo(const FloatPoint& point1, const FloatPoint& point2, float radius) = 0;

    virtual void addArc(const FloatPoint&, float radius, float startAngle, float endAngle, RotationDirection) = 0;
    virtual void addEllipse(const FloatPoint&, float radiusX, float radiusY, float rotation, float startAngle, float endAngle, RotationDirection) = 0;
    virtual void addEllipseInRect(const FloatRect&) = 0;
    virtual void addRect(const FloatRect&) = 0;
    virtual void addRoundedRect(const FloatRoundedRect&, PathRoundedRect::Strategy) = 0;

    virtual void closeSubpath() = 0;

    void addLinesForRect(const FloatRect&);
    void addBeziersForRoundedRect(const FloatRoundedRect&);

    virtual void applySegments(const PathSegmentApplier&) const = 0;
    virtual bool applyElements(const PathElementApplier&) const = 0;

    virtual bool transform(const AffineTransform&) = 0;

    virtual std::optional<PathSegment> singleSegment() const { return std::nullopt; }
    virtual std::optional<PathDataLine> singleDataLine() const { return std::nullopt; }
    virtual std::optional<PathArc> singleArc() const { return std::nullopt; }
    virtual std::optional<PathDataQuadCurve> singleQuadCurve() const { return std::nullopt; }
    virtual std::optional<PathDataBezierCurve> singleBezierCurve() const { return std::nullopt; }

    virtual bool isEmpty() const = 0;

    virtual bool isClosed() const;

    virtual FloatPoint currentPoint() const = 0;

    virtual FloatRect fastBoundingRect() const = 0;
    virtual FloatRect boundingRect() const = 0;

protected:
    PathImpl() = default;

    void appendSegment(const PathSegment&);
};

} // namespace WebCore
