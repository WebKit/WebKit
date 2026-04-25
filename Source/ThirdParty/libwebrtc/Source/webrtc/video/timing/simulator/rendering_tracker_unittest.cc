/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/rendering_tracker.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "api/sequence_checker.h"
#include "api/units/time_delta.h"
#include "api/video/encoded_frame.h"
#include "api/video/video_frame.h"
#include "modules/video_coding/timing/timing.h"
#include "rtc_base/thread_annotations.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "video/timing/simulator/assembler.h"
#include "video/timing/simulator/test/encoded_frame_generators.h"
#include "video/timing/simulator/test/matchers.h"
#include "video/timing/simulator/test/simulated_time_test_fixture.h"

namespace webrtc::video_timing_simulator {
namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::NiceMock;

class MockRenderingTrackerEvents : public RenderingTrackerEvents {
 public:
  MOCK_METHOD(void,
              OnDecodedFrame,
              (const EncodedFrame& decoded_frame,
               int frames_dropped,
               TimeDelta jitter_buffer_minimum_delay,
               TimeDelta jitter_buffer_target_delay,
               TimeDelta jitter_buffer_delay),
              (override));
  MOCK_METHOD(void,
              OnRenderedFrame,
              (const VideoFrame& rendered_frame),
              (override));
};

class MockDecodedFrameIdCallback : public DecodedFrameIdCallback {
 public:
  MOCK_METHOD(void, OnDecodedFrameId, (int64_t frame_id), (override));
};

class RenderingTrackerTest : public SimulatedTimeTestFixture {
 protected:
  RenderingTrackerTest() : done_(false) {
    SendTask([this]() {
      RTC_DCHECK_RUN_ON(queue_ptr_);
      rendering_tracker_ = std::make_unique<RenderingTracker>(
          env_,
          RenderingTracker::Config{
              .ssrc = EncodedFrameBuilderGenerator::kSsrc,
              .render_delay = TimeDelta::Millis(10)},
          std::make_unique<VCMTiming>(&env_.clock(), env_.field_trials()),
          &rendering_tracker_events_);
      rendering_tracker_->SetDecodedFrameIdCallback(&decoded_frame_id_cb_);
    });
  }
  ~RenderingTrackerTest() override {
    SendTask([this]() {
      RTC_DCHECK_RUN_ON(queue_ptr_);
      rendering_tracker_.reset();
    });
  }

  void OnAssembledFrame(std::unique_ptr<EncodedFrame> assembled_frame) {
    SendTask([this, assembled_frame = std::move(assembled_frame)]() mutable {
      RTC_DCHECK_RUN_ON(queue_ptr_);
      rendering_tracker_->OnAssembledFrame(std::move(assembled_frame));
    });
  }

  // Expectations.
  NiceMock<MockRenderingTrackerEvents> rendering_tracker_events_;
  NiceMock<MockDecodedFrameIdCallback> decoded_frame_id_cb_;

  // Object under test.
  std::unique_ptr<RenderingTracker> rendering_tracker_
      RTC_PT_GUARDED_BY(queue_ptr_);

  // Wait synchronization.
  bool done_;
};

// TODO(brandtr): Check render callback as well!

TEST_F(RenderingTrackerTest, KeyframeIsRenderedImmediately) {
  SingleLayerEncodedFrameGenerator generator(env_);

  auto keyframe = generator.NextEncodedFrame();
  EXPECT_THAT(keyframe->num_references, Eq(0));
  EXPECT_CALL(decoded_frame_id_cb_, OnDecodedFrameId(0));
  EXPECT_CALL(rendering_tracker_events_, OnRenderedFrame(VideoFrameWithId(0)));
  OnAssembledFrame(std::move(keyframe));
  // No wait -> `OnRenderedFrame` was called upon immediately.
}

TEST_F(RenderingTrackerTest, DeltaFrameIsNeverRendered) {
  SingleLayerEncodedFrameGenerator generator(env_);

  auto keyframe = generator.NextEncodedFrame();

  time_controller_.AdvanceTime(TimeDelta::Millis(33));  // 30 fps.
  auto delta_frame = generator.NextEncodedFrame();
  EXPECT_THAT(delta_frame->num_references, Eq(1));
  EXPECT_CALL(decoded_frame_id_cb_, OnDecodedFrameId(_)).Times(0);
  EXPECT_CALL(rendering_tracker_events_, OnRenderedFrame(_)).Times(0);
  OnAssembledFrame(std::move(delta_frame));

  time_controller_.AdvanceTime(TimeDelta::Seconds(1));
}

TEST_F(RenderingTrackerTest, KeyframeAndDeltaFrameAreRendered) {
  SingleLayerEncodedFrameGenerator generator(env_);

  {
    InSequence seq;
    auto keyframe = generator.NextEncodedFrame();
    EXPECT_CALL(decoded_frame_id_cb_, OnDecodedFrameId(0));
    EXPECT_CALL(rendering_tracker_events_,
                OnRenderedFrame(VideoFrameWithId(0)));
    OnAssembledFrame(std::move(keyframe));

    time_controller_.AdvanceTime(TimeDelta::Millis(33));  // 30 fps.
    auto delta_frame = generator.NextEncodedFrame();
    EXPECT_CALL(decoded_frame_id_cb_, OnDecodedFrameId(1));
    EXPECT_CALL(rendering_tracker_events_, OnRenderedFrame(VideoFrameWithId(1)))
        .WillOnce([this] { done_ = true; });
    OnAssembledFrame(std::move(delta_frame));

    time_controller_.Wait([this] { return done_; });
  }
}

TEST_F(RenderingTrackerTest, ReorderedKeyframeAndDeltaFrameAreRendered) {
  SingleLayerEncodedFrameGenerator generator(env_);

  {
    InSequence seq;
    auto keyframe = generator.NextEncodedFrame();

    time_controller_.AdvanceTime(TimeDelta::Millis(33));  // 30 fps.
    auto delta_frame = generator.NextEncodedFrame();
    OnAssembledFrame(std::move(delta_frame));
    EXPECT_CALL(rendering_tracker_events_,
                OnRenderedFrame(VideoFrameWithId(0)));
    EXPECT_CALL(rendering_tracker_events_, OnRenderedFrame(VideoFrameWithId(1)))
        .WillOnce([this] { done_ = true; });
    OnAssembledFrame(std::move(keyframe));

    time_controller_.Wait([this] { return done_; });
  }
}

TEST_F(RenderingTrackerTest, TwoTemporalLayerGoPsAreRendered) {
  TemporalLayersEncodedFrameGenerator generator(env_);
  {
    InSequence seq;
    for (int i = 0; i < 7; ++i) {
      EXPECT_CALL(rendering_tracker_events_,
                  OnRenderedFrame(VideoFrameWithId(i)));
      OnAssembledFrame(generator.NextEncodedFrame());
      time_controller_.AdvanceTime(TimeDelta::Millis(33));  // 30 fps.
    }
    EXPECT_CALL(rendering_tracker_events_, OnRenderedFrame(VideoFrameWithId(7)))
        .WillOnce([this] { done_ = true; });
    OnAssembledFrame(generator.NextEncodedFrame());

    time_controller_.Wait([this] { return done_; });
  }
}

TEST_F(RenderingTrackerTest, DroppedFrameIsReported) {
  TemporalLayersEncodedFrameGenerator generator(env_);

  {
    InSequence seq;

    auto keyframe = generator.NextEncodedFrame();
    EXPECT_CALL(
        rendering_tracker_events_,
        OnDecodedFrame(EncodedFrameWithId(0), /*frames_dropped=*/0, _, _, _));
    OnAssembledFrame(std::move(keyframe));

    // `tl1` arrives on time.
    time_controller_.AdvanceTime(TimeDelta::Millis(66));  // 30 fps.
    auto tl2a = generator.NextEncodedFrame();
    auto tl1 = generator.NextEncodedFrame();
    EXPECT_CALL(
        rendering_tracker_events_,
        OnDecodedFrame(EncodedFrameWithId(2), /*frames_dropped=*/1, _, _, _));
    OnAssembledFrame(std::move(tl1));

    // `tl2a` is very delayed, meaning that it arrived too late to be decoded.
    // This was reported for `tl1`, since that frame _was_ decoded.
    EXPECT_CALL(rendering_tracker_events_,
                OnDecodedFrame(EncodedFrameWithId(1), _, _, _, _))
        .Times(0);
    OnAssembledFrame(std::move(tl2a));

    // `tl2b` arrives on time and is decoded.
    EXPECT_CALL(
        rendering_tracker_events_,
        OnDecodedFrame(EncodedFrameWithId(3), /*frames_dropped=*/0, _, _, _))
        .WillOnce([this] { done_ = true; });
    time_controller_.AdvanceTime(TimeDelta::Millis(33));  // 30 fps.
    auto tl2b = generator.NextEncodedFrame();
    OnAssembledFrame(std::move(tl2b));

    time_controller_.Wait([this] { return done_; });
  }
}

TEST_F(RenderingTrackerTest, JitterBufferStatsAreReported) {
  SingleLayerEncodedFrameGenerator generator(env_);

  // Prime the internal timing components.
  for (int i = 0; i < 30 * 10; ++i) {
    auto frame = generator.NextEncodedFrame();
    OnAssembledFrame(std::move(frame));
    time_controller_.AdvanceTime(TimeDelta::Millis(33));  // 30 fps.
  }

  // A frame that arrives on time will spend a bit of time in the buffer.
  auto on_time_frame = generator.NextEncodedFrame();
  EXPECT_CALL(
      rendering_tracker_events_,
      OnDecodedFrame(EncodedFrameWithId(300), _,
                     /*jitter_buffer_minimum_delay=*/TimeDelta::Millis(11), _,
                     /*jitter_buffer_delay=*/TimeDelta::Millis(11)));
  OnAssembledFrame(std::move(on_time_frame));

  // A frame that arrives late will spend correspondingly less time in the
  // buffer.
  time_controller_.AdvanceTime(TimeDelta::Millis(43));  // 10ms delayed.
  auto late_frame = generator.NextEncodedFrame();
  EXPECT_CALL(
      rendering_tracker_events_,
      OnDecodedFrame(EncodedFrameWithId(301), _,
                     /*jitter_buffer_minimum_delay=*/TimeDelta::Millis(11), _,
                     /*jitter_buffer_delay=*/TimeDelta::Millis(1)))
      .WillOnce([this] { done_ = true; });
  OnAssembledFrame(std::move(late_frame));

  time_controller_.Wait([this] { return done_; });
}

}  // namespace
}  // namespace webrtc::video_timing_simulator
