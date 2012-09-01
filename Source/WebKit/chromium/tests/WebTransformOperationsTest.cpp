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

#include <public/WebTransformOperations.h>

#include "CCGeometryTestUtils.h"
#include <gtest/gtest.h>
#include <public/WebTransformationMatrix.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

using namespace std;
using namespace WebKit;

TEST(WebTransformOperationTest, transformTypesAreUnique)
{
    Vector<OwnPtr<WebTransformOperations> > transforms;

    WebTransformOperations* toAdd = new WebTransformOperations();
    toAdd->appendTranslate(1, 0, 0);
    transforms.append(adoptPtr(toAdd));

    toAdd = new WebTransformOperations();
    toAdd->appendRotate(0, 0, 1, 2);
    transforms.append(adoptPtr(toAdd));

    toAdd = new WebTransformOperations();
    toAdd->appendScale(2, 2, 2);
    transforms.append(adoptPtr(toAdd));

    toAdd = new WebTransformOperations();
    toAdd->appendSkew(1, 0);
    transforms.append(adoptPtr(toAdd));

    toAdd = new WebTransformOperations();
    toAdd->appendPerspective(800);
    transforms.append(adoptPtr(toAdd));

    for (size_t i = 0; i < transforms.size(); ++i) {
        for (size_t j = 0; j < transforms.size(); ++j) {
            bool matchesType = transforms[i]->matchesTypes(*transforms[j]);
            EXPECT_TRUE((i == j && matchesType) || !matchesType);
        }
    }
}

TEST(WebTransformOperationTest, matchTypesSameLength)
{
    WebTransformOperations translates;
    translates.appendTranslate(1, 0, 0);
    translates.appendTranslate(1, 0, 0);
    translates.appendTranslate(1, 0, 0);

    WebTransformOperations skews;
    skews.appendSkew(0, 2);
    skews.appendSkew(0, 2);
    skews.appendSkew(0, 2);

    WebTransformOperations translates2;
    translates2.appendTranslate(0, 2, 0);
    translates2.appendTranslate(0, 2, 0);
    translates2.appendTranslate(0, 2, 0);

    WebTransformOperations translates3 = translates2;

    EXPECT_FALSE(translates.matchesTypes(skews));
    EXPECT_TRUE(translates.matchesTypes(translates2));
    EXPECT_TRUE(translates.matchesTypes(translates3));
}

TEST(WebTransformOperationTest, matchTypesDifferentLength)
{
    WebTransformOperations translates;
    translates.appendTranslate(1, 0, 0);
    translates.appendTranslate(1, 0, 0);
    translates.appendTranslate(1, 0, 0);

    WebTransformOperations skews;
    skews.appendSkew(2, 0);
    skews.appendSkew(2, 0);

    WebTransformOperations translates2;
    translates2.appendTranslate(0, 2, 0);
    translates2.appendTranslate(0, 2, 0);

    EXPECT_FALSE(translates.matchesTypes(skews));
    EXPECT_FALSE(translates.matchesTypes(translates2));
}

void getIdentityOperations(Vector<OwnPtr<WebTransformOperations> >* operations)
{
    WebTransformOperations* toAdd = new WebTransformOperations();
    operations->append(adoptPtr(toAdd));

    toAdd = new WebTransformOperations();
    toAdd->appendTranslate(0, 0, 0);
    operations->append(adoptPtr(toAdd));

    toAdd = new WebTransformOperations();
    toAdd->appendTranslate(0, 0, 0);
    toAdd->appendTranslate(0, 0, 0);
    operations->append(adoptPtr(toAdd));

    toAdd = new WebTransformOperations();
    toAdd->appendScale(1, 1, 1);
    operations->append(adoptPtr(toAdd));

    toAdd = new WebTransformOperations();
    toAdd->appendScale(1, 1, 1);
    toAdd->appendScale(1, 1, 1);
    operations->append(adoptPtr(toAdd));

    toAdd = new WebTransformOperations();
    toAdd->appendSkew(0, 0);
    operations->append(adoptPtr(toAdd));

    toAdd = new WebTransformOperations();
    toAdd->appendSkew(0, 0);
    toAdd->appendSkew(0, 0);
    operations->append(adoptPtr(toAdd));

    toAdd = new WebTransformOperations();
    toAdd->appendRotate(0, 0, 1, 0);
    operations->append(adoptPtr(toAdd));

    toAdd = new WebTransformOperations();
    toAdd->appendRotate(0, 0, 1, 0);
    toAdd->appendRotate(0, 0, 1, 0);
    operations->append(adoptPtr(toAdd));

    toAdd = new WebTransformOperations();
    toAdd->appendMatrix(WebTransformationMatrix());
    operations->append(adoptPtr(toAdd));

    toAdd = new WebTransformOperations();
    toAdd->appendMatrix(WebTransformationMatrix());
    toAdd->appendMatrix(WebTransformationMatrix());
    operations->append(adoptPtr(toAdd));
}

TEST(WebTransformOperationTest, identityAlwaysMatches)
{
    Vector<OwnPtr<WebTransformOperations> > operations;
    getIdentityOperations(&operations);

    for (size_t i = 0; i < operations.size(); ++i) {
        for (size_t j = 0; j < operations.size(); ++j)
            EXPECT_TRUE(operations[i]->matchesTypes(*operations[j]));
    }
}

TEST(WebTransformOperationTest, applyTranslate)
{
    double x = 1;
    double y = 2;
    double z = 3;
    WebTransformOperations operations;
    operations.appendTranslate(x, y, z);
    WebTransformationMatrix expected;
    expected.translate3d(x, y, z);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, operations.apply());
}

TEST(WebTransformOperationTest, applyRotate)
{
    double x = 1;
    double y = 2;
    double z = 3;
    double degrees = 80;
    WebTransformOperations operations;
    operations.appendRotate(x, y, z, degrees);
    WebTransformationMatrix expected;
    expected.rotate3d(x, y, z, degrees);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, operations.apply());
}

TEST(WebTransformOperationTest, applyScale)
{
    double x = 1;
    double y = 2;
    double z = 3;
    WebTransformOperations operations;
    operations.appendScale(x, y, z);
    WebTransformationMatrix expected;
    expected.scale3d(x, y, z);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, operations.apply());
}

TEST(WebTransformOperationTest, applySkew)
{
    double x = 1;
    double y = 2;
    WebTransformOperations operations;
    operations.appendSkew(x, y);
    WebTransformationMatrix expected;
    expected.skewX(x);
    expected.skewY(y);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, operations.apply());
}

TEST(WebTransformOperationTest, applyPerspective)
{
    double depth = 800;
    WebTransformOperations operations;
    operations.appendPerspective(depth);
    WebTransformationMatrix expected;
    expected.applyPerspective(depth);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, operations.apply());
}

TEST(WebTransformOperationTest, applyMatrix)
{
    double dx = 1;
    double dy = 2;
    double dz = 3;
    WebTransformationMatrix expectedMatrix;
    expectedMatrix.translate3d(dx, dy, dz);
    WebTransformOperations matrixTransform;
    matrixTransform.appendMatrix(expectedMatrix);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expectedMatrix, matrixTransform.apply());
}

TEST(WebTransformOperationTest, applyOrder)
{
    double sx = 2;
    double sy = 4;
    double sz = 8;

    double dx = 1;
    double dy = 2;
    double dz = 3;

    WebTransformOperations operations;
    operations.appendScale(sx, sy, sz);
    operations.appendTranslate(dx, dy, dz);

    WebTransformationMatrix expectedScaleMatrix;
    expectedScaleMatrix.scale3d(sx, sy, sz);

    WebTransformationMatrix expectedTranslateMatrix;
    expectedTranslateMatrix.translate3d(dx, dy, dz);

    WebTransformationMatrix expectedCombinedMatrix = expectedScaleMatrix;
    expectedCombinedMatrix.multiply(expectedTranslateMatrix);

    EXPECT_TRANSFORMATION_MATRIX_EQ(expectedCombinedMatrix, operations.apply());
}

TEST(WebTransformOperationTest, blendOrder)
{
    double sx1 = 2;
    double sy1 = 4;
    double sz1 = 8;

    double dx1 = 1;
    double dy1 = 2;
    double dz1 = 3;

    double sx2 = 4;
    double sy2 = 8;
    double sz2 = 16;

    double dx2 = 10;
    double dy2 = 20;
    double dz2 = 30;

    WebTransformOperations operationsFrom;
    operationsFrom.appendScale(sx1, sy1, sz1);
    operationsFrom.appendTranslate(dx1, dy1, dz1);

    WebTransformOperations operationsTo;
    operationsTo.appendScale(sx2, sy2, sz2);
    operationsTo.appendTranslate(dx2, dy2, dz2);

    WebTransformationMatrix scaleFrom;
    scaleFrom.scale3d(sx1, sy1, sz1);
    WebTransformationMatrix translateFrom;
    translateFrom.translate3d(dx1, dy1, dz1);

    WebTransformationMatrix scaleTo;
    scaleTo.scale3d(sx2, sy2, sz2);
    WebTransformationMatrix translateTo;
    translateTo.translate3d(dx2, dy2, dz2);

    double progress = 0.25;

    WebTransformationMatrix blendedScale = scaleTo;
    blendedScale.blend(scaleFrom, progress);

    WebTransformationMatrix blendedTranslate = translateTo;
    blendedTranslate.blend(translateFrom, progress);

    WebTransformationMatrix expected = blendedScale;
    expected.multiply(blendedTranslate);

    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, operationsTo.blend(operationsFrom, progress));
}

static void checkProgress(double progress,
                          const WebTransformationMatrix& fromMatrix,
                          const WebTransformationMatrix& toMatrix,
                          const WebTransformOperations& fromTransform,
                          const WebTransformOperations& toTransform)
{
    WebTransformationMatrix expectedMatrix = toMatrix;
    expectedMatrix.blend(fromMatrix, progress);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expectedMatrix, toTransform.blend(fromTransform, progress));
}

TEST(WebTransformOperationTest, blendProgress)
{
    double sx = 2;
    double sy = 4;
    double sz = 8;
    WebTransformOperations operationsFrom;
    operationsFrom.appendScale(sx, sy, sz);

    WebTransformationMatrix matrixFrom;
    matrixFrom.scale3d(sx, sy, sz);

    sx = 4;
    sy = 8;
    sz = 16;
    WebTransformOperations operationsTo;
    operationsTo.appendScale(sx, sy, sz);

    WebTransformationMatrix matrixTo;
    matrixTo.scale3d(sx, sy, sz);

    checkProgress(-1, matrixFrom, matrixTo, operationsFrom, operationsTo);
    checkProgress(0, matrixFrom, matrixTo, operationsFrom, operationsTo);
    checkProgress(0.25, matrixFrom, matrixTo, operationsFrom, operationsTo);
    checkProgress(0.5, matrixFrom, matrixTo, operationsFrom, operationsTo);
    checkProgress(1, matrixFrom, matrixTo, operationsFrom, operationsTo);
    checkProgress(2, matrixFrom, matrixTo, operationsFrom, operationsTo);
}

TEST(WebTransformOperationTest, blendWhenTypesDoNotMatch)
{
    double sx1 = 2;
    double sy1 = 4;
    double sz1 = 8;

    double dx1 = 1;
    double dy1 = 2;
    double dz1 = 3;

    double sx2 = 4;
    double sy2 = 8;
    double sz2 = 16;

    double dx2 = 10;
    double dy2 = 20;
    double dz2 = 30;

    WebTransformOperations operationsFrom;
    operationsFrom.appendScale(sx1, sy1, sz1);
    operationsFrom.appendTranslate(dx1, dy1, dz1);

    WebTransformOperations operationsTo;
    operationsTo.appendTranslate(dx2, dy2, dz2);
    operationsTo.appendScale(sx2, sy2, sz2);

    WebTransformationMatrix from;
    from.scale3d(sx1, sy1, sz1);
    from.translate3d(dx1, dy1, dz1);

    WebTransformationMatrix to;
    to.translate3d(dx2, dy2, dz2);
    to.scale3d(sx2, sy2, sz2);

    double progress = 0.25;

    WebTransformationMatrix expected = to;
    expected.blend(from, progress);

    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, operationsTo.blend(operationsFrom, progress));
}

TEST(WebTransformOperationTest, largeRotationsWithSameAxis)
{
    WebTransformOperations operationsFrom;
    operationsFrom.appendRotate(0, 0, 1, 0);

    WebTransformOperations operationsTo;
    operationsTo.appendRotate(0, 0, 2, 360);

    double progress = 0.5;

    WebTransformationMatrix expected;
    expected.rotate3d(0, 0, 1, 180);

    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, operationsTo.blend(operationsFrom, progress));
}

TEST(WebTransformOperationTest, largeRotationsWithSameAxisInDifferentDirection)
{
    WebTransformOperations operationsFrom;
    operationsFrom.appendRotate(0, 0, 1, 180);

    WebTransformOperations operationsTo;
    operationsTo.appendRotate(0, 0, -1, 180);

    double progress = 0.5;

    WebTransformationMatrix expected;

    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, operationsTo.blend(operationsFrom, progress));
}

TEST(WebTransformOperationTest, largeRotationsWithDifferentAxes)
{
    WebTransformOperations operationsFrom;
    operationsFrom.appendRotate(0, 0, 1, 180);

    WebTransformOperations operationsTo;
    operationsTo.appendRotate(0, 1, 0, 180);

    double progress = 0.5;
    WebTransformationMatrix matrixFrom;
    matrixFrom.rotate3d(0, 0, 1, 180);

    WebTransformationMatrix matrixTo;
    matrixTo.rotate3d(0, 1, 0, 180);

    WebTransformationMatrix expected = matrixTo;
    expected.blend(matrixFrom, progress);

    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, operationsTo.blend(operationsFrom, progress));
}

TEST(WebTransformOperationTest, blendRotationFromIdentity)
{
    Vector<OwnPtr<WebTransformOperations> > identityOperations;
    getIdentityOperations(&identityOperations);

    for (size_t i = 0; i < identityOperations.size(); ++i) {
        WebTransformOperations operations;
        operations.appendRotate(0, 0, 1, 360);

        double progress = 0.5;

        WebTransformationMatrix expected;
        expected.rotate3d(0, 0, 1, 180);

        EXPECT_TRANSFORMATION_MATRIX_EQ(expected, operations.blend(*identityOperations[i], progress));
    }
}

TEST(WebTransformOperationTest, blendTranslationFromIdentity)
{
    Vector<OwnPtr<WebTransformOperations> > identityOperations;
    getIdentityOperations(&identityOperations);

    for (size_t i = 0; i < identityOperations.size(); ++i) {
        WebTransformOperations operations;
        operations.appendTranslate(2, 2, 2);

        double progress = 0.5;

        WebTransformationMatrix expected;
        expected.translate3d(1, 1, 1);

        EXPECT_TRANSFORMATION_MATRIX_EQ(expected, operations.blend(*identityOperations[i], progress));
    }
}

TEST(WebTransformOperationTest, blendScaleFromIdentity)
{
    Vector<OwnPtr<WebTransformOperations> > identityOperations;
    getIdentityOperations(&identityOperations);

    for (size_t i = 0; i < identityOperations.size(); ++i) {
        WebTransformOperations operations;
        operations.appendScale(3, 3, 3);

        double progress = 0.5;

        WebTransformationMatrix expected;
        expected.scale3d(2, 2, 2);

        EXPECT_TRANSFORMATION_MATRIX_EQ(expected, operations.blend(*identityOperations[i], progress));
    }
}

TEST(WebTransformOperationTest, blendSkewFromIdentity)
{
    Vector<OwnPtr<WebTransformOperations> > identityOperations;
    getIdentityOperations(&identityOperations);

    for (size_t i = 0; i < identityOperations.size(); ++i) {
        WebTransformOperations operations;
        operations.appendSkew(2, 2);

        double progress = 0.5;

        WebTransformationMatrix expected;
        expected.skewX(1);
        expected.skewY(1);

        EXPECT_TRANSFORMATION_MATRIX_EQ(expected, operations.blend(*identityOperations[i], progress));
    }
}

TEST(WebTransformOperationTest, blendPerspectiveFromIdentity)
{
    Vector<OwnPtr<WebTransformOperations> > identityOperations;
    getIdentityOperations(&identityOperations);

    for (size_t i = 0; i < identityOperations.size(); ++i) {
        WebTransformOperations operations;
        operations.appendPerspective(1000);

        double progress = 0.5;

        WebTransformationMatrix expected;
        expected.applyPerspective(500 + 0.5 * numeric_limits<double>::max());

        EXPECT_TRANSFORMATION_MATRIX_EQ(expected, operations.blend(*identityOperations[i], progress));
    }
}

TEST(WebTransformOperationTest, blendRotationToIdentity)
{
    Vector<OwnPtr<WebTransformOperations> > identityOperations;
    getIdentityOperations(&identityOperations);

    for (size_t i = 0; i < identityOperations.size(); ++i) {
        WebTransformOperations operations;
        operations.appendRotate(0, 0, 1, 360);

        double progress = 0.5;

        WebTransformationMatrix expected;
        expected.rotate3d(0, 0, 1, 180);

        EXPECT_TRANSFORMATION_MATRIX_EQ(expected, identityOperations[i]->blend(operations, progress));
    }
}

TEST(WebTransformOperationTest, blendTranslationToIdentity)
{
    Vector<OwnPtr<WebTransformOperations> > identityOperations;
    getIdentityOperations(&identityOperations);

    for (size_t i = 0; i < identityOperations.size(); ++i) {
        WebTransformOperations operations;
        operations.appendTranslate(2, 2, 2);

        double progress = 0.5;

        WebTransformationMatrix expected;
        expected.translate3d(1, 1, 1);

        EXPECT_TRANSFORMATION_MATRIX_EQ(expected, identityOperations[i]->blend(operations, progress));
    }
}

TEST(WebTransformOperationTest, blendScaleToIdentity)
{
    Vector<OwnPtr<WebTransformOperations> > identityOperations;
    getIdentityOperations(&identityOperations);

    for (size_t i = 0; i < identityOperations.size(); ++i) {
        WebTransformOperations operations;
        operations.appendScale(3, 3, 3);

        double progress = 0.5;

        WebTransformationMatrix expected;
        expected.scale3d(2, 2, 2);

        EXPECT_TRANSFORMATION_MATRIX_EQ(expected, identityOperations[i]->blend(operations, progress));
    }
}

TEST(WebTransformOperationTest, blendSkewToIdentity)
{
    Vector<OwnPtr<WebTransformOperations> > identityOperations;
    getIdentityOperations(&identityOperations);

    for (size_t i = 0; i < identityOperations.size(); ++i) {
        WebTransformOperations operations;
        operations.appendSkew(2, 2);

        double progress = 0.5;

        WebTransformationMatrix expected;
        expected.skewX(1);
        expected.skewY(1);

        EXPECT_TRANSFORMATION_MATRIX_EQ(expected, identityOperations[i]->blend(operations, progress));
    }
}

TEST(WebTransformOperationTest, blendPerspectiveToIdentity)
{
    Vector<OwnPtr<WebTransformOperations> > identityOperations;
    getIdentityOperations(&identityOperations);

    for (size_t i = 0; i < identityOperations.size(); ++i) {
        WebTransformOperations operations;
        operations.appendPerspective(1000);

        double progress = 0.5;

        WebTransformationMatrix expected;
        expected.applyPerspective(500 + 0.5 * numeric_limits<double>::max());

        EXPECT_TRANSFORMATION_MATRIX_EQ(expected, identityOperations[i]->blend(operations, progress));
    }
}

