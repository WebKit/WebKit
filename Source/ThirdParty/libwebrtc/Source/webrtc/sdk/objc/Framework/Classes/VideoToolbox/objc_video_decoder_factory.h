/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_OBJC_FRAMEWORK_CLASSES_PEERCONNECTION_OBJC_VIDEO_DECODER_FACTORY_H_
#define SDK_OBJC_FRAMEWORK_CLASSES_PEERCONNECTION_OBJC_VIDEO_DECODER_FACTORY_H_

#include "api/video_codecs/video_decoder_factory.h"
#include "media/base/codec.h"
#include "media/engine/webrtcvideodecoderfactory.h"

@protocol RTCVideoDecoderFactory;

namespace webrtc {

// TODO(andersc): Remove the inheritance from cricket::WebRtcVideoDecoderFactory
// when the legacy path in [RTCPeerConnectionFactory init] is no longer needed.
class ObjCVideoDecoderFactory : public VideoDecoderFactory,
                                public cricket::WebRtcVideoDecoderFactory {
 public:
  explicit ObjCVideoDecoderFactory(id<RTCVideoDecoderFactory>);
  ~ObjCVideoDecoderFactory();

  id<RTCVideoDecoderFactory> wrapped_decoder_factory() const;

  std::vector<SdpVideoFormat> GetSupportedFormats() const override;
  std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      const SdpVideoFormat& format) override;

  // Needed for WebRtcVideoDecoderFactory interface.
  webrtc::VideoDecoder* CreateVideoDecoderWithParams(
      const cricket::VideoCodec& codec,
      cricket::VideoDecoderParams params) override;
  webrtc::VideoDecoder* CreateVideoDecoder(
      webrtc::VideoCodecType type) override;
  void DestroyVideoDecoder(webrtc::VideoDecoder* decoder) override;

 private:
  id<RTCVideoDecoderFactory> decoder_factory_;
};

}  // namespace webrtc

#endif  // SDK_OBJC_FRAMEWORK_CLASSES_PEERCONNECTION_OBJC_VIDEO_DECODER_FACTORY_H_
