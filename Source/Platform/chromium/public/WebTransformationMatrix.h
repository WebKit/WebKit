/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebTransformationMatrix_h
#define WebTransformationMatrix_h

#if WEBKIT_IMPLEMENTATION
#include "FloatPoint.h"
#include "FloatPoint3D.h"
#include "FloatQuad.h"
#endif

#include "WebCommon.h"
#include "WebPrivateOwnPtr.h"

namespace WebCore {
class TransformationMatrix;
}

namespace WebKit {

class WebTransformationMatrix {
public:
    WebTransformationMatrix();
    WebTransformationMatrix(double a, double b, double c, double d, double e, double f);
    WebTransformationMatrix(const WebTransformationMatrix&);
    ~WebTransformationMatrix() { reset(); }
    void reset();

    // Operations that return a separate matrix and do not modify this one.
    WebTransformationMatrix inverse() const;
    WebTransformationMatrix to2dTransform() const;

    WebTransformationMatrix& operator=(const WebTransformationMatrix&);
    bool operator==(const WebTransformationMatrix&) const;
    WebTransformationMatrix operator*(const WebTransformationMatrix&) const;

    // Operations that modify this matrix
    void multiply(const WebTransformationMatrix&);
    void makeIdentity();
    void translate(double tx, double ty);
    void translate3d(double tx, double ty, double tz);
    void translateRight3d(double tx, double ty, double tz);
    void scale(double s);
    void scaleNonUniform(double sx, double sy);
    void scale3d(double sx, double sy, double sz);
    void rotate(double angle);
    void rotate3d(double rx, double ry, double rz);
    void rotate3d(double x, double y, double z, double angle);
    void skewX(double angle);
    void skewY(double angle);
    void applyPerspective(double p);
    void blend(const WebTransformationMatrix& from, double progress);

    bool hasPerspective() const;
    bool isInvertible() const;
    bool isBackFaceVisible() const;
    bool isIdentity() const;
    bool isIdentityOrTranslation() const;
    bool isIntegerTranslation() const;

    // Accessors
    double m11() const;
    void setM11(double);
    double m12() const;
    void setM12(double);
    double m13() const;
    void setM13(double);
    double m14() const;
    void setM14(double);
    double m21() const;
    void setM21(double);
    double m22() const;
    void setM22(double);
    double m23() const;
    void setM23(double);
    double m24() const;
    void setM24(double);
    double m31() const;
    void setM31(double);
    double m32() const;
    void setM32(double);
    double m33() const;
    void setM33(double);
    double m34() const;
    void setM34(double);
    double m41() const;
    void setM41(double);
    double m42() const;
    void setM42(double);
    double m43() const;
    void setM43(double);
    double m44() const;
    void setM44(double);

    double a() const;
    void setA(double);
    double b() const;
    void setB(double);
    double c() const;
    void setC(double);
    double d() const;
    void setD(double);
    double e() const;
    void setE(double);
    double f() const;
    void setF(double);

#if WEBKIT_IMPLEMENTATION
    // Conversions between WebKit::WebTransformationMatrix and WebCore::TransformationMatrix
    explicit WebTransformationMatrix(const WebCore::TransformationMatrix&);
    WebCore::TransformationMatrix& toWebCoreTransform() const;

    // FIXME: these map functions should not exist, should be using CCMathUtil
    // instead. Eventually CCMathUtil functions could be merged here, but its
    // not yet the right time for that.
    WebCore::FloatRect mapRect(const WebCore::FloatRect&) const;
    WebCore::IntRect mapRect(const WebCore::IntRect&) const;
    WebCore::FloatPoint3D mapPoint(const WebCore::FloatPoint3D&) const;
    WebCore::FloatPoint mapPoint(const WebCore::FloatPoint&) const;
    WebCore::IntPoint mapPoint(const WebCore::IntPoint&) const;
    WebCore::FloatQuad mapQuad(const WebCore::FloatQuad&) const;
    WebCore::FloatPoint projectPoint(const WebCore::FloatPoint&, bool* clamped = 0) const;
#endif

protected:
    WebPrivateOwnPtr<WebCore::TransformationMatrix> m_private;
};

} // namespace WebKit

#endif
