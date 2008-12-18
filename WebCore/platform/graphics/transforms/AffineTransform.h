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

#ifndef AffineTransform_h
#define AffineTransform_h

#if PLATFORM(CG)
#include <CoreGraphics/CGAffineTransform.h>
typedef CGAffineTransform PlatformAffineTransform;
#elif PLATFORM(QT)
#include <QMatrix>
typedef QMatrix PlatformAffineTransform;
#elif PLATFORM(CAIRO)
#include <cairo.h>
typedef cairo_matrix_t PlatformAffineTransform;
#elif PLATFORM(SKIA)
#include "SkMatrix.h"
typedef SkMatrix PlatformAffineTransform;
#elif PLATFORM(WX) && USE(WXGC)
#include <wx/defs.h>
#include <wx/graphics.h>
typedef wxGraphicsMatrix PlatformAffineTransform;
#endif

namespace WebCore {

class IntPoint;
class IntRect;
class FloatPoint;
class FloatRect;
class FloatQuad;

class AffineTransform {
public:
    AffineTransform();
    AffineTransform(double a, double b, double c, double d, double e, double f);
#if !PLATFORM(WX) || USE(WXGC)
    AffineTransform(const PlatformAffineTransform&);
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

    AffineTransform& multiply(const AffineTransform&);
    AffineTransform& scale(double); 
    AffineTransform& scale(double sx, double sy); 
    AffineTransform& scaleNonUniform(double sx, double sy);
    AffineTransform& rotate(double d);
    AffineTransform& rotateFromVector(double x, double y);
    AffineTransform& translate(double tx, double ty);
    AffineTransform& shear(double sx, double sy);
    AffineTransform& flipX();
    AffineTransform& flipY();
    AffineTransform& skew(double angleX, double angleY);
    AffineTransform& skewX(double angle);
    AffineTransform& skewY(double angle);
 
    double det() const;
    bool isInvertible() const;
    AffineTransform inverse() const;

    void blend(const AffineTransform& from, double progress);

#if !PLATFORM(WX) || USE(WXGC)
    operator PlatformAffineTransform() const;
#endif

    bool operator==(const AffineTransform&) const;
    bool operator!=(const AffineTransform& other) const { return !(*this == other); }
    AffineTransform& operator*=(const AffineTransform&);
    AffineTransform operator*(const AffineTransform&);
    
private:
#if !PLATFORM(WX) || USE(WXGC)
    PlatformAffineTransform m_transform;
#endif
};

AffineTransform makeMapBetweenRects(const FloatRect& source, const FloatRect& dest);

} // namespace WebCore

#endif // AffineTransform_h
