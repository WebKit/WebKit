/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_ENGINE_VIDEODECODERSOFTWAREFALLBACKWRAPPER_H_
#define WEBRTC_MEDIA_ENGINE_VIDEODECODERSOFTWAREFALLBACKWRAPPER_H_

#include <memory>
#include <string>

#include "webrtc/api/video_codecs/video_decoder.h"

namespace webrtc {

// Class used to wrap external VideoDecoders to provide a fallback option on
// software decoding when a hardware decoder fails to decode a stream due to
// hardware restrictions, such as max resolution.
class VideoDecoderSoftwareFallbackWrapper : public webrtc::VideoDecoder {
 public:
  VideoDecoderSoftwareFallbackWrapper(VideoCodecType codec_type,
                                      VideoDecoder* decoder);

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

  const VideoCodecType codec_type_;
  VideoDecoder* const decoder_;
  bool decoder_initialized_;

  VideoCodec codec_settings_;
  int32_t number_of_cores_;
  std::string fallback_implementation_name_;
  std::unique_ptr<VideoDecoder> fallback_decoder_;
  DecodedImageCallback* callback_;
};

}  // namespace webrtc

#endif  // WEBRTC_MEDIA_ENGINE_VIDEODECODERSOFTWAREFALLBACKWRAPPER_H_
