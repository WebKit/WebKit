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
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#if SVG_SUPPORT
#include <math.h>
#include <qwmatrix.h>

#include "KCanvasPath.h"
#include "KCanvasMatrix.h"

namespace WebCore {

static const double deg2rad = 0.017453292519943295769; // pi/180

// KCanvasMatrix
KCanvasMatrix::KCanvasMatrix()
{
    m_mode = OPS_PREMUL;
}

KCanvasMatrix::KCanvasMatrix(const QMatrix &qmatrix)
{
    m_mode = OPS_PREMUL;
    (*this) = qmatrix;
}

KCanvasMatrix::KCanvasMatrix(const KCanvasMatrix &matrix)
{
    m_mode = OPS_PREMUL;
    (*this) = matrix;
}

KCanvasMatrix::KCanvasMatrix(double a, double b, double c, double d, double e, double f)
{
    m_mode = OPS_PREMUL;
    m_matrix.setMatrix(a, b, c, d, e, f);
}

KCanvasMatrix::~KCanvasMatrix()
{
}

KCanvasMatrix &KCanvasMatrix::operator=(const QMatrix &other)
{
    m_matrix = other;
    return *this;
}

KCanvasMatrix &KCanvasMatrix::operator=(const KCanvasMatrix &other)
{
    m_matrix = other.m_matrix;
    return *this;
}

bool KCanvasMatrix::operator==(const QMatrix &other) const
{
    return (m_matrix == other);
}

bool KCanvasMatrix::operator!=(const QMatrix &other) const
{
    return !operator==(other);
}

bool KCanvasMatrix::operator==(const KCanvasMatrix &other) const
{
    return (m_matrix == other.m_matrix);
}

bool KCanvasMatrix::operator!=(const KCanvasMatrix &other) const
{
    return !operator==(other);
}

void KCanvasMatrix::setOperationMode(KCMatrixOperationMode mode)
{
    m_mode = mode;
}    

void KCanvasMatrix::setA(double a)
{
    m_matrix.setMatrix(a, m_matrix.m12(), m_matrix.m21(), m_matrix.m22(), m_matrix.dx(), m_matrix.dy());
}

double KCanvasMatrix::a() const
{
    return m_matrix.m11();
}

void KCanvasMatrix::setB(double b)
{
    m_matrix.setMatrix(m_matrix.m11(), b, m_matrix.m21(), m_matrix.m22(), m_matrix.dx(), m_matrix.dy());
}

double KCanvasMatrix::b() const
{
    return m_matrix.m12();
}

void KCanvasMatrix::setC(double c)
{
    m_matrix.setMatrix(m_matrix.m11(), m_matrix.m12(), c, m_matrix.m22(), m_matrix.dx(), m_matrix.dy());
}

double KCanvasMatrix::c() const
{
    return m_matrix.m21();
}

void KCanvasMatrix::setD(double d)
{
    m_matrix.setMatrix(m_matrix.m11(), m_matrix.m12(), m_matrix.m21(), d, m_matrix.dx(), m_matrix.dy());
}

double KCanvasMatrix::d() const
{
    return m_matrix.m22();
}

void KCanvasMatrix::setE(double e)
{
    m_matrix.setMatrix(m_matrix.m11(), m_matrix.m12(), m_matrix.m21(), m_matrix.m22(), e, m_matrix.dy());
}

double KCanvasMatrix::e() const
{
    return m_matrix.dx();
}

void KCanvasMatrix::setF(double f)
{
    m_matrix.setMatrix(m_matrix.m11(), m_matrix.m12(), m_matrix.m21(), m_matrix.m22(), m_matrix.dx(), f);
}

double KCanvasMatrix::f() const
{
    return m_matrix.dy();
}

KCanvasMatrix &KCanvasMatrix::translate(double x, double y)
{
    if(m_mode == OPS_PREMUL)
        m_matrix.translate(x, y);
    else
    {
        QMatrix temp;
        temp.translate(x, y);
        m_matrix *= temp;
    }

    return *this;
}

KCanvasMatrix &KCanvasMatrix::multiply(const KCanvasMatrix &other)
{
    QMatrix temp(other.a(), other.b(), other.c(), other.d(), other.e(), other.f());

    if(m_mode == OPS_PREMUL)
    {
        temp *= m_matrix;
        m_matrix = temp;
    }
    else
        m_matrix *= temp;

    return *this;
}

KCanvasMatrix &KCanvasMatrix::scale(double scaleFactorX, double scaleFactorY)
{
    if(m_mode == OPS_PREMUL)
        m_matrix.scale(scaleFactorX, scaleFactorY);
    else
    {
        QMatrix temp;
        temp.scale(scaleFactorX, scaleFactorY);
        m_matrix *= temp;
    }

    return *this;
}

KCanvasMatrix &KCanvasMatrix::rotate(double angle)
{
    if(m_mode == OPS_PREMUL)
        m_matrix.rotate(angle);
    else
    {
        QMatrix temp;
        temp.rotate(angle);
        m_matrix *= temp;
    }

    return *this;
}

KCanvasMatrix &KCanvasMatrix::rotateFromVector(double x, double y)
{
    if(m_mode == OPS_PREMUL)
        m_matrix.rotate(atan2(y, x) / deg2rad);
    else
    {
        QMatrix temp;
        temp.rotate(atan2(y, x) / deg2rad);
        m_matrix *= temp;
    }

    return *this;
}

KCanvasMatrix &KCanvasMatrix::flipX()
{
    return scale(-1.0f, 1.0f);
}

KCanvasMatrix &KCanvasMatrix::flipY()
{
    return scale(1.0f, -1.0f);
}

KCanvasMatrix &KCanvasMatrix::skewX(double angle)
{
    if(m_mode == OPS_PREMUL)
        m_matrix.shear(tan(angle * deg2rad), 0.0f);
    else
    {
        QMatrix temp;
        temp.shear(tan(angle * deg2rad), 0.0f);
        m_matrix *= temp;
    }

    return *this;
}

KCanvasMatrix &KCanvasMatrix::skewY(double angle)
{
    if(m_mode == OPS_PREMUL)
        m_matrix.shear(0.0f, tan(angle * deg2rad));
    else
    {
        QMatrix temp;
        temp.shear(0.0f, tan(angle * deg2rad));
        m_matrix *= temp;
    }

    return *this;
}

void KCanvasMatrix::reset()
{
    m_matrix.reset();
}

void KCanvasMatrix::removeScale(double *xScale, double *yScale)
{
    double sx = sqrt(a() * a() + b() *b());
    double sy = sqrt(c() * c() + d() *d());

    // Remove the scaling from the matrix.
    setA(a() / sx); setB(b() / sx);
    setC(c() / sy); setD(d() / sy);

    *xScale = sx; *yScale = sy;
}

QMatrix KCanvasMatrix::qmatrix() const
{
    return m_matrix;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

