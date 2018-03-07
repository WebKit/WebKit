/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/vp8/simulcast_test_utility.h"

namespace webrtc {
namespace testing {

class TestVp8Impl : public TestVp8Simulcast {
 protected:
  std::unique_ptr<VP8Encoder> CreateEncoder() override {
    return VP8Encoder::Create();
  }
  std::unique_ptr<VP8Decoder> CreateDecoder() override {
    return VP8Decoder::Create();
  }
};

TEST_F(TestVp8Impl, TestKeyFrameRequestsOnAllStreams) {
  TestVp8Simulcast::TestKeyFrameRequestsOnAllStreams();
}

TEST_F(TestVp8Impl, TestPaddingAllStreams) {
  TestVp8Simulcast::TestPaddingAllStreams();
}

TEST_F(TestVp8Impl, TestPaddingTwoStreams) {
  TestVp8Simulcast::TestPaddingTwoStreams();
}

TEST_F(TestVp8Impl, TestPaddingTwoStreamsOneMaxedOut) {
  TestVp8Simulcast::TestPaddingTwoStreamsOneMaxedOut();
}

TEST_F(TestVp8Impl, TestPaddingOneStream) {
  TestVp8Simulcast::TestPaddingOneStream();
}

TEST_F(TestVp8Impl, TestPaddingOneStreamTwoMaxedOut) {
  TestVp8Simulcast::TestPaddingOneStreamTwoMaxedOut();
}

TEST_F(TestVp8Impl, TestSendAllStreams) {
  TestVp8Simulcast::TestSendAllStreams();
}

TEST_F(TestVp8Impl, TestDisablingStreams) {
  TestVp8Simulcast::TestDisablingStreams();
}

TEST_F(TestVp8Impl, TestSwitchingToOneStream) {
  TestVp8Simulcast::TestSwitchingToOneStream();
}

TEST_F(TestVp8Impl, TestSwitchingToOneOddStream) {
  TestVp8Simulcast::TestSwitchingToOneOddStream();
}

TEST_F(TestVp8Impl, TestSwitchingToOneSmallStream) {
  TestVp8Simulcast::TestSwitchingToOneSmallStream();
}

TEST_F(TestVp8Impl, TestSaptioTemporalLayers333PatternEncoder) {
  TestVp8Simulcast::TestSaptioTemporalLayers333PatternEncoder();
}

TEST_F(TestVp8Impl, TestStrideEncodeDecode) {
  TestVp8Simulcast::TestStrideEncodeDecode();
}
}  // namespace testing
}  // namespace webrtc
