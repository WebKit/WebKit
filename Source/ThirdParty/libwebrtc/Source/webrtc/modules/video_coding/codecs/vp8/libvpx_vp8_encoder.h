/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_VP8_LIBVPX_VP8_ENCODER_H_
#define MODULES_VIDEO_CODING_CODECS_VP8_LIBVPX_VP8_ENCODER_H_

#include <memory>
#include <vector>

#include "api/video/video_frame.h"
#include "api/video_codecs/video_encoder.h"
#include "common_types.h"  // NOLINT(build/include)
#include "common_video/include/video_frame.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp8/temporal_layers.h"
#include "modules/video_coding/include/video_codec_interface.h"

#include "vpx/vp8cx.h"
#include "vpx/vpx_encoder.h"

namespace webrtc {

class LibvpxVp8Encoder : public VP8Encoder {
 public:
  LibvpxVp8Encoder();
  ~LibvpxVp8Encoder() override;

  int Release() override;

  int InitEncode(const VideoCodec* codec_settings,
                 int number_of_cores,
                 size_t max_payload_size) override;

  int Encode(const VideoFrame& input_image,
             const CodecSpecificInfo* codec_specific_info,
             const std::vector<FrameType>* frame_types) override;

  int RegisterEncodeCompleteCallback(EncodedImageCallback* callback) override;

  int SetChannelParameters(uint32_t packet_loss, int64_t rtt) override;

  int SetRateAllocation(const VideoBitrateAllocation& bitrate,
                        uint32_t new_framerate) override;

  ScalingSettings GetScalingSettings() const override;

  const char* ImplementationName() const override;

  static vpx_enc_frame_flags_t EncodeFlags(
      const TemporalLayers::FrameConfig& references);

 private:
  void SetupTemporalLayers(int num_streams,
                           int num_temporal_layers,
                           const VideoCodec& codec);

  // Set the cpu_speed setting for encoder based on resolution and/or platform.
  int SetCpuSpeed(int width, int height);

  // Determine number of encoder threads to use.
  int NumberOfThreads(int width, int height, int number_of_cores);

  // Call encoder initialize function and set control settings.
  int InitAndSetControlSettings();

  void PopulateCodecSpecific(CodecSpecificInfo* codec_specific,
                             const TemporalLayers::FrameConfig& tl_config,
                             const vpx_codec_cx_pkt& pkt,
                             int stream_idx,
                             uint32_t timestamp);

  int GetEncodedPartitions(const TemporalLayers::FrameConfig tl_configs[],
                           const VideoFrame& input_image);

  // Set the stream state for stream |stream_idx|.
  void SetStreamState(bool send_stream, int stream_idx);

  uint32_t MaxIntraTarget(uint32_t optimal_buffer_size);

  const bool use_gf_boost_;

  EncodedImageCallback* encoded_complete_callback_;
  VideoCodec codec_;
  bool inited_;
  int64_t timestamp_;
  int qp_max_;
  int cpu_speed_default_;
  int number_of_cores_;
  uint32_t rc_max_intra_target_;
  std::vector<std::unique_ptr<TemporalLayers>> temporal_layers_;
  std::vector<std::unique_ptr<TemporalLayersChecker>> temporal_layers_checkers_;
  std::vector<bool> key_frame_request_;
  std::vector<bool> send_stream_;
  std::vector<int> cpu_speed_;
  std::vector<vpx_image_t> raw_images_;
  std::vector<EncodedImage> encoded_images_;
  std::vector<vpx_codec_ctx_t> encoders_;
  std::vector<vpx_codec_enc_cfg_t> configurations_;
  std::vector<vpx_rational_t> downsampling_factors_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_VP8_LIBVPX_VP8_ENCODER_H_
