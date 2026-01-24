/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"

#if ENABLE(SPATIAL_IMAGE_DETECTION)

#import "Test.h"
#import "TestNSBundleExtras.h"
#import <WebCore/BitmapImage.h>
#import <WebCore/ShareableSpatialImage.h>
#import <WebCore/SharedBuffer.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

using namespace WebCore;

TEST(ShareableSpatialImageTests, SpatialImageRoundTrip)
{
    RetainPtr fileData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"spatial" withExtension:@"heic"]];
    ASSERT_TRUE(fileData);

    RefPtr image = BitmapImage::create();
    image->setData(SharedBuffer::create(fileData.get()), true);
    ASSERT_TRUE(image->isSpatial());
    ASSERT_EQ(image->frameCount(), 2u);

    auto spatialImage = WebCore::ShareableSpatialImage::create(*image);
    EXPECT_TRUE(spatialImage);

    auto heicData = spatialImage->createHEICData();
    EXPECT_TRUE(heicData);

    auto heicBuffer = SharedBuffer::create(heicData.get());
    RefPtr heicImage = BitmapImage::create();
    heicImage->setData(heicBuffer.get(), true);

    EXPECT_TRUE(heicImage->isSpatial());
    EXPECT_EQ(heicImage->frameCount(), 2u);

    auto leftIndex = heicImage->spatialLeftEyeFrameIndex();
    EXPECT_EQ(*leftIndex, 0u);
    auto rightIndex = heicImage->spatialRightEyeFrameIndex();
    EXPECT_EQ(*rightIndex, 1u);

    auto leftEye = heicImage->nativeImageAtIndex(*leftIndex);
    EXPECT_TRUE(leftEye);
    auto rightEye = heicImage->nativeImageAtIndex(*rightIndex);
    EXPECT_TRUE(rightEye);

    auto leftMetadata = heicImage->spatialEyePropertiesAtIndex(*leftIndex);
    EXPECT_TRUE(leftMetadata);
    auto rightMetadata = heicImage->spatialEyePropertiesAtIndex(*rightIndex);
    EXPECT_TRUE(rightMetadata);

    // Verify expected values from spatial.heic are preserved through round-trip.
    EXPECT_EQ(leftMetadata->groupMetadata.groupIndex, 0u);
    EXPECT_FLOAT_EQ(leftMetadata->groupMetadata.disparityAdjustment, 200.0f);
    EXPECT_EQ(rightMetadata->groupMetadata.groupIndex, 0u);
    EXPECT_FLOAT_EQ(rightMetadata->groupMetadata.disparityAdjustment, 200.0f);

    std::array<float, 3> expectedLeftPosition = { 0.0f, 0.0f, 0.0f };
    std::array<float, 3> expectedRightPosition = { 0.064f, 0.0f, 0.0f };
    std::array<float, 9> expectedRotation = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };
    EXPECT_EQ(leftMetadata->cameraMetadata.extrinsics.position, expectedLeftPosition);
    EXPECT_EQ(leftMetadata->cameraMetadata.extrinsics.rotation, expectedRotation);
    EXPECT_EQ(rightMetadata->cameraMetadata.extrinsics.position, expectedRightPosition);
    EXPECT_EQ(rightMetadata->cameraMetadata.extrinsics.rotation, expectedRotation);

    std::array<float, 9> expectedIntrinsics = { 14.3010435f, 0.0f, 12.0f, 0.0f, 14.3010435f, 12.0f, 0.0f, 0.0f, 1.0f };
    EXPECT_EQ(leftMetadata->cameraMetadata.intrinsics.matrix, expectedIntrinsics);
    EXPECT_EQ(rightMetadata->cameraMetadata.intrinsics.matrix, expectedIntrinsics);
}

} // namespace TestWebKitAPI

#endif
