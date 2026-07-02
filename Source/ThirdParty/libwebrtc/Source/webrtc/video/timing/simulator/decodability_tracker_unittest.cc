/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/decodability_tracker.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "api/sequence_checker.h"
#include "api/video/encoded_frame.h"
#include "rtc_base/thread_annotations.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "video/timing/simulator/assembler.h"
#include "video/timing/simulator/test/encoded_frame_generators.h"
#include "video/timing/simulator/test/matchers.h"
#include "video/timing/simulator/test/simulated_time_test_fixture.h"

namespace webrtc::video_timing_simulator {
namespace {

using ::testing::Eq;
using ::testing::InSequence;
using ::testing::NiceMock;

class MockDecodabilityTrackerEvents : public DecodabilityTrackerEvents {
 public:
  MOCK_METHOD(void,
              OnDecodableFrame,
              (const EncodedFrame& decodable_frame),
              (override));
};

class MockDecodedFrameIdCallback : public DecodedFrameIdCallback {
 public:
  MOCK_METHOD(void, OnDecodedFrameId, (int64_t frame_id), (override));
};

class DecodabilityTrackerTest : public SimulatedTimeTestFixture {
 protected:
  DecodabilityTrackerTest() {
    SendTask([this]() {
      RTC_DCHECK_RUN_ON(queue_ptr_);
      decodability_tracker_ = std::make_unique<DecodabilityTracker>(
          env_,
          DecodabilityTracker::Config{.ssrc =
                                          EncodedFrameBuilderGenerator::kSsrc},
          &decodability_tracker_events_);
      decodability_tracker_->SetDecodedFrameIdCallback(&decoded_frame_id_cb_);
    });
  }
  ~DecodabilityTrackerTest() override {
    SendTask([this]() {
      RTC_DCHECK_RUN_ON(queue_ptr_);
      decodability_tracker_.reset();
    });
  }

  void OnAssembledFrame(std::unique_ptr<EncodedFrame> assembled_frame) {
    SendTask([this, assembled_frame = std::move(assembled_frame)]() mutable {
      RTC_DCHECK_RUN_ON(queue_ptr_);
      decodability_tracker_->OnAssembledFrame(std::move(assembled_frame));
    });
  }

  // Expectations.
  MockDecodabilityTrackerEvents decodability_tracker_events_;
  NiceMock<MockDecodedFrameIdCallback> decoded_frame_id_cb_;

  // Object under test.
  std::unique_ptr<DecodabilityTracker> decodability_tracker_
      RTC_PT_GUARDED_BY(queue_ptr_);
};

TEST_F(DecodabilityTrackerTest, KeyframeIsDecodable) {
  SingleLayerEncodedFrameGenerator generator(env_);
  auto keyframe = generator.NextEncodedFrame();

  EXPECT_THAT(keyframe->num_references, Eq(0));
  EXPECT_CALL(decodability_tracker_events_,
              OnDecodableFrame(EncodedFrameWithId(0)));
  EXPECT_CALL(decoded_frame_id_cb_, OnDecodedFrameId(0));
  OnAssembledFrame(std::move(keyframe));
}

TEST_F(DecodabilityTrackerTest, DeltaFrameIsNotDecodable) {
  SingleLayerEncodedFrameGenerator generator(env_);
  auto keyframe = generator.NextEncodedFrame();
  auto delta_frame = generator.NextEncodedFrame();

  EXPECT_THAT(delta_frame->num_references, Eq(1));
  EXPECT_CALL(decodability_tracker_events_,
              OnDecodableFrame(EncodedFrameWithId(0)))
      .Times(0);
  EXPECT_CALL(decoded_frame_id_cb_, OnDecodedFrameId(0)).Times(0);
  OnAssembledFrame(std::move(delta_frame));
}

TEST_F(DecodabilityTrackerTest, KeyframeAndDeltaFrameAreDecodable) {
  SingleLayerEncodedFrameGenerator generator(env_);
  auto keyframe = generator.NextEncodedFrame();
  auto delta_frame = generator.NextEncodedFrame();

  {
    InSequence seq;
    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(0)));
    EXPECT_CALL(decoded_frame_id_cb_, OnDecodedFrameId(0));
    OnAssembledFrame(std::move(keyframe));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(1)));
    EXPECT_CALL(decoded_frame_id_cb_, OnDecodedFrameId(1));
    OnAssembledFrame(std::move(delta_frame));
  }
}

TEST_F(DecodabilityTrackerTest, ReorderedKeyframeAndDeltaFrameAreDecodable) {
  SingleLayerEncodedFrameGenerator generator(env_);
  auto keyframe = generator.NextEncodedFrame();
  auto delta_frame = generator.NextEncodedFrame();

  {
    InSequence seq;
    OnAssembledFrame(std::move(delta_frame));
    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(0)));
    EXPECT_CALL(decoded_frame_id_cb_, OnDecodedFrameId(0));
    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(1)));
    EXPECT_CALL(decoded_frame_id_cb_, OnDecodedFrameId(1));
    OnAssembledFrame(std::move(keyframe));
  }
}

TEST_F(DecodabilityTrackerTest, OneTemporalLayerGoPIsDecodable) {
  TemporalLayersEncodedFrameGenerator generator(env_);
  auto keyframe = generator.NextEncodedFrame();
  auto tl2a = generator.NextEncodedFrame();
  auto tl1 = generator.NextEncodedFrame();
  auto tl2b = generator.NextEncodedFrame();

  {
    InSequence seq;
    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(0)));
    OnAssembledFrame(std::move(keyframe));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(1)));
    OnAssembledFrame(std::move(tl2a));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(2)));
    OnAssembledFrame(std::move(tl1));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(3)));
    OnAssembledFrame(std::move(tl2b));
  }
}

TEST_F(DecodabilityTrackerTest, TwoTemporalLayerGoPsAreDecodable) {
  TemporalLayersEncodedFrameGenerator generator(env_);
  {
    InSequence seq;
    for (int i = 0; i < 8; ++i) {
      EXPECT_CALL(decodability_tracker_events_,
                  OnDecodableFrame(EncodedFrameWithId(i)));
      OnAssembledFrame(generator.NextEncodedFrame());
    }
  }
}

// TODO: b/423646186 - Update this test when we handle reordered frames better.
TEST_F(DecodabilityTrackerTest, SkipsOverReorderedTl2A) {
  TemporalLayersEncodedFrameGenerator generator(env_);
  auto keyframe = generator.NextEncodedFrame();
  auto tl2a = generator.NextEncodedFrame();
  auto tl1 = generator.NextEncodedFrame();
  auto tl2b = generator.NextEncodedFrame();

  {
    InSequence seq;
    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(0)));
    OnAssembledFrame(std::move(keyframe));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(2)));
    OnAssembledFrame(std::move(tl1));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(1)))
        .Times(0);
    OnAssembledFrame(std::move(tl2a));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(3)));
    OnAssembledFrame(std::move(tl2b));
  }
}

TEST_F(DecodabilityTrackerTest, DoesNotSkipOverReorderedTl1) {
  TemporalLayersEncodedFrameGenerator generator(env_);
  auto keyframe = generator.NextEncodedFrame();
  auto tl2a = generator.NextEncodedFrame();
  auto tl1 = generator.NextEncodedFrame();
  auto tl2b = generator.NextEncodedFrame();

  {
    InSequence seq;
    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(0)));
    OnAssembledFrame(std::move(keyframe));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(1)));
    OnAssembledFrame(std::move(tl2a));

    OnAssembledFrame(std::move(tl2b));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(2)));
    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(3)));
    OnAssembledFrame(std::move(tl1));
  }
}

// TODO: b/423646186 - Update this test when we handle reordered frames better.
TEST_F(DecodabilityTrackerTest, SkipsOverReorderedTl2B) {
  TemporalLayersEncodedFrameGenerator generator(env_);
  auto keyframe = generator.NextEncodedFrame();
  auto tl2a = generator.NextEncodedFrame();
  auto tl1 = generator.NextEncodedFrame();
  auto tl2b = generator.NextEncodedFrame();
  auto tl0_next_gop = generator.NextEncodedFrame();

  {
    InSequence seq;
    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(0)));
    OnAssembledFrame(std::move(keyframe));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(1)));
    OnAssembledFrame(std::move(tl2a));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(2)));
    OnAssembledFrame(std::move(tl1));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(4)));
    OnAssembledFrame(std::move(tl0_next_gop));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(3)))
        .Times(0);
    OnAssembledFrame(std::move(tl2b));
  }
}

}  // namespace
}  // namespace webrtc::video_timing_simulator
