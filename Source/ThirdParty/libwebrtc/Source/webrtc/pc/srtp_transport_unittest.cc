/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/srtp_transport.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "api/field_trials.h"
#include "call/rtp_demuxer.h"
#include "media/base/fake_rtp.h"
#include "p2p/dtls/dtls_transport_internal.h"
#include "p2p/test/fake_packet_transport.h"
#include "pc/test/rtp_transport_test_util.h"
#include "pc/test/srtp_test_util.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/buffer.h"
#include "rtc_base/byte_order.h"
#include "rtc_base/checks.h"
#include "rtc_base/containers/flat_set.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/thread.h"
#include "test/create_test_field_trials.h"
#include "test/gtest.h"

using ::webrtc::kSrtpAeadAes128Gcm;
using ::webrtc::kTestKey1;
using ::webrtc::kTestKey2;

namespace webrtc {
// 128 bits key + 96 bits salt.
static const ZeroOnFreeBuffer<uint8_t> kTestKeyGcm128_1{
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ12", 28};
static const ZeroOnFreeBuffer<uint8_t> kTestKeyGcm128_2{
    "21ZYXWVUTSRQPONMLKJIHGFEDCBA", 28};
// 256 bits key + 96 bits salt.
static const ZeroOnFreeBuffer<uint8_t> kTestKeyGcm256_1{
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqr", 44};
static const ZeroOnFreeBuffer<uint8_t> kTestKeyGcm256_2{
    "rqponmlkjihgfedcbaZYXWVUTSRQPONMLKJIHGFEDCBA", 44};

class SrtpTransportTest : public ::testing::Test {
 protected:
  SrtpTransportTest() {
    bool rtcp_mux_enabled = true;

    rtp_packet_transport1_ =
        std::make_unique<FakePacketTransport>("fake_packet_transport1");
    rtp_packet_transport2_ =
        std::make_unique<FakePacketTransport>("fake_packet_transport2");

    bool asymmetric = false;
    rtp_packet_transport1_->SetDestination(rtp_packet_transport2_.get(),
                                           asymmetric);

    srtp_transport1_ =
        std::make_unique<SrtpTransport>(rtcp_mux_enabled, field_trials_);
    srtp_transport2_ =
        std::make_unique<SrtpTransport>(rtcp_mux_enabled, field_trials_);

    srtp_transport1_->SetRtpPacketTransport(rtp_packet_transport1_.get());
    srtp_transport2_->SetRtpPacketTransport(rtp_packet_transport2_.get());

    srtp_transport1_->SubscribeRtcpPacketReceived(
        &rtp_sink1_, [this](CopyOnWriteBuffer* buffer, int64_t packet_time_ms) {
          rtp_sink1_.OnRtcpPacketReceived(buffer, packet_time_ms);
        });
    srtp_transport2_->SubscribeRtcpPacketReceived(
        &rtp_sink2_, [this](CopyOnWriteBuffer* buffer, int64_t packet_time_ms) {
          rtp_sink2_.OnRtcpPacketReceived(buffer, packet_time_ms);
        });

    RtpDemuxerCriteria demuxer_criteria;
    // 0x00 is the payload type used in kPcmuFrame.
    demuxer_criteria.payload_types().insert(0x00);

    srtp_transport1_->RegisterRtpDemuxerSink(demuxer_criteria, &rtp_sink1_);
    srtp_transport2_->RegisterRtpDemuxerSink(demuxer_criteria, &rtp_sink2_);
  }

  ~SrtpTransportTest() override {
    if (srtp_transport1_) {
      srtp_transport1_->UnregisterRtpDemuxerSink(&rtp_sink1_);
    }
    if (srtp_transport2_) {
      srtp_transport2_->UnregisterRtpDemuxerSink(&rtp_sink2_);
    }
  }

  // With external auth enabled, SRTP doesn't write the auth tag and
  // unprotect would fail. Check accessing the information about the
  // tag instead, similar to what the actual code would do that relies
  // on external auth.
  void TestRtpAuthParams(SrtpTransport* transport, int crypto_suite) {
    int overhead;
    EXPECT_TRUE(transport->GetSrtpOverhead(&overhead));
    switch (crypto_suite) {
      case kSrtpAes128CmSha1_32:
        EXPECT_EQ(32 / 8, overhead);  // 32-bit tag.
        break;
      case kSrtpAes128CmSha1_80:
        EXPECT_EQ(80 / 8, overhead);  // 80-bit tag.
        break;
      default:
        RTC_DCHECK_NOTREACHED();
        break;
    }

    uint8_t* auth_key = nullptr;
    int key_len = 0;
    int tag_len = 0;
    EXPECT_TRUE(transport->GetRtpAuthParams(&auth_key, &key_len, &tag_len));
    EXPECT_NE(nullptr, auth_key);
    EXPECT_EQ(160 / 8, key_len);  // Length of SHA-1 is 160 bits.
    EXPECT_EQ(overhead, tag_len);
  }

  void TestSendRecvRtpPacket(int crypto_suite) {
    size_t rtp_len = sizeof(kPcmuFrame);
    size_t packet_size = rtp_len + rtp_auth_tag_len(crypto_suite);
    Buffer rtp_packet_buffer(packet_size);
    char* rtp_packet_data = rtp_packet_buffer.data<char>();
    memcpy(rtp_packet_data, kPcmuFrame, rtp_len);
    // In order to be able to run this test function multiple times we can not
    // use the same sequence number twice. Increase the sequence number by one.
    SetBE16(reinterpret_cast<uint8_t*>(rtp_packet_data) + 2,
            ++sequence_number_);
    CopyOnWriteBuffer rtp_packet1to2(rtp_packet_data, rtp_len, packet_size);
    CopyOnWriteBuffer rtp_packet2to1(rtp_packet_data, rtp_len, packet_size);

    char original_rtp_data[sizeof(kPcmuFrame)];
    memcpy(original_rtp_data, rtp_packet_data, rtp_len);

    AsyncSocketPacketOptions options;
    // Send a packet from `srtp_transport1_` to `srtp_transport2_` and verify
    // that the packet can be successfully received and decrypted.
    ASSERT_TRUE(srtp_transport1_->SendRtpPacket(&rtp_packet1to2, options,
                                                PF_SRTP_BYPASS));
    if (srtp_transport1_->IsExternalAuthActive()) {
      TestRtpAuthParams(srtp_transport1_.get(), crypto_suite);
    } else {
      ASSERT_TRUE(rtp_sink2_.last_recv_rtp_packet().data());
      EXPECT_EQ(0, memcmp(rtp_sink2_.last_recv_rtp_packet().data(),
                          original_rtp_data, rtp_len));
      // Get the encrypted packet from underneath packet transport and verify
      // the data is actually encrypted.
      auto fake_rtp_packet_transport = static_cast<FakePacketTransport*>(
          srtp_transport1_->rtp_packet_transport());
      EXPECT_NE(0, memcmp(fake_rtp_packet_transport->last_sent_packet()->data(),
                          original_rtp_data, rtp_len));
    }

    // Do the same thing in the opposite direction;
    ASSERT_TRUE(srtp_transport2_->SendRtpPacket(&rtp_packet2to1, options,
                                                PF_SRTP_BYPASS));
    if (srtp_transport2_->IsExternalAuthActive()) {
      TestRtpAuthParams(srtp_transport2_.get(), crypto_suite);
    } else {
      ASSERT_TRUE(rtp_sink1_.last_recv_rtp_packet().data());
      EXPECT_EQ(0, memcmp(rtp_sink1_.last_recv_rtp_packet().data(),
                          original_rtp_data, rtp_len));
      auto fake_rtp_packet_transport = static_cast<FakePacketTransport*>(
          srtp_transport2_->rtp_packet_transport());
      EXPECT_NE(0, memcmp(fake_rtp_packet_transport->last_sent_packet()->data(),
                          original_rtp_data, rtp_len));
    }
  }

  void TestSendRecvRtcpPacket(int crypto_suite) {
    size_t rtcp_len = sizeof(::kRtcpReport);
    size_t packet_size = rtcp_len + 4 + rtcp_auth_tag_len(crypto_suite);
    Buffer rtcp_packet_buffer(packet_size);
    char* rtcp_packet_data = rtcp_packet_buffer.data<char>();
    memcpy(rtcp_packet_data, ::kRtcpReport, rtcp_len);

    CopyOnWriteBuffer rtcp_packet1to2(rtcp_packet_data, rtcp_len, packet_size);
    CopyOnWriteBuffer rtcp_packet2to1(rtcp_packet_data, rtcp_len, packet_size);

    AsyncSocketPacketOptions options;
    // Send a packet from `srtp_transport1_` to `srtp_transport2_` and verify
    // that the packet can be successfully received and decrypted.
    ASSERT_TRUE(srtp_transport1_->SendRtcpPacket(&rtcp_packet1to2, options,
                                                 PF_SRTP_BYPASS));
    ASSERT_TRUE(rtp_sink2_.last_recv_rtcp_packet().data());
    EXPECT_EQ(0, memcmp(rtp_sink2_.last_recv_rtcp_packet().data(),
                        rtcp_packet_data, rtcp_len));
    // Get the encrypted packet from underneath packet transport and verify the
    // data is actually encrypted.
    auto fake_rtp_packet_transport = static_cast<FakePacketTransport*>(
        srtp_transport1_->rtp_packet_transport());
    EXPECT_NE(0, memcmp(fake_rtp_packet_transport->last_sent_packet()->data(),
                        rtcp_packet_data, rtcp_len));

    // Do the same thing in the opposite direction;
    ASSERT_TRUE(srtp_transport2_->SendRtcpPacket(&rtcp_packet2to1, options,
                                                 PF_SRTP_BYPASS));
    ASSERT_TRUE(rtp_sink1_.last_recv_rtcp_packet().data());
    EXPECT_EQ(0, memcmp(rtp_sink1_.last_recv_rtcp_packet().data(),
                        rtcp_packet_data, rtcp_len));
    fake_rtp_packet_transport = static_cast<FakePacketTransport*>(
        srtp_transport2_->rtp_packet_transport());
    EXPECT_NE(0, memcmp(fake_rtp_packet_transport->last_sent_packet()->data(),
                        rtcp_packet_data, rtcp_len));
  }

  void TestSendRecvPacket(bool enable_external_auth,
                          int crypto_suite,
                          const ZeroOnFreeBuffer<uint8_t>& key1,
                          const ZeroOnFreeBuffer<uint8_t>& key2) {
    EXPECT_EQ(key1.size(), key2.size());
    if (enable_external_auth) {
      srtp_transport1_->EnableExternalAuth();
      srtp_transport2_->EnableExternalAuth();
    }
    std::vector<int> extension_ids;
    EXPECT_TRUE(srtp_transport1_->SetRtpParams(
        crypto_suite, key1, extension_ids, crypto_suite, key2, extension_ids));
    EXPECT_TRUE(srtp_transport2_->SetRtpParams(
        crypto_suite, key2, extension_ids, crypto_suite, key1, extension_ids));
    EXPECT_TRUE(srtp_transport1_->SetRtcpParams(
        crypto_suite, key1, extension_ids, crypto_suite, key2, extension_ids));
    EXPECT_TRUE(srtp_transport2_->SetRtcpParams(
        crypto_suite, key2, extension_ids, crypto_suite, key1, extension_ids));
    EXPECT_TRUE(srtp_transport1_->IsSrtpActive());
    EXPECT_TRUE(srtp_transport2_->IsSrtpActive());
    if (IsGcmCryptoSuite(crypto_suite)) {
      EXPECT_FALSE(srtp_transport1_->IsExternalAuthActive());
      EXPECT_FALSE(srtp_transport2_->IsExternalAuthActive());
    } else if (enable_external_auth) {
      EXPECT_TRUE(srtp_transport1_->IsExternalAuthActive());
      EXPECT_TRUE(srtp_transport2_->IsExternalAuthActive());
    }
    TestSendRecvRtpPacket(crypto_suite);
    TestSendRecvRtcpPacket(crypto_suite);
  }

  void TestSendRecvPacketWithEncryptedHeaderExtension(
      int crypto_suite,
      const std::vector<int>& encrypted_header_ids) {
    size_t rtp_len = sizeof(kPcmuFrameWithExtensions);
    size_t packet_size = rtp_len + rtp_auth_tag_len(crypto_suite);
    Buffer rtp_packet_buffer(packet_size);
    char* rtp_packet_data = rtp_packet_buffer.data<char>();
    memcpy(rtp_packet_data, kPcmuFrameWithExtensions, rtp_len);
    // In order to be able to run this test function multiple times we can not
    // use the same sequence number twice. Increase the sequence number by one.
    SetBE16(reinterpret_cast<uint8_t*>(rtp_packet_data) + 2,
            ++sequence_number_);
    CopyOnWriteBuffer rtp_packet1to2(rtp_packet_data, rtp_len, packet_size);
    CopyOnWriteBuffer rtp_packet2to1(rtp_packet_data, rtp_len, packet_size);

    char original_rtp_data[sizeof(kPcmuFrameWithExtensions)];
    memcpy(original_rtp_data, rtp_packet_data, rtp_len);

    AsyncSocketPacketOptions options;
    // Send a packet from `srtp_transport1_` to `srtp_transport2_` and verify
    // that the packet can be successfully received and decrypted.
    ASSERT_TRUE(srtp_transport1_->SendRtpPacket(&rtp_packet1to2, options,
                                                PF_SRTP_BYPASS));
    ASSERT_TRUE(rtp_sink2_.last_recv_rtp_packet().data());
    EXPECT_EQ(0, memcmp(rtp_sink2_.last_recv_rtp_packet().data(),
                        original_rtp_data, rtp_len));
    // Get the encrypted packet from underneath packet transport and verify the
    // data and header extension are actually encrypted.
    auto fake_rtp_packet_transport = static_cast<FakePacketTransport*>(
        srtp_transport1_->rtp_packet_transport());
    EXPECT_NE(0, memcmp(fake_rtp_packet_transport->last_sent_packet()->data(),
                        original_rtp_data, rtp_len));
    CompareHeaderExtensions(
        reinterpret_cast<const char*>(
            fake_rtp_packet_transport->last_sent_packet()->data()),
        fake_rtp_packet_transport->last_sent_packet()->size(),
        original_rtp_data, rtp_len, encrypted_header_ids, false);

    // Do the same thing in the opposite direction;
    ASSERT_TRUE(srtp_transport2_->SendRtpPacket(&rtp_packet2to1, options,
                                                PF_SRTP_BYPASS));
    ASSERT_TRUE(rtp_sink1_.last_recv_rtp_packet().data());
    EXPECT_EQ(0, memcmp(rtp_sink1_.last_recv_rtp_packet().data(),
                        original_rtp_data, rtp_len));
    fake_rtp_packet_transport = static_cast<FakePacketTransport*>(
        srtp_transport2_->rtp_packet_transport());
    EXPECT_NE(0, memcmp(fake_rtp_packet_transport->last_sent_packet()->data(),
                        original_rtp_data, rtp_len));
    CompareHeaderExtensions(
        reinterpret_cast<const char*>(
            fake_rtp_packet_transport->last_sent_packet()->data()),
        fake_rtp_packet_transport->last_sent_packet()->size(),
        original_rtp_data, rtp_len, encrypted_header_ids, false);
  }

  void TestSendRecvEncryptedHeaderExtension(
      int crypto_suite,
      const ZeroOnFreeBuffer<uint8_t>& key1,
      const ZeroOnFreeBuffer<uint8_t>& key2) {
    std::vector<int> encrypted_headers;
    encrypted_headers.push_back(kHeaderExtensionIDs[0]);
    // Don't encrypt header ids 2 and 3.
    encrypted_headers.push_back(kHeaderExtensionIDs[1]);
    EXPECT_EQ(key1.size(), key2.size());
    EXPECT_TRUE(srtp_transport1_->SetRtpParams(crypto_suite, key1,
                                               encrypted_headers, crypto_suite,
                                               key2, encrypted_headers));
    EXPECT_TRUE(srtp_transport2_->SetRtpParams(crypto_suite, key2,
                                               encrypted_headers, crypto_suite,
                                               key1, encrypted_headers));
    EXPECT_TRUE(srtp_transport1_->IsSrtpActive());
    EXPECT_TRUE(srtp_transport2_->IsSrtpActive());
    EXPECT_FALSE(srtp_transport1_->IsExternalAuthActive());
    EXPECT_FALSE(srtp_transport2_->IsExternalAuthActive());
    TestSendRecvPacketWithEncryptedHeaderExtension(crypto_suite,
                                                   encrypted_headers);
  }
  AutoThread main_thread;

  std::unique_ptr<SrtpTransport> srtp_transport1_;
  std::unique_ptr<SrtpTransport> srtp_transport2_;

  std::unique_ptr<FakePacketTransport> rtp_packet_transport1_;
  std::unique_ptr<FakePacketTransport> rtp_packet_transport2_;

  TransportObserver rtp_sink1_;
  TransportObserver rtp_sink2_;

  int sequence_number_ = 0;
  FieldTrials field_trials_ = CreateTestFieldTrials();
};

class SrtpTransportTestWithExternalAuth
    : public SrtpTransportTest,
      public ::testing::WithParamInterface<bool> {};

TEST_P(SrtpTransportTestWithExternalAuth,
       SendAndRecvPacket_AES_CM_128_HMAC_SHA1_80) {
  bool enable_external_auth = GetParam();
  TestSendRecvPacket(enable_external_auth, kSrtpAes128CmSha1_80, kTestKey1,
                     kTestKey2);
}

TEST_F(SrtpTransportTest,
       SendAndRecvPacketWithHeaderExtension_AES_CM_128_HMAC_SHA1_80) {
  TestSendRecvEncryptedHeaderExtension(kSrtpAes128CmSha1_80, kTestKey1,
                                       kTestKey2);
}

TEST_P(SrtpTransportTestWithExternalAuth,
       SendAndRecvPacket_AES_CM_128_HMAC_SHA1_32) {
  bool enable_external_auth = GetParam();
  TestSendRecvPacket(enable_external_auth, kSrtpAes128CmSha1_32, kTestKey1,
                     kTestKey2);
}

TEST_F(SrtpTransportTest,
       SendAndRecvPacketWithHeaderExtension_AES_CM_128_HMAC_SHA1_32) {
  TestSendRecvEncryptedHeaderExtension(kSrtpAes128CmSha1_32, kTestKey1,
                                       kTestKey2);
}

TEST_P(SrtpTransportTestWithExternalAuth,
       SendAndRecvPacket_kSrtpAeadAes128Gcm) {
  bool enable_external_auth = GetParam();
  TestSendRecvPacket(enable_external_auth, kSrtpAeadAes128Gcm, kTestKeyGcm128_1,
                     kTestKeyGcm128_2);
}

TEST_F(SrtpTransportTest,
       SendAndRecvPacketWithHeaderExtension_kSrtpAeadAes128Gcm) {
  TestSendRecvEncryptedHeaderExtension(kSrtpAeadAes128Gcm, kTestKeyGcm128_1,
                                       kTestKeyGcm128_2);
}

TEST_P(SrtpTransportTestWithExternalAuth,
       SendAndRecvPacket_kSrtpAeadAes256Gcm) {
  bool enable_external_auth = GetParam();
  TestSendRecvPacket(enable_external_auth, kSrtpAeadAes256Gcm, kTestKeyGcm256_1,
                     kTestKeyGcm256_2);
}

TEST_F(SrtpTransportTest,
       SendAndRecvPacketWithHeaderExtension_kSrtpAeadAes256Gcm) {
  TestSendRecvEncryptedHeaderExtension(kSrtpAeadAes256Gcm, kTestKeyGcm256_1,
                                       kTestKeyGcm256_2);
}

// Run all tests both with and without external auth enabled.
INSTANTIATE_TEST_SUITE_P(ExternalAuth,
                         SrtpTransportTestWithExternalAuth,
                         ::testing::Values(true, false));

// Test directly setting the params with bogus keys.
TEST_F(SrtpTransportTest, TestSetParamsKeyTooShort) {
  std::vector<int> extension_ids;
  EXPECT_FALSE(srtp_transport1_->SetRtpParams(
      kSrtpAes128CmSha1_80,
      ZeroOnFreeBuffer<uint8_t>(kTestKey1.data(), kTestKey1.size() - 1),
      extension_ids, kSrtpAes128CmSha1_80,
      ZeroOnFreeBuffer<uint8_t>(kTestKey1.data(), kTestKey1.size() - 1),
      extension_ids));
  EXPECT_FALSE(srtp_transport1_->SetRtcpParams(
      kSrtpAes128CmSha1_80,
      ZeroOnFreeBuffer<uint8_t>(kTestKey1.data(), kTestKey1.size() - 1),
      extension_ids, kSrtpAes128CmSha1_80,
      ZeroOnFreeBuffer<uint8_t>(kTestKey1.data(), kTestKey1.size() - 1),
      extension_ids));
}

TEST_F(SrtpTransportTest, RemoveSrtpReceiveStream) {
  FieldTrials field_trials =
      CreateTestFieldTrials("WebRTC-SrtpRemoveReceiveStream/Enabled/");
  auto srtp_transport =
      std::make_unique<SrtpTransport>(/*rtcp_mux_enabled=*/true, field_trials);
  auto rtp_packet_transport =
      std::make_unique<FakePacketTransport>("fake_packet_transport_loopback");

  bool asymmetric = false;
  rtp_packet_transport->SetDestination(rtp_packet_transport.get(), asymmetric);
  srtp_transport->SetRtpPacketTransport(rtp_packet_transport.get());

  TransportObserver rtp_sink;

  std::vector<int> extension_ids;
  EXPECT_TRUE(srtp_transport->SetRtpParams(kSrtpAeadAes128Gcm, kTestKeyGcm128_1,
                                           extension_ids, kSrtpAeadAes128Gcm,
                                           kTestKeyGcm128_1, extension_ids));

  RtpDemuxerCriteria demuxer_criteria;
  uint32_t ssrc = 0x1;  // SSRC of kPcmuFrame
  demuxer_criteria.ssrcs().insert(ssrc);
  EXPECT_TRUE(
      srtp_transport->RegisterRtpDemuxerSink(demuxer_criteria, &rtp_sink));

  // Create a packet and try to send it three times.
  size_t rtp_len = sizeof(kPcmuFrame);
  size_t packet_size = rtp_len + rtp_auth_tag_len(kSrtpAeadAes128Gcm);
  Buffer rtp_packet_buffer(packet_size);
  char* rtp_packet_data = rtp_packet_buffer.data<char>();
  memcpy(rtp_packet_data, kPcmuFrame, rtp_len);

  // First attempt will succeed.
  CopyOnWriteBuffer first_try(rtp_packet_data, rtp_len, packet_size);
  EXPECT_TRUE(srtp_transport->SendRtpPacket(
      &first_try, AsyncSocketPacketOptions(), PF_SRTP_BYPASS));
  EXPECT_EQ(rtp_sink.rtp_count(), 1);

  // Second attempt will be rejected by libSRTP as a replay attack
  // (srtp_err_status_replay_fail) since the sequence number was already seen.
  // Hence the packet never reaches the sink.
  CopyOnWriteBuffer second_try(rtp_packet_data, rtp_len, packet_size);
  EXPECT_TRUE(srtp_transport->SendRtpPacket(
      &second_try, AsyncSocketPacketOptions(), PF_SRTP_BYPASS));
  EXPECT_EQ(rtp_sink.rtp_count(), 1);

  // Reset the sink.
  EXPECT_TRUE(srtp_transport->UnregisterRtpDemuxerSink(&rtp_sink));
  EXPECT_TRUE(
      srtp_transport->RegisterRtpDemuxerSink(demuxer_criteria, &rtp_sink));

  // Third attempt will succeed again since libSRTP does not remember seeing
  // the sequence number after the reset.
  CopyOnWriteBuffer third_try(rtp_packet_data, rtp_len, packet_size);
  EXPECT_TRUE(srtp_transport->SendRtpPacket(
      &third_try, AsyncSocketPacketOptions(), PF_SRTP_BYPASS));
  EXPECT_EQ(rtp_sink.rtp_count(), 2);
  // Clear the sink to clean up.
  srtp_transport->UnregisterRtpDemuxerSink(&rtp_sink);
}

}  // namespace webrtc
