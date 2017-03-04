/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_ENGINE_INTERNALENCODERFACTORY_H_
#define WEBRTC_MEDIA_ENGINE_INTERNALENCODERFACTORY_H_

#include <vector>

#include "webrtc/media/engine/webrtcvideoencoderfactory.h"

namespace cricket {

class InternalEncoderFactory : public WebRtcVideoEncoderFactory {
 public:
  InternalEncoderFactory();
  virtual ~InternalEncoderFactory();

  // WebRtcVideoEncoderFactory implementation.
  webrtc::VideoEncoder* CreateVideoEncoder(
      const cricket::VideoCodec& codec) override;
  const std::vector<cricket::VideoCodec>& supported_codecs() const override;
  void DestroyVideoEncoder(webrtc::VideoEncoder* encoder) override;

 private:
  // Disable overloaded virtual function warning. TODO(magjed): Remove once
  // http://crbug/webrtc/6402 is fixed.
  using WebRtcVideoEncoderFactory::CreateVideoEncoder;

  std::vector<cricket::VideoCodec> supported_codecs_;
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_ENGINE_INTERNALENCODERFACTORY_H_
