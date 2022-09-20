/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/frame_decode_timing.h"

#include <stdint.h>

#include "absl/types/optional.h"
#include "api/units/time_delta.h"
#include "modules/video_coding/timing/timing.h"
#include "rtc_base/containers/flat_map.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/scoped_key_value_config.h"
#include "video/video_receive_stream2.h"

namespace webrtc {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Optional;

namespace {

class FakeVCMTiming : public webrtc::VCMTiming {
 public:
  explicit FakeVCMTiming(Clock* clock, const FieldTrialsView& field_trials)
      : webrtc::VCMTiming(clock, field_trials) {}

  Timestamp RenderTime(uint32_t frame_timestamp, Timestamp now) const override {
    RTC_DCHECK(render_time_map_.contains(frame_timestamp));
    auto it = render_time_map_.find(frame_timestamp);
    return it->second;
  }

  TimeDelta MaxWaitingTime(Timestamp render_time,
                           Timestamp now,
                           bool too_many_frames_queued) const override {
    RTC_DCHECK(wait_time_map_.contains(render_time));
    auto it = wait_time_map_.find(render_time);
    return it->second;
  }

  void SetTimes(uint32_t frame_timestamp,
                Timestamp render_time,
                TimeDelta max_decode_wait) {
    render_time_map_.insert_or_assign(frame_timestamp, render_time);
    wait_time_map_.insert_or_assign(render_time, max_decode_wait);
  }

 protected:
  flat_map<uint32_t, Timestamp> render_time_map_;
  flat_map<Timestamp, TimeDelta> wait_time_map_;
};
}  // namespace

class FrameDecodeTimingTest : public ::testing::Test {
 public:
  FrameDecodeTimingTest()
      : clock_(Timestamp::Millis(1000)),
        timing_(&clock_, field_trials_),
        frame_decode_scheduler_(&clock_, &timing_) {}

 protected:
  test::ScopedKeyValueConfig field_trials_;
  SimulatedClock clock_;
  FakeVCMTiming timing_;
  FrameDecodeTiming frame_decode_scheduler_;
};

TEST_F(FrameDecodeTimingTest, ReturnsWaitTimesWhenValid) {
  const TimeDelta decode_delay = TimeDelta::Millis(42);
  const Timestamp render_time = clock_.CurrentTime() + TimeDelta::Millis(60);
  timing_.SetTimes(90000, render_time, decode_delay);

  EXPECT_THAT(frame_decode_scheduler_.OnFrameBufferUpdated(
                  90000, 180000, kMaxWaitForFrame, false),
              Optional(AllOf(
                  Field(&FrameDecodeTiming::FrameSchedule::latest_decode_time,
                        Eq(clock_.CurrentTime() + decode_delay)),
                  Field(&FrameDecodeTiming::FrameSchedule::render_time,
                        Eq(render_time)))));
}

TEST_F(FrameDecodeTimingTest, FastForwardsFrameTooFarInThePast) {
  const TimeDelta decode_delay =
      -FrameDecodeTiming::kMaxAllowedFrameDelay - TimeDelta::Millis(1);
  const Timestamp render_time = clock_.CurrentTime();
  timing_.SetTimes(90000, render_time, decode_delay);

  EXPECT_THAT(frame_decode_scheduler_.OnFrameBufferUpdated(
                  90000, 180000, kMaxWaitForFrame, false),
              Eq(absl::nullopt));
}

TEST_F(FrameDecodeTimingTest, NoFastForwardIfOnlyFrameToDecode) {
  const TimeDelta decode_delay =
      -FrameDecodeTiming::kMaxAllowedFrameDelay - TimeDelta::Millis(1);
  const Timestamp render_time = clock_.CurrentTime();
  timing_.SetTimes(90000, render_time, decode_delay);

  // Negative `decode_delay` means that `latest_decode_time` is now.
  EXPECT_THAT(frame_decode_scheduler_.OnFrameBufferUpdated(
                  90000, 90000, kMaxWaitForFrame, false),
              Optional(AllOf(
                  Field(&FrameDecodeTiming::FrameSchedule::latest_decode_time,
                        Eq(clock_.CurrentTime())),
                  Field(&FrameDecodeTiming::FrameSchedule::render_time,
                        Eq(render_time)))));
}

TEST_F(FrameDecodeTimingTest, MaxWaitCapped) {
  TimeDelta frame_delay = TimeDelta::Millis(30);
  const TimeDelta decode_delay = TimeDelta::Seconds(3);
  const Timestamp render_time = clock_.CurrentTime() + TimeDelta::Seconds(3);
  timing_.SetTimes(90000, render_time, decode_delay);
  timing_.SetTimes(180000, render_time + frame_delay,
                   decode_delay + frame_delay);

  EXPECT_THAT(frame_decode_scheduler_.OnFrameBufferUpdated(
                  90000, 270000, kMaxWaitForFrame, false),
              Optional(AllOf(
                  Field(&FrameDecodeTiming::FrameSchedule::latest_decode_time,
                        Eq(clock_.CurrentTime() + kMaxWaitForFrame)),
                  Field(&FrameDecodeTiming::FrameSchedule::render_time,
                        Eq(render_time)))));

  // Test cap keyframe.
  clock_.AdvanceTime(frame_delay);
  EXPECT_THAT(frame_decode_scheduler_.OnFrameBufferUpdated(
                  180000, 270000, kMaxWaitForKeyFrame, false),
              Optional(AllOf(
                  Field(&FrameDecodeTiming::FrameSchedule::latest_decode_time,
                        Eq(clock_.CurrentTime() + kMaxWaitForKeyFrame)),
                  Field(&FrameDecodeTiming::FrameSchedule::render_time,
                        Eq(render_time + frame_delay)))));
}

}  // namespace webrtc
