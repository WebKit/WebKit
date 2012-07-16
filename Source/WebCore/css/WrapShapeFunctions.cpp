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
#include "WrapShapeFunctions.h"

#include "CSSPrimitiveValueMappings.h"
#include "CSSValuePool.h"
#include "CSSWrapShapes.h"
#include "StyleResolver.h"
#include "WrapShapes.h"

namespace WebCore {

PassRefPtr<CSSValue> valueForWrapShape(const WrapShape* wrapShape)
{
    RefPtr<CSSWrapShape> wrapShapeValue;
    switch (wrapShape->type()) {
    case WrapShape::WRAP_SHAPE_RECTANGLE: {
        const WrapShapeRectangle* rectangle = static_cast<const WrapShapeRectangle*>(wrapShape);
        RefPtr<CSSWrapShapeRectangle> rectangleValue = CSSWrapShapeRectangle::create();

        rectangleValue->setLeft(cssValuePool().createValue(rectangle->left()));
        rectangleValue->setTop(cssValuePool().createValue(rectangle->top()));
        rectangleValue->setWidth(cssValuePool().createValue(rectangle->width()));
        rectangleValue->setHeight(cssValuePool().createValue(rectangle->height()));
        if (!rectangle->cornerRadiusX().isUndefined()) {
            rectangleValue->setRadiusX(cssValuePool().createValue(rectangle->cornerRadiusX()));
            if (!rectangle->cornerRadiusY().isUndefined())
                rectangleValue->setRadiusY(cssValuePool().createValue(rectangle->cornerRadiusY()));
        }

        wrapShapeValue = rectangleValue.release();
        break;
    }
    case WrapShape::WRAP_SHAPE_CIRCLE: {
        const WrapShapeCircle* circle = static_cast<const WrapShapeCircle*>(wrapShape);
        RefPtr<CSSWrapShapeCircle> circleValue = CSSWrapShapeCircle::create();

        circleValue->setLeft(cssValuePool().createValue(circle->left()));
        circleValue->setTop(cssValuePool().createValue(circle->top()));
        circleValue->setRadius(cssValuePool().createValue(circle->radius()));

        wrapShapeValue = circleValue.release();
        break;
    }
    case WrapShape::WRAP_SHAPE_ELLIPSE: {
        const WrapShapeEllipse* ellipse = static_cast<const WrapShapeEllipse*>(wrapShape);
        RefPtr<CSSWrapShapeEllipse> ellipseValue = CSSWrapShapeEllipse::create();

        ellipseValue->setLeft(cssValuePool().createValue(ellipse->left()));
        ellipseValue->setTop(cssValuePool().createValue(ellipse->top()));
        ellipseValue->setRadiusX(cssValuePool().createValue(ellipse->radiusX()));
        ellipseValue->setRadiusY(cssValuePool().createValue(ellipse->radiusY()));

        wrapShapeValue = ellipseValue.release();
        break;
    }
    case WrapShape::WRAP_SHAPE_POLYGON: {
        const WrapShapePolygon* polygon = static_cast<const WrapShapePolygon*>(wrapShape);
        RefPtr<CSSWrapShapePolygon> polygonValue = CSSWrapShapePolygon::create();

        polygonValue->setWindRule(polygon->windRule());
        const Vector<Length>& values = polygon->values();
        for (unsigned i = 0; i < values.size(); i += 2)
            polygonValue->appendPoint(cssValuePool().createValue(values.at(i)), cssValuePool().createValue(values.at(i + 1)));

        wrapShapeValue = polygonValue.release();
        break;
    }
    default:
        break;
    }
    return cssValuePool().createValue<PassRefPtr<CSSWrapShape> >(wrapShapeValue.release());
}

static Length convertToLength(const StyleResolver* styleResolver, CSSPrimitiveValue* value)
{
    return value->convertToLength<FixedIntegerConversion | FixedFloatConversion | PercentConversion | ViewportPercentageConversion>(styleResolver->style(), styleResolver->rootElementStyle(), styleResolver->style()->effectiveZoom());
}

PassRefPtr<WrapShape> wrapShapeForValue(const StyleResolver* styleResolver, const CSSWrapShape* wrapShapeValue)
{
    RefPtr<WrapShape> wrapShape;

    switch (wrapShapeValue->type()) {
    case CSSWrapShape::CSS_WRAP_SHAPE_RECTANGLE: {
        const CSSWrapShapeRectangle* rectValue = static_cast<const CSSWrapShapeRectangle *>(wrapShapeValue);
        RefPtr<WrapShapeRectangle> rect = WrapShapeRectangle::create();

        rect->setLeft(convertToLength(styleResolver, rectValue->left()));
        rect->setTop(convertToLength(styleResolver, rectValue->top()));
        rect->setWidth(convertToLength(styleResolver, rectValue->width()));
        rect->setHeight(convertToLength(styleResolver, rectValue->height()));
        if (rectValue->radiusX()) {
            rect->setCornerRadiusX(convertToLength(styleResolver, rectValue->radiusX()));
            if (rectValue->radiusY())
                rect->setCornerRadiusY(convertToLength(styleResolver, rectValue->radiusY()));
        }
        wrapShape = rect.release();
        break;
    }
    case CSSWrapShape::CSS_WRAP_SHAPE_CIRCLE: {
        const CSSWrapShapeCircle* circleValue = static_cast<const CSSWrapShapeCircle *>(wrapShapeValue);
        RefPtr<WrapShapeCircle> circle = WrapShapeCircle::create();

        circle->setLeft(convertToLength(styleResolver, circleValue->left()));
        circle->setTop(convertToLength(styleResolver, circleValue->top()));
        circle->setRadius(convertToLength(styleResolver, circleValue->radius()));

        wrapShape = circle.release();
        break;
    }
    case CSSWrapShape::CSS_WRAP_SHAPE_ELLIPSE: {
        const CSSWrapShapeEllipse* ellipseValue = static_cast<const CSSWrapShapeEllipse *>(wrapShapeValue);
        RefPtr<WrapShapeEllipse> ellipse = WrapShapeEllipse::create();

        ellipse->setLeft(convertToLength(styleResolver, ellipseValue->left()));
        ellipse->setTop(convertToLength(styleResolver, ellipseValue->top()));
        ellipse->setRadiusX(convertToLength(styleResolver, ellipseValue->radiusX()));
        ellipse->setRadiusY(convertToLength(styleResolver, ellipseValue->radiusY()));

        wrapShape = ellipse.release();
        break;
    }
    case CSSWrapShape::CSS_WRAP_SHAPE_POLYGON: {
        const CSSWrapShapePolygon* polygonValue = static_cast<const CSSWrapShapePolygon *>(wrapShapeValue);
        RefPtr<WrapShapePolygon> polygon = WrapShapePolygon::create();

        polygon->setWindRule(polygonValue->windRule());
        const Vector<RefPtr<CSSPrimitiveValue> >& values = polygonValue->values();
        for (unsigned i = 0; i < values.size(); i += 2)
            polygon->appendPoint(convertToLength(styleResolver, values.at(i).get()), convertToLength(styleResolver, values.at(i + 1).get()));

        wrapShape = polygon.release();
        break;
    }
    default:
        break;
    }
    return wrapShape.release();
}
}
