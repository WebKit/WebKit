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
#include <vector>

#include "modules/video_coding/frame_object.h"
#include "modules/video_coding/jitter_estimator.h"
#include "modules/video_coding/timing.h"
#include "rtc_base/numerics/sequence_number_util.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/random.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

using testing::_;
using testing::Return;

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
                         int64_t now_ms) const override {
    return render_time_ms - now_ms - kDecodeTime;
  }

  bool GetTimings(int* decode_ms,
                  int* max_decode_ms,
                  int* current_delay_ms,
                  int* target_delay_ms,
                  int* jitter_buffer_ms,
                  int* min_playout_delay_ms,
                  int* render_delay_ms) const override {
    return true;
  }

 private:
  static constexpr int kDelayMs = 50;
  static constexpr int kDecodeTime = kDelayMs / 2;
  mutable uint32_t last_timestamp_ = 0;
  mutable int64_t last_ms_ = -1;
};

class VCMJitterEstimatorMock : public VCMJitterEstimator {
 public:
  explicit VCMJitterEstimatorMock(Clock* clock) : VCMJitterEstimator(clock) {}

  MOCK_METHOD1(UpdateRtt, void(int64_t rttMs));
  MOCK_METHOD3(UpdateEstimate,
               void(int64_t frameDelayMs,
                    uint32_t frameSizeBytes,
                    bool incompleteFrame));
  MOCK_METHOD1(GetJitterEstimate, int(double rttMultiplier));
};

class FrameObjectFake : public EncodedFrame {
 public:
  bool GetBitstream(uint8_t* destination) const override { return true; }

  int64_t ReceivedTime() const override { return 0; }

  int64_t RenderTime() const override { return _renderTimeMs; }

  // In EncodedImage |_length| is used to descibe its size and |_size| to
  // describe its capacity.
  void SetSize(int size) { _length = size; }
};

class VCMReceiveStatisticsCallbackMock : public VCMReceiveStatisticsCallback {
 public:
  MOCK_METHOD2(OnReceiveRatesUpdated,
               void(uint32_t bitRate, uint32_t frameRate));
  MOCK_METHOD3(OnCompleteFrame,
               void(bool is_keyframe,
                    size_t size_bytes,
                    VideoContentType content_type));
  MOCK_METHOD1(OnDiscardedPacketsUpdated, void(int discarded_packets));
  MOCK_METHOD1(OnFrameCountsUpdated, void(const FrameCounts& frame_counts));
  MOCK_METHOD7(OnFrameBufferTimingsUpdated,
               void(int decode_ms,
                    int max_decode_ms,
                    int current_delay_ms,
                    int target_delay_ms,
                    int jitter_buffer_ms,
                    int min_playout_delay_ms,
                    int render_delay_ms));
  MOCK_METHOD1(OnTimingFrameInfoUpdated, void(const TimingFrameInfo& info));
};

class TestFrameBuffer2 : public ::testing::Test {
 protected:
  static constexpr int kMaxReferences = 5;
  static constexpr int kFps1 = 1000;
  static constexpr int kFps10 = kFps1 / 10;
  static constexpr int kFps20 = kFps1 / 20;
  static constexpr size_t kFrameSize = 10;

  TestFrameBuffer2()
      : clock_(0),
        timing_(&clock_),
        jitter_estimator_(&clock_),
        buffer_(new FrameBuffer(&clock_,
                                &jitter_estimator_,
                                &timing_,
                                &stats_callback_)),
        rand_(0x34678213),
        tear_down_(false),
        extract_thread_(&ExtractLoop, this, "Extract Thread") {}

  void SetUp() override { extract_thread_.Start(); }

  void TearDown() override {
    tear_down_ = true;
    trigger_extract_event_.Set();
    extract_thread_.Stop();
  }

  template <typename... T>
  int InsertFrame(uint16_t picture_id,
                  uint8_t spatial_layer,
                  int64_t ts_ms,
                  bool inter_layer_predicted,
                  bool last_spatial_layer,
                  T... refs) {
    static_assert(sizeof...(refs) <= kMaxReferences,
                  "To many references specified for EncodedFrame.");
    std::array<uint16_t, sizeof...(refs)> references = {
        {rtc::checked_cast<uint16_t>(refs)...}};

    std::unique_ptr<FrameObjectFake> frame(new FrameObjectFake());
    frame->id.picture_id = picture_id;
    frame->id.spatial_layer = spatial_layer;
    frame->SetTimestamp(ts_ms * 90);
    frame->num_references = references.size();
    frame->inter_layer_predicted = inter_layer_predicted;
    frame->is_last_spatial_layer = last_spatial_layer;
    // Add some data to buffer.
    frame->VerifyAndAllocate(kFrameSize);
    frame->SetSize(kFrameSize);
    for (size_t r = 0; r < references.size(); ++r)
      frame->references[r] = references[r];

    return buffer_->InsertFrame(std::move(frame));
  }

  void ExtractFrame(int64_t max_wait_time = 0, bool keyframe_required = false) {
    crit_.Enter();
    if (max_wait_time == 0) {
      std::unique_ptr<EncodedFrame> frame;
      FrameBuffer::ReturnReason res =
          buffer_->NextFrame(0, &frame, keyframe_required);
      if (res != FrameBuffer::ReturnReason::kStopped)
        frames_.emplace_back(std::move(frame));
      crit_.Leave();
    } else {
      max_wait_time_ = max_wait_time;
      trigger_extract_event_.Set();
      crit_.Leave();
      // Make sure |crit_| is aquired by |extract_thread_| before returning.
      crit_acquired_event_.Wait(rtc::Event::kForever);
    }
  }

  void CheckFrame(size_t index, int picture_id, int spatial_layer) {
    rtc::CritScope lock(&crit_);
    ASSERT_LT(index, frames_.size());
    ASSERT_TRUE(frames_[index]);
    ASSERT_EQ(picture_id, frames_[index]->id.picture_id);
    ASSERT_EQ(spatial_layer, frames_[index]->id.spatial_layer);
  }

  void CheckFrameSize(size_t index, size_t size) {
    rtc::CritScope lock(&crit_);
    ASSERT_LT(index, frames_.size());
    ASSERT_TRUE(frames_[index]);
    ASSERT_EQ(frames_[index]->size(), size);
  }

  void CheckNoFrame(size_t index) {
    rtc::CritScope lock(&crit_);
    ASSERT_LT(index, frames_.size());
    ASSERT_FALSE(frames_[index]);
  }

  static void ExtractLoop(void* obj) {
    TestFrameBuffer2* tfb = static_cast<TestFrameBuffer2*>(obj);
    while (true) {
      tfb->trigger_extract_event_.Wait(rtc::Event::kForever);
      {
        rtc::CritScope lock(&tfb->crit_);
        tfb->crit_acquired_event_.Set();
        if (tfb->tear_down_)
          return;

        std::unique_ptr<EncodedFrame> frame;
        FrameBuffer::ReturnReason res =
            tfb->buffer_->NextFrame(tfb->max_wait_time_, &frame);
        if (res != FrameBuffer::ReturnReason::kStopped)
          tfb->frames_.emplace_back(std::move(frame));
      }
    }
  }

  uint32_t Rand() { return rand_.Rand<uint32_t>(); }

  SimulatedClock clock_;
  VCMTimingFake timing_;
  ::testing::NiceMock<VCMJitterEstimatorMock> jitter_estimator_;
  std::unique_ptr<FrameBuffer> buffer_;
  std::vector<std::unique_ptr<EncodedFrame>> frames_;
  Random rand_;
  ::testing::NiceMock<VCMReceiveStatisticsCallbackMock> stats_callback_;

  int64_t max_wait_time_;
  bool tear_down_;
  rtc::PlatformThread extract_thread_;
  rtc::Event trigger_extract_event_;
  rtc::Event crit_acquired_event_;
  rtc::CriticalSection crit_;
};

// Following tests are timing dependent. Either the timeouts have to
// be increased by a large margin, which would slow down all trybots,
// or we disable them for the very slow ones, like we do here.
#if !defined(ADDRESS_SANITIZER) && !defined(MEMORY_SANITIZER)
TEST_F(TestFrameBuffer2, WaitForFrame) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  ExtractFrame(50);
  InsertFrame(pid, 0, ts, false, true);
  CheckFrame(0, pid, 0);
}

TEST_F(TestFrameBuffer2, OneSuperFrame) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  InsertFrame(pid, 0, ts, false, false);
  InsertFrame(pid, 1, ts, true, true);
  ExtractFrame();

  CheckFrame(0, pid, 0);
}

TEST_F(TestFrameBuffer2, SetPlayoutDelay) {
  const PlayoutDelay kPlayoutDelayMs = {123, 321};
  std::unique_ptr<FrameObjectFake> test_frame(new FrameObjectFake());
  test_frame->id.picture_id = 0;
  test_frame->SetPlayoutDelay(kPlayoutDelayMs);
  buffer_->InsertFrame(std::move(test_frame));
  EXPECT_EQ(kPlayoutDelayMs.min_ms, timing_.min_playout_delay());
  EXPECT_EQ(kPlayoutDelayMs.max_ms, timing_.max_playout_delay());
}

TEST_F(TestFrameBuffer2, ZeroPlayoutDelay) {
  VCMTiming timing(&clock_);
  buffer_.reset(
      new FrameBuffer(&clock_, &jitter_estimator_, &timing, &stats_callback_));
  const PlayoutDelay kPlayoutDelayMs = {0, 0};
  std::unique_ptr<FrameObjectFake> test_frame(new FrameObjectFake());
  test_frame->id.picture_id = 0;
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
  InsertFrame(pid, 1, ts, true, true);
  InsertFrame(pid, 0, ts, false, false);
  ExtractFrame();

  CheckFrame(0, pid, 0);
  CheckFrame(1, pid, 1);
}

TEST_F(TestFrameBuffer2, DISABLED_OneLayerStreamReordered) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  InsertFrame(pid, 0, ts, false, true);
  ExtractFrame();
  CheckFrame(0, pid, 0);
  for (int i = 1; i < 10; i += 2) {
    ExtractFrame(50);
    InsertFrame(pid + i + 1, 0, ts + (i + 1) * kFps10, false, true, pid + i);
    clock_.AdvanceTimeMilliseconds(kFps10);
    InsertFrame(pid + i, 0, ts + i * kFps10, false, true, pid + i - 1);
    clock_.AdvanceTimeMilliseconds(kFps10);
    ExtractFrame();
    CheckFrame(i, pid + i, 0);
    CheckFrame(i + 1, pid + i + 1, 0);
  }
}
#endif  // Timing dependent tests.

TEST_F(TestFrameBuffer2, ExtractFromEmptyBuffer) {
  ExtractFrame();
  CheckNoFrame(0);
}

TEST_F(TestFrameBuffer2, MissingFrame) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  InsertFrame(pid, 0, ts, false, true);
  InsertFrame(pid + 2, 0, ts, false, true, pid);
  InsertFrame(pid + 3, 0, ts, false, true, pid + 1, pid + 2);
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

  InsertFrame(pid, 0, ts, false, true);
  ExtractFrame();
  CheckFrame(0, pid, 0);
  for (int i = 1; i < 10; ++i) {
    InsertFrame(pid + i, 0, ts + i * kFps10, false, true, pid + i - 1);
    ExtractFrame();
    clock_.AdvanceTimeMilliseconds(kFps10);
    CheckFrame(i, pid + i, 0);
  }
}

TEST_F(TestFrameBuffer2, DropTemporalLayerSlowDecoder) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  InsertFrame(pid, 0, ts, false, true);
  InsertFrame(pid + 1, 0, ts + kFps20, false, true, pid);
  for (int i = 2; i < 10; i += 2) {
    uint32_t ts_tl0 = ts + i / 2 * kFps10;
    InsertFrame(pid + i, 0, ts_tl0, false, true, pid + i - 2);
    InsertFrame(pid + i + 1, 0, ts_tl0 + kFps20, false, true, pid + i,
                pid + i - 1);
  }

  for (int i = 0; i < 10; ++i) {
    ExtractFrame();
    clock_.AdvanceTimeMilliseconds(70);
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

TEST_F(TestFrameBuffer2, InsertLateFrame) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  InsertFrame(pid, 0, ts, false, true);
  ExtractFrame();
  InsertFrame(pid + 2, 0, ts, false, true);
  ExtractFrame();
  InsertFrame(pid + 1, 0, ts, false, true, pid);
  ExtractFrame();

  CheckFrame(0, pid, 0);
  CheckFrame(1, pid + 2, 0);
  CheckNoFrame(2);
}

TEST_F(TestFrameBuffer2, ProtectionMode) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  EXPECT_CALL(jitter_estimator_, GetJitterEstimate(1.0));
  InsertFrame(pid, 0, ts, false, true);
  ExtractFrame();

  buffer_->SetProtectionMode(kProtectionNackFEC);
  EXPECT_CALL(jitter_estimator_, GetJitterEstimate(0.0));
  InsertFrame(pid + 1, 0, ts, false, true);
  ExtractFrame();
}

TEST_F(TestFrameBuffer2, NoContinuousFrame) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  EXPECT_EQ(-1, InsertFrame(pid + 1, 0, ts, false, true, pid));
}

TEST_F(TestFrameBuffer2, LastContinuousFrameSingleLayer) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  EXPECT_EQ(pid, InsertFrame(pid, 0, ts, false, true));
  EXPECT_EQ(pid, InsertFrame(pid + 2, 0, ts, false, true, pid + 1));
  EXPECT_EQ(pid + 2, InsertFrame(pid + 1, 0, ts, false, true, pid));
  EXPECT_EQ(pid + 2, InsertFrame(pid + 4, 0, ts, false, true, pid + 3));
  EXPECT_EQ(pid + 5, InsertFrame(pid + 5, 0, ts, false, true));
}

TEST_F(TestFrameBuffer2, LastContinuousFrameTwoLayers) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  EXPECT_EQ(pid, InsertFrame(pid, 0, ts, false, false));
  EXPECT_EQ(pid, InsertFrame(pid, 1, ts, true, true));
  EXPECT_EQ(pid, InsertFrame(pid + 1, 1, ts, true, true, pid));
  EXPECT_EQ(pid, InsertFrame(pid + 2, 0, ts, false, false, pid + 1));
  EXPECT_EQ(pid, InsertFrame(pid + 2, 1, ts, true, true, pid + 1));
  EXPECT_EQ(pid, InsertFrame(pid + 3, 0, ts, false, false, pid + 2));
  EXPECT_EQ(pid + 3, InsertFrame(pid + 1, 0, ts, false, false, pid));
  EXPECT_EQ(pid + 3, InsertFrame(pid + 3, 1, ts, true, true, pid + 2));
}

TEST_F(TestFrameBuffer2, PictureIdJumpBack) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  EXPECT_EQ(pid, InsertFrame(pid, 0, ts, false, true));
  EXPECT_EQ(pid + 1, InsertFrame(pid + 1, 0, ts + 1, false, true, pid));
  ExtractFrame();
  CheckFrame(0, pid, 0);

  // Jump back in pid but increase ts.
  EXPECT_EQ(pid - 1, InsertFrame(pid - 1, 0, ts + 2, false, true));
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
  EXPECT_CALL(stats_callback_,
              OnFrameBufferTimingsUpdated(_, _, _, _, _, _, _));

  {
    std::unique_ptr<FrameObjectFake> frame(new FrameObjectFake());
    frame->VerifyAndAllocate(kFrameSize);
    frame->SetSize(kFrameSize);
    frame->id.picture_id = pid;
    frame->id.spatial_layer = 0;
    frame->SetTimestamp(ts);
    frame->num_references = 0;
    frame->inter_layer_predicted = false;

    EXPECT_EQ(buffer_->InsertFrame(std::move(frame)), pid);
  }

  ExtractFrame();
  CheckFrame(0, pid, 0);
}

TEST_F(TestFrameBuffer2, ForwardJumps) {
  EXPECT_EQ(5453, InsertFrame(5453, 0, 1, false, true));
  ExtractFrame();
  EXPECT_EQ(5454, InsertFrame(5454, 0, 1, false, true, 5453));
  ExtractFrame();
  EXPECT_EQ(15670, InsertFrame(15670, 0, 1, false, true));
  ExtractFrame();
  EXPECT_EQ(29804, InsertFrame(29804, 0, 1, false, true));
  ExtractFrame();
  EXPECT_EQ(29805, InsertFrame(29805, 0, 1, false, true, 29804));
  ExtractFrame();
  EXPECT_EQ(29806, InsertFrame(29806, 0, 1, false, true, 29805));
  ExtractFrame();
  EXPECT_EQ(33819, InsertFrame(33819, 0, 1, false, true));
  ExtractFrame();
  EXPECT_EQ(41248, InsertFrame(41248, 0, 1, false, true));
  ExtractFrame();
}

TEST_F(TestFrameBuffer2, DuplicateFrames) {
  EXPECT_EQ(22256, InsertFrame(22256, 0, 1, false, true));
  ExtractFrame();
  EXPECT_EQ(22256, InsertFrame(22256, 0, 1, false, true));
}

// TODO(philipel): implement more unittests related to invalid references.
TEST_F(TestFrameBuffer2, InvalidReferences) {
  EXPECT_EQ(-1, InsertFrame(0, 0, 1000, false, true, 2));
  EXPECT_EQ(1, InsertFrame(1, 0, 2000, false, true));
  ExtractFrame();
  EXPECT_EQ(2, InsertFrame(2, 0, 3000, false, true, 1));
}

TEST_F(TestFrameBuffer2, KeyframeRequired) {
  EXPECT_EQ(1, InsertFrame(1, 0, 1000, false, true));
  EXPECT_EQ(2, InsertFrame(2, 0, 2000, false, true, 1));
  EXPECT_EQ(3, InsertFrame(3, 0, 3000, false, true));
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
    EXPECT_EQ(-1, InsertFrame(i, 0, i * 1000, false, true, i - 1));
  ExtractFrame();
  CheckNoFrame(0);

  EXPECT_EQ(kMaxBufferSize + 1,
            InsertFrame(kMaxBufferSize + 1, 0, (kMaxBufferSize + 1) * 1000,
                        false, true));
  ExtractFrame();
  CheckFrame(1, kMaxBufferSize + 1, 0);
}

TEST_F(TestFrameBuffer2, DontUpdateOnUndecodableFrame) {
  InsertFrame(1, 0, 0, false, true);
  ExtractFrame(0, true);
  InsertFrame(3, 0, 0, false, true, 2, 0);
  InsertFrame(3, 0, 0, false, true, 0);
  InsertFrame(2, 0, 0, false, true);
  ExtractFrame(0, true);
  ExtractFrame(0, true);
}

TEST_F(TestFrameBuffer2, DontDecodeOlderTimestamp) {
  InsertFrame(2, 0, 1, false, true);
  InsertFrame(1, 0, 2, false, true);  // Older picture id but newer timestamp.
  ExtractFrame(0);
  ExtractFrame(0);
  CheckFrame(0, 1, 0);
  CheckNoFrame(1);

  InsertFrame(3, 0, 4, false, true);
  InsertFrame(4, 0, 3, false, true);  // Newer picture id but older timestamp.
  ExtractFrame(0);
  ExtractFrame(0);
  CheckFrame(2, 3, 0);
  CheckNoFrame(3);
}

TEST_F(TestFrameBuffer2, CombineFramesToSuperframe) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  InsertFrame(pid, 0, ts, false, false);
  InsertFrame(pid, 1, ts, true, true);
  ExtractFrame(0);
  ExtractFrame(0);
  CheckFrame(0, pid, 0);
  CheckNoFrame(1);
  // Two frames should be combined and returned together.
  CheckFrameSize(0, kFrameSize * 2);
}

TEST_F(TestFrameBuffer2, HigherSpatialLayerNonDecodable) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  InsertFrame(pid, 0, ts, false, false);
  InsertFrame(pid, 1, ts, true, true);

  ExtractFrame(0);
  CheckFrame(0, pid, 0);

  InsertFrame(pid + 1, 1, ts + kFps20, false, true, pid);
  InsertFrame(pid + 2, 0, ts + kFps10, false, false, pid);
  InsertFrame(pid + 2, 1, ts + kFps10, true, true, pid + 1);

  clock_.AdvanceTimeMilliseconds(1000);
  // Frame pid+1 is decodable but too late.
  // In superframe pid+2 frame sid=0 is decodable, but frame sid=1 is not.
  // Incorrect implementation might skip pid+1 frame and output undecodable
  // pid+2 instead.
  ExtractFrame();
  ExtractFrame();
  CheckFrame(1, pid + 1, 1);
  CheckFrame(2, pid + 2, 0);
}

}  // namespace video_coding
}  // namespace webrtc
