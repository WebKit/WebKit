/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"

#if PLATFORM(COCOA)

#include "Helpers/PlatformUtilities.h"
#include <WebCore/LocalSampleBufferDisplayLayer.h>
#include <WebCore/VideoFrame.h>

namespace TestWebKitAPI {

class MockSampleBufferDisplayLayerClient : public WebCore::SampleBufferDisplayLayerClient {
public:
    void sampleBufferDisplayLayerStatusDidFail() override { }
    void updateVideoFrameCounters(uint64_t, uint64_t) override { }
#if PLATFORM(IOS_FAMILY)
    bool canShowWhileLocked() const override { return false; }
#endif
};

// Verify that a rotation change arriving via updateSampleLayerBoundsAndPosition
// without explicit bounds does not immediately swap the stale root-layer bounds
// onto the sample buffer display layer, which would produce a transient
// wrong-orientation frame. The correct bounds must come from an explicit
// updateBoundsAndPosition call.
TEST(LocalSampleBufferDisplayLayer, RotationChangeShouldNotSwapStaleBounds)
{
    MockSampleBufferDisplayLayerClient client;
    auto layer = WebCore::LocalSampleBufferDisplayLayer::create(client);
    ASSERT_TRUE(layer);

    // Set landscape bounds via the explicit-bounds path.
    layer->updateBoundsAndPosition(CGRectMake(0, 0, 960, 720), std::nullopt);
    Util::runFor(0.1_s);

    EXPECT_EQ(layer->sampleLayerBoundsForTesting().size.width, 960);
    EXPECT_EQ(layer->sampleLayerBoundsForTesting().size.height, 720);

    // Simulate what enqueueVideoFrame does when it detects a rotation change to Left:
    // update the stored rotation then call with no explicit bounds. The transform should
    // be applied immediately but bounds must not be swapped using stale root layer geometry.
    layer->setVideoFrameRotationForTesting(WebCore::VideoFrameRotation::Left);
    layer->updateSampleLayerBoundsAndPosition(std::nullopt);
    Util::runFor(0.1_s);

    EXPECT_EQ(layer->sampleLayerBoundsForTesting().size.width, 960);
    EXPECT_EQ(layer->sampleLayerBoundsForTesting().size.height, 720);

    // The explicit-bounds path must apply transform and swap bounds correctly for Left rotation.
    layer->updateBoundsAndPosition(CGRectMake(0, 0, 960, 720), std::nullopt);
    Util::runFor(0.1_s);

    EXPECT_EQ(layer->sampleLayerBoundsForTesting().size.width, 720);
    EXPECT_EQ(layer->sampleLayerBoundsForTesting().size.height, 960);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(COCOA)
