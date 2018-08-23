/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_decoder_software_fallback_wrapper.h"

#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/system/fallthrough.h"
#include "rtc_base/trace_event.h"

namespace webrtc {

namespace {

class VideoDecoderSoftwareFallbackWrapper final : public VideoDecoder {
 public:
  VideoDecoderSoftwareFallbackWrapper(
      std::unique_ptr<VideoDecoder> sw_fallback_decoder,
      std::unique_ptr<VideoDecoder> hw_decoder);
  ~VideoDecoderSoftwareFallbackWrapper() override;

  int32_t InitDecode(const VideoCodec* codec_settings,
                     int32_t number_of_cores) override;

  int32_t Decode(const EncodedImage& input_image,
                 bool missing_frames,
                 const CodecSpecificInfo* codec_specific_info,
                 int64_t render_time_ms) override;

  int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) override;

  int32_t Release() override;
  bool PrefersLateDecoding() const override;

  const char* ImplementationName() const override;

 private:
  bool InitFallbackDecoder();
  int32_t InitHwDecoder();

  VideoDecoder& active_decoder() const;

  // Determines if we are trying to use the HW or SW decoder.
  enum class DecoderType {
    kNone,
    kHardware,
    kFallback,
  } decoder_type_;
  std::unique_ptr<VideoDecoder> hw_decoder_;

  VideoCodec codec_settings_;
  int32_t number_of_cores_;
  const std::unique_ptr<VideoDecoder> fallback_decoder_;
  const std::string fallback_implementation_name_;
  DecodedImageCallback* callback_;
};

VideoDecoderSoftwareFallbackWrapper::VideoDecoderSoftwareFallbackWrapper(
    std::unique_ptr<VideoDecoder> sw_fallback_decoder,
    std::unique_ptr<VideoDecoder> hw_decoder)
    : decoder_type_(DecoderType::kNone),
      hw_decoder_(std::move(hw_decoder)),
      fallback_decoder_(std::move(sw_fallback_decoder)),
      fallback_implementation_name_(
          std::string(fallback_decoder_->ImplementationName()) +
          " (fallback from: " + hw_decoder_->ImplementationName() + ")"),
      callback_(nullptr) {}
VideoDecoderSoftwareFallbackWrapper::~VideoDecoderSoftwareFallbackWrapper() =
    default;

int32_t VideoDecoderSoftwareFallbackWrapper::InitDecode(
    const VideoCodec* codec_settings,
    int32_t number_of_cores) {
  codec_settings_ = *codec_settings;
  number_of_cores_ = number_of_cores;

  int32_t status = InitHwDecoder();
  if (status == WEBRTC_VIDEO_CODEC_OK) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  RTC_DCHECK(decoder_type_ == DecoderType::kNone);
  if (InitFallbackDecoder()) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  return status;
}

int32_t VideoDecoderSoftwareFallbackWrapper::InitHwDecoder() {
  RTC_DCHECK(decoder_type_ == DecoderType::kNone);
  int32_t status = hw_decoder_->InitDecode(&codec_settings_, number_of_cores_);
  if (status != WEBRTC_VIDEO_CODEC_OK) {
    return status;
  }

  decoder_type_ = DecoderType::kHardware;
  if (callback_)
    hw_decoder_->RegisterDecodeCompleteCallback(callback_);
  return status;
}

bool VideoDecoderSoftwareFallbackWrapper::InitFallbackDecoder() {
  RTC_DCHECK(decoder_type_ == DecoderType::kNone ||
             decoder_type_ == DecoderType::kHardware);
  RTC_LOG(LS_WARNING) << "Decoder falling back to software decoding.";
  int32_t status =
      fallback_decoder_->InitDecode(&codec_settings_, number_of_cores_);
  if (status != WEBRTC_VIDEO_CODEC_OK) {
    RTC_LOG(LS_ERROR) << "Failed to initialize software-decoder fallback.";
    return false;
  }

  if (decoder_type_ == DecoderType::kHardware) {
    hw_decoder_->Release();
  }
  decoder_type_ = DecoderType::kFallback;

  if (callback_)
    fallback_decoder_->RegisterDecodeCompleteCallback(callback_);
  return true;
}

int32_t VideoDecoderSoftwareFallbackWrapper::Decode(
    const EncodedImage& input_image,
    bool missing_frames,
    const CodecSpecificInfo* codec_specific_info,
    int64_t render_time_ms) {
  TRACE_EVENT0("webrtc", "VideoDecoderSoftwareFallbackWrapper::Decode");
  switch (decoder_type_) {
    case DecoderType::kNone:
      return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
    case DecoderType::kHardware: {
      int32_t ret = WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
      ret = hw_decoder_->Decode(input_image, missing_frames,
                                codec_specific_info, render_time_ms);
      if (ret != WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE) {
        return ret;
      }

      // HW decoder returned WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE or
      // initialization failed, fallback to software.
      if (!InitFallbackDecoder()) {
        return ret;
      }

      // Fallback decoder initialized, fall-through.
      RTC_FALLTHROUGH();
    }
    case DecoderType::kFallback:
      return fallback_decoder_->Decode(input_image, missing_frames,
                                       codec_specific_info, render_time_ms);
    default:
      RTC_NOTREACHED();
      return WEBRTC_VIDEO_CODEC_ERROR;
  }
}

int32_t VideoDecoderSoftwareFallbackWrapper::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  callback_ = callback;
  return active_decoder().RegisterDecodeCompleteCallback(callback);
}

int32_t VideoDecoderSoftwareFallbackWrapper::Release() {
  int32_t status;
  switch (decoder_type_) {
    case DecoderType::kHardware:
      status = hw_decoder_->Release();
      break;
    case DecoderType::kFallback:
      RTC_LOG(LS_INFO) << "Releasing software fallback decoder.";
      status = fallback_decoder_->Release();
      break;
    case DecoderType::kNone:
      status = WEBRTC_VIDEO_CODEC_OK;
      break;
    default:
      RTC_NOTREACHED();
      status = WEBRTC_VIDEO_CODEC_ERROR;
  }

  decoder_type_ = DecoderType::kNone;
  return status;
}

bool VideoDecoderSoftwareFallbackWrapper::PrefersLateDecoding() const {
  return active_decoder().PrefersLateDecoding();
}

const char* VideoDecoderSoftwareFallbackWrapper::ImplementationName() const {
  return decoder_type_ == DecoderType::kFallback
             ? fallback_implementation_name_.c_str()
             : hw_decoder_->ImplementationName();
}

VideoDecoder& VideoDecoderSoftwareFallbackWrapper::active_decoder() const {
  return decoder_type_ == DecoderType::kFallback ? *fallback_decoder_
                                                 : *hw_decoder_;
}

}  // namespace

std::unique_ptr<VideoDecoder> CreateVideoDecoderSoftwareFallbackWrapper(
    std::unique_ptr<VideoDecoder> sw_fallback_decoder,
    std::unique_ptr<VideoDecoder> hw_decoder) {
  return absl::make_unique<VideoDecoderSoftwareFallbackWrapper>(
      std::move(sw_fallback_decoder), std::move(hw_decoder));
}

}  // namespace webrtc
