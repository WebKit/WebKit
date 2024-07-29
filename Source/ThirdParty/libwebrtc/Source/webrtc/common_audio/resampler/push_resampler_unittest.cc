/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/resampler/include/push_resampler.h"

#include "rtc_base/checks.h"  // RTC_DCHECK_IS_ON
#include "test/gtest.h"
#include "test/testsupport/rtc_expect_death.h"

// Quality testing of PushResampler is done in audio/remix_resample_unittest.cc.

namespace webrtc {

TEST(PushResamplerTest, VerifiesInputParameters) {
  PushResampler<int16_t> resampler1(160, 160, 1);
  PushResampler<int16_t> resampler2(160, 160, 2);
  PushResampler<int16_t> resampler3(160, 160, 8);
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(PushResamplerDeathTest, VerifiesBadInputParameters1) {
  RTC_EXPECT_DEATH(PushResampler<int16_t>(-1, 160, 1),
                   "src_samples_per_channel");
}

TEST(PushResamplerDeathTest, VerifiesBadInputParameters2) {
  RTC_EXPECT_DEATH(PushResampler<int16_t>(160, -1, 1),
                   "dst_samples_per_channel");
}

TEST(PushResamplerDeathTest, VerifiesBadInputParameters3) {
  RTC_EXPECT_DEATH(PushResampler<int16_t>(160, 16000, 0), "num_channels");
}
#endif

}  // namespace webrtc
