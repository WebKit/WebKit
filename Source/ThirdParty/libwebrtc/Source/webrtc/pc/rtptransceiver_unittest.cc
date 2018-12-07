/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains tests for |RtpTransceiver|.

#include "pc/rtptransceiver.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"
#include "test/mock_channelinterface.h"

using ::testing::Return;
using ::testing::ReturnRef;

namespace webrtc {

// Checks that a channel cannot be set on a stopped |RtpTransceiver|.
TEST(RtpTransceiverTest, CannotSetChannelOnStoppedTransceiver) {
  RtpTransceiver transceiver(cricket::MediaType::MEDIA_TYPE_AUDIO);
  cricket::MockChannelInterface channel1;
  sigslot::signal1<cricket::ChannelInterface*> signal;
  EXPECT_CALL(channel1, media_type())
      .WillRepeatedly(Return(cricket::MediaType::MEDIA_TYPE_AUDIO));
  EXPECT_CALL(channel1, SignalFirstPacketReceived())
      .WillRepeatedly(ReturnRef(signal));

  transceiver.SetChannel(&channel1);
  EXPECT_EQ(&channel1, transceiver.channel());

  // Stop the transceiver.
  transceiver.Stop();
  EXPECT_EQ(&channel1, transceiver.channel());

  cricket::MockChannelInterface channel2;
  EXPECT_CALL(channel2, media_type())
      .WillRepeatedly(Return(cricket::MediaType::MEDIA_TYPE_AUDIO));

  // Channel can no longer be set, so this call should be a no-op.
  transceiver.SetChannel(&channel2);
  EXPECT_EQ(&channel1, transceiver.channel());
}

// Checks that a channel can be unset on a stopped |RtpTransceiver|
TEST(RtpTransceiverTest, CanUnsetChannelOnStoppedTransceiver) {
  RtpTransceiver transceiver(cricket::MediaType::MEDIA_TYPE_VIDEO);
  cricket::MockChannelInterface channel;
  sigslot::signal1<cricket::ChannelInterface*> signal;
  EXPECT_CALL(channel, media_type())
      .WillRepeatedly(Return(cricket::MediaType::MEDIA_TYPE_VIDEO));
  EXPECT_CALL(channel, SignalFirstPacketReceived())
      .WillRepeatedly(ReturnRef(signal));

  transceiver.SetChannel(&channel);
  EXPECT_EQ(&channel, transceiver.channel());

  // Stop the transceiver.
  transceiver.Stop();
  EXPECT_EQ(&channel, transceiver.channel());

  // Set the channel to |nullptr|.
  transceiver.SetChannel(nullptr);
  EXPECT_EQ(nullptr, transceiver.channel());
}

}  // namespace webrtc
