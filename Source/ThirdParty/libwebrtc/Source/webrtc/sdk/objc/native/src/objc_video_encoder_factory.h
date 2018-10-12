/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_OBJC_NATIVE_SRC_OBJC_VIDEO_ENCODER_FACTORY_H_
#define SDK_OBJC_NATIVE_SRC_OBJC_VIDEO_ENCODER_FACTORY_H_

#import <Foundation/Foundation.h>

#include "api/video_codecs/video_encoder_factory.h"

@protocol RTCVideoEncoderFactory;

namespace webrtc {

class ObjCVideoEncoderFactory : public VideoEncoderFactory {
 public:
  explicit ObjCVideoEncoderFactory(id<RTCVideoEncoderFactory>);
  ~ObjCVideoEncoderFactory() override;

  id<RTCVideoEncoderFactory> wrapped_encoder_factory() const;

  std::vector<SdpVideoFormat> GetSupportedFormats() const override;
  std::unique_ptr<VideoEncoder> CreateVideoEncoder(
      const SdpVideoFormat& format) override;
  CodecInfo QueryVideoEncoder(const SdpVideoFormat& format) const override;

 private:
  id<RTCVideoEncoderFactory> encoder_factory_;
};

}  // namespace webrtc

#endif  // SDK_OBJC_NATIVE_SRC_OBJC_VIDEO_ENCODER_FACTORY_H_
