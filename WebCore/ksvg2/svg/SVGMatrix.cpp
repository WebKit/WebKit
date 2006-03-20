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
#if SVG_SUPPORT
#include <math.h>

#include "ksvg.h"
#include "SVGHelper.h"
#include "SVGAngle.h"
#include "SVGMatrix.h"

using namespace WebCore;

SVGMatrix::SVGMatrix() : Shared<SVGMatrix>()
{
}

SVGMatrix::SVGMatrix(QMatrix mat) : Shared<SVGMatrix>()
{
    m_mat = mat;
}

SVGMatrix::SVGMatrix(double a, double b, double c, double d, double e, double f) : Shared<SVGMatrix>()
{
    m_mat.setMatrix(a, b, c, d, e, f);
}

SVGMatrix::~SVGMatrix()
{
}

void SVGMatrix::setA(double a)
{
    m_mat.setMatrix(a, m_mat.m12(), m_mat.m21(), m_mat.m22(), m_mat.dx(), m_mat.dy());
}

double SVGMatrix::a() const
{
    return m_mat.m11();
}

void SVGMatrix::setB(double b)
{
    m_mat.setMatrix(m_mat.m11(), b, m_mat.m21(), m_mat.m22(), m_mat.dx(), m_mat.dy());
}

double SVGMatrix::b() const
{
    return m_mat.m12();
}

void SVGMatrix::setC(double c)
{
    m_mat.setMatrix(m_mat.m11(), m_mat.m12(), c, m_mat.m22(), m_mat.dx(), m_mat.dy());
}

double SVGMatrix::c() const
{
    return m_mat.m21();
}

void SVGMatrix::setD(double d)
{
    m_mat.setMatrix(m_mat.m11(), m_mat.m12(), m_mat.m21(), d, m_mat.dx(), m_mat.dy());
}

double SVGMatrix::d() const
{
    return m_mat.m22();
}

void SVGMatrix::setE(double e)
{
    m_mat.setMatrix(m_mat.m11(), m_mat.m12(), m_mat.m21(), m_mat.m22(), e, m_mat.dy());
}

double SVGMatrix::e() const
{
    return m_mat.dx();
}

void SVGMatrix::setF(double f)
{
    m_mat.setMatrix(m_mat.m11(), m_mat.m12(), m_mat.m21(), m_mat.m22(), m_mat.dx(), f);
}

double SVGMatrix::f() const
{
    return m_mat.dy();
}

void SVGMatrix::copy(const SVGMatrix *other)
{
    m_mat.setMatrix(other->m_mat.m11(), other->m_mat.m12(), other->m_mat.m21(), other->m_mat.m22(), other->m_mat.dx(), other->m_mat.dy());
}

SVGMatrix *SVGMatrix::postMultiply(const SVGMatrix *secondMatrix)
{
    QMatrix temp(secondMatrix->a(), secondMatrix->b(), secondMatrix->c(), secondMatrix->d(), secondMatrix->e(), secondMatrix->f());
    m_mat *= temp;
    return this;
}

SVGMatrix *SVGMatrix::inverse()
{
    m_mat = m_mat.invert();
    return this;
}

SVGMatrix *SVGMatrix::postTranslate(double x,  double y)
{
    // Could optimise these.
    QMatrix temp;
    temp.translate(x, y);
    m_mat *= temp;
    return this;
}

SVGMatrix *SVGMatrix::postScale(double scaleFactor)
{
    QMatrix temp;
    temp.scale(scaleFactor, scaleFactor);
    m_mat *= temp;
    return this;
}

SVGMatrix *SVGMatrix::postScaleNonUniform(double scaleFactorX, double scaleFactorY)
{
    QMatrix temp;
    temp.scale(scaleFactorX, scaleFactorY);
    m_mat *= temp;
    return this;
}

SVGMatrix *SVGMatrix::postRotate(double angle)
{
    QMatrix temp;
    temp.rotate(angle);
    m_mat *= temp;
    return this;
}

SVGMatrix *SVGMatrix::postRotateFromVector(double x, double y)
{
    QMatrix temp;
    temp.rotate(SVGAngle::todeg(atan2(y, x)));
    m_mat *= temp;
    return this;
}

SVGMatrix *SVGMatrix::postFlipX()
{
    QMatrix temp(-1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F);
    m_mat *= temp;
    return this;
}

SVGMatrix *SVGMatrix::postFlipY()
{
    QMatrix temp(1.0F, 0.0F, 0.0F, -1.0F, 0.0F, 0.0F);
    m_mat *= temp;
    return this;
}

SVGMatrix *SVGMatrix::postSkewX(double angle)
{
    QMatrix temp;
    temp.shear(tan(SVGAngle::torad(angle)), 0.0F);
    m_mat *= temp;
    return this;
}

SVGMatrix *SVGMatrix::postSkewY(double angle)
{
    QMatrix temp;
    temp.shear(0.0F, tan(SVGAngle::torad(angle)));
    m_mat *= temp;
    return this;
}

SVGMatrix *SVGMatrix::multiply(const SVGMatrix *secondMatrix)
{
    QMatrix temp(secondMatrix->a(), secondMatrix->b(), secondMatrix->c(), secondMatrix->d(), secondMatrix->e(), secondMatrix->f());
    temp *= m_mat;
    m_mat = temp;
    return this;
}

SVGMatrix *SVGMatrix::translate(double x,  double y)
{
    m_mat.translate(x, y);
    return this;
}

SVGMatrix *SVGMatrix::scale(double scaleFactor)
{
    m_mat.scale(scaleFactor, scaleFactor);
    return this;
}

SVGMatrix *SVGMatrix::scaleNonUniform(double scaleFactorX, double scaleFactorY)
{
    m_mat.scale(scaleFactorX, scaleFactorY);
    return this;
}

SVGMatrix *SVGMatrix::rotate(double angle)
{
    m_mat.rotate(angle);
    return this;
}

SVGMatrix *SVGMatrix::rotateFromVector(double x, double y)
{
    m_mat.rotate(SVGAngle::todeg(atan2(y, x)));
    return this;
}

SVGMatrix *SVGMatrix::flipX()
{
    m_mat.scale(-1.0f, 1.0f);
    return this;
}

SVGMatrix *SVGMatrix::flipY()
{
    m_mat.scale(1.0f, -1.0f);
    return this;
}

SVGMatrix *SVGMatrix::skewX(double angle)
{
    m_mat.shear(tan(SVGAngle::torad(angle)), 0.0F);
    return this;
}

SVGMatrix *SVGMatrix::skewY(double angle)
{
    m_mat.shear(0.0F, tan(SVGAngle::torad(angle)));
    return this;
}

void SVGMatrix::setMatrix(QMatrix mat)
{
    m_mat = mat;
}

QMatrix &SVGMatrix::qmatrix()
{
    return m_mat;
}

const QMatrix &SVGMatrix::qmatrix() const
{
    return m_mat;
}

void SVGMatrix::reset()
{
    m_mat.reset();
}

void SVGMatrix::removeScale(double *xScale, double *yScale)
{
    double sx = sqrt(a() * a() + b() * b());
    double sy = sqrt(c() * c() + d() * d());

    // Remove the scaling from the matrix.
    setA(a() / sx); setB(b() / sx);
    setC(c() / sy); setD(d() / sy);

    *xScale = sx;
    *yScale = sy;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

