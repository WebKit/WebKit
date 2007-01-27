/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#ifdef SVG_SUPPORT

#include "SVGAngle.h"
#include "SVGTransform.h"
#include "SVGSVGElement.h"

#include <math.h>

using namespace WebCore;

SVGTransform::SVGTransform()
    : m_type(SVG_TRANSFORM_UNKNOWN)
    , m_angle(0)
{
}

SVGTransform::SVGTransform(const AffineTransform& matrix)
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

AffineTransform SVGTransform::matrix() const
{
    return m_matrix;
}

double SVGTransform::angle() const
{
    return m_angle;
}

void SVGTransform::setMatrix(const AffineTransform& matrix)
{
    m_type = SVG_TRANSFORM_MATRIX;
    m_angle = 0;

    m_matrix = matrix;
}

void SVGTransform::setTranslate(double tx, double ty)
{
    m_type = SVG_TRANSFORM_TRANSLATE;
    m_angle = 0;

    m_matrix.reset();
    m_matrix.translate(tx, ty);
}

FloatPoint SVGTransform::translate() const
{
    return FloatPoint(m_matrix.e(), m_matrix.f());
}

void SVGTransform::setScale(double sx, double sy)
{
    m_type = SVG_TRANSFORM_SCALE;
    m_angle = 0;

    m_matrix.reset();
    m_matrix.scale(sx, sy);
}

FloatSize SVGTransform::scale() const
{
    return FloatSize(m_matrix.a(), m_matrix.d());
}

void SVGTransform::setRotate(double angle, double cx, double cy)
{
    m_type = SVG_TRANSFORM_ROTATE;
    m_angle = angle;

    // TODO: toString() implementation, which can show cx, cy (need to be stored?)
    m_matrix.reset();
    m_matrix.translate(cx, cy);
    m_matrix.rotate(angle);
    m_matrix.translate(-cx, -cy);
}

void SVGTransform::setSkewX(double angle)
{
    m_type = SVG_TRANSFORM_SKEWX;
    m_angle = angle;

    m_matrix.reset();
    m_matrix.skewX(angle);
}

void SVGTransform::setSkewY(double angle)
{
    m_type = SVG_TRANSFORM_SKEWY;
    m_angle = angle;

    m_matrix.reset();
    m_matrix.skewY(angle);
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

