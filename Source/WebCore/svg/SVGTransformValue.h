/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2019 Apple Inc.  All rights reserved.
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

#pragma once

#include "FloatConversion.h"
#include "FloatPoint.h"
#include "FloatSize.h"
#include "SVGMatrix.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

class FloatSize;

class SVGTransformValue {
public:
    enum SVGTransformType {
        SVG_TRANSFORM_UNKNOWN = 0,
        SVG_TRANSFORM_MATRIX = 1,
        SVG_TRANSFORM_TRANSLATE = 2,
        SVG_TRANSFORM_SCALE = 3,
        SVG_TRANSFORM_ROTATE = 4,
        SVG_TRANSFORM_SKEWX = 5,
        SVG_TRANSFORM_SKEWY = 6
    };

    enum ConstructionMode {
        ConstructIdentityTransform,
        ConstructZeroTransform
    };

    SVGTransformValue(SVGTransformType type = SVG_TRANSFORM_MATRIX, const AffineTransform& transform = { })
        : m_type(type)
        , m_matrix(SVGMatrix::create(transform))
    {
    }

    SVGTransformValue(const SVGTransformValue& other)
        : m_type(other.m_type)
        , m_matrix(SVGMatrix::create(other.matrix()->value()))
        , m_angle(other.m_angle)
        , m_rotationCenter(other.m_rotationCenter)
    {
    }

    SVGTransformValue(SVGTransformType type, Ref<SVGMatrix>&& matrix, float angle, const FloatPoint& rotationCenter)
        : m_type(type)
        , m_matrix(WTFMove(matrix))
        , m_angle(angle)
        , m_rotationCenter(rotationCenter)
    {
    }

    SVGTransformValue(SVGTransformValue&& other)
        : m_type(other.m_type)
        , m_matrix(other.m_matrix.copyRef())
        , m_angle(other.m_angle)
        , m_rotationCenter(other.m_rotationCenter)
    {
    }

    SVGTransformValue& operator=(const SVGTransformValue& other)
    {
        m_type = other.m_type;
        m_matrix->setValue(other.m_matrix->value());
        m_angle = other.m_angle;
        m_rotationCenter = other.m_rotationCenter;
        return *this;
    }

    SVGTransformType type() const { return m_type; }
    const Ref<SVGMatrix>& matrix() const { return m_matrix; }
    float angle() const { return m_angle; }
    FloatPoint rotationCenter() const { return m_rotationCenter; }

    bool isValid() const { return m_type != SVG_TRANSFORM_UNKNOWN; }

    void setMatrix(const AffineTransform& matrix)
    {
        m_type = SVG_TRANSFORM_MATRIX;
        m_angle = 0;
        m_rotationCenter = FloatPoint();
        m_matrix->setValue(matrix);
    }

    void matrixDidChange()
    {
        // The underlying matrix has been changed, alter the transformation type.
        // Spec: In case the matrix object is changed directly (i.e., without using the methods on the SVGTransform interface itself)
        // then the type of the SVGTransform changes to SVG_TRANSFORM_MATRIX.
        m_type = SVG_TRANSFORM_MATRIX;
        m_angle = 0;
        m_rotationCenter = FloatPoint();
    }

    FloatPoint translate() const
    {
        return FloatPoint::narrowPrecision(m_matrix->e(), m_matrix->f());
    }

    void setTranslate(float tx, float ty)
    {
        m_type = SVG_TRANSFORM_TRANSLATE;
        m_angle = 0;
        m_rotationCenter = FloatPoint();
        
        m_matrix->value().makeIdentity();
        m_matrix->value().translate(tx, ty);
    }

    FloatSize scale() const
    {
        return FloatSize::narrowPrecision(m_matrix->a(), m_matrix->d());
    }

    void setScale(float sx, float sy)
    {
        m_type = SVG_TRANSFORM_SCALE;
        m_angle = 0;
        m_rotationCenter = FloatPoint();
        
        m_matrix->value().makeIdentity();
        m_matrix->value().scaleNonUniform(sx, sy);
    }

    void setRotate(float angle, float cx, float cy)
    {
        m_type = SVG_TRANSFORM_ROTATE;
        m_angle = angle;
        m_rotationCenter = FloatPoint(cx, cy);

        // TODO: toString() implementation, which can show cx, cy (need to be stored?)
        m_matrix->value().makeIdentity();
        m_matrix->value().translate(cx, cy);
        m_matrix->value().rotate(angle);
        m_matrix->value().translate(-cx, -cy);
    }

    void setSkewX(float angle)
    {
        m_type = SVG_TRANSFORM_SKEWX;
        m_angle = angle;
        m_rotationCenter = FloatPoint();
        
        m_matrix->value().makeIdentity();
        m_matrix->value().skewX(angle);
    }

    void setSkewY(float angle)
    {
        m_type = SVG_TRANSFORM_SKEWY;
        m_angle = angle;
        m_rotationCenter = FloatPoint();
        
        m_matrix->value().makeIdentity();
        m_matrix->value().skewY(angle);
    }

    String valueAsString() const
    {
        StringBuilder builder;
        builder.append(prefixForTransfromType(m_type));
        switch (m_type) {
        case SVG_TRANSFORM_UNKNOWN:
            break;
        case SVG_TRANSFORM_MATRIX:
            appendMatrix(builder);
            break;
        case SVG_TRANSFORM_TRANSLATE:
            appendTranslate(builder);
            break;
        case SVG_TRANSFORM_SCALE:
            appendScale(builder);
            break;
        case SVG_TRANSFORM_ROTATE:
            appendRotate(builder);
            break;
        case SVG_TRANSFORM_SKEWX:
            appendSkewX(builder);
            break;
        case SVG_TRANSFORM_SKEWY:
            appendSkewY(builder);
            break;
        }
        return builder.toString();
    }

    static String prefixForTransfromType(SVGTransformType type)
    {
        switch (type) {
        case SVG_TRANSFORM_UNKNOWN:
            return emptyString();
        case SVG_TRANSFORM_MATRIX:
            return "matrix("_s;
        case SVG_TRANSFORM_TRANSLATE:
            return "translate("_s;
        case SVG_TRANSFORM_SCALE:
            return "scale("_s;
        case SVG_TRANSFORM_ROTATE:
            return "rotate("_s;
        case SVG_TRANSFORM_SKEWX:
            return "skewX("_s;
        case SVG_TRANSFORM_SKEWY:
            return "skewY("_s;
        }
        ASSERT_NOT_REACHED();
        return emptyString();
    }

private:
    static void appendFixedPrecisionNumbers(StringBuilder& builder)
    {
        builder.append(')');
    }

    template<typename Number, typename... Numbers>
    static void appendFixedPrecisionNumbers(StringBuilder& builder, Number number, Numbers... numbers)
    {
        if (builder.length() && builder[builder.length() - 1] != '(')
            builder.append(' ');
        builder.appendFixedPrecisionNumber(number);
        appendFixedPrecisionNumbers(builder, numbers...);
    }

    void appendMatrix(StringBuilder& builder) const
    {
        appendFixedPrecisionNumbers(builder, m_matrix->a(), m_matrix->b(), m_matrix->c(), m_matrix->d(), m_matrix->e(), m_matrix->f());
    }

    void appendTranslate(StringBuilder& builder) const
    {
        appendFixedPrecisionNumbers(builder, m_matrix->e(), m_matrix->f());
    }

    void appendScale(StringBuilder& builder) const
    {
        appendFixedPrecisionNumbers(builder, m_matrix->value().xScale(), m_matrix->value().yScale());
    }

    void appendRotate(StringBuilder& builder) const
    {
        double angleInRad = deg2rad(m_angle);
        double cosAngle = std::cos(angleInRad);
        double sinAngle = std::sin(angleInRad);

        float cx = narrowPrecisionToFloat(cosAngle != 1 ? (m_matrix->e() * (1 - cosAngle) - m_matrix->f() * sinAngle) / (1 - cosAngle) / 2 : 0);
        float cy = narrowPrecisionToFloat(cosAngle != 1 ? (m_matrix->e() * sinAngle / (1 - cosAngle) + m_matrix->f()) / 2 : 0);

        if (cx || cy)
            appendFixedPrecisionNumbers(builder, m_angle, cx, cy);
        else
            appendFixedPrecisionNumbers(builder, m_angle);
    }

    void appendSkewX(StringBuilder& builder) const
    {
        appendFixedPrecisionNumbers(builder, m_angle);
    }

    void appendSkewY(StringBuilder& builder) const
    {
        appendFixedPrecisionNumbers(builder, m_angle);
    }

    SVGTransformType m_type { SVG_TRANSFORM_UNKNOWN };
    Ref<SVGMatrix> m_matrix;
    float m_angle { 0 };
    FloatPoint m_rotationCenter;
};

} // namespace WebCore
