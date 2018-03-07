/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/audio_buffer.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

const size_t kNumFrames = 480u;
const size_t kStereo = 2u;
const size_t kMono = 1u;

void ExpectNumChannels(const AudioBuffer& ab, size_t num_channels) {
  EXPECT_EQ(ab.data()->num_channels(), num_channels);
  EXPECT_EQ(ab.data_f()->num_channels(), num_channels);
  EXPECT_EQ(ab.split_data()->num_channels(), num_channels);
  EXPECT_EQ(ab.split_data_f()->num_channels(), num_channels);
  EXPECT_EQ(ab.num_channels(), num_channels);
}

}  // namespace

TEST(AudioBufferTest, SetNumChannelsSetsChannelBuffersNumChannels) {
  AudioBuffer ab(kNumFrames, kStereo, kNumFrames, kStereo, kNumFrames);
  ExpectNumChannels(ab, kStereo);
  ab.set_num_channels(kMono);
  ExpectNumChannels(ab, kMono);
  ab.InitForNewData();
  ExpectNumChannels(ab, kStereo);
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(AudioBufferTest, SetNumChannelsDeathTest) {
  AudioBuffer ab(kNumFrames, kMono, kNumFrames, kMono, kNumFrames);
  EXPECT_DEATH(ab.set_num_channels(kStereo), "num_channels");
}
#endif

}  // namespace webrtc
