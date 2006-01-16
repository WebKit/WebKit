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

#ifndef QWMATRIX_H_
#define QWMATRIX_H_

#if __APPLE__
// FIXME: Just comment out the entire file, since its implementation is completely CG-specific.

#include <ApplicationServices/ApplicationServices.h>

namespace WebCore {
    class IntRect;
    class FloatRect;
}

class QMatrix {
public:
    QMatrix();
    QMatrix(double a, double b, double c, double d, double tx, double ty);
    QMatrix(CGAffineTransform transform);

    void setMatrix(double a, double b, double c, double d, double tx, double ty);
    void map(double x, double y, double *x2, double *y2) const;
    WebCore::IntRect QMatrix::mapRect(const WebCore::IntRect&) const;
    WebCore::FloatRect QMatrix::mapRect(const WebCore::FloatRect&) const;
    
    bool isIdentity() const;
    
    double m11() const { return m_transform.a; }
    double m12() const { return m_transform.b; }
    double m21() const { return m_transform.c; }
    double m22() const { return m_transform.d; }
    double dx() const { return m_transform.tx; }
    double dy() const { return m_transform.ty; }

    void reset();
    
    QMatrix &scale(double sx, double sy);
    QMatrix &rotate(double d);
    QMatrix &translate(double tx, double ty);
    QMatrix &shear(double sx, double sy);
    
    double det() const;
    bool isInvertible() const;
    QMatrix invert() const;

    operator CGAffineTransform() const;

    bool operator== (const QMatrix &) const;
    QMatrix &operator*= (const QMatrix &);
    QMatrix operator* (const QMatrix &m2);
    
private:
    CGAffineTransform m_transform;
};

#define QWMatrix QMatrix

#endif // __APPLE__

#endif
