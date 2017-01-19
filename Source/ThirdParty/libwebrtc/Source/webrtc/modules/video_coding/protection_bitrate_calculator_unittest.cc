/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/protection_bitrate_calculator.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

static const int kCodecBitrateBps = 100000;

class ProtectionBitrateCalculatorTest : public ::testing::Test {
 protected:
  enum {
    kSampleRate = 90000  // RTP timestamps per second.
  };

  class ProtectionCallback : public VCMProtectionCallback {
   public:
    int ProtectionRequest(const FecProtectionParams* delta_params,
                          const FecProtectionParams* key_params,
                          uint32_t* sent_video_rate_bps,
                          uint32_t* sent_nack_rate_bps,
                          uint32_t* sent_fec_rate_bps) override {
      *sent_video_rate_bps = kCodecBitrateBps;
      *sent_nack_rate_bps = nack_rate_bps_;
      *sent_fec_rate_bps = fec_rate_bps_;
      return 0;
    }

    uint32_t fec_rate_bps_ = 0;
    uint32_t nack_rate_bps_ = 0;
  };

  // Note: simulated clock starts at 1 seconds, since parts of webrtc use 0 as
  // a special case (e.g. frame rate in media optimization).
  ProtectionBitrateCalculatorTest()
      : clock_(1000), media_opt_(&clock_, &protection_callback_) {}

  SimulatedClock clock_;
  ProtectionCallback protection_callback_;
  ProtectionBitrateCalculator media_opt_;
};

TEST_F(ProtectionBitrateCalculatorTest, ProtectsUsingFecBitrate) {
  static const uint32_t kMaxBitrateBps = 130000;

  media_opt_.SetProtectionMethod(true /*enable_fec*/, false /* enable_nack */);
  media_opt_.SetEncodingData(640, 480, 1, 1000);

  // Using 10% of codec bitrate for FEC.
  protection_callback_.fec_rate_bps_ = kCodecBitrateBps / 10;
  uint32_t target_bitrate = media_opt_.SetTargetRates(kMaxBitrateBps, 30, 0, 0);

  EXPECT_GT(target_bitrate, 0u);
  EXPECT_GT(kMaxBitrateBps, target_bitrate);

  // Using as much for codec bitrate as fec rate, new target rate should share
  // both equally, but only be half of max (since that ceiling should be hit).
  protection_callback_.fec_rate_bps_ = kCodecBitrateBps;
  target_bitrate = media_opt_.SetTargetRates(kMaxBitrateBps, 30, 128, 100);
  EXPECT_EQ(kMaxBitrateBps / 2, target_bitrate);
}

TEST_F(ProtectionBitrateCalculatorTest, ProtectsUsingNackBitrate) {
  static const uint32_t kMaxBitrateBps = 130000;

  media_opt_.SetProtectionMethod(false /*enable_fec*/, true /* enable_nack */);
  media_opt_.SetEncodingData(640, 480, 1, 1000);

  uint32_t target_bitrate = media_opt_.SetTargetRates(kMaxBitrateBps, 30, 0, 0);

  EXPECT_EQ(kMaxBitrateBps, target_bitrate);

  // Using as much for codec bitrate as nack rate, new target rate should share
  // both equally, but only be half of max (since that ceiling should be hit).
  protection_callback_.nack_rate_bps_ = kMaxBitrateBps;
  target_bitrate = media_opt_.SetTargetRates(kMaxBitrateBps, 30, 128, 100);
  EXPECT_EQ(kMaxBitrateBps / 2, target_bitrate);
}

TEST_F(ProtectionBitrateCalculatorTest, NoProtection) {
  static const uint32_t kMaxBitrateBps = 130000;

  media_opt_.SetProtectionMethod(false /*enable_fec*/, false /* enable_nack */);
  media_opt_.SetEncodingData(640, 480, 1, 1000);

  uint32_t target_bitrate =
      media_opt_.SetTargetRates(kMaxBitrateBps, 30, 128, 100);
  EXPECT_EQ(kMaxBitrateBps, target_bitrate);
}

}  // namespace webrtc
