/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/frame_buffer2.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>

#include "modules/video_coding/frame_object.h"
#include "modules/video_coding/jitter_estimator.h"
#include "modules/video_coding/timing.h"
#include "rtc_base/numerics/sequence_number_util.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/random.h"
#include "system_wrappers/include/clock.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

using ::testing::_;
using ::testing::Return;

namespace webrtc {
namespace video_coding {

class VCMTimingFake : public VCMTiming {
 public:
  explicit VCMTimingFake(Clock* clock) : VCMTiming(clock) {}

  int64_t RenderTimeMs(uint32_t frame_timestamp,
                       int64_t now_ms) const override {
    if (last_ms_ == -1) {
      last_ms_ = now_ms + kDelayMs;
      last_timestamp_ = frame_timestamp;
    }

    uint32_t diff = MinDiff(frame_timestamp, last_timestamp_);
    if (AheadOf(frame_timestamp, last_timestamp_))
      last_ms_ += diff / 90;
    else
      last_ms_ -= diff / 90;

    last_timestamp_ = frame_timestamp;
    return last_ms_;
  }

  int64_t MaxWaitingTime(int64_t render_time_ms,
                         int64_t now_ms,
                         bool too_many_frames_queued) const override {
    return render_time_ms - now_ms - kDecodeTime;
  }

  bool GetTimings(int* max_decode_ms,
                  int* current_delay_ms,
                  int* target_delay_ms,
                  int* jitter_buffer_ms,
                  int* min_playout_delay_ms,
                  int* render_delay_ms) const override {
    return true;
  }

  int GetCurrentJitter() {
    int max_decode_ms;
    int current_delay_ms;
    int target_delay_ms;
    int jitter_buffer_ms;
    int min_playout_delay_ms;
    int render_delay_ms;
    VCMTiming::GetTimings(&max_decode_ms, &current_delay_ms, &target_delay_ms,
                          &jitter_buffer_ms, &min_playout_delay_ms,
                          &render_delay_ms);
    return jitter_buffer_ms;
  }

 private:
  static constexpr int kDelayMs = 50;
  static constexpr int kDecodeTime = kDelayMs / 2;
  mutable uint32_t last_timestamp_ = 0;
  mutable int64_t last_ms_ = -1;
};

class FrameObjectFake : public EncodedFrame {
 public:
  int64_t ReceivedTime() const override { return 0; }

  int64_t RenderTime() const override { return _renderTimeMs; }

  bool delayed_by_retransmission() const override {
    return delayed_by_retransmission_;
  }
  void set_delayed_by_retransmission(bool delayed) {
    delayed_by_retransmission_ = delayed;
  }

 private:
  bool delayed_by_retransmission_ = false;
};

class VCMReceiveStatisticsCallbackMock : public VCMReceiveStatisticsCallback {
 public:
  MOCK_METHOD(void,
              OnCompleteFrame,
              (bool is_keyframe,
               size_t size_bytes,
               VideoContentType content_type),
              (override));
  MOCK_METHOD(void, OnDroppedFrames, (uint32_t frames_dropped), (override));
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

class TestFrameBuffer2 : public ::testing::Test {
 protected:
  static constexpr int kMaxReferences = 5;
  static constexpr int kFps1 = 1000;
  static constexpr int kFps10 = kFps1 / 10;
  static constexpr int kFps20 = kFps1 / 20;
  static constexpr size_t kFrameSize = 10;

  TestFrameBuffer2()
      : trial_("WebRTC-AddRttToPlayoutDelay/Enabled/"),
        time_controller_(Timestamp::Seconds(0)),
        time_task_queue_(
            time_controller_.GetTaskQueueFactory()->CreateTaskQueue(
                "extract queue",
                TaskQueueFactory::Priority::NORMAL)),
        timing_(time_controller_.GetClock()),
        buffer_(new FrameBuffer(time_controller_.GetClock(),
                                &timing_,
                                &stats_callback_)),
        rand_(0x34678213) {}

  template <typename... T>
  std::unique_ptr<FrameObjectFake> CreateFrame(uint16_t picture_id,
                                               uint8_t spatial_layer,
                                               int64_t ts_ms,
                                               bool last_spatial_layer,
                                               size_t frame_size_bytes,
                                               T... refs) {
    static_assert(sizeof...(refs) <= kMaxReferences,
                  "To many references specified for EncodedFrame.");
    std::array<uint16_t, sizeof...(refs)> references = {
        {rtc::checked_cast<uint16_t>(refs)...}};

    auto frame = std::make_unique<FrameObjectFake>();
    frame->SetId(picture_id);
    frame->SetSpatialIndex(spatial_layer);
    frame->SetTimestamp(ts_ms * 90);
    frame->num_references = references.size();
    frame->is_last_spatial_layer = last_spatial_layer;
    // Add some data to buffer.
    frame->SetEncodedData(EncodedImageBuffer::Create(frame_size_bytes));
    for (size_t r = 0; r < references.size(); ++r)
      frame->references[r] = references[r];
    return frame;
  }

  template <typename... T>
  int InsertFrame(uint16_t picture_id,
                  uint8_t spatial_layer,
                  int64_t ts_ms,
                  bool last_spatial_layer,
                  size_t frame_size_bytes,
                  T... refs) {
    return buffer_->InsertFrame(CreateFrame(picture_id, spatial_layer, ts_ms,
                                            last_spatial_layer,
                                            frame_size_bytes, refs...));
  }

  int InsertNackedFrame(uint16_t picture_id, int64_t ts_ms) {
    std::unique_ptr<FrameObjectFake> frame =
        CreateFrame(picture_id, 0, ts_ms, true, kFrameSize);
    frame->set_delayed_by_retransmission(true);
    return buffer_->InsertFrame(std::move(frame));
  }

  void ExtractFrame(int64_t max_wait_time = 0, bool keyframe_required = false) {
    time_task_queue_.PostTask([this, max_wait_time, keyframe_required]() {
      buffer_->NextFrame(
          max_wait_time, keyframe_required, &time_task_queue_,
          [this](std::unique_ptr<EncodedFrame> frame,
                 video_coding::FrameBuffer::ReturnReason reason) {
            if (reason != FrameBuffer::ReturnReason::kStopped) {
              frames_.emplace_back(std::move(frame));
            }
          });
    });
    if (max_wait_time == 0) {
      time_controller_.AdvanceTime(TimeDelta::Millis(0));
    }
  }

  void CheckFrame(size_t index, int picture_id, int spatial_layer) {
    ASSERT_LT(index, frames_.size());
    ASSERT_TRUE(frames_[index]);
    ASSERT_EQ(picture_id, frames_[index]->Id());
    ASSERT_EQ(spatial_layer, frames_[index]->SpatialIndex().value_or(0));
  }

  void CheckFrameSize(size_t index, size_t size) {
    ASSERT_LT(index, frames_.size());
    ASSERT_TRUE(frames_[index]);
    ASSERT_EQ(frames_[index]->size(), size);
  }

  void CheckNoFrame(size_t index) {
    ASSERT_LT(index, frames_.size());
    ASSERT_FALSE(frames_[index]);
  }

  uint32_t Rand() { return rand_.Rand<uint32_t>(); }

  // The ProtectionMode tests depends on rtt-multiplier experiment.
  test::ScopedFieldTrials trial_;
  webrtc::GlobalSimulatedTimeController time_controller_;
  rtc::TaskQueue time_task_queue_;
  VCMTimingFake timing_;
  std::unique_ptr<FrameBuffer> buffer_;
  std::vector<std::unique_ptr<EncodedFrame>> frames_;
  Random rand_;
  ::testing::NiceMock<VCMReceiveStatisticsCallbackMock> stats_callback_;
};

// From https://en.cppreference.com/w/cpp/language/static: "If ... a constexpr
// static data member (since C++11) is odr-used, a definition at namespace scope
// is still required... This definition is deprecated for constexpr data members
// since C++17."
// kFrameSize is odr-used since it is passed by reference to EXPECT_EQ().
#if __cplusplus < 201703L
constexpr size_t TestFrameBuffer2::kFrameSize;
#endif

TEST_F(TestFrameBuffer2, WaitForFrame) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  ExtractFrame(50);
  InsertFrame(pid, 0, ts, true, kFrameSize);
  time_controller_.AdvanceTime(TimeDelta::Millis(50));
  CheckFrame(0, pid, 0);
}

TEST_F(TestFrameBuffer2, OneSuperFrame) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  InsertFrame(pid, 0, ts, false, kFrameSize);
  InsertFrame(pid + 1, 1, ts, true, kFrameSize);
  ExtractFrame();

  CheckFrame(0, pid, 1);
}

TEST_F(TestFrameBuffer2, ZeroPlayoutDelay) {
  VCMTiming timing(time_controller_.GetClock());
  buffer_.reset(
      new FrameBuffer(time_controller_.GetClock(), &timing, &stats_callback_));
  const VideoPlayoutDelay kPlayoutDelayMs = {0, 0};
  std::unique_ptr<FrameObjectFake> test_frame(new FrameObjectFake());
  test_frame->SetId(0);
  test_frame->SetPlayoutDelay(kPlayoutDelayMs);
  buffer_->InsertFrame(std::move(test_frame));
  ExtractFrame(0, false);
  CheckFrame(0, 0, 0);
  EXPECT_EQ(0, frames_[0]->RenderTimeMs());
}

// Flaky test, see bugs.webrtc.org/7068.
TEST_F(TestFrameBuffer2, DISABLED_OneUnorderedSuperFrame) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  ExtractFrame(50);
  InsertFrame(pid, 1, ts, true, kFrameSize);
  InsertFrame(pid, 0, ts, false, kFrameSize);
  time_controller_.AdvanceTime(TimeDelta::Millis(0));

  CheckFrame(0, pid, 0);
  CheckFrame(1, pid, 1);
}

TEST_F(TestFrameBuffer2, DISABLED_OneLayerStreamReordered) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  InsertFrame(pid, 0, ts, false, true, kFrameSize);
  ExtractFrame();
  CheckFrame(0, pid, 0);
  for (int i = 1; i < 10; i += 2) {
    ExtractFrame(50);
    InsertFrame(pid + i + 1, 0, ts + (i + 1) * kFps10, true, kFrameSize,
                pid + i);
    time_controller_.AdvanceTime(TimeDelta::Millis(kFps10));
    InsertFrame(pid + i, 0, ts + i * kFps10, true, kFrameSize, pid + i - 1);
    time_controller_.AdvanceTime(TimeDelta::Millis(kFps10));
    ExtractFrame();
    CheckFrame(i, pid + i, 0);
    CheckFrame(i + 1, pid + i + 1, 0);
  }
}

TEST_F(TestFrameBuffer2, ExtractFromEmptyBuffer) {
  ExtractFrame();
  CheckNoFrame(0);
}

TEST_F(TestFrameBuffer2, MissingFrame) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  InsertFrame(pid, 0, ts, true, kFrameSize);
  InsertFrame(pid + 2, 0, ts, true, kFrameSize, pid);
  InsertFrame(pid + 3, 0, ts, true, kFrameSize, pid + 1, pid + 2);
  ExtractFrame();
  ExtractFrame();
  ExtractFrame();

  CheckFrame(0, pid, 0);
  CheckFrame(1, pid + 2, 0);
  CheckNoFrame(2);
}

TEST_F(TestFrameBuffer2, OneLayerStream) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  InsertFrame(pid, 0, ts, true, kFrameSize);
  ExtractFrame();
  CheckFrame(0, pid, 0);
  for (int i = 1; i < 10; ++i) {
    InsertFrame(pid + i, 0, ts + i * kFps10, true, kFrameSize, pid + i - 1);
    ExtractFrame();
    time_controller_.AdvanceTime(TimeDelta::Millis(kFps10));
    CheckFrame(i, pid + i, 0);
  }
}

TEST_F(TestFrameBuffer2, DropTemporalLayerSlowDecoder) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  InsertFrame(pid, 0, ts, true, kFrameSize);
  InsertFrame(pid + 1, 0, ts + kFps20, true, kFrameSize, pid);
  for (int i = 2; i < 10; i += 2) {
    uint32_t ts_tl0 = ts + i / 2 * kFps10;
    InsertFrame(pid + i, 0, ts_tl0, true, kFrameSize, pid + i - 2);
    InsertFrame(pid + i + 1, 0, ts_tl0 + kFps20, true, kFrameSize, pid + i,
                pid + i - 1);
  }

  EXPECT_CALL(stats_callback_, OnDroppedFrames(1)).Times(3);

  for (int i = 0; i < 10; ++i) {
    ExtractFrame();
    time_controller_.AdvanceTime(TimeDelta::Millis(70));
  }

  CheckFrame(0, pid, 0);
  CheckFrame(1, pid + 1, 0);
  CheckFrame(2, pid + 2, 0);
  CheckFrame(3, pid + 4, 0);
  CheckFrame(4, pid + 6, 0);
  CheckFrame(5, pid + 8, 0);
  CheckNoFrame(6);
  CheckNoFrame(7);
  CheckNoFrame(8);
  CheckNoFrame(9);
}

TEST_F(TestFrameBuffer2, DropFramesIfSystemIsStalled) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  InsertFrame(pid, 0, ts, true, kFrameSize);
  InsertFrame(pid + 1, 0, ts + 1 * kFps10, true, kFrameSize, pid);
  InsertFrame(pid + 2, 0, ts + 2 * kFps10, true, kFrameSize, pid + 1);
  InsertFrame(pid + 3, 0, ts + 3 * kFps10, true, kFrameSize);

  ExtractFrame();
  // Jump forward in time, simulating the system being stalled for some reason.
  time_controller_.AdvanceTime(TimeDelta::Millis(3) * kFps10);
  // Extract one more frame, expect second and third frame to be dropped.
  EXPECT_CALL(stats_callback_, OnDroppedFrames(2)).Times(1);
  ExtractFrame();

  CheckFrame(0, pid + 0, 0);
  CheckFrame(1, pid + 3, 0);
}

TEST_F(TestFrameBuffer2, DroppedFramesCountedOnClear) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  InsertFrame(pid, 0, ts, true, kFrameSize);
  for (int i = 1; i < 5; ++i) {
    InsertFrame(pid + i, 0, ts + i * kFps10, true, kFrameSize, pid + i - 1);
  }

  // All frames should be dropped when Clear is called.
  EXPECT_CALL(stats_callback_, OnDroppedFrames(5)).Times(1);
  buffer_->Clear();
}

TEST_F(TestFrameBuffer2, InsertLateFrame) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  InsertFrame(pid, 0, ts, true, kFrameSize);
  ExtractFrame();
  InsertFrame(pid + 2, 0, ts, true, kFrameSize);
  ExtractFrame();
  InsertFrame(pid + 1, 0, ts, true, kFrameSize, pid);
  ExtractFrame();

  CheckFrame(0, pid, 0);
  CheckFrame(1, pid + 2, 0);
  CheckNoFrame(2);
}

TEST_F(TestFrameBuffer2, ProtectionModeNackFEC) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();
  constexpr int64_t kRttMs = 200;
  buffer_->UpdateRtt(kRttMs);

  // Jitter estimate unaffected by RTT in this protection mode.
  buffer_->SetProtectionMode(kProtectionNackFEC);
  InsertNackedFrame(pid, ts);
  InsertNackedFrame(pid + 1, ts + 100);
  InsertNackedFrame(pid + 2, ts + 200);
  InsertFrame(pid + 3, 0, ts + 300, true, kFrameSize);
  ExtractFrame();
  ExtractFrame();
  ExtractFrame();
  ExtractFrame();
  ASSERT_EQ(4u, frames_.size());
  EXPECT_LT(timing_.GetCurrentJitter(), kRttMs);
}

TEST_F(TestFrameBuffer2, ProtectionModeNack) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();
  constexpr int64_t kRttMs = 200;

  buffer_->UpdateRtt(kRttMs);

  // Jitter estimate includes RTT (after 3 retransmitted packets)
  buffer_->SetProtectionMode(kProtectionNack);
  InsertNackedFrame(pid, ts);
  InsertNackedFrame(pid + 1, ts + 100);
  InsertNackedFrame(pid + 2, ts + 200);
  InsertFrame(pid + 3, 0, ts + 300, true, kFrameSize);
  ExtractFrame();
  ExtractFrame();
  ExtractFrame();
  ExtractFrame();
  ASSERT_EQ(4u, frames_.size());

  EXPECT_GT(timing_.GetCurrentJitter(), kRttMs);
}

TEST_F(TestFrameBuffer2, NoContinuousFrame) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  EXPECT_EQ(-1, InsertFrame(pid + 1, 0, ts, true, kFrameSize, pid));
}

TEST_F(TestFrameBuffer2, LastContinuousFrameSingleLayer) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  EXPECT_EQ(pid, InsertFrame(pid, 0, ts, true, kFrameSize));
  EXPECT_EQ(pid, InsertFrame(pid + 2, 0, ts, true, kFrameSize, pid + 1));
  EXPECT_EQ(pid + 2, InsertFrame(pid + 1, 0, ts, true, kFrameSize, pid));
  EXPECT_EQ(pid + 2, InsertFrame(pid + 4, 0, ts, true, kFrameSize, pid + 3));
  EXPECT_EQ(pid + 5, InsertFrame(pid + 5, 0, ts, true, kFrameSize));
}

TEST_F(TestFrameBuffer2, LastContinuousFrameTwoLayers) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  EXPECT_EQ(pid, InsertFrame(pid, 0, ts, false, kFrameSize));
  EXPECT_EQ(pid + 1, InsertFrame(pid + 1, 1, ts, true, kFrameSize));
  EXPECT_EQ(pid + 1,
            InsertFrame(pid + 3, 1, ts, true, kFrameSize, pid + 1, pid + 2));
  EXPECT_EQ(pid + 1, InsertFrame(pid + 4, 0, ts, false, kFrameSize, pid + 2));
  EXPECT_EQ(pid + 1,
            InsertFrame(pid + 5, 1, ts, true, kFrameSize, pid + 3, pid + 4));
  EXPECT_EQ(pid + 1, InsertFrame(pid + 6, 0, ts, false, kFrameSize, pid + 4));
  EXPECT_EQ(pid + 6, InsertFrame(pid + 2, 0, ts, false, kFrameSize, pid));
  EXPECT_EQ(pid + 7,
            InsertFrame(pid + 7, 1, ts, true, kFrameSize, pid + 5, pid + 6));
}

TEST_F(TestFrameBuffer2, PictureIdJumpBack) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  EXPECT_EQ(pid, InsertFrame(pid, 0, ts, true, kFrameSize));
  EXPECT_EQ(pid + 1, InsertFrame(pid + 1, 0, ts + 1, true, kFrameSize, pid));
  ExtractFrame();
  CheckFrame(0, pid, 0);

  // Jump back in pid but increase ts.
  EXPECT_EQ(pid - 1, InsertFrame(pid - 1, 0, ts + 2, true, kFrameSize));
  ExtractFrame();
  ExtractFrame();
  CheckFrame(1, pid - 1, 0);
  CheckNoFrame(2);
}

TEST_F(TestFrameBuffer2, StatsCallback) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();
  const int kFrameSize = 5000;

  EXPECT_CALL(stats_callback_,
              OnCompleteFrame(true, kFrameSize, VideoContentType::UNSPECIFIED));
  EXPECT_CALL(stats_callback_, OnFrameBufferTimingsUpdated(_, _, _, _, _, _));

  {
    std::unique_ptr<FrameObjectFake> frame(new FrameObjectFake());
    frame->SetEncodedData(EncodedImageBuffer::Create(kFrameSize));
    frame->SetId(pid);
    frame->SetTimestamp(ts);
    frame->num_references = 0;

    EXPECT_EQ(buffer_->InsertFrame(std::move(frame)), pid);
  }

  ExtractFrame();
  CheckFrame(0, pid, 0);
}

TEST_F(TestFrameBuffer2, ForwardJumps) {
  EXPECT_EQ(5453, InsertFrame(5453, 0, 1, true, kFrameSize));
  ExtractFrame();
  EXPECT_EQ(5454, InsertFrame(5454, 0, 1, true, kFrameSize, 5453));
  ExtractFrame();
  EXPECT_EQ(15670, InsertFrame(15670, 0, 1, true, kFrameSize));
  ExtractFrame();
  EXPECT_EQ(29804, InsertFrame(29804, 0, 1, true, kFrameSize));
  ExtractFrame();
  EXPECT_EQ(29805, InsertFrame(29805, 0, 1, true, kFrameSize, 29804));
  ExtractFrame();
  EXPECT_EQ(29806, InsertFrame(29806, 0, 1, true, kFrameSize, 29805));
  ExtractFrame();
  EXPECT_EQ(33819, InsertFrame(33819, 0, 1, true, kFrameSize));
  ExtractFrame();
  EXPECT_EQ(41248, InsertFrame(41248, 0, 1, true, kFrameSize));
  ExtractFrame();
}

TEST_F(TestFrameBuffer2, DuplicateFrames) {
  EXPECT_EQ(22256, InsertFrame(22256, 0, 1, true, kFrameSize));
  ExtractFrame();
  EXPECT_EQ(22256, InsertFrame(22256, 0, 1, true, kFrameSize));
}

// TODO(philipel): implement more unittests related to invalid references.
TEST_F(TestFrameBuffer2, InvalidReferences) {
  EXPECT_EQ(-1, InsertFrame(0, 0, 1000, true, kFrameSize, 2));
  EXPECT_EQ(1, InsertFrame(1, 0, 2000, true, kFrameSize));
  ExtractFrame();
  EXPECT_EQ(2, InsertFrame(2, 0, 3000, true, kFrameSize, 1));
}

TEST_F(TestFrameBuffer2, KeyframeRequired) {
  EXPECT_EQ(1, InsertFrame(1, 0, 1000, true, kFrameSize));
  EXPECT_EQ(2, InsertFrame(2, 0, 2000, true, kFrameSize, 1));
  EXPECT_EQ(3, InsertFrame(3, 0, 3000, true, kFrameSize));
  ExtractFrame();
  ExtractFrame(0, true);
  ExtractFrame();

  CheckFrame(0, 1, 0);
  CheckFrame(1, 3, 0);
  CheckNoFrame(2);
}

TEST_F(TestFrameBuffer2, KeyframeClearsFullBuffer) {
  const int kMaxBufferSize = 600;

  for (int i = 1; i <= kMaxBufferSize; ++i)
    EXPECT_EQ(-1, InsertFrame(i, 0, i * 1000, true, kFrameSize, i - 1));
  ExtractFrame();
  CheckNoFrame(0);

  EXPECT_EQ(kMaxBufferSize + 1,
            InsertFrame(kMaxBufferSize + 1, 0, (kMaxBufferSize + 1) * 1000,
                        true, kFrameSize));
  ExtractFrame();
  CheckFrame(1, kMaxBufferSize + 1, 0);
}

TEST_F(TestFrameBuffer2, DontUpdateOnUndecodableFrame) {
  InsertFrame(1, 0, 0, true, kFrameSize);
  ExtractFrame(0, true);
  InsertFrame(3, 0, 0, true, kFrameSize, 2, 0);
  InsertFrame(3, 0, 0, true, kFrameSize, 0);
  InsertFrame(2, 0, 0, true, kFrameSize);
  ExtractFrame(0, true);
  ExtractFrame(0, true);
}

TEST_F(TestFrameBuffer2, DontDecodeOlderTimestamp) {
  InsertFrame(2, 0, 1, true, kFrameSize);
  InsertFrame(1, 0, 2, true,
              kFrameSize);  // Older picture id but newer timestamp.
  ExtractFrame(0);
  ExtractFrame(0);
  CheckFrame(0, 1, 0);
  CheckNoFrame(1);

  InsertFrame(3, 0, 4, true, kFrameSize);
  InsertFrame(4, 0, 3, true,
              kFrameSize);  // Newer picture id but older timestamp.
  ExtractFrame(0);
  ExtractFrame(0);
  CheckFrame(2, 3, 0);
  CheckNoFrame(3);
}

TEST_F(TestFrameBuffer2, CombineFramesToSuperframe) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  InsertFrame(pid, 0, ts, false, kFrameSize);
  InsertFrame(pid + 1, 1, ts, true, 2 * kFrameSize, pid);
  ExtractFrame(0);
  ExtractFrame(0);
  CheckFrame(0, pid, 1);
  CheckNoFrame(1);
  // Two frames should be combined and returned together.
  CheckFrameSize(0, 3 * kFrameSize);

  EXPECT_EQ(frames_[0]->SpatialIndex(), 1);
  EXPECT_EQ(frames_[0]->SpatialLayerFrameSize(0), kFrameSize);
  EXPECT_EQ(frames_[0]->SpatialLayerFrameSize(1), 2 * kFrameSize);
}

TEST_F(TestFrameBuffer2, HigherSpatialLayerNonDecodable) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  InsertFrame(pid, 0, ts, false, kFrameSize);
  InsertFrame(pid + 1, 1, ts, true, kFrameSize, pid);

  ExtractFrame(0);
  CheckFrame(0, pid, 1);

  InsertFrame(pid + 3, 1, ts + kFps20, true, kFrameSize, pid);
  InsertFrame(pid + 4, 0, ts + kFps10, false, kFrameSize, pid);
  InsertFrame(pid + 5, 1, ts + kFps10, true, kFrameSize, pid + 3, pid + 4);

  time_controller_.AdvanceTime(TimeDelta::Millis(1000));
  // Frame pid+3 is decodable but too late.
  // In superframe pid+4 is decodable, but frame pid+5 is not.
  // Incorrect implementation might skip pid+2 frame and output undecodable
  // pid+5 instead.
  ExtractFrame();
  ExtractFrame();
  CheckFrame(1, pid + 3, 1);
  CheckFrame(2, pid + 4, 1);
}

}  // namespace video_coding
}  // namespace webrtc
