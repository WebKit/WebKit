/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_MOCK_VIDEO_DECODER_FACTORY_H_
#define API_TEST_MOCK_VIDEO_DECODER_FACTORY_H_

#include <memory>
#include <vector>

#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "test/gmock.h"

namespace webrtc {

class MockVideoDecoderFactory : public webrtc::VideoDecoderFactory {
 public:
  MOCK_CONST_METHOD0(GetSupportedFormats,
                     std::vector<webrtc::SdpVideoFormat>());

  // We need to proxy to a return type that is copyable.
  std::unique_ptr<webrtc::VideoDecoder> CreateVideoDecoder(
      const webrtc::SdpVideoFormat& format) {
    return std::unique_ptr<webrtc::VideoDecoder>(
        CreateVideoDecoderProxy(format));
  }
  MOCK_METHOD1(CreateVideoDecoderProxy,
               webrtc::VideoDecoder*(const webrtc::SdpVideoFormat&));

  MOCK_METHOD0(Die, void());
  ~MockVideoDecoderFactory() { Die(); }
};
}  // namespace webrtc

#endif  // API_TEST_MOCK_VIDEO_DECODER_FACTORY_H_
