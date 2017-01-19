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

#include "webrtc/modules/video_coding/codecs/vp8/temporal_layers.h"

namespace webrtc {

class DefaultTemporalLayers : public TemporalLayers {
 public:
  DefaultTemporalLayers(int number_of_temporal_layers,
                        uint8_t initial_tl0_pic_idx);
  virtual ~DefaultTemporalLayers() {}

  // Returns the recommended VP8 encode flags needed. May refresh the decoder
  // and/or update the reference buffers.
  int EncodeFlags(uint32_t timestamp) override;

  bool ConfigureBitrates(int bitrate_kbit,
                         int max_bitrate_kbit,
                         int framerate,
                         vpx_codec_enc_cfg_t* cfg) override;

  void PopulateCodecSpecific(bool base_layer_sync,
                             CodecSpecificInfoVP8* vp8_info,
                             uint32_t timestamp) override;

  void FrameEncoded(unsigned int size, uint32_t timestamp, int qp) override {}

  bool UpdateConfiguration(vpx_codec_enc_cfg_t* cfg) override { return false; }

  int CurrentLayerId() const override;

 private:
  enum TemporalReferences {
    // For 1 layer case: reference all (last, golden, and alt ref), but only
    // update last.
    kTemporalUpdateLastRefAll = 12,
    // First base layer frame for 3 temporal layers, which updates last and
    // golden with alt ref dependency.
    kTemporalUpdateLastAndGoldenRefAltRef = 11,
    // First enhancement layer with alt ref dependency.
    kTemporalUpdateGoldenRefAltRef = 10,
    // First enhancement layer with alt ref dependency.
    kTemporalUpdateGoldenWithoutDependencyRefAltRef = 9,
    // Base layer with alt ref dependency.
    kTemporalUpdateLastRefAltRef = 8,
    // Highest enhacement layer without dependency on golden with alt ref
    // dependency.
    kTemporalUpdateNoneNoRefGoldenRefAltRef = 7,
    // Second layer and last frame in cycle, for 2 layers.
    kTemporalUpdateNoneNoRefAltref = 6,
    // Highest enhancement layer.
    kTemporalUpdateNone = 5,
    // Second enhancement layer.
    kTemporalUpdateAltref = 4,
    // Second enhancement layer without dependency on previous frames in
    // the second enhancement layer.
    kTemporalUpdateAltrefWithoutDependency = 3,
    // First enhancement layer.
    kTemporalUpdateGolden = 2,
    // First enhancement layer without dependency on previous frames in
    // the first enhancement layer.
    kTemporalUpdateGoldenWithoutDependency = 1,
    // Base layer.
    kTemporalUpdateLast = 0,
  };
  enum { kMaxTemporalPattern = 16 };

  int number_of_temporal_layers_;
  int temporal_ids_length_;
  int temporal_ids_[kMaxTemporalPattern];
  int temporal_pattern_length_;
  TemporalReferences temporal_pattern_[kMaxTemporalPattern];
  uint8_t tl0_pic_idx_;
  uint8_t pattern_idx_;
  uint32_t timestamp_;
  bool last_base_layer_sync_;
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_DEFAULT_TEMPORAL_LAYERS_H_
