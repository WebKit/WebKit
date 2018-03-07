/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/videodecodersoftwarefallbackwrapper.h"

#include <string>
#include <utility>

#include "media/engine/internaldecoderfactory.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/trace_event.h"

namespace webrtc {

VideoDecoderSoftwareFallbackWrapper::VideoDecoderSoftwareFallbackWrapper(
    std::unique_ptr<VideoDecoder> sw_fallback_decoder,
    std::unique_ptr<VideoDecoder> hw_decoder)
    : use_hw_decoder_(true),
      hw_decoder_(std::move(hw_decoder)),
      hw_decoder_initialized_(false),
      fallback_decoder_(std::move(sw_fallback_decoder)),
      fallback_implementation_name_(
          std::string(fallback_decoder_->ImplementationName()) +
          " (fallback from: " + hw_decoder_->ImplementationName() + ")"),
      callback_(nullptr) {}

int32_t VideoDecoderSoftwareFallbackWrapper::InitDecode(
    const VideoCodec* codec_settings,
    int32_t number_of_cores) {
  // Always try to use the HW decoder in this state.
  use_hw_decoder_ = true;
  codec_settings_ = *codec_settings;
  number_of_cores_ = number_of_cores;
  int32_t ret = hw_decoder_->InitDecode(codec_settings, number_of_cores);
  if (ret == WEBRTC_VIDEO_CODEC_OK) {
    hw_decoder_initialized_ = true;
    return ret;
  }
  hw_decoder_initialized_ = false;

  // Try to initialize fallback decoder.
  if (InitFallbackDecoder())
    return WEBRTC_VIDEO_CODEC_OK;

  return ret;
}

bool VideoDecoderSoftwareFallbackWrapper::InitFallbackDecoder() {
  RTC_LOG(LS_WARNING) << "Decoder falling back to software decoding.";
  if (fallback_decoder_->InitDecode(&codec_settings_, number_of_cores_) !=
      WEBRTC_VIDEO_CODEC_OK) {
    RTC_LOG(LS_ERROR) << "Failed to initialize software-decoder fallback.";
    use_hw_decoder_ = true;
    return false;
  }
  if (callback_)
    fallback_decoder_->RegisterDecodeCompleteCallback(callback_);
  use_hw_decoder_ = false;
  return true;
}

int32_t VideoDecoderSoftwareFallbackWrapper::Decode(
    const EncodedImage& input_image,
    bool missing_frames,
    const RTPFragmentationHeader* fragmentation,
    const CodecSpecificInfo* codec_specific_info,
    int64_t render_time_ms) {
    TRACE_EVENT0("webrtc", "VideoDecoderSoftwareFallbackWrapper::Decode");
  // Try initializing and decoding with the provided decoder on every keyframe
  // or when there's no fallback decoder. This is the normal case.
  if (use_hw_decoder_ || input_image._frameType == kVideoFrameKey) {
    int32_t ret = WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
    // Try reinitializing the decoder if it had failed before.
    if (!hw_decoder_initialized_) {
      hw_decoder_initialized_ =
          hw_decoder_->InitDecode(&codec_settings_, number_of_cores_) ==
          WEBRTC_VIDEO_CODEC_OK;
    }
    if (hw_decoder_initialized_) {
      ret = hw_decoder_->Decode(input_image, missing_frames, fragmentation,
                             codec_specific_info, render_time_ms);
    }
    if (ret == WEBRTC_VIDEO_CODEC_OK) {
      if (!use_hw_decoder_) {
        // Decode OK -> stop using fallback decoder.
        RTC_LOG(LS_WARNING)
            << "Decode OK, no longer using the software fallback decoder.";
        use_hw_decoder_ = true;
        return WEBRTC_VIDEO_CODEC_OK;
      }
    }
    if (ret != WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE)
      return ret;
    if (use_hw_decoder_) {
      // Try to initialize fallback decoder.
      if (!InitFallbackDecoder())
        return ret;
    }
  }
  return fallback_decoder_->Decode(input_image, missing_frames, fragmentation,
                                   codec_specific_info, render_time_ms);
}

int32_t VideoDecoderSoftwareFallbackWrapper::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  callback_ = callback;
  int32_t ret = hw_decoder_->RegisterDecodeCompleteCallback(callback);
  if (!use_hw_decoder_)
    return fallback_decoder_->RegisterDecodeCompleteCallback(callback);
  return ret;
}

int32_t VideoDecoderSoftwareFallbackWrapper::Release() {
  if (!use_hw_decoder_) {
    RTC_LOG(LS_INFO) << "Releasing software fallback decoder.";
    fallback_decoder_->Release();
  }
  hw_decoder_initialized_ = false;
  return hw_decoder_->Release();
}

bool VideoDecoderSoftwareFallbackWrapper::PrefersLateDecoding() const {
  return use_hw_decoder_ ? hw_decoder_->PrefersLateDecoding()
                         : fallback_decoder_->PrefersLateDecoding();
}

const char* VideoDecoderSoftwareFallbackWrapper::ImplementationName() const {
  return use_hw_decoder_ ? hw_decoder_->ImplementationName()
                         : fallback_implementation_name_.c_str();
}

}  // namespace webrtc
