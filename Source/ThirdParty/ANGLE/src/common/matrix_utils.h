//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Matrix:
//   Utility class implementing various matrix operations.
//   Supports matrices with minimum 2 and maximum 4 number of rows/columns.
//
// TODO: Check if we can merge Matrix.h in sample_util with this and replace it with this implementation.
// TODO: Rename this file to Matrix.h once we remove Matrix.h in sample_util.

#ifndef COMMON_MATRIX_UTILS_H_
#define COMMON_MATRIX_UTILS_H_

#include <vector>

#include "common/debug.h"

namespace angle
{

template<typename T>
class Matrix
{
  public:
    Matrix(const std::vector<T> &elements, const unsigned int &numRows, const unsigned int &numCols)
        : mElements(elements),
          mRows(numRows),
          mCols(numCols)
    {
        ASSERT(rows() >= 1 && rows() <= 4);
        ASSERT(columns() >= 1 && columns() <= 4);
    }

    Matrix(const std::vector<T> &elements, const unsigned int &size)
        : mElements(elements),
          mRows(size),
          mCols(size)
    {
        ASSERT(rows() >= 1 && rows() <= 4);
        ASSERT(columns() >= 1 && columns() <= 4);
    }

    Matrix(const T *elements, const unsigned int &size)
        : mRows(size),
          mCols(size)
    {
        ASSERT(rows() >= 1 && rows() <= 4);
        ASSERT(columns() >= 1 && columns() <= 4);
        for (size_t i = 0; i < size * size; i++)
            mElements.push_back(elements[i]);
    }

    const T &operator()(const unsigned int &rowIndex, const unsigned int &columnIndex) const
    {
        return mElements[rowIndex * columns() + columnIndex];
    }

    T &operator()(const unsigned int &rowIndex, const unsigned int &columnIndex)
    {
        return mElements[rowIndex * columns() + columnIndex];
    }

    const T &at(const unsigned int &rowIndex, const unsigned int &columnIndex) const
    {
        return operator()(rowIndex, columnIndex);
    }

    Matrix<T> operator*(const Matrix<T> &m)
    {
        ASSERT(columns() == m.rows());

        unsigned int resultRows = rows();
        unsigned int resultCols = m.columns();
        Matrix<T> result(std::vector<T>(resultRows * resultCols), resultRows, resultCols);
        for (unsigned int i = 0; i < resultRows; i++)
        {
            for (unsigned int j = 0; j < resultCols; j++)
            {
                T tmp = 0.0f;
                for (unsigned int k = 0; k < columns(); k++)
                    tmp += at(i, k) * m(k, j);
                result(i, j) = tmp;
            }
        }

        return result;
    }

    unsigned int size() const
    {
        ASSERT(rows() == columns());
        return rows();
    }

    unsigned int rows() const { return mRows; }

    unsigned int columns() const { return mCols; }

    std::vector<T> elements() const { return mElements; }

    Matrix<T> compMult(const Matrix<T> &mat1) const
    {
        Matrix result(std::vector<T>(mElements.size()), size());
        for (unsigned int i = 0; i < columns(); i++)
            for (unsigned int j = 0; j < rows(); j++)
                result(i, j) = at(i, j) * mat1(i, j);

        return result;
    }

    Matrix<T> outerProduct(const Matrix<T> &mat1) const
    {
        unsigned int cols = mat1.columns();
        Matrix result(std::vector<T>(rows() * cols), rows(), cols);
        for (unsigned int i = 0; i < rows(); i++)
            for (unsigned int j = 0; j < cols; j++)
                result(i, j) = at(i, 0) * mat1(0, j);

        return result;
    }

    Matrix<T> transpose() const
    {
        Matrix result(std::vector<T>(mElements.size()), columns(), rows());
        for (unsigned int i = 0; i < columns(); i++)
            for (unsigned int j = 0; j < rows(); j++)
                result(i, j) = at(j, i);

        return result;
    }

    T determinant() const
    {
        ASSERT(rows() == columns());

        switch (size())
        {
          case 2:
            return at(0, 0) * at(1, 1) - at(0, 1) * at(1, 0);

          case 3:
            return at(0, 0) * at(1, 1) * at(2, 2) +
                at(0, 1) * at(1, 2) * at(2, 0) +
                at(0, 2) * at(1, 0) * at(2, 1) -
                at(0, 2) * at(1, 1) * at(2, 0) -
                at(0, 1) * at(1, 0) * at(2, 2) -
                at(0, 0) * at(1, 2) * at(2, 1);

          case 4:
            {
                const float minorMatrices[4][3 * 3] =
                {
                    {
                        at(1, 1), at(2, 1), at(3, 1),
                        at(1, 2), at(2, 2), at(3, 2),
                        at(1, 3), at(2, 3), at(3, 3),
                    },
                    {
                        at(1, 0), at(2, 0), at(3, 0),
                        at(1, 2), at(2, 2), at(3, 2),
                        at(1, 3), at(2, 3), at(3, 3),
                    },
                    {
                        at(1, 0), at(2, 0), at(3, 0),
                        at(1, 1), at(2, 1), at(3, 1),
                        at(1, 3), at(2, 3), at(3, 3),
                    },
                    {
                        at(1, 0), at(2, 0), at(3, 0),
                        at(1, 1), at(2, 1), at(3, 1),
                        at(1, 2), at(2, 2), at(3, 2),
                    }
              };
              return at(0, 0) * Matrix<T>(minorMatrices[0], 3).determinant() -
                  at(0, 1) * Matrix<T>(minorMatrices[1], 3).determinant() +
                  at(0, 2) * Matrix<T>(minorMatrices[2], 3).determinant() -
                  at(0, 3) * Matrix<T>(minorMatrices[3], 3).determinant();
            }

          default:
            UNREACHABLE();
            break;
        }

        return T();
    }

    Matrix<T> inverse() const
    {
        ASSERT(rows() == columns());

        Matrix<T> cof(std::vector<T>(mElements.size()), rows(), columns());
        switch (size())
        {
          case 2:
            cof(0, 0) = at(1, 1);
            cof(0, 1) = -at(1, 0);
            cof(1, 0) = -at(0, 1);
            cof(1, 1) = at(0, 0);
            break;

          case 3:
            cof(0, 0) = at(1, 1) * at(2, 2) -
                at(2, 1) * at(1, 2);
            cof(0, 1) = -(at(1, 0) * at(2, 2) -
                at(2, 0) * at(1, 2));
            cof(0, 2) = at(1, 0) * at(2, 1) -
                at(2, 0) * at(1, 1);
            cof(1, 0) = -(at(0, 1) * at(2, 2) -
                at(2, 1) * at(0, 2));
            cof(1, 1) = at(0, 0) * at(2, 2) -
                at(2, 0) * at(0, 2);
            cof(1, 2) = -(at(0, 0) * at(2, 1) -
                at(2, 0) * at(0, 1));
            cof(2, 0) = at(0, 1) * at(1, 2) -
                at(1, 1) * at(0, 2);
            cof(2, 1) = -(at(0, 0) * at(1, 2) -
                at(1, 0) * at(0, 2));
            cof(2, 2) = at(0, 0) * at(1, 1) -
                at(1, 0) * at(0, 1);
            break;

          case 4:
            cof(0, 0) = at(1, 1) * at(2, 2) * at(3, 3) +
                at(2, 1) * at(3, 2) * at(1, 3) +
                at(3, 1) * at(1, 2) * at(2, 3) -
                at(1, 1) * at(3, 2) * at(2, 3) -
                at(2, 1) * at(1, 2) * at(3, 3) -
                at(3, 1) * at(2, 2) * at(1, 3);
            cof(0, 1) = -(at(1, 0) * at(2, 2) * at(3, 3) +
                at(2, 0) * at(3, 2) * at(1, 3) +
                at(3, 0) * at(1, 2) * at(2, 3) -
                at(1, 0) * at(3, 2) * at(2, 3) -
                at(2, 0) * at(1, 2) * at(3, 3) -
                at(3, 0) * at(2, 2) * at(1, 3));
            cof(0, 2) = at(1, 0) * at(2, 1) * at(3, 3) +
                at(2, 0) * at(3, 1) * at(1, 3) +
                at(3, 0) * at(1, 1) * at(2, 3) -
                at(1, 0) * at(3, 1) * at(2, 3) -
                at(2, 0) * at(1, 1) * at(3, 3) -
                at(3, 0) * at(2, 1) * at(1, 3);
            cof(0, 3) = -(at(1, 0) * at(2, 1) * at(3, 2) +
                at(2, 0) * at(3, 1) * at(1, 2) +
                at(3, 0) * at(1, 1) * at(2, 2) -
                at(1, 0) * at(3, 1) * at(2, 2) -
                at(2, 0) * at(1, 1) * at(3, 2) -
                at(3, 0) * at(2, 1) * at(1, 2));
            cof(1, 0) = -(at(0, 1) * at(2, 2) * at(3, 3) +
                at(2, 1) * at(3, 2) * at(0, 3) +
                at(3, 1) * at(0, 2) * at(2, 3) -
                at(0, 1) * at(3, 2) * at(2, 3) -
                at(2, 1) * at(0, 2) * at(3, 3) -
                at(3, 1) * at(2, 2) * at(0, 3));
            cof(1, 1) = at(0, 0) * at(2, 2) * at(3, 3) +
                at(2, 0) * at(3, 2) * at(0, 3) +
                at(3, 0) * at(0, 2) * at(2, 3) -
                at(0, 0) * at(3, 2) * at(2, 3) -
                at(2, 0) * at(0, 2) * at(3, 3) -
                at(3, 0) * at(2, 2) * at(0, 3);
            cof(1, 2) = -(at(0, 0) * at(2, 1) * at(3, 3) +
                at(2, 0) * at(3, 1) * at(0, 3) +
                at(3, 0) * at(0, 1) * at(2, 3) -
                at(0, 0) * at(3, 1) * at(2, 3) -
                at(2, 0) * at(0, 1) * at(3, 3) -
                at(3, 0) * at(2, 1) * at(0, 3));
            cof(1, 3) = at(0, 0) * at(2, 1) * at(3, 2) +
                at(2, 0) * at(3, 1) * at(0, 2) +
                at(3, 0) * at(0, 1) * at(2, 2) -
                at(0, 0) * at(3, 1) * at(2, 2) -
                at(2, 0) * at(0, 1) * at(3, 2) -
                at(3, 0) * at(2, 1) * at(0, 2);
            cof(2, 0) = at(0, 1) * at(1, 2) * at(3, 3) +
                at(1, 1) * at(3, 2) * at(0, 3) +
                at(3, 1) * at(0, 2) * at(1, 3) -
                at(0, 1) * at(3, 2) * at(1, 3) -
                at(1, 1) * at(0, 2) * at(3, 3) -
                at(3, 1) * at(1, 2) * at(0, 3);
            cof(2, 1) = -(at(0, 0) * at(1, 2) * at(3, 3) +
                at(1, 0) * at(3, 2) * at(0, 3) +
                at(3, 0) * at(0, 2) * at(1, 3) -
                at(0, 0) * at(3, 2) * at(1, 3) -
                at(1, 0) * at(0, 2) * at(3, 3) -
                at(3, 0) * at(1, 2) * at(0, 3));
            cof(2, 2) = at(0, 0) * at(1, 1) * at(3, 3) +
                at(1, 0) * at(3, 1) * at(0, 3) +
                at(3, 0) * at(0, 1) * at(1, 3) -
                at(0, 0) * at(3, 1) * at(1, 3) -
                at(1, 0) * at(0, 1) * at(3, 3) -
                at(3, 0) * at(1, 1) * at(0, 3);
            cof(2, 3) = -(at(0, 0) * at(1, 1) * at(3, 2) +
                at(1, 0) * at(3, 1) * at(0, 2) +
                at(3, 0) * at(0, 1) * at(1, 2) -
                at(0, 0) * at(3, 1) * at(1, 2) -
                at(1, 0) * at(0, 1) * at(3, 2) -
                at(3, 0) * at(1, 1) * at(0, 2));
            cof(3, 0) = -(at(0, 1) * at(1, 2) * at(2, 3) +
                at(1, 1) * at(2, 2) * at(0, 3) +
                at(2, 1) * at(0, 2) * at(1, 3) -
                at(0, 1) * at(2, 2) * at(1, 3) -
                at(1, 1) * at(0, 2) * at(2, 3) -
                at(2, 1) * at(1, 2) * at(0, 3));
            cof(3, 1) = at(0, 0) * at(1, 2) * at(2, 3) +
                at(1, 0) * at(2, 2) * at(0, 3) +
                at(2, 0) * at(0, 2) * at(1, 3) -
                at(0, 0) * at(2, 2) * at(1, 3) -
                at(1, 0) * at(0, 2) * at(2, 3) -
                at(2, 0) * at(1, 2) * at(0, 3);
            cof(3, 2) = -(at(0, 0) * at(1, 1) * at(2, 3) +
                at(1, 0) * at(2, 1) * at(0, 3) +
                at(2, 0) * at(0, 1) * at(1, 3) -
                at(0, 0) * at(2, 1) * at(1, 3) -
                at(1, 0) * at(0, 1) * at(2, 3) -
                at(2, 0) * at(1, 1) * at(0, 3));
            cof(3, 3) = at(0, 0) * at(1, 1) * at(2, 2) +
                at(1, 0) * at(2, 1) * at(0, 2) +
                at(2, 0) * at(0, 1) * at(1, 2) -
                at(0, 0) * at(2, 1) * at(1, 2) -
                at(1, 0) * at(0, 1) * at(2, 2) -
                at(2, 0) * at(1, 1) * at(0, 2);
            break;

          default:
            UNREACHABLE();
            break;
        }

        // The inverse of A is the transpose of the cofactor matrix times the reciprocal of the determinant of A.
        Matrix<T> adjugateMatrix(cof.transpose());
        T det = determinant();
        Matrix<T> result(std::vector<T>(mElements.size()), rows(), columns());
        for (unsigned int i = 0; i < rows(); i++)
            for (unsigned int j = 0; j < columns(); j++)
                result(i, j) = det ? adjugateMatrix(i, j) / det : T();

        return result;
    }

  private:
    std::vector<T> mElements;
    unsigned int mRows;
    unsigned int mCols;
};

} // namespace angle

#endif   // COMMON_MATRIX_UTILS_H_

