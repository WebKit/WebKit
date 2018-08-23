/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_MEDIA_OPTIMIZATION_H_
#define MODULES_VIDEO_CODING_MEDIA_OPTIMIZATION_H_

#include <memory>

#include "modules/include/module_common_types.h"
#include "modules/video_coding/include/video_coding.h"
#include "modules/video_coding/media_opt_util.h"
#include "rtc_base/criticalsection.h"

namespace webrtc {

class Clock;
class FrameDropper;

namespace media_optimization {

class MediaOptimization {
 public:
  explicit MediaOptimization(Clock* clock);
  ~MediaOptimization();

  // TODO(andresp): Can Reset and SetEncodingData be done at construction time
  // only?
  void Reset();

  // Informs media optimization of initial encoding state.
  // TODO(perkj): Deprecate SetEncodingData once its not used for stats in
  // VideoStreamEncoder.
  void SetEncodingData(int32_t max_bit_rate,
                       uint32_t bit_rate,
                       uint32_t max_frame_rate);

  // Sets target rates for the encoder given the channel parameters.
  // Input: |target bitrate| - the encoder target bitrate in bits/s.
  uint32_t SetTargetRates(uint32_t target_bitrate);

  void EnableFrameDropper(bool enable);
  bool DropFrame();

  // Informs Media Optimization of encoded output.
  // TODO(perkj): Deprecate SetEncodingData once its not used for stats in
  // VideoStreamEncoder.
  void UpdateWithEncodedData(const size_t encoded_image_length,
                             const FrameType encoded_image_frametype);

  // InputFrameRate 0 = no frame rate estimate available.
  uint32_t InputFrameRate();

 private:
  enum { kFrameCountHistorySize = 90 };

  void UpdateIncomingFrameRate() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  void ProcessIncomingFrameRate(int64_t now)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  uint32_t InputFrameRateInternal() RTC_EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  // Protect all members.
  rtc::CriticalSection crit_sect_;

  Clock* const clock_ RTC_GUARDED_BY(crit_sect_);
  int32_t max_bit_rate_ RTC_GUARDED_BY(crit_sect_);
  float max_frame_rate_ RTC_GUARDED_BY(crit_sect_);
  std::unique_ptr<FrameDropper> frame_dropper_ RTC_GUARDED_BY(crit_sect_);
  float incoming_frame_rate_ RTC_GUARDED_BY(crit_sect_);
  int64_t incoming_frame_times_[kFrameCountHistorySize] RTC_GUARDED_BY(
      crit_sect_);
};
}  // namespace media_optimization
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_MEDIA_OPTIMIZATION_H_
