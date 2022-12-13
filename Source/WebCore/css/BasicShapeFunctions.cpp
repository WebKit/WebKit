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
#include "CSSValuePool.h"
#include "Pair.h"
#include "RenderStyle.h"
#include "SVGPathByteStream.h"

namespace WebCore {

static Ref<CSSPrimitiveValue> valueForCenterCoordinate(CSSValuePool& pool, const RenderStyle& style, const BasicShapeCenterCoordinate& center, BoxOrient orientation)
{
    if (center.direction() == BasicShapeCenterCoordinate::Direction::TopLeft)
        return pool.createValue(center.length(), style);

    CSSValueID keyword = orientation == BoxOrient::Horizontal ? CSSValueRight : CSSValueBottom;

    return pool.createValue(Pair::create(pool.createIdentifierValue(keyword), pool.createValue(center.length(), style)));
}

static Ref<CSSPrimitiveValue> basicShapeRadiusToCSSValue(const RenderStyle& style, CSSValuePool& pool, const BasicShapeRadius& radius)
{
    switch (radius.type()) {
    case BasicShapeRadius::Type::Value:
        return pool.createValue(radius.value(), style);
    case BasicShapeRadius::Type::ClosestSide:
        return pool.createIdentifierValue(CSSValueClosestSide);
    case BasicShapeRadius::Type::FarthestSide:
        return pool.createIdentifierValue(CSSValueFarthestSide);
    }

    ASSERT_NOT_REACHED();
    return pool.createIdentifierValue(CSSValueClosestSide);
}

static std::unique_ptr<SVGPathByteStream> copySVGPathByteStream(const SVGPathByteStream& source, SVGPathConversion conversion)
{
    switch (conversion) {
    case SVGPathConversion::None:
        return source.copy();

    case SVGPathConversion::ForceAbsolute:
        // Only returns the resulting absolute path if the conversion succeeds.
        if (auto result = convertSVGPathByteStreamToAbsoluteCoordinates(source))
            return result;

        return source.copy();
    }

    ASSERT_NOT_REACHED();
    return source.copy();
}

Ref<CSSPrimitiveValue> valueForBasicShape(const RenderStyle& style, const BasicShape& basicShape, SVGPathConversion conversion)
{
    auto& cssValuePool = CSSValuePool::singleton();

    RefPtr<CSSBasicShape> basicShapeValue;
    switch (basicShape.type()) {
    case BasicShape::Type::Circle: {
        auto& circle = downcast<BasicShapeCircle>(basicShape);
        auto circleValue = CSSBasicShapeCircle::create();

        circleValue->setCenterX(valueForCenterCoordinate(cssValuePool, style, circle.centerX(), BoxOrient::Horizontal));
        circleValue->setCenterY(valueForCenterCoordinate(cssValuePool, style, circle.centerY(), BoxOrient::Vertical));
        circleValue->setRadius(basicShapeRadiusToCSSValue(style, cssValuePool, circle.radius()));

        basicShapeValue = WTFMove(circleValue);
        break;
    }
    case BasicShape::Type::Ellipse: {
        auto& ellipse = downcast<BasicShapeEllipse>(basicShape);
        auto ellipseValue = CSSBasicShapeEllipse::create();

        ellipseValue->setCenterX(valueForCenterCoordinate(cssValuePool, style, ellipse.centerX(), BoxOrient::Horizontal));
        ellipseValue->setCenterY(valueForCenterCoordinate(cssValuePool, style, ellipse.centerY(), BoxOrient::Vertical));
        ellipseValue->setRadiusX(basicShapeRadiusToCSSValue(style, cssValuePool, ellipse.radiusX()));
        ellipseValue->setRadiusY(basicShapeRadiusToCSSValue(style, cssValuePool, ellipse.radiusY()));

        basicShapeValue = WTFMove(ellipseValue);
        break;
    }
    case BasicShape::Type::Polygon: {
        auto& polygon = downcast<BasicShapePolygon>(basicShape);
        auto polygonValue = CSSBasicShapePolygon::create();

        polygonValue->setWindRule(polygon.windRule());
        const Vector<Length>& values = polygon.values();
        for (unsigned i = 0; i < values.size(); i += 2)
            polygonValue->appendPoint(cssValuePool.createValue(values.at(i), style), cssValuePool.createValue(values.at(i + 1), style));

        basicShapeValue = WTFMove(polygonValue);
        break;
    }
    case BasicShape::Type::Path: {
        auto& pathShape = downcast<BasicShapePath>(basicShape);

        ASSERT(pathShape.pathData());
        auto pathByteStream = copySVGPathByteStream(*pathShape.pathData(), conversion);

        auto pathShapeValue = CSSBasicShapePath::create(WTFMove(pathByteStream));
        pathShapeValue->setWindRule(pathShape.windRule());

        basicShapeValue = WTFMove(pathShapeValue);

        break;
    }
    case BasicShape::Type::Inset: {
        auto& inset = downcast<BasicShapeInset>(basicShape);
        auto insetValue = CSSBasicShapeInset::create();

        insetValue->setTop(cssValuePool.createValue(inset.top(), style));
        insetValue->setRight(cssValuePool.createValue(inset.right(), style));
        insetValue->setBottom(cssValuePool.createValue(inset.bottom(), style));
        insetValue->setLeft(cssValuePool.createValue(inset.left(), style));

        insetValue->setTopLeftRadius(cssValuePool.createValue(inset.topLeftRadius(), style));
        insetValue->setTopRightRadius(cssValuePool.createValue(inset.topRightRadius(), style));
        insetValue->setBottomRightRadius(cssValuePool.createValue(inset.bottomRightRadius(), style));
        insetValue->setBottomLeftRadius(cssValuePool.createValue(inset.bottomLeftRadius(), style));

        basicShapeValue = WTFMove(insetValue);
        break;
    }
    }

    return cssValuePool.createValue(basicShapeValue.releaseNonNull());
}

static Length convertToLength(const CSSToLengthConversionData& conversionData, const CSSPrimitiveValue* value)
{
    return value->convertToLength<FixedIntegerConversion | FixedFloatConversion | PercentConversion | CalculatedConversion>(conversionData);
}

static LengthSize convertToLengthSize(const CSSToLengthConversionData& conversionData, const CSSPrimitiveValue* value)
{
    if (!value)
        return { { 0, LengthType::Fixed }, { 0, LengthType::Fixed } };

    auto& pair = *value->pairValue();
    return { convertToLength(conversionData, pair.first()), convertToLength(conversionData, pair.second()) };
}

static BasicShapeCenterCoordinate convertToCenterCoordinate(const CSSToLengthConversionData& conversionData, CSSPrimitiveValue* value)
{
    CSSValueID keyword = CSSValueTop;
    Length offset { 0, LengthType::Fixed };
    if (!value)
        keyword = CSSValueCenter;
    else if (value->isValueID())
        keyword = value->valueID();
    else if (Pair* pair = value->pairValue()) {
        keyword = pair->first()->valueID();
        offset = convertToLength(conversionData, pair->second());
    } else
        offset = convertToLength(conversionData, value);

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

static BasicShapeRadius cssValueToBasicShapeRadius(const CSSToLengthConversionData& conversionData, CSSPrimitiveValue* radius)
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

    return BasicShapeRadius(convertToLength(conversionData, radius));
}

Ref<BasicShape> basicShapeForValue(const CSSToLengthConversionData& conversionData, const CSSBasicShape& basicShapeValue, float zoom)
{
    RefPtr<BasicShape> basicShape;

    switch (basicShapeValue.type()) {
    case CSSBasicShape::CSSBasicShapeCircleType: {
        auto& circleValue = downcast<CSSBasicShapeCircle>(basicShapeValue);
        auto circle = BasicShapeCircle::create();

        circle->setCenterX(convertToCenterCoordinate(conversionData, circleValue.centerX()));
        circle->setCenterY(convertToCenterCoordinate(conversionData, circleValue.centerY()));
        circle->setRadius(cssValueToBasicShapeRadius(conversionData, circleValue.radius()));

        basicShape = WTFMove(circle);
        break;
    }
    case CSSBasicShape::CSSBasicShapeEllipseType: {
        auto& ellipseValue = downcast<CSSBasicShapeEllipse>(basicShapeValue);
        auto ellipse = BasicShapeEllipse::create();

        ellipse->setCenterX(convertToCenterCoordinate(conversionData, ellipseValue.centerX()));
        ellipse->setCenterY(convertToCenterCoordinate(conversionData, ellipseValue.centerY()));

        ellipse->setRadiusX(cssValueToBasicShapeRadius(conversionData, ellipseValue.radiusX()));
        ellipse->setRadiusY(cssValueToBasicShapeRadius(conversionData, ellipseValue.radiusY()));

        basicShape = WTFMove(ellipse);
        break;
    }
    case CSSBasicShape::CSSBasicShapePolygonType: {
        auto& polygonValue = downcast<CSSBasicShapePolygon>(basicShapeValue);
        auto polygon = BasicShapePolygon::create();

        polygon->setWindRule(polygonValue.windRule());
        auto& values = polygonValue.values();
        for (unsigned i = 0; i < values.size(); i += 2)
            polygon->appendPoint(convertToLength(conversionData, values[i].ptr()), convertToLength(conversionData, values[i + 1].ptr()));

        basicShape = WTFMove(polygon);
        break;
    }
    case CSSBasicShape::CSSBasicShapeInsetType: {
        auto& rectValue = downcast<CSSBasicShapeInset>(basicShapeValue);
        auto rect = BasicShapeInset::create();

        rect->setTop(convertToLength(conversionData, rectValue.top()));
        rect->setRight(convertToLength(conversionData, rectValue.right()));
        rect->setBottom(convertToLength(conversionData, rectValue.bottom()));
        rect->setLeft(convertToLength(conversionData, rectValue.left()));

        rect->setTopLeftRadius(convertToLengthSize(conversionData, rectValue.topLeftRadius()));
        rect->setTopRightRadius(convertToLengthSize(conversionData, rectValue.topRightRadius()));
        rect->setBottomRightRadius(convertToLengthSize(conversionData, rectValue.bottomRightRadius()));
        rect->setBottomLeftRadius(convertToLengthSize(conversionData, rectValue.bottomLeftRadius()));

        basicShape = WTFMove(rect);
        break;
    }
    case CSSBasicShape::CSSBasicShapePathType: {
        auto& pathValue = downcast<CSSBasicShapePath>(basicShapeValue);
        auto path = BasicShapePath::create(pathValue.pathData().copy());
        path->setWindRule(pathValue.windRule());
        path->setZoom(zoom);

        basicShape = WTFMove(path);
        break;
    }
    }

    return basicShape.releaseNonNull();
}

float floatValueForCenterCoordinate(const BasicShapeCenterCoordinate& center, float boxDimension)
{
    float offset = floatValueForLength(center.length(), boxDimension);
    if (center.direction() == BasicShapeCenterCoordinate::Direction::TopLeft)
        return offset;
    return boxDimension - offset;
}

}
