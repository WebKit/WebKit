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

#include <assert.h>
#include <stdio.h>

#include <algorithm>  // min_element, max_element

#include "webrtc/base/format_macros.h"

namespace webrtc {
namespace test {

FrameStatistic::FrameStatistic()
    : encoding_successful(false),
      decoding_successful(false),
      encode_return_code(0),
      decode_return_code(0),
      encode_time_in_us(0),
      decode_time_in_us(0),
      qp(-1),
      frame_number(0),
      packets_dropped(0),
      total_packets(0),
      bit_rate_in_kbps(0),
      encoded_frame_length_in_bytes(0),
      frame_type(kVideoFrameDelta) {}

Stats::Stats() {}

Stats::~Stats() {}

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

FrameStatistic& Stats::NewFrame(int frame_number) {
  assert(frame_number >= 0);
  FrameStatistic stat;
  stat.frame_number = frame_number;
  stats_.push_back(stat);
  return stats_[frame_number];
}

void Stats::PrintSummary() {
  printf("Processing summary:\n");
  if (stats_.size() == 0) {
    printf("No frame statistics have been logged yet.\n");
    return;
  }

  // Calculate min, max, average and total encoding time
  int total_encoding_time_in_us = 0;
  int total_decoding_time_in_us = 0;
  int total_qp = 0;
  int total_qp_count = 0;
  size_t total_encoded_frames_lengths = 0;
  size_t total_encoded_key_frames_lengths = 0;
  size_t total_encoded_nonkey_frames_lengths = 0;
  size_t nbr_keyframes = 0;
  size_t nbr_nonkeyframes = 0;

  for (FrameStatisticsIterator it = stats_.begin(); it != stats_.end(); ++it) {
    total_encoding_time_in_us += it->encode_time_in_us;
    total_decoding_time_in_us += it->decode_time_in_us;
    total_encoded_frames_lengths += it->encoded_frame_length_in_bytes;
    if (it->frame_type == webrtc::kVideoFrameKey) {
      total_encoded_key_frames_lengths += it->encoded_frame_length_in_bytes;
      nbr_keyframes++;
    } else {
      total_encoded_nonkey_frames_lengths += it->encoded_frame_length_in_bytes;
      nbr_nonkeyframes++;
    }
    if (it->qp >= 0) {
      total_qp += it->qp;
      ++total_qp_count;
    }
  }

  FrameStatisticsIterator frame;

  // ENCODING
  printf("Encoding time:\n");
  frame = std::min_element(stats_.begin(), stats_.end(), LessForEncodeTime);
  printf("  Min     : %7d us (frame %d)\n", frame->encode_time_in_us,
         frame->frame_number);

  frame = std::max_element(stats_.begin(), stats_.end(), LessForEncodeTime);
  printf("  Max     : %7d us (frame %d)\n", frame->encode_time_in_us,
         frame->frame_number);

  printf("  Average : %7d us\n",
         static_cast<int>(total_encoding_time_in_us / stats_.size()));

  // DECODING
  printf("Decoding time:\n");
  // only consider frames that were successfully decoded (packet loss may cause
  // failures)
  std::vector<FrameStatistic> decoded_frames;
  for (std::vector<FrameStatistic>::iterator it = stats_.begin();
       it != stats_.end(); ++it) {
    if (it->decoding_successful) {
      decoded_frames.push_back(*it);
    }
  }
  if (decoded_frames.size() == 0) {
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

  // SIZE
  printf("Frame sizes:\n");
  frame = std::min_element(stats_.begin(), stats_.end(), LessForEncodedSize);
  printf("  Min     : %7" PRIuS " bytes (frame %d)\n",
         frame->encoded_frame_length_in_bytes, frame->frame_number);

  frame = std::max_element(stats_.begin(), stats_.end(), LessForEncodedSize);
  printf("  Max     : %7" PRIuS " bytes (frame %d)\n",
         frame->encoded_frame_length_in_bytes, frame->frame_number);

  printf("  Average : %7" PRIuS " bytes\n",
         total_encoded_frames_lengths / stats_.size());
  if (nbr_keyframes > 0) {
    printf("  Average key frame size    : %7" PRIuS " bytes (%" PRIuS
           " keyframes)\n",
           total_encoded_key_frames_lengths / nbr_keyframes, nbr_keyframes);
  }
  if (nbr_nonkeyframes > 0) {
    printf("  Average non-key frame size: %7" PRIuS " bytes (%" PRIuS
           " frames)\n",
           total_encoded_nonkey_frames_lengths / nbr_nonkeyframes,
           nbr_nonkeyframes);
  }

  // BIT RATE
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
