/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SVGTransformValue.h"

#include "FloatConversion.h"
#include "FloatPoint.h"
#include "FloatSize.h"
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

SVGTransformValue::SVGTransformValue() = default;

SVGTransformValue::SVGTransformValue(SVGTransformType type, ConstructionMode mode)
    : m_type(type)
{
    if (mode == ConstructZeroTransform)
        m_matrix = AffineTransform(0, 0, 0, 0, 0, 0);
}

SVGTransformValue::SVGTransformValue(const AffineTransform& matrix)
    : m_type(SVG_TRANSFORM_MATRIX)
    , m_matrix(matrix)
{
}

void SVGTransformValue::setMatrix(const AffineTransform& matrix)
{
    m_type = SVG_TRANSFORM_MATRIX;
    m_angle = 0;
    m_matrix = matrix;
}

void SVGTransformValue::updateSVGMatrix()
{
    // The underlying matrix has been changed, alter the transformation type.
    // Spec: In case the matrix object is changed directly (i.e., without using the methods on the SVGTransform interface itself)
    // then the type of the SVGTransform changes to SVG_TRANSFORM_MATRIX.
    m_type = SVG_TRANSFORM_MATRIX;
    m_angle = 0;
}

void SVGTransformValue::setTranslate(float tx, float ty)
{
    m_type = SVG_TRANSFORM_TRANSLATE;
    m_angle = 0;

    m_matrix.makeIdentity();
    m_matrix.translate(tx, ty);
}

FloatPoint SVGTransformValue::translate() const
{
    return FloatPoint::narrowPrecision(m_matrix.e(), m_matrix.f());
}

void SVGTransformValue::setScale(float sx, float sy)
{
    m_type = SVG_TRANSFORM_SCALE;
    m_angle = 0;
    m_center = FloatPoint();

    m_matrix.makeIdentity();
    m_matrix.scaleNonUniform(sx, sy);
}

FloatSize SVGTransformValue::scale() const
{
    return FloatSize::narrowPrecision(m_matrix.a(), m_matrix.d());
}

void SVGTransformValue::setRotate(float angle, float cx, float cy)
{
    m_type = SVG_TRANSFORM_ROTATE;
    m_angle = angle;
    m_center = FloatPoint(cx, cy);

    // TODO: toString() implementation, which can show cx, cy (need to be stored?)
    m_matrix.makeIdentity();
    m_matrix.translate(cx, cy);
    m_matrix.rotate(angle);
    m_matrix.translate(-cx, -cy);
}

void SVGTransformValue::setSkewX(float angle)
{
    m_type = SVG_TRANSFORM_SKEWX;
    m_angle = angle;

    m_matrix.makeIdentity();
    m_matrix.skewX(angle);
}

void SVGTransformValue::setSkewY(float angle)
{
    m_type = SVG_TRANSFORM_SKEWY;
    m_angle = angle;

    m_matrix.makeIdentity();
    m_matrix.skewY(angle);
}

const String& SVGTransformValue::transformTypePrefixForParsing(SVGTransformType type)
{
    switch (type) {
    case SVG_TRANSFORM_UNKNOWN:
        return emptyString();
    case SVG_TRANSFORM_MATRIX: {
        static NeverDestroyed<String> matrixString(MAKE_STATIC_STRING_IMPL("matrix("));
        return matrixString;
    }
    case SVG_TRANSFORM_TRANSLATE: {
        static NeverDestroyed<String> translateString(MAKE_STATIC_STRING_IMPL("translate("));
        return translateString;
    }
    case SVG_TRANSFORM_SCALE: {
        static NeverDestroyed<String> scaleString(MAKE_STATIC_STRING_IMPL("scale("));
        return scaleString;
    }
    case SVG_TRANSFORM_ROTATE: {
        static NeverDestroyed<String> rotateString(MAKE_STATIC_STRING_IMPL("rotate("));
        return rotateString;
    }    
    case SVG_TRANSFORM_SKEWX: {
        static NeverDestroyed<String> skewXString(MAKE_STATIC_STRING_IMPL("skewX("));
        return skewXString;
    }
    case SVG_TRANSFORM_SKEWY: {
        static NeverDestroyed<String> skewYString(MAKE_STATIC_STRING_IMPL("skewY("));
        return skewYString;
    }
    }

    ASSERT_NOT_REACHED();
    return emptyString();
}

String SVGTransformValue::valueAsString() const
{
    const String& prefix = transformTypePrefixForParsing(m_type);
    switch (m_type) {
    case SVG_TRANSFORM_UNKNOWN:
        return prefix;
    case SVG_TRANSFORM_MATRIX: {
        StringBuilder builder;
        builder.append(prefix);
        builder.appendFixedPrecisionNumber(m_matrix.a());
        builder.append(' ');
        builder.appendFixedPrecisionNumber(m_matrix.b());
        builder.append(' ');
        builder.appendFixedPrecisionNumber(m_matrix.c());
        builder.append(' ');
        builder.appendFixedPrecisionNumber(m_matrix.d());
        builder.append(' ');
        builder.appendFixedPrecisionNumber(m_matrix.e());
        builder.append(' ');
        builder.appendFixedPrecisionNumber(m_matrix.f());
        builder.append(')');
        return builder.toString();
    }
    case SVG_TRANSFORM_TRANSLATE: {
        StringBuilder builder;
        builder.append(prefix);
        builder.appendFixedPrecisionNumber(m_matrix.e());
        builder.append(' ');
        builder.appendFixedPrecisionNumber(m_matrix.f());
        builder.append(')');
        return builder.toString();
    }
    case SVG_TRANSFORM_SCALE: {
        StringBuilder builder;
        builder.append(prefix);
        builder.appendFixedPrecisionNumber(m_matrix.xScale());
        builder.append(' ');
        builder.appendFixedPrecisionNumber(m_matrix.yScale());
        builder.append(')');
        return builder.toString();
    }
    case SVG_TRANSFORM_ROTATE: {
        double angleInRad = deg2rad(m_angle);
        double cosAngle = std::cos(angleInRad);
        double sinAngle = std::sin(angleInRad);
        float cx = narrowPrecisionToFloat(cosAngle != 1 ? (m_matrix.e() * (1 - cosAngle) - m_matrix.f() * sinAngle) / (1 - cosAngle) / 2 : 0);
        float cy = narrowPrecisionToFloat(cosAngle != 1 ? (m_matrix.e() * sinAngle / (1 - cosAngle) + m_matrix.f()) / 2 : 0);
        StringBuilder builder;
        builder.append(prefix);
        builder.appendFixedPrecisionNumber(m_angle);
        if (cx || cy) {
            builder.append(' ');
            builder.appendFixedPrecisionNumber(cx);
            builder.append(' ');
            builder.appendFixedPrecisionNumber(cy);
        }
        builder.append(')');
        return builder.toString();
    }
    case SVG_TRANSFORM_SKEWX:
    case SVG_TRANSFORM_SKEWY: {
        StringBuilder builder;
        builder.append(prefix);
        builder.appendFixedPrecisionNumber(m_angle);
        builder.append(')');
        return builder.toString();
    }
    }

    ASSERT_NOT_REACHED();
    return emptyString();
}

} // namespace WebCore
