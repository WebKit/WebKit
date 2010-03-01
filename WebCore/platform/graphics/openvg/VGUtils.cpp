/*
 * Copyright (C) Research In Motion Limited 2009. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "VGUtils.h"

#include "AffineTransform.h"
#include "FloatRect.h"
#include "TransformationMatrix.h"

namespace WebCore {

VGMatrix::VGMatrix(const VGfloat data[9])
{
    m_data[0] = data[0];
    m_data[1] = data[1];
    m_data[2] = data[2];
    m_data[3] = data[3];
    m_data[4] = data[4];
    m_data[5] = data[5];
    m_data[6] = data[6];
    m_data[7] = data[7];
    m_data[8] = data[8];
}

VGMatrix::VGMatrix(const AffineTransform& transformation)
{
    m_data[0] = transformation.a();
    m_data[1] = transformation.b();
    m_data[2] = 0;
    m_data[3] = transformation.c();
    m_data[4] = transformation.d();
    m_data[5] = 0;
    m_data[6] = transformation.e();
    m_data[7] = transformation.f();
    m_data[8] = 1;
}

VGMatrix::VGMatrix(const TransformationMatrix& matrix)
{
    m_data[0] = matrix.m11();
    m_data[1] = matrix.m12();
    m_data[2] = matrix.m14();
    m_data[3] = matrix.m21();
    m_data[4] = matrix.m22();
    m_data[5] = matrix.m24();
    m_data[6] = matrix.m41();
    m_data[7] = matrix.m42();
    m_data[8] = matrix.m44();
}

VGMatrix::operator AffineTransform() const
{
    AffineTransform transformation(
        m_data[0], m_data[1],
        m_data[3], m_data[4],
        m_data[6], m_data[7]);
    return transformation;
}

VGMatrix::operator TransformationMatrix() const
{
    TransformationMatrix matrix(
        m_data[0], m_data[1], 0, m_data[2],
        m_data[3], m_data[4], 0, m_data[5],
        0,         0,         1, 0,
        m_data[6], m_data[7], 0, m_data[8]);
    return matrix;
}

AffineTransform::operator VGMatrix() const
{
    return VGMatrix(*this);
}

TransformationMatrix::operator VGMatrix() const
{
    return VGMatrix(*this);
}

VGRect::VGRect(const VGfloat data[4])
{
    m_data[0] = data[0];
    m_data[1] = data[1];
    m_data[2] = data[2];
    m_data[3] = data[3];
}

VGRect::VGRect(const FloatRect& rect)
{
    m_data[0] = rect.x();
    m_data[1] = rect.y();
    m_data[2] = rect.width();
    m_data[3] = rect.height();
}

VGRect::operator FloatRect() const
{
    return FloatRect(m_data[0], m_data[1], m_data[2], m_data[3]);
}

FloatRect::operator VGRect() const
{
    return VGRect(*this);
}

}
