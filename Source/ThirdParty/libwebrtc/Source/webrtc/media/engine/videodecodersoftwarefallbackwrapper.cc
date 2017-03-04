/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/engine/videodecodersoftwarefallbackwrapper.h"

#include <string>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/media/engine/internaldecoderfactory.h"
#include "webrtc/modules/video_coding/include/video_error_codes.h"

namespace webrtc {

VideoDecoderSoftwareFallbackWrapper::VideoDecoderSoftwareFallbackWrapper(
    VideoCodecType codec_type,
    VideoDecoder* decoder)
    : codec_type_(codec_type),
      decoder_(decoder),
      decoder_initialized_(false),
      callback_(nullptr) {}

int32_t VideoDecoderSoftwareFallbackWrapper::InitDecode(
    const VideoCodec* codec_settings,
    int32_t number_of_cores) {
  RTC_DCHECK(!fallback_decoder_) << "Fallback decoder should never be "
                                    "initialized here, it should've been "
                                    "released.";
  codec_settings_ = *codec_settings;
  number_of_cores_ = number_of_cores;
  int32_t ret = decoder_->InitDecode(codec_settings, number_of_cores);
  if (ret != WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE) {
    decoder_initialized_ = (ret == WEBRTC_VIDEO_CODEC_OK);
    return ret;
  }
  decoder_initialized_ = false;

  // Try to initialize fallback decoder.
  if (InitFallbackDecoder())
    return WEBRTC_VIDEO_CODEC_OK;
  return ret;
}

bool VideoDecoderSoftwareFallbackWrapper::InitFallbackDecoder() {
  RTC_CHECK(codec_type_ != kVideoCodecUnknown)
      << "Decoder requesting fallback to codec not supported in software.";
  LOG(LS_WARNING) << "Decoder falling back to software decoding.";
  cricket::InternalDecoderFactory internal_decoder_factory;
  fallback_decoder_.reset(
      internal_decoder_factory.CreateVideoDecoder(codec_type_));
  if (fallback_decoder_->InitDecode(&codec_settings_, number_of_cores_) !=
      WEBRTC_VIDEO_CODEC_OK) {
    LOG(LS_ERROR) << "Failed to initialize software-decoder fallback.";
    fallback_decoder_.reset();
    return false;
  }
  if (callback_)
    fallback_decoder_->RegisterDecodeCompleteCallback(callback_);
  fallback_implementation_name_ =
      std::string(fallback_decoder_->ImplementationName()) +
      " (fallback from: " + decoder_->ImplementationName() + ")";
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
  if (!fallback_decoder_ || input_image._frameType == kVideoFrameKey) {
    int32_t ret = WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE;
    // Try reinitializing the decoder if it had failed before.
    if (!decoder_initialized_) {
      decoder_initialized_ =
          decoder_->InitDecode(&codec_settings_, number_of_cores_) ==
          WEBRTC_VIDEO_CODEC_OK;
    }
    if (decoder_initialized_) {
      ret = decoder_->Decode(input_image, missing_frames, fragmentation,
                             codec_specific_info, render_time_ms);
    }
    if (ret == WEBRTC_VIDEO_CODEC_OK) {
      if (fallback_decoder_) {
        // Decode OK -> stop using fallback decoder.
        LOG(LS_INFO)
            << "Decode OK, no longer using the software fallback decoder.";
        fallback_decoder_->Release();
        fallback_decoder_.reset();
        return WEBRTC_VIDEO_CODEC_OK;
      }
    }
    if (ret != WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE)
      return ret;
    if (!fallback_decoder_) {
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
  int32_t ret = decoder_->RegisterDecodeCompleteCallback(callback);
  if (fallback_decoder_)
    return fallback_decoder_->RegisterDecodeCompleteCallback(callback);
  return ret;
}

int32_t VideoDecoderSoftwareFallbackWrapper::Release() {
  if (fallback_decoder_) {
    LOG(LS_INFO) << "Releasing software fallback decoder.";
    fallback_decoder_->Release();
    fallback_decoder_.reset();
  }
  decoder_initialized_ = false;
  return decoder_->Release();
}

bool VideoDecoderSoftwareFallbackWrapper::PrefersLateDecoding() const {
  if (fallback_decoder_)
    return fallback_decoder_->PrefersLateDecoding();
  return decoder_->PrefersLateDecoding();
}

const char* VideoDecoderSoftwareFallbackWrapper::ImplementationName() const {
  if (fallback_decoder_)
    return fallback_implementation_name_.c_str();
  return decoder_->ImplementationName();
}

}  // namespace webrtc
