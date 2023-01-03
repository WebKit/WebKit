/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(INLINE_PATH_DATA)

#include "FloatPoint.h"
#include <variant>

namespace WebCore {

struct LineData {
    FloatPoint start;
    FloatPoint end;
};

struct ArcData {
    enum class Type : uint8_t {
        ArcOnly,
        LineAndArc,
        ClosedLineAndArc
    };

    FloatPoint start;
    FloatPoint center;
    float radius { 0 };
    float startAngle { 0 };
    float endAngle { 0 };
    bool clockwise { false };
    Type type { Type::ArcOnly };
};

struct MoveData {
    FloatPoint location;
};

struct QuadCurveData {
    FloatPoint startPoint;
    FloatPoint controlPoint;
    FloatPoint endPoint;
};

struct BezierCurveData {
    FloatPoint startPoint;
    FloatPoint controlPoint1;
    FloatPoint controlPoint2;
    FloatPoint endPoint;
};

using InlinePathData = std::variant<std::monostate, MoveData, LineData, ArcData, QuadCurveData, BezierCurveData>;

} // namespace WebCore

#endif // ENABLE(INLINE_PATH_DATA)
