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

#include <map>
#include <memory>
#include <vector>

#include "modules/video_coding/codecs/vp9/include/vp9.h"

#include "media/base/vp9_profile.h"
#include "modules/video_coding/codecs/vp9/vp9_frame_buffer_pool.h"
#include "modules/video_coding/utility/framerate_controller.h"

#include "vpx/vp8cx.h"
#include "vpx/vpx_decoder.h"
#include "vpx/vpx_encoder.h"

namespace webrtc {

class VP9EncoderImpl : public VP9Encoder {
 public:
  explicit VP9EncoderImpl(const cricket::VideoCodec& codec);

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

  int SetRateAllocation(const VideoBitrateAllocation& bitrate_allocation,
                        uint32_t frame_rate) override;

  const char* ImplementationName() const override;

 private:
  // Determine number of encoder threads to use.
  int NumberOfThreads(int width, int height, int number_of_cores);

  // Call encoder initialize function and set control settings.
  int InitAndSetControlSettings(const VideoCodec* inst);

  void PopulateCodecSpecific(CodecSpecificInfo* codec_specific,
                             absl::optional<int>* spatial_idx,
                             const vpx_codec_cx_pkt& pkt,
                             uint32_t timestamp,
                             bool first_frame_in_picture);
  void FillReferenceIndices(const vpx_codec_cx_pkt& pkt,
                            const size_t pic_num,
                            const bool inter_layer_predicted,
                            CodecSpecificInfoVP9* vp9_info);
  void UpdateReferenceBuffers(const vpx_codec_cx_pkt& pkt,
                              const size_t pic_num);
  vpx_svc_ref_frame_config_t SetReferences(bool is_key_pic);

  bool ExplicitlyConfiguredSpatialLayers() const;
  bool SetSvcRates(const VideoBitrateAllocation& bitrate_allocation);

  virtual int GetEncodedLayerFrame(const vpx_codec_cx_pkt* pkt);

  // Callback function for outputting packets per spatial layer.
  static void EncoderOutputCodedPacketCallback(vpx_codec_cx_pkt* pkt,
                                               void* user_data);

  void DeliverBufferedFrame(bool end_of_picture);

  bool DropFrame(uint8_t spatial_idx, uint32_t rtp_timestamp);

  // Determine maximum target for Intra frames
  //
  // Input:
  //    - optimal_buffer_size : Optimal buffer size
  // Return Value             : Max target size for Intra frames represented as
  //                            percentage of the per frame bandwidth
  uint32_t MaxIntraTarget(uint32_t optimal_buffer_size);

  EncodedImage encoded_image_;
  CodecSpecificInfo codec_specific_;
  EncodedImageCallback* encoded_complete_callback_;
  VideoCodec codec_;
  const VP9Profile profile_;
  bool inited_;
  int64_t timestamp_;
  int cpu_speed_;
  uint32_t rc_max_intra_target_;
  vpx_codec_ctx_t* encoder_;
  vpx_codec_enc_cfg_t* config_;
  vpx_image_t* raw_;
  vpx_svc_extra_cfg_t svc_params_;
  const VideoFrame* input_image_;
  GofInfoVP9 gof_;  // Contains each frame's temporal information for
                    // non-flexible mode.
  bool force_key_frame_;
  size_t pics_since_key_;
  uint8_t num_temporal_layers_;
  uint8_t num_spatial_layers_;         // Number of configured SLs
  uint8_t num_active_spatial_layers_;  // Number of actively encoded SLs
  bool layer_deactivation_requires_key_frame_;
  bool is_svc_;
  InterLayerPredMode inter_layer_pred_;
  bool external_ref_control_;

  std::vector<FramerateController> framerate_controller_;

  // Used for flexible mode.
  bool is_flexible_mode_;
  struct RefFrameBuffer {
    RefFrameBuffer(size_t pic_num,
                   size_t spatial_layer_id,
                   size_t temporal_layer_id)
        : pic_num(pic_num),
          spatial_layer_id(spatial_layer_id),
          temporal_layer_id(temporal_layer_id) {}
    RefFrameBuffer() {}

    bool operator==(const RefFrameBuffer& o) {
      return pic_num == o.pic_num && spatial_layer_id == o.spatial_layer_id &&
             temporal_layer_id == o.temporal_layer_id;
    }

    size_t pic_num = 0;
    size_t spatial_layer_id = 0;
    size_t temporal_layer_id = 0;
  };
  std::map<size_t, RefFrameBuffer> ref_buf_;
};

class VP9DecoderImpl : public VP9Decoder {
 public:
  VP9DecoderImpl();

  virtual ~VP9DecoderImpl();

  int InitDecode(const VideoCodec* inst, int number_of_cores) override;

  int Decode(const EncodedImage& input_image,
             bool missing_frames,
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
  bool key_frame_required_;
};
}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_VP9_VP9_IMPL_H_
