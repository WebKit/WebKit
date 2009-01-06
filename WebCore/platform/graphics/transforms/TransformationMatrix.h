/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef TransformationMatrix_h
#define TransformationMatrix_h

#if PLATFORM(CG)
#include <CoreGraphics/CGAffineTransform.h>
typedef CGAffineTransform PlatformTransformationMatrix;
#elif PLATFORM(QT)
#include <QMatrix>
typedef QMatrix PlatformTransformationMatrix;
#elif PLATFORM(CAIRO)
#include <cairo.h>
typedef cairo_matrix_t PlatformTransformationMatrix;
#elif PLATFORM(SKIA)
#include "SkMatrix.h"
typedef SkMatrix PlatformTransformationMatrix;
#elif PLATFORM(WX) && USE(WXGC)
#include <wx/defs.h>
#include <wx/graphics.h>
typedef wxGraphicsMatrix PlatformTransformationMatrix;
#endif

namespace WebCore {

class IntPoint;
class IntRect;
class FloatPoint;
class FloatRect;
class FloatQuad;

class TransformationMatrix {
public:
    TransformationMatrix();
    TransformationMatrix(double a, double b, double c, double d, double e, double f);
#if !PLATFORM(WX) || USE(WXGC)
    TransformationMatrix(const PlatformTransformationMatrix&);
#endif

    void setMatrix(double a, double b, double c, double d, double e, double f);
    void map(double x, double y, double *x2, double *y2) const;

    // Rounds the mapped point to the nearest integer value.
    IntPoint mapPoint(const IntPoint&) const;

    FloatPoint mapPoint(const FloatPoint&) const;

    // Rounds the resulting mapped rectangle out. This is helpful for bounding
    // box computations but may not be what is wanted in other contexts.
    IntRect mapRect(const IntRect&) const;

    FloatRect mapRect(const FloatRect&) const;

    FloatQuad mapQuad(const FloatQuad&) const;

    bool isIdentity() const;

    double a() const;
    void setA(double a);

    double b() const;
    void setB(double b);

    double c() const;
    void setC(double c);

    double d() const;
    void setD(double d);

    double e() const;
    void setE(double e);

    double f() const;
    void setF(double f);

    void reset();

    TransformationMatrix& multiply(const TransformationMatrix&);
    TransformationMatrix& scale(double); 
    TransformationMatrix& scale(double sx, double sy); 
    TransformationMatrix& scaleNonUniform(double sx, double sy);
    TransformationMatrix& rotate(double d);
    TransformationMatrix& rotateFromVector(double x, double y);
    TransformationMatrix& translate(double tx, double ty);
    TransformationMatrix& shear(double sx, double sy);
    TransformationMatrix& flipX();
    TransformationMatrix& flipY();
    TransformationMatrix& skew(double angleX, double angleY);
    TransformationMatrix& skewX(double angle);
    TransformationMatrix& skewY(double angle);
 
    double det() const;
    bool isInvertible() const;
    TransformationMatrix inverse() const;

    void blend(const TransformationMatrix& from, double progress);

#if !PLATFORM(WX) || USE(WXGC)
    operator PlatformTransformationMatrix() const;
#endif

    bool operator==(const TransformationMatrix&) const;
    bool operator!=(const TransformationMatrix& other) const { return !(*this == other); }
    TransformationMatrix& operator*=(const TransformationMatrix&);
    TransformationMatrix operator*(const TransformationMatrix&);
    
private:
#if !PLATFORM(WX) || USE(WXGC)
    PlatformTransformationMatrix m_transform;
#endif
};

TransformationMatrix makeMapBetweenRects(const FloatRect& source, const FloatRect& dest);

} // namespace WebCore

#endif // TransformationMatrix_h
