/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef MEDIA_ENGINE_VP8_ENCODER_SIMULCAST_PROXY_H_
#define MEDIA_ENGINE_VP8_ENCODER_SIMULCAST_PROXY_H_

#include <memory>
#include <vector>

#include "api/video_codecs/video_encoder_factory.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"

namespace webrtc {

// This class provides fallback to SimulcastEncoderAdapter if default VP8Encoder
// doesn't support simulcast for provided settings.
class VP8EncoderSimulcastProxy : public VP8Encoder {
 public:
  explicit VP8EncoderSimulcastProxy(VideoEncoderFactory* factory);
  virtual ~VP8EncoderSimulcastProxy();

  // Implements VideoEncoder.
  int Release() override;
  int InitEncode(const VideoCodec* inst,
                 int number_of_cores,
                 size_t max_payload_size) override;
  int Encode(const VideoFrame& input_image,
             const CodecSpecificInfo* codec_specific_info,
             const std::vector<FrameType>* frame_types) override;
  int RegisterEncodeCompleteCallback(EncodedImageCallback* callback) override;
  int SetChannelParameters(uint32_t packet_loss, int64_t rtt) override;
  int SetRateAllocation(const BitrateAllocation& bitrate,
                        uint32_t new_framerate) override;

  VideoEncoder::ScalingSettings GetScalingSettings() const override;

  int32_t SetPeriodicKeyFrames(bool enable) override;
  bool SupportsNativeHandle() const override;
  const char* ImplementationName() const override;

 private:
  VideoEncoderFactory* const factory_;
  std::unique_ptr<VideoEncoder> encoder_;
  EncodedImageCallback* callback_;
};

}  // namespace webrtc

#endif  // MEDIA_ENGINE_VP8_ENCODER_SIMULCAST_PROXY_H_
