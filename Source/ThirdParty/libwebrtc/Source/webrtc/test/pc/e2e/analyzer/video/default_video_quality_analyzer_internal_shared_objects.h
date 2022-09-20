/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_VIDEO_QUALITY_ANALYZER_INTERNAL_SHARED_OBJECTS_H_
#define TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_VIDEO_QUALITY_ANALYZER_INTERNAL_SHARED_OBJECTS_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/types/optional.h"
#include "api/units/data_size.h"
#include "api/units/timestamp.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_type.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_shared_objects.h"

namespace webrtc {

struct InternalStatsKey {
  InternalStatsKey(size_t stream, size_t sender, size_t receiver)
      : stream(stream), sender(sender), receiver(receiver) {}

  std::string ToString() const;

  size_t stream;
  size_t sender;
  size_t receiver;
};

// Required to use InternalStatsKey as std::map key.
bool operator<(const InternalStatsKey& a, const InternalStatsKey& b);
bool operator==(const InternalStatsKey& a, const InternalStatsKey& b);

// Final stats computed for frame after it went through the whole video
// pipeline from capturing to rendering or dropping.
struct FrameStats {
  explicit FrameStats(Timestamp captured_time) : captured_time(captured_time) {}

  // Frame events timestamp.
  Timestamp captured_time;
  Timestamp pre_encode_time = Timestamp::MinusInfinity();
  Timestamp encoded_time = Timestamp::MinusInfinity();
  // Time when last packet of a frame was received.
  Timestamp received_time = Timestamp::MinusInfinity();
  Timestamp decode_start_time = Timestamp::MinusInfinity();
  Timestamp decode_end_time = Timestamp::MinusInfinity();
  Timestamp rendered_time = Timestamp::MinusInfinity();
  Timestamp prev_frame_rendered_time = Timestamp::MinusInfinity();

  VideoFrameType encoded_frame_type = VideoFrameType::kEmptyFrame;
  DataSize encoded_image_size = DataSize::Bytes(0);
  VideoFrameType pre_decoded_frame_type = VideoFrameType::kEmptyFrame;
  DataSize pre_decoded_image_size = DataSize::Bytes(0);
  uint32_t target_encode_bitrate = 0;

  absl::optional<int> rendered_frame_width = absl::nullopt;
  absl::optional<int> rendered_frame_height = absl::nullopt;

  // Can be not set if frame was dropped by encoder.
  absl::optional<StreamCodecInfo> used_encoder = absl::nullopt;
  // Can be not set if frame was dropped in the network.
  absl::optional<StreamCodecInfo> used_decoder = absl::nullopt;
};

// Describes why comparison was done in overloaded mode (without calculating
// PSNR and SSIM).
enum class OverloadReason {
  kNone,
  // Not enough CPU to process all incoming comparisons.
  kCpu,
  // Not enough memory to store captured frames for all comparisons.
  kMemory
};

enum class FrameComparisonType {
  // Comparison for captured and rendered frame.
  kRegular,
  // Comparison for captured frame that is known to be dropped somewhere in
  // video pipeline.
  kDroppedFrame,
  // Comparison for captured frame that was still in the video pipeline when
  // test was stopped. It's unknown is this frame dropped or would it be
  // delivered if test continue.
  kFrameInFlight
};

// Represents comparison between two VideoFrames. Contains video frames itself
// and stats. Can be one of two types:
//   1. Normal - in this case `captured` is presented and either `rendered` is
//      presented and `dropped` is false, either `rendered` is omitted and
//      `dropped` is true.
//   2. Overloaded - in this case both `captured` and `rendered` are omitted
//      because there were too many comparisons in the queue. `dropped` can be
//      true or false showing was frame dropped or not.
struct FrameComparison {
  FrameComparison(InternalStatsKey stats_key,
                  absl::optional<VideoFrame> captured,
                  absl::optional<VideoFrame> rendered,
                  FrameComparisonType type,
                  FrameStats frame_stats,
                  OverloadReason overload_reason);

  InternalStatsKey stats_key;
  // Frames can be omitted if there too many computations waiting in the
  // queue.
  absl::optional<VideoFrame> captured;
  absl::optional<VideoFrame> rendered;
  FrameComparisonType type;
  FrameStats frame_stats;
  OverloadReason overload_reason;
};

}  // namespace webrtc

#endif  // TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_VIDEO_QUALITY_ANALYZER_INTERNAL_SHARED_OBJECTS_H_
