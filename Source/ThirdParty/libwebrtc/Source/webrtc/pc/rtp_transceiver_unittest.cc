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

#include "pc/rtp_transceiver.h"

#include <memory>

#include "media/base/fake_media_engine.h"
#include "pc/test/mock_channel_interface.h"
#include "pc/test/mock_rtp_receiver_internal.h"
#include "pc/test/mock_rtp_sender_internal.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Not;
using ::testing::Property;
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
  transceiver.StopInternal();
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
  transceiver.StopInternal();
  EXPECT_EQ(&channel, transceiver.channel());

  // Set the channel to |nullptr|.
  transceiver.SetChannel(nullptr);
  EXPECT_EQ(nullptr, transceiver.channel());
}

class RtpTransceiverUnifiedPlanTest : public ::testing::Test {
 public:
  RtpTransceiverUnifiedPlanTest()
      : channel_manager_(std::make_unique<cricket::FakeMediaEngine>(),
                         std::make_unique<cricket::FakeDataEngine>(),
                         rtc::Thread::Current(),
                         rtc::Thread::Current()),
        transceiver_(RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
                         rtc::Thread::Current(),
                         new rtc::RefCountedObject<MockRtpSenderInternal>()),
                     RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
                         rtc::Thread::Current(),
                         new rtc::RefCountedObject<MockRtpReceiverInternal>()),
                     &channel_manager_,
                     channel_manager_.GetSupportedAudioRtpHeaderExtensions()) {}

  cricket::ChannelManager channel_manager_;
  RtpTransceiver transceiver_;
};

// Basic tests for Stop()
TEST_F(RtpTransceiverUnifiedPlanTest, StopSetsDirection) {
  EXPECT_EQ(RtpTransceiverDirection::kInactive, transceiver_.direction());
  EXPECT_FALSE(transceiver_.current_direction());
  transceiver_.StopStandard();
  EXPECT_EQ(RtpTransceiverDirection::kStopped, transceiver_.direction());
  EXPECT_FALSE(transceiver_.current_direction());
  transceiver_.StopTransceiverProcedure();
  EXPECT_TRUE(transceiver_.current_direction());
  EXPECT_EQ(RtpTransceiverDirection::kStopped, transceiver_.direction());
  EXPECT_EQ(RtpTransceiverDirection::kStopped,
            *transceiver_.current_direction());
}

class RtpTransceiverTestForHeaderExtensions : public ::testing::Test {
 public:
  RtpTransceiverTestForHeaderExtensions()
      : channel_manager_(std::make_unique<cricket::FakeMediaEngine>(),
                         std::make_unique<cricket::FakeDataEngine>(),
                         rtc::Thread::Current(),
                         rtc::Thread::Current()),
        extensions_(
            {RtpHeaderExtensionCapability("uri1",
                                          1,
                                          RtpTransceiverDirection::kSendOnly),
             RtpHeaderExtensionCapability("uri2",
                                          2,
                                          RtpTransceiverDirection::kRecvOnly),
             RtpHeaderExtensionCapability(RtpExtension::kMidUri,
                                          3,
                                          RtpTransceiverDirection::kSendRecv),
             RtpHeaderExtensionCapability(RtpExtension::kVideoRotationUri,
                                          4,
                                          RtpTransceiverDirection::kSendRecv)}),
        transceiver_(RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
                         rtc::Thread::Current(),
                         new rtc::RefCountedObject<MockRtpSenderInternal>()),
                     RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
                         rtc::Thread::Current(),
                         new rtc::RefCountedObject<MockRtpReceiverInternal>()),
                     &channel_manager_,
                     extensions_) {}

  cricket::ChannelManager channel_manager_;
  std::vector<RtpHeaderExtensionCapability> extensions_;
  RtpTransceiver transceiver_;
};

TEST_F(RtpTransceiverTestForHeaderExtensions, OffersChannelManagerList) {
  EXPECT_EQ(transceiver_.HeaderExtensionsToOffer(), extensions_);
}

TEST_F(RtpTransceiverTestForHeaderExtensions, ModifiesDirection) {
  auto modified_extensions = extensions_;
  modified_extensions[0].direction = RtpTransceiverDirection::kSendOnly;
  EXPECT_TRUE(
      transceiver_.SetOfferedRtpHeaderExtensions(modified_extensions).ok());
  EXPECT_EQ(transceiver_.HeaderExtensionsToOffer(), modified_extensions);
  modified_extensions[0].direction = RtpTransceiverDirection::kRecvOnly;
  EXPECT_TRUE(
      transceiver_.SetOfferedRtpHeaderExtensions(modified_extensions).ok());
  EXPECT_EQ(transceiver_.HeaderExtensionsToOffer(), modified_extensions);
  modified_extensions[0].direction = RtpTransceiverDirection::kSendRecv;
  EXPECT_TRUE(
      transceiver_.SetOfferedRtpHeaderExtensions(modified_extensions).ok());
  EXPECT_EQ(transceiver_.HeaderExtensionsToOffer(), modified_extensions);
  modified_extensions[0].direction = RtpTransceiverDirection::kInactive;
  EXPECT_TRUE(
      transceiver_.SetOfferedRtpHeaderExtensions(modified_extensions).ok());
  EXPECT_EQ(transceiver_.HeaderExtensionsToOffer(), modified_extensions);
}

TEST_F(RtpTransceiverTestForHeaderExtensions, AcceptsStoppedExtension) {
  auto modified_extensions = extensions_;
  modified_extensions[0].direction = RtpTransceiverDirection::kStopped;
  EXPECT_TRUE(
      transceiver_.SetOfferedRtpHeaderExtensions(modified_extensions).ok());
  EXPECT_EQ(transceiver_.HeaderExtensionsToOffer(), modified_extensions);
}

TEST_F(RtpTransceiverTestForHeaderExtensions, RejectsUnsupportedExtension) {
  std::vector<RtpHeaderExtensionCapability> modified_extensions(
      {RtpHeaderExtensionCapability("uri3", 1,
                                    RtpTransceiverDirection::kSendRecv)});
  EXPECT_THAT(transceiver_.SetOfferedRtpHeaderExtensions(modified_extensions),
              Property(&RTCError::type, RTCErrorType::INVALID_PARAMETER));
  EXPECT_EQ(transceiver_.HeaderExtensionsToOffer(), extensions_);
}

TEST_F(RtpTransceiverTestForHeaderExtensions,
       RejectsStoppedMandatoryExtensions) {
  std::vector<RtpHeaderExtensionCapability> modified_extensions = extensions_;
  // Attempting to stop the mandatory MID extension.
  modified_extensions[2].direction = RtpTransceiverDirection::kStopped;
  EXPECT_THAT(transceiver_.SetOfferedRtpHeaderExtensions(modified_extensions),
              Property(&RTCError::type, RTCErrorType::INVALID_MODIFICATION));
  EXPECT_EQ(transceiver_.HeaderExtensionsToOffer(), extensions_);
  modified_extensions = extensions_;
  // Attempting to stop the mandatory video orientation extension.
  modified_extensions[3].direction = RtpTransceiverDirection::kStopped;
  EXPECT_THAT(transceiver_.SetOfferedRtpHeaderExtensions(modified_extensions),
              Property(&RTCError::type, RTCErrorType::INVALID_MODIFICATION));
  EXPECT_EQ(transceiver_.HeaderExtensionsToOffer(), extensions_);
}

}  // namespace webrtc
