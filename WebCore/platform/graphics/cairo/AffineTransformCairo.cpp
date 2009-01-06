/*
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

#include "IntRect.h"
#include "FloatRect.h"

#include <cairo.h>

namespace WebCore {

static const double deg2rad = 0.017453292519943295769; // pi/180

TransformationMatrix::TransformationMatrix()
{
    cairo_matrix_init_identity(&m_transform);
}

TransformationMatrix::TransformationMatrix(double a, double b, double c, double d, double tx, double ty)
{
    cairo_matrix_init(&m_transform, a, b, c, d, tx, ty);
}

TransformationMatrix::TransformationMatrix(const PlatformAffineTransform& matrix)
{
    m_transform = matrix;
}

void TransformationMatrix::setMatrix(double a, double b, double c, double d, double tx, double ty)
{
    cairo_matrix_init(&m_transform, a, b, c, d, tx, ty);
}

void TransformationMatrix::map(double x, double y, double* x2, double* y2) const
{
    *x2 = x;
    *y2 = y;
    cairo_matrix_transform_point(&m_transform, x2, y2);
}

IntRect TransformationMatrix::mapRect(const IntRect &rect) const
{
    FloatRect floatRect(rect);
    FloatRect enclosingFloatRect = this->mapRect(floatRect);

    return enclosingIntRect(enclosingFloatRect);
}

FloatRect TransformationMatrix::mapRect(const FloatRect &rect) const
{
    double rectMinX = rect.x();
    double rectMaxX = rect.x() + rect.width();
    double rectMinY = rect.y();
    double rectMaxY = rect.y() + rect.height();

    double px = rectMinX;
    double py = rectMinY;
    cairo_matrix_transform_point(&m_transform, &px, &py);

    double enclosingRectMinX = px;
    double enclosingRectMinY = py;
    double enclosingRectMaxX = px;
    double enclosingRectMaxY = py;

    px = rectMaxX;
    py = rectMinY;
    cairo_matrix_transform_point(&m_transform, &px, &py);
    if (px < enclosingRectMinX)
        enclosingRectMinX = px;
    else if (px > enclosingRectMaxX)
        enclosingRectMaxX = px;
    if (py < enclosingRectMinY)
        enclosingRectMinY = py;
    else if (py > enclosingRectMaxY)
        enclosingRectMaxY = py;

    px = rectMaxX;
    py = rectMaxY;
    cairo_matrix_transform_point(&m_transform, &px, &py);
    if (px < enclosingRectMinX)
        enclosingRectMinX = px;
    else if (px > enclosingRectMaxX)
        enclosingRectMaxX = px;
    if (py < enclosingRectMinY)
        enclosingRectMinY = py;
    else if (py > enclosingRectMaxY)
        enclosingRectMaxY = py;

    px = rectMinX;
    py = rectMaxY;
    cairo_matrix_transform_point(&m_transform, &px, &py);
    if (px < enclosingRectMinX)
        enclosingRectMinX = px;
    else if (px > enclosingRectMaxX)
        enclosingRectMaxX = px;
    if (py < enclosingRectMinY)
        enclosingRectMinY = py;
    else if (py > enclosingRectMaxY)
        enclosingRectMaxY = py;


    double enclosingRectWidth = enclosingRectMaxX - enclosingRectMinX;
    double enclosingRectHeight = enclosingRectMaxY - enclosingRectMinY;

    return FloatRect(enclosingRectMinX, enclosingRectMinY, enclosingRectWidth, enclosingRectHeight);
}

bool TransformationMatrix::isIdentity() const
{
    return ((m_transform.xx == 1) && (m_transform.yy == 1)
         && (m_transform.xy == 0) && (m_transform.yx == 0)
         && (m_transform.x0 == 0) && (m_transform.y0 == 0));
}

double TransformationMatrix::a() const
{
    return m_transform.xx;
}

void TransformationMatrix::setA(double a)
{
    m_transform.xx = a;
}

double TransformationMatrix::b() const
{
    return m_transform.yx;
}

void TransformationMatrix::setB(double b)
{
    m_transform.yx = b;
}

double TransformationMatrix::c() const
{
    return m_transform.xy;
}

void TransformationMatrix::setC(double c)
{
    m_transform.xy = c;
}

double TransformationMatrix::d() const
{
    return m_transform.yy;
}

void TransformationMatrix::setD(double d)
{
    m_transform.yy = d;
}

double TransformationMatrix::e() const
{
    return m_transform.x0;
}

void TransformationMatrix::setE(double e)
{
    m_transform.x0 = e;
}

double TransformationMatrix::f() const
{
    return m_transform.y0;
}

void TransformationMatrix::setF(double f)
{
    m_transform.y0 = f;
}

void TransformationMatrix::reset()
{
    cairo_matrix_init_identity(&m_transform);
}

TransformationMatrix &TransformationMatrix::scale(double sx, double sy)
{
    cairo_matrix_scale(&m_transform, sx, sy);
    return *this;
}

TransformationMatrix &TransformationMatrix::rotate(double d)
{
    cairo_matrix_rotate(&m_transform, d * deg2rad);
    return *this;
}

TransformationMatrix &TransformationMatrix::translate(double tx, double ty)
{
    cairo_matrix_translate(&m_transform, tx, ty);
    return *this;
}

TransformationMatrix &TransformationMatrix::shear(double sx, double sy)
{
    cairo_matrix_t shear;
    cairo_matrix_init(&shear, 1, sy, sx, 1, 0, 0);

    cairo_matrix_t result;
    cairo_matrix_multiply(&result, &shear, &m_transform);
    m_transform = result;

    return *this;
}

double TransformationMatrix::det() const
{
    return m_transform.xx * m_transform.yy - m_transform.xy * m_transform.yx;
}

TransformationMatrix TransformationMatrix::inverse() const
{
    if (!isInvertible()) return TransformationMatrix();

    cairo_matrix_t result = m_transform;
    cairo_matrix_invert(&result);
    return TransformationMatrix(result);
}

TransformationMatrix::operator cairo_matrix_t() const
{
    return m_transform;
}

bool TransformationMatrix::operator== (const TransformationMatrix &m2) const
{
    return ((m_transform.xx == m2.m_transform.xx)
         && (m_transform.yy == m2.m_transform.yy)
         && (m_transform.xy == m2.m_transform.xy)
         && (m_transform.yx == m2.m_transform.yx)
         && (m_transform.x0 == m2.m_transform.x0)
         && (m_transform.y0 == m2.m_transform.y0));

}

TransformationMatrix &TransformationMatrix::operator*= (const TransformationMatrix &m2)
{
    cairo_matrix_t result;
    cairo_matrix_multiply(&result, &m_transform, &m2.m_transform);
    m_transform = result;

    return *this;
}

TransformationMatrix TransformationMatrix::operator* (const TransformationMatrix &m2)
{
    cairo_matrix_t result;
    cairo_matrix_multiply(&result, &m_transform, &m2.m_transform);
    return result;
}

}

// vim: ts=4 sw=4 et
