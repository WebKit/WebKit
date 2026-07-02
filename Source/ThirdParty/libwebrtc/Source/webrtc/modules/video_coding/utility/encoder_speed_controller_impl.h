/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_UTILITY_ENCODER_SPEED_CONTROLLER_IMPL_H_
#define MODULES_VIDEO_CODING_UTILITY_ENCODER_SPEED_CONTROLLER_IMPL_H_

#include <memory>
#include <optional>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video_codecs/encoder_speed_controller.h"
namespace webrtc {

// Utility class intended to help dynamically find the optimal speed settings to
// use for a video encoder. An instance of this class is intended to handle a
// single session at a single resolution. I.e. and new instance should be
// created if the resolution is updated. That also provides the opportunity to
// configure a new set of available speeds, more appropriate for the new
// resolution.
class EncoderSpeedControllerImpl : public webrtc::EncoderSpeedController {
 public:
  // Creates an instance of the speed controller. This should be called any
  // time the encoder has been recreated e.g. due to a resolution change.
  static std::unique_ptr<webrtc::EncoderSpeedController> Create(
      const Config& config,
      TimeDelta start_frame_interval);

  // Should be called any time the rate targets of the encoder changed.
  // The frame interval (1s/fps) effectively sets the time limit for an encoding
  // operation.
  void SetFrameInterval(TimeDelta frame_interval) override;

  // Should be called before each frame to be encoded, and the encoder should
  // thereafter be configured with requested settings.
  EncodeSettings GetEncodeSettings(FrameEncodingInfo frame_info) override;

  // Should be called after each frame has completed encoding. If a baseline
  // comparison speed was set in the `EncodeSettings`, the `baseline_results`
  // parameter should be set with the results corresponding to those settings.
  void OnEncodedFrame(EncodeResults results,
                      std::optional<EncodeResults> baseline_results) override;

  const Config& config() const { return config_; }

 private:
  EncoderSpeedControllerImpl(const Config& config,
                             TimeDelta start_frame_interval);

  bool ShouldIncreaseSpeed() const;
  bool ShouldDecreaseSpeedDisregardingPsnr() const;
  bool PsnrProbeRequiredForNextSlowerSpeed() const;
  bool ShouldRecheckPsnrGain(Timestamp current_time) const;

  void ResetStats();
  void IncreaseSpeed();
  void DecreaseSpeed();

  const Config config_;
  TimeDelta frame_interval_;
  int current_speed_index_;

  // The number of frames recorded since last clearing the stats.
  int num_samples_;
  // Exponentially filtered measreuements of encode times and average frame qp.
  double slow_filtered_encode_time_ms_;
  double fast_filtered_encode_time_ms_;
  double filtered_qp_;

  // Timestamp of last request for a PSNR measurement, either due to periodic
  // sampling or requested for speed index change. Negative infinity if not set.
  Timestamp last_psnr_probe_;

  // Timestamp and speed level index of the last PSNR probing request for the
  // current layer. Note that we only track comparative PSNR gain checks here
  // (i.e. an alternate speed was given), single frame PSNR sampling does not
  // affect this value.
  struct PsnrGainCheck {
    int speed_level;
    Timestamp timestamp;
  };
  std::optional<PsnrGainCheck> last_psnr_gain_check_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_UTILITY_ENCODER_SPEED_CONTROLLER_IMPL_H_
