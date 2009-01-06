/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "TransformationMatrix.h"

#include "IntRect.h"
#include "FloatRect.h"

namespace WebCore {

TransformationMatrix::TransformationMatrix()
    : m_transform()
{
}

TransformationMatrix::TransformationMatrix(double a, double b, double c, double d, double tx, double ty)
    : m_transform(a, b, c, d, tx, ty)
{
}

TransformationMatrix::TransformationMatrix(const PlatformAffineTransform& matrix)
    : m_transform(matrix)
{
}

void TransformationMatrix::setMatrix(double a, double b, double c, double d, double tx, double ty)
{
    m_transform.setMatrix(a, b, c, d, tx, ty);
}

void TransformationMatrix::map(double x, double y, double* x2, double* y2) const
{
    qreal tx2, ty2;
    m_transform.map(qreal(x), qreal(y), &tx2, &ty2);
    *x2 = tx2;
    *y2 = ty2;
}

IntRect TransformationMatrix::mapRect(const IntRect& rect) const
{
    return m_transform.mapRect(rect);
}

FloatRect TransformationMatrix::mapRect(const FloatRect& rect) const
{
    return m_transform.mapRect(rect);
}

bool TransformationMatrix::isIdentity() const
{
    return m_transform.isIdentity();
}

double TransformationMatrix::a() const
{
    return m_transform.m11();
}

void TransformationMatrix::setA(double a)
{
    m_transform.setMatrix(a, b(), c(), d(), e(), f());
}

double TransformationMatrix::b() const
{
    return m_transform.m12();
}

void TransformationMatrix::setB(double b)
{
    m_transform.setMatrix(a(), b, c(), d(), e(), f());
}

double TransformationMatrix::c() const
{
    return m_transform.m21();
}

void TransformationMatrix::setC(double c)
{
    m_transform.setMatrix(a(), b(), c, d(), e(), f());
}

double TransformationMatrix::d() const
{
    return m_transform.m22();
}

void TransformationMatrix::setD(double d)
{
    m_transform.setMatrix(a(), b(), c(), d, e(), f());
}

double TransformationMatrix::e() const
{
    return m_transform.dx();
}

void TransformationMatrix::setE(double e)
{
    m_transform.setMatrix(a(), b(), c(), d(), e, f());
}

double TransformationMatrix::f() const
{
    return m_transform.dy();
}

void TransformationMatrix::setF(double f)
{
    m_transform.setMatrix(a(), b(), c(), d(), e(), f);
}

void TransformationMatrix::reset()
{
    m_transform.reset();
}

TransformationMatrix& TransformationMatrix::scale(double sx, double sy)
{
    m_transform.scale(sx, sy);
    return *this;
}

TransformationMatrix& TransformationMatrix::rotate(double d)
{
    m_transform.rotate(d);
    return *this;
}

TransformationMatrix& TransformationMatrix::translate(double tx, double ty)
{
    m_transform.translate(tx, ty);
    return *this;
}

TransformationMatrix& TransformationMatrix::shear(double sx, double sy)
{
    m_transform.shear(sx, sy);
    return *this;
}

double TransformationMatrix::det() const
{
    return m_transform.det();
}

TransformationMatrix TransformationMatrix::inverse() const
{
    if(!isInvertible())
        return TransformationMatrix();

    return m_transform.inverted();
}

TransformationMatrix::operator QMatrix() const
{
    return m_transform;
}

bool TransformationMatrix::operator==(const TransformationMatrix& other) const
{
    return m_transform == other.m_transform;
}

TransformationMatrix& TransformationMatrix::operator*=(const TransformationMatrix& other)
{
    m_transform *= other.m_transform;
    return *this;
}

TransformationMatrix TransformationMatrix::operator*(const TransformationMatrix& other)
{
    return m_transform * other.m_transform;
}

}

// vim: ts=4 sw=4 et
