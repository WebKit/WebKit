/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <memory>
#include <set>
#include <string>

#include "rtc_base/bufferqueue.h"
#include "rtc_base/checks.h"
#include "rtc_base/gunit.h"
#include "rtc_base/helpers.h"
#include "rtc_base/ssladapter.h"
#include "rtc_base/sslidentity.h"
#include "rtc_base/sslstreamadapter.h"
#include "rtc_base/stream.h"

using ::testing::WithParamInterface;
using ::testing::Values;
using ::testing::Combine;
using ::testing::tuple;

static const int kBlockSize = 4096;
static const char kExporterLabel[] = "label";
static const unsigned char kExporterContext[] = "context";
static int kExporterContextLen = sizeof(kExporterContext);

static const char kRSA_PRIVATE_KEY_PEM[] =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "MIICdwIBADANBgkqhkiG9w0BAQEFAASCAmEwggJdAgEAAoGBAMYRkbhmI7kVA/rM\n"
    "czsZ+6JDhDvnkF+vn6yCAGuRPV03zuRqZtDy4N4to7PZu9PjqrRl7nDMXrG3YG9y\n"
    "rlIAZ72KjcKKFAJxQyAKLCIdawKRyp8RdK3LEySWEZb0AV58IadqPZDTNHHRX8dz\n"
    "5aTSMsbbkZ+C/OzTnbiMqLL/vg6jAgMBAAECgYAvgOs4FJcgvp+TuREx7YtiYVsH\n"
    "mwQPTum2z/8VzWGwR8BBHBvIpVe1MbD/Y4seyI2aco/7UaisatSgJhsU46/9Y4fq\n"
    "2TwXH9QANf4at4d9n/R6rzwpAJOpgwZgKvdQjkfrKTtgLV+/dawvpxUYkRH4JZM1\n"
    "CVGukMfKNrSVH4Ap4QJBAOJmGV1ASPnB4r4nc99at7JuIJmd7fmuVUwUgYi4XgaR\n"
    "WhScBsgYwZ/JoywdyZJgnbcrTDuVcWG56B3vXbhdpMsCQQDf9zeJrjnPZ3Cqm79y\n"
    "kdqANep0uwZciiNiWxsQrCHztywOvbFhdp8iYVFG9EK8DMY41Y5TxUwsHD+67zao\n"
    "ZNqJAkEA1suLUP/GvL8IwuRneQd2tWDqqRQ/Td3qq03hP7e77XtF/buya3Ghclo5\n"
    "54czUR89QyVfJEC6278nzA7n2h1uVQJAcG6mztNL6ja/dKZjYZye2CY44QjSlLo0\n"
    "MTgTSjdfg/28fFn2Jjtqf9Pi/X+50LWI/RcYMC2no606wRk9kyOuIQJBAK6VSAim\n"
    "1pOEjsYQn0X5KEIrz1G3bfCbB848Ime3U2/FWlCHMr6ch8kCZ5d1WUeJD3LbwMNG\n"
    "UCXiYxSsu20QNVw=\n"
    "-----END RSA PRIVATE KEY-----\n";

static const char kCERT_PEM[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBmTCCAQKgAwIBAgIEbzBSAjANBgkqhkiG9w0BAQsFADARMQ8wDQYDVQQDEwZX\n"
    "ZWJSVEMwHhcNMTQwMTAyMTgyNDQ3WhcNMTQwMjAxMTgyNDQ3WjARMQ8wDQYDVQQD\n"
    "EwZXZWJSVEMwgZ8wDQYJKoZIhvcNAQEBBQADgY0AMIGJAoGBAMYRkbhmI7kVA/rM\n"
    "czsZ+6JDhDvnkF+vn6yCAGuRPV03zuRqZtDy4N4to7PZu9PjqrRl7nDMXrG3YG9y\n"
    "rlIAZ72KjcKKFAJxQyAKLCIdawKRyp8RdK3LEySWEZb0AV58IadqPZDTNHHRX8dz\n"
    "5aTSMsbbkZ+C/OzTnbiMqLL/vg6jAgMBAAEwDQYJKoZIhvcNAQELBQADgYEAUflI\n"
    "VUe5Krqf5RVa5C3u/UTAOAUJBiDS3VANTCLBxjuMsvqOG0WvaYWP3HYPgrz0jXK2\n"
    "LJE/mGw3MyFHEqi81jh95J+ypl6xKW6Rm8jKLR87gUvCaVYn/Z4/P3AqcQTB7wOv\n"
    "UD0A8qfhfDM+LK6rPAnCsVN0NRDY3jvd6rzix9M=\n"
    "-----END CERTIFICATE-----\n";

static const char kIntCert1[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIEUjCCAjqgAwIBAgIBAjANBgkqhkiG9w0BAQsFADCBljELMAkGA1UEBhMCVVMx\n"
    "EzARBgNVBAgMCkNhbGlmb3JuaWExFjAUBgNVBAcMDU1vdW50YWluIFZpZXcxFDAS\n"
    "BgNVBAoMC0dvb2dsZSwgSW5jMQwwCgYDVQQLDANHVFAxFzAVBgNVBAMMDnRlbGVw\n"
    "aG9ueS5nb29nMR0wGwYJKoZIhvcNAQkBFg5ndHBAZ29vZ2xlLmNvbTAeFw0xNzA5\n"
    "MjYwNDA5MDNaFw0yMDA2MjIwNDA5MDNaMGQxCzAJBgNVBAYTAlVTMQswCQYDVQQI\n"
    "DAJDQTEWMBQGA1UEBwwNTW91bnRhaW4gVmlldzEXMBUGA1UECgwOdGVsZXBob255\n"
    "Lmdvb2cxFzAVBgNVBAMMDnRlbGVwaG9ueS5nb29nMIGfMA0GCSqGSIb3DQEBAQUA\n"
    "A4GNADCBiQKBgQDJXWeeU1v1+wlqkVobzI3aN7Uh2iVQA9YCdq5suuabtiD/qoOD\n"
    "NKpmQqsx7WZGGWSZTDFEBaUpvIK7Hb+nzRqk6iioPCFOFuarm6GxO1xVneImMuE6\n"
    "tuWb3YZPr+ikChJbl11y5UcSbg0QsbeUc+jHl5umNvrL85Y+z8SP0rxbBwIDAQAB\n"
    "o2AwXjAdBgNVHQ4EFgQU7tdZobqlN8R8V72FQnRxmqq8tKswHwYDVR0jBBgwFoAU\n"
    "5GgKMUtcxkQ2dJrtNR5YOlIAPDswDwYDVR0TAQH/BAUwAwEB/zALBgNVHQ8EBAMC\n"
    "AQYwDQYJKoZIhvcNAQELBQADggIBADObh9Z+z14FmP9zSenhFtq7hFnmNrSkklk8\n"
    "eyYWXKfOuIriEQQBZsz76ZcnzStih8Rj+yQ0AXydk4fJ5LOwC2cUqQBar17g6Pd2\n"
    "8g4SIL4azR9WvtiSvpuGlwp25b+yunaacDne6ebnf/MUiiKT5w61Xo3cEPVfl38e\n"
    "/Up2l0bioid5enUTmg6LY6RxDO6tnZQkz3XD+nNSwT4ehtkqFpHYWjErj0BbkDM2\n"
    "hiVc/JsYOZn3DmuOlHVHU6sKwqh3JEyvHO/d7DGzMGWHpHwv2mCTJq6l/sR95Tc2\n"
    "GaQZgGDVNs9pdEouJCDm9e/PbQWRYhnat82PTkXx/6mDAAwdZlIi/pACzq8K4p7e\n"
    "6hF0t8uKGnXJubHPXxlnJU6yxZ0yWmivAGjwWK4ur832gKlho4jeMDhiI/T3QPpl\n"
    "iMNsIvxRhdD+GxJkQP1ezayw8s+Uc9KwKglrkBSRRDLCJUfPOvMmXLUDSTMX7kp4\n"
    "/Ak1CA8dVLJIlfEjLBUuvAttlP7+7lsKNgxAjCxZkWLXIyGULzNPQwVWkGfCbrQs\n"
    "XyMvSbFsSIb7blV7eLlmf9a+2RprUUkc2ALXLLCI9YQXmxm2beBfMyNmmebwBJzT\n"
    "B0OR+5pFFNTJPoNlqpdrDsGrDu7JlUtk0ZLZzYyKXbgy2qXxfd4OWzXXjxpLMszZ\n"
    "LDIpOAkj\n"
    "-----END CERTIFICATE-----\n";

static const char kCACert[] =
    "-----BEGIN CERTIFICATE-----\n"
    "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\n"
    "dGaXBy+qbVXVyGXaeEbIRjtJ6m92\n"
    "-----END CERTIFICATE-----\n";

class SSLStreamAdapterTestBase;

class SSLDummyStreamBase : public rtc::StreamInterface,
                           public sigslot::has_slots<> {
 public:
  SSLDummyStreamBase(SSLStreamAdapterTestBase* test,
                     const std::string& side,
                     rtc::StreamInterface* in,
                     rtc::StreamInterface* out)
      : test_base_(test), side_(side), in_(in), out_(out), first_packet_(true) {
    in_->SignalEvent.connect(this, &SSLDummyStreamBase::OnEventIn);
    out_->SignalEvent.connect(this, &SSLDummyStreamBase::OnEventOut);
  }

  rtc::StreamState GetState() const override { return rtc::SS_OPEN; }

  rtc::StreamResult Read(void* buffer,
                         size_t buffer_len,
                         size_t* read,
                         int* error) override {
    rtc::StreamResult r;

    r = in_->Read(buffer, buffer_len, read, error);
    if (r == rtc::SR_BLOCK)
      return rtc::SR_BLOCK;
    if (r == rtc::SR_EOS)
      return rtc::SR_EOS;

    if (r != rtc::SR_SUCCESS) {
      ADD_FAILURE();
      return rtc::SR_ERROR;
    }

    return rtc::SR_SUCCESS;
  }

  // Catch readability events on in and pass them up.
  void OnEventIn(rtc::StreamInterface* stream, int sig, int err) {
    int mask = (rtc::SE_READ | rtc::SE_CLOSE);

    if (sig & mask) {
      RTC_LOG(LS_VERBOSE) << "SSLDummyStreamBase::OnEvent side=" << side_
                          << " sig=" << sig << " forwarding upward";
      PostEvent(sig & mask, 0);
    }
  }

  // Catch writeability events on out and pass them up.
  void OnEventOut(rtc::StreamInterface* stream, int sig, int err) {
    if (sig & rtc::SE_WRITE) {
      RTC_LOG(LS_VERBOSE) << "SSLDummyStreamBase::OnEvent side=" << side_
                          << " sig=" << sig << " forwarding upward";

      PostEvent(sig & rtc::SE_WRITE, 0);
    }
  }

  // Write to the outgoing FifoBuffer
  rtc::StreamResult WriteData(const void* data,
                              size_t data_len,
                              size_t* written,
                              int* error) {
    return out_->Write(data, data_len, written, error);
  }

  rtc::StreamResult Write(const void* data,
                          size_t data_len,
                          size_t* written,
                          int* error) override;

  void Close() override {
    RTC_LOG(LS_INFO) << "Closing outbound stream";
    out_->Close();
  }

 protected:
  SSLStreamAdapterTestBase* test_base_;
  const std::string side_;
  rtc::StreamInterface* in_;
  rtc::StreamInterface* out_;
  bool first_packet_;
};

class SSLDummyStreamTLS : public SSLDummyStreamBase {
 public:
  SSLDummyStreamTLS(SSLStreamAdapterTestBase* test,
                    const std::string& side,
                    rtc::FifoBuffer* in,
                    rtc::FifoBuffer* out)
      : SSLDummyStreamBase(test, side, in, out) {}
};

class BufferQueueStream : public rtc::BufferQueue, public rtc::StreamInterface {
 public:
  BufferQueueStream(size_t capacity, size_t default_size)
      : rtc::BufferQueue(capacity, default_size) {}

  // Implementation of abstract StreamInterface methods.

  // A buffer queue stream is always "open".
  rtc::StreamState GetState() const override { return rtc::SS_OPEN; }

  // Reading a buffer queue stream will either succeed or block.
  rtc::StreamResult Read(void* buffer,
                         size_t buffer_len,
                         size_t* read,
                         int* error) override {
    if (!ReadFront(buffer, buffer_len, read)) {
      return rtc::SR_BLOCK;
    }
    return rtc::SR_SUCCESS;
  }

  // Writing to a buffer queue stream will either succeed or block.
  rtc::StreamResult Write(const void* data,
                          size_t data_len,
                          size_t* written,
                          int* error) override {
    if (!WriteBack(data, data_len, written)) {
      return rtc::SR_BLOCK;
    }
    return rtc::SR_SUCCESS;
  }

  // A buffer queue stream can not be closed.
  void Close() override {}

 protected:
  void NotifyReadableForTest() override { PostEvent(rtc::SE_READ, 0); }

  void NotifyWritableForTest() override { PostEvent(rtc::SE_WRITE, 0); }
};

class SSLDummyStreamDTLS : public SSLDummyStreamBase {
 public:
  SSLDummyStreamDTLS(SSLStreamAdapterTestBase* test,
                     const std::string& side,
                     BufferQueueStream* in,
                     BufferQueueStream* out)
      : SSLDummyStreamBase(test, side, in, out) {}
};

static const int kFifoBufferSize = 4096;
static const int kBufferCapacity = 1;
static const size_t kDefaultBufferSize = 2048;

class SSLStreamAdapterTestBase : public testing::Test,
                                 public sigslot::has_slots<> {
 public:
  SSLStreamAdapterTestBase(
      const std::string& client_cert_pem,
      const std::string& client_private_key_pem,
      bool dtls,
      rtc::KeyParams client_key_type = rtc::KeyParams(rtc::KT_DEFAULT),
      rtc::KeyParams server_key_type = rtc::KeyParams(rtc::KT_DEFAULT))
      : client_cert_pem_(client_cert_pem),
        client_private_key_pem_(client_private_key_pem),
        client_key_type_(client_key_type),
        server_key_type_(server_key_type),
        client_stream_(nullptr),
        server_stream_(nullptr),
        client_identity_(nullptr),
        server_identity_(nullptr),
        delay_(0),
        mtu_(1460),
        loss_(0),
        lose_first_packet_(false),
        damage_(false),
        dtls_(dtls),
        handshake_wait_(5000),
        identities_set_(false) {
    // Set use of the test RNG to get predictable loss patterns.
    rtc::SetRandomTestMode(true);
  }

  ~SSLStreamAdapterTestBase() override {
    // Put it back for the next test.
    rtc::SetRandomTestMode(false);
  }

  void SetUp() override {
    CreateStreams();

    client_ssl_.reset(rtc::SSLStreamAdapter::Create(client_stream_));
    server_ssl_.reset(rtc::SSLStreamAdapter::Create(server_stream_));

    // Set up the slots
    client_ssl_->SignalEvent.connect(this, &SSLStreamAdapterTestBase::OnEvent);
    server_ssl_->SignalEvent.connect(this, &SSLStreamAdapterTestBase::OnEvent);

    if (!client_cert_pem_.empty() && !client_private_key_pem_.empty()) {
      client_identity_ = rtc::SSLIdentity::FromPEMStrings(
          client_private_key_pem_, client_cert_pem_);
    } else {
      client_identity_ = rtc::SSLIdentity::Generate("client", client_key_type_);
    }
    server_identity_ = rtc::SSLIdentity::Generate("server", server_key_type_);

    client_ssl_->SetIdentity(client_identity_);
    server_ssl_->SetIdentity(server_identity_);
  }

  void TearDown() override {
    client_ssl_.reset(nullptr);
    server_ssl_.reset(nullptr);
  }

  virtual void CreateStreams() = 0;

  // Recreate the client/server identities with the specified validity period.
  // |not_before| and |not_after| are offsets from the current time in number
  // of seconds.
  void ResetIdentitiesWithValidity(int not_before, int not_after) {
    CreateStreams();

    client_ssl_.reset(rtc::SSLStreamAdapter::Create(client_stream_));
    server_ssl_.reset(rtc::SSLStreamAdapter::Create(server_stream_));

    client_ssl_->SignalEvent.connect(this, &SSLStreamAdapterTestBase::OnEvent);
    server_ssl_->SignalEvent.connect(this, &SSLStreamAdapterTestBase::OnEvent);

    time_t now = time(nullptr);

    rtc::SSLIdentityParams client_params;
    client_params.key_params = rtc::KeyParams(rtc::KT_DEFAULT);
    client_params.common_name = "client";
    client_params.not_before = now + not_before;
    client_params.not_after = now + not_after;
    client_identity_ = rtc::SSLIdentity::GenerateForTest(client_params);

    rtc::SSLIdentityParams server_params;
    server_params.key_params = rtc::KeyParams(rtc::KT_DEFAULT);
    server_params.common_name = "server";
    server_params.not_before = now + not_before;
    server_params.not_after = now + not_after;
    server_identity_ = rtc::SSLIdentity::GenerateForTest(server_params);

    client_ssl_->SetIdentity(client_identity_);
    server_ssl_->SetIdentity(server_identity_);
  }

  virtual void OnEvent(rtc::StreamInterface* stream, int sig, int err) {
    RTC_LOG(LS_VERBOSE) << "SSLStreamAdapterTestBase::OnEvent sig=" << sig;

    if (sig & rtc::SE_READ) {
      ReadData(stream);
    }

    if ((stream == client_ssl_.get()) && (sig & rtc::SE_WRITE)) {
      WriteData();
    }
  }

  void SetPeerIdentitiesByDigest(bool correct, bool expect_success) {
    unsigned char server_digest[20];
    size_t server_digest_len;
    unsigned char client_digest[20];
    size_t client_digest_len;
    bool rv;
    rtc::SSLPeerCertificateDigestError err;
    rtc::SSLPeerCertificateDigestError expected_err =
        expect_success
            ? rtc::SSLPeerCertificateDigestError::NONE
            : rtc::SSLPeerCertificateDigestError::VERIFICATION_FAILED;

    RTC_LOG(LS_INFO) << "Setting peer identities by digest";

    rv = server_identity_->certificate().ComputeDigest(
        rtc::DIGEST_SHA_1, server_digest, 20, &server_digest_len);
    ASSERT_TRUE(rv);
    rv = client_identity_->certificate().ComputeDigest(
        rtc::DIGEST_SHA_1, client_digest, 20, &client_digest_len);
    ASSERT_TRUE(rv);

    if (!correct) {
      RTC_LOG(LS_INFO) << "Setting bogus digest for server cert";
      server_digest[0]++;
    }
    rv = client_ssl_->SetPeerCertificateDigest(rtc::DIGEST_SHA_1, server_digest,
                                               server_digest_len, &err);
    EXPECT_EQ(expected_err, err);
    EXPECT_EQ(expect_success, rv);

    if (!correct) {
      RTC_LOG(LS_INFO) << "Setting bogus digest for client cert";
      client_digest[0]++;
    }
    rv = server_ssl_->SetPeerCertificateDigest(rtc::DIGEST_SHA_1, client_digest,
                                               client_digest_len, &err);
    EXPECT_EQ(expected_err, err);
    EXPECT_EQ(expect_success, rv);

    identities_set_ = true;
  }

  void SetupProtocolVersions(rtc::SSLProtocolVersion server_version,
                             rtc::SSLProtocolVersion client_version) {
    server_ssl_->SetMaxProtocolVersion(server_version);
    client_ssl_->SetMaxProtocolVersion(client_version);
  }

  void TestHandshake(bool expect_success = true) {
    server_ssl_->SetMode(dtls_ ? rtc::SSL_MODE_DTLS : rtc::SSL_MODE_TLS);
    client_ssl_->SetMode(dtls_ ? rtc::SSL_MODE_DTLS : rtc::SSL_MODE_TLS);

    if (!dtls_) {
      // Make sure we simulate a reliable network for TLS.
      // This is just a check to make sure that people don't write wrong
      // tests.
      RTC_CHECK_EQ(1460, mtu_);
      RTC_CHECK(!loss_);
      RTC_CHECK(!lose_first_packet_);
    }

    if (!identities_set_)
      SetPeerIdentitiesByDigest(true, true);

    // Start the handshake
    int rv;

    server_ssl_->SetServerRole();
    rv = server_ssl_->StartSSL();
    ASSERT_EQ(0, rv);

    rv = client_ssl_->StartSSL();
    ASSERT_EQ(0, rv);

    // Now run the handshake
    if (expect_success) {
      EXPECT_TRUE_WAIT((client_ssl_->GetState() == rtc::SS_OPEN) &&
                           (server_ssl_->GetState() == rtc::SS_OPEN),
                       handshake_wait_);
    } else {
      EXPECT_TRUE_WAIT(client_ssl_->GetState() == rtc::SS_CLOSED,
                       handshake_wait_);
    }
  }

  // This tests that the handshake can complete before the identity is
  // verified, and the identity will be verified after the fact.
  void TestHandshakeWithDelayedIdentity(bool valid_identity) {
    server_ssl_->SetMode(dtls_ ? rtc::SSL_MODE_DTLS : rtc::SSL_MODE_TLS);
    client_ssl_->SetMode(dtls_ ? rtc::SSL_MODE_DTLS : rtc::SSL_MODE_TLS);

    if (!dtls_) {
      // Make sure we simulate a reliable network for TLS.
      // This is just a check to make sure that people don't write wrong
      // tests.
      RTC_CHECK_EQ(1460, mtu_);
      RTC_CHECK(!loss_);
      RTC_CHECK(!lose_first_packet_);
    }

    // Start the handshake
    int rv;

    server_ssl_->SetServerRole();
    rv = server_ssl_->StartSSL();
    ASSERT_EQ(0, rv);

    rv = client_ssl_->StartSSL();
    ASSERT_EQ(0, rv);

    // Now run the handshake.
    EXPECT_TRUE_WAIT(
        client_ssl_->IsTlsConnected() && server_ssl_->IsTlsConnected(),
        handshake_wait_);

    // Until the identity has been verified, the state should still be
    // SS_OPENING and writes should return SR_BLOCK.
    EXPECT_EQ(rtc::SS_OPENING, client_ssl_->GetState());
    EXPECT_EQ(rtc::SS_OPENING, server_ssl_->GetState());
    unsigned char packet[1];
    size_t sent;
    EXPECT_EQ(rtc::SR_BLOCK, client_ssl_->Write(&packet, 1, &sent, 0));
    EXPECT_EQ(rtc::SR_BLOCK, server_ssl_->Write(&packet, 1, &sent, 0));

    // If we set an invalid identity at this point, SetPeerCertificateDigest
    // should return false.
    SetPeerIdentitiesByDigest(valid_identity, valid_identity);
    // State should then transition to SS_OPEN or SS_CLOSED based on validation
    // of the identity.
    if (valid_identity) {
      EXPECT_EQ(rtc::SS_OPEN, client_ssl_->GetState());
      EXPECT_EQ(rtc::SS_OPEN, server_ssl_->GetState());
    } else {
      EXPECT_EQ(rtc::SS_CLOSED, client_ssl_->GetState());
      EXPECT_EQ(rtc::SS_CLOSED, server_ssl_->GetState());
    }
  }

  rtc::StreamResult DataWritten(SSLDummyStreamBase* from,
                                const void* data,
                                size_t data_len,
                                size_t* written,
                                int* error) {
    // Randomly drop loss_ percent of packets
    if (rtc::CreateRandomId() % 100 < static_cast<uint32_t>(loss_)) {
      RTC_LOG(LS_VERBOSE) << "Randomly dropping packet, size=" << data_len;
      *written = data_len;
      return rtc::SR_SUCCESS;
    }
    if (dtls_ && (data_len > mtu_)) {
      RTC_LOG(LS_VERBOSE) << "Dropping packet > mtu, size=" << data_len;
      *written = data_len;
      return rtc::SR_SUCCESS;
    }

    // Optionally damage application data (type 23). Note that we don't damage
    // handshake packets and we damage the last byte to keep the header
    // intact but break the MAC.
    if (damage_ && (*static_cast<const unsigned char*>(data) == 23)) {
      std::vector<char> buf(data_len);

      RTC_LOG(LS_VERBOSE) << "Damaging packet";

      memcpy(&buf[0], data, data_len);
      buf[data_len - 1]++;

      return from->WriteData(&buf[0], data_len, written, error);
    }

    return from->WriteData(data, data_len, written, error);
  }

  void SetDelay(int delay) { delay_ = delay; }
  int GetDelay() { return delay_; }

  void SetLoseFirstPacket(bool lose) { lose_first_packet_ = lose; }
  bool GetLoseFirstPacket() { return lose_first_packet_; }

  void SetLoss(int percent) { loss_ = percent; }

  void SetDamage() { damage_ = true; }

  void SetMtu(size_t mtu) { mtu_ = mtu; }

  void SetHandshakeWait(int wait) { handshake_wait_ = wait; }

  void SetDtlsSrtpCryptoSuites(const std::vector<int>& ciphers, bool client) {
    if (client)
      client_ssl_->SetDtlsSrtpCryptoSuites(ciphers);
    else
      server_ssl_->SetDtlsSrtpCryptoSuites(ciphers);
  }

  bool GetDtlsSrtpCryptoSuite(bool client, int* retval) {
    if (client)
      return client_ssl_->GetDtlsSrtpCryptoSuite(retval);
    else
      return server_ssl_->GetDtlsSrtpCryptoSuite(retval);
  }

  std::unique_ptr<rtc::SSLCertificate> GetPeerCertificate(bool client) {
    std::unique_ptr<rtc::SSLCertChain> chain;
    if (client)
      chain = client_ssl_->GetPeerSSLCertChain();
    else
      chain = server_ssl_->GetPeerSSLCertChain();
    return (chain && chain->GetSize()) ? chain->Get(0).GetUniqueReference()
                                       : nullptr;
  }

  bool GetSslCipherSuite(bool client, int* retval) {
    if (client)
      return client_ssl_->GetSslCipherSuite(retval);
    else
      return server_ssl_->GetSslCipherSuite(retval);
  }

  int GetSslVersion(bool client) {
    if (client)
      return client_ssl_->GetSslVersion();
    else
      return server_ssl_->GetSslVersion();
  }

  bool ExportKeyingMaterial(const char* label,
                            const unsigned char* context,
                            size_t context_len,
                            bool use_context,
                            bool client,
                            unsigned char* result,
                            size_t result_len) {
    if (client)
      return client_ssl_->ExportKeyingMaterial(label, context, context_len,
                                               use_context, result, result_len);
    else
      return server_ssl_->ExportKeyingMaterial(label, context, context_len,
                                               use_context, result, result_len);
  }

  // To be implemented by subclasses.
  virtual void WriteData() = 0;
  virtual void ReadData(rtc::StreamInterface* stream) = 0;
  virtual void TestTransfer(int size) = 0;

 protected:
  std::string client_cert_pem_;
  std::string client_private_key_pem_;
  rtc::KeyParams client_key_type_;
  rtc::KeyParams server_key_type_;
  SSLDummyStreamBase* client_stream_;  // freed by client_ssl_ destructor
  SSLDummyStreamBase* server_stream_;  // freed by server_ssl_ destructor
  std::unique_ptr<rtc::SSLStreamAdapter> client_ssl_;
  std::unique_ptr<rtc::SSLStreamAdapter> server_ssl_;
  rtc::SSLIdentity* client_identity_;  // freed by client_ssl_ destructor
  rtc::SSLIdentity* server_identity_;  // freed by server_ssl_ destructor
  int delay_;
  size_t mtu_;
  int loss_;
  bool lose_first_packet_;
  bool damage_;
  bool dtls_;
  int handshake_wait_;
  bool identities_set_;
};

class SSLStreamAdapterTestTLS
    : public SSLStreamAdapterTestBase,
      public WithParamInterface<tuple<rtc::KeyParams, rtc::KeyParams>> {
 public:
  SSLStreamAdapterTestTLS()
      : SSLStreamAdapterTestBase("",
                                 "",
                                 false,
                                 ::testing::get<0>(GetParam()),
                                 ::testing::get<1>(GetParam())),
        client_buffer_(kFifoBufferSize),
        server_buffer_(kFifoBufferSize) {}

  void CreateStreams() override {
    client_stream_ =
        new SSLDummyStreamTLS(this, "c2s", &client_buffer_, &server_buffer_);
    server_stream_ =
        new SSLDummyStreamTLS(this, "s2c", &server_buffer_, &client_buffer_);
  }

  // Test data transfer for TLS
  void TestTransfer(int size) override {
    RTC_LOG(LS_INFO) << "Starting transfer test with " << size << " bytes";
    // Create some dummy data to send.
    size_t received;

    send_stream_.ReserveSize(size);
    for (int i = 0; i < size; ++i) {
      char ch = static_cast<char>(i);
      send_stream_.Write(&ch, 1, nullptr, nullptr);
    }
    send_stream_.Rewind();

    // Prepare the receive stream.
    recv_stream_.ReserveSize(size);

    // Start sending
    WriteData();

    // Wait for the client to close
    EXPECT_TRUE_WAIT(server_ssl_->GetState() == rtc::SS_CLOSED, 10000);

    // Now check the data
    recv_stream_.GetSize(&received);

    EXPECT_EQ(static_cast<size_t>(size), received);
    EXPECT_EQ(0,
              memcmp(send_stream_.GetBuffer(), recv_stream_.GetBuffer(), size));
  }

  void WriteData() override {
    size_t position, tosend, size;
    rtc::StreamResult rv;
    size_t sent;
    char block[kBlockSize];

    send_stream_.GetSize(&size);
    if (!size)
      return;

    for (;;) {
      send_stream_.GetPosition(&position);
      if (send_stream_.Read(block, sizeof(block), &tosend, nullptr) !=
          rtc::SR_EOS) {
        rv = client_ssl_->Write(block, tosend, &sent, 0);

        if (rv == rtc::SR_SUCCESS) {
          send_stream_.SetPosition(position + sent);
          RTC_LOG(LS_VERBOSE) << "Sent: " << position + sent;
        } else if (rv == rtc::SR_BLOCK) {
          RTC_LOG(LS_VERBOSE) << "Blocked...";
          send_stream_.SetPosition(position);
          break;
        } else {
          ADD_FAILURE();
          break;
        }
      } else {
        // Now close
        RTC_LOG(LS_INFO) << "Wrote " << position << " bytes. Closing";
        client_ssl_->Close();
        break;
      }
    }
  };

  void ReadData(rtc::StreamInterface* stream) override {
    char buffer[1600];
    size_t bread;
    int err2;
    rtc::StreamResult r;

    for (;;) {
      r = stream->Read(buffer, sizeof(buffer), &bread, &err2);

      if (r == rtc::SR_ERROR || r == rtc::SR_EOS) {
        // Unfortunately, errors are the way that the stream adapter
        // signals close in OpenSSL.
        stream->Close();
        return;
      }

      if (r == rtc::SR_BLOCK)
        break;

      ASSERT_EQ(rtc::SR_SUCCESS, r);
      RTC_LOG(LS_VERBOSE) << "Read " << bread;

      recv_stream_.Write(buffer, bread, nullptr, nullptr);
    }
  }

 private:
  rtc::FifoBuffer client_buffer_;
  rtc::FifoBuffer server_buffer_;
  rtc::MemoryStream send_stream_;
  rtc::MemoryStream recv_stream_;
};

class SSLStreamAdapterTestDTLS
    : public SSLStreamAdapterTestBase,
      public WithParamInterface<tuple<rtc::KeyParams, rtc::KeyParams>> {
 public:
  SSLStreamAdapterTestDTLS()
      : SSLStreamAdapterTestBase("",
                                 "",
                                 true,
                                 ::testing::get<0>(GetParam()),
                                 ::testing::get<1>(GetParam())),
        client_buffer_(kBufferCapacity, kDefaultBufferSize),
        server_buffer_(kBufferCapacity, kDefaultBufferSize),
        packet_size_(1000),
        count_(0),
        sent_(0) {}

  SSLStreamAdapterTestDTLS(const std::string& cert_pem,
                           const std::string& private_key_pem)
      : SSLStreamAdapterTestBase(cert_pem, private_key_pem, true),
        client_buffer_(kBufferCapacity, kDefaultBufferSize),
        server_buffer_(kBufferCapacity, kDefaultBufferSize),
        packet_size_(1000),
        count_(0),
        sent_(0) {}

  void CreateStreams() override {
    client_stream_ =
        new SSLDummyStreamDTLS(this, "c2s", &client_buffer_, &server_buffer_);
    server_stream_ =
        new SSLDummyStreamDTLS(this, "s2c", &server_buffer_, &client_buffer_);
  }

  void WriteData() override {
    unsigned char* packet = new unsigned char[1600];

    while (sent_ < count_) {
      unsigned int rand_state = sent_;
      packet[0] = sent_;
      for (size_t i = 1; i < packet_size_; i++) {
        // This is a simple LC PRNG.  Keep in synch with identical code below.
        rand_state = (rand_state * 251 + 19937) >> 7;
        packet[i] = rand_state & 0xff;
      }

      size_t sent;
      rtc::StreamResult rv = client_ssl_->Write(packet, packet_size_, &sent, 0);
      if (rv == rtc::SR_SUCCESS) {
        RTC_LOG(LS_VERBOSE) << "Sent: " << sent_;
        sent_++;
      } else if (rv == rtc::SR_BLOCK) {
        RTC_LOG(LS_VERBOSE) << "Blocked...";
        break;
      } else {
        ADD_FAILURE();
        break;
      }
    }

    delete[] packet;
  }

  void ReadData(rtc::StreamInterface* stream) override {
    unsigned char buffer[2000];
    size_t bread;
    int err2;
    rtc::StreamResult r;

    for (;;) {
      r = stream->Read(buffer, 2000, &bread, &err2);

      if (r == rtc::SR_ERROR) {
        // Unfortunately, errors are the way that the stream adapter
        // signals close right now
        stream->Close();
        return;
      }

      if (r == rtc::SR_BLOCK)
        break;

      ASSERT_EQ(rtc::SR_SUCCESS, r);
      RTC_LOG(LS_VERBOSE) << "Read " << bread;

      // Now parse the datagram
      ASSERT_EQ(packet_size_, bread);
      unsigned char packet_num = buffer[0];

      unsigned int rand_state = packet_num;
      for (size_t i = 1; i < packet_size_; i++) {
        // This is a simple LC PRNG.  Keep in synch with identical code above.
        rand_state = (rand_state * 251 + 19937) >> 7;
        ASSERT_EQ(rand_state & 0xff, buffer[i]);
      }
      received_.insert(packet_num);
    }
  }

  void TestTransfer(int count) override {
    count_ = count;

    WriteData();

    EXPECT_TRUE_WAIT(sent_ == count_, 10000);
    RTC_LOG(LS_INFO) << "sent_ == " << sent_;

    if (damage_) {
      WAIT(false, 2000);
      EXPECT_EQ(0U, received_.size());
    } else if (loss_ == 0) {
      EXPECT_EQ_WAIT(static_cast<size_t>(sent_), received_.size(), 1000);
    } else {
      RTC_LOG(LS_INFO) << "Sent " << sent_ << " packets; received "
                       << received_.size();
    }
  };

 private:
  BufferQueueStream client_buffer_;
  BufferQueueStream server_buffer_;
  size_t packet_size_;
  int count_;
  int sent_;
  std::set<int> received_;
};

rtc::StreamResult SSLDummyStreamBase::Write(const void* data,
                                            size_t data_len,
                                            size_t* written,
                                            int* error) {
  RTC_LOG(LS_VERBOSE) << "Writing to loopback " << data_len;

  if (first_packet_) {
    first_packet_ = false;
    if (test_base_->GetLoseFirstPacket()) {
      RTC_LOG(LS_INFO) << "Losing initial packet of length " << data_len;
      *written = data_len;  // Fake successful writing also to writer.
      return rtc::SR_SUCCESS;
    }
  }

  return test_base_->DataWritten(this, data, data_len, written, error);
};

class SSLStreamAdapterTestDTLSFromPEMStrings : public SSLStreamAdapterTestDTLS {
 public:
  SSLStreamAdapterTestDTLSFromPEMStrings()
      : SSLStreamAdapterTestDTLS(kCERT_PEM, kRSA_PRIVATE_KEY_PEM) {}
};

// Test fixture for certificate chaining. Server will push more than one
// certificate.
class SSLStreamAdapterTestDTLSCertChain : public SSLStreamAdapterTestDTLS {
 public:
  SSLStreamAdapterTestDTLSCertChain() : SSLStreamAdapterTestDTLS("", ""){};
  void SetUp() override {
    CreateStreams();

    client_ssl_.reset(rtc::SSLStreamAdapter::Create(client_stream_));
    server_ssl_.reset(rtc::SSLStreamAdapter::Create(server_stream_));

    // Set up the slots
    client_ssl_->SignalEvent.connect(
        reinterpret_cast<SSLStreamAdapterTestBase*>(this),
        &SSLStreamAdapterTestBase::OnEvent);
    server_ssl_->SignalEvent.connect(
        reinterpret_cast<SSLStreamAdapterTestBase*>(this),
        &SSLStreamAdapterTestBase::OnEvent);

    if (!client_cert_pem_.empty() && !client_private_key_pem_.empty()) {
      client_identity_ = rtc::SSLIdentity::FromPEMStrings(
          client_private_key_pem_, client_cert_pem_);
    } else {
      client_identity_ = rtc::SSLIdentity::Generate("client", client_key_type_);
    }

    client_ssl_->SetIdentity(client_identity_);
  }
};

// Basic tests: TLS

// Test that we can make a handshake work
TEST_P(SSLStreamAdapterTestTLS, TestTLSConnect) {
  TestHandshake();
};

TEST_P(SSLStreamAdapterTestTLS, GetPeerCertChainWithOneCertificate) {
  TestHandshake();
  std::unique_ptr<rtc::SSLCertChain> cert_chain =
      client_ssl_->GetPeerSSLCertChain();
  ASSERT_NE(nullptr, cert_chain);
  EXPECT_EQ(1u, cert_chain->GetSize());
  EXPECT_EQ(cert_chain->Get(0).ToPEMString(),
            server_identity_->certificate().ToPEMString());
}

TEST_F(SSLStreamAdapterTestDTLSCertChain, TwoCertHandshake) {
  server_identity_ = rtc::SSLIdentity::FromPEMChainStrings(
      kRSA_PRIVATE_KEY_PEM, std::string(kCERT_PEM) + kCACert);
  server_ssl_->SetIdentity(server_identity_);
  TestHandshake();
  std::unique_ptr<rtc::SSLCertChain> peer_cert_chain =
      client_ssl_->GetPeerSSLCertChain();
  ASSERT_NE(nullptr, peer_cert_chain);
  ASSERT_EQ(2u, peer_cert_chain->GetSize());
  EXPECT_EQ(kCERT_PEM, peer_cert_chain->Get(0).ToPEMString());
  EXPECT_EQ(kCACert, peer_cert_chain->Get(1).ToPEMString());
}

TEST_F(SSLStreamAdapterTestDTLSCertChain, TwoCertHandshakeWithCopy) {
  std::unique_ptr<rtc::SSLIdentity> identity(
      rtc::SSLIdentity::FromPEMChainStrings(kRSA_PRIVATE_KEY_PEM,
                                            std::string(kCERT_PEM) + kCACert));
  server_identity_ = identity->GetReference();
  server_ssl_->SetIdentity(server_identity_);
  TestHandshake();
  std::unique_ptr<rtc::SSLCertChain> peer_cert_chain =
      client_ssl_->GetPeerSSLCertChain();
  ASSERT_NE(nullptr, peer_cert_chain);
  ASSERT_EQ(2u, peer_cert_chain->GetSize());
  EXPECT_EQ(kCERT_PEM, peer_cert_chain->Get(0).ToPEMString());
  EXPECT_EQ(kCACert, peer_cert_chain->Get(1).ToPEMString());
}

TEST_F(SSLStreamAdapterTestDTLSCertChain, ThreeCertHandshake) {
  server_identity_ = rtc::SSLIdentity::FromPEMChainStrings(
      kRSA_PRIVATE_KEY_PEM, std::string(kCERT_PEM) + kIntCert1 + kCACert);
  server_ssl_->SetIdentity(server_identity_);
  TestHandshake();
  std::unique_ptr<rtc::SSLCertChain> peer_cert_chain =
      client_ssl_->GetPeerSSLCertChain();
  ASSERT_NE(nullptr, peer_cert_chain);
  ASSERT_EQ(3u, peer_cert_chain->GetSize());
  EXPECT_EQ(kCERT_PEM, peer_cert_chain->Get(0).ToPEMString());
  EXPECT_EQ(kIntCert1, peer_cert_chain->Get(1).ToPEMString());
  EXPECT_EQ(kCACert, peer_cert_chain->Get(2).ToPEMString());
}

// Test that closing the connection on one side updates the other side.
TEST_P(SSLStreamAdapterTestTLS, TestTLSClose) {
  TestHandshake();
  client_ssl_->Close();
  EXPECT_EQ_WAIT(rtc::SS_CLOSED, server_ssl_->GetState(), handshake_wait_);
};

// Test transfer -- trivial
TEST_P(SSLStreamAdapterTestTLS, TestTLSTransfer) {
  TestHandshake();
  TestTransfer(100000);
};

// Test read-write after close.
TEST_P(SSLStreamAdapterTestTLS, ReadWriteAfterClose) {
  TestHandshake();
  TestTransfer(100000);
  client_ssl_->Close();

  rtc::StreamResult rv;
  char block[kBlockSize];
  size_t dummy;

  // It's an error to write after closed.
  rv = client_ssl_->Write(block, sizeof(block), &dummy, nullptr);
  ASSERT_EQ(rtc::SR_ERROR, rv);

  // But after closed read gives you EOS.
  rv = client_ssl_->Read(block, sizeof(block), &dummy, nullptr);
  ASSERT_EQ(rtc::SR_EOS, rv);
};

// Test a handshake with a bogus peer digest
TEST_P(SSLStreamAdapterTestTLS, TestTLSBogusDigest) {
  SetPeerIdentitiesByDigest(false, true);
  TestHandshake(false);
};

TEST_P(SSLStreamAdapterTestTLS, TestTLSDelayedIdentity) {
  TestHandshakeWithDelayedIdentity(true);
};

TEST_P(SSLStreamAdapterTestTLS, TestTLSDelayedIdentityWithBogusDigest) {
  TestHandshakeWithDelayedIdentity(false);
};

// Test that the correct error is returned when SetPeerCertificateDigest is
// called with an unknown algorithm.
TEST_P(SSLStreamAdapterTestTLS,
       TestSetPeerCertificateDigestWithUnknownAlgorithm) {
  unsigned char server_digest[20];
  size_t server_digest_len;
  bool rv;
  rtc::SSLPeerCertificateDigestError err;

  rv = server_identity_->certificate().ComputeDigest(
      rtc::DIGEST_SHA_1, server_digest, 20, &server_digest_len);
  ASSERT_TRUE(rv);

  rv = client_ssl_->SetPeerCertificateDigest("unknown algorithm", server_digest,
                                             server_digest_len, &err);
  EXPECT_EQ(rtc::SSLPeerCertificateDigestError::UNKNOWN_ALGORITHM, err);
  EXPECT_FALSE(rv);
}

// Test that the correct error is returned when SetPeerCertificateDigest is
// called with an invalid digest length.
TEST_P(SSLStreamAdapterTestTLS, TestSetPeerCertificateDigestWithInvalidLength) {
  unsigned char server_digest[20];
  size_t server_digest_len;
  bool rv;
  rtc::SSLPeerCertificateDigestError err;

  rv = server_identity_->certificate().ComputeDigest(
      rtc::DIGEST_SHA_1, server_digest, 20, &server_digest_len);
  ASSERT_TRUE(rv);

  rv = client_ssl_->SetPeerCertificateDigest(rtc::DIGEST_SHA_1, server_digest,
                                             server_digest_len - 1, &err);
  EXPECT_EQ(rtc::SSLPeerCertificateDigestError::INVALID_LENGTH, err);
  EXPECT_FALSE(rv);
}

// Test moving a bunch of data

// Basic tests: DTLS
// Test that we can make a handshake work
TEST_P(SSLStreamAdapterTestDTLS, TestDTLSConnect) {
  TestHandshake();
};

// Test that we can make a handshake work if the first packet in
// each direction is lost. This gives us predictable loss
// rather than having to tune random
TEST_P(SSLStreamAdapterTestDTLS, TestDTLSConnectWithLostFirstPacket) {
  SetLoseFirstPacket(true);
  TestHandshake();
};

// Test a handshake with loss and delay
TEST_P(SSLStreamAdapterTestDTLS, TestDTLSConnectWithLostFirstPacketDelay2s) {
  SetLoseFirstPacket(true);
  SetDelay(2000);
  SetHandshakeWait(20000);
  TestHandshake();
};

// Test a handshake with small MTU
// Disabled due to https://code.google.com/p/webrtc/issues/detail?id=3910
TEST_P(SSLStreamAdapterTestDTLS, DISABLED_TestDTLSConnectWithSmallMtu) {
  SetMtu(700);
  SetHandshakeWait(20000);
  TestHandshake();
};

// Test transfer -- trivial
TEST_P(SSLStreamAdapterTestDTLS, TestDTLSTransfer) {
  TestHandshake();
  TestTransfer(100);
};

TEST_P(SSLStreamAdapterTestDTLS, TestDTLSTransferWithLoss) {
  TestHandshake();
  SetLoss(10);
  TestTransfer(100);
};

TEST_P(SSLStreamAdapterTestDTLS, TestDTLSTransferWithDamage) {
  SetDamage();  // Must be called first because first packet
                // write happens at end of handshake.
  TestHandshake();
  TestTransfer(100);
};

TEST_P(SSLStreamAdapterTestDTLS, TestDTLSDelayedIdentity) {
  TestHandshakeWithDelayedIdentity(true);
};

TEST_P(SSLStreamAdapterTestDTLS, TestDTLSDelayedIdentityWithBogusDigest) {
  TestHandshakeWithDelayedIdentity(false);
};

// Test DTLS-SRTP with all high ciphers
TEST_P(SSLStreamAdapterTestDTLS, TestDTLSSrtpHigh) {
  std::vector<int> high;
  high.push_back(rtc::SRTP_AES128_CM_SHA1_80);
  SetDtlsSrtpCryptoSuites(high, true);
  SetDtlsSrtpCryptoSuites(high, false);
  TestHandshake();

  int client_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(false, &server_cipher));

  ASSERT_EQ(client_cipher, server_cipher);
  ASSERT_EQ(client_cipher, rtc::SRTP_AES128_CM_SHA1_80);
};

// Test DTLS-SRTP with all low ciphers
TEST_P(SSLStreamAdapterTestDTLS, TestDTLSSrtpLow) {
  std::vector<int> low;
  low.push_back(rtc::SRTP_AES128_CM_SHA1_32);
  SetDtlsSrtpCryptoSuites(low, true);
  SetDtlsSrtpCryptoSuites(low, false);
  TestHandshake();

  int client_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(false, &server_cipher));

  ASSERT_EQ(client_cipher, server_cipher);
  ASSERT_EQ(client_cipher, rtc::SRTP_AES128_CM_SHA1_32);
};

// Test DTLS-SRTP with a mismatch -- should not converge
TEST_P(SSLStreamAdapterTestDTLS, TestDTLSSrtpHighLow) {
  std::vector<int> high;
  high.push_back(rtc::SRTP_AES128_CM_SHA1_80);
  std::vector<int> low;
  low.push_back(rtc::SRTP_AES128_CM_SHA1_32);
  SetDtlsSrtpCryptoSuites(high, true);
  SetDtlsSrtpCryptoSuites(low, false);
  TestHandshake();

  int client_cipher;
  ASSERT_FALSE(GetDtlsSrtpCryptoSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_FALSE(GetDtlsSrtpCryptoSuite(false, &server_cipher));
};

// Test DTLS-SRTP with each side being mixed -- should select high
TEST_P(SSLStreamAdapterTestDTLS, TestDTLSSrtpMixed) {
  std::vector<int> mixed;
  mixed.push_back(rtc::SRTP_AES128_CM_SHA1_80);
  mixed.push_back(rtc::SRTP_AES128_CM_SHA1_32);
  SetDtlsSrtpCryptoSuites(mixed, true);
  SetDtlsSrtpCryptoSuites(mixed, false);
  TestHandshake();

  int client_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(false, &server_cipher));

  ASSERT_EQ(client_cipher, server_cipher);
  ASSERT_EQ(client_cipher, rtc::SRTP_AES128_CM_SHA1_80);
};

// Test DTLS-SRTP with all GCM-128 ciphers.
TEST_P(SSLStreamAdapterTestDTLS, TestDTLSSrtpGCM128) {
  std::vector<int> gcm128;
  gcm128.push_back(rtc::SRTP_AEAD_AES_128_GCM);
  SetDtlsSrtpCryptoSuites(gcm128, true);
  SetDtlsSrtpCryptoSuites(gcm128, false);
  TestHandshake();

  int client_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(false, &server_cipher));

  ASSERT_EQ(client_cipher, server_cipher);
  ASSERT_EQ(client_cipher, rtc::SRTP_AEAD_AES_128_GCM);
};

// Test DTLS-SRTP with all GCM-256 ciphers.
TEST_P(SSLStreamAdapterTestDTLS, TestDTLSSrtpGCM256) {
  std::vector<int> gcm256;
  gcm256.push_back(rtc::SRTP_AEAD_AES_256_GCM);
  SetDtlsSrtpCryptoSuites(gcm256, true);
  SetDtlsSrtpCryptoSuites(gcm256, false);
  TestHandshake();

  int client_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(false, &server_cipher));

  ASSERT_EQ(client_cipher, server_cipher);
  ASSERT_EQ(client_cipher, rtc::SRTP_AEAD_AES_256_GCM);
};

// Test DTLS-SRTP with mixed GCM-128/-256 ciphers -- should not converge.
TEST_P(SSLStreamAdapterTestDTLS, TestDTLSSrtpGCMMismatch) {
  std::vector<int> gcm128;
  gcm128.push_back(rtc::SRTP_AEAD_AES_128_GCM);
  std::vector<int> gcm256;
  gcm256.push_back(rtc::SRTP_AEAD_AES_256_GCM);
  SetDtlsSrtpCryptoSuites(gcm128, true);
  SetDtlsSrtpCryptoSuites(gcm256, false);
  TestHandshake();

  int client_cipher;
  ASSERT_FALSE(GetDtlsSrtpCryptoSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_FALSE(GetDtlsSrtpCryptoSuite(false, &server_cipher));
};

// Test DTLS-SRTP with both GCM-128/-256 ciphers -- should select GCM-256.
TEST_P(SSLStreamAdapterTestDTLS, TestDTLSSrtpGCMMixed) {
  std::vector<int> gcmBoth;
  gcmBoth.push_back(rtc::SRTP_AEAD_AES_256_GCM);
  gcmBoth.push_back(rtc::SRTP_AEAD_AES_128_GCM);
  SetDtlsSrtpCryptoSuites(gcmBoth, true);
  SetDtlsSrtpCryptoSuites(gcmBoth, false);
  TestHandshake();

  int client_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(false, &server_cipher));

  ASSERT_EQ(client_cipher, server_cipher);
  ASSERT_EQ(client_cipher, rtc::SRTP_AEAD_AES_256_GCM);
};

// Test SRTP cipher suite lengths.
TEST_P(SSLStreamAdapterTestDTLS, TestDTLSSrtpKeyAndSaltLengths) {
  int key_len;
  int salt_len;

  ASSERT_FALSE(rtc::GetSrtpKeyAndSaltLengths(rtc::SRTP_INVALID_CRYPTO_SUITE,
                                             &key_len, &salt_len));

  ASSERT_TRUE(rtc::GetSrtpKeyAndSaltLengths(rtc::SRTP_AES128_CM_SHA1_32,
                                            &key_len, &salt_len));
  ASSERT_EQ(128 / 8, key_len);
  ASSERT_EQ(112 / 8, salt_len);

  ASSERT_TRUE(rtc::GetSrtpKeyAndSaltLengths(rtc::SRTP_AES128_CM_SHA1_80,
                                            &key_len, &salt_len));
  ASSERT_EQ(128 / 8, key_len);
  ASSERT_EQ(112 / 8, salt_len);

  ASSERT_TRUE(rtc::GetSrtpKeyAndSaltLengths(rtc::SRTP_AEAD_AES_128_GCM,
                                            &key_len, &salt_len));
  ASSERT_EQ(128 / 8, key_len);
  ASSERT_EQ(96 / 8, salt_len);

  ASSERT_TRUE(rtc::GetSrtpKeyAndSaltLengths(rtc::SRTP_AEAD_AES_256_GCM,
                                            &key_len, &salt_len));
  ASSERT_EQ(256 / 8, key_len);
  ASSERT_EQ(96 / 8, salt_len);
};

// Test an exporter
TEST_P(SSLStreamAdapterTestDTLS, TestDTLSExporter) {
  TestHandshake();
  unsigned char client_out[20];
  unsigned char server_out[20];

  bool result;
  result = ExportKeyingMaterial(kExporterLabel, kExporterContext,
                                kExporterContextLen, true, true, client_out,
                                sizeof(client_out));
  ASSERT_TRUE(result);

  result = ExportKeyingMaterial(kExporterLabel, kExporterContext,
                                kExporterContextLen, true, false, server_out,
                                sizeof(server_out));
  ASSERT_TRUE(result);

  ASSERT_TRUE(!memcmp(client_out, server_out, sizeof(client_out)));
}

// Test not yet valid certificates are not rejected.
TEST_P(SSLStreamAdapterTestDTLS, TestCertNotYetValid) {
  long one_day = 60 * 60 * 24;
  // Make the certificates not valid until one day later.
  ResetIdentitiesWithValidity(one_day, one_day);
  TestHandshake();
}

// Test expired certificates are not rejected.
TEST_P(SSLStreamAdapterTestDTLS, TestCertExpired) {
  long one_day = 60 * 60 * 24;
  // Make the certificates already expired.
  ResetIdentitiesWithValidity(-one_day, -one_day);
  TestHandshake();
}

// Test data transfer using certs created from strings.
TEST_F(SSLStreamAdapterTestDTLSFromPEMStrings, TestTransfer) {
  TestHandshake();
  TestTransfer(100);
}

// Test getting the remote certificate.
TEST_F(SSLStreamAdapterTestDTLSFromPEMStrings, TestDTLSGetPeerCertificate) {
  // Peer certificates haven't been received yet.
  ASSERT_FALSE(GetPeerCertificate(true));
  ASSERT_FALSE(GetPeerCertificate(false));

  TestHandshake();

  // The client should have a peer certificate after the handshake.
  std::unique_ptr<rtc::SSLCertificate> client_peer_cert =
      GetPeerCertificate(true);
  ASSERT_TRUE(client_peer_cert);

  // It's not kCERT_PEM.
  std::string client_peer_string = client_peer_cert->ToPEMString();
  ASSERT_NE(kCERT_PEM, client_peer_string);

  // The server should have a peer certificate after the handshake.
  std::unique_ptr<rtc::SSLCertificate> server_peer_cert =
      GetPeerCertificate(false);
  ASSERT_TRUE(server_peer_cert);

  // It's kCERT_PEM
  ASSERT_EQ(kCERT_PEM, server_peer_cert->ToPEMString());
}

// Test getting the used DTLS ciphers.
// DTLS 1.2 enabled for neither client nor server -> DTLS 1.0 will be used.
TEST_P(SSLStreamAdapterTestDTLS, TestGetSslCipherSuite) {
  SetupProtocolVersions(rtc::SSL_PROTOCOL_DTLS_10, rtc::SSL_PROTOCOL_DTLS_10);
  TestHandshake();

  int client_cipher;
  ASSERT_TRUE(GetSslCipherSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_TRUE(GetSslCipherSuite(false, &server_cipher));

  ASSERT_EQ(rtc::SSL_PROTOCOL_DTLS_10, GetSslVersion(true));
  ASSERT_EQ(rtc::SSL_PROTOCOL_DTLS_10, GetSslVersion(false));

  ASSERT_EQ(client_cipher, server_cipher);
  ASSERT_TRUE(rtc::SSLStreamAdapter::IsAcceptableCipher(
      server_cipher, ::testing::get<1>(GetParam()).type()));
}

// Test getting the used DTLS 1.2 ciphers.
// DTLS 1.2 enabled for client and server -> DTLS 1.2 will be used.
TEST_P(SSLStreamAdapterTestDTLS, TestGetSslCipherSuiteDtls12Both) {
  SetupProtocolVersions(rtc::SSL_PROTOCOL_DTLS_12, rtc::SSL_PROTOCOL_DTLS_12);
  TestHandshake();

  int client_cipher;
  ASSERT_TRUE(GetSslCipherSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_TRUE(GetSslCipherSuite(false, &server_cipher));

  ASSERT_EQ(rtc::SSL_PROTOCOL_DTLS_12, GetSslVersion(true));
  ASSERT_EQ(rtc::SSL_PROTOCOL_DTLS_12, GetSslVersion(false));

  ASSERT_EQ(client_cipher, server_cipher);
  ASSERT_TRUE(rtc::SSLStreamAdapter::IsAcceptableCipher(
      server_cipher, ::testing::get<1>(GetParam()).type()));
}

// DTLS 1.2 enabled for client only -> DTLS 1.0 will be used.
TEST_P(SSLStreamAdapterTestDTLS, TestGetSslCipherSuiteDtls12Client) {
  SetupProtocolVersions(rtc::SSL_PROTOCOL_DTLS_10, rtc::SSL_PROTOCOL_DTLS_12);
  TestHandshake();

  int client_cipher;
  ASSERT_TRUE(GetSslCipherSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_TRUE(GetSslCipherSuite(false, &server_cipher));

  ASSERT_EQ(rtc::SSL_PROTOCOL_DTLS_10, GetSslVersion(true));
  ASSERT_EQ(rtc::SSL_PROTOCOL_DTLS_10, GetSslVersion(false));

  ASSERT_EQ(client_cipher, server_cipher);
  ASSERT_TRUE(rtc::SSLStreamAdapter::IsAcceptableCipher(
      server_cipher, ::testing::get<1>(GetParam()).type()));
}

// DTLS 1.2 enabled for server only -> DTLS 1.0 will be used.
TEST_P(SSLStreamAdapterTestDTLS, TestGetSslCipherSuiteDtls12Server) {
  SetupProtocolVersions(rtc::SSL_PROTOCOL_DTLS_12, rtc::SSL_PROTOCOL_DTLS_10);
  TestHandshake();

  int client_cipher;
  ASSERT_TRUE(GetSslCipherSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_TRUE(GetSslCipherSuite(false, &server_cipher));

  ASSERT_EQ(rtc::SSL_PROTOCOL_DTLS_10, GetSslVersion(true));
  ASSERT_EQ(rtc::SSL_PROTOCOL_DTLS_10, GetSslVersion(false));

  ASSERT_EQ(client_cipher, server_cipher);
  ASSERT_TRUE(rtc::SSLStreamAdapter::IsAcceptableCipher(
      server_cipher, ::testing::get<1>(GetParam()).type()));
}

// The RSA keysizes here might look strange, why not include the RFC's size
// 2048?. The reason is test case slowness; testing two sizes to exercise
// parametrization is sufficient.
INSTANTIATE_TEST_CASE_P(
    SSLStreamAdapterTestsTLS,
    SSLStreamAdapterTestTLS,
    Combine(Values(rtc::KeyParams::RSA(1024, 65537),
                   rtc::KeyParams::RSA(1152, 65537),
                   rtc::KeyParams::ECDSA(rtc::EC_NIST_P256)),
            Values(rtc::KeyParams::RSA(1024, 65537),
                   rtc::KeyParams::RSA(1152, 65537),
                   rtc::KeyParams::ECDSA(rtc::EC_NIST_P256))));
INSTANTIATE_TEST_CASE_P(
    SSLStreamAdapterTestsDTLS,
    SSLStreamAdapterTestDTLS,
    Combine(Values(rtc::KeyParams::RSA(1024, 65537),
                   rtc::KeyParams::RSA(1152, 65537),
                   rtc::KeyParams::ECDSA(rtc::EC_NIST_P256)),
            Values(rtc::KeyParams::RSA(1024, 65537),
                   rtc::KeyParams::RSA(1152, 65537),
                   rtc::KeyParams::ECDSA(rtc::EC_NIST_P256))));
