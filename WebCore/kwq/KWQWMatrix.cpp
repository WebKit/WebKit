/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
#include "KWQWMatrix.h"

#include "IntRect.h"

static const double deg2rad = 0.017453292519943295769; // pi/180

QWMatrix::QWMatrix()
{
    m_transform = CGAffineTransformIdentity;
}

QWMatrix::QWMatrix(double a, double b, double c, double d, double tx, double ty)
{
    m_transform = CGAffineTransformMake(a,b,c,d,tx,ty);
}

QWMatrix::QWMatrix(CGAffineTransform t) {
    m_transform = t;
}

void QWMatrix::setMatrix(double a, double b, double c, double d, double tx, double ty)
{
    m_transform = CGAffineTransformMake(a,b,c,d,tx,ty);
}

void QWMatrix::map(double x, double y, double *x2, double *y2) const
{
    CGPoint result = CGPointApplyAffineTransform(CGPointMake(x,y),m_transform);
    *x2 = result.x;
    *y2 = result.y;
}

IntRect QWMatrix::mapRect(const IntRect &rect) const
{
    return IntRect(CGRectApplyAffineTransform(CGRect(rect), m_transform));
}

FloatRect QWMatrix::mapRect(const FloatRect &rect) const
{
    return FloatRect(CGRectApplyAffineTransform(CGRect(rect), m_transform));
}

bool QWMatrix::isIdentity() const
{
    return CGAffineTransformIsIdentity(m_transform);
}

void QWMatrix::reset()
{
    m_transform = CGAffineTransformIdentity;
}

QWMatrix &QWMatrix::scale(double sx, double sy)
{
    m_transform = CGAffineTransformScale(m_transform, sx, sy);
    return *this;
}

QWMatrix &QWMatrix::rotate(double d)
{
    m_transform = CGAffineTransformRotate(m_transform, d * deg2rad);
    return *this;
}

QWMatrix &QWMatrix::translate(double tx, double ty)
{
    m_transform = CGAffineTransformTranslate(m_transform, tx, ty);
    return *this;
}

QWMatrix &QWMatrix::shear(double sx, double sy)
{
    CGAffineTransform shear = CGAffineTransformMake(1, sy, sx, 1, 0, 0);
    m_transform = CGAffineTransformConcat(shear,m_transform);
    return *this;
}

double QWMatrix::det() const
{
    return m_transform.a * m_transform.d - m_transform.b * m_transform.c;
}

bool QWMatrix::isInvertible() const
{
    return det() == 0.0;
}

QWMatrix QWMatrix::invert() const
{
    if (isInvertible())
        return QWMatrix(CGAffineTransformInvert(m_transform));
    return QWMatrix();
}

QWMatrix::operator CGAffineTransform() const
{
    return m_transform;
}

bool QWMatrix::operator== (const QWMatrix &m2) const
{
    return CGAffineTransformEqualToTransform(m_transform, CGAffineTransform(m2));
}

QWMatrix &QWMatrix::operator*= (const QWMatrix &m2)
{
    m_transform = CGAffineTransformConcat(m_transform, CGAffineTransform(m2));
    return *this;
}

QWMatrix QWMatrix::operator* (const QWMatrix &m2)
{
    return CGAffineTransformConcat(m_transform, CGAffineTransform(m2));
}
