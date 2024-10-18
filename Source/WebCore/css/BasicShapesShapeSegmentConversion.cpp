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
#include "BasicShapesShapeSegmentConversion.h"

#include "BasicShapesShape.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSShapeSegmentValue.h"
#include "CSSValue.h"
#include "CSSValuePair.h"

namespace WebCore {

static auto lengthToCSSValue(const Length& value, const RenderStyle& style)
{
    return CSSPrimitiveValue::create(value, style);
}

static auto lengthPointToCSSValue(const LengthPoint& value, const RenderStyle& style)
{
    return CSSValuePair::createNoncoalescing(
        CSSPrimitiveValue::create(value.x, style),
        CSSPrimitiveValue::create(value.y, style));
}

static auto lengthSizeToCSSValue(const LengthSize& value, const RenderStyle& style)
{
    return CSSValuePair::create(
        CSSPrimitiveValue::create(value.width, style),
        CSSPrimitiveValue::create(value.height, style));
}

static ControlPointValue controlPointValue(const ControlPoint& controlPoint, const RenderStyle& style)
{
    return ControlPointValue {
        lengthPointToCSSValue(controlPoint.offset, style),
        controlPoint.anchoring
    };
}

Ref<CSSShapeSegmentValue> toCSSShapeSegmentValue(const RenderStyle& style, const BasicShapeShape::ShapeSegment& segment)
{
    return WTF::switchOn(segment,
        [&](const ShapeMoveSegment& segment) {
            return CSSShapeSegmentValue::createMove(segment.affinity(), lengthPointToCSSValue(segment.offset(), style));
        },
        [&](const ShapeLineSegment& segment) {
            return CSSShapeSegmentValue::createLine(segment.affinity(), lengthPointToCSSValue(segment.offset(), style));
        },
        [&](const ShapeHorizontalLineSegment& segment) {
            return CSSShapeSegmentValue::createHorizontalLine(segment.affinity(), lengthToCSSValue(segment.length(), style));
        },
        [&](const ShapeVerticalLineSegment& segment) {
            return CSSShapeSegmentValue::createVerticalLine(segment.affinity(), lengthToCSSValue(segment.length(), style));
        },
        [&](const ShapeCurveSegment& segment) {
            if (segment.controlPoint2())
                return CSSShapeSegmentValue::createCubicCurve(segment.affinity(), lengthPointToCSSValue(segment.offset(), style), controlPointValue(segment.controlPoint1(), style), controlPointValue(segment.controlPoint2().value(), style));

            return CSSShapeSegmentValue::createQuadraticCurve(segment.affinity(), lengthPointToCSSValue(segment.offset(), style), controlPointValue(segment.controlPoint1(), style));
        },
        [&](const ShapeSmoothSegment& segment) {
            if (segment.intermediatePoint())
                return CSSShapeSegmentValue::createSmoothCubicCurve(segment.affinity(), lengthPointToCSSValue(segment.offset(), style), controlPointValue(segment.intermediatePoint().value(), style));

            return CSSShapeSegmentValue::createSmoothQuadraticCurve(segment.affinity(), lengthPointToCSSValue(segment.offset(), style));
        },
        [&](const ShapeArcSegment& segment) {
            return CSSShapeSegmentValue::createArc(segment.affinity(), lengthPointToCSSValue(segment.offset(), style), lengthSizeToCSSValue(segment.ellipseSize(), style),
                segment.sweep() == RotationDirection::Clockwise ? CSSValueCw : CSSValueCcw,
                segment.arcSize() == ShapeArcSegment::ArcSize::Large ? CSSValueLarge : CSSValueSmall,
                CSSPrimitiveValue::create(segment.angle(), CSSUnitType::CSS_DEG));
        },
        [&](const ShapeCloseSegment&) {
            return CSSShapeSegmentValue::createClose();
        }
    );
}

BasicShapeShape::ShapeSegment fromCSSShapeSegmentValue(const CSSShapeSegmentValue& cssShapeSegment, const Style::BuilderState& builderState)
{
    return cssShapeSegment.toShapeSegment(builderState);
}

} // namespace WebCore
