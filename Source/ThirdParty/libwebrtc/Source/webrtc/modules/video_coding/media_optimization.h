/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_MEDIA_OPTIMIZATION_H_
#define WEBRTC_MODULES_VIDEO_CODING_MEDIA_OPTIMIZATION_H_

#include <list>
#include <memory>

#include "webrtc/base/criticalsection.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/video_coding/include/video_coding.h"
#include "webrtc/modules/video_coding/media_opt_util.h"

namespace webrtc {

// Forward declarations.
class Clock;
class FrameDropper;
class VCMContentMetricsProcessing;

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
  // VieEncoder.
  void SetEncodingData(int32_t max_bit_rate,
                       uint32_t bit_rate,
                       uint16_t width,
                       uint16_t height,
                       uint32_t frame_rate,
                       int num_temporal_layers,
                       int32_t mtu);

  // Sets target rates for the encoder given the channel parameters.
  // Input:  target bitrate - the encoder target bitrate in bits/s.
  uint32_t SetTargetRates(uint32_t target_bitrate);

  void EnableFrameDropper(bool enable);
  bool DropFrame();

  // Informs Media Optimization of encoded output.
  // TODO(perkj): Deprecate SetEncodingData once its not used for stats in
  // VieEncoder.
  int32_t UpdateWithEncodedData(const EncodedImage& encoded_image);

  // InputFrameRate 0 = no frame rate estimate available.
  uint32_t InputFrameRate();
  uint32_t SentFrameRate();
  uint32_t SentBitRate();

 private:
  enum { kFrameCountHistorySize = 90 };
  enum { kFrameHistoryWinMs = 2000 };
  enum { kBitrateAverageWinMs = 1000 };

  struct EncodedFrameSample;
  typedef std::list<EncodedFrameSample> FrameSampleList;

  void UpdateIncomingFrameRate() EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  void PurgeOldFrameSamples(int64_t now_ms)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  void UpdateSentBitrate(int64_t now_ms) EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  void UpdateSentFramerate() EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  void ProcessIncomingFrameRate(int64_t now)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  // Checks conditions for suspending the video. The method compares
  // |video_target_bitrate_| with the threshold values for suspension, and
  // changes the state of |video_suspended_| accordingly.
  void CheckSuspendConditions() EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  void SetEncodingDataInternal(int32_t max_bit_rate,
                               uint32_t frame_rate,
                               uint32_t bit_rate,
                               uint16_t width,
                               uint16_t height,
                               int num_temporal_layers,
                               int32_t mtu)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  uint32_t InputFrameRateInternal() EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  uint32_t SentFrameRateInternal() EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  // Protect all members.
  rtc::CriticalSection crit_sect_;

  Clock* clock_ GUARDED_BY(crit_sect_);
  int32_t max_bit_rate_ GUARDED_BY(crit_sect_);
  uint16_t codec_width_ GUARDED_BY(crit_sect_);
  uint16_t codec_height_ GUARDED_BY(crit_sect_);
  float user_frame_rate_ GUARDED_BY(crit_sect_);
  std::unique_ptr<FrameDropper> frame_dropper_ GUARDED_BY(crit_sect_);
  uint32_t send_statistics_[4] GUARDED_BY(crit_sect_);
  uint32_t send_statistics_zero_encode_ GUARDED_BY(crit_sect_);
  int32_t max_payload_size_ GUARDED_BY(crit_sect_);
  int video_target_bitrate_ GUARDED_BY(crit_sect_);
  float incoming_frame_rate_ GUARDED_BY(crit_sect_);
  int64_t incoming_frame_times_[kFrameCountHistorySize] GUARDED_BY(crit_sect_);
  std::list<EncodedFrameSample> encoded_frame_samples_ GUARDED_BY(crit_sect_);
  uint32_t avg_sent_bit_rate_bps_ GUARDED_BY(crit_sect_);
  uint32_t avg_sent_framerate_ GUARDED_BY(crit_sect_);
  int num_layers_ GUARDED_BY(crit_sect_);
};
}  // namespace media_optimization
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_MEDIA_OPTIMIZATION_H_
