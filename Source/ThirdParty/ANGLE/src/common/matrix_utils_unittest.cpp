//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// matrix_utils_unittests:
//   Unit tests for the matrix utils.
//

#include "matrix_utils.h"

#include <gtest/gtest.h>

using namespace angle;

namespace
{

const unsigned int minDimensions = 2;
const unsigned int maxDimensions = 4;

TEST(MatrixUtilsTest, MatrixConstructorTest)
{
    for (unsigned int i = minDimensions; i <= maxDimensions; i++)
    {
        for (unsigned int j = minDimensions; j <= maxDimensions; j++)
        {
            unsigned int numElements = i * j;
            Matrix<float> m(std::vector<float>(numElements, 1.0f), i, j);
            EXPECT_EQ(m.rows(), i);
            EXPECT_EQ(m.columns(), j);
            EXPECT_EQ(m.elements(), std::vector<float>(numElements, 1.0f));
        }
    }

    for (unsigned int i = minDimensions; i <= maxDimensions; i++)
    {
        unsigned int numElements = i * i;
        Matrix<float> m(std::vector<float>(numElements, 1.0f), i);
        EXPECT_EQ(m.size(), i);
        EXPECT_EQ(m.columns(), m.columns());
        EXPECT_EQ(m.elements(), std::vector<float>(numElements, 1.0f));
    }
}

TEST(MatrixUtilsTest, MatrixCompMultTest)
{
    for (unsigned int i = minDimensions; i <= maxDimensions; i++)
    {
        unsigned int numElements = i * i;
        Matrix<float> m1(std::vector<float>(numElements, 2.0f), i);
        Matrix<float> actualResult = m1.compMult(m1);
        std::vector<float> actualResultElements = actualResult.elements();
        std::vector<float> expectedResultElements(numElements, 4.0f);
        EXPECT_EQ(expectedResultElements, actualResultElements);
    }
}

TEST(MatrixUtilsTest, MatrixOuterProductTest)
{
    for (unsigned int i = minDimensions; i <= maxDimensions; i++)
    {
        for (unsigned int j = minDimensions; j <= maxDimensions; j++)
        {
            unsigned int numElements = i * j;
            Matrix<float> m1(std::vector<float>(numElements, 2.0f), i, 1);
            Matrix<float> m2(std::vector<float>(numElements, 2.0f), 1, j);
            Matrix<float> actualResult = m1.outerProduct(m2);
            EXPECT_EQ(actualResult.rows(), i);
            EXPECT_EQ(actualResult.columns(), j);
            std::vector<float> actualResultElements = actualResult.elements();
            std::vector<float> expectedResultElements(numElements, 4.0f);
            EXPECT_EQ(expectedResultElements, actualResultElements);
        }
    }
}

TEST(MatrixUtilsTest, MatrixTransposeTest)
{
    for (unsigned int i = minDimensions; i <= maxDimensions; i++)
    {
        for (unsigned int j = minDimensions; j <= maxDimensions; j++)
        {
            unsigned int numElements = i * j;
            Matrix<float> m1(std::vector<float>(numElements, 2.0f), i, j);
            Matrix<float> expectedResult = Matrix<float>(std::vector<float>(numElements, 2.0f), j, i);
            Matrix<float> actualResult = m1.transpose();
            EXPECT_EQ(expectedResult.elements(), actualResult.elements());
            EXPECT_EQ(actualResult.rows(), expectedResult.rows());
            EXPECT_EQ(actualResult.columns(), expectedResult.columns());
            // transpose(transpose(A)) = A
            Matrix<float> m2 = actualResult.transpose();
            EXPECT_EQ(m1.elements(), m2.elements());
        }
    }
}

TEST(MatrixUtilsTest, MatrixDeterminantTest)
{
    for (unsigned int i = minDimensions; i <= maxDimensions; i++)
    {
        unsigned int numElements = i * i;
        Matrix<float> m(std::vector<float>(numElements, 2.0f), i);
        EXPECT_EQ(m.determinant(), 0.0f);
    }
}

TEST(MatrixUtilsTest, 2x2MatrixInverseTest)
{
    float inputElements[] =
    {
        2.0f, 5.0f,
        3.0f, 7.0f
    };
    unsigned int numElements = 4;
    std::vector<float> input(inputElements, inputElements + numElements);
    Matrix<float> inputMatrix(input, 2);
    float identityElements[] =
    {
        1.0f, 0.0f,
        0.0f, 1.0f
    };
    std::vector<float> identityMatrix(identityElements, identityElements + numElements);
    // A * inverse(A) = I, where I is identity matrix.
    Matrix<float> result = inputMatrix * inputMatrix.inverse();
    EXPECT_EQ(identityMatrix, result.elements());
}

TEST(MatrixUtilsTest, 3x3MatrixInverseTest)
{
    float inputElements[] =
    {
        11.0f, 23.0f, 37.0f,
        13.0f, 29.0f, 41.0f,
        19.0f, 31.0f, 43.0f
    };
    unsigned int numElements = 9;
    std::vector<float> input(inputElements, inputElements + numElements);
    Matrix<float> inputMatrix(input, 3);
    float identityElements[] =
    {
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f
    };
    std::vector<float> identityMatrix(identityElements, identityElements + numElements);
    // A * inverse(A) = I, where I is identity matrix.
    Matrix<float> result = inputMatrix * inputMatrix.inverse();
    std::vector<float> resultElements = result.elements();
    const float floatFaultTolarance = 0.000001f;
    for (size_t i = 0; i < numElements; i++)
        EXPECT_NEAR(resultElements[i], identityMatrix[i], floatFaultTolarance);
}

TEST(MatrixUtilsTest, 4x4MatrixInverseTest)
{
    float inputElements[] =
    {
        29.0f, 43.0f, 61.0f, 79.0f,
        31.0f, 47.0f, 67.0f, 83.0f,
        37.0f, 53.0f, 71.0f, 89.0f,
        41.0f, 59.0f, 73.0f, 97.0f
    };
    unsigned int numElements = 16;
    std::vector<float> input(inputElements, inputElements + numElements);
    Matrix<float> inputMatrix(input, 4);
    float identityElements[] =
    {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    std::vector<float> identityMatrix(identityElements, identityElements + numElements);
    // A * inverse(A) = I, where I is identity matrix.
    Matrix<float> result = inputMatrix * inputMatrix.inverse();
    std::vector<float> resultElements = result.elements();
    const float floatFaultTolarance = 0.00001f;
    for (unsigned int i = 0; i < numElements; i++)
        EXPECT_NEAR(resultElements[i], identityMatrix[i], floatFaultTolarance);
}

}

