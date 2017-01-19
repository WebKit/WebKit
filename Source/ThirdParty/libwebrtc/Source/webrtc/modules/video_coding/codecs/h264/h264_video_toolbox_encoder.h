/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_H264_H264_VIDEO_TOOLBOX_ENCODER_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_H264_H264_VIDEO_TOOLBOX_ENCODER_H_

#include "webrtc/base/criticalsection.h"
#include "webrtc/common_video/h264/h264_bitstream_parser.h"
#include "webrtc/common_video/include/bitrate_adjuster.h"
#include "webrtc/common_video/rotation.h"
#include "webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "webrtc/modules/video_coding/utility/quality_scaler.h"

#if defined(WEBRTC_VIDEO_TOOLBOX_SUPPORTED)

#include <VideoToolbox/VideoToolbox.h>
#include <vector>

// This file provides a H264 encoder implementation using the VideoToolbox
// APIs. Since documentation is almost non-existent, this is largely based on
// the information in the VideoToolbox header files, a talk from WWDC 2014 and
// experimentation.

namespace webrtc {

class H264VideoToolboxEncoder : public H264Encoder {
 public:
  H264VideoToolboxEncoder();

  ~H264VideoToolboxEncoder() override;

  int InitEncode(const VideoCodec* codec_settings,
                 int number_of_cores,
                 size_t max_payload_size) override;

  int Encode(const VideoFrame& input_image,
             const CodecSpecificInfo* codec_specific_info,
             const std::vector<FrameType>* frame_types) override;

  int RegisterEncodeCompleteCallback(EncodedImageCallback* callback) override;

  void OnDroppedFrame() override;
  int SetChannelParameters(uint32_t packet_loss, int64_t rtt) override;

  int SetRates(uint32_t new_bitrate_kbit, uint32_t frame_rate) override;

  int Release() override;

  const char* ImplementationName() const override;

  bool SupportsNativeHandle() const override;

  void OnEncodedFrame(OSStatus status,
                      VTEncodeInfoFlags info_flags,
                      CMSampleBufferRef sample_buffer,
                      CodecSpecificInfo codec_specific_info,
                      int32_t width,
                      int32_t height,
                      int64_t render_time_ms,
                      uint32_t timestamp,
                      VideoRotation rotation);

 private:
  int ResetCompressionSession();
  void ConfigureCompressionSession();
  void DestroyCompressionSession();
  rtc::scoped_refptr<VideoFrameBuffer> GetScaledBufferOnEncode(
      const rtc::scoped_refptr<VideoFrameBuffer>& frame);
  void SetBitrateBps(uint32_t bitrate_bps);
  void SetEncoderBitrateBps(uint32_t bitrate_bps);

  EncodedImageCallback* callback_;
  VTCompressionSessionRef compression_session_;
  BitrateAdjuster bitrate_adjuster_;
  uint32_t target_bitrate_bps_;
  uint32_t encoder_bitrate_bps_;
  int32_t width_;
  int32_t height_;

  rtc::CriticalSection quality_scaler_crit_;
  QualityScaler quality_scaler_ GUARDED_BY(quality_scaler_crit_);
  H264BitstreamParser h264_bitstream_parser_;
  bool enable_scaling_;
  std::vector<uint8_t> nv12_scale_buffer_;
};  // H264VideoToolboxEncoder

}  // namespace webrtc

#endif  // defined(WEBRTC_VIDEO_TOOLBOX_SUPPORTED)
#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_H264_H264_VIDEO_TOOLBOX_ENCODER_H_
