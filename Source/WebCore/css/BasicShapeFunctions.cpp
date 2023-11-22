/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
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
#include "BasicShapeFunctions.h"

#include "BasicShapes.h"
#include "CSSBasicShapes.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSValuePair.h"
#include "CSSValuePool.h"
#include "CalculationValue.h"
#include "LengthFunctions.h"
#include "RenderStyle.h"
#include "SVGPathByteStream.h"

namespace WebCore {

static Ref<CSSValue> valueForCenterCoordinate(const RenderStyle& style, const BasicShapeCenterCoordinate& center, BoxOrient orientation)
{
    if (center.direction() == BasicShapeCenterCoordinate::Direction::TopLeft)
        return CSSPrimitiveValue::create(center.length(), style);

    CSSValueID keyword = orientation == BoxOrient::Horizontal ? CSSValueRight : CSSValueBottom;

    return CSSValuePair::create(CSSPrimitiveValue::create(keyword), CSSPrimitiveValue::create(center.length(), style));
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

Ref<CSSValue> valueForBasicShape(const RenderStyle& style, const BasicShape& basicShape, SVGPathConversion conversion)
{
    auto createValue = [&](const Length& length) {
        return CSSPrimitiveValue::create(length, style);
    };
    auto createPair = [&](const LengthSize& size) {
        return CSSValuePair::create(createValue(size.width), createValue(size.height));
    };

    switch (basicShape.type()) {
    case BasicShape::Type::Circle: {
        auto& circle = downcast<BasicShapeCircle>(basicShape);
        RefPtr radius = basicShapeRadiusToCSSValue(style, circle.radius());

        if (circle.positionWasOmitted())
            return CSSCircleValue::create(WTFMove(radius), nullptr, nullptr);

        return CSSCircleValue::create(WTFMove(radius),
            valueForCenterCoordinate(style, circle.centerX(), BoxOrient::Horizontal),
            valueForCenterCoordinate(style, circle.centerY(), BoxOrient::Vertical));
    }
    case BasicShape::Type::Ellipse: {
        auto& ellipse = downcast<BasicShapeEllipse>(basicShape);
        RefPtr radiusX = basicShapeRadiusToCSSValue(style, ellipse.radiusX());
        RefPtr radiusY = basicShapeRadiusToCSSValue(style, ellipse.radiusY());

        if (ellipse.positionWasOmitted())
            return CSSEllipseValue::create(WTFMove(radiusX), WTFMove(radiusY), nullptr, nullptr);

        return CSSEllipseValue::create(
            WTFMove(radiusX), WTFMove(radiusY),
            valueForCenterCoordinate(style, ellipse.centerX(), BoxOrient::Horizontal),
            valueForCenterCoordinate(style, ellipse.centerY(), BoxOrient::Vertical));
    }
    case BasicShape::Type::Polygon: {
        auto& polygon = downcast<BasicShapePolygon>(basicShape);
        CSSValueListBuilder values;
        for (auto& value : polygon.values())
            values.append(CSSPrimitiveValue::create(value, style));
        return CSSPolygonValue::create(WTFMove(values), polygon.windRule());
    }
    case BasicShape::Type::Path: {
        auto& pathShape = downcast<BasicShapePath>(basicShape);
        ASSERT(pathShape.pathData());
        return CSSPathValue::create(copySVGPathByteStream(*pathShape.pathData(), conversion), pathShape.windRule());
    }
    case BasicShape::Type::Inset: {
        auto& inset = downcast<BasicShapeInset>(basicShape);
        return CSSInsetShapeValue::create(createValue(inset.top()), createValue(inset.right()),
            createValue(inset.bottom()), createValue(inset.left()),
            createPair(inset.topLeftRadius()), createPair(inset.topRightRadius()),
            createPair(inset.bottomRightRadius()), createPair(inset.bottomLeftRadius()));
    }
    case BasicShape::Type::Xywh: {
        auto& xywh = downcast<BasicShapeXywh>(basicShape);
        return CSSXywhValue::create(createValue(xywh.insetX()), createValue(xywh.insetY()),
            createValue(xywh.width()), createValue(xywh.height()),
            createPair(xywh.topLeftRadius()), createPair(xywh.topRightRadius()),
            createPair(xywh.bottomRightRadius()), createPair(xywh.bottomLeftRadius()));
    }
    case BasicShape::Type::Rect: {
        auto& rect = downcast<BasicShapeRect>(basicShape);
        return CSSRectShapeValue::create(createValue(rect.top()), createValue(rect.right()),
            createValue(rect.bottom()), createValue(rect.left()),
            createPair(rect.topLeftRadius()), createPair(rect.topRightRadius()),
            createPair(rect.bottomRightRadius()), createPair(rect.bottomLeftRadius()));
    }
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static Length convertToLength(const CSSToLengthConversionData& conversionData, const CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).convertToLength<FixedIntegerConversion | FixedFloatConversion | PercentConversion | CalculatedConversion>(conversionData);
}

static Length convertToLengthOrAuto(const CSSToLengthConversionData& conversionData, const CSSValue& value)
{
    return downcast<CSSPrimitiveValue>(value).convertToLength<FixedIntegerConversion | FixedFloatConversion | PercentConversion | CalculatedConversion | AutoConversion>(conversionData);
}

static LengthSize convertToLengthSize(const CSSToLengthConversionData& conversionData, const CSSValue* value)
{
    if (!value)
        return { { 0, LengthType::Fixed }, { 0, LengthType::Fixed } };

    return { convertToLength(conversionData, value->first()), convertToLength(conversionData, value->second()) };
}

static BasicShapeCenterCoordinate convertToCenterCoordinate(const CSSToLengthConversionData& conversionData, const CSSValue* value)
{
    CSSValueID keyword = CSSValueTop;
    Length offset { 0, LengthType::Fixed };
    if (!value)
        keyword = CSSValueCenter;
    else if (value->isValueID())
        keyword = value->valueID();
    else if (value->isPair()) {
        keyword = value->first().valueID();
        offset = convertToLength(conversionData, value->second());
    } else
        offset = convertToLength(conversionData, *value);

    BasicShapeCenterCoordinate::Direction direction;
    switch (keyword) {
    case CSSValueTop:
    case CSSValueLeft:
        direction = BasicShapeCenterCoordinate::Direction::TopLeft;
        break;
    case CSSValueRight:
    case CSSValueBottom:
        direction = BasicShapeCenterCoordinate::Direction::BottomRight;
        break;
    case CSSValueCenter:
        direction = BasicShapeCenterCoordinate::Direction::TopLeft;
        offset = Length(50, LengthType::Percent);
        break;
    default:
        ASSERT_NOT_REACHED();
        direction = BasicShapeCenterCoordinate::Direction::TopLeft;
        break;
    }

    return BasicShapeCenterCoordinate(direction, WTFMove(offset));
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
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    return BasicShapeRadius(convertToLength(conversionData, *radius));
}

Ref<BasicShape> basicShapeForValue(const CSSToLengthConversionData& conversionData, const CSSValue& value, float zoom)
{
    if (auto* circleValue = dynamicDowncast<CSSCircleValue>(value)) {
        auto circle = BasicShapeCircle::create();
        circle->setRadius(cssValueToBasicShapeRadius(conversionData, circleValue->radius()));
        circle->setCenterX(convertToCenterCoordinate(conversionData, circleValue->centerX()));
        circle->setCenterY(convertToCenterCoordinate(conversionData, circleValue->centerY()));
        circle->setPositionWasOmitted(!circleValue->centerX() && !circleValue->centerY());
        return circle;
    }
    if (auto* ellipseValue = dynamicDowncast<CSSEllipseValue>(value)) {
        auto ellipse = BasicShapeEllipse::create();
        ellipse->setRadiusX(cssValueToBasicShapeRadius(conversionData, ellipseValue->radiusX()));
        ellipse->setRadiusY(cssValueToBasicShapeRadius(conversionData, ellipseValue->radiusY()));
        ellipse->setCenterX(convertToCenterCoordinate(conversionData, ellipseValue->centerX()));
        ellipse->setCenterY(convertToCenterCoordinate(conversionData, ellipseValue->centerY()));
        ellipse->setPositionWasOmitted(!ellipseValue->centerX() && !ellipseValue->centerY());
        return ellipse;
    }
    if (auto* polygonValue = dynamicDowncast<CSSPolygonValue>(value)) {
        auto polygon = BasicShapePolygon::create();
        polygon->setWindRule(polygonValue->windRule());
        for (unsigned i = 0; i < polygonValue->size(); i += 2)
            polygon->appendPoint(convertToLength(conversionData, *polygonValue->item(i)), convertToLength(conversionData, *polygonValue->item(i + 1)));
        return polygon;
    }
    if (auto* rectValue = dynamicDowncast<CSSInsetShapeValue>(value)) {
        auto rect = BasicShapeInset::create();
        rect->setTop(convertToLength(conversionData, rectValue->top()));
        rect->setRight(convertToLength(conversionData, rectValue->right()));
        rect->setBottom(convertToLength(conversionData, rectValue->bottom()));
        rect->setLeft(convertToLength(conversionData, rectValue->left()));
        rect->setTopLeftRadius(convertToLengthSize(conversionData, rectValue->topLeftRadius()));
        rect->setTopRightRadius(convertToLengthSize(conversionData, rectValue->topRightRadius()));
        rect->setBottomRightRadius(convertToLengthSize(conversionData, rectValue->bottomRightRadius()));
        rect->setBottomLeftRadius(convertToLengthSize(conversionData, rectValue->bottomLeftRadius()));
        return rect;
    }
    if (auto* rectValue = dynamicDowncast<CSSXywhValue>(value)) {
        auto rect = BasicShapeXywh::create();
        rect->setInsetX(convertToLength(conversionData, rectValue->insetX()));
        rect->setInsetY(convertToLength(conversionData, rectValue->insetY()));
        rect->setWidth(convertToLength(conversionData, rectValue->width()));
        rect->setHeight(convertToLength(conversionData, rectValue->height()));

        rect->setTopLeftRadius(convertToLengthSize(conversionData, rectValue->topLeftRadius()));
        rect->setTopRightRadius(convertToLengthSize(conversionData, rectValue->topRightRadius()));
        rect->setBottomRightRadius(convertToLengthSize(conversionData, rectValue->bottomRightRadius()));
        rect->setBottomLeftRadius(convertToLengthSize(conversionData, rectValue->bottomLeftRadius()));
        return rect;
    }
    if (auto* rectValue = dynamicDowncast<CSSRectShapeValue>(value)) {
        auto rect = BasicShapeRect::create();
        rect->setTop(convertToLengthOrAuto(conversionData, rectValue->top()));
        rect->setRight(convertToLengthOrAuto(conversionData, rectValue->right()));
        rect->setBottom(convertToLengthOrAuto(conversionData, rectValue->bottom()));
        rect->setLeft(convertToLengthOrAuto(conversionData, rectValue->left()));

        rect->setTopLeftRadius(convertToLengthSize(conversionData, rectValue->topLeftRadius()));
        rect->setTopRightRadius(convertToLengthSize(conversionData, rectValue->topRightRadius()));
        rect->setBottomRightRadius(convertToLengthSize(conversionData, rectValue->bottomRightRadius()));
        rect->setBottomLeftRadius(convertToLengthSize(conversionData, rectValue->bottomLeftRadius()));
        return rect;
    }
    if (auto* pathValue = dynamicDowncast<CSSPathValue>(value)) {
        auto path = BasicShapePath::create(pathValue->pathData().copy());
        path->setWindRule(pathValue->windRule());
        path->setZoom(zoom);
        return path;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

float floatValueForCenterCoordinate(const BasicShapeCenterCoordinate& center, float boxDimension)
{
    float offset = floatValueForLength(center.length(), boxDimension);
    if (center.direction() == BasicShapeCenterCoordinate::Direction::TopLeft)
        return offset;
    return boxDimension - offset;
}

}
