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

struct BezierSegment {
    FloatPoint start;
    FloatPoint controlPoint1;
    FloatPoint controlPoint2;
    FloatPoint end;
};

struct BezierIntersection {
    double parameterOnFirst { 0 };
    double parameterOnSecond { 0 };
    FloatPoint point;
    // Touching without crossing
    bool tangential { false };
};

// A range over which two edges lie on top of one another
struct BezierCoincidentSpan {
    double firstStart { 0 };
    double firstEnd { 0 };
    double secondStart { 0 };
    double secondEnd { 0 };
    bool opposedDirection { false };
};

struct BezierIntersections {
    // Two cubics cross at most nine times.
    Vector<BezierIntersection, 9> crossings;
    Vector<BezierCoincidentSpan, 2> coincidences;
};

WEBCORE_EXPORT BezierIntersections intersectBeziers(const BezierSegment& first, const BezierSegment& second);
WEBCORE_EXPORT Vector<BezierIntersection, 3> intersectBezierAndLine(const BezierSegment&, const FloatPoint& lineStart, const FloatPoint& lineEnd);
WEBCORE_EXPORT Vector<BezierSegment> splitBezierAtParameters(const BezierSegment&, const Vector<double>& parameters);

WEBCORE_EXPORT Vector<BezierSegment> trimBezierToRect(const BezierSegment& curve, const FloatRect&);

using BezierLoop = Vector<BezierSegment>;

// Non-zero winding number of `loop` around `point`, for classifying which side of a loop a piece lies on.
WEBCORE_EXPORT int windingNumberForLoop(const BezierLoop&, const FloatPoint&);

// Returns the loops bounding what is left of `region` once `sliver` is taken out of it, or none if the sliver covered it entirely.
WEBCORE_EXPORT Vector<BezierLoop> subtractLoopFromLoop(const BezierLoop& region, const BezierLoop& sliver);
// `parameter` is the Bézier curve parameter t in [0, 1]: 0 is the start point, 1 is the end point (not arc length).
FloatPoint pointOnBezierAtParameter(const BezierSegment& curve, double parameter);
WEBCORE_EXPORT FloatSize tangentOnBezierAtParameter(const BezierSegment& curve, double parameter);

} // namespace WebCore
