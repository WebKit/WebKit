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

#include "FloatRect.h"
#include "IntRect.h"

static const double deg2rad = 0.017453292519943295769; // pi/180

QMatrix::QMatrix()
{
    m_transform = CGAffineTransformIdentity;
}

QMatrix::QMatrix(double a, double b, double c, double d, double tx, double ty)
{
    m_transform = CGAffineTransformMake(a,b,c,d,tx,ty);
}

QMatrix::QMatrix(CGAffineTransform t) {
    m_transform = t;
}

void QMatrix::setMatrix(double a, double b, double c, double d, double tx, double ty)
{
    m_transform = CGAffineTransformMake(a,b,c,d,tx,ty);
}

void QMatrix::map(double x, double y, double *x2, double *y2) const
{
    CGPoint result = CGPointApplyAffineTransform(CGPointMake(x,y),m_transform);
    *x2 = result.x;
    *y2 = result.y;
}

IntRect QMatrix::mapRect(const IntRect &rect) const
{
    return enclosingIntRect(CGRectApplyAffineTransform(CGRect(rect), m_transform));
}

FloatRect QMatrix::mapRect(const FloatRect &rect) const
{
    return FloatRect(CGRectApplyAffineTransform(CGRect(rect), m_transform));
}

bool QMatrix::isIdentity() const
{
    return CGAffineTransformIsIdentity(m_transform);
}

void QMatrix::reset()
{
    m_transform = CGAffineTransformIdentity;
}

QMatrix &QMatrix::scale(double sx, double sy)
{
    m_transform = CGAffineTransformScale(m_transform, sx, sy);
    return *this;
}

QMatrix &QMatrix::rotate(double d)
{
    m_transform = CGAffineTransformRotate(m_transform, d * deg2rad);
    return *this;
}

QMatrix &QMatrix::translate(double tx, double ty)
{
    m_transform = CGAffineTransformTranslate(m_transform, tx, ty);
    return *this;
}

QMatrix &QMatrix::shear(double sx, double sy)
{
    CGAffineTransform shear = CGAffineTransformMake(1, sy, sx, 1, 0, 0);
    m_transform = CGAffineTransformConcat(shear,m_transform);
    return *this;
}

double QMatrix::det() const
{
    return m_transform.a * m_transform.d - m_transform.b * m_transform.c;
}

bool QMatrix::isInvertible() const
{
    return det() != 0.0;
}

QMatrix QMatrix::invert() const
{
    if (isInvertible())
        return QMatrix(CGAffineTransformInvert(m_transform));
    return QMatrix();
}

QMatrix::operator CGAffineTransform() const
{
    return m_transform;
}

bool QMatrix::operator== (const QMatrix &m2) const
{
    return CGAffineTransformEqualToTransform(m_transform, CGAffineTransform(m2));
}

QMatrix &QMatrix::operator*= (const QMatrix &m2)
{
    m_transform = CGAffineTransformConcat(m_transform, CGAffineTransform(m2));
    return *this;
}

QMatrix QMatrix::operator* (const QMatrix &m2)
{
    return CGAffineTransformConcat(m_transform, CGAffineTransform(m2));
}
