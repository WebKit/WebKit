/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vpx/vp8cx.h"
#include "vpx/vpx_encoder.h"
#include "webrtc/modules/video_coding/codecs/vp8/reference_picture_selection.h"
#include "webrtc/test/gtest.h"

using webrtc::ReferencePictureSelection;

// The minimum time between reference frame updates. Should match the values
// set in reference_picture_selection.h
static const uint32_t kMinUpdateInterval = 10;
// The minimum time between decoder refreshes through restricted prediction.
// Should match the values set in reference_picture_selection.h
static const int kRtt = 10;

static const int kNoPropagationGolden =
    VP8_EFLAG_NO_REF_ARF | VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF;
static const int kNoPropagationAltRef =
    VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_UPD_GF | VP8_EFLAG_NO_UPD_ARF;
static const int kPropagateGolden = VP8_EFLAG_FORCE_GF | VP8_EFLAG_NO_UPD_ARF |
                                    VP8_EFLAG_NO_REF_GF | VP8_EFLAG_NO_REF_LAST;
static const int kPropagateAltRef = VP8_EFLAG_FORCE_ARF | VP8_EFLAG_NO_UPD_GF |
                                    VP8_EFLAG_NO_REF_ARF |
                                    VP8_EFLAG_NO_REF_LAST;
static const int kRefreshFromGolden =
    VP8_EFLAG_NO_REF_LAST | VP8_EFLAG_NO_REF_ARF;
static const int kRefreshFromAltRef =
    VP8_EFLAG_NO_REF_LAST | VP8_EFLAG_NO_REF_GF;

class TestRPS : public ::testing::Test {
 protected:
  virtual void SetUp() {
    rps_.Init();
    // Initialize with sending a key frame and acknowledging it.
    rps_.EncodedKeyFrame(0);
    rps_.ReceivedRPSI(0);
    rps_.SetRtt(kRtt);
  }

  ReferencePictureSelection rps_;
};

TEST_F(TestRPS, TestPropagateReferenceFrames) {
  // Should propagate the alt-ref reference.
  uint32_t time = (4 * kMinUpdateInterval) / 3 + 1;
  EXPECT_EQ(rps_.EncodeFlags(1, false, 90 * time), kPropagateAltRef);
  rps_.ReceivedRPSI(1);
  time += (4 * (time + kMinUpdateInterval)) / 3 + 1;
  // Should propagate the golden reference.
  EXPECT_EQ(rps_.EncodeFlags(2, false, 90 * time), kPropagateGolden);
  rps_.ReceivedRPSI(2);
  // Should propagate the alt-ref reference.
  time = (4 * (time + kMinUpdateInterval)) / 3 + 1;
  EXPECT_EQ(rps_.EncodeFlags(3, false, 90 * time), kPropagateAltRef);
  rps_.ReceivedRPSI(3);
  // Shouldn't propagate any reference frames (except last), and the established
  // reference is alt-ref.
  time = time + kMinUpdateInterval;
  EXPECT_EQ(rps_.EncodeFlags(4, false, 90 * time), kNoPropagationAltRef);
}

TEST_F(TestRPS, TestDecoderRefresh) {
  uint32_t time = kRtt + 1;
  // No more than one refresh per RTT.
  EXPECT_EQ(rps_.ReceivedSLI(90 * time), true);
  time += 5;
  EXPECT_EQ(rps_.ReceivedSLI(90 * time), false);
  time += kRtt - 4;
  EXPECT_EQ(rps_.ReceivedSLI(90 * time), true);
  // Enough time have elapsed since the previous reference propagation, we will
  // therefore get both a refresh from golden and a propagation of alt-ref.
  EXPECT_EQ(rps_.EncodeFlags(5, true, 90 * time),
            kRefreshFromGolden | kPropagateAltRef);
  rps_.ReceivedRPSI(5);
  time += kRtt + 1;
  // Enough time for a new refresh, but not enough time for a reference
  // propagation.
  EXPECT_EQ(rps_.ReceivedSLI(90 * time), true);
  EXPECT_EQ(rps_.EncodeFlags(6, true, 90 * time),
            kRefreshFromAltRef | kNoPropagationAltRef);
}

TEST_F(TestRPS, TestWrap) {
  EXPECT_EQ(rps_.ReceivedSLI(0xffffffff), true);
  EXPECT_EQ(rps_.ReceivedSLI(1), false);
  EXPECT_EQ(rps_.ReceivedSLI(90 * 100), true);

  EXPECT_EQ(rps_.EncodeFlags(7, false, 0xffffffff), kPropagateAltRef);
  EXPECT_EQ(rps_.EncodeFlags(8, false, 1), kNoPropagationGolden);
  EXPECT_EQ(rps_.EncodeFlags(10, false, 90 * 100), kPropagateAltRef);
}
