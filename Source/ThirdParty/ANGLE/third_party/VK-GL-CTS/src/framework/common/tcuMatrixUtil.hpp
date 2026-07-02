#ifndef _TCUMATRIXUTIL_HPP
#define _TCUMATRIXUTIL_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Matrix utility functions
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuMatrix.hpp"
#include "deMath.h"

namespace tcu
{

template <typename T, int Size>
Matrix<T, Size + 1, Size + 1> translationMatrix(const Vector<T, Size> &translation);

template <typename T, int Rows, int Cols>
Matrix<T, Cols, Rows> transpose(const Matrix<T, Rows, Cols> &mat);

// 2D affine transformations.
Matrix<float, 2, 2> rotationMatrix(float radians);
Matrix<float, 2, 2> shearMatrix(const Vector<float, 2> &shear);

// 3D axis rotations.
Matrix<float, 3, 3> rotationMatrixX(float radiansX);
Matrix<float, 3, 3> rotationMatrixY(float radiansY);
Matrix<float, 3, 3> rotationMatrixZ(float radiansZ);

template <class T, int Rows, int Cols>
Matrix<T, Rows, Cols> ortho2DMatrix(T l, T r, T b, T t, T n, T f);

// Implementations.

// Builds a translation matrix for a homogenous coordinate system
template <typename T, int Len>
inline Matrix<T, Len + 1, Len + 1> translationMatrix(const Vector<T, Len> &translation)
{
    Matrix<T, Len + 1, Len + 1> res = Matrix<T, Len + 1, Len + 1>();
    for (int row = 0; row < Len; row++)
        res(row, Len) = translation.m_data[row];
    return res;
}

template <typename T, int Rows, int Cols>
inline Matrix<T, Cols, Rows> transpose(const Matrix<T, Rows, Cols> &mat)
{
    Matrix<T, Cols, Rows> res;
    for (int row = 0; row < Rows; row++)
        for (int col = 0; col < Cols; col++)
            res(col, row) = mat(row, col);
    return res;
}

inline Matrix<float, 2, 2> rotationMatrix(float radians)
{
    Matrix<float, 2, 2> mat;
    float c = deFloatCos(radians);
    float s = deFloatSin(radians);

    mat(0, 0) = c;
    mat(0, 1) = -s;
    mat(1, 0) = s;
    mat(1, 1) = c;

    return mat;
}

inline Matrix<float, 2, 2> shearMatrix(const Vector<float, 2> &shear)
{
    Matrix<float, 2, 2> mat;
    mat(0, 0) = 1.0f;
    mat(0, 1) = shear.x();
    mat(1, 0) = shear.y();
    mat(1, 1) = 1.0f + shear.x() * shear.y();
    return mat;
}

inline Matrix<float, 3, 3> rotationMatrixX(float radiansX)
{
    Matrix<float, 3, 3> mat(1.0f);
    float c = deFloatCos(radiansX);
    float s = deFloatSin(radiansX);

    mat(1, 1) = c;
    mat(1, 2) = -s;
    mat(2, 1) = s;
    mat(2, 2) = c;

    return mat;
}

inline Matrix<float, 3, 3> rotationMatrixY(float radiansY)
{
    Matrix<float, 3, 3> mat(1.0f);
    float c = deFloatCos(radiansY);
    float s = deFloatSin(radiansY);

    mat(0, 0) = c;
    mat(0, 2) = s;
    mat(2, 0) = -s;
    mat(2, 2) = c;

    return mat;
}

inline Matrix<float, 3, 3> rotationMatrixZ(float radiansZ)
{
    Matrix<float, 3, 3> mat(1.0f);
    float c = deFloatCos(radiansZ);
    float s = deFloatSin(radiansZ);

    mat(0, 0) = c;
    mat(0, 1) = -s;
    mat(1, 0) = s;
    mat(1, 1) = c;

    return mat;
}

template <class T, int Rows, int Cols>
Matrix<T, Rows, Cols> ortho2DMatrix(T l, T r, T b, T t, T n, T f)
{
    Matrix<T, Rows, Cols> res;

    if ((r - l) == 0.f || (t - b) == 0.f || (f - n) == 0.f)
        return res;

    T inv_width  = 1.0f / (r - l);
    T inv_height = 1.0f / (t - b);
    T inv_depth  = 1.0f / (f - n);

    res(0, 0) = 2.0f * inv_width;
    res(1, 1) = 2.0f * inv_height;
    res(2, 2) = 2.0f * inv_depth;

    res(3, 0) = -(r + l) * inv_width;
    res(3, 1) = -(t + b) * inv_height;
    res(3, 2) = -(f + n) * inv_depth;
    res(3, 3) = 1.0f;

    return res;
}

} // namespace tcu

#endif // _TCUMATRIXUTIL_HPP
