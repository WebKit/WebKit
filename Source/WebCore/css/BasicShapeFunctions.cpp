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
#include "RenderStyle.h"

namespace WebCore {

PassRefPtr<CSSValue> valueForBasicShape(const RenderStyle* style, const BasicShape* basicShape)
{
    CSSValuePool& pool = cssValuePool();

    RefPtr<CSSBasicShape> basicShapeValue;
    switch (basicShape->type()) {
    case BasicShape::BasicShapeRectangleType: {
        const BasicShapeRectangle* rectangle = static_cast<const BasicShapeRectangle*>(basicShape);
        RefPtr<CSSBasicShapeRectangle> rectangleValue = CSSBasicShapeRectangle::create();

        rectangleValue->setX(pool.createValue(rectangle->x(), style));
        rectangleValue->setY(pool.createValue(rectangle->y(), style));
        rectangleValue->setWidth(pool.createValue(rectangle->width(), style));
        rectangleValue->setHeight(pool.createValue(rectangle->height(), style));
        rectangleValue->setRadiusX(pool.createValue(rectangle->cornerRadiusX(), style));
        rectangleValue->setRadiusY(pool.createValue(rectangle->cornerRadiusY(), style));

        basicShapeValue = rectangleValue.release();
        break;
    }
    case BasicShape::BasicShapeCircleType: {
        const BasicShapeCircle* circle = static_cast<const BasicShapeCircle*>(basicShape);
        RefPtr<CSSBasicShapeCircle> circleValue = CSSBasicShapeCircle::create();

        circleValue->setCenterX(pool.createValue(circle->centerX(), style));
        circleValue->setCenterY(pool.createValue(circle->centerY(), style));
        circleValue->setRadius(pool.createValue(circle->radius(), style));

        basicShapeValue = circleValue.release();
        break;
    }
    case BasicShape::BasicShapeEllipseType: {
        const BasicShapeEllipse* ellipse = static_cast<const BasicShapeEllipse*>(basicShape);
        RefPtr<CSSBasicShapeEllipse> ellipseValue = CSSBasicShapeEllipse::create();

        ellipseValue->setCenterX(pool.createValue(ellipse->centerX(), style));
        ellipseValue->setCenterY(pool.createValue(ellipse->centerY(), style));
        ellipseValue->setRadiusX(pool.createValue(ellipse->radiusX(), style));
        ellipseValue->setRadiusY(pool.createValue(ellipse->radiusY(), style));

        basicShapeValue = ellipseValue.release();
        break;
    }
    case BasicShape::BasicShapePolygonType: {
        const BasicShapePolygon* polygon = static_cast<const BasicShapePolygon*>(basicShape);
        RefPtr<CSSBasicShapePolygon> polygonValue = CSSBasicShapePolygon::create();

        polygonValue->setWindRule(polygon->windRule());
        const Vector<Length>& values = polygon->values();
        for (unsigned i = 0; i < values.size(); i += 2)
            polygonValue->appendPoint(pool.createValue(values.at(i), style), pool.createValue(values.at(i + 1), style));

        basicShapeValue = polygonValue.release();
        break;
    }
    case BasicShape::BasicShapeInsetRectangleType: {
        const BasicShapeInsetRectangle* rectangle = static_cast<const BasicShapeInsetRectangle*>(basicShape);
        RefPtr<CSSBasicShapeInsetRectangle> rectangleValue = CSSBasicShapeInsetRectangle::create();

        rectangleValue->setTop(pool.createValue(rectangle->top(), style));
        rectangleValue->setRight(pool.createValue(rectangle->right(), style));
        rectangleValue->setBottom(pool.createValue(rectangle->bottom(), style));
        rectangleValue->setLeft(pool.createValue(rectangle->left(), style));
        rectangleValue->setRadiusX(pool.createValue(rectangle->cornerRadiusX(), style));
        rectangleValue->setRadiusY(pool.createValue(rectangle->cornerRadiusY(), style));

        basicShapeValue = rectangleValue.release();
        break;
    }
    default:
        break;
    }
    return pool.createValue(basicShapeValue.release());
}

static Length convertToLength(const RenderStyle* style, const RenderStyle* rootStyle, CSSPrimitiveValue* value)
{
    return value->convertToLength<FixedIntegerConversion | FixedFloatConversion | PercentConversion | CalculatedConversion | ViewportPercentageConversion>(style, rootStyle, style->effectiveZoom());
}

PassRefPtr<BasicShape> basicShapeForValue(const RenderStyle* style, const RenderStyle* rootStyle, const CSSBasicShape* basicShapeValue)
{
    RefPtr<BasicShape> basicShape;

    switch (basicShapeValue->type()) {
    case CSSBasicShape::CSSBasicShapeRectangleType: {
        const CSSBasicShapeRectangle* rectValue = static_cast<const CSSBasicShapeRectangle *>(basicShapeValue);
        RefPtr<BasicShapeRectangle> rect = BasicShapeRectangle::create();

        rect->setX(convertToLength(style, rootStyle, rectValue->x()));
        rect->setY(convertToLength(style, rootStyle, rectValue->y()));
        rect->setWidth(convertToLength(style, rootStyle, rectValue->width()));
        rect->setHeight(convertToLength(style, rootStyle, rectValue->height()));
        if (rectValue->radiusX()) {
            Length radiusX = convertToLength(style, rootStyle, rectValue->radiusX());
            rect->setCornerRadiusX(radiusX);
            if (rectValue->radiusY())
                rect->setCornerRadiusY(convertToLength(style, rootStyle, rectValue->radiusY()));
            else
                rect->setCornerRadiusY(radiusX);
        } else {
            rect->setCornerRadiusX(Length(0, Fixed));
            rect->setCornerRadiusY(Length(0, Fixed));
        }
        basicShape = rect.release();
        break;
    }
    case CSSBasicShape::CSSBasicShapeCircleType: {
        const CSSBasicShapeCircle* circleValue = static_cast<const CSSBasicShapeCircle *>(basicShapeValue);
        RefPtr<BasicShapeCircle> circle = BasicShapeCircle::create();

        circle->setCenterX(convertToLength(style, rootStyle, circleValue->centerX()));
        circle->setCenterY(convertToLength(style, rootStyle, circleValue->centerY()));
        circle->setRadius(convertToLength(style, rootStyle, circleValue->radius()));

        basicShape = circle.release();
        break;
    }
    case CSSBasicShape::CSSBasicShapeEllipseType: {
        const CSSBasicShapeEllipse* ellipseValue = static_cast<const CSSBasicShapeEllipse *>(basicShapeValue);
        RefPtr<BasicShapeEllipse> ellipse = BasicShapeEllipse::create();

        ellipse->setCenterX(convertToLength(style, rootStyle, ellipseValue->centerX()));
        ellipse->setCenterY(convertToLength(style, rootStyle, ellipseValue->centerY()));
        ellipse->setRadiusX(convertToLength(style, rootStyle, ellipseValue->radiusX()));
        ellipse->setRadiusY(convertToLength(style, rootStyle, ellipseValue->radiusY()));

        basicShape = ellipse.release();
        break;
    }
    case CSSBasicShape::CSSBasicShapePolygonType: {
        const CSSBasicShapePolygon* polygonValue = static_cast<const CSSBasicShapePolygon *>(basicShapeValue);
        RefPtr<BasicShapePolygon> polygon = BasicShapePolygon::create();

        polygon->setWindRule(polygonValue->windRule());
        const Vector<RefPtr<CSSPrimitiveValue> >& values = polygonValue->values();
        for (unsigned i = 0; i < values.size(); i += 2)
            polygon->appendPoint(convertToLength(style, rootStyle, values.at(i).get()), convertToLength(style, rootStyle, values.at(i + 1).get()));

        basicShape = polygon.release();
        break;
    }
    case CSSBasicShape::CSSBasicShapeInsetRectangleType: {
        const CSSBasicShapeInsetRectangle* rectValue = static_cast<const CSSBasicShapeInsetRectangle *>(basicShapeValue);
        RefPtr<BasicShapeInsetRectangle> rect = BasicShapeInsetRectangle::create();

        rect->setTop(convertToLength(style, rootStyle, rectValue->top()));
        rect->setRight(convertToLength(style, rootStyle, rectValue->right()));
        rect->setBottom(convertToLength(style, rootStyle, rectValue->bottom()));
        rect->setLeft(convertToLength(style, rootStyle, rectValue->left()));
        if (rectValue->radiusX()) {
            Length radiusX = convertToLength(style, rootStyle, rectValue->radiusX());
            rect->setCornerRadiusX(radiusX);
            if (rectValue->radiusY())
                rect->setCornerRadiusY(convertToLength(style, rootStyle, rectValue->radiusY()));
            else
                rect->setCornerRadiusY(radiusX);
        } else {
            rect->setCornerRadiusX(Length(0, Fixed));
            rect->setCornerRadiusY(Length(0, Fixed));
        }
        basicShape = rect.release();
        break;
    }
    default:
        break;
    }
    return basicShape.release();
}
}
