/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_ENGINE_VIDEODECODERSOFTWAREFALLBACKWRAPPER_H_
#define MEDIA_ENGINE_VIDEODECODERSOFTWAREFALLBACKWRAPPER_H_

#include <memory>
#include <string>

#include "api/video_codecs/video_decoder.h"

namespace webrtc {

// Class used to wrap external VideoDecoders to provide a fallback option on
// software decoding when a hardware decoder fails to decode a stream due to
// hardware restrictions, such as max resolution.
class VideoDecoderSoftwareFallbackWrapper : public VideoDecoder {
 public:
  VideoDecoderSoftwareFallbackWrapper(
      std::unique_ptr<VideoDecoder> sw_fallback_decoder,
      std::unique_ptr<VideoDecoder> hw_decoder);

  int32_t InitDecode(const VideoCodec* codec_settings,
                     int32_t number_of_cores) override;

  int32_t Decode(const EncodedImage& input_image,
                 bool missing_frames,
                 const RTPFragmentationHeader* fragmentation,
                 const CodecSpecificInfo* codec_specific_info,
                 int64_t render_time_ms) override;

  int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) override;

  int32_t Release() override;
  bool PrefersLateDecoding() const override;

  const char* ImplementationName() const override;

 private:
  bool InitFallbackDecoder();

  // Determines if we are trying to use the HW or SW decoder.
  bool use_hw_decoder_;
  std::unique_ptr<VideoDecoder> hw_decoder_;
  bool hw_decoder_initialized_;

  VideoCodec codec_settings_;
  int32_t number_of_cores_;
  const std::unique_ptr<VideoDecoder> fallback_decoder_;
  const std::string fallback_implementation_name_;
  DecodedImageCallback* callback_;
};

}  // namespace webrtc

#endif  // MEDIA_ENGINE_VIDEODECODERSOFTWAREFALLBACKWRAPPER_H_
