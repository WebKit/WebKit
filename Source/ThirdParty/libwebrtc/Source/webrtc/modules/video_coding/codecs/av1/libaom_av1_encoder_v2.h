/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_AV1_LIBAOM_AV1_ENCODER_V2_H_
#define MODULES_VIDEO_CODING_CODECS_AV1_LIBAOM_AV1_ENCODER_V2_H_

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "api/scoped_refptr.h"
#include "api/video/resolution.h"
#include "api/video/video_codec_constants.h"
#include "api/video/video_frame_buffer.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder_factory_interface.h"
#include "api/video_codecs/video_encoder_interface.h"
#include "third_party/libaom/source/libaom/aom/aom_codec.h"
#include "third_party/libaom/source/libaom/aom/aom_encoder.h"
#include "third_party/libaom/source/libaom/aom/aom_image.h"

namespace webrtc {

class LibaomAv1EncoderV2 : public VideoEncoderInterface {
 public:
  LibaomAv1EncoderV2() = default;
  ~LibaomAv1EncoderV2() override;

  bool InitEncode(
      const VideoEncoderFactoryInterface::StaticEncoderSettings& settings,
      const std::map<std::string, std::string>& encoder_specific_settings);

  void Encode(scoped_refptr<VideoFrameBuffer> frame_buffer,
              const TemporalUnitSettings& tu_settings,
              std::vector<FrameEncodeSettings> frame_settings) override;

 private:
  using aom_img_ptr = std::unique_ptr<aom_image_t, decltype(&aom_img_free)>;

  aom_img_ptr image_to_encode_ = aom_img_ptr(nullptr, aom_img_free);
  aom_codec_ctx_t ctx_;
  aom_codec_enc_cfg_t cfg_;

  std::optional<VideoCodecMode> current_content_type_;
  std::array<std::optional<int>, kMaxSpatialLayers> current_effort_level_;
  int max_number_of_threads_;
  std::array<std::optional<Resolution>, 8> last_resolution_in_buffer_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_AV1_LIBAOM_AV1_ENCODER_V2_H_
