/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>

#include <memory>
#include <ostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/audio/audio_mixer.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/crypto/crypto_options.h"
#include "api/crypto_params.h"
#include "api/jsep.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "p2p/base/fake_port_allocator.h"
#include "p2p/base/port_allocator.h"
#include "p2p/base/transport_description.h"
#include "p2p/base/transport_info.h"
#include "pc/media_protocol_names.h"
#include "pc/media_session.h"
#include "pc/peer_connection_wrapper.h"
#include "pc/sdp_utils.h"
#include "pc/session_description.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/checks.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/ssl_fingerprint.h"
#include "rtc_base/thread.h"
#include "test/gtest.h"
#ifdef WEBRTC_ANDROID
#include "pc/test/android_test_initializer.h"
#endif
#include "pc/test/fake_audio_capture_module.h"
#include "pc/test/fake_rtc_certificate_generator.h"
#include "rtc_base/gunit.h"
#include "rtc_base/virtual_socket_server.h"

namespace webrtc {

using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;
using RTCOfferAnswerOptions = PeerConnectionInterface::RTCOfferAnswerOptions;
using ::testing::Combine;
using ::testing::Values;

constexpr int kGenerateCertTimeout = 1000;

class PeerConnectionCryptoBaseTest : public ::testing::Test {
 protected:
  typedef std::unique_ptr<PeerConnectionWrapper> WrapperPtr;

  explicit PeerConnectionCryptoBaseTest(SdpSemantics sdp_semantics)
      : vss_(new rtc::VirtualSocketServer()),
        main_(vss_.get()),
        sdp_semantics_(sdp_semantics) {
#ifdef WEBRTC_ANDROID
    InitializeAndroidObjects();
#endif
    pc_factory_ = CreatePeerConnectionFactory(
        rtc::Thread::Current(), rtc::Thread::Current(), rtc::Thread::Current(),
        FakeAudioCaptureModule::Create(), CreateBuiltinAudioEncoderFactory(),
        CreateBuiltinAudioDecoderFactory(), CreateBuiltinVideoEncoderFactory(),
        CreateBuiltinVideoDecoderFactory(), nullptr /* audio_mixer */,
        nullptr /* audio_processing */);
  }

  WrapperPtr CreatePeerConnection() {
    return CreatePeerConnection(RTCConfiguration());
  }

  WrapperPtr CreatePeerConnection(const RTCConfiguration& config) {
    return CreatePeerConnection(config, nullptr);
  }

  WrapperPtr CreatePeerConnection(
      const RTCConfiguration& config,
      std::unique_ptr<rtc::RTCCertificateGeneratorInterface> cert_gen) {
    auto fake_port_allocator = std::make_unique<cricket::FakePortAllocator>(
        rtc::Thread::Current(),
        std::make_unique<rtc::BasicPacketSocketFactory>(vss_.get()));
    auto observer = std::make_unique<MockPeerConnectionObserver>();
    RTCConfiguration modified_config = config;
    modified_config.sdp_semantics = sdp_semantics_;
    PeerConnectionDependencies pc_dependencies(observer.get());
    pc_dependencies.allocator = std::move(fake_port_allocator);
    pc_dependencies.cert_generator = std::move(cert_gen);
    auto result = pc_factory_->CreatePeerConnectionOrError(
        modified_config, std::move(pc_dependencies));
    if (!result.ok()) {
      return nullptr;
    }

    observer->SetPeerConnectionInterface(result.value().get());
    return std::make_unique<PeerConnectionWrapper>(
        pc_factory_, result.MoveValue(), std::move(observer));
  }

  // Accepts the same arguments as CreatePeerConnection and adds default audio
  // and video tracks.
  template <typename... Args>
  WrapperPtr CreatePeerConnectionWithAudioVideo(Args&&... args) {
    auto wrapper = CreatePeerConnection(std::forward<Args>(args)...);
    if (!wrapper) {
      return nullptr;
    }
    wrapper->AddAudioTrack("a");
    wrapper->AddVideoTrack("v");
    return wrapper;
  }

  cricket::ConnectionRole& AudioConnectionRole(
      cricket::SessionDescription* desc) {
    return ConnectionRoleFromContent(desc, cricket::GetFirstAudioContent(desc));
  }

  cricket::ConnectionRole& VideoConnectionRole(
      cricket::SessionDescription* desc) {
    return ConnectionRoleFromContent(desc, cricket::GetFirstVideoContent(desc));
  }

  cricket::ConnectionRole& ConnectionRoleFromContent(
      cricket::SessionDescription* desc,
      cricket::ContentInfo* content) {
    RTC_DCHECK(content);
    auto* transport_info = desc->GetTransportInfoByName(content->name);
    RTC_DCHECK(transport_info);
    return transport_info->description.connection_role;
  }

  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  rtc::AutoSocketServerThread main_;
  rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory_;
  const SdpSemantics sdp_semantics_;
};

SdpContentPredicate HaveDtlsFingerprint() {
  return [](const cricket::ContentInfo* content,
            const cricket::TransportInfo* transport) {
    return transport->description.identity_fingerprint != nullptr;
  };
}

SdpContentPredicate HaveSdesCryptos() {
  return [](const cricket::ContentInfo* content,
            const cricket::TransportInfo* transport) {
    return !content->media_description()->cryptos().empty();
  };
}

SdpContentPredicate HaveProtocol(const std::string& protocol) {
  return [protocol](const cricket::ContentInfo* content,
                    const cricket::TransportInfo* transport) {
    return content->media_description()->protocol() == protocol;
  };
}

SdpContentPredicate HaveSdesGcmCryptos(size_t num_crypto_suites) {
  return [num_crypto_suites](const cricket::ContentInfo* content,
                             const cricket::TransportInfo* transport) {
    const auto& cryptos = content->media_description()->cryptos();
    if (cryptos.size() != num_crypto_suites) {
      return false;
    }
    for (size_t i = 0; i < cryptos.size(); ++i) {
      if (cryptos[i].key_params.size() == 67U &&
          cryptos[i].cipher_suite == "AEAD_AES_256_GCM")
        return true;
    }
    return false;
  };
}

class PeerConnectionCryptoTest
    : public PeerConnectionCryptoBaseTest,
      public ::testing::WithParamInterface<SdpSemantics> {
 protected:
  PeerConnectionCryptoTest() : PeerConnectionCryptoBaseTest(GetParam()) {}
};

SdpContentMutator RemoveSdesCryptos() {
  return [](cricket::ContentInfo* content, cricket::TransportInfo* transport) {
    content->media_description()->set_cryptos({});
  };
}

SdpContentMutator RemoveDtlsFingerprint() {
  return [](cricket::ContentInfo* content, cricket::TransportInfo* transport) {
    transport->description.identity_fingerprint.reset();
  };
}

// When DTLS is enabled, the SDP offer/answer should have a DTLS fingerprint and
// no SDES cryptos.
TEST_P(PeerConnectionCryptoTest, CorrectCryptoInOfferWhenDtlsEnabled) {
  RTCConfiguration config;
  auto caller = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  ASSERT_TRUE(offer);

  ASSERT_FALSE(offer->description()->contents().empty());
  EXPECT_TRUE(SdpContentsAll(HaveDtlsFingerprint(), offer->description()));
  EXPECT_TRUE(SdpContentsNone(HaveSdesCryptos(), offer->description()));
  EXPECT_TRUE(SdpContentsAll(HaveProtocol(cricket::kMediaProtocolDtlsSavpf),
                             offer->description()));
}
TEST_P(PeerConnectionCryptoTest, CorrectCryptoInAnswerWhenDtlsEnabled) {
  RTCConfiguration config;
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  callee->SetRemoteDescription(caller->CreateOffer());
  auto answer = callee->CreateAnswer();
  ASSERT_TRUE(answer);

  ASSERT_FALSE(answer->description()->contents().empty());
  EXPECT_TRUE(SdpContentsAll(HaveDtlsFingerprint(), answer->description()));
  EXPECT_TRUE(SdpContentsNone(HaveSdesCryptos(), answer->description()));
  EXPECT_TRUE(SdpContentsAll(HaveProtocol(cricket::kMediaProtocolDtlsSavpf),
                             answer->description()));
}

#if defined(WEBRTC_FUCHSIA)
// When DTLS is disabled, the SDP offer/answer should include SDES cryptos and
// should not have a DTLS fingerprint.
TEST_P(PeerConnectionCryptoTest, CorrectCryptoInOfferWhenDtlsDisabled) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  ASSERT_TRUE(offer);

  ASSERT_FALSE(offer->description()->contents().empty());
  EXPECT_TRUE(SdpContentsAll(HaveSdesCryptos(), offer->description()));
  EXPECT_TRUE(SdpContentsNone(HaveDtlsFingerprint(), offer->description()));
  EXPECT_TRUE(SdpContentsAll(HaveProtocol(cricket::kMediaProtocolSavpf),
                             offer->description()));
}

TEST_P(PeerConnectionCryptoTest, CorrectCryptoInAnswerWhenDtlsDisabled) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  callee->SetRemoteDescription(caller->CreateOffer());
  auto answer = callee->CreateAnswer();
  ASSERT_TRUE(answer);

  ASSERT_FALSE(answer->description()->contents().empty());
  EXPECT_TRUE(SdpContentsAll(HaveSdesCryptos(), answer->description()));
  EXPECT_TRUE(SdpContentsNone(HaveDtlsFingerprint(), answer->description()));
  EXPECT_TRUE(SdpContentsAll(HaveProtocol(cricket::kMediaProtocolSavpf),
                             answer->description()));
}

// When encryption is disabled, the SDP offer/answer should have neither a DTLS
// fingerprint nor any SDES crypto options.
TEST_P(PeerConnectionCryptoTest, CorrectCryptoInOfferWhenEncryptionDisabled) {
  PeerConnectionFactoryInterface::Options options;
  options.disable_encryption = true;
  pc_factory_->SetOptions(options);

  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  ASSERT_TRUE(offer);

  ASSERT_FALSE(offer->description()->contents().empty());
  EXPECT_TRUE(SdpContentsNone(HaveSdesCryptos(), offer->description()));
  EXPECT_TRUE(SdpContentsNone(HaveDtlsFingerprint(), offer->description()));
  EXPECT_TRUE(SdpContentsAll(HaveProtocol(cricket::kMediaProtocolAvpf),
                             offer->description()));
}

TEST_P(PeerConnectionCryptoTest, CorrectCryptoInAnswerWhenEncryptionDisabled) {
  PeerConnectionFactoryInterface::Options options;
  options.disable_encryption = true;
  pc_factory_->SetOptions(options);

  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  callee->SetRemoteDescription(caller->CreateOffer());
  auto answer = callee->CreateAnswer();
  ASSERT_TRUE(answer);

  ASSERT_FALSE(answer->description()->contents().empty());
  EXPECT_TRUE(SdpContentsNone(HaveSdesCryptos(), answer->description()));
  EXPECT_TRUE(SdpContentsNone(HaveDtlsFingerprint(), answer->description()));
  EXPECT_TRUE(SdpContentsAll(HaveProtocol(cricket::kMediaProtocolAvpf),
                             answer->description()));
}

// CryptoOptions has been promoted to RTCConfiguration. As such if it is ever
// set in the configuration it should overrite the settings set in the factory.
TEST_P(PeerConnectionCryptoTest, RTCConfigurationCryptoOptionOverridesFactory) {
  PeerConnectionFactoryInterface::Options options;
  options.crypto_options.srtp.enable_gcm_crypto_suites = true;
  pc_factory_->SetOptions(options);

  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  CryptoOptions crypto_options;
  crypto_options.srtp.enable_gcm_crypto_suites = false;
  config.crypto_options = crypto_options;
  auto caller = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  ASSERT_TRUE(offer);

  ASSERT_FALSE(offer->description()->contents().empty());
  // This should exist if GCM is enabled see CorrectCryptoInOfferWithSdesAndGcm
  EXPECT_FALSE(SdpContentsAll(HaveSdesGcmCryptos(3), offer->description()));
}

// When DTLS is disabled and GCM cipher suites are enabled, the SDP offer/answer
// should have the correct ciphers in the SDES crypto options.
// With GCM cipher suites enabled, there will be 3 cryptos in the offer and 1
// in the answer.
TEST_P(PeerConnectionCryptoTest, CorrectCryptoInOfferWithSdesAndGcm) {
  PeerConnectionFactoryInterface::Options options;
  options.crypto_options.srtp.enable_gcm_crypto_suites = true;
  pc_factory_->SetOptions(options);

  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  ASSERT_TRUE(offer);

  ASSERT_FALSE(offer->description()->contents().empty());
  EXPECT_TRUE(SdpContentsAll(HaveSdesGcmCryptos(3), offer->description()));
}

TEST_P(PeerConnectionCryptoTest, CorrectCryptoInAnswerWithSdesAndGcm) {
  PeerConnectionFactoryInterface::Options options;
  options.crypto_options.srtp.enable_gcm_crypto_suites = true;
  pc_factory_->SetOptions(options);

  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  for (cricket::ContentInfo& content : offer->description()->contents()) {
    auto cryptos = content.media_description()->cryptos();
    cryptos.erase(cryptos.begin());  // Assumes that non-GCM is the default.
    content.media_description()->set_cryptos(cryptos);
  }

  callee->SetRemoteDescription(std::move(offer));
  auto answer = callee->CreateAnswer();
  ASSERT_TRUE(answer);

  ASSERT_FALSE(answer->description()->contents().empty());
  EXPECT_TRUE(SdpContentsAll(HaveSdesGcmCryptos(1), answer->description()));
}

TEST_P(PeerConnectionCryptoTest, CanSetSdesGcmRemoteOfferAndLocalAnswer) {
  PeerConnectionFactoryInterface::Options options;
  options.crypto_options.srtp.enable_gcm_crypto_suites = true;
  pc_factory_->SetOptions(options);

  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  ASSERT_TRUE(offer);
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswer();
  ASSERT_TRUE(answer);
  ASSERT_TRUE(callee->SetLocalDescription(std::move(answer)));
}

// The following group tests that two PeerConnections can successfully exchange
// an offer/answer when DTLS is off and that they will refuse any offer/answer
// applied locally/remotely if it does not include SDES cryptos.
TEST_P(PeerConnectionCryptoTest, ExchangeOfferAnswerWhenSdesOn) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOfferAndSetAsLocal();
  ASSERT_TRUE(offer);
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswerAndSetAsLocal();
  ASSERT_TRUE(answer);
  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));
}
TEST_P(PeerConnectionCryptoTest, FailToSetLocalOfferWithNoCryptosWhenSdesOn) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  SdpContentsForEach(RemoveSdesCryptos(), offer->description());

  EXPECT_FALSE(caller->SetLocalDescription(std::move(offer)));
}
TEST_P(PeerConnectionCryptoTest, FailToSetRemoteOfferWithNoCryptosWhenSdesOn) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  SdpContentsForEach(RemoveSdesCryptos(), offer->description());

  EXPECT_FALSE(callee->SetRemoteDescription(std::move(offer)));
}
TEST_P(PeerConnectionCryptoTest, FailToSetLocalAnswerWithNoCryptosWhenSdesOn) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal());
  auto answer = callee->CreateAnswer();
  SdpContentsForEach(RemoveSdesCryptos(), answer->description());

  EXPECT_FALSE(callee->SetLocalDescription(std::move(answer)));
}
TEST_P(PeerConnectionCryptoTest, FailToSetRemoteAnswerWithNoCryptosWhenSdesOn) {
  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal());
  auto answer = callee->CreateAnswerAndSetAsLocal();
  SdpContentsForEach(RemoveSdesCryptos(), answer->description());

  EXPECT_FALSE(caller->SetRemoteDescription(std::move(answer)));
}
#endif

// The following group tests that two PeerConnections can successfully exchange
// an offer/answer when DTLS is on and that they will refuse any offer/answer
// applied locally/remotely if it does not include a DTLS fingerprint.
TEST_P(PeerConnectionCryptoTest, ExchangeOfferAnswerWhenDtlsOn) {
  RTCConfiguration config;
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOfferAndSetAsLocal();
  ASSERT_TRUE(offer);
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswerAndSetAsLocal();
  ASSERT_TRUE(answer);
  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));
}
TEST_P(PeerConnectionCryptoTest,
       FailToSetLocalOfferWithNoFingerprintWhenDtlsOn) {
  RTCConfiguration config;
  auto caller = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  SdpContentsForEach(RemoveDtlsFingerprint(), offer->description());

  EXPECT_FALSE(caller->SetLocalDescription(std::move(offer)));
}
TEST_P(PeerConnectionCryptoTest,
       FailToSetRemoteOfferWithNoFingerprintWhenDtlsOn) {
  RTCConfiguration config;
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOffer();
  SdpContentsForEach(RemoveDtlsFingerprint(), offer->description());

  EXPECT_FALSE(callee->SetRemoteDescription(std::move(offer)));
}
TEST_P(PeerConnectionCryptoTest,
       FailToSetLocalAnswerWithNoFingerprintWhenDtlsOn) {
  RTCConfiguration config;
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal());
  auto answer = callee->CreateAnswer();
  SdpContentsForEach(RemoveDtlsFingerprint(), answer->description());
}
TEST_P(PeerConnectionCryptoTest,
       FailToSetRemoteAnswerWithNoFingerprintWhenDtlsOn) {
  RTCConfiguration config;
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal());
  auto answer = callee->CreateAnswerAndSetAsLocal();
  SdpContentsForEach(RemoveDtlsFingerprint(), answer->description());

  EXPECT_FALSE(caller->SetRemoteDescription(std::move(answer)));
}

#if defined(WEBRTC_FUCHSIA)
// Test that an offer/answer can be exchanged when encryption is disabled.
TEST_P(PeerConnectionCryptoTest, ExchangeOfferAnswerWhenNoEncryption) {
  PeerConnectionFactoryInterface::Options options;
  options.disable_encryption = true;
  pc_factory_->SetOptions(options);

  RTCConfiguration config;
  config.enable_dtls_srtp.emplace(false);
  auto caller = CreatePeerConnectionWithAudioVideo(config);
  auto callee = CreatePeerConnectionWithAudioVideo(config);

  auto offer = caller->CreateOfferAndSetAsLocal();
  ASSERT_TRUE(offer);
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswerAndSetAsLocal();
  ASSERT_TRUE(answer);
  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));
}
#endif

// Tests that a DTLS call can be established when the certificate is specified
// in the PeerConnection config and no certificate generator is specified.
TEST_P(PeerConnectionCryptoTest,
       ExchangeOfferAnswerWhenDtlsCertificateInConfig) {
  RTCConfiguration caller_config;
  caller_config.certificates.push_back(
      FakeRTCCertificateGenerator::GenerateCertificate());
  auto caller = CreatePeerConnectionWithAudioVideo(caller_config);

  RTCConfiguration callee_config;
  callee_config.certificates.push_back(
      FakeRTCCertificateGenerator::GenerateCertificate());
  auto callee = CreatePeerConnectionWithAudioVideo(callee_config);

  auto offer = caller->CreateOfferAndSetAsLocal();
  ASSERT_TRUE(offer);
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  auto answer = callee->CreateAnswerAndSetAsLocal();
  ASSERT_TRUE(answer);
  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));
}

// The following parameterized test verifies that CreateOffer/CreateAnswer
// returns successfully (or with failure if the underlying certificate generator
// fails) no matter when the DTLS certificate is generated. If multiple
// CreateOffer/CreateAnswer calls are made while waiting for the certificate,
// they all finish after the certificate is generated.

// Whether the certificate will be generated before calling CreateOffer or
// while CreateOffer is executing.
enum class CertGenTime { kBefore, kDuring };
std::ostream& operator<<(std::ostream& out, CertGenTime value) {
  switch (value) {
    case CertGenTime::kBefore:
      return out << "before";
    case CertGenTime::kDuring:
      return out << "during";
    default:
      return out << "unknown";
  }
}

// Whether the fake certificate generator will produce a certificate or fail.
enum class CertGenResult { kSucceed, kFail };
std::ostream& operator<<(std::ostream& out, CertGenResult value) {
  switch (value) {
    case CertGenResult::kSucceed:
      return out << "succeed";
    case CertGenResult::kFail:
      return out << "fail";
    default:
      return out << "unknown";
  }
}

class PeerConnectionCryptoDtlsCertGenTest
    : public PeerConnectionCryptoBaseTest,
      public ::testing::WithParamInterface<std::tuple<SdpSemantics,
                                                      SdpType,
                                                      CertGenTime,
                                                      CertGenResult,
                                                      size_t>> {
 protected:
  PeerConnectionCryptoDtlsCertGenTest()
      : PeerConnectionCryptoBaseTest(std::get<0>(GetParam())) {
    sdp_type_ = std::get<1>(GetParam());
    cert_gen_time_ = std::get<2>(GetParam());
    cert_gen_result_ = std::get<3>(GetParam());
    concurrent_calls_ = std::get<4>(GetParam());
  }

  SdpType sdp_type_;
  CertGenTime cert_gen_time_;
  CertGenResult cert_gen_result_;
  size_t concurrent_calls_;
};

TEST_P(PeerConnectionCryptoDtlsCertGenTest, TestCertificateGeneration) {
  RTCConfiguration config;
  auto owned_fake_certificate_generator =
      std::make_unique<FakeRTCCertificateGenerator>();
  auto* fake_certificate_generator = owned_fake_certificate_generator.get();
  fake_certificate_generator->set_should_fail(cert_gen_result_ ==
                                              CertGenResult::kFail);
  fake_certificate_generator->set_should_wait(cert_gen_time_ ==
                                              CertGenTime::kDuring);
  WrapperPtr pc;
  if (sdp_type_ == SdpType::kOffer) {
    pc = CreatePeerConnectionWithAudioVideo(
        config, std::move(owned_fake_certificate_generator));
  } else {
    auto caller = CreatePeerConnectionWithAudioVideo(config);
    pc = CreatePeerConnectionWithAudioVideo(
        config, std::move(owned_fake_certificate_generator));
    pc->SetRemoteDescription(caller->CreateOfferAndSetAsLocal());
  }
  if (cert_gen_time_ == CertGenTime::kBefore) {
    ASSERT_TRUE_WAIT(fake_certificate_generator->generated_certificates() +
                             fake_certificate_generator->generated_failures() >
                         0,
                     kGenerateCertTimeout);
  } else {
    ASSERT_EQ(fake_certificate_generator->generated_certificates(), 0);
    fake_certificate_generator->set_should_wait(false);
  }
  std::vector<rtc::scoped_refptr<MockCreateSessionDescriptionObserver>>
      observers;
  for (size_t i = 0; i < concurrent_calls_; i++) {
    rtc::scoped_refptr<MockCreateSessionDescriptionObserver> observer =
        rtc::make_ref_counted<MockCreateSessionDescriptionObserver>();
    observers.push_back(observer);
    if (sdp_type_ == SdpType::kOffer) {
      pc->pc()->CreateOffer(observer.get(),
                            PeerConnectionInterface::RTCOfferAnswerOptions());
    } else {
      pc->pc()->CreateAnswer(observer.get(),
                             PeerConnectionInterface::RTCOfferAnswerOptions());
    }
  }
  for (auto& observer : observers) {
    EXPECT_TRUE_WAIT(observer->called(), 1000);
    if (cert_gen_result_ == CertGenResult::kSucceed) {
      EXPECT_TRUE(observer->result());
    } else {
      EXPECT_FALSE(observer->result());
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    PeerConnectionCryptoTest,
    PeerConnectionCryptoDtlsCertGenTest,
    Combine(Values(SdpSemantics::kPlanB_DEPRECATED, SdpSemantics::kUnifiedPlan),
            Values(SdpType::kOffer, SdpType::kAnswer),
            Values(CertGenTime::kBefore, CertGenTime::kDuring),
            Values(CertGenResult::kSucceed, CertGenResult::kFail),
            Values(1, 3)));

// Test that we can create and set an answer correctly when different
// SSL roles have been negotiated for different transports.
// See: https://bugs.chromium.org/p/webrtc/issues/detail?id=4525
TEST_P(PeerConnectionCryptoTest, CreateAnswerWithDifferentSslRoles) {
  auto caller = CreatePeerConnectionWithAudioVideo();
  auto callee = CreatePeerConnectionWithAudioVideo();

  RTCOfferAnswerOptions options_no_bundle;
  options_no_bundle.use_rtp_mux = false;

  // First, negotiate different SSL roles for audio and video.
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  auto answer = callee->CreateAnswer(options_no_bundle);

  AudioConnectionRole(answer->description()) = cricket::CONNECTIONROLE_ACTIVE;
  VideoConnectionRole(answer->description()) = cricket::CONNECTIONROLE_PASSIVE;

  ASSERT_TRUE(
      callee->SetLocalDescription(CloneSessionDescription(answer.get())));
  ASSERT_TRUE(caller->SetRemoteDescription(std::move(answer)));

  // Now create an offer in the reverse direction, and ensure the initial
  // offerer responds with an answer with the correct SSL roles.
  ASSERT_TRUE(caller->SetRemoteDescription(callee->CreateOfferAndSetAsLocal()));
  answer = caller->CreateAnswer(options_no_bundle);

  EXPECT_EQ(cricket::CONNECTIONROLE_PASSIVE,
            AudioConnectionRole(answer->description()));
  EXPECT_EQ(cricket::CONNECTIONROLE_ACTIVE,
            VideoConnectionRole(answer->description()));

  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(answer.get())));
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(answer)));

  // Lastly, start BUNDLE-ing on "audio", expecting that the "passive" role of
  // audio is transferred over to video in the answer that completes the BUNDLE
  // negotiation.
  RTCOfferAnswerOptions options_bundle;
  options_bundle.use_rtp_mux = true;

  ASSERT_TRUE(caller->SetRemoteDescription(callee->CreateOfferAndSetAsLocal()));
  answer = caller->CreateAnswer(options_bundle);

  EXPECT_EQ(cricket::CONNECTIONROLE_PASSIVE,
            AudioConnectionRole(answer->description()));
  EXPECT_EQ(cricket::CONNECTIONROLE_PASSIVE,
            VideoConnectionRole(answer->description()));

  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(answer.get())));
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(answer)));
}

// Tests that if the DTLS fingerprint is invalid then all future calls to
// SetLocalDescription and SetRemoteDescription will fail due to a session
// error.
// This is a regression test for crbug.com/800775
TEST_P(PeerConnectionCryptoTest, SessionErrorIfFingerprintInvalid) {
  auto callee_certificate = rtc::RTCCertificate::FromPEM(kRsaPems[0]);
  auto other_certificate = rtc::RTCCertificate::FromPEM(kRsaPems[1]);

  auto caller = CreatePeerConnectionWithAudioVideo();
  RTCConfiguration callee_config;
  callee_config.certificates.push_back(callee_certificate);
  auto callee = CreatePeerConnectionWithAudioVideo(callee_config);

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  // Create an invalid answer with the other certificate's fingerprint.
  auto valid_answer = callee->CreateAnswer();
  auto invalid_answer = CloneSessionDescription(valid_answer.get());
  auto* audio_content =
      cricket::GetFirstAudioContent(invalid_answer->description());
  ASSERT_TRUE(audio_content);
  auto* audio_transport_info =
      invalid_answer->description()->GetTransportInfoByName(
          audio_content->name);
  ASSERT_TRUE(audio_transport_info);
  audio_transport_info->description.identity_fingerprint =
      rtc::SSLFingerprint::CreateFromCertificate(*other_certificate);

  // Set the invalid answer and expect a fingerprint error.
  std::string error;
  ASSERT_FALSE(callee->SetLocalDescription(std::move(invalid_answer), &error));
  EXPECT_PRED_FORMAT2(AssertStringContains, error,
                      "Local fingerprint does not match identity.");

  // Make sure that setting a valid remote offer or local answer also fails now.
  ASSERT_FALSE(callee->SetRemoteDescription(caller->CreateOffer(), &error));
  EXPECT_PRED_FORMAT2(AssertStringContains, error,
                      "Session error code: ERROR_CONTENT.");
  ASSERT_FALSE(callee->SetLocalDescription(std::move(valid_answer), &error));
  EXPECT_PRED_FORMAT2(AssertStringContains, error,
                      "Session error code: ERROR_CONTENT.");
}

INSTANTIATE_TEST_SUITE_P(PeerConnectionCryptoTest,
                         PeerConnectionCryptoTest,
                         Values(SdpSemantics::kPlanB_DEPRECATED,
                                SdpSemantics::kUnifiedPlan));

}  // namespace webrtc
