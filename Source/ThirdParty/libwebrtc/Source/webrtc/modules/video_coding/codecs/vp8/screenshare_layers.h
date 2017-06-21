/* Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/
#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_SCREENSHARE_LAYERS_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_SCREENSHARE_LAYERS_H_

#include <vector>

#include "webrtc/base/rate_statistics.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/modules/video_coding/codecs/vp8/temporal_layers.h"
#include "webrtc/modules/video_coding/utility/frame_dropper.h"
#include "webrtc/typedefs.h"

namespace webrtc {

struct CodecSpecificInfoVP8;
class Clock;

class ScreenshareLayers : public TemporalLayers {
 public:
  static const double kMaxTL0FpsReduction;
  static const double kAcceptableTargetOvershoot;
  static const int kMaxFrameIntervalMs;

  ScreenshareLayers(int num_temporal_layers,
                    uint8_t initial_tl0_pic_idx,
                    Clock* clock);
  virtual ~ScreenshareLayers();

  // Returns the recommended VP8 encode flags needed. May refresh the decoder
  // and/or update the reference buffers.
  TemporalLayers::FrameConfig UpdateLayerConfig(uint32_t timestamp) override;

  // Update state based on new bitrate target and incoming framerate.
  // Returns the bitrate allocation for the active temporal layers.
  std::vector<uint32_t> OnRatesUpdated(int bitrate_kbps,
                                       int max_bitrate_kbps,
                                       int framerate) override;

  // Update the encoder configuration with target bitrates or other parameters.
  // Returns true iff the configuration was actually modified.
  bool UpdateConfiguration(vpx_codec_enc_cfg_t* cfg) override;

  void PopulateCodecSpecific(bool base_layer_sync,
                             const TemporalLayers::FrameConfig& tl_config,
                             CodecSpecificInfoVP8* vp8_info,
                             uint32_t timestamp) override;

  void FrameEncoded(unsigned int size, int qp) override;

  int GetTemporalLayerId(
      const TemporalLayers::FrameConfig& tl_config) const override;

  uint8_t Tl0PicIdx() const override;

 private:
  enum class TemporalLayerState : int { kDrop, kTl0, kTl1, kTl1Sync };

  bool TimeToSync(int64_t timestamp) const;
  uint32_t GetCodecTargetBitrateKbps() const;

  Clock* const clock_;

  int number_of_temporal_layers_;
  bool last_base_layer_sync_;
  uint8_t tl0_pic_idx_;
  int active_layer_;
  int64_t last_timestamp_;
  int64_t last_sync_timestamp_;
  int64_t last_emitted_tl0_timestamp_;
  rtc::TimestampWrapAroundHandler time_wrap_handler_;
  int min_qp_;
  int max_qp_;
  uint32_t max_debt_bytes_;

  // Configured max framerate.
  rtc::Optional<uint32_t> target_framerate_;
  // Incoming framerate from capturer.
  rtc::Optional<uint32_t> capture_framerate_;
  // Tracks what framerate we actually encode, and drops frames on overshoot.
  RateStatistics encode_framerate_;
  bool bitrate_updated_;

  static constexpr int kMaxNumTemporalLayers = 2;
  struct TemporalLayer {
    TemporalLayer()
        : state(State::kNormal),
          enhanced_max_qp(-1),
          last_qp(-1),
          debt_bytes_(0),
          target_rate_kbps_(0) {}

    enum class State {
      kNormal,
      kDropped,
      kReencoded,
      kQualityBoost,
    } state;

    int enhanced_max_qp;
    int last_qp;
    uint32_t debt_bytes_;
    uint32_t target_rate_kbps_;

    void UpdateDebt(int64_t delta_ms);
  } layers_[kMaxNumTemporalLayers];

  void UpdateHistograms();
  // Data for histogram statistics.
  struct Stats {
    int64_t first_frame_time_ms_ = -1;
    int64_t num_tl0_frames_ = 0;
    int64_t num_tl1_frames_ = 0;
    int64_t num_dropped_frames_ = 0;
    int64_t num_overshoots_ = 0;
    int64_t tl0_qp_sum_ = 0;
    int64_t tl1_qp_sum_ = 0;
    int64_t tl0_target_bitrate_sum_ = 0;
    int64_t tl1_target_bitrate_sum_ = 0;
  } stats_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_SCREENSHARE_LAYERS_H_
