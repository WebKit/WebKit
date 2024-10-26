/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
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

#include "config.h"
#include "BasicShapeConversion.h"

#include "BasicShapes.h"
#include "BasicShapesShape.h"
#include "BasicShapesShapeSegmentConversion.h"
#include "CSSBasicShapes.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSShapeSegmentValue.h"
#include "CSSValuePair.h"
#include "CSSValuePool.h"
#include "CalculationValue.h"
#include "LengthFunctions.h"
#include "RenderStyle.h"
#include "RenderStyleInlines.h"
#include "SVGPathByteStream.h"
#include "StyleBuilderConverter.h"
#include "StyleBuilderState.h"

namespace WebCore {

static Length convertToLengthOrAuto(const CSSToLengthConversionData& conversionData, const CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).convertToLength<FixedIntegerConversion | FixedFloatConversion | PercentConversion | CalculatedConversion | AutoConversion>(conversionData);
}

Length convertToLength(const CSSToLengthConversionData& conversionData, const CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).convertToLength<FixedIntegerConversion | FixedFloatConversion | PercentConversion | CalculatedConversion>(conversionData);
}

LengthSize convertToLengthSize(const CSSToLengthConversionData& conversionData, const CSSValue& value)
{
    return { convertToLength(conversionData, value.protectedFirst()), convertToLength(conversionData, value.protectedSecond()) };
}

LengthPoint coordinatePairToLengthPoint(const CSSToLengthConversionData& conversionData, const CSSValue& value)
{
    RefPtr pairValue = dynamicDowncast<CSSValuePair>(value);
    if (!pairValue)
        return { };

    return { convertToLength(conversionData, pairValue->first()), convertToLength(conversionData, pairValue->second()) };
}

LengthPoint positionOrCoordinatePairToLengthPoint(const CSSValue& value, CoordinateAffinity affinity, const Style::BuilderState& builderState)
{
    if (affinity == CoordinateAffinity::Absolute)
        return Style::BuilderConverter::convertPosition(builderState, value);

    RefPtr pairValue = dynamicDowncast<CSSValuePair>(value);
    if (!pairValue)
        return { };

    return coordinatePairToLengthPoint(builderState.cssToLengthConversionData(), value);
}

static LengthSize convertToLengthSize(const CSSToLengthConversionData& conversionData, const CSSValue* value)
{
    if (!value)
        return { { 0, LengthType::Fixed }, { 0, LengthType::Fixed } };

    return convertToLengthSize(conversionData, *value);
}

static Ref<CSSPrimitiveValue> basicShapeRadiusToCSSValue(const RenderStyle& style, const BasicShapeRadius& radius)
{
    switch (radius.type()) {
    case BasicShapeRadius::Type::Value:
        return CSSPrimitiveValue::create(radius.value(), style);
    case BasicShapeRadius::Type::ClosestSide:
        return CSSPrimitiveValue::create(CSSValueClosestSide);
    case BasicShapeRadius::Type::FarthestSide:
        return CSSPrimitiveValue::create(CSSValueFarthestSide);
    case BasicShapeRadius::Type::ClosestCorner:
        return CSSPrimitiveValue::create(CSSValueClosestCorner);
    case BasicShapeRadius::Type::FarthestCorner:
        return CSSPrimitiveValue::create(CSSValueFarthestCorner);
    }

    ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::create(CSSValueClosestSide);
}

static SVGPathByteStream copySVGPathByteStream(const SVGPathByteStream& source, SVGPathConversion conversion)
{
    if (conversion == SVGPathConversion::ForceAbsolute) {
        // Only returns the resulting absolute path if the conversion succeeds.
        if (auto result = convertSVGPathByteStreamToAbsoluteCoordinates(source))
            return *result;
    }
    return source;
}

Ref<CSSValue> valueForSVGPath(const BasicShapePath& path, SVGPathConversion conversion)
{
    ASSERT(path.pathData());
    return CSSPathValue::create(copySVGPathByteStream(*path.pathData(), conversion), path.windRule());
}

Ref<CSSValue> valueForBasicShape(const RenderStyle& style, const BasicShape& basicShape, SVGPathConversion conversion)
{
    auto createValue = [&](const Length& length) {
        return CSSPrimitiveValue::create(length.isAuto() ? Length(0, LengthType::Percent) : length, style);
    };
    auto createPair = [&](const LengthSize& size) {
        return CSSValuePair::create(createValue(size.width), createValue(size.height));
    };
    auto createCoordinatePair = [&](const LengthPoint& point) {
        return CSSValuePair::createNoncoalescing(createValue(point.x), createValue(point.y));
    };
    auto createReflectedSumValue = [&](const Length& a, const Length& b) {
        auto reflected = convertTo100PercentMinusLengthSum(a, b);
        return CSSPrimitiveValue::create(reflected, style);
    };
    auto createReflectedValue = [&](const Length& length) -> Ref<CSSValue> {
        if (length.isAuto())
            return createValue(Length(0, LengthType::Percent));
        auto reflected = convertTo100PercentMinusLength(length);
        return CSSPrimitiveValue::create(reflected, style);
    };

    switch (basicShape.type()) {
    case BasicShape::Type::Circle: {
        auto& circle = uncheckedDowncast<BasicShapeCircle>(basicShape);
        RefPtr radius = basicShapeRadiusToCSSValue(style, circle.radius());

        if (circle.positionWasOmitted())
            return CSSCircleValue::create(WTFMove(radius), nullptr, nullptr);

        return CSSCircleValue::create(WTFMove(radius),
            CSSPrimitiveValue::create(circle.centerX().length(), style),
            CSSPrimitiveValue::create(circle.centerY().length(), style)
        );
    }
    case BasicShape::Type::Ellipse: {
        auto& ellipse = uncheckedDowncast<BasicShapeEllipse>(basicShape);
        RefPtr radiusX = basicShapeRadiusToCSSValue(style, ellipse.radiusX());
        RefPtr radiusY = basicShapeRadiusToCSSValue(style, ellipse.radiusY());

        if (ellipse.positionWasOmitted())
            return CSSEllipseValue::create(WTFMove(radiusX), WTFMove(radiusY), nullptr, nullptr);

        return CSSEllipseValue::create(
            WTFMove(radiusX),
            WTFMove(radiusY),
            CSSPrimitiveValue::create(ellipse.centerX().length(), style),
            CSSPrimitiveValue::create(ellipse.centerY().length(), style)
        );
    }
    case BasicShape::Type::Polygon: {
        auto& polygon = uncheckedDowncast<BasicShapePolygon>(basicShape);
        CSSValueListBuilder values;
        for (auto& value : polygon.values())
            values.append(CSSPrimitiveValue::create(value, style));
        return CSSPolygonValue::create(WTFMove(values), polygon.windRule());
    }
    case BasicShape::Type::Path:
        return valueForSVGPath(uncheckedDowncast<BasicShapePath>(basicShape), conversion);
    case BasicShape::Type::Inset: {
        auto& inset = uncheckedDowncast<BasicShapeInset>(basicShape);
        return CSSInsetShapeValue::create(createValue(inset.top()), createValue(inset.right()),
            createValue(inset.bottom()), createValue(inset.left()),
            createPair(inset.topLeftRadius()), createPair(inset.topRightRadius()),
            createPair(inset.bottomRightRadius()), createPair(inset.bottomLeftRadius()));
    }
    case BasicShape::Type::Xywh: {
        auto& xywh = uncheckedDowncast<BasicShapeXywh>(basicShape);
        return CSSInsetShapeValue::create(createValue(xywh.insetY()), createReflectedSumValue(xywh.insetX(), xywh.width()),
            createReflectedSumValue(xywh.insetY(), xywh.height()), createValue(xywh.insetX()),
            createPair(xywh.topLeftRadius()), createPair(xywh.topRightRadius()),
            createPair(xywh.bottomRightRadius()), createPair(xywh.bottomLeftRadius()));
    }
    case BasicShape::Type::Rect: {
        auto& rect = uncheckedDowncast<BasicShapeRect>(basicShape);
        return CSSInsetShapeValue::create(createValue(rect.top()), createReflectedValue(rect.right()),
            createReflectedValue(rect.bottom()), createValue(rect.left()),
            createPair(rect.topLeftRadius()), createPair(rect.topRightRadius()),
            createPair(rect.bottomRightRadius()), createPair(rect.bottomLeftRadius()));
    }
    case BasicShape::Type::Shape: {
        auto& shape = uncheckedDowncast<BasicShapeShape>(basicShape);

        CSSValueListBuilder segments;

        for (auto& segment : shape.segments()) {
            Ref value = toCSSShapeSegmentValue(style, segment);
            segments.append(WTFMove(value));
        }

        return CSSShapeValue::create(shape.windRule(), createCoordinatePair(shape.startPoint()), WTFMove(segments));
    }
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static BasicShapeCenterCoordinate convertToCenterCoordinate(const CSSToLengthConversionData& conversionData, const CSSValue* value)
{
    CSSValueID keyword = CSSValueTop;
    Length offset { 0, LengthType::Percent };
    if (!value)
        keyword = CSSValueCenter;
    else if (value->isValueID())
        keyword = value->valueID();
    else if (value->isPair()) {
        keyword = value->first().valueID();
        offset = convertToLength(conversionData, value->protectedSecond());
    } else
        offset = convertToLength(conversionData, *value);

    switch (keyword) {
    case CSSValueTop:
    case CSSValueLeft:
        break;
    case CSSValueRight:
    case CSSValueBottom:
        offset = convertTo100PercentMinusLength(WTFMove(offset));
        break;
    case CSSValueCenter:
        offset = Length(50, LengthType::Percent);
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return BasicShapeCenterCoordinate(WTFMove(offset));
}

static BasicShapeRadius cssValueToBasicShapeRadius(const CSSToLengthConversionData& conversionData, const CSSValue* radius)
{
    if (!radius)
        return BasicShapeRadius(BasicShapeRadius::Type::ClosestSide);

    if (radius->isValueID()) {
        switch (radius->valueID()) {
        case CSSValueClosestSide:
            return BasicShapeRadius(BasicShapeRadius::Type::ClosestSide);
        case CSSValueFarthestSide:
            return BasicShapeRadius(BasicShapeRadius::Type::FarthestSide);
        case CSSValueClosestCorner:
            return BasicShapeRadius(BasicShapeRadius::Type::ClosestCorner);
        case CSSValueFarthestCorner:
            return BasicShapeRadius(BasicShapeRadius::Type::FarthestCorner);
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    return BasicShapeRadius(convertToLength(conversionData, *radius));
}

Ref<BasicShape> basicShapeForValue(const CSSValue& value, const Style::BuilderState& builderState, std::optional<float> zoom)
{
    auto& conversionData = builderState.cssToLengthConversionData();

    if (RefPtr circleValue = dynamicDowncast<CSSCircleValue>(value)) {
        auto circle = BasicShapeCircle::create();
        circle->setRadius(cssValueToBasicShapeRadius(conversionData, circleValue->protectedRadius().get()));
        circle->setCenterX(convertToCenterCoordinate(conversionData, circleValue->protectedCenterX().get()));
        circle->setCenterY(convertToCenterCoordinate(conversionData, circleValue->protectedCenterY().get()));
        circle->setPositionWasOmitted(!circleValue->centerX() && !circleValue->centerY());
        return circle;
    }

    if (RefPtr ellipseValue = dynamicDowncast<CSSEllipseValue>(value)) {
        auto ellipse = BasicShapeEllipse::create();
        ellipse->setRadiusX(cssValueToBasicShapeRadius(conversionData, ellipseValue->protectedRadiusX().get()));
        ellipse->setRadiusY(cssValueToBasicShapeRadius(conversionData, ellipseValue->protectedRadiusY().get()));
        ellipse->setCenterX(convertToCenterCoordinate(conversionData, ellipseValue->protectedCenterX().get()));
        ellipse->setCenterY(convertToCenterCoordinate(conversionData, ellipseValue->protectedCenterY().get()));
        ellipse->setPositionWasOmitted(!ellipseValue->centerX() && !ellipseValue->centerY());
        return ellipse;
    }

    if (RefPtr polygonValue = dynamicDowncast<CSSPolygonValue>(value)) {
        auto polygon = BasicShapePolygon::create();
        polygon->setWindRule(polygonValue->windRule());
        for (unsigned i = 0; i < polygonValue->size(); i += 2)
            polygon->appendPoint(convertToLength(conversionData, *polygonValue->protectedItem(i)), convertToLength(conversionData, *polygonValue->protectedItem(i + 1)));
        return polygon;
    }

    if (RefPtr rectValue = dynamicDowncast<CSSInsetShapeValue>(value)) {
        auto rect = BasicShapeInset::create();
        rect->setTop(convertToLength(conversionData, rectValue->protectedTop()));
        rect->setRight(convertToLength(conversionData, rectValue->protectedRight()));
        rect->setBottom(convertToLength(conversionData, rectValue->protectedBottom()));
        rect->setLeft(convertToLength(conversionData, rectValue->protectedLeft()));
        rect->setTopLeftRadius(convertToLengthSize(conversionData, rectValue->protectedTopLeftRadius().get()));
        rect->setTopRightRadius(convertToLengthSize(conversionData, rectValue->protectedTopRightRadius().get()));
        rect->setBottomRightRadius(convertToLengthSize(conversionData, rectValue->protectedBottomRightRadius().get()));
        rect->setBottomLeftRadius(convertToLengthSize(conversionData, rectValue->protectedBottomLeftRadius().get()));
        return rect;
    }

    if (RefPtr rectValue = dynamicDowncast<CSSXywhValue>(value)) {
        auto rect = BasicShapeXywh::create();
        rect->setInsetX(convertToLength(conversionData, rectValue->protectedInsetX().get()));
        rect->setInsetY(convertToLength(conversionData, rectValue->protectedInsetY().get()));
        rect->setWidth(convertToLength(conversionData, rectValue->protectedWidth().get()));
        rect->setHeight(convertToLength(conversionData, rectValue->protectedHeight().get()));

        rect->setTopLeftRadius(convertToLengthSize(conversionData, rectValue->protectedTopLeftRadius().get()));
        rect->setTopRightRadius(convertToLengthSize(conversionData, rectValue->protectedTopRightRadius().get()));
        rect->setBottomRightRadius(convertToLengthSize(conversionData, rectValue->protectedBottomRightRadius().get()));
        rect->setBottomLeftRadius(convertToLengthSize(conversionData, rectValue->protectedBottomLeftRadius().get()));
        return rect;
    }

    if (RefPtr rectValue = dynamicDowncast<CSSRectShapeValue>(value)) {
        auto rect = BasicShapeRect::create();
        rect->setTop(convertToLengthOrAuto(conversionData, rectValue->protectedTop()));
        rect->setRight(convertToLengthOrAuto(conversionData, rectValue->protectedRight()));
        rect->setBottom(convertToLengthOrAuto(conversionData, rectValue->protectedBottom()));
        rect->setLeft(convertToLengthOrAuto(conversionData, rectValue->protectedLeft()));

        rect->setTopLeftRadius(convertToLengthSize(conversionData, rectValue->protectedTopLeftRadius().get()));
        rect->setTopRightRadius(convertToLengthSize(conversionData, rectValue->protectedTopRightRadius().get()));
        rect->setBottomRightRadius(convertToLengthSize(conversionData, rectValue->protectedBottomRightRadius().get()));
        rect->setBottomLeftRadius(convertToLengthSize(conversionData, rectValue->protectedBottomLeftRadius().get()));
        return rect;
    }

    if (RefPtr pathValue = dynamicDowncast<CSSPathValue>(value))
        return basicShapePathForValue(*pathValue, builderState, zoom);

    if (RefPtr shapeValue = dynamicDowncast<CSSShapeValue>(value))
        return basicShapeShapeForValue(*shapeValue, builderState);

    RELEASE_ASSERT_NOT_REACHED();
}

Ref<BasicShapePath> basicShapePathForValue(const CSSPathValue& value, const Style::BuilderState& builderState, std::optional<float> zoom)
{
    auto path = BasicShapePath::create(value.pathData().copy());
    path->setWindRule(value.windRule());
    path->setZoom(zoom.value_or(builderState.style().usedZoom()));
    return path;
}

Ref<BasicShapeShape> basicShapeShapeForValue(const CSSShapeValue& shapeValue, const Style::BuilderState& builderState)
{
    Vector<BasicShapeShape::ShapeSegment> segments;
    segments.reserveInitialCapacity(shapeValue.length());

    for (auto& segment : shapeValue) {
        RefPtr shapeSegment = dynamicDowncast<CSSShapeSegmentValue>(segment);
        if (!shapeSegment) {
            ASSERT_NOT_REACHED();
            continue;
        }
        segments.append(fromCSSShapeSegmentValue(*shapeSegment, builderState));
    }

    auto fromPoint = positionOrCoordinatePairToLengthPoint(shapeValue.protectedFromCoordinates().get(), CoordinateAffinity::Absolute, builderState);
    return BasicShapeShape::create(shapeValue.windRule(), WTFMove(fromPoint), WTFMove(segments));
}

float floatValueForCenterCoordinate(const BasicShapeCenterCoordinate& center, float boxDimension)
{
    return floatValueForLength(center.length(), boxDimension);
}

} // namespace WebCore
