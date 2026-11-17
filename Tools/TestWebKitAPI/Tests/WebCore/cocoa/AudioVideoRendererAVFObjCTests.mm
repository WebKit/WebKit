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
#include <QuartzCore/QuartzCore.h>
#include <WebCore/AudioVideoRendererAVFObjC.h>
#include <WebCore/MediaPlayerEnums.h>
#include <WebCore/MediaSampleAVFObjC.h>
#include <WebCore/TrackInfo.h>
#include <WebCore/VideoFrame.h>
#include <wtf/Logger.h>
#include <wtf/RunLoop.h>

#include <pal/cf/CoreMediaSoftLink.h>

using namespace WebCore;

namespace TestWebKitAPI {

static RetainPtr<CMSampleBufferRef> createVideoSampleBuffer(int64_t presentationTimeInMilliseconds = 0)
{
    CVPixelBufferRef rawPixelBuffer = nullptr;
    if (CVPixelBufferCreate(kCFAllocatorDefault, 16, 16, kCVPixelFormatType_32BGRA, nullptr, &rawPixelBuffer))
        return nullptr;
    RetainPtr pixelBuffer = adoptCF(rawPixelBuffer);

    CMVideoFormatDescriptionRef rawFormatDescription = nullptr;
    if (PAL::CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, pixelBuffer.get(), &rawFormatDescription))
        return nullptr;
    RetainPtr formatDescription = adoptCF(rawFormatDescription);

    CMSampleTimingInfo timing = { PAL::kCMTimeInvalid, PAL::CMTimeMake(presentationTimeInMilliseconds, 1000), PAL::kCMTimeInvalid };
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

    void enqueueVideoSample(int64_t presentationTimeInMilliseconds = 0)
    {
        RetainPtr sampleBuffer = createVideoSampleBuffer(presentationTimeInMilliseconds);
        ASSERT_TRUE(sampleBuffer);
        renderer->enqueueSample(*videoTrackId, MediaSampleAVFObjC::create(sampleBuffer.get(), 0), { });
    }

    void observeFirstVideoFrame()
    {
        hasFirstVideoFrame = false;
        renderer->notifyFirstFrameAvailable([this] {
            hasFirstVideoFrame = true;
        });
    }

    static bool runUntil(const bool& condition)
    {
        for (unsigned attempt = 0; attempt < 40 && !condition; ++attempt)
            Util::runFor(0.05_s);
        return condition;
    }

    // Creates the display layer and gives it a decoded frame, as a visible page would.
    void startRenderingVideo()
    {
        observeFirstVideoFrame();
        renderer->setIsVisible(true);
        renderer->renderingCanBeAcceleratedChanged(true);
        enqueueVideoSample();
        EXPECT_TRUE(runUntil(hasFirstVideoFrame)) << "The first video frame was never decoded";
        Util::runFor(0.05_s);
    }

    // The renderer's platform layer is a container; the display layer is inside it.
    RetainPtr<CALayer> displayLayer() const
    {
        RetainPtr<CALayer> containerLayer = renderer->platformVideoLayer();
        for (CALayer *sublayer in [containerLayer sublayers]) {
            if ([sublayer isKindOfClass:NSClassFromString(@"AVSampleBufferDisplayLayer")])
                return sublayer;
        }
        return nil;
    }

    bool isReadyForMoreVideoSamples() const
    {
        return renderer->isReadyForMoreSamples(*videoTrackId);
    }

    RefPtr<AudioVideoRenderer> renderer;
    std::optional<AudioVideoRenderer::TrackIdentifier> videoTrackId;
    int flushToResumeCount { 0 };
    int errorCount { 0 };
    bool hasFirstVideoFrame { false };
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

// A sample holding an image buffer is already decoded: it bypasses the decompression session.
TEST_F(AudioVideoRendererAVFObjCTest, AlreadyDecodedVideoSampleIsDisplayedWithoutDecompressionSession)
{
    ASSERT_TRUE(videoTrackId.has_value());

    bool hasFirstFrame = false;
    renderer->notifyFirstFrameAvailable([&] {
        hasFirstFrame = true;
    });
    renderer->renderingCanBeAcceleratedChanged(true);

    enqueueVideoSample();
    Util::runFor(0.5_s);

    EXPECT_TRUE(hasFirstFrame);
    EXPECT_EQ(flushToResumeCount, 0);
    EXPECT_EQ(errorCount, 0);

    renderer->flushTrack(*videoTrackId);
    Util::runFor(0.1_s);
    EXPECT_EQ(errorCount, 0);
}

// Hiding a renderer that is playing must not disturb it at all. Suspending its video requires a
// flush to resume, and that flush is audible as a hiccup in the audio.
TEST_F(AudioVideoRendererAVFObjCTest, HidingWhilePlayingDoesNotSuspendVideo)
{
    ASSERT_TRUE(videoTrackId.has_value());

    Vector<double> effectiveRates;
    renderer->notifyEffectiveRateChanged([&](double rate) {
        effectiveRates.append(rate);
    });

    startRenderingVideo();
    ASSERT_TRUE(displayLayer());

    renderer->play();
    Util::runFor(0.2_s);
    size_t ratesBeforeHiding = effectiveRates.size();

    renderer->setIsVisible(false);
    Util::runFor(0.1_s);
    EXPECT_FALSE([displayLayer() isHidden]);
    EXPECT_TRUE(isReadyForMoreVideoSamples());

    renderer->setIsVisible(true);
    Util::runFor(0.1_s);
    EXPECT_FALSE([displayLayer() isHidden]);
    EXPECT_TRUE(isReadyForMoreVideoSamples());

    EXPECT_EQ(flushToResumeCount, 0);
    // The synchronizer must not have stopped, even momentarily.
    for (size_t i = ratesBeforeHiding; i < effectiveRates.size(); ++i)
        EXPECT_NE(effectiveRates[i], 0.0) << "effective rate change " << i << " stopped the synchronizer";
    EXPECT_EQ(errorCount, 0);
}

TEST_F(AudioVideoRendererAVFObjCTest, HidingWhilePausedSuspendsVideo)
{
    ASSERT_TRUE(videoTrackId.has_value());

    startRenderingVideo();
    ASSERT_TRUE(displayLayer());
    EXPECT_FALSE([displayLayer() isHidden]);
    EXPECT_TRUE(isReadyForMoreVideoSamples());

    renderer->setIsVisible(false);
    Util::runFor(0.1_s);
    EXPECT_TRUE([displayLayer() isHidden]);
    EXPECT_FALSE(isReadyForMoreVideoSamples());

    // Suspending alone does not ask the client to re-enqueue; only resuming does.
    EXPECT_EQ(flushToResumeCount, 0);
    EXPECT_EQ(errorCount, 0);
}

// A renderer that was playing when hidden suspends its video once it is paused.
TEST_F(AudioVideoRendererAVFObjCTest, PausingWhileHiddenSuspendsVideo)
{
    ASSERT_TRUE(videoTrackId.has_value());

    startRenderingVideo();
    ASSERT_TRUE(displayLayer());

    renderer->play();
    renderer->setIsVisible(false);
    Util::runFor(0.1_s);
    EXPECT_FALSE([displayLayer() isHidden]);
    EXPECT_TRUE(isReadyForMoreVideoSamples());

    renderer->pause();
    Util::runFor(0.1_s);
    EXPECT_TRUE([displayLayer() isHidden]);
    EXPECT_FALSE(isReadyForMoreVideoSamples());
    EXPECT_EQ(flushToResumeCount, 0);
    EXPECT_EQ(errorCount, 0);
}

// Video samples were dropped while suspended, so resuming asks the client to flush and re-enqueue,
// and no more samples are requested until it does.
TEST_F(AudioVideoRendererAVFObjCTest, ShowingSuspendedVideoResumesIt)
{
    ASSERT_TRUE(videoTrackId.has_value());

    startRenderingVideo();
    ASSERT_TRUE(displayLayer());

    renderer->setIsVisible(false);
    Util::runFor(0.1_s);
    ASSERT_TRUE([displayLayer() isHidden]);

    renderer->setIsVisible(true);
    Util::runFor(0.1_s);
    EXPECT_FALSE([displayLayer() isHidden]);
    EXPECT_EQ(flushToResumeCount, 1);
    EXPECT_FALSE(isReadyForMoreVideoSamples());

    renderer->flushTrack(*videoTrackId);
    Util::runFor(0.1_s);
    EXPECT_TRUE(isReadyForMoreVideoSamples());
    EXPECT_EQ(flushToResumeCount, 1);
    EXPECT_EQ(errorCount, 0);
}

// Starting playback resumes suspended video even while the renderer is still hidden, so that the
// flush happens before playback, not during it.
TEST_F(AudioVideoRendererAVFObjCTest, PlayingSuspendedVideoResumesIt)
{
    ASSERT_TRUE(videoTrackId.has_value());

    startRenderingVideo();
    ASSERT_TRUE(displayLayer());

    renderer->setIsVisible(false);
    Util::runFor(0.1_s);
    ASSERT_TRUE([displayLayer() isHidden]);

    renderer->play();
    Util::runFor(0.1_s);
    EXPECT_FALSE([displayLayer() isHidden]);
    EXPECT_EQ(flushToResumeCount, 1);
    EXPECT_EQ(errorCount, 0);
}

// A display layer created while video is suspended must start out hidden, or recreating the layer
// would undo the suspension.
TEST_F(AudioVideoRendererAVFObjCTest, DisplayLayerCreatedWhileSuspendedIsHidden)
{
    ASSERT_TRUE(videoTrackId.has_value());

    // With a decompression session, losing accelerated rendering destroys the display layer, so
    // that regaining it creates a new one.
    renderer->setPreferences({ VideoRendererPreference::PrefersDecompressionSession });
    startRenderingVideo();
    RetainPtr originalDisplayLayer = displayLayer();
    ASSERT_TRUE(originalDisplayLayer);

    renderer->setIsVisible(false);
    Util::runFor(0.1_s);

    renderer->renderingCanBeAcceleratedChanged(false);
    Util::runFor(0.1_s);
    renderer->renderingCanBeAcceleratedChanged(true);
    Util::runFor(0.1_s);

    RetainPtr recreatedDisplayLayer = displayLayer();
    ASSERT_TRUE(recreatedDisplayLayer);
    ASSERT_NE(recreatedDisplayLayer.get(), originalDisplayLayer.get());
    EXPECT_TRUE([recreatedDisplayLayer isHidden]);
    EXPECT_EQ(errorCount, 0);
}

// Video isn't suspended until the frame for the current time is decoded, so that a paused element
// that has never shown a frame still gets one.
TEST_F(AudioVideoRendererAVFObjCTest, HidingWhilePausedWaitsForCurrentFrameBeforeSuspending)
{
    ASSERT_TRUE(videoTrackId.has_value());

    observeFirstVideoFrame();
    renderer->setIsVisible(true);
    renderer->renderingCanBeAcceleratedChanged(true);
    Util::runFor(0.1_s);
    ASSERT_TRUE(displayLayer());

    renderer->setIsVisible(false);
    Util::runFor(0.1_s);
    EXPECT_FALSE([displayLayer() isHidden]);
    EXPECT_TRUE(isReadyForMoreVideoSamples());

    enqueueVideoSample();
    ASSERT_TRUE(runUntil(hasFirstVideoFrame));
    Util::runFor(0.05_s);
    EXPECT_TRUE([displayLayer() isHidden]);
    EXPECT_FALSE(isReadyForMoreVideoSamples());
    EXPECT_EQ(flushToResumeCount, 0);
    EXPECT_EQ(errorCount, 0);
}

// requestVideoFrameCallback relies on being told about every frame, including the one decoded for
// a paused, hidden element. Suspending flushes the video renderer, which drops that notification
// if it hasn't been delivered yet.
TEST_F(AudioVideoRendererAVFObjCTest, HiddenPausedRendererReportsCurrentFrameBeforeSuspending)
{
    ASSERT_TRUE(videoTrackId.has_value());

    std::optional<MediaTime> reportedFrameTime;
    renderer->notifyWhenHasAvailableVideoFrame([&](const MediaTime& presentationTime, double) {
        reportedFrameTime = presentationTime;
    });

    renderer->setIsVisible(true);
    renderer->renderingCanBeAcceleratedChanged(true);
    Util::runFor(0.1_s);
    renderer->setIsVisible(false);

    enqueueVideoSample();
    bool hasReportedFrame = false;
    for (unsigned attempt = 0; attempt < 40 && !hasReportedFrame; ++attempt) {
        Util::runFor(0.05_s);
        hasReportedFrame = reportedFrameTime.has_value();
    }
    ASSERT_TRUE(hasReportedFrame) << "The decoded frame was never reported";
    EXPECT_EQ(*reportedFrameTime, MediaTime::zeroTime());

    Util::runFor(0.05_s);
    EXPECT_TRUE([displayLayer() isHidden]);
    EXPECT_EQ(errorCount, 0);
}

// Suspending flushes the video renderer, but a paused element must still be able to provide an
// image of its current frame.
TEST_F(AudioVideoRendererAVFObjCTest, SuspendedVideoKeepsCurrentFrame)
{
    ASSERT_TRUE(videoTrackId.has_value());

    startRenderingVideo();
    ASSERT_TRUE(renderer->currentVideoFrame());

    renderer->setIsVisible(false);
    Util::runFor(0.1_s);
    ASSERT_TRUE([displayLayer() isHidden]);
    EXPECT_TRUE(renderer->currentVideoFrame());
    EXPECT_EQ(errorCount, 0);
}

// A seek can't complete until the frame at its destination is decoded. Seeking resumes suspended
// video without asking for a flush, since the seek re-enqueues from its destination anyway, and
// video is suspended again once the seek completes.
TEST_F(AudioVideoRendererAVFObjCTest, SeekingWhileSuspendedDecodesDestinationFrame)
{
    ASSERT_TRUE(videoTrackId.has_value());

    std::optional<MediaTime> reportedFrameTime;
    renderer->notifyWhenHasAvailableVideoFrame([&](const MediaTime& presentationTime, double) {
        reportedFrameTime = presentationTime;
    });

    startRenderingVideo();
    renderer->setIsVisible(false);
    Util::runFor(0.1_s);
    ASSERT_TRUE([displayLayer() isHidden]);

    // Far enough from the current time that the synchronizer has to move.
    constexpr int64_t seekTimeInMilliseconds = 1000;
    auto seekTime = MediaTime(seekTimeInMilliseconds, 1000);
    bool prepared = false;
    renderer->prepareToSeek(seekTime)->whenSettled(RunLoop::currentSingleton(), [&](auto&&) {
        prepared = true;
    });
    ASSERT_TRUE(runUntil(prepared));
    EXPECT_FALSE([displayLayer() isHidden]);
    EXPECT_EQ(flushToResumeCount, 0);

    // As a client does when seeking: flush, then enqueue from the destination.
    reportedFrameTime = std::nullopt;
    observeFirstVideoFrame();
    renderer->flushTrack(*videoTrackId);
    Util::runFor(0.05_s);
    EXPECT_TRUE(isReadyForMoreVideoSamples());
    enqueueVideoSample(seekTimeInMilliseconds);

    bool seekCompleted = false;
    bool seekSucceeded = false;
    renderer->finishSeek(seekTime)->whenSettled(RunLoop::currentSingleton(), [&](auto&& result) {
        seekSucceeded = !!result;
        seekCompleted = true;
    });
    ASSERT_TRUE(runUntil(seekCompleted)) << "The seek never completed";
    EXPECT_TRUE(seekSucceeded);
    Util::runFor(0.05_s);

    // The destination frame is reported, as requestVideoFrameCallback requires, before suspending.
    ASSERT_TRUE(reportedFrameTime.has_value()) << "The frame at the seek destination was never reported";
    EXPECT_EQ(*reportedFrameTime, seekTime);
    EXPECT_TRUE([displayLayer() isHidden]);
    EXPECT_FALSE(isReadyForMoreVideoSamples());
    EXPECT_EQ(flushToResumeCount, 0);
    EXPECT_EQ(errorCount, 0);
}

// As with the accelerated rendering changes above, there is nothing to re-enqueue when no sample
// was ever enqueued, so a visibility cycle must not ask for a flush.
TEST_F(AudioVideoRendererAVFObjCTest, NoFlushOnVisibilityCycleWithoutEnqueuedSamples)
{
    ASSERT_TRUE(videoTrackId.has_value());

    renderer->setIsVisible(true);
    renderer->renderingCanBeAcceleratedChanged(true);
    Util::runFor(0.1_s);

    renderer->setIsVisible(false);
    Util::runFor(0.1_s);
    renderer->setIsVisible(true);
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
