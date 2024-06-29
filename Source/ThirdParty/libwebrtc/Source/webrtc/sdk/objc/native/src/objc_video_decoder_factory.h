/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_OBJC_NATIVE_SRC_OBJC_VIDEO_DECODER_FACTORY_H_
#define SDK_OBJC_NATIVE_SRC_OBJC_VIDEO_DECODER_FACTORY_H_

#include "api/video_codecs/video_decoder_factory.h"
#include "media/base/codec.h"

@protocol RTCVideoDecoderFactory;

namespace webrtc {

class ObjCVideoDecoderFactory : public VideoDecoderFactory {
 public:
  explicit ObjCVideoDecoderFactory(id<RTCVideoDecoderFactory>);
  ~ObjCVideoDecoderFactory() override;

  id<RTCVideoDecoderFactory> wrapped_decoder_factory() const;

  std::vector<SdpVideoFormat> GetSupportedFormats() const override;
  std::unique_ptr<VideoDecoder> Create(
      const Environment& environment,
      const SdpVideoFormat& format) override;
  CodecSupport QueryCodecSupport(const SdpVideoFormat&, bool reference_scaling) const final;

 private:
  id<RTCVideoDecoderFactory> decoder_factory_;
};

}  // namespace webrtc

#endif  // SDK_OBJC_NATIVE_SRC_OBJC_VIDEO_DECODER_FACTORY_H_
