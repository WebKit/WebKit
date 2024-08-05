/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include "BasicShapes.h"
#include "LengthPoint.h"
#include "LengthSize.h"
#include "RotationDirection.h"

namespace WTF {
class TextStream;
}

namespace WebCore {

using CoordinatePair = LengthPoint;

class ShapeSegmentBase {
public:
    explicit ShapeSegmentBase(CoordinateAffinity affinity)
        : m_affinity(affinity)
    { }

    bool operator==(const ShapeSegmentBase&) const = default;

    CoordinateAffinity affinity() const { return m_affinity; }

private:
    const CoordinateAffinity m_affinity;
};

class ShapeMoveSegment final : public ShapeSegmentBase {
public:
    ShapeMoveSegment(CoordinateAffinity affinity, CoordinatePair&& offset)
        : ShapeSegmentBase(affinity)
        , m_offset(WTFMove(offset))
    {
    }

    const CoordinatePair& offset() const { return m_offset; }

    bool operator==(const ShapeMoveSegment&) const = default;

private:
    CoordinatePair m_offset;
};

class ShapeLineSegment final : public ShapeSegmentBase {
public:
    ShapeLineSegment(CoordinateAffinity affinity, CoordinatePair&& offset)
        : ShapeSegmentBase(affinity)
        , m_offset(WTFMove(offset))
    {
    }

    const CoordinatePair& offset() const { return m_offset; }

    bool operator==(const ShapeLineSegment&) const = default;

private:
    CoordinatePair m_offset;
};

class ShapeHorizontalLineSegment final : public ShapeSegmentBase {
public:
    ShapeHorizontalLineSegment(CoordinateAffinity affinity, Length&& length)
        : ShapeSegmentBase(affinity)
        , m_length(WTFMove(length))
    {
    }

    Length length() const { return m_length; }

    bool operator==(const ShapeHorizontalLineSegment&) const = default;

private:
    Length m_length;
};

class ShapeVerticalLineSegment final : public ShapeSegmentBase {
public:
    ShapeVerticalLineSegment(CoordinateAffinity affinity, Length&& length)
        : ShapeSegmentBase(affinity)
        , m_length(WTFMove(length))
    {
    }

    Length length() const { return m_length; }

    bool operator==(const ShapeVerticalLineSegment&) const = default;

private:
    Length m_length;
};

class ShapeCurveSegment final : public ShapeSegmentBase {
public:
    ShapeCurveSegment(CoordinateAffinity affinity, CoordinatePair&& offset, CoordinatePair&& controlPoint1, std::optional<CoordinatePair>&& controlPoint2 = std::nullopt)
        : ShapeSegmentBase(affinity)
        , m_offset(WTFMove(offset))
        , m_controlPoint1(WTFMove(controlPoint1))
        , m_controlPoint2(WTFMove(controlPoint2))
    {
    }

    const CoordinatePair& offset() const { return m_offset; }
    const CoordinatePair& controlPoint1() const { return m_controlPoint1; }
    const std::optional<CoordinatePair>& controlPoint2() const { return m_controlPoint2; };

    bool operator==(const ShapeCurveSegment&) const = default;

private:
    CoordinatePair m_offset;
    CoordinatePair m_controlPoint1;
    std::optional<CoordinatePair> m_controlPoint2;
};

class ShapeSmoothSegment final : public ShapeSegmentBase {
public:
    ShapeSmoothSegment(CoordinateAffinity affinity, CoordinatePair&& offset, std::optional<CoordinatePair>&& intermediatePoint = std::nullopt)
        : ShapeSegmentBase(affinity)
        , m_offset(WTFMove(offset))
        , m_intermediatePoint(WTFMove(intermediatePoint))
    {
    }

    const CoordinatePair& offset() const { return m_offset; }
    const std::optional<CoordinatePair>& intermediatePoint() const { return m_intermediatePoint; };

    bool operator==(const ShapeSmoothSegment&) const = default;

private:
    CoordinatePair m_offset;
    std::optional<CoordinatePair> m_intermediatePoint;
};

class ShapeArcSegment final : public ShapeSegmentBase {
public:
    enum class ArcSize : uint8_t { Small, Large };
    using AngleDegrees = double;

    ShapeArcSegment(CoordinateAffinity affinity, CoordinatePair&& offset, LengthSize&& ellipseSize, RotationDirection sweep, ArcSize arcSize, AngleDegrees angle)
        : ShapeSegmentBase(affinity)
        , m_offset(WTFMove(offset))
        , m_ellipseSize(WTFMove(ellipseSize))
        , m_arcSweep(sweep)
        , m_arcSize(arcSize)
        , m_angle(angle)
    {
    }

    const CoordinatePair& offset() const { return m_offset; }
    const LengthSize& ellipseSize() const { return m_ellipseSize; }
    RotationDirection sweep() const { return m_arcSweep; }
    ArcSize arcSize() const { return m_arcSize; }
    AngleDegrees angle() const { return m_angle; }

    bool operator==(const ShapeArcSegment&) const = default;

private:
    CoordinatePair m_offset;
    LengthSize m_ellipseSize;
    RotationDirection m_arcSweep { RotationDirection::Counterclockwise };
    ArcSize m_arcSize { ArcSize::Small };
    AngleDegrees m_angle { 0 };
};

class ShapeCloseSegment {
public:
    bool operator==(const ShapeCloseSegment&) const = default;
};

// https://drafts.csswg.org/css-shapes-2/#shape-function
class BasicShapeShape final : public BasicShape {
public:
    Ref<BasicShape> clone() const final;

    Type type() const final { return Type::Shape; }
    WindRule windRule() const final { return m_windRule; }

    const CoordinatePair& startPoint() const { return m_startPoint; }

    Path path(const FloatRect&) const final;

    bool canBlend(const BasicShape&) const final;
    Ref<BasicShape> blend(const BasicShape& from, const BlendingContext&) const final;

    bool operator==(const BasicShape&) const final;

    void dump(TextStream&) const final;

    using ShapeSegment = std::variant<
        ShapeMoveSegment,
        ShapeLineSegment,
        ShapeHorizontalLineSegment,
        ShapeVerticalLineSegment,
        ShapeCurveSegment,
        ShapeSmoothSegment,
        ShapeArcSegment,
        ShapeCloseSegment
    >;

    const Vector<ShapeSegment>& segments() const { return m_segments; }

    static Ref<BasicShapeShape> create(WindRule, const CoordinatePair&, Vector<ShapeSegment>&&);

private:
    BasicShapeShape(WindRule, const CoordinatePair&, Vector<ShapeSegment>&&);

    CoordinatePair m_startPoint;
    const WindRule m_windRule { WindRule::NonZero };
    Vector<ShapeSegment> m_segments;
};

WTF::TextStream& operator<<(WTF::TextStream&, const BasicShapeShape::ShapeSegment&);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BASIC_SHAPE(BasicShapeShape, BasicShape::Type::Shape)
