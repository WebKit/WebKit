/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/webrtcvideoencoderfactory.h"

#include "test/gtest.h"

class WebRtcVideoEncoderFactoryForTest
    : public cricket::WebRtcVideoEncoderFactory {
 public:
  WebRtcVideoEncoderFactoryForTest() {
    codecs_.push_back(cricket::VideoCodec("H264"));
    codecs_.push_back(cricket::VideoCodec("VP8"));
  }

  webrtc::VideoEncoder* CreateVideoEncoder(
      const cricket::VideoCodec& codec) override {
    return nullptr;
  }

  const std::vector<cricket::VideoCodec>& supported_codecs() const override {
    return codecs_;
  }

  void DestroyVideoEncoder(webrtc::VideoEncoder* encoder) override {}

  std::vector<cricket::VideoCodec> codecs_;
};

TEST(WebRtcVideoEncoderFactoryTest, TestMultipleCallsToSupportedCodecs) {
  WebRtcVideoEncoderFactoryForTest factory;
  EXPECT_EQ(2u, factory.supported_codecs().size());
  EXPECT_EQ("H264", factory.supported_codecs()[0].name);
  EXPECT_EQ("VP8", factory.supported_codecs()[1].name);

  // The codec list doesn't grow when called repeatedly.
  EXPECT_EQ(2u, factory.supported_codecs().size());
}
