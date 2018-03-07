/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/scopedvideodecoder.h"

#include <vector>

#include "api/video_codecs/video_decoder.h"

namespace cricket {

namespace {

class ScopedVideoDecoder : public webrtc::VideoDecoder {
 public:
  ScopedVideoDecoder(WebRtcVideoDecoderFactory* factory,
                     webrtc::VideoDecoder* decoder);

  int32_t InitDecode(const webrtc::VideoCodec* codec_settings,
                     int32_t number_of_cores) override;
  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* callback) override;
  int32_t Release() override;
  int32_t Decode(const webrtc::EncodedImage& input_image,
                 bool missing_frames,
                 const webrtc::RTPFragmentationHeader* fragmentation,
                 const webrtc::CodecSpecificInfo* codec_specific_info,
                 int64_t render_time_ms) override;
  bool PrefersLateDecoding() const override;
  const char* ImplementationName() const override;

  ~ScopedVideoDecoder() override;

 private:
  WebRtcVideoDecoderFactory* factory_;
  webrtc::VideoDecoder* decoder_;
};

ScopedVideoDecoder::ScopedVideoDecoder(WebRtcVideoDecoderFactory* factory,
                                       webrtc::VideoDecoder* decoder)
    : factory_(factory), decoder_(decoder) {}

int32_t ScopedVideoDecoder::InitDecode(const webrtc::VideoCodec* codec_settings,
                                       int32_t number_of_cores) {
  return decoder_->InitDecode(codec_settings, number_of_cores);
}

int32_t ScopedVideoDecoder::RegisterDecodeCompleteCallback(
    webrtc::DecodedImageCallback* callback) {
  return decoder_->RegisterDecodeCompleteCallback(callback);
}

int32_t ScopedVideoDecoder::Release() {
  return decoder_->Release();
}

int32_t ScopedVideoDecoder::Decode(
    const webrtc::EncodedImage& input_image,
    bool missing_frames,
    const webrtc::RTPFragmentationHeader* fragmentation,
    const webrtc::CodecSpecificInfo* codec_specific_info,
    int64_t render_time_ms) {
  return decoder_->Decode(input_image, missing_frames, fragmentation,
                          codec_specific_info, render_time_ms);
}

bool ScopedVideoDecoder::PrefersLateDecoding() const {
  return decoder_->PrefersLateDecoding();
}

const char* ScopedVideoDecoder::ImplementationName() const {
  return decoder_->ImplementationName();
}

ScopedVideoDecoder::~ScopedVideoDecoder() {
  factory_->DestroyVideoDecoder(decoder_);
}

}  // namespace

std::unique_ptr<webrtc::VideoDecoder> CreateScopedVideoDecoder(
    WebRtcVideoDecoderFactory* factory,
    const VideoCodec& codec,
    VideoDecoderParams params) {
  webrtc::VideoDecoder* decoder =
      factory->CreateVideoDecoderWithParams(codec, params);
  if (!decoder)
    return nullptr;
  return std::unique_ptr<webrtc::VideoDecoder>(
      new ScopedVideoDecoder(factory, decoder));
}

}  // namespace cricket
