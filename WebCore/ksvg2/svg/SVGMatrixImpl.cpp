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

#include <math.h>

#include "ksvg.h"
#include "SVGHelper.h"
#include "SVGAngleImpl.h"
#include "SVGMatrixImpl.h"

using namespace KSVG;

SVGMatrixImpl::SVGMatrixImpl() : KDOM::Shared()
{
}

SVGMatrixImpl::SVGMatrixImpl(QMatrix mat) : KDOM::Shared()
{
    m_mat = mat;
}

SVGMatrixImpl::SVGMatrixImpl(double a, double b, double c, double d, double e, double f) : KDOM::Shared()
{
    m_mat.setMatrix(a, b, c, d, e, f);
}

SVGMatrixImpl::~SVGMatrixImpl()
{
}

void SVGMatrixImpl::setA(double a)
{
    m_mat.setMatrix(a, m_mat.m12(), m_mat.m21(), m_mat.m22(), m_mat.dx(), m_mat.dy());
}

double SVGMatrixImpl::a() const
{
    return m_mat.m11();
}

void SVGMatrixImpl::setB(double b)
{
    m_mat.setMatrix(m_mat.m11(), b, m_mat.m21(), m_mat.m22(), m_mat.dx(), m_mat.dy());
}

double SVGMatrixImpl::b() const
{
    return m_mat.m12();
}

void SVGMatrixImpl::setC(double c)
{
    m_mat.setMatrix(m_mat.m11(), m_mat.m12(), c, m_mat.m22(), m_mat.dx(), m_mat.dy());
}

double SVGMatrixImpl::c() const
{
    return m_mat.m21();
}

void SVGMatrixImpl::setD(double d)
{
    m_mat.setMatrix(m_mat.m11(), m_mat.m12(), m_mat.m21(), d, m_mat.dx(), m_mat.dy());
}

double SVGMatrixImpl::d() const
{
    return m_mat.m22();
}

void SVGMatrixImpl::setE(double e)
{
    m_mat.setMatrix(m_mat.m11(), m_mat.m12(), m_mat.m21(), m_mat.m22(), e, m_mat.dy());
}

double SVGMatrixImpl::e() const
{
    return m_mat.dx();
}

void SVGMatrixImpl::setF(double f)
{
    m_mat.setMatrix(m_mat.m11(), m_mat.m12(), m_mat.m21(), m_mat.m22(), m_mat.dx(), f);
}

double SVGMatrixImpl::f() const
{
    return m_mat.dy();
}

void SVGMatrixImpl::copy(const SVGMatrixImpl *other)
{
    m_mat.setMatrix(other->m_mat.m11(), other->m_mat.m12(), other->m_mat.m21(), other->m_mat.m22(), other->m_mat.dx(), other->m_mat.dy());
}

SVGMatrixImpl *SVGMatrixImpl::postMultiply(const SVGMatrixImpl *secondMatrix)
{
    QMatrix temp(secondMatrix->a(), secondMatrix->b(), secondMatrix->c(), secondMatrix->d(), secondMatrix->e(), secondMatrix->f());
    m_mat *= temp;
    return this;
}

SVGMatrixImpl *SVGMatrixImpl::inverse()
{
    m_mat = m_mat.invert();
    return this;
}

SVGMatrixImpl *SVGMatrixImpl::postTranslate(double x,  double y)
{
    // Could optimise these.
    QMatrix temp;
    temp.translate(x, y);
    m_mat *= temp;
    return this;
}

SVGMatrixImpl *SVGMatrixImpl::postScale(double scaleFactor)
{
    QMatrix temp;
    temp.scale(scaleFactor, scaleFactor);
    m_mat *= temp;
    return this;
}

SVGMatrixImpl *SVGMatrixImpl::postScaleNonUniform(double scaleFactorX, double scaleFactorY)
{
    QMatrix temp;
    temp.scale(scaleFactorX, scaleFactorY);
    m_mat *= temp;
    return this;
}

SVGMatrixImpl *SVGMatrixImpl::postRotate(double angle)
{
    QMatrix temp;
    temp.rotate(angle);
    m_mat *= temp;
    return this;
}

SVGMatrixImpl *SVGMatrixImpl::postRotateFromVector(double x, double y)
{
    QMatrix temp;
    temp.rotate(SVGAngleImpl::todeg(atan2(y, x)));
    m_mat *= temp;
    return this;
}

SVGMatrixImpl *SVGMatrixImpl::postFlipX()
{
    QMatrix temp(-1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F);
    m_mat *= temp;
    return this;
}

SVGMatrixImpl *SVGMatrixImpl::postFlipY()
{
    QMatrix temp(1.0F, 0.0F, 0.0F, -1.0F, 0.0F, 0.0F);
    m_mat *= temp;
    return this;
}

SVGMatrixImpl *SVGMatrixImpl::postSkewX(double angle)
{
    QMatrix temp;
    temp.shear(tan(SVGAngleImpl::torad(angle)), 0.0F);
    m_mat *= temp;
    return this;
}

SVGMatrixImpl *SVGMatrixImpl::postSkewY(double angle)
{
    QMatrix temp;
    temp.shear(0.0F, tan(SVGAngleImpl::torad(angle)));
    m_mat *= temp;
    return this;
}

SVGMatrixImpl *SVGMatrixImpl::multiply(const SVGMatrixImpl *secondMatrix)
{
    QMatrix temp(secondMatrix->a(), secondMatrix->b(), secondMatrix->c(), secondMatrix->d(), secondMatrix->e(), secondMatrix->f());
    temp *= m_mat;
    m_mat = temp;
    return this;
}

SVGMatrixImpl *SVGMatrixImpl::translate(double x,  double y)
{
    m_mat.translate(x, y);
    return this;
}

SVGMatrixImpl *SVGMatrixImpl::scale(double scaleFactor)
{
    m_mat.scale(scaleFactor, scaleFactor);
    return this;
}

SVGMatrixImpl *SVGMatrixImpl::scaleNonUniform(double scaleFactorX, double scaleFactorY)
{
    m_mat.scale(scaleFactorX, scaleFactorY);
    return this;
}

SVGMatrixImpl *SVGMatrixImpl::rotate(double angle)
{
    m_mat.rotate(angle);
    return this;
}

SVGMatrixImpl *SVGMatrixImpl::rotateFromVector(double x, double y)
{
    m_mat.rotate(SVGAngleImpl::todeg(atan2(y, x)));
    return this;
}

SVGMatrixImpl *SVGMatrixImpl::flipX()
{
    m_mat.scale(-1.0f, 1.0f);
    return this;
}

SVGMatrixImpl *SVGMatrixImpl::flipY()
{
    m_mat.scale(1.0f, -1.0f);
    return this;
}

SVGMatrixImpl *SVGMatrixImpl::skewX(double angle)
{
    m_mat.shear(tan(SVGAngleImpl::torad(angle)), 0.0F);
    return this;
}

SVGMatrixImpl *SVGMatrixImpl::skewY(double angle)
{
    m_mat.shear(0.0F, tan(SVGAngleImpl::torad(angle)));
    return this;
}

void SVGMatrixImpl::setMatrix(QMatrix mat)
{
    m_mat = mat;
}

QMatrix &SVGMatrixImpl::qmatrix()
{
    return m_mat;
}

const QMatrix &SVGMatrixImpl::qmatrix() const
{
    return m_mat;
}

void SVGMatrixImpl::reset()
{
    m_mat.reset();
}

void SVGMatrixImpl::removeScale(double *xScale, double *yScale)
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
