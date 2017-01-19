/* Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_VP9_SCREENSHARE_LAYERS_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_VP9_SCREENSHARE_LAYERS_H_

#include "webrtc/modules/video_coding/codecs/vp9/vp9_impl.h"

namespace webrtc {

class ScreenshareLayersVP9 {
 public:
  explicit ScreenshareLayersVP9(uint8_t num_layers);

  // The target bitrate for layer with id layer_id.
  void ConfigureBitrate(int threshold_kbps, uint8_t layer_id);

  // The current start layer.
  uint8_t GetStartLayer() const;

  // Update the layer with the size of the layer frame.
  void LayerFrameEncoded(unsigned int size_bytes, uint8_t layer_id);

  // Get the layer settings for the next superframe.
  //
  // In short, each time the GetSuperFrameSettings is called the
  // bitrate of every layer is calculated and if the cummulative
  // bitrate exceeds the configured cummulative bitrates
  // (ConfigureBitrate to configure) up to and including that
  // layer then the resulting encoding settings for the
  // superframe will only encode layers above that layer.
  VP9EncoderImpl::SuperFrameRefSettings GetSuperFrameSettings(
      uint32_t timestamp,
      bool is_keyframe);

 private:
  // How many layers that are used.
  uint8_t num_layers_;

  // The index of the first layer to encode.
  uint8_t start_layer_;

  // Cummulative target kbps for the different layers.
  float threshold_kbps_[kMaxVp9NumberOfSpatialLayers - 1];

  // How many bits that has been used for a certain layer. Increased in
  // FrameEncoded() by the size of the encoded frame and decreased in
  // GetSuperFrameSettings() depending on the time between frames.
  float bits_used_[kMaxVp9NumberOfSpatialLayers];

  // Timestamp of last frame.
  uint32_t last_timestamp_;

  // If the last_timestamp_ has been set.
  bool timestamp_initialized_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_VP9_SCREENSHARE_LAYERS_H_
