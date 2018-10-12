/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/convert_legacy_video_factory.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_decoder_software_fallback_wrapper.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "api/video_codecs/video_encoder_software_fallback_wrapper.h"
#include "media/base/h264_profile_level_id.h"
#include "media/engine/internaldecoderfactory.h"
#include "media/engine/internalencoderfactory.h"
#include "media/engine/scopedvideodecoder.h"
#include "media/engine/scopedvideoencoder.h"
#include "media/engine/simulcast_encoder_adapter.h"
#include "media/engine/vp8_encoder_simulcast_proxy.h"
#include "media/engine/webrtcvideodecoderfactory.h"
#include "media/engine/webrtcvideoencoderfactory.h"
#include "rtc_base/checks.h"

namespace cricket {

namespace {

bool IsFormatSupported(
    const std::vector<webrtc::SdpVideoFormat>& supported_formats,
    const webrtc::SdpVideoFormat& format) {
  for (const webrtc::SdpVideoFormat& supported_format : supported_formats) {
    if (IsSameCodec(format.name, format.parameters, supported_format.name,
                    supported_format.parameters)) {
      return true;
    }
  }
  return false;
}

// Converts the cricket::WebRtcVideoEncoderFactory to a
// webrtc::VideoEncoderFactory (without adding any simulcast or SW fallback).
class CricketToWebRtcEncoderFactory : public webrtc::VideoEncoderFactory {
 public:
  explicit CricketToWebRtcEncoderFactory(
      std::unique_ptr<WebRtcVideoEncoderFactory> external_encoder_factory)
      : external_encoder_factory_(std::move(external_encoder_factory)) {}

  webrtc::VideoEncoderFactory::CodecInfo QueryVideoEncoder(
      const webrtc::SdpVideoFormat& format) const override {
    CodecInfo info;
    info.has_internal_source = false;
    info.is_hardware_accelerated = false;
    if (!external_encoder_factory_)
      return info;

    info.has_internal_source =
        external_encoder_factory_->EncoderTypeHasInternalSource(
            webrtc::PayloadStringToCodecType(format.name));
    info.is_hardware_accelerated = true;
    return info;
  }

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
    if (!external_encoder_factory_)
      return std::vector<webrtc::SdpVideoFormat>();

    std::vector<webrtc::SdpVideoFormat> formats;
    for (const VideoCodec& codec :
         external_encoder_factory_->supported_codecs()) {
      formats.push_back(webrtc::SdpVideoFormat(codec.name, codec.params));
    }
    return formats;
  }

  std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
      const webrtc::SdpVideoFormat& format) override {
    return CreateScopedVideoEncoder(external_encoder_factory_.get(),
                                    VideoCodec(format));
  }

 private:
  const std::unique_ptr<WebRtcVideoEncoderFactory> external_encoder_factory_;
};

// This class combines an external factory with the internal factory and adds
// internal SW codecs, simulcast, and SW fallback wrappers.
class EncoderAdapter : public webrtc::VideoEncoderFactory {
 public:
  explicit EncoderAdapter(
      std::unique_ptr<WebRtcVideoEncoderFactory> external_encoder_factory)
      : internal_encoder_factory_(new webrtc::InternalEncoderFactory()),
        external_encoder_factory_(
            absl::make_unique<CricketToWebRtcEncoderFactory>(
                std::move(external_encoder_factory))) {}

  explicit EncoderAdapter(
      std::unique_ptr<webrtc::VideoEncoderFactory> external_encoder_factory)
      : internal_encoder_factory_(new webrtc::InternalEncoderFactory()),
        external_encoder_factory_(std::move(external_encoder_factory)) {}

  webrtc::VideoEncoderFactory::CodecInfo QueryVideoEncoder(
      const webrtc::SdpVideoFormat& format) const override {
    if (IsFormatSupported(external_encoder_factory_->GetSupportedFormats(),
                          format)) {
      return external_encoder_factory_->QueryVideoEncoder(format);
    }

    // Format must be one of the internal formats.
    RTC_DCHECK(IsFormatSupported(
        internal_encoder_factory_->GetSupportedFormats(), format));
    webrtc::VideoEncoderFactory::CodecInfo info;
    info.has_internal_source = false;
    info.is_hardware_accelerated = false;
    return info;
  }

  std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
      const webrtc::SdpVideoFormat& format) override {
    // Try creating internal encoder.
    std::unique_ptr<webrtc::VideoEncoder> internal_encoder;
    if (IsFormatSupported(internal_encoder_factory_->GetSupportedFormats(),
                          format)) {
      internal_encoder =
          CodecNamesEq(format.name.c_str(), kVp8CodecName)
              ? absl::make_unique<webrtc::VP8EncoderSimulcastProxy>(
                    internal_encoder_factory_.get(), format)
              : internal_encoder_factory_->CreateVideoEncoder(format);
    }

    // Try creating external encoder.
    std::unique_ptr<webrtc::VideoEncoder> external_encoder;
    if (IsFormatSupported(external_encoder_factory_->GetSupportedFormats(),
                          format)) {
      external_encoder =
          CodecNamesEq(format.name.c_str(), kVp8CodecName)
              ? absl::make_unique<webrtc::SimulcastEncoderAdapter>(
                    external_encoder_factory_.get(), format)
              : external_encoder_factory_->CreateVideoEncoder(format);
    }

    if (internal_encoder && external_encoder) {
      // Both internal SW encoder and external HW encoder available - create
      // fallback encoder.
      return webrtc::CreateVideoEncoderSoftwareFallbackWrapper(
          std::move(internal_encoder), std::move(external_encoder));
    }
    return external_encoder ? std::move(external_encoder)
                            : std::move(internal_encoder);
  }

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
    std::vector<webrtc::SdpVideoFormat> formats =
        internal_encoder_factory_->GetSupportedFormats();

    // Add external codecs.
    for (const webrtc::SdpVideoFormat& format :
         external_encoder_factory_->GetSupportedFormats()) {
      // Don't add same codec twice.
      if (!IsFormatSupported(formats, format))
        formats.push_back(format);
    }

    return formats;
  }

 private:
  const std::unique_ptr<webrtc::VideoEncoderFactory> internal_encoder_factory_;
  const std::unique_ptr<webrtc::VideoEncoderFactory> external_encoder_factory_;
};

// Converts the cricket::WebRtcVideoDecoderFactory to a
// webrtc::VideoDecoderFactory (without adding any SW fallback).
class CricketToWebRtcDecoderFactory : public webrtc::VideoDecoderFactory {
 public:
  explicit CricketToWebRtcDecoderFactory(
      std::unique_ptr<WebRtcVideoDecoderFactory> external_decoder_factory)
      : external_decoder_factory_(std::move(external_decoder_factory)) {}

  std::unique_ptr<webrtc::VideoDecoder> CreateVideoDecoder(
      const webrtc::SdpVideoFormat& format) override {
    return LegacyCreateVideoDecoder(format, /* receive_stream_id= */ "");
  }

  std::unique_ptr<webrtc::VideoDecoder> LegacyCreateVideoDecoder(
      const webrtc::SdpVideoFormat& format,
      const std::string& receive_stream_id) override {
    if (external_decoder_factory_ != nullptr) {
      return CreateScopedVideoDecoder(external_decoder_factory_.get(),
                                      VideoCodec(format), {receive_stream_id});
    }
    return nullptr;
  }

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
    // This is not implemented for the legacy decoder factory.
    RTC_NOTREACHED();
    return std::vector<webrtc::SdpVideoFormat>();
  }

 private:
  const std::unique_ptr<WebRtcVideoDecoderFactory> external_decoder_factory_;
};

// This class combines an external factory with the internal factory and adds
// internal SW codecs and SW fallback wrappers.
class DecoderAdapter : public webrtc::VideoDecoderFactory {
 public:
  explicit DecoderAdapter(
      std::unique_ptr<WebRtcVideoDecoderFactory> external_decoder_factory)
      : internal_decoder_factory_(new webrtc::InternalDecoderFactory()),
        external_decoder_factory_(
            absl::make_unique<CricketToWebRtcDecoderFactory>(
                std::move(external_decoder_factory))) {}

  std::unique_ptr<webrtc::VideoDecoder> CreateVideoDecoder(
      const webrtc::SdpVideoFormat& format) override {
    return LegacyCreateVideoDecoder(format, /* receive_stream_id= */ "");
  }

  std::unique_ptr<webrtc::VideoDecoder> LegacyCreateVideoDecoder(
      const webrtc::SdpVideoFormat& format,
      const std::string& receive_stream_id) override {
    std::unique_ptr<webrtc::VideoDecoder> internal_decoder;
    if (IsFormatSupported(internal_decoder_factory_->GetSupportedFormats(),
                          format)) {
      internal_decoder = internal_decoder_factory_->CreateVideoDecoder(format);
    }

    std::unique_ptr<webrtc::VideoDecoder> external_decoder = nullptr;
    if (external_decoder_factory_ != nullptr) {
      external_decoder = external_decoder_factory_->LegacyCreateVideoDecoder(
          format, receive_stream_id);
    }

    if (internal_decoder && external_decoder) {
      // Both internal SW decoder and external HW decoder available - create
      // fallback decoder.
      return webrtc::CreateVideoDecoderSoftwareFallbackWrapper(
          std::move(internal_decoder), std::move(external_decoder));
    }
    return external_decoder ? std::move(external_decoder)
                            : std::move(internal_decoder);
  }

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override {
    std::vector<webrtc::SdpVideoFormat> formats =
        internal_decoder_factory_->GetSupportedFormats();

    // Add external codecs.
    for (const webrtc::SdpVideoFormat& format :
         external_decoder_factory_->GetSupportedFormats()) {
      // Don't add same codec twice.
      if (!IsFormatSupported(formats, format))
        formats.push_back(format);
    }

    return formats;
  }

 private:
  const std::unique_ptr<webrtc::VideoDecoderFactory> internal_decoder_factory_;
  const std::unique_ptr<webrtc::VideoDecoderFactory> external_decoder_factory_;
};

}  // namespace

std::unique_ptr<webrtc::VideoEncoderFactory> ConvertVideoEncoderFactory(
    std::unique_ptr<WebRtcVideoEncoderFactory> external_encoder_factory) {
  return std::unique_ptr<webrtc::VideoEncoderFactory>(
      new EncoderAdapter(std::move(external_encoder_factory)));
}

std::unique_ptr<webrtc::VideoDecoderFactory> ConvertVideoDecoderFactory(
    std::unique_ptr<WebRtcVideoDecoderFactory> external_decoder_factory) {
  return std::unique_ptr<webrtc::VideoDecoderFactory>(
      new DecoderAdapter(std::move(external_decoder_factory)));
}

}  // namespace cricket
