/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_ENGINE_MULTIPLEXCODECFACTORY_H_
#define MEDIA_ENGINE_MULTIPLEXCODECFACTORY_H_

#include <memory>
#include <vector>

#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"

namespace webrtc {

class MultiplexEncoderFactory : public VideoEncoderFactory {
 public:
  // supports_augmenting_data defines if the encoder would support augmenting
  // data in that case the encoder expects video frame buffer of type
  // AugmentedVideoFrameBuffer the encoder would encode the attached buffer and
  // data together if the flag is not set any frame buffer can be passed in
  MultiplexEncoderFactory(std::unique_ptr<VideoEncoderFactory> factory,
                          bool supports_augmenting_data = false);

  std::vector<SdpVideoFormat> GetSupportedFormats() const override;
  CodecInfo QueryVideoEncoder(const SdpVideoFormat& format) const override;
  std::unique_ptr<VideoEncoder> CreateVideoEncoder(
      const SdpVideoFormat& format) override;

 private:
  std::unique_ptr<VideoEncoderFactory> factory_;
  const bool supports_augmenting_data_;
};

class MultiplexDecoderFactory : public VideoDecoderFactory {
 public:
  // supports_augmenting_data defines if the decoder would support augmenting
  // data in that case the decoder expects the encoded video frame to contain
  // augmenting_data it is expected that the sender is using MultiplexEncoder
  // with supports_augmenting_data set
  MultiplexDecoderFactory(std::unique_ptr<VideoDecoderFactory> factory,
                          bool supports_augmenting_data = false);

  std::vector<SdpVideoFormat> GetSupportedFormats() const override;
  std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      const SdpVideoFormat& format) override;

 private:
  std::unique_ptr<VideoDecoderFactory> factory_;
  const bool supports_augmenting_data_;
};

}  // namespace webrtc

#endif  // MEDIA_ENGINE_MULTIPLEXCODECFACTORY_H_
