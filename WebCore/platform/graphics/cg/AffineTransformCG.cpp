/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc.  All rights reserved.
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

#if PLATFORM(CG)

#include "FloatConversion.h"
#include "FloatRect.h"
#include "IntRect.h"

#include <wtf/MathExtras.h>

namespace WebCore {

TransformationMatrix::TransformationMatrix()
    : m_transform(CGAffineTransformIdentity)
{
}

TransformationMatrix::TransformationMatrix(double a, double b, double c, double d, double tx, double ty)
{
    m_transform = CGAffineTransformMake(narrowPrecisionToCGFloat(a),
                                        narrowPrecisionToCGFloat(b),
                                        narrowPrecisionToCGFloat(c),
                                        narrowPrecisionToCGFloat(d),
                                        narrowPrecisionToCGFloat(tx),
                                        narrowPrecisionToCGFloat(ty));
}

TransformationMatrix::TransformationMatrix(const PlatformTransformationMatrix& t)
    : m_transform(t)
{
}

void TransformationMatrix::setMatrix(double a, double b, double c, double d, double tx, double ty)
{
    m_transform = CGAffineTransformMake(narrowPrecisionToCGFloat(a),
                                        narrowPrecisionToCGFloat(b),
                                        narrowPrecisionToCGFloat(c),
                                        narrowPrecisionToCGFloat(d),
                                        narrowPrecisionToCGFloat(tx),
                                        narrowPrecisionToCGFloat(ty));
}

void TransformationMatrix::map(double x, double y, double *x2, double *y2) const
{
    CGPoint result = CGPointApplyAffineTransform(CGPointMake(narrowPrecisionToCGFloat(x), narrowPrecisionToCGFloat(y)), m_transform);
    *x2 = result.x;
    *y2 = result.y;
}

IntRect TransformationMatrix::mapRect(const IntRect &rect) const
{
    return enclosingIntRect(CGRectApplyAffineTransform(CGRect(rect), m_transform));
}

FloatRect TransformationMatrix::mapRect(const FloatRect &rect) const
{
    return FloatRect(CGRectApplyAffineTransform(CGRect(rect), m_transform));
}

bool TransformationMatrix::isIdentity() const
{
    return CGAffineTransformIsIdentity(m_transform);
}

double TransformationMatrix::a() const
{
    return m_transform.a;
}

void TransformationMatrix::setA(double a)
{
    m_transform.a = narrowPrecisionToCGFloat(a);
}

double TransformationMatrix::b() const
{
    return m_transform.b;
}

void TransformationMatrix::setB(double b)
{
    m_transform.b = narrowPrecisionToCGFloat(b);
}

double TransformationMatrix::c() const
{
    return m_transform.c;
}

void TransformationMatrix::setC(double c)
{
    m_transform.c = narrowPrecisionToCGFloat(c);
}

double TransformationMatrix::d() const
{
    return m_transform.d;
}

void TransformationMatrix::setD(double d)
{
    m_transform.d = narrowPrecisionToCGFloat(d);
}

double TransformationMatrix::e() const
{
    return m_transform.tx;
}

void TransformationMatrix::setE(double e)
{
    m_transform.tx = narrowPrecisionToCGFloat(e);
}

double TransformationMatrix::f() const
{
    return m_transform.ty;
}

void TransformationMatrix::setF(double f)
{
    m_transform.ty = narrowPrecisionToCGFloat(f);
}

void TransformationMatrix::reset()
{
    m_transform = CGAffineTransformIdentity;
}

TransformationMatrix &TransformationMatrix::scale(double sx, double sy)
{
    m_transform = CGAffineTransformScale(m_transform, narrowPrecisionToCGFloat(sx), narrowPrecisionToCGFloat(sy));
    return *this;
}

TransformationMatrix &TransformationMatrix::rotate(double d)
{
    m_transform = CGAffineTransformRotate(m_transform, narrowPrecisionToCGFloat(deg2rad(d)));
    return *this;
}

TransformationMatrix &TransformationMatrix::translate(double tx, double ty)
{
    m_transform = CGAffineTransformTranslate(m_transform, narrowPrecisionToCGFloat(tx), narrowPrecisionToCGFloat(ty));
    return *this;
}

TransformationMatrix &TransformationMatrix::shear(double sx, double sy)
{
    CGAffineTransform shear = CGAffineTransformMake(1.0f, narrowPrecisionToCGFloat(sy), narrowPrecisionToCGFloat(sx), 1.0f, 0.0f, 0.0f);
    m_transform = CGAffineTransformConcat(shear, m_transform);
    return *this;
}

double TransformationMatrix::det() const
{
    return m_transform.a * m_transform.d - m_transform.b * m_transform.c;
}

TransformationMatrix TransformationMatrix::inverse() const
{
    if (isInvertible())
        return TransformationMatrix(CGAffineTransformInvert(m_transform));
    return TransformationMatrix();
}

TransformationMatrix::operator PlatformTransformationMatrix() const
{
    return m_transform;
}

bool TransformationMatrix::operator== (const TransformationMatrix &m2) const
{
    return CGAffineTransformEqualToTransform(m_transform, CGAffineTransform(m2));
}

TransformationMatrix &TransformationMatrix::operator*= (const TransformationMatrix &m2)
{
    m_transform = CGAffineTransformConcat(m_transform, CGAffineTransform(m2));
    return *this;
}

TransformationMatrix TransformationMatrix::operator* (const TransformationMatrix &m2)
{
    return CGAffineTransformConcat(m_transform, CGAffineTransform(m2));
}

}

#endif // PLATFORM(CG)
