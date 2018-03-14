/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <tuple>

#include "api/peerconnectionproxy.h"
#include "media/base/fakemediaengine.h"
#include "pc/mediasession.h"
#include "pc/peerconnection.h"
#include "pc/peerconnectionfactory.h"
#include "pc/peerconnectionwrapper.h"
#ifdef WEBRTC_ANDROID
#include "pc/test/androidtestinitializer.h"
#endif
#include "pc/test/fakesctptransport.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/virtualsocketserver.h"

namespace webrtc {

using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;
using RTCOfferAnswerOptions = PeerConnectionInterface::RTCOfferAnswerOptions;
using ::testing::Values;

class PeerConnectionFactoryForDataChannelTest
    : public rtc::RefCountedObject<PeerConnectionFactory> {
 public:
  PeerConnectionFactoryForDataChannelTest()
      : rtc::RefCountedObject<PeerConnectionFactory>(
            rtc::Thread::Current(),
            rtc::Thread::Current(),
            rtc::Thread::Current(),
            rtc::MakeUnique<cricket::FakeMediaEngine>(),
            CreateCallFactory(),
            nullptr) {}

  std::unique_ptr<cricket::SctpTransportInternalFactory>
  CreateSctpTransportInternalFactory() {
    auto factory = rtc::MakeUnique<FakeSctpTransportFactory>();
    last_fake_sctp_transport_factory_ = factory.get();
    return factory;
  }

  FakeSctpTransportFactory* last_fake_sctp_transport_factory_ = nullptr;
};

class PeerConnectionWrapperForDataChannelTest : public PeerConnectionWrapper {
 public:
  using PeerConnectionWrapper::PeerConnectionWrapper;

  FakeSctpTransportFactory* sctp_transport_factory() {
    return sctp_transport_factory_;
  }

  void set_sctp_transport_factory(
      FakeSctpTransportFactory* sctp_transport_factory) {
    sctp_transport_factory_ = sctp_transport_factory;
  }

  rtc::Optional<std::string> sctp_content_name() {
    return GetInternalPeerConnection()->sctp_content_name();
  }

  rtc::Optional<std::string> sctp_transport_name() {
    return GetInternalPeerConnection()->sctp_transport_name();
  }

  PeerConnection* GetInternalPeerConnection() {
    auto* pci =
        static_cast<PeerConnectionProxyWithInternal<PeerConnectionInterface>*>(
            pc());
    return static_cast<PeerConnection*>(pci->internal());
  }

 private:
  FakeSctpTransportFactory* sctp_transport_factory_ = nullptr;
};

class PeerConnectionDataChannelTest : public ::testing::Test {
 protected:
  typedef std::unique_ptr<PeerConnectionWrapperForDataChannelTest> WrapperPtr;

  PeerConnectionDataChannelTest()
      : vss_(new rtc::VirtualSocketServer()), main_(vss_.get()) {
#ifdef WEBRTC_ANDROID
    InitializeAndroidObjects();
#endif
  }

  WrapperPtr CreatePeerConnection() {
    return CreatePeerConnection(RTCConfiguration());
  }

  WrapperPtr CreatePeerConnection(const RTCConfiguration& config) {
    return CreatePeerConnection(config,
                                PeerConnectionFactoryInterface::Options());
  }

  WrapperPtr CreatePeerConnection(
      const RTCConfiguration& config,
      const PeerConnectionFactoryInterface::Options factory_options) {
    rtc::scoped_refptr<PeerConnectionFactoryForDataChannelTest> pc_factory(
        new PeerConnectionFactoryForDataChannelTest());
    pc_factory->SetOptions(factory_options);
    RTC_CHECK(pc_factory->Initialize());
    auto observer = rtc::MakeUnique<MockPeerConnectionObserver>();
    auto pc = pc_factory->CreatePeerConnection(config, nullptr, nullptr,
                                               observer.get());
    if (!pc) {
      return nullptr;
    }

    auto wrapper = rtc::MakeUnique<PeerConnectionWrapperForDataChannelTest>(
        pc_factory, pc, std::move(observer));
    RTC_DCHECK(pc_factory->last_fake_sctp_transport_factory_);
    wrapper->set_sctp_transport_factory(
        pc_factory->last_fake_sctp_transport_factory_);
    return wrapper;
  }

  // Accepts the same arguments as CreatePeerConnection and adds a default data
  // channel.
  template <typename... Args>
  WrapperPtr CreatePeerConnectionWithDataChannel(Args&&... args) {
    auto wrapper = CreatePeerConnection(std::forward<Args>(args)...);
    if (!wrapper) {
      return nullptr;
    }
    EXPECT_TRUE(wrapper->pc()->CreateDataChannel("dc", nullptr));
    return wrapper;
  }

  // Changes the SCTP data channel port on the given session description.
  void ChangeSctpPortOnDescription(cricket::SessionDescription* desc,
                                   int port) {
    cricket::DataCodec sctp_codec(cricket::kGoogleSctpDataCodecPlType,
                                  cricket::kGoogleSctpDataCodecName);
    sctp_codec.SetParam(cricket::kCodecParamPort, port);

    auto* data_content = cricket::GetFirstDataContent(desc);
    RTC_DCHECK(data_content);
    auto* data_desc = data_content->media_description()->as_data();
    data_desc->set_codecs({sctp_codec});
  }

  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  rtc::AutoSocketServerThread main_;
};

TEST_F(PeerConnectionDataChannelTest,
       NoSctpTransportCreatedIfRtpDataChannelEnabled) {
  RTCConfiguration config;
  config.enable_rtp_data_channel = true;
  auto caller = CreatePeerConnectionWithDataChannel(config);

  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));
  EXPECT_FALSE(caller->sctp_transport_factory()->last_fake_sctp_transport());
}

TEST_F(PeerConnectionDataChannelTest,
       RtpDataChannelCreatedEvenIfSctpAvailable) {
  RTCConfiguration config;
  config.enable_rtp_data_channel = true;
  PeerConnectionFactoryInterface::Options options;
  options.disable_sctp_data_channels = false;
  auto caller = CreatePeerConnectionWithDataChannel(config, options);

  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));
  EXPECT_FALSE(caller->sctp_transport_factory()->last_fake_sctp_transport());
}

// Test that sctp_content_name/sctp_transport_name (used for stats) are correct
// before and after BUNDLE is negotiated.
TEST_F(PeerConnectionDataChannelTest, SctpContentAndTransportNameSetCorrectly) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  // Initially these fields should be empty.
  EXPECT_FALSE(caller->sctp_content_name());
  EXPECT_FALSE(caller->sctp_transport_name());

  // Create offer with audio/video/data.
  // Default bundle policy is "balanced", so data should be using its own
  // transport.
  caller->AddAudioTrack("a");
  caller->AddVideoTrack("v");
  caller->pc()->CreateDataChannel("dc", nullptr);
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_TRUE(caller->sctp_content_name());
  EXPECT_EQ(cricket::CN_DATA, *caller->sctp_content_name());
  ASSERT_TRUE(caller->sctp_transport_name());
  EXPECT_EQ(cricket::CN_DATA, *caller->sctp_transport_name());

  // Create answer that finishes BUNDLE negotiation, which means everything
  // should be bundled on the first transport (audio).
  RTCOfferAnswerOptions options;
  options.use_rtp_mux = true;
  ASSERT_TRUE(
      caller->SetRemoteDescription(callee->CreateAnswerAndSetAsLocal()));

  ASSERT_TRUE(caller->sctp_content_name());
  EXPECT_EQ(cricket::CN_DATA, *caller->sctp_content_name());
  ASSERT_TRUE(caller->sctp_transport_name());
  EXPECT_EQ(cricket::CN_AUDIO, *caller->sctp_transport_name());
}

TEST_F(PeerConnectionDataChannelTest,
       CreateOfferWithNoDataChannelsGivesNoDataSection) {
  auto caller = CreatePeerConnection();
  auto offer = caller->CreateOffer();

  EXPECT_FALSE(offer->description()->GetContentByName(cricket::CN_DATA));
  EXPECT_FALSE(offer->description()->GetTransportInfoByName(cricket::CN_DATA));
}

TEST_F(PeerConnectionDataChannelTest,
       CreateAnswerWithRemoteSctpDataChannelIncludesDataSection) {
  auto caller = CreatePeerConnectionWithDataChannel();
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  auto answer = callee->CreateAnswer();
  ASSERT_TRUE(answer);
  auto* data_content =
      answer->description()->GetContentByName(cricket::CN_DATA);
  ASSERT_TRUE(data_content);
  EXPECT_FALSE(data_content->rejected);
  EXPECT_TRUE(answer->description()->GetTransportInfoByName(cricket::CN_DATA));
}

TEST_F(PeerConnectionDataChannelTest,
       CreateDataChannelWithDtlsDisabledSucceeds) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnection();

  EXPECT_TRUE(caller->pc()->CreateDataChannel("dc", nullptr));
}

TEST_F(PeerConnectionDataChannelTest, CreateDataChannelWithSctpDisabledFails) {
  PeerConnectionFactoryInterface::Options options;
  options.disable_sctp_data_channels = true;
  auto caller = CreatePeerConnection(RTCConfiguration(), options);

  EXPECT_FALSE(caller->pc()->CreateDataChannel("dc", nullptr));
}

// Test that if a callee has SCTP disabled and receives an offer with an SCTP
// data channel, the data section is rejected and no SCTP transport is created
// on the callee.
TEST_F(PeerConnectionDataChannelTest,
       DataSectionRejectedIfCalleeHasSctpDisabled) {
  auto caller = CreatePeerConnectionWithDataChannel();
  PeerConnectionFactoryInterface::Options options;
  options.disable_sctp_data_channels = true;
  auto callee = CreatePeerConnection(RTCConfiguration(), options);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  EXPECT_FALSE(callee->sctp_transport_factory()->last_fake_sctp_transport());

  auto answer = callee->CreateAnswer();
  auto* data_content =
      answer->description()->GetContentByName(cricket::CN_DATA);
  ASSERT_TRUE(data_content);
  EXPECT_TRUE(data_content->rejected);
}

TEST_F(PeerConnectionDataChannelTest, SctpPortPropagatedFromSdpToTransport) {
  constexpr int kNewSendPort = 9998;
  constexpr int kNewRecvPort = 7775;

  auto caller = CreatePeerConnectionWithDataChannel();
  auto callee = CreatePeerConnectionWithDataChannel();

  auto offer = caller->CreateOffer();
  ChangeSctpPortOnDescription(offer->description(), kNewSendPort);
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswer();
  ChangeSctpPortOnDescription(answer->description(), kNewRecvPort);
  ASSERT_TRUE(callee->SetLocalDescription(std::move(answer)));

  auto* callee_transport =
      callee->sctp_transport_factory()->last_fake_sctp_transport();
  ASSERT_TRUE(callee_transport);
  EXPECT_EQ(kNewSendPort, callee_transport->remote_port());
  EXPECT_EQ(kNewRecvPort, callee_transport->local_port());
}

}  // namespace webrtc
