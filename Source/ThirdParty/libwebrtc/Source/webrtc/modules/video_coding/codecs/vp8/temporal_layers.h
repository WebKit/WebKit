/* Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/
/*
* This file defines the interface for doing temporal layers with VP8.
*/
#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_TEMPORAL_LAYERS_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_TEMPORAL_LAYERS_H_

#include "vpx/vpx_encoder.h"

#include "webrtc/common_video/include/video_image.h"
#include "webrtc/typedefs.h"

namespace webrtc {

struct CodecSpecificInfoVP8;

class TemporalLayers {
 public:
  // Factory for TemporalLayer strategy. Default behaviour is a fixed pattern
  // of temporal layers. See default_temporal_layers.cc
  virtual ~TemporalLayers() {}

  // Returns the recommended VP8 encode flags needed. May refresh the decoder
  // and/or update the reference buffers.
  virtual int EncodeFlags(uint32_t timestamp) = 0;

  virtual bool ConfigureBitrates(int bitrate_kbit,
                                 int max_bitrate_kbit,
                                 int framerate,
                                 vpx_codec_enc_cfg_t* cfg) = 0;

  virtual void PopulateCodecSpecific(bool base_layer_sync,
                                     CodecSpecificInfoVP8* vp8_info,
                                     uint32_t timestamp) = 0;

  virtual void FrameEncoded(unsigned int size, uint32_t timestamp, int qp) = 0;

  virtual int CurrentLayerId() const = 0;

  virtual bool UpdateConfiguration(vpx_codec_enc_cfg_t* cfg) = 0;
};

class TemporalLayersFactory {
 public:
  virtual ~TemporalLayersFactory() {}
  virtual TemporalLayers* Create(int temporal_layers,
                                 uint8_t initial_tl0_pic_idx) const;
};

// Factory for a temporal layers strategy that adaptively changes the number of
// layers based on input framerate so that the base layer has an acceptable
// framerate. See realtime_temporal_layers.cc
class RealTimeTemporalLayersFactory : public TemporalLayersFactory {
 public:
  ~RealTimeTemporalLayersFactory() override {}
  TemporalLayers* Create(int num_temporal_layers,
                         uint8_t initial_tl0_pic_idx) const override;
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_TEMPORAL_LAYERS_H_
