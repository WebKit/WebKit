/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains tests for `RtpTransceiver`.

#include "pc/rtp_transceiver.h"

#include <memory>

#include "absl/types/optional.h"
#include "api/rtp_parameters.h"
#include "media/base/fake_media_engine.h"
#include "pc/test/mock_channel_interface.h"
#include "pc/test/mock_rtp_receiver_internal.h"
#include "pc/test/mock_rtp_sender_internal.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Optional;
using ::testing::Property;
using ::testing::Return;
using ::testing::ReturnRef;

namespace webrtc {

// Checks that a channel cannot be set on a stopped `RtpTransceiver`.
TEST(RtpTransceiverTest, CannotSetChannelOnStoppedTransceiver) {
  auto cm = cricket::ChannelManager::Create(
      nullptr, true, rtc::Thread::Current(), rtc::Thread::Current());
  RtpTransceiver transceiver(cricket::MediaType::MEDIA_TYPE_AUDIO, cm.get());
  cricket::MockChannelInterface channel1;
  EXPECT_CALL(channel1, media_type())
      .WillRepeatedly(Return(cricket::MediaType::MEDIA_TYPE_AUDIO));
  EXPECT_CALL(channel1, SetFirstPacketReceivedCallback(_));

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

// Checks that a channel can be unset on a stopped `RtpTransceiver`
TEST(RtpTransceiverTest, CanUnsetChannelOnStoppedTransceiver) {
  auto cm = cricket::ChannelManager::Create(
      nullptr, true, rtc::Thread::Current(), rtc::Thread::Current());
  RtpTransceiver transceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, cm.get());
  cricket::MockChannelInterface channel;
  EXPECT_CALL(channel, media_type())
      .WillRepeatedly(Return(cricket::MediaType::MEDIA_TYPE_VIDEO));
  EXPECT_CALL(channel, SetFirstPacketReceivedCallback(_))
      .WillRepeatedly(testing::Return());

  transceiver.SetChannel(&channel);
  EXPECT_EQ(&channel, transceiver.channel());

  // Stop the transceiver.
  transceiver.StopInternal();
  EXPECT_EQ(&channel, transceiver.channel());

  // Set the channel to `nullptr`.
  transceiver.SetChannel(nullptr);
  EXPECT_EQ(nullptr, transceiver.channel());
}

class RtpTransceiverUnifiedPlanTest : public ::testing::Test {
 public:
  RtpTransceiverUnifiedPlanTest()
      : channel_manager_(cricket::ChannelManager::Create(
            std::make_unique<cricket::FakeMediaEngine>(),
            false,
            rtc::Thread::Current(),
            rtc::Thread::Current())),
        transceiver_(RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
                         rtc::Thread::Current(),
                         sender_),
                     RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
                         rtc::Thread::Current(),
                         rtc::Thread::Current(),
                         receiver_),
                     channel_manager_.get(),
                     channel_manager_->GetSupportedAudioRtpHeaderExtensions(),
                     /* on_negotiation_needed= */ [] {}) {}

  static rtc::scoped_refptr<MockRtpReceiverInternal> MockReceiver() {
    auto receiver = rtc::make_ref_counted<MockRtpReceiverInternal>();
    EXPECT_CALL(*receiver.get(), media_type())
        .WillRepeatedly(Return(cricket::MediaType::MEDIA_TYPE_AUDIO));
    return receiver;
  }

  static rtc::scoped_refptr<MockRtpSenderInternal> MockSender() {
    auto sender = rtc::make_ref_counted<MockRtpSenderInternal>();
    EXPECT_CALL(*sender.get(), media_type())
        .WillRepeatedly(Return(cricket::MediaType::MEDIA_TYPE_AUDIO));
    return sender;
  }

  rtc::scoped_refptr<MockRtpReceiverInternal> receiver_ = MockReceiver();
  rtc::scoped_refptr<MockRtpSenderInternal> sender_ = MockSender();
  std::unique_ptr<cricket::ChannelManager> channel_manager_;
  RtpTransceiver transceiver_;
};

// Basic tests for Stop()
TEST_F(RtpTransceiverUnifiedPlanTest, StopSetsDirection) {
  EXPECT_CALL(*receiver_.get(), StopAndEndTrack());
  EXPECT_CALL(*sender_.get(), SetTransceiverAsStopped());
  EXPECT_CALL(*sender_.get(), Stop());

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
      : channel_manager_(cricket::ChannelManager::Create(
            std::make_unique<cricket::FakeMediaEngine>(),
            false,
            rtc::Thread::Current(),
            rtc::Thread::Current())),
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
                         sender_),
                     RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
                         rtc::Thread::Current(),
                         rtc::Thread::Current(),
                         receiver_),
                     channel_manager_.get(),
                     extensions_,
                     /* on_negotiation_needed= */ [] {}) {}

  static rtc::scoped_refptr<MockRtpReceiverInternal> MockReceiver() {
    auto receiver = rtc::make_ref_counted<MockRtpReceiverInternal>();
    EXPECT_CALL(*receiver.get(), media_type())
        .WillRepeatedly(Return(cricket::MediaType::MEDIA_TYPE_AUDIO));
    return receiver;
  }

  static rtc::scoped_refptr<MockRtpSenderInternal> MockSender() {
    auto sender = rtc::make_ref_counted<MockRtpSenderInternal>();
    EXPECT_CALL(*sender.get(), media_type())
        .WillRepeatedly(Return(cricket::MediaType::MEDIA_TYPE_AUDIO));
    return sender;
  }

  rtc::scoped_refptr<MockRtpReceiverInternal> receiver_ = MockReceiver();
  rtc::scoped_refptr<MockRtpSenderInternal> sender_ = MockSender();

  std::unique_ptr<cricket::ChannelManager> channel_manager_;
  std::vector<RtpHeaderExtensionCapability> extensions_;
  RtpTransceiver transceiver_;
};

TEST_F(RtpTransceiverTestForHeaderExtensions, OffersChannelManagerList) {
  EXPECT_CALL(*receiver_.get(), StopAndEndTrack());
  EXPECT_CALL(*sender_.get(), SetTransceiverAsStopped());
  EXPECT_CALL(*sender_.get(), Stop());

  EXPECT_EQ(transceiver_.HeaderExtensionsToOffer(), extensions_);
}

TEST_F(RtpTransceiverTestForHeaderExtensions, ModifiesDirection) {
  EXPECT_CALL(*receiver_.get(), StopAndEndTrack());
  EXPECT_CALL(*sender_.get(), SetTransceiverAsStopped());
  EXPECT_CALL(*sender_.get(), Stop());

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
  EXPECT_CALL(*receiver_.get(), StopAndEndTrack());
  EXPECT_CALL(*sender_.get(), SetTransceiverAsStopped());
  EXPECT_CALL(*sender_.get(), Stop());

  auto modified_extensions = extensions_;
  modified_extensions[0].direction = RtpTransceiverDirection::kStopped;
  EXPECT_TRUE(
      transceiver_.SetOfferedRtpHeaderExtensions(modified_extensions).ok());
  EXPECT_EQ(transceiver_.HeaderExtensionsToOffer(), modified_extensions);
}

TEST_F(RtpTransceiverTestForHeaderExtensions, RejectsUnsupportedExtension) {
  EXPECT_CALL(*receiver_.get(), StopAndEndTrack());
  EXPECT_CALL(*sender_.get(), SetTransceiverAsStopped());
  EXPECT_CALL(*sender_.get(), Stop());

  std::vector<RtpHeaderExtensionCapability> modified_extensions(
      {RtpHeaderExtensionCapability("uri3", 1,
                                    RtpTransceiverDirection::kSendRecv)});
  EXPECT_THAT(transceiver_.SetOfferedRtpHeaderExtensions(modified_extensions),
              Property(&RTCError::type, RTCErrorType::UNSUPPORTED_PARAMETER));
  EXPECT_EQ(transceiver_.HeaderExtensionsToOffer(), extensions_);
}

TEST_F(RtpTransceiverTestForHeaderExtensions,
       RejectsStoppedMandatoryExtensions) {
  EXPECT_CALL(*receiver_.get(), StopAndEndTrack());
  EXPECT_CALL(*sender_.get(), SetTransceiverAsStopped());
  EXPECT_CALL(*sender_.get(), Stop());

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

TEST_F(RtpTransceiverTestForHeaderExtensions,
       NoNegotiatedHdrExtsWithoutChannel) {
  EXPECT_CALL(*receiver_.get(), StopAndEndTrack());
  EXPECT_CALL(*sender_.get(), SetTransceiverAsStopped());
  EXPECT_CALL(*sender_.get(), Stop());
  EXPECT_THAT(transceiver_.HeaderExtensionsNegotiated(), ElementsAre());
}

TEST_F(RtpTransceiverTestForHeaderExtensions,
       NoNegotiatedHdrExtsWithChannelWithoutNegotiation) {
  EXPECT_CALL(*receiver_.get(), SetMediaChannel(_));
  EXPECT_CALL(*receiver_.get(), StopAndEndTrack());
  EXPECT_CALL(*sender_.get(), SetMediaChannel(_));
  EXPECT_CALL(*sender_.get(), SetTransceiverAsStopped());
  EXPECT_CALL(*sender_.get(), Stop());
  cricket::MockChannelInterface mock_channel;
  EXPECT_CALL(mock_channel, SetFirstPacketReceivedCallback(_));
  EXPECT_CALL(mock_channel, media_type())
      .WillRepeatedly(Return(cricket::MediaType::MEDIA_TYPE_AUDIO));
  EXPECT_CALL(mock_channel, media_channel()).WillRepeatedly(Return(nullptr));
  transceiver_.SetChannel(&mock_channel);
  EXPECT_THAT(transceiver_.HeaderExtensionsNegotiated(), ElementsAre());
}

TEST_F(RtpTransceiverTestForHeaderExtensions, ReturnsNegotiatedHdrExts) {
  EXPECT_CALL(*receiver_.get(), SetMediaChannel(_));
  EXPECT_CALL(*receiver_.get(), StopAndEndTrack());
  EXPECT_CALL(*sender_.get(), SetMediaChannel(_));
  EXPECT_CALL(*sender_.get(), SetTransceiverAsStopped());
  EXPECT_CALL(*sender_.get(), Stop());

  cricket::MockChannelInterface mock_channel;
  EXPECT_CALL(mock_channel, SetFirstPacketReceivedCallback(_));
  EXPECT_CALL(mock_channel, media_type())
      .WillRepeatedly(Return(cricket::MediaType::MEDIA_TYPE_AUDIO));
  EXPECT_CALL(mock_channel, media_channel()).WillRepeatedly(Return(nullptr));

  cricket::RtpHeaderExtensions extensions = {webrtc::RtpExtension("uri1", 1),
                                             webrtc::RtpExtension("uri2", 2)};
  cricket::AudioContentDescription description;
  description.set_rtp_header_extensions(extensions);
  transceiver_.OnNegotiationUpdate(SdpType::kAnswer, &description);

  transceiver_.SetChannel(&mock_channel);
  EXPECT_THAT(transceiver_.HeaderExtensionsNegotiated(),
              ElementsAre(RtpHeaderExtensionCapability(
                              "uri1", 1, RtpTransceiverDirection::kSendRecv),
                          RtpHeaderExtensionCapability(
                              "uri2", 2, RtpTransceiverDirection::kSendRecv)));
}

TEST_F(RtpTransceiverTestForHeaderExtensions,
       ReturnsNegotiatedHdrExtsSecondTime) {
  EXPECT_CALL(*receiver_.get(), StopAndEndTrack());
  EXPECT_CALL(*sender_.get(), SetTransceiverAsStopped());
  EXPECT_CALL(*sender_.get(), Stop());

  cricket::RtpHeaderExtensions extensions = {webrtc::RtpExtension("uri1", 1),
                                             webrtc::RtpExtension("uri2", 2)};
  cricket::AudioContentDescription description;
  description.set_rtp_header_extensions(extensions);
  transceiver_.OnNegotiationUpdate(SdpType::kAnswer, &description);

  EXPECT_THAT(transceiver_.HeaderExtensionsNegotiated(),
              ElementsAre(RtpHeaderExtensionCapability(
                              "uri1", 1, RtpTransceiverDirection::kSendRecv),
                          RtpHeaderExtensionCapability(
                              "uri2", 2, RtpTransceiverDirection::kSendRecv)));

  extensions = {webrtc::RtpExtension("uri3", 4),
                webrtc::RtpExtension("uri5", 6)};
  description.set_rtp_header_extensions(extensions);
  transceiver_.OnNegotiationUpdate(SdpType::kAnswer, &description);

  EXPECT_THAT(transceiver_.HeaderExtensionsNegotiated(),
              ElementsAre(RtpHeaderExtensionCapability(
                              "uri3", 4, RtpTransceiverDirection::kSendRecv),
                          RtpHeaderExtensionCapability(
                              "uri5", 6, RtpTransceiverDirection::kSendRecv)));
}

}  // namespace webrtc
