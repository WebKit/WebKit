/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric@webkit.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#include <math.h>
#include "FloatPoint.h"
#include "FloatPoint3D.h"

namespace WebCore {

FloatPoint3D::FloatPoint3D()
    : m_x(0)
    , m_y(0)
    , m_z(0)
{
}

FloatPoint3D::FloatPoint3D(float x, float y, float z)
    : m_x(x)
    , m_y(y)
    , m_z(z)
{
}

FloatPoint3D::FloatPoint3D(const FloatPoint& p)
    : m_x(p.x())
    , m_y(p.y())
    , m_z(0)
{
}

void FloatPoint3D::normalize()
{
    float length = sqrtf(m_x * m_x + m_y * m_y + m_z * m_z);

    if (length != 0) {
        m_x /= length;
        m_y /= length;
        m_z /= length;
    }
}

} // namespace WebCore

