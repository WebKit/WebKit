/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_VIDEO_VIDEO_TIMING_H_
#define WEBRTC_API_VIDEO_VIDEO_TIMING_H_

#include <stdint.h>
#include <limits>
#include "webrtc/base/checks.h"
#include "webrtc/base/safe_conversions.h"

namespace webrtc {

// Video timing timstamps in ms counted from capture_time_ms of a frame.
struct VideoTiming {
  static const uint8_t kEncodeStartDeltaIdx = 0;
  static const uint8_t kEncodeFinishDeltaIdx = 1;
  static const uint8_t kPacketizationFinishDeltaIdx = 2;
  static const uint8_t kPacerExitDeltaIdx = 3;
  static const uint8_t kNetworkTimestampDeltaIdx = 4;
  static const uint8_t kNetwork2TimestampDeltaIdx = 5;

  // Returns |time_ms - base_ms| capped at max 16-bit value.
  // Used to fill this data structure as per
  // https://webrtc.org/experiments/rtp-hdrext/video-timing/ extension stores
  // 16-bit deltas of timestamps from packet capture time.
  static uint16_t GetDeltaCappedMs(int64_t base_ms, int64_t time_ms) {
    RTC_DCHECK_GE(time_ms, base_ms);
    return rtc::saturated_cast<uint16_t>(time_ms - base_ms);
  }

  uint16_t encode_start_delta_ms;
  uint16_t encode_finish_delta_ms;
  uint16_t packetization_finish_delta_ms;
  uint16_t pacer_exit_delta_ms;
  uint16_t network_timstamp_delta_ms;
  uint16_t network2_timstamp_delta_ms;
  bool is_timing_frame;
};

}  // namespace webrtc

#endif  // WEBRTC_API_VIDEO_VIDEO_TIMING_H_
