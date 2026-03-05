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
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/environment/environment.h"
#include "api/jsep.h"
#include "api/make_ref_counted.h"
#include "api/media_types.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/rtp_transceiver_direction.h"
#include "api/scoped_refptr.h"
#include "api/test/rtc_error_matchers.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/sdp_video_format.h"
#include "media/base/codec.h"
#include "media/base/codec_comparators.h"
#include "media/base/fake_media_engine.h"
#include "pc/codec_vendor.h"
#include "pc/connection_context.h"
#include "pc/rtp_parameters_conversion.h"
#include "pc/rtp_receiver.h"
#include "pc/rtp_receiver_proxy.h"
#include "pc/rtp_sender.h"
#include "pc/rtp_sender_proxy.h"
#include "pc/session_description.h"
#include "pc/test/enable_fake_media.h"
#include "pc/test/fake_codec_lookup_helper.h"
#include "pc/test/mock_channel_interface.h"
#include "pc/test/mock_rtp_receiver_internal.h"
#include "pc/test/mock_rtp_sender_internal.h"
#include "rtc_base/thread.h"
#include "test/create_test_environment.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Property;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SizeIs;

namespace webrtc {

namespace {

class RtpTransceiverTest : public testing::Test {
 public:
  RtpTransceiverTest()
      : env_(CreateTestEnvironment()),
        dependencies_(MakeDependencies()),
        context_(ConnectionContext::Create(env_, &dependencies_)),
        media_engine_ref_(context_),
        codec_lookup_helper_(context_.get(), env_.field_trials()) {}

 protected:
  const Environment& env() const { return env_; }
  FakeMediaEngine* media_engine() {
    // We know this cast is safe because we supplied the fake implementation
    // in MakeDependencies().
    return static_cast<FakeMediaEngine*>(media_engine_ref_.media_engine());
  }
  ConnectionContext* context() { return context_.get(); }
  CodecLookupHelper* codec_lookup_helper() { return &codec_lookup_helper_; }
  FakeCodecLookupHelper* fake_codec_lookup_helper() {
    return &codec_lookup_helper_;
  }

 private:
  AutoThread main_thread_;

  static PeerConnectionFactoryDependencies MakeDependencies() {
    PeerConnectionFactoryDependencies d;
    d.network_thread = Thread::Current();
    d.worker_thread = Thread::Current();
    d.signaling_thread = Thread::Current();
    EnableFakeMedia(d, std::make_unique<FakeMediaEngine>());
    return d;
  }

  Environment env_;
  PeerConnectionFactoryDependencies dependencies_;
  scoped_refptr<ConnectionContext> context_;
  ConnectionContext::MediaEngineReference media_engine_ref_;
  FakeCodecLookupHelper codec_lookup_helper_;
};

// Checks that a channel cannot be set on a stopped `RtpTransceiver`.
TEST_F(RtpTransceiverTest, CannotSetChannelOnStoppedTransceiver) {
  const std::string content_name("my_mid");
  auto transceiver = make_ref_counted<RtpTransceiver>(
      env(), MediaType::AUDIO, context(), codec_lookup_helper());
  auto channel1 = std::make_unique<NiceMock<MockChannelInterface>>();
  EXPECT_CALL(*channel1, media_type()).WillRepeatedly(Return(MediaType::AUDIO));
  EXPECT_CALL(*channel1, mid()).WillRepeatedly(ReturnRef(content_name));
  EXPECT_CALL(*channel1, SetFirstPacketReceivedCallback(_));
  EXPECT_CALL(*channel1, SetRtpTransport(_)).WillRepeatedly(Return(true));
  auto channel1_ptr = channel1.get();
  transceiver->SetChannel(std::move(channel1), [&](const std::string& mid) {
    EXPECT_EQ(mid, content_name);
    return nullptr;
  });
  EXPECT_EQ(channel1_ptr, transceiver->channel());

  // Stop the transceiver.
  transceiver->StopInternal();
  EXPECT_EQ(channel1_ptr, transceiver->channel());

  auto channel2 = std::make_unique<NiceMock<MockChannelInterface>>();
  EXPECT_CALL(*channel2, media_type()).WillRepeatedly(Return(MediaType::AUDIO));

  // Clear the current channel - required to allow SetChannel()
  EXPECT_CALL(*channel1_ptr, SetFirstPacketReceivedCallback(_));
  transceiver->ClearChannel();
  ASSERT_EQ(nullptr, transceiver->channel());
  // Channel can no longer be set, so this call should be a no-op.
  transceiver->SetChannel(std::move(channel2),
                          [](const std::string&) { return nullptr; });
  EXPECT_EQ(nullptr, transceiver->channel());
}

// Checks that a channel can be unset on a stopped `RtpTransceiver`
TEST_F(RtpTransceiverTest, CanUnsetChannelOnStoppedTransceiver) {
  const std::string content_name("my_mid");
  auto transceiver = make_ref_counted<RtpTransceiver>(
      env(), MediaType::VIDEO, context(), codec_lookup_helper());
  auto channel = std::make_unique<NiceMock<MockChannelInterface>>();
  EXPECT_CALL(*channel, media_type()).WillRepeatedly(Return(MediaType::VIDEO));
  EXPECT_CALL(*channel, mid()).WillRepeatedly(ReturnRef(content_name));
  EXPECT_CALL(*channel, SetFirstPacketReceivedCallback(_))
      .WillRepeatedly(testing::Return());
  EXPECT_CALL(*channel, SetRtpTransport(_)).WillRepeatedly(Return(true));

  auto channel_ptr = channel.get();
  transceiver->SetChannel(std::move(channel), [&](const std::string& mid) {
    EXPECT_EQ(mid, content_name);
    return nullptr;
  });
  EXPECT_EQ(channel_ptr, transceiver->channel());

  // Stop the transceiver.
  transceiver->StopInternal();
  EXPECT_EQ(channel_ptr, transceiver->channel());

  // Set the channel to `nullptr`.
  transceiver->ClearChannel();
  EXPECT_EQ(nullptr, transceiver->channel());
}

class RtpTransceiverUnifiedPlanTest : public RtpTransceiverTest {
 public:
  static scoped_refptr<MockRtpReceiverInternal> MockReceiver(
      MediaType media_type) {
    auto receiver = make_ref_counted<NiceMock<MockRtpReceiverInternal>>();
    EXPECT_CALL(*receiver.get(), media_type())
        .WillRepeatedly(Return(media_type));
    return receiver;
  }

  static scoped_refptr<MockRtpSenderInternal> MockSender(MediaType media_type) {
    auto sender = make_ref_counted<NiceMock<MockRtpSenderInternal>>();
    EXPECT_CALL(*sender.get(), media_type()).WillRepeatedly(Return(media_type));
    return sender;
  }

  scoped_refptr<RtpTransceiver> CreateTransceiver(
      scoped_refptr<RtpSenderInternal> sender,
      scoped_refptr<RtpReceiverInternal> receiver) {
    return make_ref_counted<RtpTransceiver>(
        env(),
        RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
            Thread::Current(), std::move(sender)),
        RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
            Thread::Current(), Thread::Current(), std::move(receiver)),
        context(), codec_lookup_helper(),
        media_engine()->voice().GetRtpHeaderExtensions(&env().field_trials()),
        /* on_negotiation_needed= */ [] {});
  }

 protected:
  AutoThread main_thread_;
};

// Basic tests for Stop()
TEST_F(RtpTransceiverUnifiedPlanTest, StopSetsDirection) {
  scoped_refptr<MockRtpReceiverInternal> receiver =
      MockReceiver(MediaType::AUDIO);
  scoped_refptr<MockRtpSenderInternal> sender = MockSender(MediaType::AUDIO);
  scoped_refptr<RtpTransceiver> transceiver =
      CreateTransceiver(sender, receiver);


  EXPECT_EQ(RtpTransceiverDirection::kInactive, transceiver->direction());
  EXPECT_FALSE(transceiver->current_direction());
  transceiver->StopStandard();
  EXPECT_EQ(RtpTransceiverDirection::kStopped, transceiver->direction());
  EXPECT_FALSE(transceiver->current_direction());
  transceiver->StopTransceiverProcedure();
  EXPECT_TRUE(transceiver->current_direction());
  EXPECT_EQ(RtpTransceiverDirection::kStopped, transceiver->direction());
  EXPECT_EQ(RtpTransceiverDirection::kStopped,
            *transceiver->current_direction());
}

class RtpTransceiverFilteredCodecPreferencesTest
    : public RtpTransceiverUnifiedPlanTest {
 public:
  RtpTransceiverFilteredCodecPreferencesTest()
      : transceiver_(CreateTransceiver(MockSender(MediaType::VIDEO),
                                       MockReceiver(MediaType::VIDEO))) {}

  struct H264CodecCapabilities {
    Codec cricket_sendrecv_codec;
    RtpCodecCapability sendrecv_codec;
    Codec cricket_sendonly_codec;
    RtpCodecCapability sendonly_codec;
    Codec cricket_recvonly_codec;
    RtpCodecCapability recvonly_codec;
    Codec cricket_rtx_codec;
    RtpCodecCapability rtx_codec;
  };

  // This function must be called after modifying the media factory's
  // capabilities, since the transceiver picks up codecs from the factory
  // at transceiver create time.
  void RecreateTransceiver() {
    fake_codec_lookup_helper()->Reset();
    transceiver_ = CreateTransceiver(MockSender(MediaType::VIDEO),
                                     MockReceiver(MediaType::VIDEO));
  }

  // For H264, the profile and level IDs are entangled. This function uses
  // profile-level-id values that are not equal even when levels are ignored.
  H264CodecCapabilities ConfigureH264CodecCapabilities() {
    Codec cricket_sendrecv_codec =
        CreateVideoCodec(SdpVideoFormat("H264",
                                        {{"level-asymmetry-allowed", "1"},
                                         {"packetization-mode", "1"},
                                         {"profile-level-id", "42f00b"}},
                                        {ScalabilityMode::kL1T1}));
    Codec cricket_sendonly_codec =
        CreateVideoCodec(SdpVideoFormat("H264",
                                        {{"level-asymmetry-allowed", "1"},
                                         {"packetization-mode", "1"},
                                         {"profile-level-id", "640034"}},
                                        {ScalabilityMode::kL1T1}));
    Codec cricket_recvonly_codec =
        CreateVideoCodec(SdpVideoFormat("H264",
                                        {{"level-asymmetry-allowed", "1"},
                                         {"packetization-mode", "1"},
                                         {"profile-level-id", "f4001f"}},
                                        {ScalabilityMode::kL1T1}));
    Codec cricket_rtx_codec =
        CreateVideoRtxCodec(Codec::kIdNotSet, Codec::kIdNotSet);
    media_engine()->SetVideoSendCodecs(
        {cricket_sendrecv_codec, cricket_sendonly_codec, cricket_rtx_codec});
    media_engine()->SetVideoRecvCodecs(
        {cricket_sendrecv_codec, cricket_recvonly_codec, cricket_rtx_codec});
    H264CodecCapabilities capabilities = {
        .cricket_sendrecv_codec = cricket_sendrecv_codec,
        .sendrecv_codec = ToRtpCodecCapability(cricket_sendrecv_codec),
        .cricket_sendonly_codec = cricket_sendonly_codec,
        .sendonly_codec = ToRtpCodecCapability(cricket_sendonly_codec),
        .cricket_recvonly_codec = cricket_recvonly_codec,
        .recvonly_codec = ToRtpCodecCapability(cricket_recvonly_codec),
        .cricket_rtx_codec = cricket_rtx_codec,
        .rtx_codec = ToRtpCodecCapability(cricket_rtx_codec),
    };
    EXPECT_FALSE(IsSameRtpCodecIgnoringLevel(
        capabilities.cricket_sendrecv_codec, capabilities.sendonly_codec));
    EXPECT_FALSE(IsSameRtpCodecIgnoringLevel(
        capabilities.cricket_sendrecv_codec, capabilities.recvonly_codec));
    EXPECT_FALSE(IsSameRtpCodecIgnoringLevel(
        capabilities.cricket_sendonly_codec, capabilities.recvonly_codec));
    // Because RtpTransceiver buffers codec information in a CodecVendor,
    // we must recreate it after changing the supported codecs.
    RecreateTransceiver();
    return capabilities;
  }

#ifdef RTC_ENABLE_H265
  struct H265CodecCapabilities {
    // The level-id from sender getCapabilities() or receiver getCapabilities().
    static constexpr const char* kSendOnlyLevel = "180";
    static constexpr const char* kRecvOnlyLevel = "156";
    // A valid H265 level-id, but one not present in either getCapabilities().
    static constexpr const char* kLevelNotInCapabilities = "135";

    Codec cricket_sendonly_codec;
    RtpCodecCapability sendonly_codec;
    Codec cricket_recvonly_codec;
    RtpCodecCapability recvonly_codec;
  };

  // For H265, the profile and level IDs are separate and are ignored by
  // IsSameRtpCodecIgnoringLevel().
  H265CodecCapabilities ConfigureH265CodecCapabilities() {
    Codec cricket_sendonly_codec = CreateVideoCodec(
        SdpVideoFormat("H265",
                       {{"profile-id", "1"},
                        {"tier-flag", "0"},
                        {"level-id", H265CodecCapabilities::kSendOnlyLevel},
                        {"tx-mode", "SRST"}},
                       {ScalabilityMode::kL1T1}));
    Codec cricket_recvonly_codec = CreateVideoCodec(
        SdpVideoFormat("H265",
                       {{"profile-id", "1"},
                        {"tier-flag", "0"},
                        {"level-id", H265CodecCapabilities::kRecvOnlyLevel},
                        {"tx-mode", "SRST"}},
                       {ScalabilityMode::kL1T1}));
    media_engine()->SetVideoSendCodecs({cricket_sendonly_codec});
    media_engine()->SetVideoRecvCodecs({cricket_recvonly_codec});
    // Because RtpTransceiver buffers codec information in a CodecVendor,
    // we must recreate it after changing the supported codecs.
    RecreateTransceiver();
    return {
        .cricket_sendonly_codec = cricket_sendonly_codec,
        .sendonly_codec = ToRtpCodecCapability(cricket_sendonly_codec),
        .cricket_recvonly_codec = cricket_recvonly_codec,
        .recvonly_codec = ToRtpCodecCapability(cricket_recvonly_codec),
    };
  }
#endif  // RTC_ENABLE_H265

 protected:
  scoped_refptr<RtpTransceiver> transceiver_;
};

TEST_F(RtpTransceiverFilteredCodecPreferencesTest, EmptyByDefault) {
  ConfigureH264CodecCapabilities();

  EXPECT_THAT(
      transceiver_->SetDirectionWithError(RtpTransceiverDirection::kSendRecv),
      IsRtcOk());
  EXPECT_THAT(transceiver_->filtered_codec_preferences(), SizeIs(0));

  EXPECT_THAT(
      transceiver_->SetDirectionWithError(RtpTransceiverDirection::kSendOnly),
      IsRtcOk());
  EXPECT_THAT(transceiver_->filtered_codec_preferences(), SizeIs(0));

  EXPECT_THAT(
      transceiver_->SetDirectionWithError(RtpTransceiverDirection::kRecvOnly),
      IsRtcOk());
  EXPECT_THAT(transceiver_->filtered_codec_preferences(), SizeIs(0));

  EXPECT_THAT(
      transceiver_->SetDirectionWithError(RtpTransceiverDirection::kInactive),
      IsRtcOk());
  EXPECT_THAT(transceiver_->filtered_codec_preferences(), SizeIs(0));
}

TEST_F(RtpTransceiverFilteredCodecPreferencesTest, OrderIsMaintained) {
  const auto codecs = ConfigureH264CodecCapabilities();
  std::vector<RtpCodecCapability> codec_capabilities = {codecs.sendrecv_codec,
                                                        codecs.rtx_codec};
  EXPECT_THAT(transceiver_->SetCodecPreferences(codec_capabilities), IsRtcOk());
  EXPECT_THAT(transceiver_->filtered_codec_preferences(),
              ElementsAre(codec_capabilities[0], codec_capabilities[1]));
  // Reverse order.
  codec_capabilities = {codecs.rtx_codec, codecs.sendrecv_codec};
  EXPECT_THAT(transceiver_->SetCodecPreferences(codec_capabilities), IsRtcOk());
  EXPECT_THAT(transceiver_->filtered_codec_preferences(),
              ElementsAre(codec_capabilities[0], codec_capabilities[1]));
}

TEST_F(RtpTransceiverFilteredCodecPreferencesTest,
       FiltersCodecsBasedOnDirection) {
  const auto codecs = ConfigureH264CodecCapabilities();
  std::vector<RtpCodecCapability> codec_capabilities = {
      codecs.sendonly_codec, codecs.sendrecv_codec, codecs.recvonly_codec};
  EXPECT_THAT(transceiver_->SetCodecPreferences(codec_capabilities), IsRtcOk());

  EXPECT_THAT(
      transceiver_->SetDirectionWithError(RtpTransceiverDirection::kSendRecv),
      IsRtcOk());
  EXPECT_THAT(transceiver_->filtered_codec_preferences(),
              ElementsAre(codecs.sendrecv_codec));

  EXPECT_THAT(
      transceiver_->SetDirectionWithError(RtpTransceiverDirection::kSendOnly),
      IsRtcOk());
  EXPECT_THAT(transceiver_->filtered_codec_preferences(),
              ElementsAre(codecs.sendonly_codec, codecs.sendrecv_codec));

  EXPECT_THAT(
      transceiver_->SetDirectionWithError(RtpTransceiverDirection::kRecvOnly),
      IsRtcOk());
  EXPECT_THAT(transceiver_->filtered_codec_preferences(),
              ElementsAre(codecs.sendrecv_codec, codecs.recvonly_codec));

  EXPECT_THAT(
      transceiver_->SetDirectionWithError(RtpTransceiverDirection::kInactive),
      IsRtcOk());
  EXPECT_THAT(transceiver_->filtered_codec_preferences(),
              ElementsAre(codecs.sendrecv_codec));
}

TEST_F(RtpTransceiverFilteredCodecPreferencesTest,
       RtxIsIncludedAfterFiltering) {
  const auto codecs = ConfigureH264CodecCapabilities();
  std::vector<RtpCodecCapability> codec_capabilities = {codecs.recvonly_codec,
                                                        codecs.rtx_codec};
  EXPECT_THAT(transceiver_->SetCodecPreferences(codec_capabilities), IsRtcOk());

  EXPECT_THAT(
      transceiver_->SetDirectionWithError(RtpTransceiverDirection::kRecvOnly),
      IsRtcOk());
  EXPECT_THAT(transceiver_->filtered_codec_preferences(),
              ElementsAre(codecs.recvonly_codec, codecs.rtx_codec));
}

TEST_F(RtpTransceiverFilteredCodecPreferencesTest,
       NoMediaIsTheSameAsNoPreference) {
  const auto codecs = ConfigureH264CodecCapabilities();
  std::vector<RtpCodecCapability> codec_capabilities = {codecs.recvonly_codec,
                                                        codecs.rtx_codec};
  EXPECT_THAT(transceiver_->SetCodecPreferences(codec_capabilities), IsRtcOk());

  EXPECT_THAT(
      transceiver_->SetDirectionWithError(RtpTransceiverDirection::kSendOnly),
      IsRtcOk());
  // After filtering the only codec that remains is RTX which is not a media
  // codec, this is the same as not having any preferences.
  EXPECT_THAT(transceiver_->filtered_codec_preferences(), SizeIs(0));

  // But the preferences are remembered in case the direction changes such that
  // we do have a media codec.
  EXPECT_THAT(
      transceiver_->SetDirectionWithError(RtpTransceiverDirection::kRecvOnly),
      IsRtcOk());
  EXPECT_THAT(transceiver_->filtered_codec_preferences(),
              ElementsAre(codecs.recvonly_codec, codecs.rtx_codec));
}

TEST_F(RtpTransceiverFilteredCodecPreferencesTest,
       H264LevelIdsIgnoredByFilter) {
  // Baseline 3.1 and 5.2 are compatible when ignoring level IDs.
  Codec baseline_3_1 =
      CreateVideoCodec(SdpVideoFormat("H264",
                                      {{"level-asymmetry-allowed", "1"},
                                       {"packetization-mode", "1"},
                                       {"profile-level-id", "42001f"}},
                                      {ScalabilityMode::kL1T1}));
  Codec baseline_5_2 =
      CreateVideoCodec(SdpVideoFormat("H264",
                                      {{"level-asymmetry-allowed", "1"},
                                       {"packetization-mode", "1"},
                                       {"profile-level-id", "420034"}},
                                      {ScalabilityMode::kL1T1}));
  // High is NOT compatible with baseline.
  Codec high_3_1 =
      CreateVideoCodec(SdpVideoFormat("H264",
                                      {{"level-asymmetry-allowed", "1"},
                                       {"packetization-mode", "1"},
                                       {"profile-level-id", "64001f"}},
                                      {ScalabilityMode::kL1T1}));
  // Configure being able to both send and receive Baseline but using different
  // level IDs in either direction, while the High profile is "truly" recvonly.
  media_engine()->SetVideoSendCodecs({baseline_3_1});
  media_engine()->SetVideoRecvCodecs({baseline_5_2, high_3_1});
  // Because RtpTransceiver buffers codec information in a CodecVendor,
  // we must recreate it after changing the supported codecs.
  RecreateTransceiver();

  // Prefer to "sendrecv" Baseline 5.2. Even though we can only send 3.1 this
  // codec is not filtered out due to 5.2 and 3.1 being compatible when ignoring
  // level IDs.
  std::vector<RtpCodecCapability> codec_capabilities = {
      ToRtpCodecCapability(baseline_5_2)};
  EXPECT_THAT(transceiver_->SetCodecPreferences(codec_capabilities), IsRtcOk());
  EXPECT_THAT(
      transceiver_->SetDirectionWithError(RtpTransceiverDirection::kSendRecv),
      IsRtcOk());
  EXPECT_THAT(transceiver_->filtered_codec_preferences(),
              ElementsAre(codec_capabilities[0]));
  // Prefer to "sendrecv" High 3.1. This gets filtered out because we cannot
  // send it (Baseline 3.1 is not compatible with it).
  codec_capabilities = {ToRtpCodecCapability(high_3_1)};
  EXPECT_THAT(transceiver_->SetCodecPreferences(codec_capabilities), IsRtcOk());
  EXPECT_THAT(transceiver_->filtered_codec_preferences(), SizeIs(0));
  // Change direction to "recvonly" to avoid High 3.1 being filtered out.
  EXPECT_THAT(
      transceiver_->SetDirectionWithError(RtpTransceiverDirection::kRecvOnly),
      IsRtcOk());
  EXPECT_THAT(transceiver_->filtered_codec_preferences(),
              ElementsAre(codec_capabilities[0]));
}

#ifdef RTC_ENABLE_H265
TEST_F(RtpTransceiverFilteredCodecPreferencesTest,
       H265LevelIdIsIgnoredByFilter) {
  const auto codecs = ConfigureH265CodecCapabilities();
  std::vector<RtpCodecCapability> codec_capabilities = {codecs.sendonly_codec,
                                                        codecs.recvonly_codec};
  EXPECT_THAT(transceiver_->SetCodecPreferences(codec_capabilities), IsRtcOk());
  // Regardless of direction, both codecs are preferred due to ignoring levels.
  EXPECT_THAT(
      transceiver_->SetDirectionWithError(RtpTransceiverDirection::kSendOnly),
      IsRtcOk());
  EXPECT_THAT(transceiver_->filtered_codec_preferences(),
              ElementsAre(codec_capabilities[0], codec_capabilities[1]));
  EXPECT_THAT(
      transceiver_->SetDirectionWithError(RtpTransceiverDirection::kRecvOnly),
      IsRtcOk());
  EXPECT_THAT(transceiver_->filtered_codec_preferences(),
              ElementsAre(codec_capabilities[0], codec_capabilities[1]));
  EXPECT_THAT(
      transceiver_->SetDirectionWithError(RtpTransceiverDirection::kSendRecv),
      IsRtcOk());
  EXPECT_THAT(transceiver_->filtered_codec_preferences(),
              ElementsAre(codec_capabilities[0], codec_capabilities[1]));
}

TEST_F(RtpTransceiverFilteredCodecPreferencesTest,
       H265LevelIdHasToBeFromSenderOrReceiverCapabilities) {
  ConfigureH265CodecCapabilities();
  Codec cricket_codec = CreateVideoCodec(SdpVideoFormat(
      "H265",
      {{"profile-id", "1"},
       {"tier-flag", "0"},
       {"level-id", H265CodecCapabilities::kLevelNotInCapabilities},
       {"tx-mode", "SRST"}},
      {ScalabilityMode::kL1T1}));

  std::vector<RtpCodecCapability> codec_capabilities = {
      ToRtpCodecCapability(cricket_codec)};
  EXPECT_THAT(transceiver_->SetCodecPreferences(codec_capabilities),
              IsRtcErrorWithTypeAndMessage(
                  RTCErrorType::INVALID_MODIFICATION,
                  "Invalid codec preferences: Missing codec from codec "
                  "capabilities."));
}
#endif  // RTC_ENABLE_H265

class RtpTransceiverTestForHeaderExtensions
    : public RtpTransceiverUnifiedPlanTest {
 public:
  RtpTransceiverTestForHeaderExtensions()
      : extensions_(
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
        transceiver_(make_ref_counted<RtpTransceiver>(
            env(),
            RtpSenderProxyWithInternal<RtpSenderInternal>::Create(
                Thread::Current(),
                sender_),
            RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
                Thread::Current(),
                Thread::Current(),
                receiver_),
            context(),
            codec_lookup_helper(),
            extensions_,
            /* on_negotiation_needed= */ [] {})) {}

  void ClearChannel() {
    EXPECT_CALL(*sender_.get(), SetMediaChannel(_));
    transceiver_->ClearChannel();
  }

 protected:
  scoped_refptr<MockRtpReceiverInternal> receiver_ =
      MockReceiver(MediaType::AUDIO);
  scoped_refptr<MockRtpSenderInternal> sender_ = MockSender(MediaType::AUDIO);

  std::vector<RtpHeaderExtensionCapability> extensions_;
  scoped_refptr<RtpTransceiver> transceiver_;
};

TEST_F(RtpTransceiverTestForHeaderExtensions, OffersChannelManagerList) {
  EXPECT_EQ(transceiver_->GetHeaderExtensionsToNegotiate(), extensions_);
}

TEST_F(RtpTransceiverTestForHeaderExtensions, ModifiesDirection) {
  auto modified_extensions = extensions_;
  modified_extensions[0].direction = RtpTransceiverDirection::kSendOnly;
  EXPECT_TRUE(
      transceiver_->SetHeaderExtensionsToNegotiate(modified_extensions).ok());
  EXPECT_EQ(transceiver_->GetHeaderExtensionsToNegotiate(),
            modified_extensions);
  modified_extensions[0].direction = RtpTransceiverDirection::kRecvOnly;
  EXPECT_TRUE(
      transceiver_->SetHeaderExtensionsToNegotiate(modified_extensions).ok());
  EXPECT_EQ(transceiver_->GetHeaderExtensionsToNegotiate(),
            modified_extensions);
  modified_extensions[0].direction = RtpTransceiverDirection::kSendRecv;
  EXPECT_TRUE(
      transceiver_->SetHeaderExtensionsToNegotiate(modified_extensions).ok());
  EXPECT_EQ(transceiver_->GetHeaderExtensionsToNegotiate(),
            modified_extensions);
  modified_extensions[0].direction = RtpTransceiverDirection::kInactive;
  EXPECT_TRUE(
      transceiver_->SetHeaderExtensionsToNegotiate(modified_extensions).ok());
  EXPECT_EQ(transceiver_->GetHeaderExtensionsToNegotiate(),
            modified_extensions);
}

TEST_F(RtpTransceiverTestForHeaderExtensions, AcceptsStoppedExtension) {
  auto modified_extensions = extensions_;
  modified_extensions[0].direction = RtpTransceiverDirection::kStopped;
  EXPECT_TRUE(
      transceiver_->SetHeaderExtensionsToNegotiate(modified_extensions).ok());
  EXPECT_EQ(transceiver_->GetHeaderExtensionsToNegotiate(),
            modified_extensions);
}

TEST_F(RtpTransceiverTestForHeaderExtensions, RejectsDifferentSize) {
  auto modified_extensions = extensions_;
  modified_extensions.pop_back();

  EXPECT_THAT(transceiver_->SetHeaderExtensionsToNegotiate(modified_extensions),
              Property(&RTCError::type, RTCErrorType::INVALID_MODIFICATION));
  EXPECT_EQ(transceiver_->GetHeaderExtensionsToNegotiate(), extensions_);
}

TEST_F(RtpTransceiverTestForHeaderExtensions, RejectsChangedUri) {
  auto modified_extensions = extensions_;
  ASSERT_TRUE(!modified_extensions.empty());
  modified_extensions[0].uri = "http://webrtc.org";

  EXPECT_THAT(transceiver_->SetHeaderExtensionsToNegotiate(modified_extensions),
              Property(&RTCError::type, RTCErrorType::INVALID_MODIFICATION));
  EXPECT_EQ(transceiver_->GetHeaderExtensionsToNegotiate(), extensions_);
}

TEST_F(RtpTransceiverTestForHeaderExtensions, RejectsReorder) {
  auto modified_extensions = extensions_;
  ASSERT_GE(modified_extensions.size(), 2u);
  std::swap(modified_extensions[0], modified_extensions[1]);

  EXPECT_THAT(transceiver_->SetHeaderExtensionsToNegotiate(modified_extensions),
              Property(&RTCError::type, RTCErrorType::INVALID_MODIFICATION));
  EXPECT_EQ(transceiver_->GetHeaderExtensionsToNegotiate(), extensions_);
}

TEST_F(RtpTransceiverTestForHeaderExtensions,
       RejectsStoppedMandatoryExtensions) {
  std::vector<RtpHeaderExtensionCapability> modified_extensions = extensions_;
  // Attempting to stop the mandatory MID extension.
  modified_extensions[2].direction = RtpTransceiverDirection::kStopped;
  EXPECT_THAT(transceiver_->SetHeaderExtensionsToNegotiate(modified_extensions),
              Property(&RTCError::type, RTCErrorType::INVALID_MODIFICATION));
  EXPECT_EQ(transceiver_->GetHeaderExtensionsToNegotiate(), extensions_);
}

TEST_F(RtpTransceiverTestForHeaderExtensions,
       NoNegotiatedHdrExtsWithoutChannel) {
  EXPECT_THAT(transceiver_->GetNegotiatedHeaderExtensions(),
              ElementsAre(Field(&RtpHeaderExtensionCapability::direction,
                                RtpTransceiverDirection::kStopped),
                          Field(&RtpHeaderExtensionCapability::direction,
                                RtpTransceiverDirection::kStopped),
                          Field(&RtpHeaderExtensionCapability::direction,
                                RtpTransceiverDirection::kStopped),
                          Field(&RtpHeaderExtensionCapability::direction,
                                RtpTransceiverDirection::kStopped)));
}

TEST_F(RtpTransceiverTestForHeaderExtensions,
       NoNegotiatedHdrExtsWithChannelWithoutNegotiation) {
  const std::string content_name("my_mid");
  auto mock_channel = std::make_unique<NiceMock<MockChannelInterface>>();
  EXPECT_CALL(*mock_channel, media_type())
      .WillRepeatedly(Return(MediaType::AUDIO));
  EXPECT_CALL(*mock_channel, mid()).WillRepeatedly(ReturnRef(content_name));
  EXPECT_CALL(*mock_channel, SetRtpTransport(_)).WillRepeatedly(Return(true));
  transceiver_->SetChannel(std::move(mock_channel),
                           [](const std::string&) { return nullptr; });
  EXPECT_THAT(transceiver_->GetNegotiatedHeaderExtensions(),
              ElementsAre(Field(&RtpHeaderExtensionCapability::direction,
                                RtpTransceiverDirection::kStopped),
                          Field(&RtpHeaderExtensionCapability::direction,
                                RtpTransceiverDirection::kStopped),
                          Field(&RtpHeaderExtensionCapability::direction,
                                RtpTransceiverDirection::kStopped),
                          Field(&RtpHeaderExtensionCapability::direction,
                                RtpTransceiverDirection::kStopped)));

  ClearChannel();
}

TEST_F(RtpTransceiverTestForHeaderExtensions, ReturnsNegotiatedHdrExts) {
  const std::string content_name("my_mid");

  auto mock_channel = std::make_unique<NiceMock<MockChannelInterface>>();
  EXPECT_CALL(*mock_channel, media_type())
      .WillRepeatedly(Return(MediaType::AUDIO));
  EXPECT_CALL(*mock_channel, voice_media_send_channel())
      .WillRepeatedly(Return(nullptr));
  EXPECT_CALL(*mock_channel, mid()).WillRepeatedly(ReturnRef(content_name));
  EXPECT_CALL(*mock_channel, SetRtpTransport(_)).WillRepeatedly(Return(true));

  RtpHeaderExtensions extensions = {RtpExtension("uri1", 1),
                                    RtpExtension("uri2", 2)};
  AudioContentDescription description;
  description.set_rtp_header_extensions(extensions);
  transceiver_->OnNegotiationUpdate(SdpType::kAnswer, &description);

  transceiver_->SetChannel(std::move(mock_channel),
                           [](const std::string&) { return nullptr; });

  EXPECT_THAT(transceiver_->GetNegotiatedHeaderExtensions(),
              ElementsAre(Field(&RtpHeaderExtensionCapability::direction,
                                RtpTransceiverDirection::kSendRecv),
                          Field(&RtpHeaderExtensionCapability::direction,
                                RtpTransceiverDirection::kSendRecv),
                          Field(&RtpHeaderExtensionCapability::direction,
                                RtpTransceiverDirection::kStopped),
                          Field(&RtpHeaderExtensionCapability::direction,
                                RtpTransceiverDirection::kStopped)));
  ClearChannel();
}

TEST_F(RtpTransceiverTestForHeaderExtensions,
       ReturnsNegotiatedHdrExtsOnPrAnswer) {
  const std::string content_name("my_mid");

  auto mock_channel = std::make_unique<NiceMock<MockChannelInterface>>();
  EXPECT_CALL(*mock_channel, media_type())
      .WillRepeatedly(Return(MediaType::AUDIO));
  EXPECT_CALL(*mock_channel, voice_media_send_channel())
      .WillRepeatedly(Return(nullptr));
  EXPECT_CALL(*mock_channel, mid()).WillRepeatedly(ReturnRef(content_name));
  EXPECT_CALL(*mock_channel, SetRtpTransport(_)).WillRepeatedly(Return(true));

  RtpHeaderExtensions extensions = {RtpExtension("uri1", 1),
                                    RtpExtension("uri2", 2)};
  AudioContentDescription description;
  description.set_rtp_header_extensions(extensions);
  transceiver_->OnNegotiationUpdate(SdpType::kPrAnswer, &description);

  transceiver_->SetChannel(std::move(mock_channel),
                           [](const std::string&) { return nullptr; });

  EXPECT_THAT(transceiver_->GetNegotiatedHeaderExtensions(),
              ElementsAre(Field(&RtpHeaderExtensionCapability::direction,
                                RtpTransceiverDirection::kSendRecv),
                          Field(&RtpHeaderExtensionCapability::direction,
                                RtpTransceiverDirection::kSendRecv),
                          Field(&RtpHeaderExtensionCapability::direction,
                                RtpTransceiverDirection::kStopped),
                          Field(&RtpHeaderExtensionCapability::direction,
                                RtpTransceiverDirection::kStopped)));
  ClearChannel();
}

TEST_F(RtpTransceiverTestForHeaderExtensions,
       ReturnsNegotiatedHdrExtsSecondTime) {
  RtpHeaderExtensions extensions = {RtpExtension("uri1", 1),
                                    RtpExtension("uri2", 2)};
  AudioContentDescription description;
  description.set_rtp_header_extensions(extensions);
  transceiver_->OnNegotiationUpdate(SdpType::kAnswer, &description);

  EXPECT_THAT(transceiver_->GetNegotiatedHeaderExtensions(),
              ElementsAre(Field(&RtpHeaderExtensionCapability::direction,
                                RtpTransceiverDirection::kSendRecv),
                          Field(&RtpHeaderExtensionCapability::direction,
                                RtpTransceiverDirection::kSendRecv),
                          Field(&RtpHeaderExtensionCapability::direction,
                                RtpTransceiverDirection::kStopped),
                          Field(&RtpHeaderExtensionCapability::direction,
                                RtpTransceiverDirection::kStopped)));
  extensions = {RtpExtension("uri3", 4), RtpExtension("uri5", 6)};
  description.set_rtp_header_extensions(extensions);
  transceiver_->OnNegotiationUpdate(SdpType::kAnswer, &description);

  EXPECT_THAT(transceiver_->GetNegotiatedHeaderExtensions(),
              ElementsAre(Field(&RtpHeaderExtensionCapability::direction,
                                RtpTransceiverDirection::kStopped),
                          Field(&RtpHeaderExtensionCapability::direction,
                                RtpTransceiverDirection::kStopped),
                          Field(&RtpHeaderExtensionCapability::direction,
                                RtpTransceiverDirection::kStopped),
                          Field(&RtpHeaderExtensionCapability::direction,
                                RtpTransceiverDirection::kStopped)));
}

TEST_F(RtpTransceiverTestForHeaderExtensions,
       SimulcastOrSvcEnablesExtensionsByDefault) {
  std::vector<RtpHeaderExtensionCapability> extensions = {
      {RtpExtension::kDependencyDescriptorUri, 1,
       RtpTransceiverDirection::kStopped},
      {RtpExtension::kVideoLayersAllocationUri, 2,
       RtpTransceiverDirection::kStopped},
  };

  // Default is stopped.
  auto sender = make_ref_counted<NiceMock<MockRtpSenderInternal>>();
  auto transceiver = make_ref_counted<RtpTransceiver>(
      env(),
      RtpSenderProxyWithInternal<RtpSenderInternal>::Create(Thread::Current(),
                                                            sender),
      RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
          Thread::Current(), Thread::Current(), receiver_),
      context(), codec_lookup_helper(), extensions,
      /* on_negotiation_needed= */ [] {});
  std::vector<RtpHeaderExtensionCapability> header_extensions =
      transceiver->GetHeaderExtensionsToNegotiate();
  ASSERT_EQ(header_extensions.size(), 2u);
  EXPECT_EQ(header_extensions[0].uri, RtpExtension::kDependencyDescriptorUri);
  EXPECT_EQ(header_extensions[0].direction, RtpTransceiverDirection::kStopped);
  EXPECT_EQ(header_extensions[1].uri, RtpExtension::kVideoLayersAllocationUri);
  EXPECT_EQ(header_extensions[1].direction, RtpTransceiverDirection::kStopped);

  // Simulcast, i.e. more than one encoding.
  RtpParameters simulcast_parameters;
  simulcast_parameters.encodings.resize(2);
  auto simulcast_sender = make_ref_counted<NiceMock<MockRtpSenderInternal>>();
  EXPECT_CALL(*simulcast_sender, GetParametersInternal())
      .WillRepeatedly(Return(simulcast_parameters));
  auto simulcast_transceiver = make_ref_counted<RtpTransceiver>(
      env(),
      RtpSenderProxyWithInternal<RtpSenderInternal>::Create(Thread::Current(),
                                                            simulcast_sender),
      RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
          Thread::Current(), Thread::Current(), receiver_),
      context(), codec_lookup_helper(), extensions,
      /* on_negotiation_needed= */ [] {});
  auto simulcast_extensions =
      simulcast_transceiver->GetHeaderExtensionsToNegotiate();
  ASSERT_EQ(simulcast_extensions.size(), 2u);
  EXPECT_EQ(simulcast_extensions[0].uri,
            RtpExtension::kDependencyDescriptorUri);
  EXPECT_EQ(simulcast_extensions[0].direction,
            RtpTransceiverDirection::kSendRecv);
  EXPECT_EQ(simulcast_extensions[1].uri,
            RtpExtension::kVideoLayersAllocationUri);
  EXPECT_EQ(simulcast_extensions[1].direction,
            RtpTransceiverDirection::kSendRecv);

  // SVC, a single encoding with a scalabilityMode other than L1T1.
  RtpParameters svc_parameters;
  svc_parameters.encodings.resize(1);
  svc_parameters.encodings[0].scalability_mode = "L3T3";

  auto svc_sender = make_ref_counted<NiceMock<MockRtpSenderInternal>>();
  EXPECT_CALL(*svc_sender, GetParametersInternal())
      .WillRepeatedly(Return(svc_parameters));
  auto svc_transceiver = make_ref_counted<RtpTransceiver>(
      env(),
      RtpSenderProxyWithInternal<RtpSenderInternal>::Create(Thread::Current(),
                                                            svc_sender),
      RtpReceiverProxyWithInternal<RtpReceiverInternal>::Create(
          Thread::Current(), Thread::Current(), receiver_),
      context(), codec_lookup_helper(), extensions,
      /* on_negotiation_needed= */ [] {});
  std::vector<RtpHeaderExtensionCapability> svc_extensions =
      svc_transceiver->GetHeaderExtensionsToNegotiate();
  ASSERT_EQ(svc_extensions.size(), 2u);
  EXPECT_EQ(svc_extensions[0].uri, RtpExtension::kDependencyDescriptorUri);
  EXPECT_EQ(svc_extensions[0].direction, RtpTransceiverDirection::kSendRecv);
  EXPECT_EQ(svc_extensions[1].uri, RtpExtension::kVideoLayersAllocationUri);
  EXPECT_EQ(svc_extensions[1].direction, RtpTransceiverDirection::kSendRecv);
}

}  // namespace

}  // namespace webrtc
