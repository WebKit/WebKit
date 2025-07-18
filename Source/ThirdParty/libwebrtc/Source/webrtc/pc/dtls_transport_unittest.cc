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
#include <utility>
#include <vector>

#include "api/dtls_transport_interface.h"
#include "api/make_ref_counted.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/test/rtc_error_matchers.h"
#include "p2p/base/p2p_constants.h"
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
  DtlsTransportObserverInterface* observer() { return &observer_; }

  void CreateTransport(FakeSSLCertificate* certificate = nullptr) {
    auto cricket_transport = std::make_unique<FakeDtlsTransport>(
        "audio", ICE_CANDIDATE_COMPONENT_RTP);
    if (certificate) {
      cricket_transport->SetRemoteSSLCertificate(certificate);
    }
    cricket_transport->SetSslCipherSuite(kNonsenseCipherSuite);
    transport_ = make_ref_counted<DtlsTransport>(std::move(cricket_transport));
  }

  void CompleteDtlsHandshake() {
    auto fake_dtls1 = static_cast<FakeDtlsTransport*>(transport_->internal());
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
  TestDtlsTransportObserver observer_;
};

TEST_F(DtlsTransportTest, CreateClearDelete) {
  auto cricket_transport =
      std::make_unique<FakeDtlsTransport>("audio", ICE_CANDIDATE_COMPONENT_RTP);
  auto webrtc_transport =
      make_ref_counted<DtlsTransport>(std::move(cricket_transport));
  ASSERT_TRUE(webrtc_transport->internal());
  ASSERT_EQ(DtlsTransportState::kNew, webrtc_transport->Information().state());
  webrtc_transport->Clear();
  ASSERT_FALSE(webrtc_transport->internal());
  ASSERT_EQ(DtlsTransportState::kClosed,
            webrtc_transport->Information().state());
}

TEST_F(DtlsTransportTest, EventsObservedWhenConnecting) {
  CreateTransport();
  transport()->RegisterObserver(observer());
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
  transport()->RegisterObserver(observer());
  CompleteDtlsHandshake();
  ASSERT_THAT(WaitUntil([&] { return observer_.state(); },
                        ::testing::Eq(DtlsTransportState::kConnected)),
              IsRtcOk());
  transport()->Clear();
  ASSERT_THAT(WaitUntil([&] { return observer_.state(); },
                        ::testing::Eq(DtlsTransportState::kClosed)),
              IsRtcOk());
}

TEST_F(DtlsTransportTest, RoleAppearsOnConnect) {
  FakeSSLCertificate fake_certificate("fake data");
  CreateTransport(&fake_certificate);
  transport()->RegisterObserver(observer());
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
  transport()->RegisterObserver(observer());
  CompleteDtlsHandshake();
  ASSERT_THAT(WaitUntil([&] { return observer_.state(); },
                        ::testing::Eq(DtlsTransportState::kConnected)),
              IsRtcOk());
  EXPECT_TRUE(observer_.info_.remote_ssl_certificates() != nullptr);
}

TEST_F(DtlsTransportTest, CertificateDisappearsOnClose) {
  FakeSSLCertificate fake_certificate("fake data");
  CreateTransport(&fake_certificate);
  transport()->RegisterObserver(observer());
  CompleteDtlsHandshake();
  ASSERT_THAT(WaitUntil([&] { return observer_.state(); },
                        ::testing::Eq(DtlsTransportState::kConnected)),
              IsRtcOk());
  EXPECT_TRUE(observer_.info_.remote_ssl_certificates() != nullptr);
  transport()->Clear();
  ASSERT_THAT(WaitUntil([&] { return observer_.state(); },
                        ::testing::Eq(DtlsTransportState::kClosed)),
              IsRtcOk());
  EXPECT_FALSE(observer_.info_.remote_ssl_certificates());
}

TEST_F(DtlsTransportTest, CipherSuiteVisibleWhenConnected) {
  CreateTransport();
  transport()->RegisterObserver(observer());
  CompleteDtlsHandshake();
  ASSERT_THAT(WaitUntil([&] { return observer_.state(); },
                        ::testing::Eq(DtlsTransportState::kConnected)),
              IsRtcOk());
  ASSERT_TRUE(observer_.info_.ssl_cipher_suite());
  EXPECT_EQ(kNonsenseCipherSuite, *observer_.info_.ssl_cipher_suite());
  transport()->Clear();
  ASSERT_THAT(WaitUntil([&] { return observer_.state(); },
                        ::testing::Eq(DtlsTransportState::kClosed)),
              IsRtcOk());
  EXPECT_FALSE(observer_.info_.ssl_cipher_suite());
}

}  // namespace webrtc
