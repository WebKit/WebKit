/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_INCLUDE_VIDEO_CODEC_INTERFACE_H_
#define MODULES_VIDEO_CODING_INCLUDE_VIDEO_CODEC_INTERFACE_H_

#include <vector>

#include "api/video/video_frame.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/include/module_common_types.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

class RTPFragmentationHeader;  // forward declaration

// Note: if any pointers are added to this struct, it must be fitted
// with a copy-constructor. See below.
struct CodecSpecificInfoVP8 {
  int16_t pictureId;  // Negative value to skip pictureId.
  bool nonReference;
  uint8_t simulcastIdx;
  uint8_t temporalIdx;
  bool layerSync;
  int tl0PicIdx;  // Negative value to skip tl0PicIdx.
  int8_t keyIdx;  // Negative value to skip keyIdx.
};

struct CodecSpecificInfoVP9 {
  int16_t picture_id;  // Negative value to skip pictureId.

  bool inter_pic_predicted;  // This layer frame is dependent on previously
                             // coded frame(s).
  bool flexible_mode;
  bool ss_data_available;

  int tl0_pic_idx;  // Negative value to skip tl0PicIdx.
  uint8_t temporal_idx;
  uint8_t spatial_idx;
  bool temporal_up_switch;
  bool inter_layer_predicted;  // Frame is dependent on directly lower spatial
                               // layer frame.
  uint8_t gof_idx;

  // SS data.
  size_t num_spatial_layers;  // Always populated.
  bool spatial_layer_resolution_present;
  uint16_t width[kMaxVp9NumberOfSpatialLayers];
  uint16_t height[kMaxVp9NumberOfSpatialLayers];
  GofInfoVP9 gof;

  // Frame reference data.
  uint8_t num_ref_pics;
  uint8_t p_diff[kMaxVp9RefPics];
};

struct CodecSpecificInfoGeneric {
  uint8_t simulcast_idx;
};

struct CodecSpecificInfoH264 {
  H264PacketizationMode packetization_mode;
};

struct CodecSpecificInfoStereo {
  VideoCodecType associated_codec_type;
  StereoIndices indices;
};

union CodecSpecificInfoUnion {
  CodecSpecificInfoGeneric generic;
  CodecSpecificInfoVP8 VP8;
  CodecSpecificInfoVP9 VP9;
  CodecSpecificInfoH264 H264;
  CodecSpecificInfoStereo stereo;
};

// Note: if any pointers are added to this struct or its sub-structs, it
// must be fitted with a copy-constructor. This is because it is copied
// in the copy-constructor of VCMEncodedFrame.
struct CodecSpecificInfo {
  CodecSpecificInfo() : codecType(kVideoCodecUnknown), codec_name(nullptr) {}
  VideoCodecType codecType;
  const char* codec_name;
  CodecSpecificInfoUnion codecSpecific;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_INCLUDE_VIDEO_CODEC_INTERFACE_H_
