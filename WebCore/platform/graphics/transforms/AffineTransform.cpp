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

#include "config.h"
#include "AffineTransform.h"

#include "FloatRect.h"
#include "FloatQuad.h"
#include "IntRect.h"

#include <wtf/MathExtras.h>

namespace WebCore {

static void affineTransformDecompose(const AffineTransform& matrix, double sr[9])
{
    AffineTransform m(matrix);

    // Compute scaling factors
    double sx = sqrt(m.a() * m.a() + m.b() * m.b());
    double sy = sqrt(m.c() * m.c() + m.d() * m.d());

    /* Compute cross product of transformed unit vectors. If negative,
        one axis was flipped. */

    if (m.a() * m.d() - m.c() * m.b() < 0.0) {
        // Flip axis with minimum unit vector dot product

        if (m.a() < m.d())
            sx = -sx;
        else
            sy = -sy;
    }

    // Remove scale from matrix

    m.scale(1.0 / sx, 1.0 / sy);

    // Compute rotation

    double angle = atan2(m.b(), m.a());

    // Remove rotation from matrix

    m.rotate(rad2deg(-angle));

    // Return results

    sr[0] = sx; sr[1] = sy; sr[2] = angle;
    sr[3] = m.a(); sr[4] = m.b();
    sr[5] = m.c(); sr[6] = m.d();
    sr[7] = m.e(); sr[8] = m.f();
}

static void affineTransformCompose(AffineTransform& m, const double sr[9])
{
    m.setA(sr[3]);
    m.setB(sr[4]);
    m.setC(sr[5]);
    m.setD(sr[6]);
    m.setE(sr[7]);
    m.setF(sr[8]);
    m.rotate(rad2deg(sr[2]));
    m.scale(sr[0], sr[1]);
}

bool AffineTransform::isInvertible() const
{
    return det() != 0.0;
}

AffineTransform& AffineTransform::multiply(const AffineTransform& other)
{
    return (*this) *= other;
}

AffineTransform& AffineTransform::scale(double s)
{
    return scale(s, s);
}

AffineTransform& AffineTransform::scaleNonUniform(double sx, double sy)
{
    return scale(sx, sy);
}

AffineTransform& AffineTransform::rotateFromVector(double x, double y)
{
    return rotate(rad2deg(atan2(y, x)));
}

AffineTransform& AffineTransform::flipX()
{
    return scale(-1.0f, 1.0f);
}

AffineTransform& AffineTransform::flipY()
{
    return scale(1.0f, -1.0f);
}

AffineTransform& AffineTransform::skew(double angleX, double angleY)
{
    return shear(tan(deg2rad(angleX)), tan(deg2rad(angleY)));
}

AffineTransform& AffineTransform::skewX(double angle)
{
    return shear(tan(deg2rad(angle)), 0.0f);
}

AffineTransform& AffineTransform::skewY(double angle)
{
    return shear(0.0f, tan(deg2rad(angle)));
}

AffineTransform makeMapBetweenRects(const FloatRect& source, const FloatRect& dest)
{
    AffineTransform transform;
    transform.translate(dest.x() - source.x(), dest.y() - source.y());
    transform.scale(dest.width() / source.width(), dest.height() / source.height());
    return transform;
}

IntPoint AffineTransform::mapPoint(const IntPoint& point) const
{
    double x2, y2;
    map(point.x(), point.y(), &x2, &y2);
    
    // Round the point.
    return IntPoint(lround(x2), lround(y2));
}

FloatPoint AffineTransform::mapPoint(const FloatPoint& point) const
{
    double x2, y2;
    map(point.x(), point.y(), &x2, &y2);

    return FloatPoint(static_cast<float>(x2), static_cast<float>(y2));
}

FloatQuad AffineTransform::mapQuad(const FloatQuad& quad) const
{
    // FIXME: avoid 4 seperate library calls. Point mapping really needs
    // to be platform-independent code.
    return FloatQuad(mapPoint(quad.p1()),
                     mapPoint(quad.p2()),
                     mapPoint(quad.p3()),
                     mapPoint(quad.p4()));
}

void AffineTransform::blend(const AffineTransform& from, double progress)
{
    double srA[9], srB[9];

    affineTransformDecompose(from, srA);
    affineTransformDecompose(*this, srB);

    // If x-axis of one is flipped, and y-axis of the other, convert to an unflipped rotation.
    if ((srA[0] < 0.0 && srB[1] < 0.0) || (srA[1] < 0.0 &&  srB[0] < 0.0)) {
        srA[0] = -srA[0];
        srA[1] = -srA[1];
        srA[2] += srA[2] < 0 ? piDouble : -piDouble;
    }

    // Don't rotate the long way around.
    srA[2] = fmod(srA[2], 2.0 * piDouble);
    srB[2] = fmod(srB[2], 2.0 * piDouble);

    if (fabs(srA[2] - srB[2]) > piDouble) {
        if (srA[2] > srB[2])
            srA[2] -= piDouble * 2.0;
        else
            srB[2] -= piDouble * 2.0;
    }

    for (int i = 0; i < 9; i++)
        srA[i] = srA[i] + progress * (srB[i] - srA[i]);

    affineTransformCompose(*this, srA);
}

}
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

#include "config.h"
#include "AffineTransform.h"

#include "FloatRect.h"
#include "FloatQuad.h"
#include "IntRect.h"

#include <wtf/MathExtras.h>

namespace WebCore {

static void affineTransformDecompose(const AffineTransform& matrix, double sr[9])
{
    AffineTransform m(matrix);

    // Compute scaling factors
    double sx = sqrt(m.a() * m.a() + m.b() * m.b());
    double sy = sqrt(m.c() * m.c() + m.d() * m.d());

    /* Compute cross product of transformed unit vectors. If negative,
        one axis was flipped. */

    if (m.a() * m.d() - m.c() * m.b() < 0.0) {
        // Flip axis with minimum unit vector dot product

        if (m.a() < m.d())
            sx = -sx;
        else
            sy = -sy;
    }

    // Remove scale from matrix

    m.scale(1.0 / sx, 1.0 / sy);

    // Compute rotation

    double angle = atan2(m.b(), m.a());

    // Remove rotation from matrix

    m.rotate(rad2deg(-angle));

    // Return results

    sr[0] = sx; sr[1] = sy; sr[2] = angle;
    sr[3] = m.a(); sr[4] = m.b();
    sr[5] = m.c(); sr[6] = m.d();
    sr[7] = m.e(); sr[8] = m.f();
}

static void affineTransformCompose(AffineTransform& m, const double sr[9])
{
    m.setA(sr[3]);
    m.setB(sr[4]);
    m.setC(sr[5]);
    m.setD(sr[6]);
    m.setE(sr[7]);
    m.setF(sr[8]);
    m.rotate(rad2deg(sr[2]));
    m.scale(sr[0], sr[1]);
}

bool AffineTransform::isInvertible() const
{
    return det() != 0.0;
}

AffineTransform& AffineTransform::multiply(const AffineTransform& other)
{
    return (*this) *= other;
}

AffineTransform& AffineTransform::scale(double s)
{
    return scale(s, s);
}

AffineTransform& AffineTransform::scaleNonUniform(double sx, double sy)
{
    return scale(sx, sy);
}

AffineTransform& AffineTransform::rotateFromVector(double x, double y)
{
    return rotate(rad2deg(atan2(y, x)));
}

AffineTransform& AffineTransform::flipX()
{
    return scale(-1.0f, 1.0f);
}

AffineTransform& AffineTransform::flipY()
{
    return scale(1.0f, -1.0f);
}

AffineTransform& AffineTransform::skew(double angleX, double angleY)
{
    return shear(tan(deg2rad(angleX)), tan(deg2rad(angleY)));
}

AffineTransform& AffineTransform::skewX(double angle)
{
    return shear(tan(deg2rad(angle)), 0.0f);
}

AffineTransform& AffineTransform::skewY(double angle)
{
    return shear(0.0f, tan(deg2rad(angle)));
}

AffineTransform makeMapBetweenRects(const FloatRect& source, const FloatRect& dest)
{
    AffineTransform transform;
    transform.translate(dest.x() - source.x(), dest.y() - source.y());
    transform.scale(dest.width() / source.width(), dest.height() / source.height());
    return transform;
}

IntPoint AffineTransform::mapPoint(const IntPoint& point) const
{
    double x2, y2;
    map(point.x(), point.y(), &x2, &y2);
    
    // Round the point.
    return IntPoint(lround(x2), lround(y2));
}

FloatPoint AffineTransform::mapPoint(const FloatPoint& point) const
{
    double x2, y2;
    map(point.x(), point.y(), &x2, &y2);

    return FloatPoint(static_cast<float>(x2), static_cast<float>(y2));
}

FloatQuad AffineTransform::mapQuad(const FloatQuad& quad) const
{
    // FIXME: avoid 4 seperate library calls. Point mapping really needs
    // to be platform-independent code.
    return FloatQuad(mapPoint(quad.p1()),
                     mapPoint(quad.p2()),
                     mapPoint(quad.p3()),
                     mapPoint(quad.p4()));
}

void AffineTransform::blend(const AffineTransform& from, double progress)
{
    double srA[9], srB[9];

    affineTransformDecompose(from, srA);
    affineTransformDecompose(*this, srB);

    // If x-axis of one is flipped, and y-axis of the other, convert to an unflipped rotation.
    if ((srA[0] < 0.0 && srB[1] < 0.0) || (srA[1] < 0.0 &&  srB[0] < 0.0)) {
        srA[0] = -srA[0];
        srA[1] = -srA[1];
        srA[2] += srA[2] < 0 ? piDouble : -piDouble;
    }

    // Don't rotate the long way around.
    srA[2] = fmod(srA[2], 2.0 * piDouble);
    srB[2] = fmod(srB[2], 2.0 * piDouble);

    if (fabs(srA[2] - srB[2]) > piDouble) {
        if (srA[2] > srB[2])
            srA[2] -= piDouble * 2.0;
        else
            srB[2] -= piDouble * 2.0;
    }

    for (int i = 0; i < 9; i++)
        srA[i] = srA[i] + progress * (srB[i] - srA[i]);

    affineTransformCompose(*this, srA);
}

}
