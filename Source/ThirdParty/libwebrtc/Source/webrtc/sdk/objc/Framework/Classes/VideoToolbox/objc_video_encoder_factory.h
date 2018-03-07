/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_OBJC_FRAMEWORK_CLASSES_PEERCONNECTION_OBJC_VIDEO_ENCODER_FACTORY_H_
#define SDK_OBJC_FRAMEWORK_CLASSES_PEERCONNECTION_OBJC_VIDEO_ENCODER_FACTORY_H_

#import <Foundation/Foundation.h>

#include "api/video_codecs/video_encoder_factory.h"
#include "media/engine/webrtcvideoencoderfactory.h"

@protocol RTCVideoEncoderFactory;

namespace webrtc {

// TODO(andersc): Remove the inheritance from cricket::WebRtcVideoEncoderFactory
// when the legacy path in [RTCPeerConnectionFactory init] is no longer needed.
class ObjCVideoEncoderFactory : public VideoEncoderFactory,
                                public cricket::WebRtcVideoEncoderFactory {
 public:
  explicit ObjCVideoEncoderFactory(id<RTCVideoEncoderFactory>);
  ~ObjCVideoEncoderFactory();

  id<RTCVideoEncoderFactory> wrapped_encoder_factory() const;

  std::vector<SdpVideoFormat> GetSupportedFormats() const override;
  std::unique_ptr<VideoEncoder> CreateVideoEncoder(
      const SdpVideoFormat& format) override;
  CodecInfo QueryVideoEncoder(const SdpVideoFormat& format) const override;

  // Needed for WebRtcVideoEncoderFactory interface.
  webrtc::VideoEncoder* CreateVideoEncoder(
      const cricket::VideoCodec& codec) override;
  const std::vector<cricket::VideoCodec>& supported_codecs() const override;
  void DestroyVideoEncoder(webrtc::VideoEncoder* encoder) override;

 private:
  id<RTCVideoEncoderFactory> encoder_factory_;

  // Needed for WebRtcVideoEncoderFactory interface.
  mutable std::vector<cricket::VideoCodec> supported_codecs_;
};

}  // namespace webrtc

#endif  // SDK_OBJC_FRAMEWORK_CLASSES_PEERCONNECTION_OBJC_VIDEO_ENCODER_FACTORY_H_
