/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/dtls_transport.h"

#include <memory>
#include <optional>
#include <vector>

#include "api/dtls_transport_interface.h"
#include "api/make_ref_counted.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/test/rtc_error_matchers.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/dtls/dtls_transport_internal.h"
#include "p2p/dtls/fake_dtls_transport.h"
#include "rtc_base/fake_ssl_identity.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/ssl_identity.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

namespace webrtc {

constexpr int kNonsenseCipherSuite = 1234;

using ::testing::ElementsAre;

class TestDtlsTransportObserver : public DtlsTransportObserverInterface {
 public:
  void OnStateChange(DtlsTransportInformation info) override {
    state_change_called_ = true;
    states_.push_back(info.state());
    info_ = info;
  }

  void OnError(RTCError error) override {}

  DtlsTransportState state() {
    if (!states_.empty()) {
      return states_[states_.size() - 1];
    } else {
      return DtlsTransportState::kNew;
    }
  }

  bool state_change_called_ = false;
  DtlsTransportInformation info_;
  std::vector<DtlsTransportState> states_;
};

class DtlsTransportTest : public ::testing::Test {
 public:
  DtlsTransport* transport() { return transport_.get(); }

  void TearDown() override {
    if (transport_ && internal_transport_) {
      internal_transport_->UnsubscribeDtlsTransportState(transport_.get());
      transport_->Clear(internal_transport_.get());
    }
  }

  void CreateTransport(FakeSSLCertificate* certificate = nullptr) {
    internal_transport_ = std::make_unique<FakeDtlsTransport>(
        "audio", ICE_CANDIDATE_COMPONENT_RTP);
    if (certificate) {
      internal_transport_->SetRemoteSSLCertificate(certificate);
    }
    internal_transport_->SetSslCipherSuite(kNonsenseCipherSuite);
    transport_ =
        make_ref_counted<DtlsTransport>(internal_transport_.get(), &observer_);
    internal_transport_->SubscribeDtlsTransportState(
        transport_.get(),
        [this](DtlsTransportInternal* transport, DtlsTransportState state) {
          transport_->OnInternalDtlsState(transport);
        });
  }

  void CompleteDtlsHandshake() {
    auto fake_dtls1 = internal_transport_.get();
    auto fake_dtls2 = std::make_unique<FakeDtlsTransport>(
        "audio", ICE_CANDIDATE_COMPONENT_RTP);
    auto cert1 =
        RTCCertificate::Create(SSLIdentity::Create("session1", KT_DEFAULT));
    fake_dtls1->SetLocalCertificate(cert1);
    auto cert2 =
        RTCCertificate::Create(SSLIdentity::Create("session1", KT_DEFAULT));
    fake_dtls2->SetLocalCertificate(cert2);
    fake_dtls1->SetDestination(fake_dtls2.get());
  }

  AutoThread main_thread_;
  scoped_refptr<DtlsTransport> transport_;
  std::unique_ptr<FakeDtlsTransport> internal_transport_;
  TestDtlsTransportObserver observer_;
};

TEST_F(DtlsTransportTest, CreateClearDelete) {
  auto transport =
      std::make_unique<FakeDtlsTransport>("audio", ICE_CANDIDATE_COMPONENT_RTP);
  auto dtls_transport = make_ref_counted<DtlsTransport>(transport.get());
  ASSERT_EQ(DtlsTransportState::kNew, dtls_transport->Information().state());
  dtls_transport->Clear(transport.get());
  ASSERT_EQ(DtlsTransportState::kClosed, dtls_transport->Information().state());
}

TEST_F(DtlsTransportTest, EventsObservedWhenConnecting) {
  CreateTransport();
  CompleteDtlsHandshake();
  ASSERT_THAT(WaitUntil([&] { return observer_.state_change_called_; },
                        ::testing::IsTrue()),
              IsRtcOk());
  EXPECT_THAT(
      observer_.states_,
      ElementsAre(  // FakeDtlsTransport doesn't signal the "connecting" state.
                    // TODO(hta): fix FakeDtlsTransport or file bug on it.
                    // DtlsTransportState::kConnecting,
          DtlsTransportState::kConnected));
}

TEST_F(DtlsTransportTest, CloseWhenClearing) {
  CreateTransport();
  CompleteDtlsHandshake();
  ASSERT_THAT(WaitUntil([&] { return observer_.state(); },
                        ::testing::Eq(DtlsTransportState::kConnected)),
              IsRtcOk());
  transport()->Clear(internal_transport_.get());
  ASSERT_THAT(WaitUntil([&] { return observer_.state(); },
                        ::testing::Eq(DtlsTransportState::kClosed)),
              IsRtcOk());
}

TEST_F(DtlsTransportTest, RoleAppearsOnConnect) {
  FakeSSLCertificate fake_certificate("fake data");
  CreateTransport(&fake_certificate);
  EXPECT_FALSE(transport()->Information().role());
  CompleteDtlsHandshake();
  ASSERT_THAT(WaitUntil([&] { return observer_.state(); },
                        ::testing::Eq(DtlsTransportState::kConnected)),
              IsRtcOk());
  EXPECT_TRUE(observer_.info_.role());
  EXPECT_TRUE(transport()->Information().role());
  EXPECT_EQ(transport()->Information().role(), DtlsTransportTlsRole::kClient);
}

TEST_F(DtlsTransportTest, CertificateAppearsOnConnect) {
  FakeSSLCertificate fake_certificate("fake data");
  CreateTransport(&fake_certificate);
  CompleteDtlsHandshake();
  ASSERT_THAT(WaitUntil([&] { return observer_.state(); },
                        ::testing::Eq(DtlsTransportState::kConnected)),
              IsRtcOk());
  EXPECT_TRUE(observer_.info_.remote_ssl_certificates() != nullptr);
}

TEST_F(DtlsTransportTest, CertificateDisappearsOnClose) {
  FakeSSLCertificate fake_certificate("fake data");
  CreateTransport(&fake_certificate);
  CompleteDtlsHandshake();
  ASSERT_THAT(WaitUntil([&] { return observer_.state(); },
                        ::testing::Eq(DtlsTransportState::kConnected)),
              IsRtcOk());
  EXPECT_TRUE(observer_.info_.remote_ssl_certificates() != nullptr);
  transport()->Clear(internal_transport_.get());
  ASSERT_THAT(WaitUntil([&] { return observer_.state(); },
                        ::testing::Eq(DtlsTransportState::kClosed)),
              IsRtcOk());
  EXPECT_FALSE(observer_.info_.remote_ssl_certificates());
}

TEST_F(DtlsTransportTest, CipherSuiteVisibleWhenConnected) {
  CreateTransport();
  CompleteDtlsHandshake();
  ASSERT_THAT(WaitUntil([&] { return observer_.state(); },
                        ::testing::Eq(DtlsTransportState::kConnected)),
              IsRtcOk());
  ASSERT_TRUE(observer_.info_.ssl_cipher_suite());
  EXPECT_EQ(kNonsenseCipherSuite, *observer_.info_.ssl_cipher_suite());
  transport()->Clear(internal_transport_.get());
  ASSERT_THAT(WaitUntil([&] { return observer_.state(); },
                        ::testing::Eq(DtlsTransportState::kClosed)),
              IsRtcOk());
  EXPECT_FALSE(observer_.info_.ssl_cipher_suite());
}

}  // namespace webrtc
