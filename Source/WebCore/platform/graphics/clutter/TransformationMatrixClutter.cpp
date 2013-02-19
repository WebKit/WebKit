/*
 * Copyright (C) 2011 Collabora Ltd.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1 of
 *  the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#include "config.h"
#include "TransformationMatrix.h"

#include <cogl/cogl.h>

namespace WebCore {

TransformationMatrix::operator CoglMatrix() const
{
    CoglMatrix matrix;

    matrix.xx = m11();
    matrix.xy = m21();
    matrix.xz = m31();
    matrix.xw = m41();

    matrix.yx = m12();
    matrix.yy = m22();
    matrix.yz = m32();
    matrix.yw = m42();

    matrix.zx = m13();
    matrix.zy = m23();
    matrix.zz = m33();
    matrix.zw = m43();

    matrix.wx = m14();
    matrix.wy = m24();
    matrix.wz = m34();
    matrix.ww = m44();

    return matrix;
}

TransformationMatrix::TransformationMatrix(const CoglMatrix* matrix)
{
    setMatrix(matrix->xx, matrix->yx, matrix->zx, matrix->wx,
        matrix->xy, matrix->yy, matrix->zy, matrix->wy,
        matrix->xz, matrix->yz, matrix->zz, matrix->wz,
        matrix->xw, matrix->yw, matrix->zw, matrix->ww);
}

}
