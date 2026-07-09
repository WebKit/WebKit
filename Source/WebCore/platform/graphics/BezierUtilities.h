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
#include <array>
#include <utility>
#include <wtf/Vector.h>

namespace WebCore {

// A cubic Bézier curve segment, ordered { start, control1, control2, end }
using CubicBezier = std::array<FloatPoint, 4>;

struct BezierIntersection {
    float parameterOnFirst { 0 };
    float parameterOnSecond { 0 };
    FloatPoint point;
};

// TODO: implement.
Vector<BezierIntersection> intersectBezierAndLine(const CubicBezier&, const FloatPoint& lineStart, const FloatPoint& lineEnd);

// Splits `curve` at `parameter` (a fraction in [0, 1]) into two sub-curves that together retrace
// the original, using de Casteljau subdivision.
// TODO: implement.
std::pair<CubicBezier, CubicBezier> splitBezier(const CubicBezier& curve, float parameter);

} // namespace WebCore
