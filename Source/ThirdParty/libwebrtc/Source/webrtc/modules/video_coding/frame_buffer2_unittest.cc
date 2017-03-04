/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/frame_buffer2.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

#include "webrtc/base/platform_thread.h"
#include "webrtc/base/random.h"
#include "webrtc/modules/video_coding/frame_object.h"
#include "webrtc/modules/video_coding/jitter_estimator.h"
#include "webrtc/modules/video_coding/sequence_number_util.h"
#include "webrtc/modules/video_coding/timing.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

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

  uint32_t MaxWaitingTime(int64_t render_time_ms,
                          int64_t now_ms) const override {
    return std::max<int>(0, render_time_ms - now_ms - kDecodeTime);
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

class FrameObjectFake : public FrameObject {
 public:
  bool GetBitstream(uint8_t* destination) const override { return true; }

  uint32_t Timestamp() const override { return timestamp; }

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
  MOCK_METHOD2(OnCompleteFrame, void(bool is_keyframe, size_t size_bytes));
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
};

class TestFrameBuffer2 : public ::testing::Test {
 protected:
  static constexpr int kMaxReferences = 5;
  static constexpr int kFps1 = 1000;
  static constexpr int kFps10 = kFps1 / 10;
  static constexpr int kFps20 = kFps1 / 20;

  TestFrameBuffer2()
      : clock_(0),
        timing_(&clock_),
        jitter_estimator_(&clock_),
        buffer_(&clock_, &jitter_estimator_, &timing_, &stats_callback_),
        rand_(0x34678213),
        tear_down_(false),
        extract_thread_(&ExtractLoop, this, "Extract Thread"),
        trigger_extract_event_(false, false),
        crit_acquired_event_(false, false) {}

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
                  T... refs) {
    static_assert(sizeof...(refs) <= kMaxReferences,
                  "To many references specified for FrameObject.");
    std::array<uint16_t, sizeof...(refs)> references = {{refs...}};

    std::unique_ptr<FrameObjectFake> frame(new FrameObjectFake());
    frame->picture_id = picture_id;
    frame->spatial_layer = spatial_layer;
    frame->timestamp = ts_ms * 90;
    frame->num_references = references.size();
    frame->inter_layer_predicted = inter_layer_predicted;
    for (size_t r = 0; r < references.size(); ++r)
      frame->references[r] = references[r];

    return buffer_.InsertFrame(std::move(frame));
  }

  void ExtractFrame(int64_t max_wait_time = 0) {
    crit_.Enter();
    if (max_wait_time == 0) {
      std::unique_ptr<FrameObject> frame;
      FrameBuffer::ReturnReason res = buffer_.NextFrame(0, &frame);
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
    ASSERT_EQ(picture_id, frames_[index]->picture_id);
    ASSERT_EQ(spatial_layer, frames_[index]->spatial_layer);
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

        std::unique_ptr<FrameObject> frame;
        FrameBuffer::ReturnReason res =
            tfb->buffer_.NextFrame(tfb->max_wait_time_, &frame);
        if (res != FrameBuffer::ReturnReason::kStopped)
          tfb->frames_.emplace_back(std::move(frame));
      }
    }
  }

  uint32_t Rand() { return rand_.Rand<uint32_t>(); }

  SimulatedClock clock_;
  VCMTimingFake timing_;
  ::testing::NiceMock<VCMJitterEstimatorMock> jitter_estimator_;
  FrameBuffer buffer_;
  std::vector<std::unique_ptr<FrameObject>> frames_;
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
  InsertFrame(pid, 0, ts, false);
  CheckFrame(0, pid, 0);
}

TEST_F(TestFrameBuffer2, OneSuperFrame) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  InsertFrame(pid, 0, ts, false);
  ExtractFrame();
  InsertFrame(pid, 1, ts, true);
  ExtractFrame();

  CheckFrame(0, pid, 0);
  CheckFrame(1, pid, 1);
}

// Flaky test, see bugs.webrtc.org/7068.
TEST_F(TestFrameBuffer2, DISABLED_OneUnorderedSuperFrame) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  ExtractFrame(50);
  InsertFrame(pid, 1, ts, true);
  InsertFrame(pid, 0, ts, false);
  ExtractFrame();

  CheckFrame(0, pid, 0);
  CheckFrame(1, pid, 1);
}

TEST_F(TestFrameBuffer2, DISABLED_OneLayerStreamReordered) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  InsertFrame(pid, 0, ts, false);
  ExtractFrame();
  CheckFrame(0, pid, 0);
  for (int i = 1; i < 10; i += 2) {
    ExtractFrame(50);
    InsertFrame(pid + i + 1, 0, ts + (i + 1) * kFps10, false, pid + i);
    clock_.AdvanceTimeMilliseconds(kFps10);
    InsertFrame(pid + i, 0, ts + i * kFps10, false, pid + i - 1);
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

  InsertFrame(pid, 0, ts, false);
  InsertFrame(pid + 2, 0, ts, false, pid);
  InsertFrame(pid + 3, 0, ts, false, pid + 1, pid + 2);
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

  InsertFrame(pid, 0, ts, false);
  ExtractFrame();
  CheckFrame(0, pid, 0);
  for (int i = 1; i < 10; ++i) {
    InsertFrame(pid + i, 0, ts + i * kFps10, false, pid + i - 1);
    ExtractFrame();
    clock_.AdvanceTimeMilliseconds(kFps10);
    CheckFrame(i, pid + i, 0);
  }
}

TEST_F(TestFrameBuffer2, DropTemporalLayerSlowDecoder) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  InsertFrame(pid, 0, ts, false);
  InsertFrame(pid + 1, 0, ts + kFps20, false, pid);
  for (int i = 2; i < 10; i += 2) {
    uint32_t ts_tl0 = ts + i / 2 * kFps10;
    InsertFrame(pid + i, 0, ts_tl0, false, pid + i - 2);
    InsertFrame(pid + i + 1, 0, ts_tl0 + kFps20, false, pid + i, pid + i - 1);
  }

  for (int i = 0; i < 10; ++i) {
    ExtractFrame();
    clock_.AdvanceTimeMilliseconds(60);
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

TEST_F(TestFrameBuffer2, DropSpatialLayerSlowDecoder) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  InsertFrame(pid, 0, ts, false);
  InsertFrame(pid, 1, ts, false);
  for (int i = 1; i < 6; ++i) {
    uint32_t ts_tl0 = ts + i * kFps10;
    InsertFrame(pid + i, 0, ts_tl0, false, pid + i - 1);
    InsertFrame(pid + i, 1, ts_tl0, false, pid + i - 1);
  }

  ExtractFrame();
  ExtractFrame();
  clock_.AdvanceTimeMilliseconds(55);
  for (int i = 2; i < 12; ++i) {
    ExtractFrame();
    clock_.AdvanceTimeMilliseconds(55);
  }

  CheckFrame(0, pid, 0);
  CheckFrame(1, pid, 1);
  CheckFrame(2, pid + 1, 0);
  CheckFrame(3, pid + 1, 1);
  CheckFrame(4, pid + 2, 0);
  CheckFrame(5, pid + 2, 1);
  CheckFrame(6, pid + 3, 0);
  CheckFrame(7, pid + 4, 0);
  CheckFrame(8, pid + 5, 0);
  CheckNoFrame(9);
  CheckNoFrame(10);
  CheckNoFrame(11);
}

TEST_F(TestFrameBuffer2, InsertLateFrame) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  InsertFrame(pid, 0, ts, false);
  ExtractFrame();
  InsertFrame(pid + 2, 0, ts, false);
  ExtractFrame();
  InsertFrame(pid + 1, 0, ts, false, pid);
  ExtractFrame();

  CheckFrame(0, pid, 0);
  CheckFrame(1, pid + 2, 0);
  CheckNoFrame(2);
}

TEST_F(TestFrameBuffer2, ProtectionMode) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  EXPECT_CALL(jitter_estimator_, GetJitterEstimate(1.0));
  InsertFrame(pid, 0, ts, false);
  ExtractFrame();

  buffer_.SetProtectionMode(kProtectionNackFEC);
  EXPECT_CALL(jitter_estimator_, GetJitterEstimate(0.0));
  InsertFrame(pid + 1, 0, ts, false);
  ExtractFrame();
}

TEST_F(TestFrameBuffer2, NoContinuousFrame) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  EXPECT_EQ(-1, InsertFrame(pid + 1, 0, ts, false, pid));
}

TEST_F(TestFrameBuffer2, LastContinuousFrameSingleLayer) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  EXPECT_EQ(pid, InsertFrame(pid, 0, ts, false));
  EXPECT_EQ(pid, InsertFrame(pid + 2, 0, ts, false, pid + 1));
  EXPECT_EQ(pid + 2, InsertFrame(pid + 1, 0, ts, false, pid));
  EXPECT_EQ(pid + 2, InsertFrame(pid + 4, 0, ts, false, pid + 3));
  EXPECT_EQ(pid + 5, InsertFrame(pid + 5, 0, ts, false));
}

TEST_F(TestFrameBuffer2, LastContinuousFrameTwoLayers) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  EXPECT_EQ(pid, InsertFrame(pid, 0, ts, false));
  EXPECT_EQ(pid, InsertFrame(pid, 1, ts, true));
  EXPECT_EQ(pid, InsertFrame(pid + 1, 1, ts, true, pid));
  EXPECT_EQ(pid, InsertFrame(pid + 2, 0, ts, false, pid + 1));
  EXPECT_EQ(pid, InsertFrame(pid + 2, 1, ts, true, pid + 1));
  EXPECT_EQ(pid, InsertFrame(pid + 3, 0, ts, false, pid + 2));
  EXPECT_EQ(pid + 3, InsertFrame(pid + 1, 0, ts, false, pid));
  EXPECT_EQ(pid + 3, InsertFrame(pid + 3, 1, ts, true, pid + 2));
}

TEST_F(TestFrameBuffer2, PictureIdJumpBack) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();

  EXPECT_EQ(pid, InsertFrame(pid, 0, ts, false));
  EXPECT_EQ(pid + 1, InsertFrame(pid + 1, 0, ts + 1, false, pid));
  ExtractFrame();
  CheckFrame(0, pid, 0);

  // Jump back in pid but increase ts.
  EXPECT_EQ(pid - 1, InsertFrame(pid - 1, 0, ts + 2, false));
  ExtractFrame();
  ExtractFrame();
  CheckFrame(1, pid - 1, 0);
  CheckNoFrame(2);
}

TEST_F(TestFrameBuffer2, StatsCallback) {
  uint16_t pid = Rand();
  uint32_t ts = Rand();
  const int kFrameSize = 5000;

  EXPECT_CALL(stats_callback_, OnCompleteFrame(true, kFrameSize));
  EXPECT_CALL(stats_callback_,
              OnFrameBufferTimingsUpdated(_, _, _, _, _, _, _));

  {
    std::unique_ptr<FrameObjectFake> frame(new FrameObjectFake());
    frame->SetSize(kFrameSize);
    frame->picture_id = pid;
    frame->spatial_layer = 0;
    frame->timestamp = ts;
    frame->num_references = 0;
    frame->inter_layer_predicted = false;

    EXPECT_EQ(buffer_.InsertFrame(std::move(frame)), pid);
  }

  ExtractFrame();
  CheckFrame(0, pid, 0);
}

}  // namespace video_coding
}  // namespace webrtc
