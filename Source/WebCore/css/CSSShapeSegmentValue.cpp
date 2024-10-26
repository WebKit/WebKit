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

#include "config.h"
#include "CSSShapeSegmentValue.h"

#include "BasicShapeConversion.h"
#include "BasicShapes.h"
#include "BasicShapesShapeSegmentConversion.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSValuePair.h"
#include "CalculationValue.h"
#include "StyleBuilderConverter.h"
#include "StyleBuilderState.h"
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

template<CSSValueID cssValueFor0, CSSValueID cssValueFor100>
Length positionComponentOrCoordinateToLength(const CSSValue& value, CoordinateAffinity affinity, const Style::BuilderState& builderState)
{
    if (affinity == CoordinateAffinity::Absolute)
        return Style::BuilderConverter::convertPositionComponent<cssValueFor0, cssValueFor100>(builderState, value);

    return convertToLength(builderState.cssToLengthConversionData(), value);
}

static ControlPoint controlPoint(const ControlPointValue& controlPointValue, CoordinateAffinity affinity, const Style::BuilderState& builderState)
{
    return ControlPoint {
        positionOrCoordinatePairToLengthPoint(controlPointValue.coordinatePair, affinity, builderState),
        controlPointValue.anchoring
    };
}

String ControlPointValue::cssText() const
{
    if (!anchoring)
        return coordinatePair->cssText();

    auto anchorString = [](ControlPointAnchoring anchoring) {
        switch (anchoring) {
        case ControlPointAnchoring::FromStart: return " from start"_s;
        case ControlPointAnchoring::FromEnd: return " from end"_s;
        case ControlPointAnchoring::FromOrigin: return " from origin"_s;
        }
        return ""_s;
    };
    return makeString(coordinatePair->cssText(), anchorString(*anchoring));
}

Ref<CSSShapeSegmentValue> CSSShapeSegmentValue::createMove(CoordinateAffinity affinity, Ref<CSSValue>&& offset)
{
    auto data = makeUnique<ShapeSegmentData>(SegmentDataType::Base, affinity, WTFMove(offset));
    return CSSShapeSegmentValue::create(SegmentType::Move, WTFMove(data));
}

Ref<CSSShapeSegmentValue> CSSShapeSegmentValue::createLine(CoordinateAffinity affinity, Ref<CSSValue>&& offset)
{
    auto data = makeUnique<ShapeSegmentData>(SegmentDataType::Base, affinity, WTFMove(offset));
    return CSSShapeSegmentValue::create(SegmentType::Line, WTFMove(data));
}

Ref<CSSShapeSegmentValue> CSSShapeSegmentValue::createHorizontalLine(CoordinateAffinity affinity, Ref<CSSValue>&& x)
{
    auto data = makeUnique<ShapeSegmentData>(SegmentDataType::Base, affinity, WTFMove(x));
    return CSSShapeSegmentValue::create(SegmentType::HorizontalLine, WTFMove(data));
}

Ref<CSSShapeSegmentValue> CSSShapeSegmentValue::createVerticalLine(CoordinateAffinity affinity, Ref<CSSValue>&& y)
{
    auto data = makeUnique<ShapeSegmentData>(SegmentDataType::Base, affinity, WTFMove(y));
    return CSSShapeSegmentValue::create(SegmentType::VerticalLine, WTFMove(data));
}

Ref<CSSShapeSegmentValue> CSSShapeSegmentValue::createCubicCurve(CoordinateAffinity affinity, Ref<CSSValue>&& offset, ControlPointValue&& p1, ControlPointValue&& p2)
{
    auto data = makeUnique<TwoPointData>(affinity, WTFMove(offset), WTFMove(p1), WTFMove(p2));
    return CSSShapeSegmentValue::create(SegmentType::CubicCurve, WTFMove(data));
}

Ref<CSSShapeSegmentValue> CSSShapeSegmentValue::createQuadraticCurve(CoordinateAffinity affinity, Ref<CSSValue>&& offset, ControlPointValue&& p1)
{
    auto data = makeUnique<OnePointData>(affinity, WTFMove(offset), WTFMove(p1));
    return CSSShapeSegmentValue::create(SegmentType::QuadraticCurve, WTFMove(data));
}

Ref<CSSShapeSegmentValue> CSSShapeSegmentValue::createSmoothCubicCurve(CoordinateAffinity affinity, Ref<CSSValue>&& offset, ControlPointValue&& p1)
{
    auto data = makeUnique<OnePointData>(affinity, WTFMove(offset), WTFMove(p1));
    return CSSShapeSegmentValue::create(SegmentType::SmoothCubicCurve, WTFMove(data));
}

Ref<CSSShapeSegmentValue> CSSShapeSegmentValue::createSmoothQuadraticCurve(CoordinateAffinity affinity, Ref<CSSValue>&& offset)
{
    auto data = makeUnique<ShapeSegmentData>(SegmentDataType::Base, affinity, WTFMove(offset));
    return CSSShapeSegmentValue::create(SegmentType::SmoothQuadraticCurve, WTFMove(data));
}

Ref<CSSShapeSegmentValue> CSSShapeSegmentValue::createArc(CoordinateAffinity affinity, Ref<CSSValue>&& offset, Ref<CSSValue>&& radius, CSSValueID sweep, CSSValueID size, Ref<CSSValue>&& angle)
{
    ASSERT(sweep == CSSValueCcw || sweep == CSSValueCw);
    ASSERT(size == CSSValueSmall || size == CSSValueLarge);

    auto data = makeUnique<ArcData>(affinity, WTFMove(offset), WTFMove(radius), sweep, size, WTFMove(angle));
    return CSSShapeSegmentValue::create(SegmentType::Arc, WTFMove(data));
}

Ref<CSSShapeSegmentValue> CSSShapeSegmentValue::createClose()
{
    return CSSShapeSegmentValue::create(SegmentType::Close);
}

bool CSSShapeSegmentValue::equals(const CSSShapeSegmentValue& other) const
{
    if (type() != other.type())
        return false;

    if (m_type == SegmentType::Close)
        return true;

    ASSERT(m_data && other.m_data);
    return *m_data == *other.m_data;
}

String CSSShapeSegmentValue::customCSSText() const
{
    if (m_type == SegmentType::Close)
        return "close"_s;

    ASSERT(m_data);

    const auto segmentName = [&]() {
        switch (type()) {
        case SegmentType::Move:
            return "move"_s;
        case SegmentType::Line:
            return "line"_s;
        case SegmentType::HorizontalLine:
            return "hline"_s;
        case SegmentType::VerticalLine:
            return "vline"_s;
        case SegmentType::CubicCurve:
        case SegmentType::QuadraticCurve:
            return "curve"_s;
        case SegmentType::SmoothCubicCurve:
        case SegmentType::SmoothQuadraticCurve:
            return "smooth"_s;
        case SegmentType::Arc:
            return "arc"_s;
        default:
            ASSERT_NOT_REACHED();
            return ""_s;
        }
    };

    const auto conjunction = [&]() {
        switch (type()) {
        case SegmentType::Move:
        case SegmentType::Line:
        case SegmentType::HorizontalLine:
        case SegmentType::VerticalLine:
        case SegmentType::SmoothQuadraticCurve:
            return ""_s;
        case SegmentType::CubicCurve:
        case SegmentType::QuadraticCurve:
        case SegmentType::SmoothCubicCurve:
            return " with "_s;
        case SegmentType::Arc:
            return " of "_s;
        default:
            ASSERT_NOT_REACHED();
            return ""_s;
        }
    };

    StringBuilder builder;
    auto byTo = m_data->affinity == CoordinateAffinity::Absolute ? " to "_s : " by "_s;
    builder.append(segmentName(), byTo, m_data->offset->cssText(), conjunction());

    switch (m_data->type) {
    case SegmentDataType::Base:
        break;
    case SegmentDataType::OnePoint: {
        auto& onePointData = static_cast<const OnePointData&>(*m_data);
        builder.append(onePointData.p1.cssText());
        break;
    }
    case SegmentDataType::TwoPoint: {
        auto& twoPointData = static_cast<const TwoPointData&>(*m_data);
        builder.append(twoPointData.p1.cssText(), " / "_s, twoPointData.p2.cssText());
        break;
    }
    case SegmentDataType::Arc: {
        auto& arcData = static_cast<const ArcData&>(*m_data);
        builder.append(arcData.radius->cssText());

        if (arcData.arcSweep == CSSValueCw)
            builder.append(" cw"_s);

        if (arcData.arcSize == CSSValueLarge)
            builder.append(" large"_s);

        if (RefPtr angleValue = dynamicDowncast<CSSPrimitiveValue>(arcData.angle)) {
            if (!angleValue->isZero().value_or(false))
                builder.append(" rotate "_s, angleValue->cssText());
        }
        break;
    }
    }

    return builder.toString();
}

BasicShapeShape::ShapeSegment CSSShapeSegmentValue::toShapeSegment(const Style::BuilderState& builderState) const
{
    auto toDegrees = [&](const CSSValue& value) {
        RefPtr angleValue = dynamicDowncast<CSSPrimitiveValue>(value);
        if (!angleValue || !angleValue->isAngle())
            return 0.0;

        return angleValue->resolveAsAngle(builderState.cssToLengthConversionData());
    };

    switch (type()) {
    case SegmentType::Move: {
        auto targetLengthPoint = positionOrCoordinatePairToLengthPoint(m_data->offset, m_data->affinity, builderState);
        return ShapeMoveSegment(m_data->affinity, WTFMove(targetLengthPoint));
    }
    case SegmentType::Line: {
        auto targetLengthPoint = positionOrCoordinatePairToLengthPoint(m_data->offset, m_data->affinity, builderState);
        return ShapeLineSegment(m_data->affinity, WTFMove(targetLengthPoint));
    }
    case SegmentType::HorizontalLine:
        return ShapeHorizontalLineSegment(m_data->affinity, positionComponentOrCoordinateToLength<CSSValueLeft, CSSValueRight>(m_data->offset, m_data->affinity, builderState));
    case SegmentType::VerticalLine:
        return ShapeVerticalLineSegment(m_data->affinity, positionComponentOrCoordinateToLength<CSSValueTop, CSSValueBottom>(m_data->offset, m_data->affinity, builderState));
    case SegmentType::CubicCurve: {
        auto& twoPointData = static_cast<const TwoPointData&>(*m_data);
        auto targetLengthPoint = positionOrCoordinatePairToLengthPoint(twoPointData.offset, twoPointData.affinity, builderState);
        return ShapeCurveSegment(twoPointData.affinity, WTFMove(targetLengthPoint), controlPoint(twoPointData.p1, twoPointData.affinity, builderState), controlPoint(twoPointData.p2, twoPointData.affinity, builderState));
    }
    case SegmentType::QuadraticCurve: {
        auto& onePointData = static_cast<const OnePointData&>(*m_data);
        auto targetLengthPoint = positionOrCoordinatePairToLengthPoint(onePointData.offset, onePointData.affinity, builderState);
        return ShapeCurveSegment(onePointData.affinity, WTFMove(targetLengthPoint), controlPoint(onePointData.p1, onePointData.affinity, builderState));
    }
    case SegmentType::SmoothCubicCurve: {
        auto& onePointData = static_cast<const OnePointData&>(*m_data);
        auto targetLengthPoint = positionOrCoordinatePairToLengthPoint(onePointData.offset, onePointData.affinity, builderState);
        return ShapeSmoothSegment(onePointData.affinity, WTFMove(targetLengthPoint), controlPoint(onePointData.p1, onePointData.affinity, builderState));
    }
    case SegmentType::SmoothQuadraticCurve: {
        auto targetLengthPoint = positionOrCoordinatePairToLengthPoint(m_data->offset, m_data->affinity, builderState);
        return ShapeSmoothSegment(m_data->affinity, WTFMove(targetLengthPoint));
    }
    case SegmentType::Arc: {
        auto& arcData = static_cast<const ArcData&>(*m_data);
        auto targetLengthPoint = positionOrCoordinatePairToLengthPoint(arcData.offset, arcData.affinity, builderState);
        auto arcSweep = arcData.arcSweep == CSSValueCcw ? RotationDirection::Counterclockwise : RotationDirection::Clockwise;
        auto arcSize = arcData.arcSize == CSSValueLarge ? ShapeArcSegment::ArcSize::Large : ShapeArcSegment::ArcSize::Small;
        return ShapeArcSegment(arcData.affinity, WTFMove(targetLengthPoint), convertToLengthSize(builderState.cssToLengthConversionData(), arcData.radius), arcSweep, arcSize, toDegrees(arcData.angle));
    }
    case SegmentType::Close:
        return ShapeCloseSegment();
    }

    ASSERT_NOT_REACHED();
    return ShapeCloseSegment();
}

} // namespace WebCore
