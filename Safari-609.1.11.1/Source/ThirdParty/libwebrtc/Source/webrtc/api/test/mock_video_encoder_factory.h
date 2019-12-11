/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_MOCK_VIDEO_ENCODER_FACTORY_H_
#define API_TEST_MOCK_VIDEO_ENCODER_FACTORY_H_

#include <memory>
#include <vector>

#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "test/gmock.h"

namespace webrtc {

class MockVideoEncoderFactory : public webrtc::VideoEncoderFactory {
 public:
  MOCK_CONST_METHOD0(GetSupportedFormats,
                     std::vector<webrtc::SdpVideoFormat>());
  MOCK_CONST_METHOD1(QueryVideoEncoder,
                     CodecInfo(const webrtc::SdpVideoFormat&));

  // We need to proxy to a return type that is copyable.
  std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
      const webrtc::SdpVideoFormat& format) {
    return std::unique_ptr<webrtc::VideoEncoder>(
        CreateVideoEncoderProxy(format));
  }
  MOCK_METHOD1(CreateVideoEncoderProxy,
               webrtc::VideoEncoder*(const webrtc::SdpVideoFormat&));

  MOCK_METHOD0(Die, void());
  ~MockVideoEncoderFactory() { Die(); }
};

}  // namespace webrtc

#endif  // API_TEST_MOCK_VIDEO_ENCODER_FACTORY_H_
