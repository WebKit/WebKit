/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/srtp_filter.h"

#include <string.h>

#include "api/crypto_params.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "test/gtest.h"

using cricket::CryptoParams;
using cricket::CS_LOCAL;
using cricket::CS_REMOTE;

namespace rtc {

static const char kTestKeyParams1[] =
    "inline:WVNfX19zZW1jdGwgKCkgewkyMjA7fQp9CnVubGVz";
static const char kTestKeyParams2[] =
    "inline:PS1uQCVeeCFCanVmcjkpPywjNWhcYD0mXXtxaVBR";
static const char kTestKeyParams3[] =
    "inline:1234X19zZW1jdGwgKCkgewkyMjA7fQp9CnVubGVz";
static const char kTestKeyParams4[] =
    "inline:4567QCVeeCFCanVmcjkpPywjNWhcYD0mXXtxaVBR";
static const char kTestKeyParamsGcm1[] =
    "inline:e166KFlKzJsGW0d5apX+rrI05vxbrvMJEzFI14aTDCa63IRTlLK4iH66uOI=";
static const char kTestKeyParamsGcm2[] =
    "inline:6X0oCd55zfz4VgtOwsuqcFq61275PDYN5uwuu3p7ZUHbfUY2FMpdP4m2PEo=";
static const char kTestKeyParamsGcm3[] =
    "inline:YKlABGZWMgX32xuMotrG0v0T7G83veegaVzubQ==";
static const char kTestKeyParamsGcm4[] =
    "inline:gJ6tWoUym2v+/F6xjr7xaxiS3QbJJozl3ZD/0A==";
static const cricket::CryptoParams kTestCryptoParams1(1,
                                                      "AES_CM_128_HMAC_SHA1_80",
                                                      kTestKeyParams1,
                                                      "");
static const cricket::CryptoParams kTestCryptoParams2(1,
                                                      "AES_CM_128_HMAC_SHA1_80",
                                                      kTestKeyParams2,
                                                      "");
static const cricket::CryptoParams kTestCryptoParamsGcm1(1,
                                                         "AEAD_AES_256_GCM",
                                                         kTestKeyParamsGcm1,
                                                         "");
static const cricket::CryptoParams kTestCryptoParamsGcm2(1,
                                                         "AEAD_AES_256_GCM",
                                                         kTestKeyParamsGcm2,
                                                         "");
static const cricket::CryptoParams kTestCryptoParamsGcm3(1,
                                                         "AEAD_AES_128_GCM",
                                                         kTestKeyParamsGcm3,
                                                         "");
static const cricket::CryptoParams kTestCryptoParamsGcm4(1,
                                                         "AEAD_AES_128_GCM",
                                                         kTestKeyParamsGcm4,
                                                         "");

class SrtpFilterTest : public ::testing::Test {
 protected:
  SrtpFilterTest() {}
  static std::vector<CryptoParams> MakeVector(const CryptoParams& params) {
    std::vector<CryptoParams> vec;
    vec.push_back(params);
    return vec;
  }

  void TestSetParams(const std::vector<CryptoParams>& params1,
                     const std::vector<CryptoParams>& params2) {
    EXPECT_TRUE(f1_.SetOffer(params1, CS_LOCAL));
    EXPECT_TRUE(f2_.SetOffer(params1, CS_REMOTE));
    EXPECT_FALSE(f1_.IsActive());
    EXPECT_FALSE(f2_.IsActive());
    EXPECT_TRUE(f2_.SetAnswer(params2, CS_LOCAL));
    EXPECT_TRUE(f1_.SetAnswer(params2, CS_REMOTE));
    EXPECT_TRUE(f1_.IsActive());
    EXPECT_TRUE(f2_.IsActive());
  }

  void VerifyKeysAreEqual(ArrayView<const uint8_t> key1,
                          ArrayView<const uint8_t> key2) {
    EXPECT_EQ(key1.size(), key2.size());
    EXPECT_EQ(0, memcmp(key1.data(), key2.data(), key1.size()));
  }

  void VerifyCryptoParamsMatch(const std::string& cs1, const std::string& cs2) {
    EXPECT_EQ(rtc::SrtpCryptoSuiteFromName(cs1), f1_.send_crypto_suite());
    EXPECT_EQ(rtc::SrtpCryptoSuiteFromName(cs2), f2_.send_crypto_suite());
    VerifyKeysAreEqual(f1_.send_key(), f2_.recv_key());
    VerifyKeysAreEqual(f2_.send_key(), f1_.recv_key());
  }

  cricket::SrtpFilter f1_;
  cricket::SrtpFilter f2_;
};

// Test that we can set up the session and keys properly.
TEST_F(SrtpFilterTest, TestGoodSetupOneCryptoSuite) {
  EXPECT_TRUE(f1_.SetOffer(MakeVector(kTestCryptoParams1), CS_LOCAL));
  EXPECT_FALSE(f1_.IsActive());
  EXPECT_TRUE(f1_.SetAnswer(MakeVector(kTestCryptoParams2), CS_REMOTE));
  EXPECT_TRUE(f1_.IsActive());
}

TEST_F(SrtpFilterTest, TestGoodSetupOneCryptoSuiteGcm) {
  EXPECT_TRUE(f1_.SetOffer(MakeVector(kTestCryptoParamsGcm1), CS_LOCAL));
  EXPECT_FALSE(f1_.IsActive());
  EXPECT_TRUE(f1_.SetAnswer(MakeVector(kTestCryptoParamsGcm2), CS_REMOTE));
  EXPECT_TRUE(f1_.IsActive());
}

// Test that we can set up things with multiple params.
TEST_F(SrtpFilterTest, TestGoodSetupMultipleCryptoSuites) {
  std::vector<CryptoParams> offer(MakeVector(kTestCryptoParams1));
  std::vector<CryptoParams> answer(MakeVector(kTestCryptoParams2));
  offer.push_back(kTestCryptoParams1);
  offer[1].tag = 2;
  offer[1].crypto_suite = kCsAesCm128HmacSha1_32;
  answer[0].tag = 2;
  answer[0].crypto_suite = kCsAesCm128HmacSha1_32;
  EXPECT_TRUE(f1_.SetOffer(offer, CS_LOCAL));
  EXPECT_FALSE(f1_.IsActive());
  EXPECT_TRUE(f1_.SetAnswer(answer, CS_REMOTE));
  EXPECT_TRUE(f1_.IsActive());
}

TEST_F(SrtpFilterTest, TestGoodSetupMultipleCryptoSuitesGcm) {
  std::vector<CryptoParams> offer(MakeVector(kTestCryptoParamsGcm1));
  std::vector<CryptoParams> answer(MakeVector(kTestCryptoParamsGcm3));
  offer.push_back(kTestCryptoParamsGcm4);
  offer[1].tag = 2;
  answer[0].tag = 2;
  EXPECT_TRUE(f1_.SetOffer(offer, CS_LOCAL));
  EXPECT_FALSE(f1_.IsActive());
  EXPECT_TRUE(f1_.SetAnswer(answer, CS_REMOTE));
  EXPECT_TRUE(f1_.IsActive());
}

// Test that we handle the cases where crypto is not desired.
TEST_F(SrtpFilterTest, TestGoodSetupNoCryptoSuites) {
  std::vector<CryptoParams> offer, answer;
  EXPECT_TRUE(f1_.SetOffer(offer, CS_LOCAL));
  EXPECT_TRUE(f1_.SetAnswer(answer, CS_REMOTE));
  EXPECT_FALSE(f1_.IsActive());
}

// Test that we handle the cases where crypto is not desired by the remote side.
TEST_F(SrtpFilterTest, TestGoodSetupNoAnswerCryptoSuites) {
  std::vector<CryptoParams> answer;
  EXPECT_TRUE(f1_.SetOffer(MakeVector(kTestCryptoParams1), CS_LOCAL));
  EXPECT_TRUE(f1_.SetAnswer(answer, CS_REMOTE));
  EXPECT_FALSE(f1_.IsActive());
}

// Test that we fail if we call the functions the wrong way.
TEST_F(SrtpFilterTest, TestBadSetup) {
  std::vector<CryptoParams> offer(MakeVector(kTestCryptoParams1));
  std::vector<CryptoParams> answer(MakeVector(kTestCryptoParams2));
  EXPECT_FALSE(f1_.SetAnswer(answer, CS_LOCAL));
  EXPECT_FALSE(f1_.SetAnswer(answer, CS_REMOTE));
  EXPECT_TRUE(f1_.SetOffer(offer, CS_LOCAL));
  EXPECT_FALSE(f1_.SetAnswer(answer, CS_LOCAL));
  EXPECT_FALSE(f1_.IsActive());
}

// Test that we can set offer multiple times from the same source.
TEST_F(SrtpFilterTest, TestGoodSetupMultipleOffers) {
  EXPECT_TRUE(f1_.SetOffer(MakeVector(kTestCryptoParams1), CS_LOCAL));
  EXPECT_TRUE(f1_.SetOffer(MakeVector(kTestCryptoParams2), CS_LOCAL));
  EXPECT_FALSE(f1_.IsActive());
  EXPECT_TRUE(f1_.SetAnswer(MakeVector(kTestCryptoParams2), CS_REMOTE));
  EXPECT_TRUE(f1_.IsActive());
  EXPECT_TRUE(f1_.SetOffer(MakeVector(kTestCryptoParams1), CS_LOCAL));
  EXPECT_TRUE(f1_.SetOffer(MakeVector(kTestCryptoParams2), CS_LOCAL));
  EXPECT_TRUE(f1_.SetAnswer(MakeVector(kTestCryptoParams2), CS_REMOTE));

  EXPECT_TRUE(f2_.SetOffer(MakeVector(kTestCryptoParams1), CS_REMOTE));
  EXPECT_TRUE(f2_.SetOffer(MakeVector(kTestCryptoParams2), CS_REMOTE));
  EXPECT_FALSE(f2_.IsActive());
  EXPECT_TRUE(f2_.SetAnswer(MakeVector(kTestCryptoParams2), CS_LOCAL));
  EXPECT_TRUE(f2_.IsActive());
  EXPECT_TRUE(f2_.SetOffer(MakeVector(kTestCryptoParams1), CS_REMOTE));
  EXPECT_TRUE(f2_.SetOffer(MakeVector(kTestCryptoParams2), CS_REMOTE));
  EXPECT_TRUE(f2_.SetAnswer(MakeVector(kTestCryptoParams2), CS_LOCAL));
}
// Test that we can't set offer multiple times from different sources.
TEST_F(SrtpFilterTest, TestBadSetupMultipleOffers) {
  EXPECT_TRUE(f1_.SetOffer(MakeVector(kTestCryptoParams1), CS_LOCAL));
  EXPECT_FALSE(f1_.SetOffer(MakeVector(kTestCryptoParams2), CS_REMOTE));
  EXPECT_FALSE(f1_.IsActive());
  EXPECT_TRUE(f1_.SetAnswer(MakeVector(kTestCryptoParams1), CS_REMOTE));
  EXPECT_TRUE(f1_.IsActive());
  EXPECT_TRUE(f1_.SetOffer(MakeVector(kTestCryptoParams2), CS_LOCAL));
  EXPECT_FALSE(f1_.SetOffer(MakeVector(kTestCryptoParams1), CS_REMOTE));
  EXPECT_TRUE(f1_.SetAnswer(MakeVector(kTestCryptoParams2), CS_REMOTE));

  EXPECT_TRUE(f2_.SetOffer(MakeVector(kTestCryptoParams2), CS_REMOTE));
  EXPECT_FALSE(f2_.SetOffer(MakeVector(kTestCryptoParams1), CS_LOCAL));
  EXPECT_FALSE(f2_.IsActive());
  EXPECT_TRUE(f2_.SetAnswer(MakeVector(kTestCryptoParams2), CS_LOCAL));
  EXPECT_TRUE(f2_.IsActive());
  EXPECT_TRUE(f2_.SetOffer(MakeVector(kTestCryptoParams2), CS_REMOTE));
  EXPECT_FALSE(f2_.SetOffer(MakeVector(kTestCryptoParams1), CS_LOCAL));
  EXPECT_TRUE(f2_.SetAnswer(MakeVector(kTestCryptoParams2), CS_LOCAL));
}

// Test that we fail if we have params in the answer when none were offered.
TEST_F(SrtpFilterTest, TestNoAnswerCryptoSuites) {
  std::vector<CryptoParams> offer;
  EXPECT_TRUE(f1_.SetOffer(offer, CS_LOCAL));
  EXPECT_FALSE(f1_.SetAnswer(MakeVector(kTestCryptoParams2), CS_REMOTE));
  EXPECT_FALSE(f1_.IsActive());
}

// Test that we fail if we have too many params in our answer.
TEST_F(SrtpFilterTest, TestMultipleAnswerCryptoSuites) {
  std::vector<CryptoParams> answer(MakeVector(kTestCryptoParams2));
  answer.push_back(kTestCryptoParams2);
  answer[1].tag = 2;
  answer[1].crypto_suite = kCsAesCm128HmacSha1_32;
  EXPECT_TRUE(f1_.SetOffer(MakeVector(kTestCryptoParams1), CS_LOCAL));
  EXPECT_FALSE(f1_.SetAnswer(answer, CS_REMOTE));
  EXPECT_FALSE(f1_.IsActive());
}

// Test that we fail if we don't support the crypto suite.
TEST_F(SrtpFilterTest, TestInvalidCryptoSuite) {
  std::vector<CryptoParams> offer(MakeVector(kTestCryptoParams1));
  std::vector<CryptoParams> answer(MakeVector(kTestCryptoParams2));
  offer[0].crypto_suite = answer[0].crypto_suite = "FOO";
  EXPECT_TRUE(f1_.SetOffer(offer, CS_LOCAL));
  EXPECT_FALSE(f1_.SetAnswer(answer, CS_REMOTE));
  EXPECT_FALSE(f1_.IsActive());
}

// Test that we fail if we can't agree on a tag.
TEST_F(SrtpFilterTest, TestNoMatchingTag) {
  std::vector<CryptoParams> offer(MakeVector(kTestCryptoParams1));
  std::vector<CryptoParams> answer(MakeVector(kTestCryptoParams2));
  answer[0].tag = 99;
  EXPECT_TRUE(f1_.SetOffer(offer, CS_LOCAL));
  EXPECT_FALSE(f1_.SetAnswer(answer, CS_REMOTE));
  EXPECT_FALSE(f1_.IsActive());
}

// Test that we fail if we can't agree on a crypto suite.
TEST_F(SrtpFilterTest, TestNoMatchingCryptoSuite) {
  std::vector<CryptoParams> offer(MakeVector(kTestCryptoParams1));
  std::vector<CryptoParams> answer(MakeVector(kTestCryptoParams2));
  answer[0].tag = 2;
  answer[0].crypto_suite = "FOO";
  EXPECT_TRUE(f1_.SetOffer(offer, CS_LOCAL));
  EXPECT_FALSE(f1_.SetAnswer(answer, CS_REMOTE));
  EXPECT_FALSE(f1_.IsActive());
}

// Test that we fail keys with bad base64 content.
TEST_F(SrtpFilterTest, TestInvalidKeyData) {
  std::vector<CryptoParams> offer(MakeVector(kTestCryptoParams1));
  std::vector<CryptoParams> answer(MakeVector(kTestCryptoParams2));
  answer[0].key_params = "inline:!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
  EXPECT_TRUE(f1_.SetOffer(offer, CS_LOCAL));
  EXPECT_FALSE(f1_.SetAnswer(answer, CS_REMOTE));
  EXPECT_FALSE(f1_.IsActive());
}

// Test that we fail keys with the wrong key-method.
TEST_F(SrtpFilterTest, TestWrongKeyMethod) {
  std::vector<CryptoParams> offer(MakeVector(kTestCryptoParams1));
  std::vector<CryptoParams> answer(MakeVector(kTestCryptoParams2));
  answer[0].key_params = "outline:PS1uQCVeeCFCanVmcjkpPywjNWhcYD0mXXtxaVBR";
  EXPECT_TRUE(f1_.SetOffer(offer, CS_LOCAL));
  EXPECT_FALSE(f1_.SetAnswer(answer, CS_REMOTE));
  EXPECT_FALSE(f1_.IsActive());
}

// Test that we fail keys of the wrong length.
TEST_F(SrtpFilterTest, TestKeyTooShort) {
  std::vector<CryptoParams> offer(MakeVector(kTestCryptoParams1));
  std::vector<CryptoParams> answer(MakeVector(kTestCryptoParams2));
  answer[0].key_params = "inline:PS1uQCVeeCFCanVmcjkpPywjNWhcYD0mXXtx";
  EXPECT_TRUE(f1_.SetOffer(offer, CS_LOCAL));
  EXPECT_FALSE(f1_.SetAnswer(answer, CS_REMOTE));
  EXPECT_FALSE(f1_.IsActive());
}

// Test that we fail keys of the wrong length.
TEST_F(SrtpFilterTest, TestKeyTooLong) {
  std::vector<CryptoParams> offer(MakeVector(kTestCryptoParams1));
  std::vector<CryptoParams> answer(MakeVector(kTestCryptoParams2));
  answer[0].key_params = "inline:PS1uQCVeeCFCanVmcjkpPywjNWhcYD0mXXtxaVBRABCD";
  EXPECT_TRUE(f1_.SetOffer(offer, CS_LOCAL));
  EXPECT_FALSE(f1_.SetAnswer(answer, CS_REMOTE));
  EXPECT_FALSE(f1_.IsActive());
}

// Test that we fail keys with lifetime or MKI set (since we don't support)
TEST_F(SrtpFilterTest, TestUnsupportedOptions) {
  std::vector<CryptoParams> offer(MakeVector(kTestCryptoParams1));
  std::vector<CryptoParams> answer(MakeVector(kTestCryptoParams2));
  answer[0].key_params =
      "inline:PS1uQCVeeCFCanVmcjkpPywjNWhcYD0mXXtxaVBR|2^20|1:4";
  EXPECT_TRUE(f1_.SetOffer(offer, CS_LOCAL));
  EXPECT_FALSE(f1_.SetAnswer(answer, CS_REMOTE));
  EXPECT_FALSE(f1_.IsActive());
}

// Test that we can encrypt/decrypt after negotiating AES_CM_128_HMAC_SHA1_80.
TEST_F(SrtpFilterTest, TestProtect_AES_CM_128_HMAC_SHA1_80) {
  std::vector<CryptoParams> offer(MakeVector(kTestCryptoParams1));
  std::vector<CryptoParams> answer(MakeVector(kTestCryptoParams2));
  offer.push_back(kTestCryptoParams1);
  offer[1].tag = 2;
  offer[1].crypto_suite = kCsAesCm128HmacSha1_32;
  TestSetParams(offer, answer);
  VerifyCryptoParamsMatch(kCsAesCm128HmacSha1_80, kCsAesCm128HmacSha1_80);
}

// Test that we can encrypt/decrypt after negotiating AES_CM_128_HMAC_SHA1_32.
TEST_F(SrtpFilterTest, TestProtect_AES_CM_128_HMAC_SHA1_32) {
  std::vector<CryptoParams> offer(MakeVector(kTestCryptoParams1));
  std::vector<CryptoParams> answer(MakeVector(kTestCryptoParams2));
  offer.push_back(kTestCryptoParams1);
  offer[1].tag = 2;
  offer[1].crypto_suite = kCsAesCm128HmacSha1_32;
  answer[0].tag = 2;
  answer[0].crypto_suite = kCsAesCm128HmacSha1_32;
  TestSetParams(offer, answer);
  VerifyCryptoParamsMatch(kCsAesCm128HmacSha1_32, kCsAesCm128HmacSha1_32);
}

// Test that we can change encryption parameters.
TEST_F(SrtpFilterTest, TestChangeParameters) {
  std::vector<CryptoParams> offer(MakeVector(kTestCryptoParams1));
  std::vector<CryptoParams> answer(MakeVector(kTestCryptoParams2));

  TestSetParams(offer, answer);
  VerifyCryptoParamsMatch(kCsAesCm128HmacSha1_80, kCsAesCm128HmacSha1_80);

  // Change the key parameters and crypto_suite.
  offer[0].key_params = kTestKeyParams3;
  offer[0].crypto_suite = kCsAesCm128HmacSha1_32;
  answer[0].key_params = kTestKeyParams4;
  answer[0].crypto_suite = kCsAesCm128HmacSha1_32;

  EXPECT_TRUE(f1_.SetOffer(offer, CS_LOCAL));
  EXPECT_TRUE(f2_.SetOffer(offer, CS_REMOTE));
  EXPECT_TRUE(f1_.IsActive());
  EXPECT_TRUE(f1_.IsActive());

  // Test that the old keys are valid until the negotiation is complete.
  VerifyCryptoParamsMatch(kCsAesCm128HmacSha1_80, kCsAesCm128HmacSha1_80);

  // Complete the negotiation and test that we can still understand each other.
  EXPECT_TRUE(f2_.SetAnswer(answer, CS_LOCAL));
  EXPECT_TRUE(f1_.SetAnswer(answer, CS_REMOTE));

  VerifyCryptoParamsMatch(kCsAesCm128HmacSha1_32, kCsAesCm128HmacSha1_32);
}

// Test that we can send and receive provisional answers with crypto enabled.
// Also test that we can change the crypto.
TEST_F(SrtpFilterTest, TestProvisionalAnswer) {
  std::vector<CryptoParams> offer(MakeVector(kTestCryptoParams1));
  offer.push_back(kTestCryptoParams1);
  offer[1].tag = 2;
  offer[1].crypto_suite = kCsAesCm128HmacSha1_32;
  std::vector<CryptoParams> answer(MakeVector(kTestCryptoParams2));

  EXPECT_TRUE(f1_.SetOffer(offer, CS_LOCAL));
  EXPECT_TRUE(f2_.SetOffer(offer, CS_REMOTE));
  EXPECT_FALSE(f1_.IsActive());
  EXPECT_FALSE(f2_.IsActive());
  EXPECT_TRUE(f2_.SetProvisionalAnswer(answer, CS_LOCAL));
  EXPECT_TRUE(f1_.SetProvisionalAnswer(answer, CS_REMOTE));
  EXPECT_TRUE(f1_.IsActive());
  EXPECT_TRUE(f2_.IsActive());
  VerifyCryptoParamsMatch(kCsAesCm128HmacSha1_80, kCsAesCm128HmacSha1_80);

  answer[0].key_params = kTestKeyParams4;
  answer[0].tag = 2;
  answer[0].crypto_suite = kCsAesCm128HmacSha1_32;
  EXPECT_TRUE(f2_.SetAnswer(answer, CS_LOCAL));
  EXPECT_TRUE(f1_.SetAnswer(answer, CS_REMOTE));
  EXPECT_TRUE(f1_.IsActive());
  EXPECT_TRUE(f2_.IsActive());
  VerifyCryptoParamsMatch(kCsAesCm128HmacSha1_32, kCsAesCm128HmacSha1_32);
}

// Test that a provisional answer doesn't need to contain a crypto.
TEST_F(SrtpFilterTest, TestProvisionalAnswerWithoutCrypto) {
  std::vector<CryptoParams> offer(MakeVector(kTestCryptoParams1));
  std::vector<CryptoParams> answer;

  EXPECT_TRUE(f1_.SetOffer(offer, CS_LOCAL));
  EXPECT_TRUE(f2_.SetOffer(offer, CS_REMOTE));
  EXPECT_FALSE(f1_.IsActive());
  EXPECT_FALSE(f2_.IsActive());
  EXPECT_TRUE(f2_.SetProvisionalAnswer(answer, CS_LOCAL));
  EXPECT_TRUE(f1_.SetProvisionalAnswer(answer, CS_REMOTE));
  EXPECT_FALSE(f1_.IsActive());
  EXPECT_FALSE(f2_.IsActive());

  answer.push_back(kTestCryptoParams2);
  EXPECT_TRUE(f2_.SetAnswer(answer, CS_LOCAL));
  EXPECT_TRUE(f1_.SetAnswer(answer, CS_REMOTE));
  EXPECT_TRUE(f1_.IsActive());
  EXPECT_TRUE(f2_.IsActive());
  VerifyCryptoParamsMatch(kCsAesCm128HmacSha1_80, kCsAesCm128HmacSha1_80);
}

// Test that if we get a new local offer after a provisional answer
// with no crypto, that we are in an inactive state.
TEST_F(SrtpFilterTest, TestLocalOfferAfterProvisionalAnswerWithoutCrypto) {
  std::vector<CryptoParams> offer(MakeVector(kTestCryptoParams1));
  std::vector<CryptoParams> answer;

  EXPECT_TRUE(f1_.SetOffer(offer, CS_LOCAL));
  EXPECT_TRUE(f2_.SetOffer(offer, CS_REMOTE));
  EXPECT_TRUE(f1_.SetProvisionalAnswer(answer, CS_REMOTE));
  EXPECT_TRUE(f2_.SetProvisionalAnswer(answer, CS_LOCAL));
  EXPECT_FALSE(f1_.IsActive());
  EXPECT_FALSE(f2_.IsActive());
  // The calls to set an offer after a provisional answer fail, so the
  // state doesn't change.
  EXPECT_FALSE(f1_.SetOffer(offer, CS_LOCAL));
  EXPECT_FALSE(f2_.SetOffer(offer, CS_REMOTE));
  EXPECT_FALSE(f1_.IsActive());
  EXPECT_FALSE(f2_.IsActive());

  answer.push_back(kTestCryptoParams2);
  EXPECT_TRUE(f2_.SetAnswer(answer, CS_LOCAL));
  EXPECT_TRUE(f1_.SetAnswer(answer, CS_REMOTE));
  EXPECT_TRUE(f1_.IsActive());
  EXPECT_TRUE(f2_.IsActive());
  VerifyCryptoParamsMatch(kCsAesCm128HmacSha1_80, kCsAesCm128HmacSha1_80);
}

// Test that we can disable encryption.
TEST_F(SrtpFilterTest, TestDisableEncryption) {
  std::vector<CryptoParams> offer(MakeVector(kTestCryptoParams1));
  std::vector<CryptoParams> answer(MakeVector(kTestCryptoParams2));

  TestSetParams(offer, answer);
  VerifyCryptoParamsMatch(kCsAesCm128HmacSha1_80, kCsAesCm128HmacSha1_80);

  offer.clear();
  answer.clear();
  EXPECT_TRUE(f1_.SetOffer(offer, CS_LOCAL));
  EXPECT_TRUE(f2_.SetOffer(offer, CS_REMOTE));
  EXPECT_TRUE(f1_.IsActive());
  EXPECT_TRUE(f2_.IsActive());

  // Test that the old keys are valid until the negotiation is complete.
  VerifyCryptoParamsMatch(kCsAesCm128HmacSha1_80, kCsAesCm128HmacSha1_80);

  // Complete the negotiation.
  EXPECT_TRUE(f2_.SetAnswer(answer, CS_LOCAL));
  EXPECT_TRUE(f1_.SetAnswer(answer, CS_REMOTE));

  EXPECT_FALSE(f1_.IsActive());
  EXPECT_FALSE(f2_.IsActive());
}

}  // namespace rtc
