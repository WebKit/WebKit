/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef MODULES_VIDEO_CODING_CODECS_VP9_VP9_IMPL_H_
#define MODULES_VIDEO_CODING_CODECS_VP9_VP9_IMPL_H_

#include <memory>
#include <vector>

#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "modules/video_coding/codecs/vp9/vp9_frame_buffer_pool.h"

#include "vpx/vp8cx.h"
#include "vpx/vpx_decoder.h"
#include "vpx/vpx_encoder.h"

namespace webrtc {

class ScreenshareLayersVP9;

class VP9EncoderImpl : public VP9Encoder {
 public:
  VP9EncoderImpl();

  virtual ~VP9EncoderImpl();

  int Release() override;

  int InitEncode(const VideoCodec* codec_settings,
                 int number_of_cores,
                 size_t max_payload_size) override;

  int Encode(const VideoFrame& input_image,
             const CodecSpecificInfo* codec_specific_info,
             const std::vector<FrameType>* frame_types) override;

  int RegisterEncodeCompleteCallback(EncodedImageCallback* callback) override;

  int SetChannelParameters(uint32_t packet_loss, int64_t rtt) override;

  int SetRateAllocation(const BitrateAllocation& bitrate_allocation,
                        uint32_t frame_rate) override;

  const char* ImplementationName() const override;

  struct LayerFrameRefSettings {
    int8_t upd_buf = -1;   // -1 - no update,    0..7 - update buffer 0..7
    int8_t ref_buf1 = -1;  // -1 - no reference, 0..7 - reference buffer 0..7
    int8_t ref_buf2 = -1;  // -1 - no reference, 0..7 - reference buffer 0..7
    int8_t ref_buf3 = -1;  // -1 - no reference, 0..7 - reference buffer 0..7
  };

  struct SuperFrameRefSettings {
    LayerFrameRefSettings layer[kMaxVp9NumberOfSpatialLayers];
    uint8_t start_layer = 0;  // The first spatial layer to be encoded.
    uint8_t stop_layer = 0;   // The last spatial layer to be encoded.
    bool is_keyframe = false;
  };

 private:
  // Determine number of encoder threads to use.
  int NumberOfThreads(int width, int height, int number_of_cores);

  // Call encoder initialize function and set control settings.
  int InitAndSetControlSettings(const VideoCodec* inst);

  void PopulateCodecSpecific(CodecSpecificInfo* codec_specific,
                             const vpx_codec_cx_pkt& pkt,
                             uint32_t timestamp);

  bool ExplicitlyConfiguredSpatialLayers() const;
  bool SetSvcRates();

  // Used for flexible mode to set the flags and buffer references used
  // by the encoder. Also calculates the references used by the RTP
  // packetizer.
  //
  // Has to be called for every frame (keyframes included) to update the
  // state used to calculate references.
  vpx_svc_ref_frame_config GenerateRefsAndFlags(
      const SuperFrameRefSettings& settings);

  virtual int GetEncodedLayerFrame(const vpx_codec_cx_pkt* pkt);

  // Callback function for outputting packets per spatial layer.
  static void EncoderOutputCodedPacketCallback(vpx_codec_cx_pkt* pkt,
                                               void* user_data);

  // Determine maximum target for Intra frames
  //
  // Input:
  //    - optimal_buffer_size : Optimal buffer size
  // Return Value             : Max target size for Intra frames represented as
  //                            percentage of the per frame bandwidth
  uint32_t MaxIntraTarget(uint32_t optimal_buffer_size);

  EncodedImage encoded_image_;
  EncodedImageCallback* encoded_complete_callback_;
  VideoCodec codec_;
  bool inited_;
  int64_t timestamp_;
  int cpu_speed_;
  uint32_t rc_max_intra_target_;
  vpx_codec_ctx_t* encoder_;
  vpx_codec_enc_cfg_t* config_;
  vpx_image_t* raw_;
  vpx_svc_extra_cfg_t svc_params_;
  const VideoFrame* input_image_;
  GofInfoVP9 gof_;       // Contains each frame's temporal information for
                         // non-flexible mode.
  size_t frames_since_kf_;
  uint8_t num_temporal_layers_;
  uint8_t num_spatial_layers_;

  // Used for flexible mode.
  bool is_flexible_mode_;
  int64_t buffer_updated_at_frame_[kNumVp9Buffers];
  int64_t frames_encoded_;
  uint8_t num_ref_pics_[kMaxVp9NumberOfSpatialLayers];
  uint8_t p_diff_[kMaxVp9NumberOfSpatialLayers][kMaxVp9RefPics];
  std::unique_ptr<ScreenshareLayersVP9> spatial_layer_;

  // RTP state.
  uint16_t picture_id_;
  uint8_t tl0_pic_idx_;  // Only used in non-flexible mode.
};

class VP9DecoderImpl : public VP9Decoder {
 public:
  VP9DecoderImpl();

  virtual ~VP9DecoderImpl();

  int InitDecode(const VideoCodec* inst, int number_of_cores) override;

  int Decode(const EncodedImage& input_image,
             bool missing_frames,
             const RTPFragmentationHeader* fragmentation,
             const CodecSpecificInfo* codec_specific_info,
             int64_t /*render_time_ms*/) override;

  int RegisterDecodeCompleteCallback(DecodedImageCallback* callback) override;

  int Release() override;

  const char* ImplementationName() const override;

 private:
  int ReturnFrame(const vpx_image_t* img,
                  uint32_t timestamp,
                  int64_t ntp_time_ms,
                  int qp);

  // Memory pool used to share buffers between libvpx and webrtc.
  Vp9FrameBufferPool frame_buffer_pool_;
  DecodedImageCallback* decode_complete_callback_;
  bool inited_;
  vpx_codec_ctx_t* decoder_;
  VideoCodec codec_;
  bool key_frame_required_;
};
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_VP9_VP9_IMPL_H_
