/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "../../../../Platform/chromium/public/WebTransformationMatrix.h"

#include <gtest/gtest.h>

#define EXPECT_ROW1_EQ(a, b, c, d, matrix)      \
    EXPECT_FLOAT_EQ((a), (matrix).m11());       \
    EXPECT_FLOAT_EQ((b), (matrix).m21());       \
    EXPECT_FLOAT_EQ((c), (matrix).m31());       \
    EXPECT_FLOAT_EQ((d), (matrix).m41());

#define EXPECT_ROW2_EQ(a, b, c, d, matrix)      \
    EXPECT_FLOAT_EQ((a), (matrix).m12());       \
    EXPECT_FLOAT_EQ((b), (matrix).m22());       \
    EXPECT_FLOAT_EQ((c), (matrix).m32());       \
    EXPECT_FLOAT_EQ((d), (matrix).m42());

#define EXPECT_ROW3_EQ(a, b, c, d, matrix)      \
    EXPECT_FLOAT_EQ((a), (matrix).m13());       \
    EXPECT_FLOAT_EQ((b), (matrix).m23());       \
    EXPECT_FLOAT_EQ((c), (matrix).m33());       \
    EXPECT_FLOAT_EQ((d), (matrix).m43());

#define EXPECT_ROW4_EQ(a, b, c, d, matrix)      \
    EXPECT_FLOAT_EQ((a), (matrix).m14());       \
    EXPECT_FLOAT_EQ((b), (matrix).m24());       \
    EXPECT_FLOAT_EQ((c), (matrix).m34());       \
    EXPECT_FLOAT_EQ((d), (matrix).m44());       \

// Checking float values for equality close to zero is not robust using EXPECT_FLOAT_EQ
// (see gtest documentation). So, to verify rotation matrices, we must use a looser
// absolute error threshold in some places.
#define EXPECT_ROW1_NEAR(a, b, c, d, matrix, errorThreshold)      \
    EXPECT_NEAR((a), (matrix).m11(), (errorThreshold));           \
    EXPECT_NEAR((b), (matrix).m21(), (errorThreshold));           \
    EXPECT_NEAR((c), (matrix).m31(), (errorThreshold));           \
    EXPECT_NEAR((d), (matrix).m41(), (errorThreshold));

#define EXPECT_ROW2_NEAR(a, b, c, d, matrix, errorThreshold)      \
    EXPECT_NEAR((a), (matrix).m12(), (errorThreshold));           \
    EXPECT_NEAR((b), (matrix).m22(), (errorThreshold));           \
    EXPECT_NEAR((c), (matrix).m32(), (errorThreshold));           \
    EXPECT_NEAR((d), (matrix).m42(), (errorThreshold));

#define EXPECT_ROW3_NEAR(a, b, c, d, matrix, errorThreshold)      \
    EXPECT_NEAR((a), (matrix).m13(), (errorThreshold));           \
    EXPECT_NEAR((b), (matrix).m23(), (errorThreshold));           \
    EXPECT_NEAR((c), (matrix).m33(), (errorThreshold));           \
    EXPECT_NEAR((d), (matrix).m43(), (errorThreshold));

#define ERROR_THRESHOLD 1e-14

using namespace WebKit;

namespace {

static void initializeTestMatrix(WebTransformationMatrix& transform)
{
    transform.setM11(10);
    transform.setM12(11);
    transform.setM13(12);
    transform.setM14(13);
    transform.setM21(14);
    transform.setM22(15);
    transform.setM23(16);
    transform.setM24(17);
    transform.setM31(18);
    transform.setM32(19);
    transform.setM33(20);
    transform.setM34(21);
    transform.setM41(22);
    transform.setM42(23);
    transform.setM43(24);
    transform.setM44(25);

    // Sanity check
    EXPECT_ROW1_EQ(10, 14, 18, 22, transform);
    EXPECT_ROW2_EQ(11, 15, 19, 23, transform);
    EXPECT_ROW3_EQ(12, 16, 20, 24, transform);
    EXPECT_ROW4_EQ(13, 17, 21, 25, transform);
}

static void initializeTestMatrix2(WebTransformationMatrix& transform)
{
    transform.setM11(30);
    transform.setM12(31);
    transform.setM13(32);
    transform.setM14(33);
    transform.setM21(34);
    transform.setM22(35);
    transform.setM23(36);
    transform.setM24(37);
    transform.setM31(38);
    transform.setM32(39);
    transform.setM33(40);
    transform.setM34(41);
    transform.setM41(42);
    transform.setM42(43);
    transform.setM43(44);
    transform.setM44(45);

    // Sanity check
    EXPECT_ROW1_EQ(30, 34, 38, 42, transform);
    EXPECT_ROW2_EQ(31, 35, 39, 43, transform);
    EXPECT_ROW3_EQ(32, 36, 40, 44, transform);
    EXPECT_ROW4_EQ(33, 37, 41, 45, transform);
}

TEST(WebTransformationMatrixTest, verifyDefaultConstructorCreatesIdentityMatrix)
{
    WebTransformationMatrix A;
    EXPECT_ROW1_EQ(1, 0, 0, 0, A);
    EXPECT_ROW2_EQ(0, 1, 0, 0, A);
    EXPECT_ROW3_EQ(0, 0, 1, 0, A);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);
    EXPECT_TRUE(A.isIdentity());
}

TEST(WebTransformationMatrixTest, verifyConstructorFor2dElements)
{
    WebTransformationMatrix A(1, 2, 3, 4, 5, 6);
    EXPECT_ROW1_EQ(1, 3, 0, 5, A);
    EXPECT_ROW2_EQ(2, 4, 0, 6, A);
    EXPECT_ROW3_EQ(0, 0, 1, 0, A);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);
}

TEST(WebTransformationMatrixTest, verifyCopyConstructor)
{
    WebTransformationMatrix A;
    initializeTestMatrix(A);

    // Copy constructor should produce exact same elements as matrix A.
    WebTransformationMatrix B(A);
    EXPECT_ROW1_EQ(10, 14, 18, 22, B);
    EXPECT_ROW2_EQ(11, 15, 19, 23, B);
    EXPECT_ROW3_EQ(12, 16, 20, 24, B);
    EXPECT_ROW4_EQ(13, 17, 21, 25, B);
}

TEST(WebTransformationMatrixTest, verifyMatrixInversion)
{
    // Invert a translation
    WebTransformationMatrix translation;
    translation.translate3d(2, 3, 4);
    EXPECT_TRUE(translation.isInvertible());

    WebTransformationMatrix inverseTranslation = translation.inverse();
    EXPECT_ROW1_EQ(1, 0, 0, -2, inverseTranslation);
    EXPECT_ROW2_EQ(0, 1, 0, -3, inverseTranslation);
    EXPECT_ROW3_EQ(0, 0, 1, -4, inverseTranslation);
    EXPECT_ROW4_EQ(0, 0, 0,  1, inverseTranslation);

    // Note that inversion should not have changed the original matrix.
    EXPECT_ROW1_EQ(1, 0, 0, 2, translation);
    EXPECT_ROW2_EQ(0, 1, 0, 3, translation);
    EXPECT_ROW3_EQ(0, 0, 1, 4, translation);
    EXPECT_ROW4_EQ(0, 0, 0, 1, translation);

    // Invert a non-uniform scale
    WebTransformationMatrix scale;
    scale.scale3d(4, 10, 100);
    EXPECT_TRUE(scale.isInvertible());

    WebTransformationMatrix inverseScale = scale.inverse();
    EXPECT_ROW1_EQ(0.25,  0,    0,    0, inverseScale);
    EXPECT_ROW2_EQ(0,    0.1,   0,    0, inverseScale);
    EXPECT_ROW3_EQ(0,     0,   0.01,  0, inverseScale);
    EXPECT_ROW4_EQ(0,     0,    0,    1, inverseScale);

    // Try to invert a matrix that is not invertible.
    // The inverse() function should simply return an identity matrix.
    WebTransformationMatrix notInvertible;
    notInvertible.setM11(0);
    notInvertible.setM22(0);
    notInvertible.setM33(0);
    notInvertible.setM44(0);
    EXPECT_FALSE(notInvertible.isInvertible());

    WebTransformationMatrix inverseOfNotInvertible;
    initializeTestMatrix(inverseOfNotInvertible); // initialize this to something non-identity, to make sure that assignment below actually took place.
    inverseOfNotInvertible = notInvertible.inverse();
    EXPECT_TRUE(inverseOfNotInvertible.isIdentity());
}

TEST(WebTransformationMatrixTest, verifyTo2DTransform)
{
    WebTransformationMatrix A;
    initializeTestMatrix(A);

    WebTransformationMatrix B = A.to2dTransform();

    EXPECT_ROW1_EQ(10, 14, 0, 22, B);
    EXPECT_ROW2_EQ(11, 15, 0, 23, B);
    EXPECT_ROW3_EQ(0,  0,  1, 0,  B);
    EXPECT_ROW4_EQ(13, 17, 0, 25, B);

    // Note that to2DTransform should not have changed the original matrix.
    EXPECT_ROW1_EQ(10, 14, 18, 22, A);
    EXPECT_ROW2_EQ(11, 15, 19, 23, A);
    EXPECT_ROW3_EQ(12, 16, 20, 24, A);
    EXPECT_ROW4_EQ(13, 17, 21, 25, A);
}

TEST(WebTransformationMatrixTest, verifyAssignmentOperator)
{
    WebTransformationMatrix A;
    initializeTestMatrix(A);
    WebTransformationMatrix B;
    initializeTestMatrix2(B);
    WebTransformationMatrix C;
    initializeTestMatrix2(C);
    C = B = A;

    // Both B and C should now have been re-assigned to the value of A.
    EXPECT_ROW1_EQ(10, 14, 18, 22, B);
    EXPECT_ROW2_EQ(11, 15, 19, 23, B);
    EXPECT_ROW3_EQ(12, 16, 20, 24, B);
    EXPECT_ROW4_EQ(13, 17, 21, 25, B);

    EXPECT_ROW1_EQ(10, 14, 18, 22, C);
    EXPECT_ROW2_EQ(11, 15, 19, 23, C);
    EXPECT_ROW3_EQ(12, 16, 20, 24, C);
    EXPECT_ROW4_EQ(13, 17, 21, 25, C);
}

TEST(WebTransformationMatrixTest, verifyEqualsBooleanOperator)
{
    WebTransformationMatrix A;
    initializeTestMatrix(A);

    WebTransformationMatrix B;
    initializeTestMatrix(B);
    EXPECT_TRUE(A == B);

    // Modifying multiple elements should cause equals operator to return false.
    WebTransformationMatrix C;
    initializeTestMatrix2(C);
    EXPECT_FALSE(A == C);

    // Modifying any one individual element should cause equals operator to return false.
    WebTransformationMatrix D;
    D = A;
    D.setM11(0);
    EXPECT_FALSE(A == D);

    D = A;
    D.setM12(0);
    EXPECT_FALSE(A == D);

    D = A;
    D.setM13(0);
    EXPECT_FALSE(A == D);

    D = A;
    D.setM14(0);
    EXPECT_FALSE(A == D);

    D = A;
    D.setM21(0);
    EXPECT_FALSE(A == D);

    D = A;
    D.setM22(0);
    EXPECT_FALSE(A == D);

    D = A;
    D.setM23(0);
    EXPECT_FALSE(A == D);

    D = A;
    D.setM24(0);
    EXPECT_FALSE(A == D);

    D = A;
    D.setM31(0);
    EXPECT_FALSE(A == D);

    D = A;
    D.setM32(0);
    EXPECT_FALSE(A == D);

    D = A;
    D.setM33(0);
    EXPECT_FALSE(A == D);

    D = A;
    D.setM34(0);
    EXPECT_FALSE(A == D);

    D = A;
    D.setM41(0);
    EXPECT_FALSE(A == D);

    D = A;
    D.setM42(0);
    EXPECT_FALSE(A == D);

    D = A;
    D.setM43(0);
    EXPECT_FALSE(A == D);

    D = A;
    D.setM44(0);
    EXPECT_FALSE(A == D);
}

TEST(WebTransformationMatrixTest, verifyMultiplyOperator)
{
    WebTransformationMatrix A;
    initializeTestMatrix(A);

    WebTransformationMatrix B;
    initializeTestMatrix2(B);

    WebTransformationMatrix C = A * B;
    EXPECT_ROW1_EQ(2036, 2292, 2548, 2804, C);
    EXPECT_ROW2_EQ(2162, 2434, 2706, 2978, C);
    EXPECT_ROW3_EQ(2288, 2576, 2864, 3152, C);
    EXPECT_ROW4_EQ(2414, 2718, 3022, 3326, C);

    // Just an additional sanity check; matrix multiplication is not commutative.
    EXPECT_FALSE(A * B == B * A);
}

TEST(WebTransformationMatrixTest, verifyMatrixMultiplication)
{
    WebTransformationMatrix A;
    initializeTestMatrix(A);

    WebTransformationMatrix B;
    initializeTestMatrix2(B);

    A.multiply(B);
    EXPECT_ROW1_EQ(2036, 2292, 2548, 2804, A);
    EXPECT_ROW2_EQ(2162, 2434, 2706, 2978, A);
    EXPECT_ROW3_EQ(2288, 2576, 2864, 3152, A);
    EXPECT_ROW4_EQ(2414, 2718, 3022, 3326, A);
}

TEST(WebTransformationMatrixTest, verifyMakeIdentiy)
{
    WebTransformationMatrix A;
    initializeTestMatrix(A);
    A.makeIdentity();
    EXPECT_ROW1_EQ(1, 0, 0, 0, A);
    EXPECT_ROW2_EQ(0, 1, 0, 0, A);
    EXPECT_ROW3_EQ(0, 0, 1, 0, A);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);
    EXPECT_TRUE(A.isIdentity());
}

TEST(WebTransformationMatrixTest, verifyTranslate)
{
    WebTransformationMatrix A;
    A.translate(2, 3);
    EXPECT_ROW1_EQ(1, 0, 0, 2, A);
    EXPECT_ROW2_EQ(0, 1, 0, 3, A);
    EXPECT_ROW3_EQ(0, 0, 1, 0, A);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);

    // Verify that translate() post-multiplies the existing matrix.
    A.makeIdentity();
    A.scale(5);
    A.translate(2, 3);
    EXPECT_ROW1_EQ(5, 0, 0, 10, A);
    EXPECT_ROW2_EQ(0, 5, 0, 15, A);
    EXPECT_ROW3_EQ(0, 0, 1, 0,  A);
    EXPECT_ROW4_EQ(0, 0, 0, 1,  A);
}

TEST(WebTransformationMatrixTest, verifyTranslate3d)
{
    WebTransformationMatrix A;
    A.translate3d(2, 3, 4);
    EXPECT_ROW1_EQ(1, 0, 0, 2, A);
    EXPECT_ROW2_EQ(0, 1, 0, 3, A);
    EXPECT_ROW3_EQ(0, 0, 1, 4, A);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);

    // Verify that translate3d() post-multiplies the existing matrix.
    A.makeIdentity();
    A.scale3d(6, 7, 8);
    A.translate3d(2, 3, 4);
    EXPECT_ROW1_EQ(6, 0, 0, 12, A);
    EXPECT_ROW2_EQ(0, 7, 0, 21, A);
    EXPECT_ROW3_EQ(0, 0, 8, 32, A);
    EXPECT_ROW4_EQ(0, 0, 0, 1,  A);
}

TEST(WebTransformationMatrixTest, verifyTranslateRight3d)
{
    WebTransformationMatrix A;
    A.translateRight3d(2, 3, 4);
    EXPECT_ROW1_EQ(1, 0, 0, 2, A);
    EXPECT_ROW2_EQ(0, 1, 0, 3, A);
    EXPECT_ROW3_EQ(0, 0, 1, 4, A);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);

    // Note carefully, all other operations do post-multiply, this one is unique.
    // Verify that translateRight3d() PRE-multiplies the existing matrix.
    A.makeIdentity();
    A.scale3d(6, 7, 8);
    A.translateRight3d(2, 3, 4);
    EXPECT_ROW1_EQ(6, 0, 0, 2, A);
    EXPECT_ROW2_EQ(0, 7, 0, 3, A);
    EXPECT_ROW3_EQ(0, 0, 8, 4, A);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);
}

TEST(WebTransformationMatrixTest, verifyScale)
{
    WebTransformationMatrix A;
    A.scale(5);
    EXPECT_ROW1_EQ(5, 0, 0, 0, A);
    EXPECT_ROW2_EQ(0, 5, 0, 0, A);
    EXPECT_ROW3_EQ(0, 0, 1, 0, A);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);

    // Verify that scale() post-multiplies the existing matrix.
    A.makeIdentity();
    A.translate3d(2, 3, 4);
    A.scale(5);
    EXPECT_ROW1_EQ(5, 0, 0, 2, A);
    EXPECT_ROW2_EQ(0, 5, 0, 3, A);
    EXPECT_ROW3_EQ(0, 0, 1, 4, A);
    EXPECT_ROW4_EQ(0, 0, 0, 1,  A);
}

TEST(WebTransformationMatrixTest, verifyNonUniformScale)
{
    WebTransformationMatrix A;
    A.scaleNonUniform(6, 7);
    EXPECT_ROW1_EQ(6, 0, 0, 0, A);
    EXPECT_ROW2_EQ(0, 7, 0, 0, A);
    EXPECT_ROW3_EQ(0, 0, 1, 0, A);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);

    // Verify that scaleNonUniform() post-multiplies the existing matrix.
    A.makeIdentity();
    A.translate3d(2, 3, 4);
    A.scaleNonUniform(6, 7);
    EXPECT_ROW1_EQ(6, 0, 0, 2, A);
    EXPECT_ROW2_EQ(0, 7, 0, 3, A);
    EXPECT_ROW3_EQ(0, 0, 1, 4, A);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);
}

TEST(WebTransformationMatrixTest, verifyScale3d)
{
    WebTransformationMatrix A;
    A.scale3d(6, 7, 8);
    EXPECT_ROW1_EQ(6, 0, 0, 0, A);
    EXPECT_ROW2_EQ(0, 7, 0, 0, A);
    EXPECT_ROW3_EQ(0, 0, 8, 0, A);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);

    // Verify that scale3d() post-multiplies the existing matrix.
    A.makeIdentity();
    A.translate3d(2, 3, 4);
    A.scale3d(6, 7, 8);
    EXPECT_ROW1_EQ(6, 0, 0, 2, A);
    EXPECT_ROW2_EQ(0, 7, 0, 3, A);
    EXPECT_ROW3_EQ(0, 0, 8, 4, A);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);
}

TEST(WebTransformationMatrixTest, verifyRotate)
{
    WebTransformationMatrix A;
    A.rotate(90);
    EXPECT_ROW1_NEAR(0, -1, 0, 0, A, ERROR_THRESHOLD);
    EXPECT_ROW2_NEAR(1, 0, 0, 0, A, ERROR_THRESHOLD);
    EXPECT_ROW3_EQ(0, 0, 1, 0, A);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);

    // Verify that rotate() post-multiplies the existing matrix.
    A.makeIdentity();
    A.scale3d(6, 7, 8);
    A.rotate(90);
    EXPECT_ROW1_NEAR(0, -6, 0, 0, A, ERROR_THRESHOLD);
    EXPECT_ROW2_NEAR(7, 0,  0, 0, A, ERROR_THRESHOLD);
    EXPECT_ROW3_EQ(0, 0, 8, 0, A);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);
}

TEST(WebTransformationMatrixTest, verifyRotate3d)
{
    WebTransformationMatrix A;

    // Check rotation about z-axis
    A.makeIdentity();
    A.rotate3d(0, 0, 90);
    EXPECT_ROW1_NEAR(0, -1, 0, 0, A, ERROR_THRESHOLD);
    EXPECT_ROW2_NEAR(1, 0, 0, 0, A, ERROR_THRESHOLD);
    EXPECT_ROW3_EQ(0, 0, 1, 0, A);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);

    // Check rotation about x-axis
    A.makeIdentity();
    A.rotate3d(90, 0, 0);
    EXPECT_ROW1_EQ(1, 0, 0, 0, A);
    EXPECT_ROW2_NEAR(0, 0, -1, 0, A, ERROR_THRESHOLD);
    EXPECT_ROW3_NEAR(0, 1, 0, 0, A, ERROR_THRESHOLD);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);

    // Check rotation about y-axis.
    // Note carefully, the expected pattern is inverted compared to rotating about x axis or z axis.
    A.makeIdentity();
    A.rotate3d(0, 90, 0);
    EXPECT_ROW1_NEAR(0, 0, 1, 0, A, ERROR_THRESHOLD);
    EXPECT_ROW2_EQ(0, 1, 0, 0, A);
    EXPECT_ROW3_NEAR(-1, 0, 0, 0, A, ERROR_THRESHOLD);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);

    // Verify that rotate3d(rx, ry, rz) post-multiplies the existing matrix.
    A.makeIdentity();
    A.scale3d(6, 7, 8);
    A.rotate3d(0, 0, 90);
    EXPECT_ROW1_NEAR(0, -6, 0, 0, A, ERROR_THRESHOLD);
    EXPECT_ROW2_NEAR(7, 0,  0, 0, A, ERROR_THRESHOLD);
    EXPECT_ROW3_EQ(0, 0, 8, 0, A);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);
}

TEST(WebTransformationMatrixTest, verifyRotate3dOrderOfCompositeRotations)
{
    // Rotate3d(degreesX, degreesY, degreesZ) is actually composite transform consiting of
    // three primitive rotations. This test verifies that the ordering of those three
    // transforms is the intended ordering.
    //
    // The correct ordering for this test case should be:
    //   1. rotate by 30 degrees about z-axis
    //   2. rotate by 20 degrees about y-axis
    //   3. rotate by 10 degrees about x-axis
    //
    // Note: there are 6 possible orderings of 3 transforms. For the specific transforms
    // used in this test, all 6 combinations produce a unique matrix that is different
    // from the other orderings. That way, this test verifies the exact ordering.

    WebTransformationMatrix A;
    A.makeIdentity();
    A.rotate3d(10, 20, 30);

    EXPECT_ROW1_NEAR(0.8137976813493738026394908,
                     -0.4409696105298823720630708,
                     0.3785223063697923939763257,
                     0, A, ERROR_THRESHOLD);
    EXPECT_ROW2_NEAR(0.4698463103929541584413698,
                     0.8825641192593856043657752,
                     0.0180283112362972230968694,
                     0, A, ERROR_THRESHOLD);
    EXPECT_ROW3_NEAR(-0.3420201433256686573969318,
                     0.1631759111665348205288950,
                     0.9254165783983233639631294,
                     0, A, ERROR_THRESHOLD);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);
}

TEST(WebTransformationMatrixTest, verifyRotateAxisAngle3d)
{
    WebTransformationMatrix A;

    // Check rotation about z-axis
    A.makeIdentity();
    A.rotate3d(0, 0, 1, 90);
    EXPECT_ROW1_NEAR(0, -1, 0, 0, A, ERROR_THRESHOLD);
    EXPECT_ROW2_NEAR(1, 0, 0, 0, A, ERROR_THRESHOLD);
    EXPECT_ROW3_EQ(0, 0, 1, 0, A);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);

    // Check rotation about x-axis
    A.makeIdentity();
    A.rotate3d(1, 0, 0, 90);
    EXPECT_ROW1_EQ(1, 0, 0, 0, A);
    EXPECT_ROW2_NEAR(0, 0, -1, 0, A, ERROR_THRESHOLD);
    EXPECT_ROW3_NEAR(0, 1, 0, 0, A, ERROR_THRESHOLD);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);

    // Check rotation about y-axis.
    // Note carefully, the expected pattern is inverted compared to rotating about x axis or z axis.
    A.makeIdentity();
    A.rotate3d(0, 1, 0, 90);
    EXPECT_ROW1_NEAR(0, 0, 1, 0, A, ERROR_THRESHOLD);
    EXPECT_ROW2_EQ(0, 1, 0, 0, A);
    EXPECT_ROW3_NEAR(-1, 0, 0, 0, A, ERROR_THRESHOLD);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);

    // Verify that rotate3d(axis, angle) post-multiplies the existing matrix.
    A.makeIdentity();
    A.scale3d(6, 7, 8);
    A.rotate3d(0, 0, 1, 90);
    EXPECT_ROW1_NEAR(0, -6, 0, 0, A, ERROR_THRESHOLD);
    EXPECT_ROW2_NEAR(7, 0,  0, 0, A, ERROR_THRESHOLD);
    EXPECT_ROW3_EQ(0, 0, 8, 0, A);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);
}

TEST(WebTransformationMatrixTest, verifySkewX)
{
    WebTransformationMatrix A;
    A.skewX(45);
    EXPECT_ROW1_EQ(1, 1, 0, 0, A);
    EXPECT_ROW2_EQ(0, 1, 0, 0, A);
    EXPECT_ROW3_EQ(0, 0, 1, 0, A);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);

    // Verify that skewX() post-multiplies the existing matrix.
    // Row 1, column 2, would incorrectly have value "7" if the matrix is pre-multiplied instead of post-multiplied.
    A.makeIdentity();
    A.scale3d(6, 7, 8);
    A.skewX(45);
    EXPECT_ROW1_EQ(6, 6, 0, 0, A);
    EXPECT_ROW2_EQ(0, 7, 0, 0, A);
    EXPECT_ROW3_EQ(0, 0, 8, 0, A);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);
}

TEST(WebTransformationMatrixTest, verifySkewY)
{
    WebTransformationMatrix A;
    A.skewY(45);
    EXPECT_ROW1_EQ(1, 0, 0, 0, A);
    EXPECT_ROW2_EQ(1, 1, 0, 0, A);
    EXPECT_ROW3_EQ(0, 0, 1, 0, A);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);

    // Verify that skewY() post-multiplies the existing matrix.
    // Row 2, column 1, would incorrectly have value "6" if the matrix is pre-multiplied instead of post-multiplied.
    A.makeIdentity();
    A.scale3d(6, 7, 8);
    A.skewY(45);
    EXPECT_ROW1_EQ(6, 0, 0, 0, A);
    EXPECT_ROW2_EQ(7, 7, 0, 0, A);
    EXPECT_ROW3_EQ(0, 0, 8, 0, A);
    EXPECT_ROW4_EQ(0, 0, 0, 1, A);
}

TEST(WebTransformationMatrixTest, verifyApplyPerspective)
{
    WebTransformationMatrix A;
    A.applyPerspective(1);
    EXPECT_ROW1_EQ(1, 0,  0, 0, A);
    EXPECT_ROW2_EQ(0, 1,  0, 0, A);
    EXPECT_ROW3_EQ(0, 0,  1, 0, A);
    EXPECT_ROW4_EQ(0, 0, -1, 1, A);

    // Verify that applyPerspective() post-multiplies the existing matrix.
    A.makeIdentity();
    A.translate3d(2, 3, 4);
    A.applyPerspective(1);
    EXPECT_ROW1_EQ(1, 0, -2, 2, A);
    EXPECT_ROW2_EQ(0, 1, -3, 3, A);
    EXPECT_ROW3_EQ(0, 0, -3, 4, A);
    EXPECT_ROW4_EQ(0, 0, -1, 1, A);
}

TEST(WebTransformationMatrixTest, verifyHasPerspective)
{
    WebTransformationMatrix A;
    A.applyPerspective(1);
    EXPECT_TRUE(A.hasPerspective());

    A.makeIdentity();
    A.applyPerspective(0);
    EXPECT_FALSE(A.hasPerspective());

    A.makeIdentity();
    A.setM34(-0.3);
    EXPECT_TRUE(A.hasPerspective());

    // FIXME: WebCore only checkes m34() for perspective, but that is probably
    //        wrong. https://bugs.webkit.org/show_bug.cgi?id=83088. For now, this test
    //        case expects the exact behavior as implemented by WebCore, but this should
    //        probably be changed so that if the entire bottom row is not exactly
    //        (0, 0, 0, 1), then hasPerspective should return true.

    A.makeIdentity();
    A.setM14(-1);
    EXPECT_FALSE(A.hasPerspective());

    A.makeIdentity();
    A.setM24(-1);
    EXPECT_FALSE(A.hasPerspective());

    A.makeIdentity();
    A.setM44(0.5);
    EXPECT_FALSE(A.hasPerspective());
}

TEST(WebTransformationMatrixTest, verifyIsInvertible)
{
    WebTransformationMatrix A;

    // Translations, rotations, scales, skews and arbitrary combinations of them are invertible.
    A.makeIdentity();
    EXPECT_TRUE(A.isInvertible());

    A.makeIdentity();
    A.translate3d(2, 3, 4);
    EXPECT_TRUE(A.isInvertible());

    A.makeIdentity();
    A.scale3d(6, 7, 8);
    EXPECT_TRUE(A.isInvertible());

    A.makeIdentity();
    A.rotate3d(10, 20, 30);
    EXPECT_TRUE(A.isInvertible());

    A.makeIdentity();
    A.skewX(45);
    EXPECT_TRUE(A.isInvertible());

    // A perspective matrix (projection plane at z=0) is invertible. The intuitive
    // explanation is that perspective is eqivalent to a skew of the w-axis; skews are
    // invertible.
    A.makeIdentity();
    A.applyPerspective(1);
    EXPECT_TRUE(A.isInvertible());

    // A "pure" perspective matrix derived by similar triangles, with m44() set to zero
    // (i.e. camera positioned at the origin), is not invertible.
    A.makeIdentity();
    A.applyPerspective(1);
    A.setM44(0);
    EXPECT_FALSE(A.isInvertible());

    // Adding more to a non-invertible matrix will not make it invertible in the general case.
    A.makeIdentity();
    A.applyPerspective(1);
    A.setM44(0);
    A.scale3d(6, 7, 8);
    A.rotate3d(10, 20, 30);
    A.translate3d(6, 7, 8);
    EXPECT_FALSE(A.isInvertible());

    // A degenerate matrix of all zeros is not invertible.
    A.makeIdentity();
    A.setM11(0);
    A.setM22(0);
    A.setM33(0);
    A.setM44(0);
    EXPECT_FALSE(A.isInvertible());
}

TEST(WebTransformationMatrixTest, verifyIsIdentity)
{
    WebTransformationMatrix A;

    initializeTestMatrix(A);
    EXPECT_FALSE(A.isIdentity());

    A.makeIdentity();
    EXPECT_TRUE(A.isIdentity());

    // Modifying any one individual element should cause the matrix to no longer be identity.
    A.makeIdentity();
    A.setM11(2);
    EXPECT_FALSE(A.isIdentity());

    A.makeIdentity();
    A.setM12(2);
    EXPECT_FALSE(A.isIdentity());

    A.makeIdentity();
    A.setM13(2);
    EXPECT_FALSE(A.isIdentity());

    A.makeIdentity();
    A.setM14(2);
    EXPECT_FALSE(A.isIdentity());

    A.makeIdentity();
    A.setM21(2);
    EXPECT_FALSE(A.isIdentity());

    A.makeIdentity();
    A.setM22(2);
    EXPECT_FALSE(A.isIdentity());

    A.makeIdentity();
    A.setM23(2);
    EXPECT_FALSE(A.isIdentity());

    A.makeIdentity();
    A.setM24(2);
    EXPECT_FALSE(A.isIdentity());

    A.makeIdentity();
    A.setM31(2);
    EXPECT_FALSE(A.isIdentity());

    A.makeIdentity();
    A.setM32(2);
    EXPECT_FALSE(A.isIdentity());

    A.makeIdentity();
    A.setM33(2);
    EXPECT_FALSE(A.isIdentity());

    A.makeIdentity();
    A.setM34(2);
    EXPECT_FALSE(A.isIdentity());

    A.makeIdentity();
    A.setM41(2);
    EXPECT_FALSE(A.isIdentity());

    A.makeIdentity();
    A.setM42(2);
    EXPECT_FALSE(A.isIdentity());

    A.makeIdentity();
    A.setM43(2);
    EXPECT_FALSE(A.isIdentity());

    A.makeIdentity();
    A.setM44(2);
    EXPECT_FALSE(A.isIdentity());
}

TEST(WebTransformationMatrixTest, verifyIsIdentityOrTranslation)
{
    WebTransformationMatrix A;

    initializeTestMatrix(A);
    EXPECT_FALSE(A.isIdentityOrTranslation());

    A.makeIdentity();
    EXPECT_TRUE(A.isIdentityOrTranslation());

    // Modifying any non-translation components should cause isIdentityOrTranslation() to
    // return false. NOTE: m41(), m42(), and m43() are the translation components, so
    // modifying them should still return true for isIdentityOrTranslation().
    A.makeIdentity();
    A.setM11(2);
    EXPECT_FALSE(A.isIdentityOrTranslation());

    A.makeIdentity();
    A.setM12(2);
    EXPECT_FALSE(A.isIdentityOrTranslation());

    A.makeIdentity();
    A.setM13(2);
    EXPECT_FALSE(A.isIdentityOrTranslation());

    A.makeIdentity();
    A.setM14(2);
    EXPECT_FALSE(A.isIdentityOrTranslation());

    A.makeIdentity();
    A.setM21(2);
    EXPECT_FALSE(A.isIdentityOrTranslation());

    A.makeIdentity();
    A.setM22(2);
    EXPECT_FALSE(A.isIdentityOrTranslation());

    A.makeIdentity();
    A.setM23(2);
    EXPECT_FALSE(A.isIdentityOrTranslation());

    A.makeIdentity();
    A.setM24(2);
    EXPECT_FALSE(A.isIdentityOrTranslation());

    A.makeIdentity();
    A.setM31(2);
    EXPECT_FALSE(A.isIdentityOrTranslation());

    A.makeIdentity();
    A.setM32(2);
    EXPECT_FALSE(A.isIdentityOrTranslation());

    A.makeIdentity();
    A.setM33(2);
    EXPECT_FALSE(A.isIdentityOrTranslation());

    A.makeIdentity();
    A.setM34(2);
    EXPECT_FALSE(A.isIdentityOrTranslation());

    // Note carefully - expecting true here.
    A.makeIdentity();
    A.setM41(2);
    EXPECT_TRUE(A.isIdentityOrTranslation());

    // Note carefully - expecting true here.
    A.makeIdentity();
    A.setM42(2);
    EXPECT_TRUE(A.isIdentityOrTranslation());

    // Note carefully - expecting true here.
    A.makeIdentity();
    A.setM43(2);
    EXPECT_TRUE(A.isIdentityOrTranslation());

    A.makeIdentity();
    A.setM44(2);
    EXPECT_FALSE(A.isIdentityOrTranslation());
}

TEST(WebTransformationMatrixTest, isIntegerTranslation)
{
    WebTransformationMatrix A;

    A.makeIdentity();
    A.translate(2, 3);
    EXPECT_TRUE(A.isIntegerTranslation());

    A.makeIdentity();
    A.translate(2.0, 3.0);
    EXPECT_TRUE(A.isIntegerTranslation());

    A.makeIdentity();
    A.translate(2.00001, 3);
    EXPECT_FALSE(A.isIntegerTranslation());

    A.makeIdentity();
    A.translate(2, 2.99999);
    EXPECT_FALSE(A.isIntegerTranslation());

    // Stacking many integer translations should ideally not accumulate any precision error.
    A.makeIdentity();
    for (int i = 0; i < 100000; ++i)
        A.translate(2, 3);
    EXPECT_TRUE(A.isIntegerTranslation());
}

} // namespace
