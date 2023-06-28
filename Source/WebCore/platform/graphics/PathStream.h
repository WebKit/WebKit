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

#include "PathImpl.h"
#include "PathSegment.h"
#include <wtf/Vector.h>

namespace WebCore {

class PathStream final : public PathImpl {
public:
    static UniqueRef<PathStream> create();
    static UniqueRef<PathStream> create(Vector<PathSegment>&&);
    static UniqueRef<PathStream> create(const Vector<PathSegment>&);
    static UniqueRef<PathStream> create(const Vector<FloatPoint>&);

    PathStream() = default;
    PathStream(Vector<PathSegment>&&);
    PathStream(const Vector<PathSegment>&);

    UniqueRef<PathImpl> clone() const final;

    bool operator==(const PathImpl& other) const final;

    void moveTo(const FloatPoint&) final;

    void addLineTo(const FloatPoint&) final;
    void addQuadCurveTo(const FloatPoint& controlPoint, const FloatPoint& endPoint) final;
    void addBezierCurveTo(const FloatPoint& controlPoint1, const FloatPoint& controlPoint2, const FloatPoint& endPoint) final;
    void addArcTo(const FloatPoint& point1, const FloatPoint& point2, float radius) final;

    void addArc(const FloatPoint&, float radius, float startAngle, float endAngle, RotationDirection) final;
    void addEllipse(const FloatPoint&, float radiusX, float radiusY, float rotation, float startAngle, float endAngle, RotationDirection) final;
    void addEllipseInRect(const FloatRect&) final;
    void addRect(const FloatRect&) final;
    void addRoundedRect(const FloatRoundedRect&, PathRoundedRect::Strategy) final;

    void closeSubpath() final;

    WEBCORE_EXPORT const Vector<PathSegment>& segments() const;

    void applySegments(const PathSegmentApplier&) const final;
    void applyElements(const PathElementApplier&) const final;

    FloatRect fastBoundingRect() const final;
    FloatRect boundingRect() const final;

private:
    template<typename DataType1, typename DataType2>
    bool mergeIntoComposite(const DataType2&);

    bool isPathStream() const final { return true; }

    template<typename DataType>
    std::optional<DataType> singleDataType() const;

    std::optional<PathSegment> singleSegment() const final;
    std::optional<PathDataLine> singleDataLine() const final;
    std::optional<PathArc> singleArc() const final;
    std::optional<PathDataQuadCurve> singleQuadCurve() const final;
    std::optional<PathDataBezierCurve> singleBezierCurve() const final;

    bool isEmpty() const final { return m_segments.isEmpty(); }

    bool isClosed() const final;
    FloatPoint currentPoint() const final;

    Vector<PathSegment> m_segments;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::PathStream)
    static bool isType(const WebCore::PathImpl& pathImpl) { return pathImpl.isPathStream(); }
SPECIALIZE_TYPE_TRAITS_END()
