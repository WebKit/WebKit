/*
 * Copyright (C) 2022 Noam Rosenthal All rights reserved.
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "BasicShapesShape.h"
#include "CSSPrimitiveValue.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include <wtf/Ref.h>
#include <wtf/TypeCasts.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace Style {
class BuilderState;
}

class CSSToLengthConversionData;

enum class CoordinateAffinity : uint8_t;

struct ControlPointValue {
    Ref<CSSValue> coordinatePair; // This is a CSSValuePair.
    std::optional<ControlPointAnchoring> anchoring;

    bool operator==(const ControlPointValue& other) const
    {
        return compareCSSValue(coordinatePair, other.coordinatePair) && anchoring == other.anchoring;
    }

    String cssText() const;
};

class CSSShapeSegmentValue : public CSSValue {
public:
    enum class SegmentType : uint8_t {
        Move,
        Line,
        HorizontalLine,
        VerticalLine,
        CubicCurve,
        QuadraticCurve,
        SmoothCubicCurve,
        SmoothQuadraticCurve,
        Arc,
        Close
    };

    SegmentType type() const { return m_type; }

    bool equals(const CSSShapeSegmentValue& other) const;
    String customCSSText() const;

    static Ref<CSSShapeSegmentValue> createClose();
    static Ref<CSSShapeSegmentValue> createMove(CoordinateAffinity, Ref<CSSValue>&& offset);
    static Ref<CSSShapeSegmentValue> createLine(CoordinateAffinity, Ref<CSSValue>&& offset);
    static Ref<CSSShapeSegmentValue> createHorizontalLine(CoordinateAffinity, Ref<CSSValue>&& length);
    static Ref<CSSShapeSegmentValue> createVerticalLine(CoordinateAffinity, Ref<CSSValue>&& length);

    static Ref<CSSShapeSegmentValue> createCubicCurve(CoordinateAffinity, Ref<CSSValue>&& offset, ControlPointValue&& p1, ControlPointValue&& p2);
    static Ref<CSSShapeSegmentValue> createQuadraticCurve(CoordinateAffinity, Ref<CSSValue>&& offset, ControlPointValue&& p1);

    static Ref<CSSShapeSegmentValue> createSmoothCubicCurve(CoordinateAffinity, Ref<CSSValue>&& offset, ControlPointValue&& p1);
    static Ref<CSSShapeSegmentValue> createSmoothQuadraticCurve(CoordinateAffinity, Ref<CSSValue>&& offset);

    static Ref<CSSShapeSegmentValue> createArc(CoordinateAffinity, Ref<CSSValue>&& offset, Ref<CSSValue>&& radius, CSSValueID sweep, CSSValueID size, Ref<CSSValue>&& angle);

    BasicShapeShape::ShapeSegment toShapeSegment(const Style::BuilderState&) const;

private:
    enum class SegmentDataType : uint8_t { Base, OnePoint, TwoPoint, Arc };

    struct ShapeSegmentData {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;

        const SegmentDataType type;
        const CoordinateAffinity affinity;
        Ref<CSSValue> offset; // CSSValuePair for x,y offsets, or CSSPrimitiveValue for horizontal/vertical line segments.

        ShapeSegmentData(SegmentDataType dataType, CoordinateAffinity affinity, Ref<CSSValue>&& offset)
            : type(dataType)
            , affinity(affinity)
            , offset(WTFMove(offset))
        { }

        virtual ~ShapeSegmentData() = default;

        virtual bool operator==(const ShapeSegmentData& other) const
        {
            return type == other.type
                && affinity == other.affinity
                && compareCSSValue(offset, other.offset);
        }
    };

    struct OnePointData : public ShapeSegmentData {
        ControlPointValue p1;

        OnePointData(CoordinateAffinity affinity, Ref<CSSValue>&& offset, ControlPointValue&& p1)
            : ShapeSegmentData(SegmentDataType::OnePoint, affinity, WTFMove(offset))
            , p1(WTFMove(p1))
        { }

        bool operator==(const ShapeSegmentData& other) const override
        {
            if (!ShapeSegmentData::operator==(other))
                return false;

            auto& otherOnePoint = static_cast<const OnePointData&>(other);
            return p1 == otherOnePoint.p1;
        }
    };

    struct TwoPointData : public ShapeSegmentData {
        ControlPointValue p1;
        ControlPointValue p2;

        TwoPointData(CoordinateAffinity affinity, Ref<CSSValue>&& offset, ControlPointValue&& p1, ControlPointValue&& p2)
            : ShapeSegmentData(SegmentDataType::TwoPoint, affinity, WTFMove(offset))
            , p1(WTFMove(p1))
            , p2(WTFMove(p2))
        { }

        bool operator==(const ShapeSegmentData& other) const override
        {
            if (!ShapeSegmentData::operator==(other))
                return false;

            auto& otherTwoPoint = static_cast<const TwoPointData&>(other);
            return p1 == otherTwoPoint.p1 && p2 == otherTwoPoint.p2;
        }
    };

    struct ArcData : public ShapeSegmentData {
        CSSValueID arcSweep;
        CSSValueID arcSize;
        Ref<CSSValue> radius;
        Ref<CSSValue> angle;

        ArcData(CoordinateAffinity affinity, Ref<CSSValue>&& offset, Ref<CSSValue>&& radius, CSSValueID arcSweep, CSSValueID arcSize, Ref<CSSValue>&& angle)
            : ShapeSegmentData(SegmentDataType::Arc, affinity, WTFMove(offset))
            , arcSweep(arcSweep)
            , arcSize(arcSize)
            , radius(WTFMove(radius))
            , angle(WTFMove(angle))
        { }

        bool operator==(const ShapeSegmentData& other) const override
        {
            if (!ShapeSegmentData::operator==(other))
                return false;

            auto& otherArc = static_cast<const ArcData&>(other);
            return arcSweep == otherArc.arcSweep
                && arcSize == otherArc.arcSize
                && compareCSSValue(radius, otherArc.radius)
                && compareCSSValue(angle, otherArc.angle);
        }
    };

    static Ref<CSSShapeSegmentValue> create(SegmentType type, std::unique_ptr<ShapeSegmentData>&& data = nullptr)
    {
        return adoptRef(*new CSSShapeSegmentValue(type, WTFMove(data)));
    }

    CSSShapeSegmentValue(SegmentType type, std::unique_ptr<ShapeSegmentData>&& data)
        : CSSValue(ClassType::ShapeSegment)
        , m_type(type)
        , m_data(WTFMove(data))
    {
    }

    const SegmentType m_type;
    std::unique_ptr<ShapeSegmentData> m_data;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSShapeSegmentValue, isShapeSegment())
