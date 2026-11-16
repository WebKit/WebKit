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
#import <CoreVideo/CoreVideo.h>
#import <WebCore/CVUtilities.h>
#import <WebCore/ImageRotationSessionVT.h>
#import <WebCore/VideoFrameCV.h>

namespace TestWebKitAPI {

using namespace WebCore;

#if ENABLE(VIDEO) && USE(AVFOUNDATION)

// Marks the top-left quadrant white so any flip is observable.
static void markTopLeftQuadrant(CVPixelBufferRef pixelBuffer)
{
    CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    auto width = CVPixelBufferGetWidth(pixelBuffer);
    auto height = CVPixelBufferGetHeight(pixelBuffer);
    auto stride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
    auto yPlane = unsafeMakeSpan(static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0)), stride * height);
    for (size_t row = 0; row < height / 2; ++row)
        memsetSpan(yPlane.subspan(row * stride, width / 2), 255);
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
}

// Average luma over a small window centered at the normalized (fx, fy) location.
static unsigned sampleLuma(CVPixelBufferRef pixelBuffer, float fx, float fy)
{
    CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    auto width = CVPixelBufferGetWidth(pixelBuffer);
    auto height = CVPixelBufferGetHeight(pixelBuffer);
    auto stride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
    auto* yPlane = static_cast<const uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0));

    auto centerX = static_cast<size_t>(fx * width);
    auto centerY = static_cast<size_t>(fy * height);
    constexpr size_t window = 8;
    unsigned total = 0;
    unsigned count = 0;
    for (size_t y = centerY; y < centerY + window && y < height; ++y) {
        for (size_t x = centerX; x < centerX + window && x < width; ++x) {
            total += yPlane[y * stride + x];
            ++count;
        }
    }
    CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    return count ? total / count : 0;
}

TEST(ImageRotationSessionVT, MirroredFrameFlipsHorizontally)
{
    auto pixelBuffer = createBlackPixelBuffer(640, 480);
    markTopLeftQuadrant(pixelBuffer.get());

    auto videoFrame = VideoFrameCV::create({ }, true, VideoFrame::Rotation::None, WTF::move(pixelBuffer));
    auto session = makeUnique<ImageRotationSessionVT>();
    auto rotated = session->applyRotation(videoFrame.get(), ImageRotationSessionVT::IsCGImageCompatible::No);
    ASSERT_NE(rotated, nullptr);

    RetainPtr result = rotated->pixelBuffer();
    ASSERT_NE(result, nullptr);

    EXPECT_GT(sampleLuma(result.get(), 0.75f, 0.25f), 200u); // top-right
    EXPECT_LT(sampleLuma(result.get(), 0.25f, 0.25f), 50u); // top-left
    EXPECT_LT(sampleLuma(result.get(), 0.25f, 0.75f), 50u); // bottom-left
    EXPECT_LT(sampleLuma(result.get(), 0.75f, 0.75f), 50u); // bottom-right
}

TEST(ImageRotationSessionVT, UnmirroredFrameIsUnchanged)
{
    auto pixelBuffer = createBlackPixelBuffer(640, 480);
    markTopLeftQuadrant(pixelBuffer.get());

    auto videoFrame = VideoFrameCV::create({ }, false, VideoFrame::Rotation::None, WTF::move(pixelBuffer));
    auto session = makeUnique<ImageRotationSessionVT>();
    auto rotated = session->applyRotation(videoFrame.get(), ImageRotationSessionVT::IsCGImageCompatible::No);
    ASSERT_NE(rotated, nullptr);

    RetainPtr result = rotated->pixelBuffer();
    ASSERT_NE(result, nullptr);

    EXPECT_GT(sampleLuma(result.get(), 0.25f, 0.25f), 200u); // top-left
    EXPECT_LT(sampleLuma(result.get(), 0.75f, 0.25f), 50u); // top-right
}

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
