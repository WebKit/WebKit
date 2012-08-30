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
#include "StyleResolver.h"

namespace WebCore {

PassRefPtr<CSSValue> valueForBasicShape(const BasicShape* basicShape)
{
    RefPtr<CSSBasicShape> basicShapeValue;
    switch (basicShape->type()) {
    case BasicShape::BASIC_SHAPE_RECTANGLE: {
        const BasicShapeRectangle* rectangle = static_cast<const BasicShapeRectangle*>(basicShape);
        RefPtr<CSSBasicShapeRectangle> rectangleValue = CSSBasicShapeRectangle::create();

        rectangleValue->setX(cssValuePool().createValue(rectangle->x()));
        rectangleValue->setY(cssValuePool().createValue(rectangle->y()));
        rectangleValue->setWidth(cssValuePool().createValue(rectangle->width()));
        rectangleValue->setHeight(cssValuePool().createValue(rectangle->height()));
        if (!rectangle->cornerRadiusX().isUndefined()) {
            rectangleValue->setRadiusX(cssValuePool().createValue(rectangle->cornerRadiusX()));
            if (!rectangle->cornerRadiusY().isUndefined())
                rectangleValue->setRadiusY(cssValuePool().createValue(rectangle->cornerRadiusY()));
        }

        basicShapeValue = rectangleValue.release();
        break;
    }
    case BasicShape::BASIC_SHAPE_CIRCLE: {
        const BasicShapeCircle* circle = static_cast<const BasicShapeCircle*>(basicShape);
        RefPtr<CSSBasicShapeCircle> circleValue = CSSBasicShapeCircle::create();

        circleValue->setCenterX(cssValuePool().createValue(circle->centerX()));
        circleValue->setCenterY(cssValuePool().createValue(circle->centerY()));
        circleValue->setRadius(cssValuePool().createValue(circle->radius()));

        basicShapeValue = circleValue.release();
        break;
    }
    case BasicShape::BASIC_SHAPE_ELLIPSE: {
        const BasicShapeEllipse* ellipse = static_cast<const BasicShapeEllipse*>(basicShape);
        RefPtr<CSSBasicShapeEllipse> ellipseValue = CSSBasicShapeEllipse::create();

        ellipseValue->setCenterX(cssValuePool().createValue(ellipse->centerX()));
        ellipseValue->setCenterY(cssValuePool().createValue(ellipse->centerY()));
        ellipseValue->setRadiusX(cssValuePool().createValue(ellipse->radiusX()));
        ellipseValue->setRadiusY(cssValuePool().createValue(ellipse->radiusY()));

        basicShapeValue = ellipseValue.release();
        break;
    }
    case BasicShape::BASIC_SHAPE_POLYGON: {
        const BasicShapePolygon* polygon = static_cast<const BasicShapePolygon*>(basicShape);
        RefPtr<CSSBasicShapePolygon> polygonValue = CSSBasicShapePolygon::create();

        polygonValue->setWindRule(polygon->windRule());
        const Vector<Length>& values = polygon->values();
        for (unsigned i = 0; i < values.size(); i += 2)
            polygonValue->appendPoint(cssValuePool().createValue(values.at(i)), cssValuePool().createValue(values.at(i + 1)));

        basicShapeValue = polygonValue.release();
        break;
    }
    default:
        break;
    }
    return cssValuePool().createValue<PassRefPtr<CSSBasicShape> >(basicShapeValue.release());
}

static Length convertToLength(const StyleResolver* styleResolver, CSSPrimitiveValue* value)
{
    return value->convertToLength<FixedIntegerConversion | FixedFloatConversion | PercentConversion | ViewportPercentageConversion>(styleResolver->style(), styleResolver->rootElementStyle(), styleResolver->style()->effectiveZoom());
}

PassRefPtr<BasicShape> basicShapeForValue(const StyleResolver* styleResolver, const CSSBasicShape* basicShapeValue)
{
    RefPtr<BasicShape> basicShape;

    switch (basicShapeValue->type()) {
    case CSSBasicShape::CSS_BASIC_SHAPE_RECTANGLE: {
        const CSSBasicShapeRectangle* rectValue = static_cast<const CSSBasicShapeRectangle *>(basicShapeValue);
        RefPtr<BasicShapeRectangle> rect = BasicShapeRectangle::create();

        rect->setX(convertToLength(styleResolver, rectValue->x()));
        rect->setY(convertToLength(styleResolver, rectValue->y()));
        rect->setWidth(convertToLength(styleResolver, rectValue->width()));
        rect->setHeight(convertToLength(styleResolver, rectValue->height()));
        if (rectValue->radiusX()) {
            rect->setCornerRadiusX(convertToLength(styleResolver, rectValue->radiusX()));
            if (rectValue->radiusY())
                rect->setCornerRadiusY(convertToLength(styleResolver, rectValue->radiusY()));
        }
        basicShape = rect.release();
        break;
    }
    case CSSBasicShape::CSS_BASIC_SHAPE_CIRCLE: {
        const CSSBasicShapeCircle* circleValue = static_cast<const CSSBasicShapeCircle *>(basicShapeValue);
        RefPtr<BasicShapeCircle> circle = BasicShapeCircle::create();

        circle->setCenterX(convertToLength(styleResolver, circleValue->centerX()));
        circle->setCenterY(convertToLength(styleResolver, circleValue->centerY()));
        circle->setRadius(convertToLength(styleResolver, circleValue->radius()));

        basicShape = circle.release();
        break;
    }
    case CSSBasicShape::CSS_BASIC_SHAPE_ELLIPSE: {
        const CSSBasicShapeEllipse* ellipseValue = static_cast<const CSSBasicShapeEllipse *>(basicShapeValue);
        RefPtr<BasicShapeEllipse> ellipse = BasicShapeEllipse::create();

        ellipse->setCenterX(convertToLength(styleResolver, ellipseValue->centerX()));
        ellipse->setCenterY(convertToLength(styleResolver, ellipseValue->centerY()));
        ellipse->setRadiusX(convertToLength(styleResolver, ellipseValue->radiusX()));
        ellipse->setRadiusY(convertToLength(styleResolver, ellipseValue->radiusY()));

        basicShape = ellipse.release();
        break;
    }
    case CSSBasicShape::CSS_BASIC_SHAPE_POLYGON: {
        const CSSBasicShapePolygon* polygonValue = static_cast<const CSSBasicShapePolygon *>(basicShapeValue);
        RefPtr<BasicShapePolygon> polygon = BasicShapePolygon::create();

        polygon->setWindRule(polygonValue->windRule());
        const Vector<RefPtr<CSSPrimitiveValue> >& values = polygonValue->values();
        for (unsigned i = 0; i < values.size(); i += 2)
            polygon->appendPoint(convertToLength(styleResolver, values.at(i).get()), convertToLength(styleResolver, values.at(i + 1).get()));

        basicShape = polygon.release();
        break;
    }
    default:
        break;
    }
    return basicShape.release();
}
}
