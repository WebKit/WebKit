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
#include "TransformationMatrix.h"
#endif

#include "WebCommon.h"
#include "WebPrivateOwnPtr.h"

namespace WebCore {
class TransformationMatrix;
}

namespace WebKit {

class WebTransformationMatrix {
public:
    WEBKIT_EXPORT WebTransformationMatrix();
    WEBKIT_EXPORT WebTransformationMatrix(double a, double b, double c, double d, double e, double f);
    WEBKIT_EXPORT WebTransformationMatrix(double m11, double m12, double m13, double m14,
                                          double m21, double m22, double m23, double m24,
                                          double m31, double m32, double m33, double m34,
                                          double m41, double m42, double m43, double m44);
    WEBKIT_EXPORT WebTransformationMatrix(const WebTransformationMatrix&);
    ~WebTransformationMatrix() { reset(); }

    WEBKIT_EXPORT void reset();

    // Operations that return a separate matrix and do not modify this one.
    WEBKIT_EXPORT WebTransformationMatrix inverse() const;
    WEBKIT_EXPORT WebTransformationMatrix to2dTransform() const;

    WEBKIT_EXPORT WebTransformationMatrix& operator=(const WebTransformationMatrix&);
    WEBKIT_EXPORT bool operator==(const WebTransformationMatrix&) const;
    WEBKIT_EXPORT WebTransformationMatrix operator*(const WebTransformationMatrix&) const;

    // Operations that modify this matrix
    WEBKIT_EXPORT void multiply(const WebTransformationMatrix&);
    WEBKIT_EXPORT void makeIdentity();
    WEBKIT_EXPORT void translate(double tx, double ty);
    WEBKIT_EXPORT void translate3d(double tx, double ty, double tz);
    WEBKIT_EXPORT void translateRight3d(double tx, double ty, double tz);
    WEBKIT_EXPORT void scale(double s);
    WEBKIT_EXPORT void scaleNonUniform(double sx, double sy);
    WEBKIT_EXPORT void scale3d(double sx, double sy, double sz);
    WEBKIT_EXPORT void rotate(double angle);
    WEBKIT_EXPORT void rotate3d(double rx, double ry, double rz);
    WEBKIT_EXPORT void rotate3d(double x, double y, double z, double angle);
    WEBKIT_EXPORT void skewX(double angle);
    WEBKIT_EXPORT void skewY(double angle);
    WEBKIT_EXPORT void applyPerspective(double p);
    WEBKIT_EXPORT bool blend(const WebTransformationMatrix& from, double progress);

    WEBKIT_EXPORT bool hasPerspective() const;
    WEBKIT_EXPORT bool isInvertible() const;
    WEBKIT_EXPORT bool isBackFaceVisible() const;
    WEBKIT_EXPORT bool isIdentity() const;
    WEBKIT_EXPORT bool isIdentityOrTranslation() const;
    WEBKIT_EXPORT bool isIntegerTranslation() const;

    // Accessors
    WEBKIT_EXPORT double m11() const;
    WEBKIT_EXPORT void setM11(double);
    WEBKIT_EXPORT double m12() const;
    WEBKIT_EXPORT void setM12(double);
    WEBKIT_EXPORT double m13() const;
    WEBKIT_EXPORT void setM13(double);
    WEBKIT_EXPORT double m14() const;
    WEBKIT_EXPORT void setM14(double);
    WEBKIT_EXPORT double m21() const;
    WEBKIT_EXPORT void setM21(double);
    WEBKIT_EXPORT double m22() const;
    WEBKIT_EXPORT void setM22(double);
    WEBKIT_EXPORT double m23() const;
    WEBKIT_EXPORT void setM23(double);
    WEBKIT_EXPORT double m24() const;
    WEBKIT_EXPORT void setM24(double);
    WEBKIT_EXPORT double m31() const;
    WEBKIT_EXPORT void setM31(double);
    WEBKIT_EXPORT double m32() const;
    WEBKIT_EXPORT void setM32(double);
    WEBKIT_EXPORT double m33() const;
    WEBKIT_EXPORT void setM33(double);
    WEBKIT_EXPORT double m34() const;
    WEBKIT_EXPORT void setM34(double);
    WEBKIT_EXPORT double m41() const;
    WEBKIT_EXPORT void setM41(double);
    WEBKIT_EXPORT double m42() const;
    WEBKIT_EXPORT void setM42(double);
    WEBKIT_EXPORT double m43() const;
    WEBKIT_EXPORT void setM43(double);
    WEBKIT_EXPORT double m44() const;
    WEBKIT_EXPORT void setM44(double);

    WEBKIT_EXPORT double a() const;
    WEBKIT_EXPORT void setA(double);
    WEBKIT_EXPORT double b() const;
    WEBKIT_EXPORT void setB(double);
    WEBKIT_EXPORT double c() const;
    WEBKIT_EXPORT void setC(double);
    WEBKIT_EXPORT double d() const;
    WEBKIT_EXPORT void setD(double);
    WEBKIT_EXPORT double e() const;
    WEBKIT_EXPORT void setE(double);
    WEBKIT_EXPORT double f() const;
    WEBKIT_EXPORT void setF(double);

#if WEBKIT_IMPLEMENTATION
    // Conversions between WebKit::WebTransformationMatrix and WebCore::TransformationMatrix
    explicit WebTransformationMatrix(const WebCore::TransformationMatrix&);
    WebCore::TransformationMatrix toWebCoreTransform() const;
#endif

protected:

    WebPrivateOwnPtr<WebCore::TransformationMatrix> m_private;
};

} // namespace WebKit

#endif
