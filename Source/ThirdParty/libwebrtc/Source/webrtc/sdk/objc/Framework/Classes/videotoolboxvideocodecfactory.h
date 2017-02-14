/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */
#ifndef WEBRTC_SDK_OBJC_FRAMEWORK_CLASSES_VIDEOTOOLBOXVIDEOCODECFACTORY_H_
#define WEBRTC_SDK_OBJC_FRAMEWORK_CLASSES_VIDEOTOOLBOXVIDEOCODECFACTORY_H_

#include "webrtc/base/export.h"
#include "webrtc/media/engine/webrtcvideoencoderfactory.h"
#include "webrtc/media/engine/webrtcvideodecoderfactory.h"

namespace webrtc {

class WEBRTC_EXPORT VideoToolboxVideoEncoderFactory
    : public cricket::WebRtcVideoEncoderFactory {
 public:
  VideoToolboxVideoEncoderFactory();
  ~VideoToolboxVideoEncoderFactory();

  // WebRtcVideoEncoderFactory implementation.
  VideoEncoder* CreateVideoEncoder(const cricket::VideoCodec& codec) override;
  void DestroyVideoEncoder(VideoEncoder* encoder) override;
  const std::vector<cricket::VideoCodec>& supported_codecs() const override;

 private:
  std::vector<cricket::VideoCodec> supported_codecs_;
};

class WEBRTC_EXPORT VideoToolboxVideoDecoderFactory
    : public cricket::WebRtcVideoDecoderFactory {
 public:
  VideoToolboxVideoDecoderFactory();
  ~VideoToolboxVideoDecoderFactory();

  // WebRtcVideoDecoderFactory implementation.
  VideoDecoder* CreateVideoDecoder(VideoCodecType type) override;
  void DestroyVideoDecoder(VideoDecoder* decoder) override;

 private:
  std::vector<cricket::VideoCodec> supported_codecs_;
};

}  // namespace webrtc

#endif  // WEBRTC_SDK_OBJC_FRAMEWORK_CLASSES_VIDEOTOOLBOXVIDEOCODECFACTORY_H_
