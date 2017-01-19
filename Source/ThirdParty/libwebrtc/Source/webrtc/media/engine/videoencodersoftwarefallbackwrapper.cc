/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/engine/videoencodersoftwarefallbackwrapper.h"

#include "webrtc/base/logging.h"
#include "webrtc/modules/video_coding/include/video_error_codes.h"

namespace webrtc {

VideoEncoderSoftwareFallbackWrapper::VideoEncoderSoftwareFallbackWrapper(
    VideoCodecType codec_type,
    webrtc::VideoEncoder* encoder)
    : rates_set_(false),
      channel_parameters_set_(false),
      encoder_type_(CodecToEncoderType(codec_type)),
      encoder_(encoder),
      callback_(nullptr) {}

bool VideoEncoderSoftwareFallbackWrapper::InitFallbackEncoder() {
  if (!VideoEncoder::IsSupportedSoftware(encoder_type_)) {
    LOG(LS_WARNING)
        << "Encoder requesting fallback to codec not supported in software.";
    return false;
  }
  fallback_encoder_.reset(VideoEncoder::Create(encoder_type_));
  if (fallback_encoder_->InitEncode(&codec_settings_, number_of_cores_,
                                    max_payload_size_) !=
      WEBRTC_VIDEO_CODEC_OK) {
    LOG(LS_ERROR) << "Failed to initialize software-encoder fallback.";
    fallback_encoder_->Release();
    fallback_encoder_.reset();
    return false;
  }
  // Replay callback, rates, and channel parameters.
  if (callback_)
    fallback_encoder_->RegisterEncodeCompleteCallback(callback_);
  if (rates_set_)
    fallback_encoder_->SetRates(bitrate_, framerate_);
  if (channel_parameters_set_)
    fallback_encoder_->SetChannelParameters(packet_loss_, rtt_);

  fallback_implementation_name_ =
      std::string(fallback_encoder_->ImplementationName()) +
      " (fallback from: " + encoder_->ImplementationName() + ")";
  // Since we're switching to the fallback encoder, Release the real encoder. It
  // may be re-initialized via InitEncode later, and it will continue to get
  // Set calls for rates and channel parameters in the meantime.
  encoder_->Release();
  return true;
}

int32_t VideoEncoderSoftwareFallbackWrapper::InitEncode(
    const VideoCodec* codec_settings,
    int32_t number_of_cores,
    size_t max_payload_size) {
  // Store settings, in case we need to dynamically switch to the fallback
  // encoder after a failed Encode call.
  codec_settings_ = *codec_settings;
  number_of_cores_ = number_of_cores;
  max_payload_size_ = max_payload_size;
  // Clear stored rate/channel parameters.
  rates_set_ = false;
  channel_parameters_set_ = false;

  int32_t ret =
      encoder_->InitEncode(codec_settings, number_of_cores, max_payload_size);
  if (ret == WEBRTC_VIDEO_CODEC_OK || encoder_type_ == kUnsupportedCodec) {
    if (fallback_encoder_)
      fallback_encoder_->Release();
    fallback_encoder_.reset();
    if (callback_)
      encoder_->RegisterEncodeCompleteCallback(callback_);
    return ret;
  }
  // Try to instantiate software codec.
  if (InitFallbackEncoder()) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  // Software encoder failed, use original return code.
  return ret;
}

int32_t VideoEncoderSoftwareFallbackWrapper::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  callback_ = callback;
  int32_t ret = encoder_->RegisterEncodeCompleteCallback(callback);
  if (fallback_encoder_)
    return fallback_encoder_->RegisterEncodeCompleteCallback(callback);
  return ret;
}

int32_t VideoEncoderSoftwareFallbackWrapper::Release() {
  // If the fallback_encoder_ is non-null, it means it was created via
  // InitFallbackEncoder which has Release()d encoder_, so we should only ever
  // need to Release() whichever one is active.
  if (fallback_encoder_)
    return fallback_encoder_->Release();
  return encoder_->Release();
}

int32_t VideoEncoderSoftwareFallbackWrapper::Encode(
    const VideoFrame& frame,
    const CodecSpecificInfo* codec_specific_info,
    const std::vector<FrameType>* frame_types) {
  if (fallback_encoder_)
    return fallback_encoder_->Encode(frame, codec_specific_info, frame_types);
  int32_t ret = encoder_->Encode(frame, codec_specific_info, frame_types);
  // If requested, try a software fallback.
  if (ret == WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE && InitFallbackEncoder()) {
    if (frame.video_frame_buffer()->native_handle() &&
        !fallback_encoder_->SupportsNativeHandle()) {
      LOG(LS_WARNING) << "Fallback encoder doesn't support native frames, "
                      << "dropping one frame.";
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    // Fallback was successful, so start using it with this frame.
    return fallback_encoder_->Encode(frame, codec_specific_info, frame_types);
  }
  return ret;
}

int32_t VideoEncoderSoftwareFallbackWrapper::SetChannelParameters(
    uint32_t packet_loss,
    int64_t rtt) {
  channel_parameters_set_ = true;
  packet_loss_ = packet_loss;
  rtt_ = rtt;
  int32_t ret = encoder_->SetChannelParameters(packet_loss, rtt);
  if (fallback_encoder_)
    return fallback_encoder_->SetChannelParameters(packet_loss, rtt);
  return ret;
}

int32_t VideoEncoderSoftwareFallbackWrapper::SetRates(uint32_t bitrate,
                                                      uint32_t framerate) {
  rates_set_ = true;
  bitrate_ = bitrate;
  framerate_ = framerate;
  int32_t ret = encoder_->SetRates(bitrate, framerate);
  if (fallback_encoder_)
    return fallback_encoder_->SetRates(bitrate, framerate);
  return ret;
}

void VideoEncoderSoftwareFallbackWrapper::OnDroppedFrame() {
  if (fallback_encoder_)
    return fallback_encoder_->OnDroppedFrame();
  return encoder_->OnDroppedFrame();
}

bool VideoEncoderSoftwareFallbackWrapper::SupportsNativeHandle() const {
  if (fallback_encoder_)
    return fallback_encoder_->SupportsNativeHandle();
  return encoder_->SupportsNativeHandle();
}

}  // namespace webrtc
