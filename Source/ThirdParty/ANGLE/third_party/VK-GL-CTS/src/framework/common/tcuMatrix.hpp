#ifndef _TCUMATRIX_HPP
#define _TCUMATRIX_HPP
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
 * \brief Templatized matrix class.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuVector.hpp"
#include "tcuArray.hpp"

namespace tcu
{

// Templated matrix class.
template <typename T, int Rows, int Cols>
class Matrix
{
public:
    typedef Vector<T, Rows> Element;
    typedef T Scalar;

    enum
    {
        SIZE = Cols,
        ROWS = Rows,
        COLS = Cols,
    };

    Matrix(void);
    explicit Matrix(const T &src);
    explicit Matrix(const T src[Rows * Cols]);
    Matrix(const Vector<T, Rows> &src);
    Matrix(const Matrix<T, Rows, Cols> &src);
    ~Matrix(void);

    Matrix<T, Rows, Cols> &operator=(const Matrix<T, Rows, Cols> &src);
    Matrix<T, Rows, Cols> &operator*=(const Matrix<T, Rows, Cols> &src);

    void setRow(int rowNdx, const Vector<T, Cols> &vec);
    void setColumn(int colNdx, const Vector<T, Rows> &vec);

    Vector<T, Cols> getRow(int ndx) const;
    Vector<T, Rows> &getColumn(int ndx);
    const Vector<T, Rows> &getColumn(int ndx) const;

    Vector<T, Rows> &operator[](int ndx)
    {
        return getColumn(ndx);
    }
    const Vector<T, Rows> &operator[](int ndx) const
    {
        return getColumn(ndx);
    }

    inline const T &operator()(int row, int col) const
    {
        return m_data[col][row];
    }
    inline T &operator()(int row, int col)
    {
        return m_data[col][row];
    }

    Array<T, Rows * Cols> getRowMajorData(void) const;
    Array<T, Rows * Cols> getColumnMajorData(void) const;

private:
    Vector<Vector<T, Rows>, Cols> m_data;
} DE_WARN_UNUSED_TYPE;

// Operators.

// Mat * Mat.
template <typename T, int Rows0, int Cols0, int Rows1, int Cols1>
Matrix<T, Rows0, Cols1> operator*(const Matrix<T, Rows0, Cols0> &a, const Matrix<T, Rows1, Cols1> &b);

// Mat * Vec (column vector).
template <typename T, int Rows, int Cols>
Vector<T, Rows> operator*(const Matrix<T, Rows, Cols> &mtx, const Vector<T, Cols> &vec);

// Vec * Mat (row vector).
template <typename T, int Rows, int Cols>
Vector<T, Cols> operator*(const Vector<T, Rows> &vec, const Matrix<T, Rows, Cols> &mtx);

template <typename T, int Rows, int Cols>
bool operator==(const Matrix<T, Rows, Cols> &lhs, const Matrix<T, Rows, Cols> &rhs);

template <typename T, int Rows, int Cols>
bool operator!=(const Matrix<T, Rows, Cols> &lhs, const Matrix<T, Rows, Cols> &rhs);

// Further operations

template <typename T, int Size>
struct SquareMatrixOps
{
    static T doDeterminant(const Matrix<T, Size, Size> &mat);
    static Matrix<T, Size, Size> doInverse(const Matrix<T, Size, Size> &mat);
};

template <typename T>
struct SquareMatrixOps<T, 2>
{
    static T doDeterminant(const Matrix<T, 2, 2> &mat);
    static Matrix<T, 2, 2> doInverse(const Matrix<T, 2, 2> &mat);
};

template <typename T>
struct SquareMatrixOps<T, 3>
{
    static T doDeterminant(const Matrix<T, 3, 3> &mat);
    static Matrix<T, 3, 3> doInverse(const Matrix<T, 3, 3> &mat);
};

template <typename T>
struct SquareMatrixOps<T, 4>
{
    static T doDeterminant(const Matrix<T, 4, 4> &mat);
    static Matrix<T, 4, 4> doInverse(const Matrix<T, 4, 4> &mat);
};

namespace matrix
{

template <typename T, int Size>
T determinant(const Matrix<T, Size, Size> &mat)
{
    return SquareMatrixOps<T, Size>::doDeterminant(mat);
}

template <typename T, int Size>
Matrix<T, Size, Size> inverse(const Matrix<T, Size, Size> &mat)
{
    return SquareMatrixOps<T, Size>::doInverse(mat);
}

} // namespace matrix

// Template implementations.

template <typename T>
T SquareMatrixOps<T, 2>::doDeterminant(const Matrix<T, 2, 2> &mat)
{
    return mat(0, 0) * mat(1, 1) - mat(1, 0) * mat(0, 1);
}

template <typename T>
T SquareMatrixOps<T, 3>::doDeterminant(const Matrix<T, 3, 3> &mat)
{
    return +mat(0, 0) * mat(1, 1) * mat(2, 2) + mat(0, 1) * mat(1, 2) * mat(2, 0) + mat(0, 2) * mat(1, 0) * mat(2, 1) -
           mat(0, 0) * mat(1, 2) * mat(2, 1) - mat(0, 1) * mat(1, 0) * mat(2, 2) - mat(0, 2) * mat(1, 1) * mat(2, 0);
}

template <typename T>
T SquareMatrixOps<T, 4>::doDeterminant(const Matrix<T, 4, 4> &mat)
{
    using matrix::determinant;

    const T minorMatrices[4][3 * 3] = {{
                                           mat(1, 1),
                                           mat(2, 1),
                                           mat(3, 1),
                                           mat(1, 2),
                                           mat(2, 2),
                                           mat(3, 2),
                                           mat(1, 3),
                                           mat(2, 3),
                                           mat(3, 3),
                                       },
                                       {
                                           mat(1, 0),
                                           mat(2, 0),
                                           mat(3, 0),
                                           mat(1, 2),
                                           mat(2, 2),
                                           mat(3, 2),
                                           mat(1, 3),
                                           mat(2, 3),
                                           mat(3, 3),
                                       },
                                       {
                                           mat(1, 0),
                                           mat(2, 0),
                                           mat(3, 0),
                                           mat(1, 1),
                                           mat(2, 1),
                                           mat(3, 1),
                                           mat(1, 3),
                                           mat(2, 3),
                                           mat(3, 3),
                                       },
                                       {
                                           mat(1, 0),
                                           mat(2, 0),
                                           mat(3, 0),
                                           mat(1, 1),
                                           mat(2, 1),
                                           mat(3, 1),
                                           mat(1, 2),
                                           mat(2, 2),
                                           mat(3, 2),
                                       }};

    return +mat(0, 0) * determinant(Matrix<T, 3, 3>(minorMatrices[0])) -
           mat(0, 1) * determinant(Matrix<T, 3, 3>(minorMatrices[1])) +
           mat(0, 2) * determinant(Matrix<T, 3, 3>(minorMatrices[2])) -
           mat(0, 3) * determinant(Matrix<T, 3, 3>(minorMatrices[3]));
}

template <typename T>
Matrix<T, 2, 2> SquareMatrixOps<T, 2>::doInverse(const Matrix<T, 2, 2> &mat)
{
    using matrix::determinant;

    const T det = determinant(mat);
    Matrix<T, 2, 2> retVal;

    retVal(0, 0) = mat(1, 1) / det;
    retVal(0, 1) = -mat(0, 1) / det;
    retVal(1, 0) = -mat(1, 0) / det;
    retVal(1, 1) = mat(0, 0) / det;

    return retVal;
}

template <typename T>
Matrix<T, 3, 3> SquareMatrixOps<T, 3>::doInverse(const Matrix<T, 3, 3> &mat)
{
    // Blockwise inversion
    using matrix::inverse;

    const T areaA[2 * 2] = {mat(0, 0), mat(0, 1), mat(1, 0), mat(1, 1)};
    const T areaB[2]     = {
        mat(0, 2),
        mat(1, 2),
    };
    const T areaC[2] = {
        mat(2, 0),
        mat(2, 1),
    };
    const T areaD[1]     = {mat(2, 2)};
    const T nullField[4] = {T(0.0f)};

    const Matrix<T, 2, 2> invA = inverse(Matrix<T, 2, 2>(areaA));
    const Matrix<T, 2, 1> matB = Matrix<T, 2, 1>(areaB);
    const Matrix<T, 1, 2> matC = Matrix<T, 1, 2>(areaC);
    const Matrix<T, 1, 1> matD = Matrix<T, 1, 1>(areaD);

    const T schurComplement       = T(1.0f) / (matD - matC * invA * matB)(0, 0);
    const Matrix<T, 2, 2> zeroMat = Matrix<T, 2, 2>(nullField);

    const Matrix<T, 2, 2> blockA = invA + invA * matB * schurComplement * matC * invA;
    const Matrix<T, 2, 1> blockB = (zeroMat - invA) * matB * schurComplement;
    const Matrix<T, 1, 2> blockC = matC * invA * (-schurComplement);
    const T blockD               = schurComplement;

    const T result[3 * 3] = {
        blockA(0, 0), blockA(0, 1), blockB(0, 0), blockA(1, 0), blockA(1, 1),
        blockB(1, 0), blockC(0, 0), blockC(0, 1), blockD,
    };

    return Matrix<T, 3, 3>(result);
}

template <typename T>
Matrix<T, 4, 4> SquareMatrixOps<T, 4>::doInverse(const Matrix<T, 4, 4> &mat)
{
    // Blockwise inversion
    using matrix::inverse;

    const T areaA[2 * 2] = {mat(0, 0), mat(0, 1), mat(1, 0), mat(1, 1)};
    const T areaB[2 * 2] = {mat(0, 2), mat(0, 3), mat(1, 2), mat(1, 3)};
    const T areaC[2 * 2] = {mat(2, 0), mat(2, 1), mat(3, 0), mat(3, 1)};
    const T areaD[2 * 2] = {mat(2, 2), mat(2, 3), mat(3, 2), mat(3, 3)};
    const T nullField[4] = {T(0.0f)};

    const Matrix<T, 2, 2> invA = inverse(Matrix<T, 2, 2>(areaA));
    const Matrix<T, 2, 2> matB = Matrix<T, 2, 2>(areaB);
    const Matrix<T, 2, 2> matC = Matrix<T, 2, 2>(areaC);
    const Matrix<T, 2, 2> matD = Matrix<T, 2, 2>(areaD);

    const Matrix<T, 2, 2> schurComplement = inverse(matD - matC * invA * matB);
    const Matrix<T, 2, 2> zeroMat         = Matrix<T, 2, 2>(nullField);

    const Matrix<T, 2, 2> blockA = invA + invA * matB * schurComplement * matC * invA;
    const Matrix<T, 2, 2> blockB = (zeroMat - invA) * matB * schurComplement;
    const Matrix<T, 2, 2> blockC = (zeroMat - schurComplement) * matC * invA;
    const Matrix<T, 2, 2> blockD = schurComplement;

    const T result[4 * 4] = {
        blockA(0, 0), blockA(0, 1), blockB(0, 0), blockB(0, 1), blockA(1, 0), blockA(1, 1), blockB(1, 0), blockB(1, 1),
        blockC(0, 0), blockC(0, 1), blockD(0, 0), blockD(0, 1), blockC(1, 0), blockC(1, 1), blockD(1, 0), blockD(1, 1),
    };

    return Matrix<T, 4, 4>(result);
}

// Initialize to identity.
template <typename T, int Rows, int Cols>
Matrix<T, Rows, Cols>::Matrix(void)
{
    for (int row = 0; row < Rows; row++)
        for (int col = 0; col < Cols; col++)
            (*this)(row, col) = (row == col) ? T(1) : T(0);
}

// Initialize to diagonal matrix.
template <typename T, int Rows, int Cols>
Matrix<T, Rows, Cols>::Matrix(const T &src)
{
    for (int row = 0; row < Rows; row++)
        for (int col = 0; col < Cols; col++)
            (*this)(row, col) = (row == col) ? src : T(0);
}

// Initialize from data array.
template <typename T, int Rows, int Cols>
Matrix<T, Rows, Cols>::Matrix(const T src[Rows * Cols])
{
    for (int row = 0; row < Rows; row++)
        for (int col = 0; col < Cols; col++)
            (*this)(row, col) = src[row * Cols + col];
}

// Initialize to diagonal matrix.
template <typename T, int Rows, int Cols>
Matrix<T, Rows, Cols>::Matrix(const Vector<T, Rows> &src)
{
    DE_STATIC_ASSERT(Rows == Cols);
    for (int row = 0; row < Rows; row++)
        for (int col = 0; col < Cols; col++)
            (*this)(row, col) = (row == col) ? src.m_data[row] : T(0);
}

// Copy constructor.
template <typename T, int Rows, int Cols>
Matrix<T, Rows, Cols>::Matrix(const Matrix<T, Rows, Cols> &src)
{
    *this = src;
}

// Destructor.
template <typename T, int Rows, int Cols>
Matrix<T, Rows, Cols>::~Matrix(void)
{
}

// Assignment operator.
template <typename T, int Rows, int Cols>
Matrix<T, Rows, Cols> &Matrix<T, Rows, Cols>::operator=(const Matrix<T, Rows, Cols> &src)
{
    for (int row = 0; row < Rows; row++)
        for (int col = 0; col < Cols; col++)
            (*this)(row, col) = src(row, col);
    return *this;
}

// Multipy and assign op
template <typename T, int Rows, int Cols>
Matrix<T, Rows, Cols> &Matrix<T, Rows, Cols>::operator*=(const Matrix<T, Rows, Cols> &src)
{
    *this = *this * src;
    return *this;
}

template <typename T, int Rows, int Cols>
void Matrix<T, Rows, Cols>::setRow(int rowNdx, const Vector<T, Cols> &vec)
{
    for (int col = 0; col < Cols; col++)
        (*this)(rowNdx, col) = vec.m_data[col];
}

template <typename T, int Rows, int Cols>
void Matrix<T, Rows, Cols>::setColumn(int colNdx, const Vector<T, Rows> &vec)
{
    m_data[colNdx] = vec;
}

template <typename T, int Rows, int Cols>
Vector<T, Cols> Matrix<T, Rows, Cols>::getRow(int rowNdx) const
{
    Vector<T, Cols> res;
    for (int col = 0; col < Cols; col++)
        res[col] = (*this)(rowNdx, col);
    return res;
}

template <typename T, int Rows, int Cols>
Vector<T, Rows> &Matrix<T, Rows, Cols>::getColumn(int colNdx)
{
    return m_data[colNdx];
}

template <typename T, int Rows, int Cols>
const Vector<T, Rows> &Matrix<T, Rows, Cols>::getColumn(int colNdx) const
{
    return m_data[colNdx];
}

template <typename T, int Rows, int Cols>
Array<T, Rows * Cols> Matrix<T, Rows, Cols>::getColumnMajorData(void) const
{
    Array<T, Rows * Cols> a;
    T *dst = a.getPtr();
    for (int col = 0; col < Cols; col++)
        for (int row = 0; row < Rows; row++)
            *dst++ = (*this)(row, col);
    return a;
}

template <typename T, int Rows, int Cols>
Array<T, Rows * Cols> Matrix<T, Rows, Cols>::getRowMajorData(void) const
{
    Array<T, Rows * Cols> a;
    T *dst = a.getPtr();
    for (int row = 0; row < Rows; row++)
        for (int col = 0; col < Cols; col++)
            *dst++ = (*this)(row, col);
    return a;
}

// Multiplication of two matrices.
template <typename T, int Rows0, int Cols0, int Rows1, int Cols1>
Matrix<T, Rows0, Cols1> operator*(const Matrix<T, Rows0, Cols0> &a, const Matrix<T, Rows1, Cols1> &b)
{
    DE_STATIC_ASSERT(Cols0 == Rows1);
    Matrix<T, Rows0, Cols1> res;
    for (int row = 0; row < Rows0; row++)
    {
        for (int col = 0; col < Cols1; col++)
        {
            T v = T(0);
            for (int ndx = 0; ndx < Cols0; ndx++)
                v += a(row, ndx) * b(ndx, col);
            res(row, col) = v;
        }
    }
    return res;
}

// Multiply of matrix with column vector.
template <typename T, int Rows, int Cols>
Vector<T, Rows> operator*(const Matrix<T, Rows, Cols> &mtx, const Vector<T, Cols> &vec)
{
    Vector<T, Rows> res;
    for (int row = 0; row < Rows; row++)
    {
        T v = T(0);
        for (int col = 0; col < Cols; col++)
            v += mtx(row, col) * vec.m_data[col];
        res.m_data[row] = v;
    }
    return res;
}

// Multiply of matrix with row vector.
template <typename T, int Rows, int Cols>
Vector<T, Cols> operator*(const Vector<T, Rows> &vec, const Matrix<T, Rows, Cols> &mtx)
{
    Vector<T, Cols> res;
    for (int col = 0; col < Cols; col++)
    {
        T v = T(0);
        for (int row = 0; row < Rows; row++)
            v += mtx(row, col) * vec.m_data[row];
        res.m_data[col] = v;
    }
    return res;
}

// Common typedefs.
typedef Matrix<float, 2, 2> Matrix2f;
typedef Matrix<float, 3, 3> Matrix3f;
typedef Matrix<float, 4, 4> Matrix4f;

// GLSL-style naming \note CxR.
typedef Matrix2f Mat2;
typedef Matrix<float, 3, 2> Mat2x3;
typedef Matrix<float, 4, 2> Mat2x4;
typedef Matrix<float, 2, 3> Mat3x2;
typedef Matrix3f Mat3;
typedef Matrix<float, 4, 3> Mat3x4;
typedef Matrix<float, 2, 4> Mat4x2;
typedef Matrix<float, 3, 4> Mat4x3;
typedef Matrix4f Mat4;

//using tcu::Matrix;
// Common typedefs 16Bit.
typedef Matrix<uint16_t, 2, 2> Matrix2f16b;
typedef Matrix<uint16_t, 3, 3> Matrix3f16b;
typedef Matrix<uint16_t, 4, 4> Matrix4f16b;

// GLSL-style naming \note CxR.
typedef Matrix2f16b Mat2_16b;
typedef Matrix<uint16_t, 3, 2> Mat2x3_16b;
typedef Matrix<uint16_t, 4, 2> Mat2x4_16b;
typedef Matrix<uint16_t, 2, 3> Mat3x2_16b;
typedef Matrix3f16b Mat3_16b;
typedef Matrix<uint16_t, 4, 3> Mat3x4_16b;
typedef Matrix<uint16_t, 2, 4> Mat4x2_16b;
typedef Matrix<uint16_t, 3, 4> Mat4x3_16b;
typedef Matrix4f16b Mat4_16b;

// 64-bit matrices.
typedef Matrix<double, 2, 2> Matrix2d;
typedef Matrix<double, 3, 3> Matrix3d;
typedef Matrix<double, 4, 4> Matrix4d;

// GLSL-style naming \note CxR.
typedef Matrix2d Mat2d;
typedef Matrix<double, 3, 2> Mat2x3d;
typedef Matrix<double, 4, 2> Mat2x4d;
typedef Matrix<double, 2, 3> Mat3x2d;
typedef Matrix3d Mat3d;
typedef Matrix<double, 4, 3> Mat3x4d;
typedef Matrix<double, 2, 4> Mat4x2d;
typedef Matrix<double, 3, 4> Mat4x3d;
typedef Matrix4d Mat4d;

// Matrix-scalar operators.

template <typename T, int Rows, int Cols>
Matrix<T, Rows, Cols> operator+(const Matrix<T, Rows, Cols> &mtx, T scalar)
{
    Matrix<T, Rows, Cols> res;
    for (int col = 0; col < Cols; col++)
        for (int row = 0; row < Rows; row++)
            res(row, col) = mtx(row, col) + scalar;
    return res;
}

template <typename T, int Rows, int Cols>
Matrix<T, Rows, Cols> operator-(const Matrix<T, Rows, Cols> &mtx, T scalar)
{
    Matrix<T, Rows, Cols> res;
    for (int col = 0; col < Cols; col++)
        for (int row = 0; row < Rows; row++)
            res(row, col) = mtx(row, col) - scalar;
    return res;
}

template <typename T, int Rows, int Cols>
Matrix<T, Rows, Cols> operator*(const Matrix<T, Rows, Cols> &mtx, T scalar)
{
    Matrix<T, Rows, Cols> res;
    for (int col = 0; col < Cols; col++)
        for (int row = 0; row < Rows; row++)
            res(row, col) = mtx(row, col) * scalar;
    return res;
}

template <typename T, int Rows, int Cols>
Matrix<T, Rows, Cols> operator/(const Matrix<T, Rows, Cols> &mtx, T scalar)
{
    Matrix<T, Rows, Cols> res;
    for (int col = 0; col < Cols; col++)
        for (int row = 0; row < Rows; row++)
            res(row, col) = mtx(row, col) / scalar;
    return res;
}

// Matrix-matrix component-wise operators.

template <typename T, int Rows, int Cols>
Matrix<T, Rows, Cols> operator+(const Matrix<T, Rows, Cols> &a, const Matrix<T, Rows, Cols> &b)
{
    Matrix<T, Rows, Cols> res;
    for (int col = 0; col < Cols; col++)
        for (int row = 0; row < Rows; row++)
            res(row, col) = a(row, col) + b(row, col);
    return res;
}

template <typename T, int Rows, int Cols>
Matrix<T, Rows, Cols> operator-(const Matrix<T, Rows, Cols> &a, const Matrix<T, Rows, Cols> &b)
{
    Matrix<T, Rows, Cols> res;
    for (int col = 0; col < Cols; col++)
        for (int row = 0; row < Rows; row++)
            res(row, col) = a(row, col) - b(row, col);
    return res;
}

template <typename T, int Rows, int Cols>
Matrix<T, Rows, Cols> operator/(const Matrix<T, Rows, Cols> &a, const Matrix<T, Rows, Cols> &b)
{
    Matrix<T, Rows, Cols> res;
    for (int col = 0; col < Cols; col++)
        for (int row = 0; row < Rows; row++)
            res(row, col) = a(row, col) / b(row, col);
    return res;
}

template <typename T, int Rows, int Cols>
bool operator==(const Matrix<T, Rows, Cols> &lhs, const Matrix<T, Rows, Cols> &rhs)
{
    for (int row = 0; row < Rows; row++)
        for (int col = 0; col < Cols; col++)
            if (lhs(row, col) != rhs(row, col))
                return false;
    return true;
}

template <typename T, int Rows, int Cols>
bool operator!=(const Matrix<T, Rows, Cols> &lhs, const Matrix<T, Rows, Cols> &rhs)
{
    return !(lhs == rhs);
}

} // namespace tcu

#endif // _TCUMATRIX_HPP
