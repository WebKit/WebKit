/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_PROTECTION_BITRATE_CALCULATOR_H_
#define WEBRTC_MODULES_VIDEO_CODING_PROTECTION_BITRATE_CALCULATOR_H_

#include <list>
#include <memory>

#include "webrtc/base/criticalsection.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/video_coding/include/video_coding.h"
#include "webrtc/modules/video_coding/media_opt_util.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {

// ProtectionBitrateCalculator calculates how much of the allocated network
// capacity that can be used by an encoder and how much that
// is needed for redundant packets such as FEC and NACK. It uses an
// implementation of |VCMProtectionCallback| to set new FEC parameters and get
// the bitrate currently used for FEC and NACK.
// Usage:
// Setup by calling SetProtectionMethod and SetEncodingData.
// For each encoded image, call UpdateWithEncodedData.
// Each time the bandwidth estimate change, call SetTargetRates. SetTargetRates
// will return the bitrate that can be used by an encoder.
// A lock is used to protect internal states, so methods can be called on an
// arbitrary thread.
class ProtectionBitrateCalculator {
 public:
  ProtectionBitrateCalculator(Clock* clock,
                              VCMProtectionCallback* protection_callback);
  ~ProtectionBitrateCalculator();

  void SetProtectionMethod(bool enable_fec, bool enable_nack);

  // Informs media optimization of initial encoding state.
  void SetEncodingData(size_t width,
                       size_t height,
                       size_t num_temporal_layers,
                       size_t max_payload_size);

  // Returns target rate for the encoder given the channel parameters.
  // Inputs:  estimated_bitrate_bps - the estimated network bitrate in bits/s.
  //          actual_framerate - encoder frame rate.
  //          fraction_lost - packet loss rate in % in the network.
  //          round_trip_time_ms - round trip time in milliseconds.
  uint32_t SetTargetRates(uint32_t estimated_bitrate_bps,
                          int actual_framerate,
                          uint8_t fraction_lost,
                          int64_t round_trip_time_ms);
  // Informs of encoded output.
  void UpdateWithEncodedData(const EncodedImage& encoded_image);

 private:
  struct EncodedFrameSample;
  enum { kBitrateAverageWinMs = 1000 };

  Clock* const clock_;
  VCMProtectionCallback* const protection_callback_;

  rtc::CriticalSection crit_sect_;
  std::unique_ptr<media_optimization::VCMLossProtectionLogic> loss_prot_logic_
      GUARDED_BY(crit_sect_);
  size_t max_payload_size_ GUARDED_BY(crit_sect_);

  RTC_DISALLOW_COPY_AND_ASSIGN(ProtectionBitrateCalculator);
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_CODING_PROTECTION_BITRATE_CALCULATOR_H_
