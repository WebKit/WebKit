/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/test/stats.h"

#include <stdio.h>

#include <algorithm>

#include "rtc_base/checks.h"
#include "rtc_base/format_macros.h"

namespace webrtc {
namespace test {

namespace {

bool LessForEncodeTime(const FrameStatistic& s1, const FrameStatistic& s2) {
  RTC_DCHECK_NE(s1.frame_number, s2.frame_number);
  return s1.encode_time_us < s2.encode_time_us;
}

bool LessForDecodeTime(const FrameStatistic& s1, const FrameStatistic& s2) {
  RTC_DCHECK_NE(s1.frame_number, s2.frame_number);
  return s1.decode_time_us < s2.decode_time_us;
}

bool LessForEncodedSize(const FrameStatistic& s1, const FrameStatistic& s2) {
  RTC_DCHECK_NE(s1.frame_number, s2.frame_number);
  return s1.encoded_frame_size_bytes < s2.encoded_frame_size_bytes;
}

bool LessForBitRate(const FrameStatistic& s1, const FrameStatistic& s2) {
  RTC_DCHECK_NE(s1.frame_number, s2.frame_number);
  return s1.bitrate_kbps < s2.bitrate_kbps;
}

bool LessForPsnr(const FrameStatistic& s1, const FrameStatistic& s2) {
  RTC_DCHECK_NE(s1.frame_number, s2.frame_number);
  return s1.psnr < s2.psnr;
}

bool LessForSsim(const FrameStatistic& s1, const FrameStatistic& s2) {
  RTC_DCHECK_NE(s1.frame_number, s2.frame_number);
  return s1.ssim < s2.ssim;
}

}  // namespace

FrameStatistic* Stats::AddFrame() {
  // We don't expect more frames than what can be stored in an int.
  stats_.emplace_back(static_cast<int>(stats_.size()));
  return &stats_.back();
}

FrameStatistic* Stats::GetFrame(int frame_number) {
  RTC_CHECK_GE(frame_number, 0);
  RTC_CHECK_LT(frame_number, stats_.size());
  return &stats_[frame_number];
}

size_t Stats::size() const {
  return stats_.size();
}

void Stats::PrintSummary() const {
  if (stats_.empty()) {
    printf("No frame statistics have been logged yet.\n");
    return;
  }

  printf("Encode/decode statistics\n==\n");

  // Calculate min, max, average and total encoding time.
  int total_encoding_time_us = 0;
  int total_decoding_time_us = 0;
  size_t total_encoded_frame_size_bytes = 0;
  size_t total_encoded_key_frame_size_bytes = 0;
  size_t total_encoded_delta_frame_size_bytes = 0;
  size_t num_key_frames = 0;
  size_t num_delta_frames = 0;
  int num_encode_failures = 0;
  double total_psnr = 0.0;
  double total_ssim = 0.0;

  for (const FrameStatistic& stat : stats_) {
    total_encoding_time_us += stat.encode_time_us;
    total_decoding_time_us += stat.decode_time_us;
    total_encoded_frame_size_bytes += stat.encoded_frame_size_bytes;
    if (stat.frame_type == webrtc::kVideoFrameKey) {
      total_encoded_key_frame_size_bytes += stat.encoded_frame_size_bytes;
      ++num_key_frames;
    } else {
      total_encoded_delta_frame_size_bytes += stat.encoded_frame_size_bytes;
      ++num_delta_frames;
    }
    if (stat.encode_return_code != 0) {
      ++num_encode_failures;
    }
    if (stat.decoding_successful) {
      total_psnr += stat.psnr;
      total_ssim += stat.ssim;
    }
  }

  // Encoding stats.
  printf("# Encoded frame failures: %d\n", num_encode_failures);
  printf("Encoding time:\n");
  auto frame_it =
      std::min_element(stats_.begin(), stats_.end(), LessForEncodeTime);
  printf("  Min     : %7d us (frame %d)\n", frame_it->encode_time_us,
         frame_it->frame_number);
  frame_it = std::max_element(stats_.begin(), stats_.end(), LessForEncodeTime);
  printf("  Max     : %7d us (frame %d)\n", frame_it->encode_time_us,
         frame_it->frame_number);
  printf("  Average : %7d us\n",
         static_cast<int>(total_encoding_time_us / stats_.size()));

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
    frame_it = std::min_element(decoded_frames.begin(), decoded_frames.end(),
                                LessForDecodeTime);
    printf("  Min     : %7d us (frame %d)\n", frame_it->decode_time_us,
           frame_it->frame_number);
    frame_it = std::max_element(decoded_frames.begin(), decoded_frames.end(),
                                LessForDecodeTime);
    printf("  Max     : %7d us (frame %d)\n", frame_it->decode_time_us,
           frame_it->frame_number);
    printf("  Average : %7d us\n",
           static_cast<int>(total_decoding_time_us / decoded_frames.size()));
    printf("  Failures: %d frames failed to decode.\n",
           static_cast<int>(stats_.size() - decoded_frames.size()));
  }

  // Frame size stats.
  printf("Frame sizes:\n");
  frame_it = std::min_element(stats_.begin(), stats_.end(), LessForEncodedSize);
  printf("  Min     : %7" PRIuS " bytes (frame %d)\n",
         frame_it->encoded_frame_size_bytes, frame_it->frame_number);
  frame_it = std::max_element(stats_.begin(), stats_.end(), LessForEncodedSize);
  printf("  Max     : %7" PRIuS " bytes (frame %d)\n",
         frame_it->encoded_frame_size_bytes, frame_it->frame_number);
  printf("  Average : %7" PRIuS " bytes\n",
         total_encoded_frame_size_bytes / stats_.size());
  if (num_key_frames > 0) {
    printf("  Average key frame size    : %7" PRIuS " bytes (%" PRIuS
           " keyframes)\n",
           total_encoded_key_frame_size_bytes / num_key_frames, num_key_frames);
  }
  if (num_delta_frames > 0) {
    printf("  Average non-key frame size: %7" PRIuS " bytes (%" PRIuS
           " frames)\n",
           total_encoded_delta_frame_size_bytes / num_delta_frames,
           num_delta_frames);
  }

  // Bitrate stats.
  printf("Bitrates:\n");
  frame_it = std::min_element(stats_.begin(), stats_.end(), LessForBitRate);
  printf("  Min bitrate: %7d kbps (frame %d)\n", frame_it->bitrate_kbps,
         frame_it->frame_number);
  frame_it = std::max_element(stats_.begin(), stats_.end(), LessForBitRate);
  printf("  Max bitrate: %7d kbps (frame %d)\n", frame_it->bitrate_kbps,
         frame_it->frame_number);

  // Quality.
  printf("Quality:\n");
  if (decoded_frames.empty()) {
    printf("No successfully decoded frames exist in this statistics.\n");
  } else {
    frame_it = std::min_element(decoded_frames.begin(), decoded_frames.end(),
                                LessForPsnr);
    printf("  PSNR min: %f (frame %d)\n", frame_it->psnr,
           frame_it->frame_number);
    printf("  PSNR avg: %f\n", total_psnr / decoded_frames.size());

    frame_it = std::min_element(decoded_frames.begin(), decoded_frames.end(),
                                LessForSsim);
    printf("  SSIM min: %f (frame %d)\n", frame_it->ssim,
           frame_it->frame_number);
    printf("  SSIM avg: %f\n", total_ssim / decoded_frames.size());
  }

  printf("\n");
  printf("Total encoding time  : %7d ms.\n", total_encoding_time_us / 1000);
  printf("Total decoding time  : %7d ms.\n", total_decoding_time_us / 1000);
  printf("Total processing time: %7d ms.\n",
         (total_encoding_time_us + total_decoding_time_us) / 1000);

  // QP stats.
  int total_qp = 0;
  int total_qp_count = 0;
  for (const FrameStatistic& stat : stats_) {
    if (stat.qp >= 0) {
      total_qp += stat.qp;
      ++total_qp_count;
    }
  }
  int avg_qp = (total_qp_count > 0) ? (total_qp / total_qp_count) : -1;
  printf("Average QP: %d\n", avg_qp);
  printf("\n");
}

}  // namespace test
}  // namespace webrtc
