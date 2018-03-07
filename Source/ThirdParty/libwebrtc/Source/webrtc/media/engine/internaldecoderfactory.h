/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_ENGINE_INTERNALDECODERFACTORY_H_
#define MEDIA_ENGINE_INTERNALDECODERFACTORY_H_

#include <memory>
#include <vector>

#include "api/video_codecs/video_decoder_factory.h"

namespace webrtc {

class InternalDecoderFactory : public VideoDecoderFactory {
 public:
  std::vector<SdpVideoFormat> GetSupportedFormats() const override;
  std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      const SdpVideoFormat& format) override;
};

}  // namespace webrtc

#endif  // MEDIA_ENGINE_INTERNALDECODERFACTORY_H_
