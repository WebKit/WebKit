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

#include "FloatRect.h"
#include "FloatRoundedRect.h"
#include "PathElement.h"
#include "RotationDirection.h"

namespace WTF {
class TextStream;
}

namespace WebCore {

class PathImpl;

struct PathMoveTo {
    FloatPoint point;

    bool operator==(const PathMoveTo&) const = default;

    FloatPoint calculateEndPoint(const FloatPoint& currentPoint, FloatPoint& lastMoveToPoint) const;

    void extendFastBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const;
    void extendBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const;

    void addToImpl(PathImpl&) const;
    void applyElements(const PathElementApplier&) const;
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const PathMoveTo&);

struct PathLineTo {
    FloatPoint point;

    bool operator==(const PathLineTo&) const = default;

    FloatPoint calculateEndPoint(const FloatPoint& currentPoint, FloatPoint& lastMoveToPoint) const;

    void extendFastBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const;
    void extendBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const;

    void addToImpl(PathImpl&) const;
    void applyElements(const PathElementApplier&) const;
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const PathLineTo&);

struct PathQuadCurveTo {
    FloatPoint controlPoint;
    FloatPoint endPoint;

    bool operator==(const PathQuadCurveTo&) const = default;

    FloatPoint calculateEndPoint(const FloatPoint& currentPoint, FloatPoint& lastMoveToPoint) const;

    void extendFastBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const;
    void extendBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const;

    void addToImpl(PathImpl&) const;
    void applyElements(const PathElementApplier&) const;
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const PathQuadCurveTo&);

struct PathBezierCurveTo {
    FloatPoint controlPoint1;
    FloatPoint controlPoint2;
    FloatPoint endPoint;

    bool operator==(const PathBezierCurveTo&) const = default;

    FloatPoint calculateEndPoint(const FloatPoint& currentPoint, FloatPoint& lastMoveToPoint) const;

    void extendFastBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const;
    void extendBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const;

    void addToImpl(PathImpl&) const;
    void applyElements(const PathElementApplier&) const;
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const PathBezierCurveTo&);

struct PathArcTo {
    FloatPoint controlPoint1;
    FloatPoint controlPoint2;
    float radius;

    bool operator==(const PathArcTo&) const = default;

    FloatPoint calculateEndPoint(const FloatPoint& currentPoint) const;
    FloatPoint calculateEndPoint(const FloatPoint& currentPoint, FloatPoint& lastMoveToPoint) const;

    void extendFastBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const;
    void extendBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const;

    void addToImpl(PathImpl&) const;
    [[noreturn]] void applyElements(const PathElementApplier&) const { RELEASE_ASSERT_NOT_REACHED(); }
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const PathArcTo&);

struct PathArc {
    FloatPoint center;
    float radius;
    float startAngle;
    float endAngle;
    RotationDirection direction;

    bool operator==(const PathArc&) const = default;

    FloatPoint calculateEndPoint(const FloatPoint& currentPoint, FloatPoint& lastMoveToPoint) const;

    void extendFastBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const;
    void extendBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const;

    void addToImpl(PathImpl&) const;
    [[noreturn]] void applyElements(const PathElementApplier&) const { RELEASE_ASSERT_NOT_REACHED(); }
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const PathArc&);

struct PathEllipse {
    FloatPoint center;
    float radiusX;
    float radiusY;
    float rotation;
    float startAngle;
    float endAngle;
    RotationDirection direction;

    bool operator==(const PathEllipse&) const = default;

    FloatPoint calculateEndPoint(const FloatPoint& currentPoint, FloatPoint& lastMoveToPoint) const;

    void extendFastBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const;
    void extendBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const;

    void addToImpl(PathImpl&) const;
    [[noreturn]] void applyElements(const PathElementApplier&) const { RELEASE_ASSERT_NOT_REACHED(); }
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const PathEllipse&);

struct PathEllipseInRect {
    FloatRect rect;

    bool operator==(const PathEllipseInRect&) const = default;

    FloatPoint calculateEndPoint(const FloatPoint& currentPoint, FloatPoint& lastMoveToPoint) const;

    void extendFastBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const;
    void extendBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const;

    void addToImpl(PathImpl&) const;
    [[noreturn]] void applyElements(const PathElementApplier&) const { RELEASE_ASSERT_NOT_REACHED(); }
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const PathEllipseInRect&);

struct PathRect {
    FloatRect rect;

    bool operator==(const PathRect&) const = default;

    FloatPoint calculateEndPoint(const FloatPoint& currentPoint, FloatPoint& lastMoveToPoint) const;

    void extendFastBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const;
    void extendBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const;

    void addToImpl(PathImpl&) const;
    [[noreturn]] void applyElements(const PathElementApplier&) const { RELEASE_ASSERT_NOT_REACHED(); }
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const PathRect&);

struct PathRoundedRect {
    enum class Strategy : uint8_t {
        PreferNative,
        PreferBezier
    };

    FloatRoundedRect roundedRect;
    Strategy strategy;

    bool operator==(const PathRoundedRect&) const = default;

    FloatPoint calculateEndPoint(const FloatPoint& currentPoint, FloatPoint& lastMoveToPoint) const;

    void extendFastBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const;
    void extendBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const;

    void addToImpl(PathImpl&) const;
    [[noreturn]] void applyElements(const PathElementApplier&) const { RELEASE_ASSERT_NOT_REACHED(); }
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const PathRoundedRect&);

template<typename DataType1, typename DataType2>
struct PathDataComposite {
    DataType1 data1;
    DataType2 data2;

    bool operator==(const PathDataComposite&) const = default;

    FloatPoint calculateEndPoint(const FloatPoint& currentPoint, FloatPoint& lastMoveToPoint) const;

    void extendFastBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const;
    void extendBoundingRect(const FloatPoint& currentPoint, const FloatPoint& lastMoveToPoint, FloatRect& boundingRect) const;

    void addToImpl(PathImpl&) const;
    void applyElements(const PathElementApplier&) const;
};

template<typename DataType1, typename DataType2>
inline WTF::TextStream& operator<<(WTF::TextStream& ts, const PathDataComposite<DataType1, DataType2>& data)
{
    ts << data.data1;
    ts << ", ";
    ts << data.data2;
    return ts;
}

using PathDataLine        = PathDataComposite<PathMoveTo, PathLineTo>;
using PathDataQuadCurve   = PathDataComposite<PathMoveTo, PathQuadCurveTo>;
using PathDataBezierCurve = PathDataComposite<PathMoveTo, PathBezierCurveTo>;
using PathDataArc         = PathDataComposite<PathMoveTo, PathArcTo>;

} // namespace WebCore
