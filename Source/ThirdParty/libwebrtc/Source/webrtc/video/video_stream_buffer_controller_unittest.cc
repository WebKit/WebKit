/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/video_stream_buffer_controller.h"

#include <stdint.h>

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "api/metronome/test/fake_metronome.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/video_content_type.h"
#include "api/video/video_timing.h"
#include "rtc_base/checks.h"
#include "test/fake_encoded_frame.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/scoped_key_value_config.h"
#include "test/time_controller/simulated_time_controller.h"
#include "video/decode_synchronizer.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Each;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Matches;
using ::testing::Ne;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::SizeIs;
using ::testing::VariantWith;

namespace webrtc {

namespace {

constexpr size_t kFrameSize = 10;
constexpr uint32_t kFps30Rtp = 90000 / 30;
constexpr TimeDelta kFps30Delay = 1 / Frequency::Hertz(30);
const VideoPlayoutDelay kZeroPlayoutDelay = {0, 0};
constexpr Timestamp kClockStart = Timestamp::Millis(1000);

auto TimedOut() {
  return Optional(VariantWith<TimeDelta>(_));
}

auto Frame(testing::Matcher<EncodedFrame> m) {
  return Optional(VariantWith<std::unique_ptr<EncodedFrame>>(Pointee(m)));
}

std::unique_ptr<test::FakeEncodedFrame> WithReceiveTimeFromRtpTimestamp(
    std::unique_ptr<test::FakeEncodedFrame> frame) {
  if (frame->Timestamp() == 0) {
    frame->SetReceivedTime(kClockStart.ms());
  } else {
    frame->SetReceivedTime(
        TimeDelta::Seconds(frame->Timestamp() / 90000.0).ms() +
        kClockStart.ms());
  }
  return frame;
}

class VCMReceiveStatisticsCallbackMock : public VCMReceiveStatisticsCallback {
 public:
  MOCK_METHOD(void,
              OnCompleteFrame,
              (bool is_keyframe,
               size_t size_bytes,
               VideoContentType content_type),
              (override));
  MOCK_METHOD(void, OnDroppedFrames, (uint32_t num_dropped), (override));
  MOCK_METHOD(void,
              OnFrameBufferTimingsUpdated,
              (int max_decode_ms,
               int current_delay_ms,
               int target_delay_ms,
               int jitter_buffer_ms,
               int min_playout_delay_ms,
               int render_delay_ms),
              (override));
  MOCK_METHOD(void,
              OnTimingFrameInfoUpdated,
              (const TimingFrameInfo& info),
              (override));
};

}  // namespace

constexpr auto kMaxWaitForKeyframe = TimeDelta::Millis(500);
constexpr auto kMaxWaitForFrame = TimeDelta::Millis(1500);
class VideoStreamBufferControllerFixture
    : public ::testing::WithParamInterface<std::string>,
      public FrameSchedulingReceiver {
 public:
  VideoStreamBufferControllerFixture()
      : field_trials_(GetParam()),
        time_controller_(kClockStart),
        clock_(time_controller_.GetClock()),
        fake_metronome_(time_controller_.GetTaskQueueFactory(),
                        TimeDelta::Millis(16)),
        decode_sync_(clock_,
                     &fake_metronome_,
                     time_controller_.GetMainThread()),
        timing_(clock_, field_trials_),
        buffer_(VideoStreamBufferController::CreateFromFieldTrial(
            clock_,
            time_controller_.GetMainThread(),
            &timing_,
            &stats_callback_,
            this,
            kMaxWaitForKeyframe,
            kMaxWaitForFrame,
            &decode_sync_,
            field_trials_)) {
    // Avoid starting with negative render times.
    timing_.set_min_playout_delay(TimeDelta::Millis(10));

    ON_CALL(stats_callback_, OnDroppedFrames)
        .WillByDefault(
            [this](auto num_dropped) { dropped_frames_ += num_dropped; });
  }

  ~VideoStreamBufferControllerFixture() override {
    if (buffer_) {
      buffer_->Stop();
    }
    fake_metronome_.Stop();
    time_controller_.AdvanceTime(TimeDelta::Zero());
  }

  void OnEncodedFrame(std::unique_ptr<EncodedFrame> frame) override {
    RTC_DCHECK(frame);
    SetWaitResult(std::move(frame));
  }

  void OnDecodableFrameTimeout(TimeDelta wait_time) override {
    SetWaitResult(wait_time);
  }

  using WaitResult =
      absl::variant<std::unique_ptr<EncodedFrame>, TimeDelta /*wait_time*/>;

  absl::optional<WaitResult> WaitForFrameOrTimeout(TimeDelta wait) {
    if (wait_result_) {
      return std::move(wait_result_);
    }
    time_controller_.AdvanceTime(TimeDelta::Zero());
    if (wait_result_) {
      return std::move(wait_result_);
    }

    Timestamp now = clock_->CurrentTime();
    // TODO(bugs.webrtc.org/13756): Remove this when rtc::Thread uses uses
    // Timestamp instead of an integer milliseconds. This extra wait is needed
    // for some tests that use the metronome. This is due to rounding
    // milliseconds, affecting the precision of simulated time controller uses
    // when posting tasks from threads.
    TimeDelta potential_extra_wait =
        Timestamp::Millis((now + wait).ms()) - (now + wait);

    time_controller_.AdvanceTime(wait);
    if (potential_extra_wait > TimeDelta::Zero()) {
      time_controller_.AdvanceTime(potential_extra_wait);
    }
    return std::move(wait_result_);
  }

  void StartNextDecode() {
    ResetLastResult();
    buffer_->StartNextDecode(false);
  }

  void StartNextDecodeForceKeyframe() {
    ResetLastResult();
    buffer_->StartNextDecode(true);
  }

  void ResetLastResult() { wait_result_.reset(); }

  int dropped_frames() const { return dropped_frames_; }

 protected:
  test::ScopedKeyValueConfig field_trials_;
  GlobalSimulatedTimeController time_controller_;
  Clock* const clock_;
  test::FakeMetronome fake_metronome_;
  DecodeSynchronizer decode_sync_;
  VCMTiming timing_;

  ::testing::NiceMock<VCMReceiveStatisticsCallbackMock> stats_callback_;
  std::unique_ptr<VideoStreamBufferController> buffer_;

 private:
  void SetWaitResult(WaitResult result) {
    RTC_DCHECK(!wait_result_);
    if (absl::holds_alternative<std::unique_ptr<EncodedFrame>>(result)) {
      RTC_DCHECK(absl::get<std::unique_ptr<EncodedFrame>>(result));
    }
    wait_result_.emplace(std::move(result));
  }

  uint32_t dropped_frames_ = 0;
  absl::optional<WaitResult> wait_result_;
};

class VideoStreamBufferControllerTest
    : public ::testing::Test,
      public VideoStreamBufferControllerFixture {};

TEST_P(VideoStreamBufferControllerTest,
       InitialTimeoutAfterKeyframeTimeoutPeriod) {
  StartNextDecodeForceKeyframe();
  // No frame inserted. Timeout expected.
  EXPECT_THAT(WaitForFrameOrTimeout(kMaxWaitForKeyframe), TimedOut());

  // No new timeout set since receiver has not started new decode.
  ResetLastResult();
  EXPECT_THAT(WaitForFrameOrTimeout(kMaxWaitForKeyframe), Eq(absl::nullopt));

  // Now that receiver has asked for new frame, a new timeout can occur.
  StartNextDecodeForceKeyframe();
  EXPECT_THAT(WaitForFrameOrTimeout(kMaxWaitForKeyframe), TimedOut());
}

TEST_P(VideoStreamBufferControllerTest, KeyFramesAreScheduled) {
  StartNextDecodeForceKeyframe();
  time_controller_.AdvanceTime(TimeDelta::Millis(50));

  auto frame = test::FakeFrameBuilder().Id(0).Time(0).AsLast().Build();
  buffer_->InsertFrame(std::move(frame));

  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(test::WithId(0)));
}

TEST_P(VideoStreamBufferControllerTest,
       DeltaFrameTimeoutAfterKeyframeExtracted) {
  StartNextDecodeForceKeyframe();

  time_controller_.AdvanceTime(TimeDelta::Millis(50));
  auto frame = test::FakeFrameBuilder().Id(0).Time(0).AsLast().Build();
  buffer_->InsertFrame(std::move(frame));
  EXPECT_THAT(WaitForFrameOrTimeout(kMaxWaitForKeyframe),
              Frame(test::WithId(0)));

  StartNextDecode();
  time_controller_.AdvanceTime(TimeDelta::Millis(50));

  // Timeouts should now happen at the normal frequency.
  const int expected_timeouts = 5;
  for (int i = 0; i < expected_timeouts; ++i) {
    EXPECT_THAT(WaitForFrameOrTimeout(kMaxWaitForFrame), TimedOut());
    StartNextDecode();
  }
}

TEST_P(VideoStreamBufferControllerTest, DependantFramesAreScheduled) {
  StartNextDecodeForceKeyframe();
  buffer_->InsertFrame(test::FakeFrameBuilder().Id(0).Time(0).AsLast().Build());
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(test::WithId(0)));

  StartNextDecode();

  time_controller_.AdvanceTime(kFps30Delay);
  buffer_->InsertFrame(test::FakeFrameBuilder()
                           .Id(1)
                           .Time(kFps30Rtp)
                           .AsLast()
                           .Refs({0})
                           .Build());
  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay), Frame(test::WithId(1)));
}

TEST_P(VideoStreamBufferControllerTest, SpatialLayersAreScheduled) {
  StartNextDecodeForceKeyframe();
  buffer_->InsertFrame(WithReceiveTimeFromRtpTimestamp(
      test::FakeFrameBuilder().Id(0).SpatialLayer(0).Time(0).Build()));
  buffer_->InsertFrame(WithReceiveTimeFromRtpTimestamp(
      test::FakeFrameBuilder().Id(1).SpatialLayer(1).Time(0).Build()));
  buffer_->InsertFrame(WithReceiveTimeFromRtpTimestamp(
      test::FakeFrameBuilder().Id(2).SpatialLayer(2).Time(0).AsLast().Build()));
  EXPECT_THAT(
      WaitForFrameOrTimeout(TimeDelta::Zero()),
      Frame(AllOf(test::WithId(0), test::FrameWithSize(3 * kFrameSize))));

  buffer_->InsertFrame(WithReceiveTimeFromRtpTimestamp(
      test::FakeFrameBuilder().Id(3).Time(kFps30Rtp).SpatialLayer(0).Build()));
  buffer_->InsertFrame(WithReceiveTimeFromRtpTimestamp(
      test::FakeFrameBuilder().Id(4).Time(kFps30Rtp).SpatialLayer(1).Build()));
  buffer_->InsertFrame(WithReceiveTimeFromRtpTimestamp(test::FakeFrameBuilder()
                                                           .Id(5)
                                                           .Time(kFps30Rtp)
                                                           .SpatialLayer(2)
                                                           .AsLast()
                                                           .Build()));

  StartNextDecode();
  EXPECT_THAT(
      WaitForFrameOrTimeout(kFps30Delay * 10),
      Frame(AllOf(test::WithId(3), test::FrameWithSize(3 * kFrameSize))));
}

TEST_P(VideoStreamBufferControllerTest,
       OutstandingFrameTasksAreCancelledAfterDeletion) {
  StartNextDecodeForceKeyframe();
  buffer_->InsertFrame(WithReceiveTimeFromRtpTimestamp(
      test::FakeFrameBuilder().Id(0).Time(0).AsLast().Build()));
  // Get keyframe. Delta frame should now be scheduled.
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(test::WithId(0)));

  StartNextDecode();
  buffer_->InsertFrame(WithReceiveTimeFromRtpTimestamp(test::FakeFrameBuilder()
                                                           .Id(1)
                                                           .Time(kFps30Rtp)
                                                           .AsLast()
                                                           .Refs({0})
                                                           .Build()));
  buffer_->Stop();
  // Wait for 2x max wait time. Since we stopped, this should cause no timeouts
  // or frame-ready callbacks.
  EXPECT_THAT(WaitForFrameOrTimeout(kMaxWaitForFrame * 2), Eq(absl::nullopt));
}

TEST_P(VideoStreamBufferControllerTest, FramesWaitForDecoderToComplete) {
  StartNextDecodeForceKeyframe();

  // Start with a keyframe.
  buffer_->InsertFrame(test::FakeFrameBuilder().Id(0).Time(0).AsLast().Build());
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(test::WithId(0)));

  ResetLastResult();
  // Insert a delta frame.
  buffer_->InsertFrame(test::FakeFrameBuilder()
                           .Id(1)
                           .Time(kFps30Rtp)
                           .AsLast()
                           .Refs({0})
                           .Build());

  // Advancing time should not result in a frame since the scheduler has not
  // been signalled that we are ready.
  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay), Eq(absl::nullopt));
  // Signal ready.
  StartNextDecode();
  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay), Frame(test::WithId(1)));
}

TEST_P(VideoStreamBufferControllerTest, LateFrameDropped) {
  StartNextDecodeForceKeyframe();
  //   F1
  //   /
  // F0 --> F2
  buffer_->InsertFrame(test::FakeFrameBuilder().Id(0).Time(0).AsLast().Build());
  // Start with a keyframe.
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(test::WithId(0)));

  StartNextDecode();

  // Simulate late F1 which arrives after F2.
  time_controller_.AdvanceTime(kFps30Delay * 2);
  buffer_->InsertFrame(test::FakeFrameBuilder()
                           .Id(2)
                           .Time(2 * kFps30Rtp)
                           .AsLast()
                           .Refs({0})
                           .Build());
  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay), Frame(test::WithId(2)));

  StartNextDecode();

  buffer_->InsertFrame(test::FakeFrameBuilder()
                           .Id(1)
                           .Time(1 * kFps30Rtp)
                           .AsLast()
                           .Refs({0})
                           .Build());
  // Confirm frame 1 is never scheduled by timing out.
  EXPECT_THAT(WaitForFrameOrTimeout(kMaxWaitForFrame), TimedOut());
}

TEST_P(VideoStreamBufferControllerTest, FramesFastForwardOnSystemHalt) {
  StartNextDecodeForceKeyframe();
  //   F1
  //   /
  // F0 --> F2
  buffer_->InsertFrame(test::FakeFrameBuilder().Id(0).Time(0).AsLast().Build());

  // Start with a keyframe.
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(test::WithId(0)));

  time_controller_.AdvanceTime(kFps30Delay);
  buffer_->InsertFrame(test::FakeFrameBuilder()
                           .Id(1)
                           .Time(kFps30Rtp)
                           .AsLast()
                           .Refs({0})
                           .Build());
  time_controller_.AdvanceTime(kFps30Delay);
  buffer_->InsertFrame(test::FakeFrameBuilder()
                           .Id(2)
                           .Time(2 * kFps30Rtp)
                           .AsLast()
                           .Refs({0})
                           .Build());

  // Halting time should result in F1 being skipped.
  time_controller_.AdvanceTime(kFps30Delay * 2);
  StartNextDecode();
  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay), Frame(test::WithId(2)));
  EXPECT_EQ(dropped_frames(), 1);
}

TEST_P(VideoStreamBufferControllerTest, ForceKeyFrame) {
  StartNextDecodeForceKeyframe();
  // Initial keyframe.
  buffer_->InsertFrame(test::FakeFrameBuilder().Id(0).Time(0).AsLast().Build());
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(test::WithId(0)));

  StartNextDecodeForceKeyframe();

  // F2 is the next keyframe, and should be extracted since a keyframe was
  // forced.
  buffer_->InsertFrame(test::FakeFrameBuilder()
                           .Id(1)
                           .Time(kFps30Rtp)
                           .AsLast()
                           .Refs({0})
                           .Build());
  buffer_->InsertFrame(
      test::FakeFrameBuilder().Id(2).Time(kFps30Rtp * 2).AsLast().Build());

  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay * 3), Frame(test::WithId(2)));
}

TEST_P(VideoStreamBufferControllerTest, SlowDecoderDropsTemporalLayers) {
  StartNextDecodeForceKeyframe();
  // 2 temporal layers, at 15fps per layer to make 30fps total.
  // Decoder is slower than 30fps, so last_frame() will be skipped.
  //   F1 --> F3 --> F5
  //   /      /     /
  // F0 --> F2 --> F4
  buffer_->InsertFrame(test::FakeFrameBuilder().Id(0).Time(0).AsLast().Build());
  // Keyframe received.
  // Don't start next decode until slow delay.
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(test::WithId(0)));

  time_controller_.AdvanceTime(kFps30Delay);
  buffer_->InsertFrame(test::FakeFrameBuilder()
                           .Id(1)
                           .Time(1 * kFps30Rtp)
                           .Refs({0})
                           .AsLast()
                           .Build());
  time_controller_.AdvanceTime(kFps30Delay);
  buffer_->InsertFrame(test::FakeFrameBuilder()
                           .Id(2)
                           .Time(2 * kFps30Rtp)
                           .Refs({0})
                           .AsLast()
                           .Build());

  // Simulate decode taking 3x FPS rate.
  time_controller_.AdvanceTime(kFps30Delay * 1.5);
  StartNextDecode();
  // F2 is the best frame since decoding was so slow that F1 is too old.
  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay * 2), Frame(test::WithId(2)));
  EXPECT_EQ(dropped_frames(), 1);
  time_controller_.AdvanceTime(kFps30Delay / 2);

  buffer_->InsertFrame(test::FakeFrameBuilder()
                           .Id(3)
                           .Time(3 * kFps30Rtp)
                           .Refs({1, 2})
                           .AsLast()
                           .Build());
  time_controller_.AdvanceTime(kFps30Delay / 2);
  buffer_->InsertFrame(test::FakeFrameBuilder()
                           .Id(4)
                           .Time(4 * kFps30Rtp)
                           .Refs({2})
                           .AsLast()
                           .Build());
  time_controller_.AdvanceTime(kFps30Delay / 2);

  // F4 is the best frame since decoding was so slow that F1 is too old.
  time_controller_.AdvanceTime(kFps30Delay);
  StartNextDecode();
  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay), Frame(test::WithId(4)));

  buffer_->InsertFrame(test::FakeFrameBuilder()
                           .Id(5)
                           .Time(5 * kFps30Rtp)
                           .Refs({3, 4})
                           .AsLast()
                           .Build());
  time_controller_.AdvanceTime(kFps30Delay / 2);

  // F5 is not decodable since F4 was decoded, so a timeout is expected.
  time_controller_.AdvanceTime(TimeDelta::Millis(10));
  StartNextDecode();
  EXPECT_THAT(WaitForFrameOrTimeout(kMaxWaitForFrame), TimedOut());
  // TODO(bugs.webrtc.org/13343): This should be 2 dropped frames since frames 1
  // and 3 were dropped. However, frame_buffer2 does not mark frame 3 as dropped
  // which is a bug. Uncomment below when that is fixed for frame_buffer2 is
  // deleted.
  // EXPECT_EQ(dropped_frames(), 2);
}

TEST_P(VideoStreamBufferControllerTest,
       NewFrameInsertedWhileWaitingToReleaseFrame) {
  StartNextDecodeForceKeyframe();
  // Initial keyframe.
  buffer_->InsertFrame(WithReceiveTimeFromRtpTimestamp(
      test::FakeFrameBuilder().Id(0).Time(0).AsLast().Build()));
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(test::WithId(0)));

  time_controller_.AdvanceTime(kFps30Delay / 2);
  buffer_->InsertFrame(WithReceiveTimeFromRtpTimestamp(test::FakeFrameBuilder()
                                                           .Id(1)
                                                           .Time(kFps30Rtp)
                                                           .Refs({0})
                                                           .AsLast()
                                                           .Build()));
  StartNextDecode();
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Eq(absl::nullopt));

  // Scheduler is waiting to deliver Frame 1 now. Insert Frame 2. Frame 1 should
  // be delivered still.
  buffer_->InsertFrame(WithReceiveTimeFromRtpTimestamp(test::FakeFrameBuilder()
                                                           .Id(2)
                                                           .Time(kFps30Rtp * 2)
                                                           .Refs({0})
                                                           .AsLast()
                                                           .Build()));
  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay), Frame(test::WithId(1)));
}

TEST_P(VideoStreamBufferControllerTest, SameFrameNotScheduledTwice) {
  // A frame could be scheduled twice if last_frame() arrive out-of-order but
  // the older frame is old enough to be fast forwarded.
  //
  // 1. F2 arrives and is scheduled.
  // 2. F3 arrives, but scheduling will not change since F2 is next.
  // 3. F1 arrives late and scheduling is checked since it is before F2. F1
  // fast-forwarded since it is older.
  //
  // F2 is the best frame, but should only be scheduled once, followed by F3.
  StartNextDecodeForceKeyframe();

  // First keyframe.
  buffer_->InsertFrame(WithReceiveTimeFromRtpTimestamp(
      test::FakeFrameBuilder().Id(0).Time(0).AsLast().Build()));
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Millis(15)),
              Frame(test::WithId(0)));

  StartNextDecode();

  // F2 arrives and is scheduled.
  buffer_->InsertFrame(WithReceiveTimeFromRtpTimestamp(
      test::FakeFrameBuilder().Id(2).Time(2 * kFps30Rtp).AsLast().Build()));

  // F3 arrives before F2 is extracted.
  time_controller_.AdvanceTime(kFps30Delay);
  buffer_->InsertFrame(WithReceiveTimeFromRtpTimestamp(
      test::FakeFrameBuilder().Id(3).Time(3 * kFps30Rtp).AsLast().Build()));

  // F1 arrives and is fast-forwarded since it is too late.
  // F2 is already scheduled and should not be rescheduled.
  time_controller_.AdvanceTime(kFps30Delay / 2);
  buffer_->InsertFrame(WithReceiveTimeFromRtpTimestamp(
      test::FakeFrameBuilder().Id(1).Time(1 * kFps30Rtp).AsLast().Build()));

  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay), Frame(test::WithId(2)));
  StartNextDecode();

  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay), Frame(test::WithId(3)));
  StartNextDecode();
  EXPECT_THAT(WaitForFrameOrTimeout(kMaxWaitForFrame), TimedOut());
  EXPECT_EQ(dropped_frames(), 1);
}

TEST_P(VideoStreamBufferControllerTest, TestStatsCallback) {
  EXPECT_CALL(stats_callback_,
              OnCompleteFrame(true, kFrameSize, VideoContentType::UNSPECIFIED));
  EXPECT_CALL(stats_callback_, OnFrameBufferTimingsUpdated);

  // Fake timing having received decoded frame.
  timing_.StopDecodeTimer(TimeDelta::Millis(1), clock_->CurrentTime());
  StartNextDecodeForceKeyframe();
  buffer_->InsertFrame(test::FakeFrameBuilder().Id(0).Time(0).AsLast().Build());
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(test::WithId(0)));

  // Flush stats posted on the decode queue.
  time_controller_.AdvanceTime(TimeDelta::Zero());
}

TEST_P(VideoStreamBufferControllerTest,
       FrameCompleteCalledOnceForDuplicateFrame) {
  EXPECT_CALL(stats_callback_,
              OnCompleteFrame(true, kFrameSize, VideoContentType::UNSPECIFIED))
      .Times(1);

  StartNextDecodeForceKeyframe();
  buffer_->InsertFrame(test::FakeFrameBuilder().Id(0).Time(0).AsLast().Build());
  buffer_->InsertFrame(test::FakeFrameBuilder().Id(0).Time(0).AsLast().Build());
  // Flush stats posted on the decode queue.
  time_controller_.AdvanceTime(TimeDelta::Zero());
}

TEST_P(VideoStreamBufferControllerTest,
       FrameCompleteCalledOnceForSingleTemporalUnit) {
  StartNextDecodeForceKeyframe();

  // `OnCompleteFrame` should not be called for the first two frames since they
  // do not complete the temporal layer.
  EXPECT_CALL(stats_callback_, OnCompleteFrame(_, _, _)).Times(0);
  buffer_->InsertFrame(test::FakeFrameBuilder().Id(0).Time(0).Build());
  buffer_->InsertFrame(
      test::FakeFrameBuilder().Id(1).Time(0).Refs({0}).Build());
  time_controller_.AdvanceTime(TimeDelta::Zero());
  // Flush stats posted on the decode queue.
  ::testing::Mock::VerifyAndClearExpectations(&stats_callback_);

  // Note that this frame is not marked as a keyframe since the last spatial
  // layer has dependencies.
  EXPECT_CALL(stats_callback_,
              OnCompleteFrame(false, kFrameSize, VideoContentType::UNSPECIFIED))
      .Times(1);
  buffer_->InsertFrame(
      test::FakeFrameBuilder().Id(2).Time(0).Refs({0, 1}).AsLast().Build());
  // Flush stats posted on the decode queue.
  time_controller_.AdvanceTime(TimeDelta::Zero());
}

TEST_P(VideoStreamBufferControllerTest,
       FrameCompleteCalledOnceForCompleteTemporalUnit) {
  // FrameBuffer2 logs the complete frame on the arrival of the last layer.
  StartNextDecodeForceKeyframe();

  // `OnCompleteFrame` should not be called for the first two frames since they
  // do not complete the temporal layer. Frame 1 arrives later, at which time
  // this frame can finally be considered complete.
  EXPECT_CALL(stats_callback_, OnCompleteFrame(_, _, _)).Times(0);
  buffer_->InsertFrame(test::FakeFrameBuilder().Id(0).Time(0).Build());
  buffer_->InsertFrame(
      test::FakeFrameBuilder().Id(2).Time(0).Refs({0, 1}).AsLast().Build());
  time_controller_.AdvanceTime(TimeDelta::Zero());
  // Flush stats posted on the decode queue.
  ::testing::Mock::VerifyAndClearExpectations(&stats_callback_);

  EXPECT_CALL(stats_callback_,
              OnCompleteFrame(false, kFrameSize, VideoContentType::UNSPECIFIED))
      .Times(1);
  buffer_->InsertFrame(
      test::FakeFrameBuilder().Id(1).Time(0).Refs({0}).Build());
  // Flush stats posted on the decode queue.
  time_controller_.AdvanceTime(TimeDelta::Zero());
}

// Note: This test takes a long time to run if the fake metronome is active.
// Since the test needs to wait for the timestamp to rollover, it has a fake
// delay of around 6.5 hours. Even though time is simulated, this will be
// around 1,500,000 metronome tick invocations.
TEST_P(VideoStreamBufferControllerTest, NextFrameWithOldTimestamp) {
  // Test inserting 31 frames and pause the stream for a long time before
  // frame 32.
  StartNextDecodeForceKeyframe();
  constexpr uint32_t kBaseRtp = std::numeric_limits<uint32_t>::max() / 2;

  // First keyframe. The receive time must be explicitly set in this test since
  // the RTP derived time used in all tests does not work when the long pause
  // happens later in the test.
  buffer_->InsertFrame(test::FakeFrameBuilder()
                           .Id(0)
                           .Time(kBaseRtp)
                           .ReceivedTime(clock_->CurrentTime())
                           .AsLast()
                           .Build());
  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay), Frame(test::WithId(0)));

  // 1 more frame to warmup VCMTiming for 30fps.
  StartNextDecode();
  buffer_->InsertFrame(test::FakeFrameBuilder()
                           .Id(1)
                           .Time(kBaseRtp + kFps30Rtp)
                           .ReceivedTime(clock_->CurrentTime())
                           .AsLast()
                           .Build());
  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay), Frame(test::WithId(1)));

  // Pause the stream for such a long time it incurs an RTP timestamp rollover
  // by over half.
  constexpr uint32_t kLastRtp = kBaseRtp + kFps30Rtp;
  constexpr uint32_t kRolloverRtp =
      kLastRtp + std::numeric_limits<uint32_t>::max() / 2 + 1;
  constexpr Frequency kRtpHz = Frequency::KiloHertz(90);
  // Pause for corresponding delay such that RTP timestamp would increase this
  // much at 30fps.
  constexpr TimeDelta kRolloverDelay =
      (std::numeric_limits<uint32_t>::max() / 2 + 1) / kRtpHz;

  // Avoid timeout being set while waiting for the frame and before the receiver
  // is ready.
  ResetLastResult();
  EXPECT_THAT(WaitForFrameOrTimeout(kMaxWaitForFrame), Eq(absl::nullopt));
  time_controller_.AdvanceTime(kRolloverDelay - kMaxWaitForFrame);
  StartNextDecode();
  buffer_->InsertFrame(test::FakeFrameBuilder()
                           .Id(2)
                           .Time(kRolloverRtp)
                           .ReceivedTime(clock_->CurrentTime())
                           .AsLast()
                           .Build());
  // FrameBuffer2 drops the frame, while FrameBuffer3 will continue the stream.
  EXPECT_THAT(WaitForFrameOrTimeout(kFps30Delay), Frame(test::WithId(2)));
}

INSTANTIATE_TEST_SUITE_P(
    VideoStreamBufferController,
    VideoStreamBufferControllerTest,
    ::testing::Values("WebRTC-FrameBuffer3/arm:FrameBuffer3/",
                      "WebRTC-FrameBuffer3/arm:SyncDecoding/"));

class LowLatencyVideoStreamBufferControllerTest
    : public ::testing::Test,
      public VideoStreamBufferControllerFixture {};

TEST_P(LowLatencyVideoStreamBufferControllerTest,
       FramesDecodedInstantlyWithLowLatencyRendering) {
  // Initial keyframe.
  StartNextDecodeForceKeyframe();
  timing_.set_min_playout_delay(TimeDelta::Zero());
  timing_.set_max_playout_delay(TimeDelta::Millis(10));
  auto frame = test::FakeFrameBuilder().Id(0).Time(0).AsLast().Build();
  // Playout delay of 0 implies low-latency rendering.
  frame->SetPlayoutDelay({0, 10});
  buffer_->InsertFrame(std::move(frame));
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(test::WithId(0)));

  // Delta frame would normally wait here, but should decode at the pacing rate
  // in low-latency mode.
  StartNextDecode();
  frame = test::FakeFrameBuilder().Id(1).Time(kFps30Rtp).AsLast().Build();
  frame->SetPlayoutDelay({0, 10});
  buffer_->InsertFrame(std::move(frame));
  // Pacing is set to 16ms in the field trial so we should not decode yet.
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Eq(absl::nullopt));
  time_controller_.AdvanceTime(TimeDelta::Millis(16));
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(test::WithId(1)));
}

TEST_P(LowLatencyVideoStreamBufferControllerTest, ZeroPlayoutDelayFullQueue) {
  // Initial keyframe.
  StartNextDecodeForceKeyframe();
  timing_.set_min_playout_delay(TimeDelta::Zero());
  timing_.set_max_playout_delay(TimeDelta::Millis(10));
  auto frame = test::FakeFrameBuilder().Id(0).Time(0).AsLast().Build();
  // Playout delay of 0 implies low-latency rendering.
  frame->SetPlayoutDelay({0, 10});
  buffer_->InsertFrame(std::move(frame));
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(test::WithId(0)));

  // Queue up 5 frames (configured max queue size for 0-playout delay pacing).
  for (int id = 1; id <= 6; ++id) {
    frame =
        test::FakeFrameBuilder().Id(id).Time(kFps30Rtp * id).AsLast().Build();
    frame->SetPlayoutDelay({0, 10});
    buffer_->InsertFrame(std::move(frame));
  }

  // The queue is at its max size for zero playout delay pacing, so the pacing
  // should be ignored and the next frame should be decoded instantly.
  StartNextDecode();
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(test::WithId(1)));
}

TEST_P(LowLatencyVideoStreamBufferControllerTest,
       MinMaxDelayZeroLowLatencyMode) {
  // Initial keyframe.
  StartNextDecodeForceKeyframe();
  timing_.set_min_playout_delay(TimeDelta::Zero());
  timing_.set_max_playout_delay(TimeDelta::Zero());
  auto frame = test::FakeFrameBuilder().Id(0).Time(0).AsLast().Build();
  // Playout delay of 0 implies low-latency rendering.
  frame->SetPlayoutDelay({0, 0});
  buffer_->InsertFrame(std::move(frame));
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(test::WithId(0)));

  // Delta frame would normally wait here, but should decode at the pacing rate
  // in low-latency mode.
  StartNextDecode();
  frame = test::FakeFrameBuilder().Id(1).Time(kFps30Rtp).AsLast().Build();
  frame->SetPlayoutDelay({0, 0});
  buffer_->InsertFrame(std::move(frame));
  // The min/max=0 version of low-latency rendering will result in a large
  // negative decode wait time, so the frame should be ready right away.
  EXPECT_THAT(WaitForFrameOrTimeout(TimeDelta::Zero()), Frame(test::WithId(1)));
}

INSTANTIATE_TEST_SUITE_P(
    VideoStreamBufferController,
    LowLatencyVideoStreamBufferControllerTest,
    ::testing::Values(
        "WebRTC-FrameBuffer3/arm:FrameBuffer3/"
        "WebRTC-ZeroPlayoutDelay/min_pacing:16ms,max_decode_queue_size:5/",
        "WebRTC-FrameBuffer3/arm:SyncDecoding/"
        "WebRTC-ZeroPlayoutDelay/min_pacing:16ms,max_decode_queue_size:5/"));

}  // namespace webrtc
