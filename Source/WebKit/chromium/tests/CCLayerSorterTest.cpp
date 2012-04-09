/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "cc/CCLayerSorter.h"

#include "cc/CCLayerImpl.h"
#include "cc/CCMathUtil.h"
#include "cc/CCSingleThreadProxy.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

// Note: In the following overlap tests, the "camera" is looking down the negative Z axis,
// meaning that layers with smaller z values (more negative) are further from the camera
// and therefore must be drawn before layers with higher z values.

TEST(CCLayerSorterTest, BasicOverlap)
{
    CCLayerSorter::ABCompareResult overlapResult;
    const float zThreshold = 0.1f;
    float weight = 0;

    // Trivial test, with one layer directly obscuring the other.
    TransformationMatrix neg4Translate;
    neg4Translate.translate3d(0, 0, -4);
    CCLayerSorter::LayerShape front(2, 2, neg4Translate);

    TransformationMatrix neg5Translate;
    neg5Translate.translate3d(0, 0, -5);
    CCLayerSorter::LayerShape back(2, 2, neg5Translate);

    overlapResult = CCLayerSorter::checkOverlap(&front, &back, zThreshold, weight);
    EXPECT_EQ(CCLayerSorter::BBeforeA, overlapResult);
    EXPECT_EQ(1, weight);

    overlapResult = CCLayerSorter::checkOverlap(&back, &front, zThreshold, weight);
    EXPECT_EQ(CCLayerSorter::ABeforeB, overlapResult);
    EXPECT_EQ(1, weight);

    // One layer translated off to the right. No overlap should be detected.
    TransformationMatrix rightTranslate;
    rightTranslate.translate3d(10, 0, -5);
    CCLayerSorter::LayerShape backRight(2, 2, rightTranslate);
    overlapResult = CCLayerSorter::checkOverlap(&front, &backRight, zThreshold, weight);
    EXPECT_EQ(CCLayerSorter::None, overlapResult);

    // When comparing a layer with itself, z difference is always 0.
    overlapResult = CCLayerSorter::checkOverlap(&front, &front, zThreshold, weight);
    EXPECT_EQ(0, weight);
}

TEST(CCLayerSorterTest, RightAngleOverlap)
{
    CCLayerSorter::ABCompareResult overlapResult;
    const float zThreshold = 0.1f;
    float weight = 0;

    TransformationMatrix perspectiveMatrix;
    perspectiveMatrix.applyPerspective(1000);

    // Two layers forming a right angle with a perspective viewing transform.
    TransformationMatrix leftFaceMatrix;
    leftFaceMatrix.rotate3d(0, 1, 0, -90).translateRight3d(-1, 0, -5);
    CCLayerSorter::LayerShape leftFace(2, 2, perspectiveMatrix * leftFaceMatrix);
    TransformationMatrix frontFaceMatrix;
    frontFaceMatrix.translate3d(0, 0, -4);
    CCLayerSorter::LayerShape frontFace(2, 2, perspectiveMatrix * frontFaceMatrix);

    overlapResult = CCLayerSorter::checkOverlap(&frontFace, &leftFace, zThreshold, weight);
    EXPECT_EQ(CCLayerSorter::BBeforeA, overlapResult);
}

TEST(CCLayerSorterTest, IntersectingLayerOverlap)
{
    CCLayerSorter::ABCompareResult overlapResult;
    const float zThreshold = 0.1f;
    float weight = 0;

    TransformationMatrix perspectiveMatrix;
    perspectiveMatrix.applyPerspective(1000);

    // Intersecting layers. An explicit order will be returned based on relative z
    // values at the overlapping features but the weight returned should be zero.
    TransformationMatrix frontFaceMatrix;
    frontFaceMatrix.translate3d(0, 0, -4);
    CCLayerSorter::LayerShape frontFace(2, 2, perspectiveMatrix * frontFaceMatrix);

    TransformationMatrix throughMatrix;
    throughMatrix.rotate3d(0, 1, 0, 45).translateRight3d(0, 0, -4);
    CCLayerSorter::LayerShape rotatedFace(2, 2, perspectiveMatrix * throughMatrix);
    overlapResult = CCLayerSorter::checkOverlap(&frontFace, &rotatedFace, zThreshold, weight);
    EXPECT_NE(CCLayerSorter::None, overlapResult);
    EXPECT_EQ(0, weight);
}

TEST(CCLayerSorterTest, LayersAtAngleOverlap)
{
    CCLayerSorter::ABCompareResult overlapResult;
    const float zThreshold = 0.1f;
    float weight = 0;

    // Trickier test with layers at an angle.
    //
    //   -x . . . . 0 . . . . +x
    // -z             /
    //  :            /----B----
    //  0           C
    //  : ----A----/
    // +z         /
    //
    // C is in front of A and behind B (not what you'd expect by comparing centers).
    // A and B don't overlap, so they're incomparable.

    TransformationMatrix transformA;
    transformA.translate3d(-6, 0, 1);
    CCLayerSorter::LayerShape layerA(8, 20, transformA);

    TransformationMatrix transformB;
    transformB.translate3d(6, 0, -1);
    CCLayerSorter::LayerShape layerB(8, 20, transformB);

    TransformationMatrix transformC;
    transformC.rotate3d(0, 1, 0, 40);
    CCLayerSorter::LayerShape layerC(8, 20, transformC);

    overlapResult = CCLayerSorter::checkOverlap(&layerA, &layerC, zThreshold, weight);
    EXPECT_EQ(CCLayerSorter::ABeforeB, overlapResult);
    overlapResult = CCLayerSorter::checkOverlap(&layerC, &layerB, zThreshold, weight);
    EXPECT_EQ(CCLayerSorter::ABeforeB, overlapResult);
    overlapResult = CCLayerSorter::checkOverlap(&layerA, &layerB, zThreshold, weight);
    EXPECT_EQ(CCLayerSorter::None, overlapResult);
}

TEST(CCLayerSorterTest, LayersUnderPathologicalPerspectiveTransform)
{
    CCLayerSorter::ABCompareResult overlapResult;
    const float zThreshold = 0.1f;
    float weight = 0;

    // On perspective projection, if w becomes negative, the re-projected point will be
    // invalid and un-usable. Correct code needs to clip away portions of the geometry
    // where w < 0. If the code uses the invalid value, it will think that a layer has
    // different bounds than it really does, which can cause things to sort incorrectly.

    TransformationMatrix perspectiveMatrix;
    perspectiveMatrix.applyPerspective(1);

    TransformationMatrix transformA;
    transformA.translate3d(-15, 0, -2);
    CCLayerSorter::LayerShape layerA(10, 10, perspectiveMatrix * transformA);

    // With this sequence of transforms, when layer B is correctly clipped, it will be
    // visible on the left half of the projection plane, in front of layerA. When it is
    // not clipped, its bounds will actually incorrectly appear much smaller and the
    // correct sorting dependency will not be found.
    TransformationMatrix transformB;
    transformB.translate3d(0, 0, 0.7);
    transformB.rotate3d(0, 45, 0);
    CCLayerSorter::LayerShape layerB(10, 10, perspectiveMatrix * transformB);

    // Sanity check that the test case actually covers the intended scenario, where part
    // of layer B go behind the w = 0 plane.
    FloatQuad testQuad = FloatQuad(FloatRect(FloatPoint(-0.5, -0.5), FloatSize(1, 1)));
    bool clipped = false;
    CCMathUtil::mapQuad(perspectiveMatrix * transformB, testQuad, clipped);
    ASSERT_TRUE(clipped);

    overlapResult = CCLayerSorter::checkOverlap(&layerA, &layerB, zThreshold, weight);
    EXPECT_EQ(CCLayerSorter::ABeforeB, overlapResult);
}

TEST(CCLayerSorterTest, verifyExistingOrderingPreservedWhenNoZDiff)
{
    DebugScopedSetImplThread thisScopeIsOnImplThread;

    // If there is no reason to re-sort the layers (i.e. no 3d z difference), then the
    // existing ordering provided on input should be retained. This test covers the fix in
    // https://bugs.webkit.org/show_bug.cgi?id=75046. Before this fix, ordering was
    // accidentally reversed, causing bugs in z-index ordering on websites when
    // preserves3D triggered the CCLayerSorter.

    // Input list of layers: [1, 2, 3, 4, 5].
    // Expected output: [3, 4, 1, 2, 5].
    //    - 1, 2, and 5 do not have a 3d z difference, and therefore their relative ordering should be retained.
    //    - 3 and 4 do not have a 3d z difference, and therefore their relative ordering should be retained.
    //    - 3 and 4 should be re-sorted so they are in front of 1, 2, and 5.

    OwnPtr<CCLayerImpl> layer1 = CCLayerImpl::create(1);
    OwnPtr<CCLayerImpl> layer2 = CCLayerImpl::create(2);
    OwnPtr<CCLayerImpl> layer3 = CCLayerImpl::create(3);
    OwnPtr<CCLayerImpl> layer4 = CCLayerImpl::create(4);
    OwnPtr<CCLayerImpl> layer5 = CCLayerImpl::create(5);

    TransformationMatrix BehindMatrix;
    BehindMatrix.translate3d(0, 0, 2);
    TransformationMatrix FrontMatrix;
    FrontMatrix.translate3d(0, 0, 1);

    layer1->setBounds(IntSize(10, 10));
    layer1->setDrawTransform(BehindMatrix);
    layer1->setDrawsContent(true);

    layer2->setBounds(IntSize(20, 20));
    layer2->setDrawTransform(BehindMatrix);
    layer2->setDrawsContent(true);

    layer3->setBounds(IntSize(30, 30));
    layer3->setDrawTransform(FrontMatrix);
    layer3->setDrawsContent(true);

    layer4->setBounds(IntSize(40, 40));
    layer4->setDrawTransform(FrontMatrix);
    layer4->setDrawsContent(true);

    layer5->setBounds(IntSize(50, 50));
    layer5->setDrawTransform(BehindMatrix);
    layer5->setDrawsContent(true);

    Vector<CCLayerImpl*> layerList;
    layerList.append(layer1.get());
    layerList.append(layer2.get());
    layerList.append(layer3.get());
    layerList.append(layer4.get());
    layerList.append(layer5.get());

    ASSERT_EQ(static_cast<size_t>(5), layerList.size());
    EXPECT_EQ(1, layerList[0]->id());
    EXPECT_EQ(2, layerList[1]->id());
    EXPECT_EQ(3, layerList[2]->id());
    EXPECT_EQ(4, layerList[3]->id());
    EXPECT_EQ(5, layerList[4]->id());

    CCLayerSorter layerSorter;
    layerSorter.sort(layerList.begin(), layerList.end());

    ASSERT_EQ(static_cast<size_t>(5), layerList.size());
    EXPECT_EQ(3, layerList[0]->id());
    EXPECT_EQ(4, layerList[1]->id());
    EXPECT_EQ(1, layerList[2]->id());
    EXPECT_EQ(2, layerList[3]->id());
    EXPECT_EQ(5, layerList[4]->id());
}

} // namespace
