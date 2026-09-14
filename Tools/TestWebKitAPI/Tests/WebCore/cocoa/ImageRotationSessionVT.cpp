/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#import "Helpers/Utilities.h"
#import <WebCore/CVUtilities.h>
#import <WebCore/ImageRotationSessionVT.h>
#import <WebCore/VideoFrameCV.h>

namespace TestWebKitAPI {

using namespace WebCore;

#if ENABLE(VIDEO) && USE(AVFOUNDATION)

TEST(ImageRotationSessionVT, ChangeOfRotationAngle)
{
    auto videoFrame = VideoFrameCV::create({ }, false, VideoFrame::Rotation::None, createBlackPixelBuffer(640, 480));
    auto session = makeUnique<ImageRotationSessionVT>();
    ImageRotationSessionVT::RotationProperties rotation;

    rotation.angle = 90;
    auto videoFrame90 = VideoFrameCV::create({ }, false, VideoFrame::Rotation::None, session->rotate(videoFrame, rotation, ImageRotationSessionVT::IsCGImageCompatible::No));
    EXPECT_EQ(480, videoFrame90->presentationSize().width());
    EXPECT_EQ(640, videoFrame90->presentationSize().height());

    rotation.angle = 180;
    auto videoFrame180 = VideoFrameCV::create({ }, false, VideoFrame::Rotation::None, session->rotate(videoFrame, rotation, ImageRotationSessionVT::IsCGImageCompatible::No));
    EXPECT_EQ(640, videoFrame180->presentationSize().width());
    EXPECT_EQ(480, videoFrame180->presentationSize().height());
}

TEST(CVUtilities, CreateBlackPixelBuffer)
{
    auto pixelBuffer = createBlackPixelBuffer(64, 64);
    ASSERT_TRUE(pixelBuffer);

    OSType format = CVPixelBufferGetPixelFormatType(pixelBuffer.get());
    ASSERT_TRUE(format == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange || format == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange);

    // Full-range black is Y=0; video-range black is the luma floor Y=16. Chroma is neutral (128) for both.
    uint8_t expectedLuma = format == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange ? 16 : 0;

    ASSERT_EQ(kCVReturnSuccess, CVPixelBufferLockBaseAddress(pixelBuffer.get(), kCVPixelBufferLock_ReadOnly));
    auto* yPlane = static_cast<const uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer.get(), 0));
    auto* uvPlane = static_cast<const uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer.get(), 1));
    EXPECT_EQ(expectedLuma, yPlane[0]);
    EXPECT_EQ(128, uvPlane[0]);
    EXPECT_EQ(128, uvPlane[1]);
    CVPixelBufferUnlockBaseAddress(pixelBuffer.get(), kCVPixelBufferLock_ReadOnly);
}

#endif

}; // namespace TestWebKitAPI
