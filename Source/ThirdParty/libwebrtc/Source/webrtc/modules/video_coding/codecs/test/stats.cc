/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/codecs/test/stats.h"

#include <stdio.h>

#include <algorithm>  // min_element, max_element

#include "webrtc/base/checks.h"
#include "webrtc/base/format_macros.h"

namespace webrtc {
namespace test {
namespace {
bool LessForEncodeTime(const FrameStatistic& s1, const FrameStatistic& s2) {
  return s1.encode_time_in_us < s2.encode_time_in_us;
}

bool LessForDecodeTime(const FrameStatistic& s1, const FrameStatistic& s2) {
  return s1.decode_time_in_us < s2.decode_time_in_us;
}

bool LessForEncodedSize(const FrameStatistic& s1, const FrameStatistic& s2) {
  return s1.encoded_frame_length_in_bytes < s2.encoded_frame_length_in_bytes;
}

bool LessForBitRate(const FrameStatistic& s1, const FrameStatistic& s2) {
  return s1.bit_rate_in_kbps < s2.bit_rate_in_kbps;
}
}  // namespace

Stats::Stats() {}

Stats::~Stats() {}

FrameStatistic& Stats::NewFrame(int frame_number) {
  RTC_DCHECK_GE(frame_number, 0);
  FrameStatistic stat;
  stat.frame_number = frame_number;
  stats_.push_back(stat);
  return stats_[frame_number];
}

void Stats::PrintSummary() {
  printf("Processing summary:\n");
  if (stats_.empty()) {
    printf("No frame statistics have been logged yet.\n");
    return;
  }

  // Calculate min, max, average and total encoding time.
  int total_encoding_time_in_us = 0;
  int total_decoding_time_in_us = 0;
  int total_qp = 0;
  int total_qp_count = 0;
  size_t total_encoded_frames_lengths = 0;
  size_t total_encoded_key_frames_lengths = 0;
  size_t total_encoded_nonkey_frames_lengths = 0;
  size_t num_keyframes = 0;
  size_t num_nonkeyframes = 0;

  for (const FrameStatistic& stat : stats_) {
    total_encoding_time_in_us += stat.encode_time_in_us;
    total_decoding_time_in_us += stat.decode_time_in_us;
    total_encoded_frames_lengths += stat.encoded_frame_length_in_bytes;
    if (stat.frame_type == webrtc::kVideoFrameKey) {
      total_encoded_key_frames_lengths += stat.encoded_frame_length_in_bytes;
      ++num_keyframes;
    } else {
      total_encoded_nonkey_frames_lengths += stat.encoded_frame_length_in_bytes;
      ++num_nonkeyframes;
    }
    if (stat.qp >= 0) {
      total_qp += stat.qp;
      ++total_qp_count;
    }
  }

  // Encoding stats.
  printf("Encoding time:\n");
  FrameStatisticsIterator frame;
  frame = std::min_element(stats_.begin(), stats_.end(), LessForEncodeTime);
  printf("  Min     : %7d us (frame %d)\n", frame->encode_time_in_us,
         frame->frame_number);
  frame = std::max_element(stats_.begin(), stats_.end(), LessForEncodeTime);
  printf("  Max     : %7d us (frame %d)\n", frame->encode_time_in_us,
         frame->frame_number);
  printf("  Average : %7d us\n",
         static_cast<int>(total_encoding_time_in_us / stats_.size()));

  // Decoding stats.
  printf("Decoding time:\n");
  // Only consider successfully decoded frames (packet loss may cause failures).
  std::vector<FrameStatistic> decoded_frames;
  for (const FrameStatistic& stat : stats_) {
    if (stat.decoding_successful) {
      decoded_frames.push_back(stat);
    }
  }
  if (decoded_frames.empty()) {
    printf("No successfully decoded frames exist in this statistics.\n");
  } else {
    frame = std::min_element(decoded_frames.begin(), decoded_frames.end(),
                             LessForDecodeTime);
    printf("  Min     : %7d us (frame %d)\n", frame->decode_time_in_us,
           frame->frame_number);
    frame = std::max_element(decoded_frames.begin(), decoded_frames.end(),
                             LessForDecodeTime);
    printf("  Max     : %7d us (frame %d)\n", frame->decode_time_in_us,
           frame->frame_number);
    printf("  Average : %7d us\n",
           static_cast<int>(total_decoding_time_in_us / decoded_frames.size()));
    printf("  Failures: %d frames failed to decode.\n",
           static_cast<int>(stats_.size() - decoded_frames.size()));
  }

  // Frame size stats.
  printf("Frame sizes:\n");
  frame = std::min_element(stats_.begin(), stats_.end(), LessForEncodedSize);
  printf("  Min     : %7" PRIuS " bytes (frame %d)\n",
         frame->encoded_frame_length_in_bytes, frame->frame_number);
  frame = std::max_element(stats_.begin(), stats_.end(), LessForEncodedSize);
  printf("  Max     : %7" PRIuS " bytes (frame %d)\n",
         frame->encoded_frame_length_in_bytes, frame->frame_number);
  printf("  Average : %7" PRIuS " bytes\n",
         total_encoded_frames_lengths / stats_.size());
  if (num_keyframes > 0) {
    printf("  Average key frame size    : %7" PRIuS " bytes (%" PRIuS
           " keyframes)\n",
           total_encoded_key_frames_lengths / num_keyframes, num_keyframes);
  }
  if (num_nonkeyframes > 0) {
    printf("  Average non-key frame size: %7" PRIuS " bytes (%" PRIuS
           " frames)\n",
           total_encoded_nonkey_frames_lengths / num_nonkeyframes,
           num_nonkeyframes);
  }

  // Bitrate stats.
  printf("Bit rates:\n");
  frame = std::min_element(stats_.begin(), stats_.end(), LessForBitRate);
  printf("  Min bit rate: %7d kbps (frame %d)\n", frame->bit_rate_in_kbps,
         frame->frame_number);
  frame = std::max_element(stats_.begin(), stats_.end(), LessForBitRate);
  printf("  Max bit rate: %7d kbps (frame %d)\n", frame->bit_rate_in_kbps,
         frame->frame_number);

  int avg_qp = (total_qp_count > 0) ? (total_qp / total_qp_count) : -1;
  printf("Average QP: %d\n", avg_qp);

  printf("\n");
  printf("Total encoding time  : %7d ms.\n", total_encoding_time_in_us / 1000);
  printf("Total decoding time  : %7d ms.\n", total_decoding_time_in_us / 1000);
  printf("Total processing time: %7d ms.\n",
         (total_encoding_time_in_us + total_decoding_time_in_us) / 1000);
}

}  // namespace test
}  // namespace webrtc
