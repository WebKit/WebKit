/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/channel_buffer.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

const size_t kNumFrames = 480u;
const size_t kStereo = 2u;
const size_t kMono = 1u;

void ExpectNumChannels(const IFChannelBuffer& ifchb, size_t num_channels) {
  EXPECT_EQ(ifchb.ibuf_const()->num_channels(), num_channels);
  EXPECT_EQ(ifchb.fbuf_const()->num_channels(), num_channels);
  EXPECT_EQ(ifchb.num_channels(), num_channels);
}

}  // namespace

TEST(ChannelBufferTest, SetNumChannelsSetsNumChannels) {
  ChannelBuffer<float> chb(kNumFrames, kStereo);
  EXPECT_EQ(chb.num_channels(), kStereo);
  chb.set_num_channels(kMono);
  EXPECT_EQ(chb.num_channels(), kMono);
}

TEST(IFChannelBufferTest, SetNumChannelsSetsChannelBuffersNumChannels) {
  IFChannelBuffer ifchb(kNumFrames, kStereo);
  ExpectNumChannels(ifchb, kStereo);
  ifchb.set_num_channels(kMono);
  ExpectNumChannels(ifchb, kMono);
}

TEST(IFChannelBufferTest, SettingNumChannelsOfOneChannelBufferSetsTheOther) {
  IFChannelBuffer ifchb(kNumFrames, kStereo);
  ExpectNumChannels(ifchb, kStereo);
  ifchb.ibuf()->set_num_channels(kMono);
  ExpectNumChannels(ifchb, kMono);
  ifchb.fbuf()->set_num_channels(kStereo);
  ExpectNumChannels(ifchb, kStereo);
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(ChannelBufferTest, SetNumChannelsDeathTest) {
  ChannelBuffer<float> chb(kNumFrames, kMono);
  EXPECT_DEATH(chb.set_num_channels(kStereo), "num_channels");
}

TEST(IFChannelBufferTest, SetNumChannelsDeathTest) {
  IFChannelBuffer ifchb(kNumFrames, kMono);
  EXPECT_DEATH(ifchb.ibuf()->set_num_channels(kStereo), "num_channels");
}
#endif

}  // namespace webrtc
