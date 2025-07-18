/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/rtc_certificate_generator.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "api/scoped_refptr.h"
#include "api/test/rtc_error_matchers.h"
#include "api/units/time_delta.h"
#include "rtc_base/checks.h"
#include "rtc_base/rtc_certificate.h"
#include "rtc_base/ssl_identity.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

namespace webrtc {

class RTCCertificateGeneratorFixture {
 public:
  RTCCertificateGeneratorFixture()
      : signaling_thread_(Thread::Current()),
        worker_thread_(Thread::Create()),
        generate_async_completed_(false) {
    RTC_CHECK(signaling_thread_);
    RTC_CHECK(worker_thread_->Start());
    generator_.reset(
        new RTCCertificateGenerator(signaling_thread_, worker_thread_.get()));
  }

  RTCCertificateGenerator* generator() const { return generator_.get(); }
  RTCCertificate* certificate() const { return certificate_.get(); }

  RTCCertificateGeneratorInterface::Callback OnGenerated() {
    return [this](scoped_refptr<RTCCertificate> certificate) mutable {
      RTC_CHECK(signaling_thread_->IsCurrent());
      certificate_ = std::move(certificate);
      generate_async_completed_ = true;
    };
  }

  bool GenerateAsyncCompleted() {
    RTC_CHECK(signaling_thread_->IsCurrent());
    if (generate_async_completed_) {
      // Reset flag so that future generation requests are not considered done.
      generate_async_completed_ = false;
      return true;
    }
    return false;
  }

 protected:
  Thread* const signaling_thread_;
  std::unique_ptr<Thread> worker_thread_;
  std::unique_ptr<RTCCertificateGenerator> generator_;
  scoped_refptr<RTCCertificate> certificate_;
  bool generate_async_completed_;
};

class RTCCertificateGeneratorTest : public ::testing::Test {
 public:
 protected:
  static constexpr TimeDelta kGenerationTimeoutMs = TimeDelta::Millis(10000);

  AutoThread main_thread_;
  RTCCertificateGeneratorFixture fixture_;
};

TEST_F(RTCCertificateGeneratorTest, GenerateECDSA) {
  EXPECT_TRUE(RTCCertificateGenerator::GenerateCertificate(KeyParams::ECDSA(),
                                                           std::nullopt));
}

TEST_F(RTCCertificateGeneratorTest, GenerateRSA) {
  EXPECT_TRUE(RTCCertificateGenerator::GenerateCertificate(KeyParams::RSA(),
                                                           std::nullopt));
}

TEST_F(RTCCertificateGeneratorTest, GenerateAsyncECDSA) {
  EXPECT_FALSE(fixture_.certificate());
  fixture_.generator()->GenerateCertificateAsync(
      KeyParams::ECDSA(), std::nullopt, fixture_.OnGenerated());
  // Until generation has completed, the certificate is null. Since this is an
  // async call, generation must not have completed until we process messages
  // posted to this thread (which is done by `EXPECT_TRUE_WAIT`).
  EXPECT_FALSE(fixture_.GenerateAsyncCompleted());
  EXPECT_FALSE(fixture_.certificate());
  EXPECT_THAT(WaitUntil([&] { return fixture_.GenerateAsyncCompleted(); },
                        ::testing::IsTrue(), {.timeout = kGenerationTimeoutMs}),
              IsRtcOk());
  EXPECT_TRUE(fixture_.certificate());
}

TEST_F(RTCCertificateGeneratorTest, GenerateWithExpires) {
  // By generating two certificates with different expiration we can compare the
  // two expiration times relative to each other without knowing the current
  // time relative to epoch, 1970-01-01T00:00:00Z. This verifies that the
  // expiration parameter is correctly used relative to the generator's clock,
  // but does not verify that this clock is relative to epoch.

  // Generate a certificate that expires immediately.
  scoped_refptr<RTCCertificate> cert_a =
      RTCCertificateGenerator::GenerateCertificate(KeyParams::ECDSA(), 0);
  EXPECT_TRUE(cert_a);

  // Generate a certificate that expires in one minute.
  const uint64_t kExpiresMs = 60000;
  scoped_refptr<RTCCertificate> cert_b =
      RTCCertificateGenerator::GenerateCertificate(KeyParams::ECDSA(),
                                                   kExpiresMs);
  EXPECT_TRUE(cert_b);

  // Verify that `cert_b` expires approximately `kExpiresMs` after `cert_a`
  // (allowing a +/- 1 second plus maximum generation time difference).
  EXPECT_GT(cert_b->Expires(), cert_a->Expires());
  uint64_t expires_diff = cert_b->Expires() - cert_a->Expires();
  EXPECT_GE(expires_diff, kExpiresMs);
  EXPECT_LE(expires_diff, kExpiresMs + 2 * kGenerationTimeoutMs.ms() + 1000);
}

TEST_F(RTCCertificateGeneratorTest, GenerateWithInvalidParamsShouldFail) {
  KeyParams invalid_params = KeyParams::RSA(0, 0);
  EXPECT_FALSE(invalid_params.IsValid());

  EXPECT_FALSE(RTCCertificateGenerator::GenerateCertificate(invalid_params,
                                                            std::nullopt));

  fixture_.generator()->GenerateCertificateAsync(invalid_params, std::nullopt,
                                                 fixture_.OnGenerated());
  EXPECT_THAT(WaitUntil([&] { return fixture_.GenerateAsyncCompleted(); },
                        ::testing::IsTrue(), {.timeout = kGenerationTimeoutMs}),
              IsRtcOk());
  EXPECT_FALSE(fixture_.certificate());
}

}  // namespace webrtc
