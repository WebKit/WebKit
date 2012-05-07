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

#include "cc/CCMathUtil.h"

#include "TransformationMatrix.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

TEST(CCMathUtilTest, verifyBackfaceVisibilityBasicCases)
{
    TransformationMatrix transform;

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
    TransformationMatrix layerSpaceToProjectionPlane;

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

    TransformationMatrix transform;
    transform.makeIdentity();
    transform.setM33(0);

    FloatRect rect = FloatRect(0, 0, 1, 1);
    FloatRect projectedRect = CCMathUtil::projectClippedRect(transform, rect);

    EXPECT_EQ(0, projectedRect.x());
    EXPECT_EQ(0, projectedRect.y());
    EXPECT_TRUE(projectedRect.isEmpty());
}

} // namespace
