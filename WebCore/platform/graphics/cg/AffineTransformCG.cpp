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
#include "AffineTransform.h"

#if PLATFORM(CG)

#include "FloatConversion.h"
#include "FloatRect.h"
#include "IntRect.h"

#include <wtf/MathExtras.h>

namespace WebCore {

AffineTransform::AffineTransform()
{
    m_transform = CGAffineTransformIdentity;
}

AffineTransform::AffineTransform(double a, double b, double c, double d, double tx, double ty)
{
    m_transform = CGAffineTransformMake(narrowPrecisionToCGFloat(a),
                                        narrowPrecisionToCGFloat(b),
                                        narrowPrecisionToCGFloat(c),
                                        narrowPrecisionToCGFloat(d),
                                        narrowPrecisionToCGFloat(tx),
                                        narrowPrecisionToCGFloat(ty));
}

AffineTransform::AffineTransform(CGAffineTransform t)
{
    m_transform = t;
}

void AffineTransform::setMatrix(double a, double b, double c, double d, double tx, double ty)
{
    m_transform = CGAffineTransformMake(narrowPrecisionToCGFloat(a),
                                        narrowPrecisionToCGFloat(b),
                                        narrowPrecisionToCGFloat(c),
                                        narrowPrecisionToCGFloat(d),
                                        narrowPrecisionToCGFloat(tx),
                                        narrowPrecisionToCGFloat(ty));
}

void AffineTransform::map(double x, double y, double *x2, double *y2) const
{
    CGPoint result = CGPointApplyAffineTransform(CGPointMake(narrowPrecisionToCGFloat(x), narrowPrecisionToCGFloat(y)), m_transform);
    *x2 = result.x;
    *y2 = result.y;
}

IntRect AffineTransform::mapRect(const IntRect &rect) const
{
    return enclosingIntRect(CGRectApplyAffineTransform(CGRect(rect), m_transform));
}

FloatRect AffineTransform::mapRect(const FloatRect &rect) const
{
    return FloatRect(CGRectApplyAffineTransform(CGRect(rect), m_transform));
}

bool AffineTransform::isIdentity() const
{
    return CGAffineTransformIsIdentity(m_transform);
}

double AffineTransform::a() const
{
    return m_transform.a;
}

void AffineTransform::setA(double a)
{
    m_transform.a = narrowPrecisionToCGFloat(a);
}

double AffineTransform::b() const
{
    return m_transform.b;
}

void AffineTransform::setB(double b)
{
    m_transform.b = narrowPrecisionToCGFloat(b);
}

double AffineTransform::c() const
{
    return m_transform.c;
}

void AffineTransform::setC(double c)
{
    m_transform.c = narrowPrecisionToCGFloat(c);
}

double AffineTransform::d() const
{
    return m_transform.d;
}

void AffineTransform::setD(double d)
{
    m_transform.d = narrowPrecisionToCGFloat(d);
}

double AffineTransform::e() const
{
    return m_transform.tx;
}

void AffineTransform::setE(double e)
{
    m_transform.tx = narrowPrecisionToCGFloat(e);
}

double AffineTransform::f() const
{
    return m_transform.ty;
}

void AffineTransform::setF(double f)
{
    m_transform.ty = narrowPrecisionToCGFloat(f);
}

void AffineTransform::reset()
{
    m_transform = CGAffineTransformIdentity;
}

AffineTransform &AffineTransform::scale(double sx, double sy)
{
    m_transform = CGAffineTransformScale(m_transform, narrowPrecisionToCGFloat(sx), narrowPrecisionToCGFloat(sy));
    return *this;
}

AffineTransform &AffineTransform::rotate(double d)
{
    m_transform = CGAffineTransformRotate(m_transform, narrowPrecisionToCGFloat(deg2rad(d)));
    return *this;
}

AffineTransform &AffineTransform::translate(double tx, double ty)
{
    m_transform = CGAffineTransformTranslate(m_transform, narrowPrecisionToCGFloat(tx), narrowPrecisionToCGFloat(ty));
    return *this;
}

AffineTransform &AffineTransform::shear(double sx, double sy)
{
    CGAffineTransform shear = CGAffineTransformMake(1.0f, narrowPrecisionToCGFloat(sy), narrowPrecisionToCGFloat(sx), 1.0f, 0.0f, 0.0f);
    m_transform = CGAffineTransformConcat(shear, m_transform);
    return *this;
}

double AffineTransform::det() const
{
    return m_transform.a * m_transform.d - m_transform.b * m_transform.c;
}

AffineTransform AffineTransform::inverse() const
{
    if (isInvertible())
        return AffineTransform(CGAffineTransformInvert(m_transform));
    return AffineTransform();
}

AffineTransform::operator CGAffineTransform() const
{
    return m_transform;
}

bool AffineTransform::operator== (const AffineTransform &m2) const
{
    return CGAffineTransformEqualToTransform(m_transform, CGAffineTransform(m2));
}

AffineTransform &AffineTransform::operator*= (const AffineTransform &m2)
{
    m_transform = CGAffineTransformConcat(m_transform, CGAffineTransform(m2));
    return *this;
}

AffineTransform AffineTransform::operator* (const AffineTransform &m2)
{
    return CGAffineTransformConcat(m_transform, CGAffineTransform(m2));
}

}

#endif // PLATFORM(CG)
