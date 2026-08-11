/*
 * Copyright (C) 2006, 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "FloatPoint.h"
#include "GeometryUtilities.h"
#include <cmath>
#include <optional>
#include <utility>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

namespace WebCore {

constexpr float kPathSegmentLengthTolerance = 0.00001f;

inline float NODELETE distanceLine(const FloatPoint& start, const FloatPoint& end)
{
    return std::hypot(end.x() - start.x(), end.y() - start.y());
}

struct QuadraticBezier {
    QuadraticBezier() = default;
    QuadraticBezier(const FloatPoint& s, const FloatPoint& c, const FloatPoint& e)
        : start(s)
        , control(c)
        , end(e)
    {
    }

    friend bool NODELETE operator==(const QuadraticBezier&, const QuadraticBezier&) = default;

    float NODELETE approximateDistance() const
    {
        return distanceLine(start, control) + distanceLine(control, end);
    }

    // std::nullopt when halving no longer makes progress in float arithmetic.
    std::optional<std::pair<QuadraticBezier, QuadraticBezier>> NODELETE split() const
    {
        QuadraticBezier left;
        QuadraticBezier right;

        left.control = midPoint(start, control);
        right.control = midPoint(control, end);

        FloatPoint leftControlToRightControl = midPoint(left.control, right.control);
        left.end = leftControlToRightControl;
        right.start = leftControlToRightControl;

        left.start = start;
        right.end = end;

        if (left == *this || right == *this)
            return std::nullopt;

        return std::pair { left, right };
    }

    FloatPoint start;
    FloatPoint control;
    FloatPoint end;
};

struct CubicBezier {
    CubicBezier() = default;
    CubicBezier(const FloatPoint& s, const FloatPoint& c1, const FloatPoint& c2, const FloatPoint& e)
        : start(s)
        , control1(c1)
        , control2(c2)
        , end(e)
    {
    }

    friend bool NODELETE operator==(const CubicBezier&, const CubicBezier&) = default;

    float NODELETE approximateDistance() const
    {
        return distanceLine(start, control1) + distanceLine(control1, control2) + distanceLine(control2, end);
    }

    // std::nullopt when halving no longer makes progress in float arithmetic.
    std::optional<std::pair<CubicBezier, CubicBezier>> NODELETE split() const
    {
        CubicBezier left;
        CubicBezier right;

        FloatPoint startToControl1 = midPoint(control1, control2);

        left.start = start;
        left.control1 = midPoint(start, control1);
        left.control2 = midPoint(left.control1, startToControl1);

        right.control2 = midPoint(control2, end);
        right.control1 = midPoint(right.control2, startToControl1);
        right.end = end;

        FloatPoint leftControl2ToRightControl1 = midPoint(left.control2, right.control1);
        left.end = leftControl2ToRightControl1;
        right.start = leftControl2ToRightControl1;

        if (left == *this || right == *this)
            return std::nullopt;

        return std::pair { left, right };
    }

    FloatPoint start;
    FloatPoint control1;
    FloatPoint control2;
    FloatPoint end;
};

template<class CurveType>
void forEachFlattenedCurveLeaf(const CurveType& originalCurve, NOESCAPE const Invocable<bool(const CurveType&, float, bool)> auto& processLeaf)
{
    static constexpr unsigned curveStackDepthLimit = 20;

    Vector<CurveType, curveStackDepthLimit + 1> curveStack;
    curveStack.append(originalCurve);

    while (!curveStack.isEmpty()) {
        auto curve = curveStack.takeLast();
        float length = curve.approximateDistance();

        if ((length - distanceLine(curve.start, curve.end)) > kPathSegmentLengthTolerance && curveStack.size() < curveStackDepthLimit) {
            if (auto halves = curve.split()) {
                curveStack.append(halves->second);
                curveStack.append(halves->first);
                continue;
            }
        }

        if (!processLeaf(curve, length, curveStack.isEmpty()))
            break;
    }
}

} // namespace WebCore
