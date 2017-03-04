/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_ENGINE_INTERNALDECODERFACTORY_H_
#define WEBRTC_MEDIA_ENGINE_INTERNALDECODERFACTORY_H_

#include <vector>

#include "webrtc/media/engine/webrtcvideodecoderfactory.h"

namespace cricket {

class InternalDecoderFactory : public WebRtcVideoDecoderFactory {
 public:
  InternalDecoderFactory();
  virtual ~InternalDecoderFactory();

  // WebRtcVideoDecoderFactory implementation.
  webrtc::VideoDecoder* CreateVideoDecoder(
      webrtc::VideoCodecType type) override;

  void DestroyVideoDecoder(webrtc::VideoDecoder* decoder) override;
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_ENGINE_INTERNALDECODERFACTORY_H_
