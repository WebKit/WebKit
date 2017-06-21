/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <vector>

#include "webrtc/modules/video_coding/encoded_frame.h"
#include "webrtc/modules/video_coding/generic_encoder.h"
#include "webrtc/modules/video_coding/include/video_coding_defines.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace test {
namespace {
inline size_t FrameSize(const size_t& min_frame_size,
                        const size_t& max_frame_size,
                        const int& s,
                        const int& i) {
  return min_frame_size + (s + 1) * i % (max_frame_size - min_frame_size);
}

class FakeEncodedImageCallback : public EncodedImageCallback {
 public:
  FakeEncodedImageCallback() : last_frame_was_timing_(false) {}
  Result OnEncodedImage(const EncodedImage& encoded_image,
                        const CodecSpecificInfo* codec_specific_info,
                        const RTPFragmentationHeader* fragmentation) override {
    last_frame_was_timing_ = encoded_image.timing_.is_timing_frame;
    return Result::OK;
  };

  bool WasTimingFrame() { return last_frame_was_timing_; }

 private:
  bool last_frame_was_timing_;
};

enum class FrameType {
  kNormal,
  kTiming,
  kDropped,
};

// Emulates |num_frames| on |num_streams| frames with capture timestamps
// increased by 1 from 0. Size of each frame is between
// |min_frame_size| and |max_frame_size|, outliers are counted relatevely to
// |average_frame_sizes[]| for each stream.
std::vector<std::vector<FrameType>> GetTimingFrames(
    const int64_t delay_ms,
    const size_t min_frame_size,
    const size_t max_frame_size,
    std::vector<size_t> average_frame_sizes,
    const int num_streams,
    const int num_frames) {
  FakeEncodedImageCallback sink;
  VCMEncodedFrameCallback callback(&sink, nullptr);
  const size_t kFramerate = 30;
  callback.SetTimingFramesThresholds(
      {delay_ms, kDefaultOutlierFrameSizePercent});
  callback.OnFrameRateChanged(kFramerate);
  int s, i;
  std::vector<std::vector<FrameType>> result(num_streams);
  for (s = 0; s < num_streams; ++s)
    callback.OnTargetBitrateChanged(average_frame_sizes[s] * kFramerate, s);
  int64_t current_timestamp = 0;
  for (i = 0; i < num_frames; ++i) {
    current_timestamp += 1;
    for (s = 0; s < num_streams; ++s) {
      // every (5+s)-th frame is dropped on s-th stream by design.
      bool dropped = i % (5 + s) == 0;

      EncodedImage image;
      CodecSpecificInfo codec_specific;
      image._length = FrameSize(min_frame_size, max_frame_size, s, i);
      image.capture_time_ms_ = current_timestamp;
      codec_specific.codecType = kVideoCodecGeneric;
      codec_specific.codecSpecific.generic.simulcast_idx = s;
      callback.OnEncodeStarted(current_timestamp, s);
      if (dropped) {
        result[s].push_back(FrameType::kDropped);
        continue;
      }
      callback.OnEncodedImage(image, &codec_specific, nullptr);
      if (sink.WasTimingFrame()) {
        result[s].push_back(FrameType::kTiming);
      } else {
        result[s].push_back(FrameType::kNormal);
      }
    }
  }
  return result;
}
}  // namespace

TEST(TestVCMEncodedFrameCallback, MarksTimingFramesPeriodicallyTogether) {
  const int64_t kDelayMs = 29;
  const size_t kMinFrameSize = 10;
  const size_t kMaxFrameSize = 20;
  const int kNumFrames = 1000;
  const int kNumStreams = 3;
  // No outliers as 1000 is larger than anything from range [10,20].
  const std::vector<size_t> kAverageSize = {1000, 1000, 1000};
  auto frames = GetTimingFrames(kDelayMs, kMinFrameSize, kMaxFrameSize,
                                kAverageSize, kNumStreams, kNumFrames);
  // Timing frames should be tirggered every delayMs.
  // As no outliers are expected, frames on all streams have to be
  // marked together.
  int last_timing_frame = -1;
  for (int i = 0; i < kNumFrames; ++i) {
    int num_normal = 0;
    int num_timing = 0;
    int num_dropped = 0;
    for (int s = 0; s < kNumStreams; ++s) {
      if (frames[s][i] == FrameType::kTiming) {
        ++num_timing;
      } else if (frames[s][i] == FrameType::kNormal) {
        ++num_normal;
      } else {
        ++num_dropped;
      }
    }
    // Can't have both normal and timing frames at the same timstamp.
    EXPECT_TRUE(num_timing == 0 || num_normal == 0);
    if (num_dropped < kNumStreams) {
      if (last_timing_frame == -1 || i >= last_timing_frame + kDelayMs) {
        // If didn't have timing frames for a period, current sent frame has to
        // be one. No normal frames should be sent.
        EXPECT_EQ(num_normal, 0);
      } else {
        // No unneeded timing frames should be sent.
        EXPECT_EQ(num_timing, 0);
      }
    }
    if (num_timing > 0)
      last_timing_frame = i;
  }
}

TEST(TestVCMEncodedFrameCallback, MarksOutliers) {
  const int64_t kDelayMs = 29;
  const size_t kMinFrameSize = 2495;
  const size_t kMaxFrameSize = 2505;
  const int kNumFrames = 1000;
  const int kNumStreams = 3;
  // Possible outliers as 1000 lies in range [995, 1005].
  const std::vector<size_t> kAverageSize = {998, 1000, 1004};
  auto frames = GetTimingFrames(kDelayMs, kMinFrameSize, kMaxFrameSize,
                                kAverageSize, kNumStreams, kNumFrames);
  // All outliers should be marked.
  for (int i = 0; i < kNumFrames; ++i) {
    for (int s = 0; s < kNumStreams; ++s) {
      if (FrameSize(kMinFrameSize, kMaxFrameSize, s, i) >=
          kAverageSize[s] * kDefaultOutlierFrameSizePercent / 100) {
        // Too big frame. May be dropped or timing, but not normal.
        EXPECT_NE(frames[s][i], FrameType::kNormal);
      }
    }
  }
}

}  // namespace test
}  // namespace webrtc
