/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/sframe/sframe_types.h"
#include "modules/sframe/sframe_encryptor.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr uint64_t kKeyId = 7;
const std::vector<uint8_t> kKeyMaterial = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                                           0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
                                           0x0c, 0x0d, 0x0e, 0x0f};
const std::vector<uint8_t> kPlaintext = {0xde, 0xad, 0xbe, 0xef, 0x01,
                                         0x02, 0x03, 0x04, 0x05, 0x06};

// TODO(webrtc:479862368): Add SframeDecryptor and couple it with the encryptor.
class SframeEncryptorDecryptorTest : public ::testing::Test {
 protected:
  SframeEncryptorDecryptorTest()
      : encryptor_(
            SframeEncryptor::Create(SframeMode::kPerFrame,
                                    SframeCipherSuite::kAes128GcmSha256_128)) {}

  scoped_refptr<SframeEncryptor> encryptor_;
};

TEST_F(SframeEncryptorDecryptorTest, SetEncryptionKeySucceeds) {
  EXPECT_TRUE(encryptor_->SetEncryptionKey(kKeyId, kKeyMaterial).ok());
}

TEST_F(SframeEncryptorDecryptorTest, EncryptProducesCiphertext) {
  ASSERT_TRUE(encryptor_->SetEncryptionKey(kKeyId, kKeyMaterial).ok());

  size_t max_ct_size = encryptor_->GetMaxCiphertextByteSize(kPlaintext.size());
  std::vector<uint8_t> ciphertext(max_ct_size);

  auto result = encryptor_->Encrypt(kPlaintext, /*additional_data=*/{},
                                    std::span<uint8_t>(ciphertext));
  ASSERT_TRUE(result.ok());
  EXPECT_GT(result.value(), kPlaintext.size());
}

TEST_F(SframeEncryptorDecryptorTest, EncryptFailsWithoutKey) {
  // No key set — encryption should fail with INVALID_STATE.
  size_t max_ct_size = encryptor_->GetMaxCiphertextByteSize(kPlaintext.size());
  std::vector<uint8_t> ciphertext(max_ct_size);
  auto result = encryptor_->Encrypt(kPlaintext, /*additional_data=*/{},
                                    std::span<uint8_t>(ciphertext));
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error().type(), RTCErrorType::INVALID_STATE);
}

TEST_F(SframeEncryptorDecryptorTest, MultipleKeyRotation) {
  constexpr uint64_t kKeyId2 = 42;
  const std::vector<uint8_t> key2 = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
                                     0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b,
                                     0x1c, 0x1d, 0x1e, 0x1f};

  ASSERT_TRUE(encryptor_->SetEncryptionKey(kKeyId, kKeyMaterial).ok());
  size_t max_ct_size = encryptor_->GetMaxCiphertextByteSize(kPlaintext.size());
  std::vector<uint8_t> ct1(max_ct_size);
  ASSERT_TRUE(
      encryptor_
          ->Encrypt(kPlaintext, /*additional_data=*/{}, std::span<uint8_t>(ct1))
          .ok());

  // Rotate to second key and encrypt again.
  ASSERT_TRUE(encryptor_->SetEncryptionKey(kKeyId2, key2).ok());
  std::vector<uint8_t> ct2(max_ct_size);
  ASSERT_TRUE(
      encryptor_
          ->Encrypt(kPlaintext, /*additional_data=*/{}, std::span<uint8_t>(ct2))
          .ok());
  EXPECT_NE(ct1, ct2);
}

TEST_F(SframeEncryptorDecryptorTest, GetMaxCiphertextByteSizeIsLarger) {
  EXPECT_GT(encryptor_->GetMaxCiphertextByteSize(100), 100u);
}

}  // namespace
}  // namespace webrtc
