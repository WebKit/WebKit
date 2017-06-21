/* Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/
/*
* This file defines classes for doing temporal layers with VP8.
*/
#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_DEFAULT_TEMPORAL_LAYERS_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_DEFAULT_TEMPORAL_LAYERS_H_

#include <vector>

#include "webrtc/modules/video_coding/codecs/vp8/temporal_layers.h"

#include "webrtc/base/optional.h"

namespace webrtc {

class DefaultTemporalLayers : public TemporalLayers {
 public:
  DefaultTemporalLayers(int number_of_temporal_layers,
                        uint8_t initial_tl0_pic_idx);
  virtual ~DefaultTemporalLayers() {}

  // Returns the recommended VP8 encode flags needed. May refresh the decoder
  // and/or update the reference buffers.
  TemporalLayers::FrameConfig UpdateLayerConfig(uint32_t timestamp) override;

  // Update state based on new bitrate target and incoming framerate.
  // Returns the bitrate allocation for the active temporal layers.
  std::vector<uint32_t> OnRatesUpdated(int bitrate_kbps,
                                       int max_bitrate_kbps,
                                       int framerate) override;

  bool UpdateConfiguration(vpx_codec_enc_cfg_t* cfg) override;

  void PopulateCodecSpecific(bool frame_is_keyframe,
                             const TemporalLayers::FrameConfig& tl_config,
                             CodecSpecificInfoVP8* vp8_info,
                             uint32_t timestamp) override;

  void FrameEncoded(unsigned int size, int qp) override {}

  int GetTemporalLayerId(
      const TemporalLayers::FrameConfig& references) const override;

  uint8_t Tl0PicIdx() const override;

 private:
  const size_t num_layers_;
  const std::vector<unsigned int> temporal_ids_;
  const std::vector<bool> temporal_layer_sync_;
  const std::vector<TemporalLayers::FrameConfig> temporal_pattern_;

  uint8_t tl0_pic_idx_;
  uint8_t pattern_idx_;
  uint32_t timestamp_;
  bool last_base_layer_sync_;
  rtc::Optional<std::vector<uint32_t>> new_bitrates_kbps_;
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_DEFAULT_TEMPORAL_LAYERS_H_
