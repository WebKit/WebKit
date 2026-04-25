/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/fake_webrtc_video_engine.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "api/environment/environment.h"
#include "api/fec_controller_override.h"
#include "api/units/time_delta.h"
#include "api/video/encoded_image.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_type.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "media/base/codec.h"
#include "media/base/media_constants.h"
#include "media/engine/simulcast_encoder_adapter.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/logging.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

namespace {

constexpr TimeDelta kEventTimeout = TimeDelta::Seconds(10);

bool IsScalabilityModeSupported(const std::vector<SdpVideoFormat>& formats,
                                std::optional<std::string> scalability_mode) {
  if (!scalability_mode.has_value()) {
    return true;
  }
  for (const auto& format : formats) {
    for (const auto& mode : format.scalability_modes) {
      if (ScalabilityModeToString(mode) == scalability_mode)
        return true;
    }
  }
  return false;
}

}  // namespace

// Decoder.
FakeWebRtcVideoDecoder::FakeWebRtcVideoDecoder(
    FakeWebRtcVideoDecoderFactory* factory)
    : num_frames_received_(0), factory_(factory) {}

FakeWebRtcVideoDecoder::~FakeWebRtcVideoDecoder() {
  if (factory_) {
    factory_->DecoderDestroyed(this);
  }
}

bool FakeWebRtcVideoDecoder::Configure(const Settings& /* settings */) {
  return true;
}

int32_t FakeWebRtcVideoDecoder::Decode(const EncodedImage&, int64_t) {
  num_frames_received_++;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t FakeWebRtcVideoDecoder::RegisterDecodeCompleteCallback(
    DecodedImageCallback*) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t FakeWebRtcVideoDecoder::Release() {
  return WEBRTC_VIDEO_CODEC_OK;
}

int FakeWebRtcVideoDecoder::GetNumFramesReceived() const {
  return num_frames_received_;
}

// Decoder factory.
FakeWebRtcVideoDecoderFactory::FakeWebRtcVideoDecoderFactory()
    : num_created_decoders_(0) {}

std::vector<SdpVideoFormat> FakeWebRtcVideoDecoderFactory::GetSupportedFormats()
    const {
  std::vector<SdpVideoFormat> formats;

  for (const SdpVideoFormat& format : supported_codec_formats_) {
    // We need to test erroneous scenarios, so just warn if there's
    // a duplicate.
    if (format.IsCodecInList(formats)) {
      RTC_LOG(LS_WARNING) << "GetSupportedFormats found a duplicate format: "
                          << format << ", check that this is expected.";
    }
    formats.push_back(format);
  }

  return formats;
}

std::unique_ptr<VideoDecoder> FakeWebRtcVideoDecoderFactory::Create(
    const Environment& /* env */,
    const SdpVideoFormat& format) {
  if (format.IsCodecInList(supported_codec_formats_)) {
    num_created_decoders_++;
    std::unique_ptr<FakeWebRtcVideoDecoder> decoder =
        std::make_unique<FakeWebRtcVideoDecoder>(this);
    decoders_.push_back(decoder.get());
    return decoder;
  }

  return nullptr;
}

void FakeWebRtcVideoDecoderFactory::DecoderDestroyed(
    FakeWebRtcVideoDecoder* decoder) {
  std::erase(decoders_, decoder);
}

void FakeWebRtcVideoDecoderFactory::AddSupportedVideoCodec(
    const SdpVideoFormat& format) {
  supported_codec_formats_.push_back(format);
}

void FakeWebRtcVideoDecoderFactory::AddSupportedVideoCodecType(
    const std::string& name) {
  // This is to match the default H264 params of Codec.
  Codec video_codec = CreateVideoCodec(name);
  supported_codec_formats_.push_back(
      SdpVideoFormat(video_codec.name, video_codec.params));
}

int FakeWebRtcVideoDecoderFactory::GetNumCreatedDecoders() {
  return num_created_decoders_;
}

const std::vector<FakeWebRtcVideoDecoder*>&
FakeWebRtcVideoDecoderFactory::decoders() {
  return decoders_;
}

// Encoder.
FakeWebRtcVideoEncoder::FakeWebRtcVideoEncoder(
    FakeWebRtcVideoEncoderFactory* factory)
    : num_frames_encoded_(0), factory_(factory) {}

FakeWebRtcVideoEncoder::~FakeWebRtcVideoEncoder() {
  if (factory_) {
    factory_->EncoderDestroyed(this);
  }
}

void FakeWebRtcVideoEncoder::SetFecControllerOverride(
    FecControllerOverride* /* fec_controller_override */) {
  // Ignored.
}

int32_t FakeWebRtcVideoEncoder::InitEncode(
    const VideoCodec* codecSettings,
    const VideoEncoder::Settings& /* settings */) {
  MutexLock lock(&mutex_);
  codec_settings_ = *codecSettings;
  init_encode_event_.Set();
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t FakeWebRtcVideoEncoder::Encode(
    const VideoFrame& /* inputImage */,
    const std::vector<VideoFrameType>* /* frame_types */) {
  MutexLock lock(&mutex_);
  ++num_frames_encoded_;
  init_encode_event_.Set();
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t FakeWebRtcVideoEncoder::RegisterEncodeCompleteCallback(
    EncodedImageCallback* /* callback */) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t FakeWebRtcVideoEncoder::Release() {
  return WEBRTC_VIDEO_CODEC_OK;
}

void FakeWebRtcVideoEncoder::SetRates(
    const RateControlParameters& /* parameters */) {}

VideoEncoder::EncoderInfo FakeWebRtcVideoEncoder::GetEncoderInfo() const {
  EncoderInfo info;
  info.is_hardware_accelerated = true;
  return info;
}

bool FakeWebRtcVideoEncoder::WaitForInitEncode() {
  return init_encode_event_.Wait(kEventTimeout);
}

VideoCodec FakeWebRtcVideoEncoder::GetCodecSettings() {
  MutexLock lock(&mutex_);
  return codec_settings_;
}

int FakeWebRtcVideoEncoder::GetNumEncodedFrames() {
  MutexLock lock(&mutex_);
  return num_frames_encoded_;
}

// Video encoder factory.
FakeWebRtcVideoEncoderFactory::FakeWebRtcVideoEncoderFactory()
    : num_created_encoders_(0), vp8_factory_mode_(false) {}

std::vector<SdpVideoFormat> FakeWebRtcVideoEncoderFactory::GetSupportedFormats()
    const {
  std::vector<SdpVideoFormat> formats;

  for (const SdpVideoFormat& format : formats_) {
    // Don't add same codec twice.
    if (!format.IsCodecInList(formats))
      formats.push_back(format);
  }

  return formats;
}

VideoEncoderFactory::CodecSupport
FakeWebRtcVideoEncoderFactory::QueryCodecSupport(
    const SdpVideoFormat& format,
    std::optional<std::string> scalability_mode) const {
  std::vector<SdpVideoFormat> supported_formats;
  for (const auto& f : formats_) {
    if (format.IsSameCodec(f))
      supported_formats.push_back(f);
  }
  if (format.IsCodecInList(formats_)) {
    return {.is_supported = IsScalabilityModeSupported(supported_formats,
                                                       scalability_mode)};
  }
  return {.is_supported = false};
}

std::unique_ptr<VideoEncoder> FakeWebRtcVideoEncoderFactory::Create(
    const Environment& env,
    const SdpVideoFormat& format) {
  MutexLock lock(&mutex_);
  std::unique_ptr<VideoEncoder> encoder;
  if (format.IsCodecInList(formats_)) {
    if (absl::EqualsIgnoreCase(format.name, kVp8CodecName) &&
        !vp8_factory_mode_) {
      // The simulcast adapter will ask this factory for multiple VP8
      // encoders. Enter vp8_factory_mode so that we now create these encoders
      // instead of more adapters.
      vp8_factory_mode_ = true;
      encoder = std::make_unique<SimulcastEncoderAdapter>(
          env, /*primary_factory=*/this, /*fallback_factory=*/nullptr, format);
    } else {
      num_created_encoders_++;
      encoder = std::make_unique<FakeWebRtcVideoEncoder>(this);
      encoders_.push_back(static_cast<FakeWebRtcVideoEncoder*>(encoder.get()));
    }
  }
  return encoder;
}

void FakeWebRtcVideoEncoderFactory::EncoderDestroyed(
    FakeWebRtcVideoEncoder* encoder) {
  MutexLock lock(&mutex_);
  std::erase(encoders_, encoder);
}

void FakeWebRtcVideoEncoderFactory::AddSupportedVideoCodec(
    const SdpVideoFormat& format) {
  formats_.push_back(format);
}

void FakeWebRtcVideoEncoderFactory::AddSupportedVideoCodecType(
    const std::string& name,
    const std::vector<ScalabilityMode>& scalability_modes) {
  // This is to match the default H264 params of Codec.
  Codec video_codec = CreateVideoCodec(name);
  formats_.push_back(
      SdpVideoFormat(video_codec.name, video_codec.params,
                     {scalability_modes.begin(), scalability_modes.end()}));
}

int FakeWebRtcVideoEncoderFactory::GetNumCreatedEncoders() {
  MutexLock lock(&mutex_);
  return num_created_encoders_;
}

const std::vector<FakeWebRtcVideoEncoder*>
FakeWebRtcVideoEncoderFactory::encoders() {
  MutexLock lock(&mutex_);
  return encoders_;
}

}  // namespace webrtc
