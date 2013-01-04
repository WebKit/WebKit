/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <public/WebTransformationMatrix.h>

#include "TransformationMatrix.h"

using namespace WebCore;

namespace WebKit {

WebTransformationMatrix::WebTransformationMatrix()
    : m_private(new TransformationMatrix)
{
}

WebTransformationMatrix::WebTransformationMatrix(double a, double b, double c, double d, double e, double f)
    : m_private(new TransformationMatrix(a, b, c, d, e, f))
{
}

WebTransformationMatrix::WebTransformationMatrix(double m11, double m12, double m13, double m14,
                                                 double m21, double m22, double m23, double m24,
                                                 double m31, double m32, double m33, double m34,
                                                 double m41, double m42, double m43, double m44)
    : m_private(new TransformationMatrix(m11, m12, m13, m14, m21, m22, m23, m24, m31, m32, m33, m34, m41, m42, m43, m44))
{
}

WebTransformationMatrix::WebTransformationMatrix(const WebTransformationMatrix& t)
    : m_private(new TransformationMatrix(*t.m_private.get()))
{
}

WebTransformationMatrix::WebTransformationMatrix(const TransformationMatrix& t)
    : m_private(new TransformationMatrix(t))
{
}

void WebTransformationMatrix::reset()
{
    m_private.reset(0);
}

WebTransformationMatrix& WebTransformationMatrix::operator=(const WebTransformationMatrix& t)
{
    m_private.reset(new TransformationMatrix(*t.m_private.get()));
    return *this;
}

bool WebTransformationMatrix::operator==(const WebTransformationMatrix& t) const
{
    return *m_private.get() == *t.m_private.get();
}

WebTransformationMatrix WebTransformationMatrix::operator*(const WebTransformationMatrix& t) const
{
    WebTransformationMatrix result = *this;
    result.multiply(t);
    return result;
}

WebTransformationMatrix WebTransformationMatrix::inverse() const
{
    return WebTransformationMatrix(m_private->inverse());
}

WebTransformationMatrix WebTransformationMatrix::to2dTransform() const
{
    WebTransformationMatrix result;
    result.m_private.reset(new TransformationMatrix(m_private->to2dTransform()));
    return result;
}

void WebTransformationMatrix::multiply(const WebTransformationMatrix& t)
{
    m_private->multiply(*t.m_private.get());
}

void WebTransformationMatrix::makeIdentity()
{
    m_private->makeIdentity();
}

void WebTransformationMatrix::translate(double tx, double ty)
{
    m_private->translate(tx, ty);
}

void WebTransformationMatrix::translate3d(double tx, double ty, double tz)
{
    m_private->translate3d(tx, ty, tz);
}

void WebTransformationMatrix::translateRight3d(double tx, double ty, double tz)
{
    m_private->translateRight3d(tx, ty, tz);
}

void WebTransformationMatrix::scale(double s)
{
    m_private->scale(s);
}

void WebTransformationMatrix::scaleNonUniform(double sx, double sy)
{
    m_private->scaleNonUniform(sx, sy);
}

void WebTransformationMatrix::scale3d(double sx, double sy, double sz)
{
    m_private->scale3d(sx, sy, sz);
}

void WebTransformationMatrix::rotate(double angle)
{
    m_private->rotate(angle);
}

void WebTransformationMatrix::rotate3d(double rx, double ry, double rz)
{
    m_private->rotate3d(rx, ry, rz);
}

void WebTransformationMatrix::rotate3d(double x, double y, double z, double angle)
{
    m_private->rotate3d(x, y, z, angle);
}

void WebTransformationMatrix::skewX(double angle)
{
    m_private->skewX(angle);
}

void WebTransformationMatrix::skewY(double angle)
{
    m_private->skewY(angle);
}

void WebTransformationMatrix::applyPerspective(double p)
{
    m_private->applyPerspective(p);
}

bool WebTransformationMatrix::blend(const WebTransformationMatrix& from, double progress)
{
    WebCore::TransformationMatrix::DecomposedType dummy;
    if (!m_private->decompose(dummy) || !from.m_private->decompose(dummy))
        return false;
    m_private->blend(*from.m_private.get(), progress);
    return true;
}

bool WebTransformationMatrix::hasPerspective() const
{
    return m_private->hasPerspective();
}

bool WebTransformationMatrix::isInvertible() const
{
    return m_private->isInvertible();
}

bool WebTransformationMatrix::isBackFaceVisible() const
{
    return m_private->isBackFaceVisible();
}

bool WebTransformationMatrix::isIdentity() const
{
    return m_private->isIdentity();
}

bool WebTransformationMatrix::isIdentityOrTranslation() const
{
    return m_private->isIdentityOrTranslation();
}

bool WebTransformationMatrix::isIntegerTranslation() const
{
    return m_private->isIntegerTranslation();
}

double WebTransformationMatrix::m11() const
{
    return m_private->m11();
}

void WebTransformationMatrix::setM11(double f)
{
    m_private->setM11(f);
}

double WebTransformationMatrix::m12() const
{
    return m_private->m12();
}

void WebTransformationMatrix::setM12(double f)
{
    m_private->setM12(f);
}

double WebTransformationMatrix::m13() const
{
    return m_private->m13();
}

void WebTransformationMatrix::setM13(double f)
{
    m_private->setM13(f);
}

double WebTransformationMatrix::m14() const
{
    return m_private->m14();
}

void WebTransformationMatrix::setM14(double f)
{
    m_private->setM14(f);
}

double WebTransformationMatrix::m21() const
{
    return m_private->m21();
}

void WebTransformationMatrix::setM21(double f)
{
    m_private->setM21(f);
}

double WebTransformationMatrix::m22() const
{
    return m_private->m22();
}

void WebTransformationMatrix::setM22(double f)
{
    m_private->setM22(f);
}

double WebTransformationMatrix::m23() const
{
    return m_private->m23();
}

void WebTransformationMatrix::setM23(double f)
{
    m_private->setM23(f);
}

double WebTransformationMatrix::m24() const
{
    return m_private->m24();
}

void WebTransformationMatrix::setM24(double f)
{
    m_private->setM24(f);
}

double WebTransformationMatrix::m31() const
{
    return m_private->m31();
}

void WebTransformationMatrix::setM31(double f)
{
    m_private->setM31(f);
}

double WebTransformationMatrix::m32() const
{
    return m_private->m32();
}

void WebTransformationMatrix::setM32(double f)
{
    m_private->setM32(f);
}

double WebTransformationMatrix::m33() const
{
    return m_private->m33();
}

void WebTransformationMatrix::setM33(double f)
{
    m_private->setM33(f);
}

double WebTransformationMatrix::m34() const
{
    return m_private->m34();
}

void WebTransformationMatrix::setM34(double f)
{
    m_private->setM34(f);
}

double WebTransformationMatrix::m41() const
{
    return m_private->m41();
}

void WebTransformationMatrix::setM41(double f)
{
    m_private->setM41(f);
}

double WebTransformationMatrix::m42() const
{
    return m_private->m42();
}

void WebTransformationMatrix::setM42(double f)
{
    m_private->setM42(f);
}

double WebTransformationMatrix::m43() const
{
    return m_private->m43();
}

void WebTransformationMatrix::setM43(double f)
{
    m_private->setM43(f);
}

double WebTransformationMatrix::m44() const
{
    return m_private->m44();
}

void WebTransformationMatrix::setM44(double f)
{
    m_private->setM44(f);
}

double WebTransformationMatrix::a() const
{
    return m_private->a();
}

void WebTransformationMatrix::setA(double a)
{
    m_private->setA(a);
}

double WebTransformationMatrix::b() const
{
    return m_private->b();
}

void WebTransformationMatrix::setB(double b)
{
    m_private->setB(b);
}

double WebTransformationMatrix::c() const
{
    return m_private->c();
}

void WebTransformationMatrix::setC(double c)
{
    m_private->setC(c);
}

double WebTransformationMatrix::d() const
{
    return m_private->d();
}

void WebTransformationMatrix::setD(double d)
{
    m_private->setD(d);
}

double WebTransformationMatrix::e() const
{
    return m_private->e();
}

void WebTransformationMatrix::setE(double e)
{
    m_private->setE(e);
}

double WebTransformationMatrix::f() const
{
    return m_private->f();
}

void WebTransformationMatrix::setF(double f)
{
    m_private->setF(f);
}

TransformationMatrix WebTransformationMatrix::toWebCoreTransform() const
{
    return *m_private.get();
}

} // namespace WebKit
