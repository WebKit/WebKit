/*
 * Copyright (C) 2007 Kevin Ollivier  All rights reserved.
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

#include "FloatRect.h"
#include "IntRect.h"
#include "NotImplemented.h"

#include <stdio.h>
#include <wx/defs.h>
#include <wx/graphics.h>

namespace WebCore {

#if USE(WXGC)
TransformationMatrix::TransformationMatrix(const PlatformTransformationMatrix& matrix)
{
    m_transform = matrix;
}
#endif

TransformationMatrix::TransformationMatrix(double a, double b, double c, double d, double e, double f)
{
#if USE(WXGC)
    wxGraphicsRenderer* renderer = wxGraphicsRenderer::GetDefaultRenderer();
    m_transform = renderer->CreateMatrix();
#endif
    setMatrix(a, b, c, d, e, f);
}

TransformationMatrix::TransformationMatrix()
{
    // NB: If we ever support using Cairo backend on Win/Mac, this will need to be
    // changed somehow (though I'm not sure how as we don't have a reference to the
    // graphics context here.
#if USE(WXGC)
    wxGraphicsRenderer* renderer = wxGraphicsRenderer::GetDefaultRenderer();
    m_transform = renderer->CreateMatrix();
#endif
}

TransformationMatrix TransformationMatrix::inverse() const
{
    notImplemented();
    return *this;
}

void TransformationMatrix::setMatrix(double a, double b, double c, double d, double e, double f)
{
#if USE(WXGC)
    m_transform.Set(a, b, c, d, e, f);
#endif
}

void TransformationMatrix::map(double x, double y, double *x2, double *y2) const
{
    notImplemented();
}

IntRect TransformationMatrix::mapRect(const IntRect &rect) const
{
#if USE(WXGC)
    double x, y, width, height;
    x = rect.x();
    y = rect.y();
    width = rect.width();
    height = rect.height();

    m_transform.TransformPoint(&x, &y);
    m_transform.TransformDistance(&width, &height);
    return IntRect(x, y, width, height);
#endif
    return IntRect();
}

FloatRect TransformationMatrix::mapRect(const FloatRect &rect) const
{
#if USE(WXGC)
    double x, y, width, height;
    x = rect.x();
    y = rect.y();
    width = rect.width();
    height = rect.height();

    m_transform.TransformPoint(&x, &y);
    m_transform.TransformDistance(&width, &height);
    return FloatRect(x, y, width, height);
#endif
    return FloatRect();
}


TransformationMatrix& TransformationMatrix::scale(double sx, double sy)
{
#if USE(WXGC)
    m_transform.Scale((wxDouble)sx, (wxDouble)sy);
#endif
    return *this;
}

void TransformationMatrix::reset()
{
    notImplemented();
}

TransformationMatrix& TransformationMatrix::rotate(double d)
{
#if USE(WXGC)
    m_transform.Rotate((wxDouble)d);
#endif
    return *this;
}

TransformationMatrix& TransformationMatrix::translate(double tx, double ty)
{
#if USE(WXGC)
    m_transform.Translate((wxDouble)tx, (wxDouble)ty);
#endif
    return *this;
}

TransformationMatrix& TransformationMatrix::shear(double sx, double sy)
{
    notImplemented();
    return *this;
}

TransformationMatrix& TransformationMatrix::operator*=(const TransformationMatrix& other)
{
    notImplemented();
    return *this;
}

bool TransformationMatrix::operator== (const TransformationMatrix &other) const
{
#if USE(WXGC)
    return m_transform.IsEqual((wxGraphicsMatrix)other);
#else
    notImplemented();
    return true;
#endif
}

TransformationMatrix TransformationMatrix::operator* (const TransformationMatrix &other)
{
    notImplemented();
    return *this; //m_transform * other.m_transform;
}

double TransformationMatrix::det() const
{
    notImplemented();
    return 0;
}

#if USE(WXGC)
TransformationMatrix::operator wxGraphicsMatrix() const
{
    return m_transform;
}
#endif

double TransformationMatrix::a() const
{
    double a = 0;
#if USE(WXGC)
    m_transform.Get(&a);
#endif
    return a;
}

void TransformationMatrix::setA(double a)
{
    setMatrix(a, b(), c(), d(), e(), f());
}

double TransformationMatrix::b() const
{
    double b = 0;
#if USE(WXGC)
    m_transform.Get(&b);
#endif
    return b;
}

void TransformationMatrix::setB(double b)
{
    setMatrix(a(), b, c(), d(), e(), f());
}

double TransformationMatrix::c() const
{
    double c = 0;
#if USE(WXGC)
    m_transform.Get(&c);
#endif
    return c;
}

void TransformationMatrix::setC(double c)
{
    setMatrix(a(), b(), c, d(), e(), f());
}

double TransformationMatrix::d() const
{
    double d = 0;
#if USE(WXGC)
    m_transform.Get(&d);
#endif
    return d;
}

void TransformationMatrix::setD(double d)
{
    setMatrix(a(), b(), c(), d, e(), f());
}

double TransformationMatrix::e() const
{
    double e = 0;
#if USE(WXGC)
    m_transform.Get(&e);
#endif
    return e;
}

void TransformationMatrix::setE(double e)
{
    setMatrix(a(), b(), c(), d(), e, f());
}

double TransformationMatrix::f() const
{
    double f = 0;
#if USE(WXGC)
    m_transform.Get(&f);
#endif
    return f;
}

void TransformationMatrix::setF(double f)
{
    setMatrix(a(), b(), c(), d(), e(), f);
}

}
