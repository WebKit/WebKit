/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#if ENABLE(SVG)

#include "FloatPoint.h"
#include "FloatSize.h"
#include "SVGAngle.h"
#include "SVGSVGElement.h"
#include "SVGTransform.h"

#include <math.h>

using namespace WebCore;

SVGTransform::SVGTransform()
    : m_type(SVG_TRANSFORM_UNKNOWN)
    , m_angle(0)
{
}

SVGTransform::SVGTransform(SVGTransformType type)
    : m_type(type)
    , m_angle(0)
    , m_center(FloatPoint())
    , m_matrix(TransformationMatrix())
{
}

SVGTransform::SVGTransform(const TransformationMatrix& matrix)
    : m_type(SVG_TRANSFORM_MATRIX)
    , m_angle(0)
    , m_matrix(matrix)
{
}

SVGTransform::~SVGTransform()
{
}

bool SVGTransform::isValid()
{
    return (m_type != SVG_TRANSFORM_UNKNOWN);
}

SVGTransform::SVGTransformType SVGTransform::type() const
{
    return m_type;
}

TransformationMatrix SVGTransform::matrix() const
{
    return m_matrix;
}

float SVGTransform::angle() const
{
    return m_angle;
}

FloatPoint SVGTransform::rotationCenter() const
{
    return m_center;
}

void SVGTransform::setMatrix(TransformationMatrix matrix) //const TransformationMatrix& matrix)
{
    m_type = SVG_TRANSFORM_MATRIX;
    m_angle = 0;

    m_matrix = matrix;
}

void SVGTransform::setTranslate(float tx, float ty)
{
    m_type = SVG_TRANSFORM_TRANSLATE;
    m_angle = 0;

    m_matrix.makeIdentity();
    m_matrix.translate(tx, ty);
}

FloatPoint SVGTransform::translate() const
{
    return FloatPoint::narrowPrecision(m_matrix.e(), m_matrix.f());
}

void SVGTransform::setScale(float sx, float sy)
{
    m_type = SVG_TRANSFORM_SCALE;
    m_angle = 0;
    m_center = FloatPoint();

    m_matrix.makeIdentity();
    m_matrix.scaleNonUniform(sx, sy);
}

FloatSize SVGTransform::scale() const
{
    return FloatSize::narrowPrecision(m_matrix.a(), m_matrix.d());
}

void SVGTransform::setRotate(float angle, float cx, float cy)
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

void SVGTransform::setSkewX(float angle)
{
    m_type = SVG_TRANSFORM_SKEWX;
    m_angle = angle;

    m_matrix.makeIdentity();
    m_matrix.skewX(angle);
}

void SVGTransform::setSkewY(float angle)
{
    m_type = SVG_TRANSFORM_SKEWY;
    m_angle = angle;

    m_matrix.makeIdentity();
    m_matrix.skewY(angle);
}

// vim:ts=4:noet
#endif // ENABLE(SVG)

