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

#if USE(AVFOUNDATION)

#include "Helpers/PlatformUtilities.h"
#include <CoreAudio/CoreAudioTypes.h>
#include <WebCore/AudioVideoRendererAVFObjC.h>
#include <WebCore/MediaPlayerEnums.h>
#include <WebCore/MediaSampleAVFObjC.h>
#include <WebCore/TrackInfo.h>
#include <wtf/Logger.h>
#include <wtf/RunLoop.h>

#include <pal/cf/CoreMediaSoftLink.h>

using namespace WebCore;

namespace TestWebKitAPI {

static RetainPtr<CMSampleBufferRef> createVideoSampleBuffer()
{
    CVPixelBufferRef rawPixelBuffer = nullptr;
    if (CVPixelBufferCreate(kCFAllocatorDefault, 16, 16, kCVPixelFormatType_32BGRA, nullptr, &rawPixelBuffer))
        return nullptr;
    RetainPtr pixelBuffer = adoptCF(rawPixelBuffer);

    CMVideoFormatDescriptionRef rawFormatDescription = nullptr;
    if (PAL::CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, pixelBuffer.get(), &rawFormatDescription))
        return nullptr;
    RetainPtr formatDescription = adoptCF(rawFormatDescription);

    CMSampleTimingInfo timing = { PAL::kCMTimeInvalid, PAL::kCMTimeZero, PAL::kCMTimeInvalid };
    CMSampleBufferRef rawSampleBuffer = nullptr;
    if (PAL::CMSampleBufferCreateForImageBuffer(kCFAllocatorDefault, pixelBuffer.get(), true, nullptr, nullptr, formatDescription.get(), &timing, &rawSampleBuffer))
        return nullptr;

    return adoptCF(rawSampleBuffer);
}

static RetainPtr<CMSampleBufferRef> createAudioSampleBuffer()
{
    constexpr int32_t sampleRate = 44100;
    constexpr size_t frameCount = 128;
    constexpr size_t dataLength = frameCount * sizeof(float);
    // Backs a non-owning block buffer, so it has to outlive every sample buffer handed out here.
    static std::array<float, frameCount> silence { };

    AudioStreamBasicDescription streamDescription { };
    streamDescription.mSampleRate = sampleRate;
    streamDescription.mFormatID = kAudioFormatLinearPCM;
    streamDescription.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    streamDescription.mChannelsPerFrame = 1;
    streamDescription.mFramesPerPacket = 1;
    streamDescription.mBytesPerFrame = sizeof(float);
    streamDescription.mBytesPerPacket = sizeof(float);
    streamDescription.mBitsPerChannel = 8 * sizeof(float);

    CMAudioFormatDescriptionRef rawFormatDescription = nullptr;
    if (PAL::CMAudioFormatDescriptionCreate(kCFAllocatorDefault, &streamDescription, 0, nullptr, 0, nullptr, nullptr, &rawFormatDescription))
        return nullptr;
    RetainPtr formatDescription = adoptCF(rawFormatDescription);

    CMBlockBufferRef rawBlockBuffer = nullptr;
    if (PAL::CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, silence.data(), dataLength, kCFAllocatorNull, nullptr, 0, dataLength, 0, &rawBlockBuffer))
        return nullptr;
    RetainPtr blockBuffer = adoptCF(rawBlockBuffer);

    // The format description is constant bitrate, so no per-sample size array is needed.
    CMSampleTimingInfo timing = { PAL::CMTimeMake(1, sampleRate), PAL::kCMTimeZero, PAL::kCMTimeInvalid };
    CMSampleBufferRef rawSampleBuffer = nullptr;
    if (PAL::CMSampleBufferCreateReady(kCFAllocatorDefault, blockBuffer.get(), formatDescription.get(), frameCount, 1, &timing, 0, nullptr, &rawSampleBuffer))
        return nullptr;

    return adoptCF(rawSampleBuffer);
}

class AudioVideoRendererAVFObjCTest : public testing::Test {
public:
    void SetUp() final
    {
        Ref logger = Logger::create(this);
        renderer = AudioVideoRendererAVFObjC::create(logger, 0);
        renderer->setPreferences({ });
        renderer->notifyWhenRequiresFlushToResume([this] {
            ++flushToResumeCount;
        });
        renderer->notifyWhenErrorOccurs([this](PlatformMediaError error) {
            ++errorCount;
        });
        videoTrackId = renderer->addTrack(TrackInfo::TrackType::Video);
    }

    void TearDown() final
    {
        renderer = nullptr;
    }

    void enqueueVideoSample()
    {
        RetainPtr sampleBuffer = createVideoSampleBuffer();
        ASSERT_TRUE(sampleBuffer);
        renderer->enqueueSample(*videoTrackId, MediaSampleAVFObjC::create(sampleBuffer.get(), 0), { });
    }

    RefPtr<AudioVideoRenderer> renderer;
    std::optional<AudioVideoRenderer::TrackIdentifier> videoTrackId;
    int flushToResumeCount { 0 };
    int errorCount { 0 };
};

TEST_F(AudioVideoRendererAVFObjCTest, NoFlushWithoutEnqueuedSamples)
{
    renderer->renderingCanBeAcceleratedChanged(true);
    Util::runFor(0.1_s);
    EXPECT_EQ(flushToResumeCount, 0);

    renderer->renderingCanBeAcceleratedChanged(false);
    Util::runFor(0.1_s);
    EXPECT_EQ(flushToResumeCount, 0);

    renderer->renderingCanBeAcceleratedChanged(true);
    Util::runFor(0.1_s);
    EXPECT_EQ(flushToResumeCount, 0);
}

TEST_F(AudioVideoRendererAVFObjCTest, NoFlushOnRendererDestroy)
{
    ASSERT_TRUE(videoTrackId.has_value());

    renderer->renderingCanBeAcceleratedChanged(true);
    Util::runFor(0.1_s);
    EXPECT_EQ(flushToResumeCount, 0);

    enqueueVideoSample();

    renderer->renderingCanBeAcceleratedChanged(false);
    Util::runFor(0.1_s);
    EXPECT_EQ(flushToResumeCount, 0);
    EXPECT_EQ(errorCount, 0);
}

TEST_F(AudioVideoRendererAVFObjCTest, NoFlushOnProtectedContentVisibilityCycle)
{
    ASSERT_TRUE(videoTrackId.has_value());

    renderer->setHasProtectedVideoContent(true);
    renderer->renderingCanBeAcceleratedChanged(true);
    Util::runFor(0.1_s);
    EXPECT_EQ(flushToResumeCount, 0);

    enqueueVideoSample();

    renderer->renderingCanBeAcceleratedChanged(false);
    Util::runFor(0.1_s);
    EXPECT_EQ(flushToResumeCount, 0);

    renderer->renderingCanBeAcceleratedChanged(true);
    Util::runFor(0.1_s);
    EXPECT_EQ(flushToResumeCount, 0);
    EXPECT_EQ(errorCount, 0);
}

TEST_F(AudioVideoRendererAVFObjCTest, NoFlushWithDecompressionSessionForProtectedContent)
{
    ASSERT_TRUE(videoTrackId.has_value());

    renderer->setPreferences({ VideoRendererPreference::PrefersDecompressionSession, VideoRendererPreference::UseDecompressionSessionForProtectedContent });
    renderer->setHasProtectedVideoContent(true);
    renderer->renderingCanBeAcceleratedChanged(true);
    Util::runFor(0.1_s);
    EXPECT_EQ(flushToResumeCount, 0);

    enqueueVideoSample();

    renderer->renderingCanBeAcceleratedChanged(false);
    Util::runFor(0.1_s);
    EXPECT_EQ(flushToResumeCount, 0);

    renderer->renderingCanBeAcceleratedChanged(true);
    Util::runFor(0.1_s);
    EXPECT_EQ(flushToResumeCount, 0);
    EXPECT_EQ(errorCount, 0);
}

// prepareToSeek's fast path is taken when the destination is within 1ms of the synchronizer's
// current time: the synchronizer isn't moved and no time jumped notification is awaited. These
// tests use an audio-only renderer because that is the only way to make
// allRenderersHaveAvailableSamples() — which gates the fast path — true synchronously; a video
// track would have to wait on a first decoded frame.
class AudioVideoRendererAVFObjCSeekTest : public testing::Test {
public:
    void SetUp() final
    {
        Ref logger = Logger::create(this);
        renderer = AudioVideoRendererAVFObjC::create(logger, 0);
        renderer->setPreferences({ });
        audioTrackId = renderer->addTrack(TrackInfo::TrackType::Audio);
    }

    void TearDown() final
    {
        renderer = nullptr;
    }

    void enqueueAudioSample()
    {
        RetainPtr sampleBuffer = createAudioSampleBuffer();
        ASSERT_TRUE(sampleBuffer);
        renderer->enqueueSample(*audioTrackId, MediaSampleAVFObjC::create(sampleBuffer.get(), 0), { });
    }

    std::optional<MediaTime> seekAndWaitForReportedTime(const MediaTime& seekTime)
    {
        std::optional<MediaTime> reportedTime;
        bool done = false;
        renderer->prepareToSeek(seekTime)->whenSettled(RunLoop::currentSingleton(), [&](MediaTimePromise::Result result) {
            if (result)
                reportedTime = *result;
            done = true;
        });
        Util::run(&done);
        return reportedTime;
    }

    RefPtr<AudioVideoRenderer> renderer;
    std::optional<AudioVideoRenderer::TrackIdentifier> audioTrackId;
};

TEST_F(AudioVideoRendererAVFObjCSeekTest, ShortForwardSeekReportsRequestedTime)
{
    ASSERT_TRUE(audioTrackId.has_value());
    enqueueAudioSample();

    // 1ms past the synchronizer's time, which is the furthest the fast path reaches.
    auto seekTime = MediaTime(1, 1000);
    auto reportedTime = seekAndWaitForReportedTime(seekTime);

    // A completed seek with a finite time means the fast path ran; the slow path leaves the
    // renderer seeking and resolves with an indefinite time.
    ASSERT_FALSE(renderer->seeking());
    ASSERT_TRUE(reportedTime.has_value());
    ASSERT_TRUE(reportedTime->isFinite());

    // The requested time, not the synchronizer's pre-seek time. Reporting the latter leaves
    // currentTime short of a forward seek, and a page nudging forward into a buffered range
    // re-seeks to the same place forever.
    EXPECT_EQ(*reportedTime, seekTime);
    EXPECT_GE(renderer->currentTime(), seekTime);
}

TEST_F(AudioVideoRendererAVFObjCSeekTest, ShortBackwardSeekReportsRequestedTime)
{
    ASSERT_TRUE(audioTrackId.has_value());
    enqueueAudioSample();

    // Park the synchronizer away from zero so there is room to seek backwards. This is far
    // enough to take the slow path, which moves the synchronizer synchronously.
    auto parkTime = MediaTime(100, 1000);
    EXPECT_TRUE(seekAndWaitForReportedTime(parkTime).has_value());
    ASSERT_TRUE(renderer->seeking());

    // Clear the pending seek without moving the synchronizer back, then restore sample
    // availability, which flush() dropped.
    renderer->flush();
    ASSERT_FALSE(renderer->seeking());
    enqueueAudioSample();

    auto seekTime = parkTime - MediaTime(1, 2000);
    auto reportedTime = seekAndWaitForReportedTime(seekTime);

    ASSERT_FALSE(renderer->seeking());
    ASSERT_TRUE(reportedTime.has_value());
    ASSERT_TRUE(reportedTime->isFinite());

    // A backward seek has to report the requested time too, so clamping the reported time up to
    // the synchronizer's would be just as wrong as leaving a forward seek short. currentTime is
    // not checked: lowering the time floor below the timebase is a no-op, so it stays at
    // parkTime until the synchronizer itself moves.
    EXPECT_EQ(*reportedTime, seekTime);
}

} // namespace TestWebKitAPI

#endif // USE(AVFOUNDATION)
