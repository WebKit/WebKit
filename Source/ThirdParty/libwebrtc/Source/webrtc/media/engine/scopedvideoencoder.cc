/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/scopedvideoencoder.h"

#include <vector>

#include "api/video/video_bitrate_allocation.h"
#include "api/video_codecs/video_encoder.h"

namespace cricket {

namespace {

class ScopedVideoEncoder : public webrtc::VideoEncoder {
 public:
  ScopedVideoEncoder(WebRtcVideoEncoderFactory* factory,
                     webrtc::VideoEncoder* encoder);

  int32_t InitEncode(const webrtc::VideoCodec* codec_settings,
                     int32_t number_of_cores,
                     size_t max_payload_size) override;
  int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* callback) override;
  int32_t Release() override;
  int32_t Encode(const webrtc::VideoFrame& frame,
                 const webrtc::CodecSpecificInfo* codec_specific_info,
                 const std::vector<webrtc::FrameType>* frame_types) override;
  int32_t SetRates(uint32_t bitrate, uint32_t framerate) override;
  int32_t SetRateAllocation(const webrtc::VideoBitrateAllocation& allocation,
                            uint32_t framerate) override;
  EncoderInfo GetEncoderInfo() const override;
  ~ScopedVideoEncoder() override;

 private:
  WebRtcVideoEncoderFactory* factory_;
  webrtc::VideoEncoder* encoder_;
};

ScopedVideoEncoder::ScopedVideoEncoder(WebRtcVideoEncoderFactory* factory,
                                       webrtc::VideoEncoder* encoder)
    : factory_(factory), encoder_(encoder) {}

int32_t ScopedVideoEncoder::InitEncode(const webrtc::VideoCodec* codec_settings,
                                       int32_t number_of_cores,
                                       size_t max_payload_size) {
  return encoder_->InitEncode(codec_settings, number_of_cores,
                              max_payload_size);
}

int32_t ScopedVideoEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* callback) {
  return encoder_->RegisterEncodeCompleteCallback(callback);
}

int32_t ScopedVideoEncoder::Release() {
  return encoder_->Release();
}

int32_t ScopedVideoEncoder::Encode(
    const webrtc::VideoFrame& frame,
    const webrtc::CodecSpecificInfo* codec_specific_info,
    const std::vector<webrtc::FrameType>* frame_types) {
  return encoder_->Encode(frame, codec_specific_info, frame_types);
}

int32_t ScopedVideoEncoder::SetRates(uint32_t bitrate, uint32_t framerate) {
  return encoder_->SetRates(bitrate, framerate);
}

int32_t ScopedVideoEncoder::SetRateAllocation(
    const webrtc::VideoBitrateAllocation& allocation,
    uint32_t framerate) {
  return encoder_->SetRateAllocation(allocation, framerate);
}

webrtc::VideoEncoder::EncoderInfo ScopedVideoEncoder::GetEncoderInfo() const {
  return encoder_->GetEncoderInfo();
}

ScopedVideoEncoder::~ScopedVideoEncoder() {
  factory_->DestroyVideoEncoder(encoder_);
}

}  // namespace

std::unique_ptr<webrtc::VideoEncoder> CreateScopedVideoEncoder(
    WebRtcVideoEncoderFactory* factory,
    const VideoCodec& codec) {
  webrtc::VideoEncoder* encoder = factory->CreateVideoEncoder(codec);
  if (!encoder)
    return nullptr;
  return std::unique_ptr<webrtc::VideoEncoder>(
      new ScopedVideoEncoder(factory, encoder));
}

}  // namespace cricket
