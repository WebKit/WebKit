/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/vp8_encoder_simulcast_proxy.h"

#include "media/engine/simulcast_encoder_adapter.h"

namespace webrtc {
VP8EncoderSimulcastProxy::VP8EncoderSimulcastProxy(VideoEncoderFactory* factory,
                                                   const SdpVideoFormat& format)
    : factory_(factory), video_format_(format), callback_(nullptr) {
  encoder_ = factory_->CreateVideoEncoder(format);
}

VP8EncoderSimulcastProxy::VP8EncoderSimulcastProxy(VideoEncoderFactory* factory)
    : VP8EncoderSimulcastProxy(factory, SdpVideoFormat("VP8")) {}

VP8EncoderSimulcastProxy::~VP8EncoderSimulcastProxy() {}

int VP8EncoderSimulcastProxy::Release() {
  return encoder_->Release();
}

int VP8EncoderSimulcastProxy::InitEncode(const VideoCodec* inst,
                                         int number_of_cores,
                                         size_t max_payload_size) {
  int ret = encoder_->InitEncode(inst, number_of_cores, max_payload_size);
  if (ret == WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED) {
    encoder_.reset(new SimulcastEncoderAdapter(factory_, video_format_));
    if (callback_) {
      encoder_->RegisterEncodeCompleteCallback(callback_);
    }
    ret = encoder_->InitEncode(inst, number_of_cores, max_payload_size);
  }
  return ret;
}

int VP8EncoderSimulcastProxy::Encode(
    const VideoFrame& input_image,
    const CodecSpecificInfo* codec_specific_info,
    const std::vector<FrameType>* frame_types) {
  return encoder_->Encode(input_image, codec_specific_info, frame_types);
}

int VP8EncoderSimulcastProxy::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  callback_ = callback;
  return encoder_->RegisterEncodeCompleteCallback(callback);
}

int VP8EncoderSimulcastProxy::SetRateAllocation(
    const VideoBitrateAllocation& bitrate,
    uint32_t new_framerate) {
  return encoder_->SetRateAllocation(bitrate, new_framerate);
}

VideoEncoder::EncoderInfo VP8EncoderSimulcastProxy::GetEncoderInfo() const {
  return encoder_->GetEncoderInfo();
}

}  // namespace webrtc
