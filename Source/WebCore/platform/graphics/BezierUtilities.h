/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/FloatPoint.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FloatSize.h>
#include <wtf/Vector.h>

namespace WebCore {

class Path;

struct BezierSegment {
    FloatPoint start;
    FloatPoint controlPoint1;
    FloatPoint controlPoint2;
    FloatPoint end;
};

WEBCORE_EXPORT Vector<BezierSegment> trimBezierToRect(const BezierSegment& curve, const FloatRect&);
// `parameter` is the Bézier curve parameter t in [0, 1]: 0 is the start point, 1 is the end point (not arc length).
FloatPoint pointOnBezierAtParameter(const BezierSegment& curve, double parameter);
Vector<FloatPoint> resampleByArcLength(const Vector<FloatPoint>& polyline, unsigned count);
// `interpolationFraction` blends from the start curve (0) to the end curve (1); it is a shape-morph
// fraction between two whole curves, not a position along a single curve.
Vector<FloatPoint> hermiteInterpolate(const Vector<FloatPoint>& startPoints, const Vector<FloatSize>& startVelocities, const Vector<FloatPoint>& endPoints, const Vector<FloatSize>& endVelocities, double interpolationFraction);

void addCatmullRomBeziers(Path&, const Vector<FloatPoint>& points, unsigned segmentCount, bool& started);

} // namespace WebCore
