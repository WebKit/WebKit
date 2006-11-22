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
#include <ApplicationServices/ApplicationServices.h>
#elif PLATFORM(QT)
#include <QMatrix>
#elif PLATFORM(CAIRO)
#include "cairo.h"
#endif

namespace WebCore {

    class IntRect;
    class FloatRect;

class AffineTransform {
public:
    AffineTransform();
    AffineTransform(double a, double b, double c, double d, double tx, double ty);
#if PLATFORM(CG)
    AffineTransform(CGAffineTransform transform);
#elif PLATFORM(QT)
    AffineTransform(const QMatrix &matrix);
#elif PLATFORM(CAIRO)
    AffineTransform(const cairo_matrix_t &matrix);
#endif

    void setMatrix(double a, double b, double c, double d, double tx, double ty);
    void map(double x, double y, double *x2, double *y2) const;
    IntRect mapRect(const IntRect&) const;
    FloatRect mapRect(const FloatRect&) const;
    
    bool isIdentity() const;
    
    double m11() const;
    double m12() const;
    double m21() const;
    double m22() const;
    double dx() const;
    double dy() const;

    void reset();
    
    AffineTransform &scale(double sx, double sy);
    AffineTransform &rotate(double d);
    AffineTransform &translate(double tx, double ty);
    AffineTransform &shear(double sx, double sy);
    
    double det() const;
    bool isInvertible() const;
    AffineTransform invert() const;

#if PLATFORM(CG)
    operator CGAffineTransform() const;
#elif PLATFORM(QT)
    operator QMatrix() const;
#elif PLATFORM(CAIRO)
    operator cairo_matrix_t() const;
#endif

    bool operator==(const AffineTransform&) const;
    AffineTransform& operator*=(const AffineTransform&);
    AffineTransform operator*(const AffineTransform&);
    
private:
#if PLATFORM(CG)
    CGAffineTransform m_transform;
#elif PLATFORM(QT)
    QMatrix m_transform;
#elif PLATFORM(CAIRO)
    cairo_matrix_t m_transform;
#endif
};

} // namespace WebCore

#endif // AffineTransform_h
