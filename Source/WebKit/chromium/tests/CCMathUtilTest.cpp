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

#include "CCMathUtil.h"

#include "CCLayerTreeTestCommon.h"
#include "FloatRect.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <public/WebTransformationMatrix.h>

using namespace WebCore;
using WebKit::WebTransformationMatrix;

namespace {

TEST(CCMathUtilTest, verifyBackfaceVisibilityBasicCases)
{
    WebTransformationMatrix transform;

    transform.makeIdentity();
    EXPECT_FALSE(transform.isBackFaceVisible());

    transform.makeIdentity();
    transform.rotate3d(0, 80, 0);
    EXPECT_FALSE(transform.isBackFaceVisible());

    transform.makeIdentity();
    transform.rotate3d(0, 100, 0);
    EXPECT_TRUE(transform.isBackFaceVisible());

    // Edge case, 90 degree rotation should return false.
    transform.makeIdentity();
    transform.rotate3d(0, 90, 0);
    EXPECT_FALSE(transform.isBackFaceVisible());
}

TEST(CCMathUtilTest, verifyBackfaceVisibilityForPerspective)
{
    WebTransformationMatrix layerSpaceToProjectionPlane;

    // This tests if isBackFaceVisible works properly under perspective transforms.
    // Specifically, layers that may have their back face visible in orthographic
    // projection, may not actually have back face visible under perspective projection.

    // Case 1: Layer is rotated by slightly more than 90 degrees, at the center of the
    //         prespective projection. In this case, the layer's back-side is visible to
    //         the camera.
    layerSpaceToProjectionPlane.makeIdentity();
    layerSpaceToProjectionPlane.applyPerspective(1);
    layerSpaceToProjectionPlane.translate3d(0, 0, 0);
    layerSpaceToProjectionPlane.rotate3d(0, 100, 0);
    EXPECT_TRUE(layerSpaceToProjectionPlane.isBackFaceVisible());

    // Case 2: Layer is rotated by slightly more than 90 degrees, but shifted off to the
    //         side of the camera. Because of the wide field-of-view, the layer's front
    //         side is still visible.
    //
    //                       |<-- front side of layer is visible to perspective camera
    //                    \  |            /
    //                     \ |           /
    //                      \|          /
    //                       |         /
    //                       |\       /<-- camera field of view
    //                       | \     /
    // back side of layer -->|  \   /
    //                           \./ <-- camera origin
    //
    layerSpaceToProjectionPlane.makeIdentity();
    layerSpaceToProjectionPlane.applyPerspective(1);
    layerSpaceToProjectionPlane.translate3d(-10, 0, 0);
    layerSpaceToProjectionPlane.rotate3d(0, 100, 0);
    EXPECT_FALSE(layerSpaceToProjectionPlane.isBackFaceVisible());

    // Case 3: Additionally rotating the layer by 180 degrees should of course show the
    //         opposite result of case 2.
    layerSpaceToProjectionPlane.rotate3d(0, 180, 0);
    EXPECT_TRUE(layerSpaceToProjectionPlane.isBackFaceVisible());
}

TEST(CCMathUtilTest, verifyProjectionOfPerpendicularPlane)
{
    // In this case, the m33() element of the transform becomes zero, which could cause a
    // divide-by-zero when projecting points/quads.

    WebTransformationMatrix transform;
    transform.makeIdentity();
    transform.setM33(0);

    FloatRect rect = FloatRect(0, 0, 1, 1);
    FloatRect projectedRect = CCMathUtil::projectClippedRect(transform, rect);

    EXPECT_EQ(0, projectedRect.x());
    EXPECT_EQ(0, projectedRect.y());
    EXPECT_TRUE(projectedRect.isEmpty());
}

TEST(CCMathUtilTest, verifyEnclosingClippedRectUsesCorrectInitialBounds)
{
    HomogeneousCoordinate h1(-100, -100, 0, 1);
    HomogeneousCoordinate h2(-10, -10, 0, 1);
    HomogeneousCoordinate h3(10, 10, 0, -1);
    HomogeneousCoordinate h4(100, 100, 0, -1);

    // The bounds of the enclosing clipped rect should be -100 to -10 for both x and y.
    // However, if there is a bug where the initial xmin/xmax/ymin/ymax are initialized to
    // numeric_limits<float>::min() (which is zero, not -flt_max) then the enclosing
    // clipped rect will be computed incorrectly.
    FloatRect result = CCMathUtil::computeEnclosingClippedRect(h1, h2, h3, h4);

    EXPECT_FLOAT_RECT_EQ(FloatRect(FloatPoint(-100, -100), FloatSize(90, 90)), result);
}

TEST(CCMathUtilTest, verifyEnclosingRectOfVerticesUsesCorrectInitialBounds)
{
    FloatPoint vertices[3];
    int numVertices = 3;

    vertices[0] = FloatPoint(-10, -100);
    vertices[1] = FloatPoint(-100, -10);
    vertices[2] = FloatPoint(-30, -30);

    // The bounds of the enclosing rect should be -100 to -10 for both x and y. However,
    // if there is a bug where the initial xmin/xmax/ymin/ymax are initialized to
    // numeric_limits<float>::min() (which is zero, not -flt_max) then the enclosing
    // clipped rect will be computed incorrectly.
    FloatRect result = CCMathUtil::computeEnclosingRectOfVertices(vertices, numVertices);

    EXPECT_FLOAT_RECT_EQ(FloatRect(FloatPoint(-100, -100), FloatSize(90, 90)), result);
}

// http://webkit.org/b/94502
TEST(CCMathUtilTest, DISABLED_smallestAngleBetweenVectors)
{
    FloatSize x(1, 0);
    FloatSize y(0, 1);
    FloatSize testVector(0.5, 0.5);

    // Orthogonal vectors are at an angle of 90 degress.
    EXPECT_EQ(90, CCMathUtil::smallestAngleBetweenVectors(x, y));

    // A vector makes a zero angle with itself.
    EXPECT_EQ(0, CCMathUtil::smallestAngleBetweenVectors(x, x));
    EXPECT_EQ(0, CCMathUtil::smallestAngleBetweenVectors(y, y));
    EXPECT_EQ(0, CCMathUtil::smallestAngleBetweenVectors(testVector, testVector));

    // Parallel but reversed vectors are at 180 degrees.
    EXPECT_EQ(180, CCMathUtil::smallestAngleBetweenVectors(x, -x));
    EXPECT_EQ(180, CCMathUtil::smallestAngleBetweenVectors(y, -y));
    EXPECT_EQ(180, CCMathUtil::smallestAngleBetweenVectors(testVector, -testVector));

    // The test vector is at a known angle.
    EXPECT_EQ(45, floor(CCMathUtil::smallestAngleBetweenVectors(testVector, x)));
    EXPECT_EQ(45, floor(CCMathUtil::smallestAngleBetweenVectors(testVector, y)));
}

TEST(CCMathUtilTest, vectorProjection)
{
    FloatSize x(1, 0);
    FloatSize y(0, 1);
    FloatSize testVector(0.3, 0.7);

    // Orthogonal vectors project to a zero vector.
    EXPECT_EQ(FloatSize(0, 0), CCMathUtil::projectVector(x, y));
    EXPECT_EQ(FloatSize(0, 0), CCMathUtil::projectVector(y, x));

    // Projecting a vector onto the orthonormal basis gives the corresponding component of the
    // vector.
    EXPECT_EQ(FloatSize(testVector.width(), 0), CCMathUtil::projectVector(testVector, x));
    EXPECT_EQ(FloatSize(0, testVector.height()), CCMathUtil::projectVector(testVector, y));

    // Finally check than an arbitrary vector projected to another one gives a vector parallel to
    // the second vector.
    FloatSize targetVector(0.5, 0.2);
    FloatSize projectedVector = CCMathUtil::projectVector(testVector, targetVector);
    EXPECT_EQ(projectedVector.width() / targetVector.width(),
              projectedVector.height() / targetVector.height());
}

} // namespace
