/* Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#include <algorithm>
#include "webrtc/modules/video_coding/codecs/vp9/screenshare_layers.h"
#include "webrtc/base/checks.h"

namespace webrtc {

ScreenshareLayersVP9::ScreenshareLayersVP9(uint8_t num_layers)
    : num_layers_(num_layers),
      start_layer_(0),
      last_timestamp_(0),
      timestamp_initialized_(false) {
  RTC_DCHECK_GT(num_layers, 0);
  RTC_DCHECK_LE(num_layers, kMaxVp9NumberOfSpatialLayers);
  memset(bits_used_, 0, sizeof(bits_used_));
  memset(threshold_kbps_, 0, sizeof(threshold_kbps_));
}

uint8_t ScreenshareLayersVP9::GetStartLayer() const {
  return start_layer_;
}

void ScreenshareLayersVP9::ConfigureBitrate(int threshold_kbps,
                                            uint8_t layer_id) {
  // The upper layer is always the layer we spill frames
  // to when the bitrate becomes to high, therefore setting
  // a max limit is not allowed. The top layer bitrate is
  // never used either so configuring it makes no difference.
  RTC_DCHECK_LT(layer_id, num_layers_ - 1);
  threshold_kbps_[layer_id] = threshold_kbps;
}

void ScreenshareLayersVP9::LayerFrameEncoded(unsigned int size_bytes,
                                             uint8_t layer_id) {
  RTC_DCHECK_LT(layer_id, num_layers_);
  bits_used_[layer_id] += size_bytes * 8;
}

VP9EncoderImpl::SuperFrameRefSettings
ScreenshareLayersVP9::GetSuperFrameSettings(uint32_t timestamp,
                                            bool is_keyframe) {
  VP9EncoderImpl::SuperFrameRefSettings settings;
  if (!timestamp_initialized_) {
    last_timestamp_ = timestamp;
    timestamp_initialized_ = true;
  }
  float time_diff = (timestamp - last_timestamp_) / 90.f;
  float total_bits_used = 0;
  float total_threshold_kbps = 0;
  start_layer_ = 0;

  // Up to (num_layers - 1) because we only have
  // (num_layers - 1) thresholds to check.
  for (int layer_id = 0; layer_id < num_layers_ - 1; ++layer_id) {
    bits_used_[layer_id] = std::max(
        0.f, bits_used_[layer_id] - time_diff * threshold_kbps_[layer_id]);
    total_bits_used += bits_used_[layer_id];
    total_threshold_kbps += threshold_kbps_[layer_id];

    // If this is a keyframe then there should be no
    // references to any previous frames.
    if (!is_keyframe) {
      settings.layer[layer_id].ref_buf1 = layer_id;
      if (total_bits_used > total_threshold_kbps * 1000)
        start_layer_ = layer_id + 1;
    }

    settings.layer[layer_id].upd_buf = layer_id;
  }
  // Since the above loop does not iterate over the last layer
  // the reference of the last layer has to be set after the loop,
  // and if this is a keyframe there should be no references to
  // any previous frames.
  if (!is_keyframe)
    settings.layer[num_layers_ - 1].ref_buf1 = num_layers_ - 1;

  settings.layer[num_layers_ - 1].upd_buf = num_layers_ - 1;
  settings.is_keyframe = is_keyframe;
  settings.start_layer = start_layer_;
  settings.stop_layer = num_layers_ - 1;
  last_timestamp_ = timestamp;
  return settings;
}

}  // namespace webrtc
