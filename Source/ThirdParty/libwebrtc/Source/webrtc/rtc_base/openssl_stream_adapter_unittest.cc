/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/openssl_stream_adapter.h"

#include <openssl/ssl.h>

#include <cstdint>
#include <set>
#include <vector>

#include "api/field_trials.h"
#include "rtc_base/logging.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "test/create_test_field_trials.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(OpenSSLStreamAdapterTest, GetSupportedEphemeralKeyExchangeCipherGroups) {
  RTC_LOG(LS_INFO) << "OpenSSLStreamAdapter::IsBoringSsl(): "
                   << OpenSSLStreamAdapter::IsBoringSsl();
  std::set<uint16_t> expected = {
#ifdef SSL_GROUP_SECP224R1
      SSL_GROUP_SECP224R1,
#endif
#ifdef SSL_GROUP_SECP256R1
      SSL_GROUP_SECP256R1,
#endif
#ifdef SSL_GROUP_SECP384R1
      SSL_GROUP_SECP384R1,
#endif
#ifdef SSL_GROUP_SECP521R1
      SSL_GROUP_SECP521R1,
#endif
#ifdef SSL_GROUP_X25519
      SSL_GROUP_X25519,
#endif
#ifdef SSL_GROUP_X25519_MLKEM768
      SSL_GROUP_X25519_MLKEM768,
#endif
  };
  EXPECT_EQ(SSLStreamAdapter::GetSupportedEphemeralKeyExchangeCipherGroups(),
            expected);
}

TEST(OpenSSLStreamAdapterTest, GetEphemeralKeyExchangeCipherGroupName) {
#ifdef SSL_GROUP_SECP224R1
  EXPECT_EQ(*SSLStreamAdapter::GetEphemeralKeyExchangeCipherGroupName(
                SSL_GROUP_SECP224R1),
            "P-224");
#endif
#ifdef SSL_GROUP_SECP256R1
  EXPECT_EQ(*SSLStreamAdapter::GetEphemeralKeyExchangeCipherGroupName(
                SSL_GROUP_SECP256R1),
            "P-256");
#endif
#ifdef SSL_GROUP_SECP384R1
  EXPECT_EQ(*SSLStreamAdapter::GetEphemeralKeyExchangeCipherGroupName(
                SSL_GROUP_SECP384R1),
            "P-384");
#endif
#ifdef SSL_GROUP_SECP521R1
  EXPECT_EQ(*SSLStreamAdapter::GetEphemeralKeyExchangeCipherGroupName(
                SSL_GROUP_SECP521R1),
            "P-521");
#endif
#ifdef SSL_GROUP_X25519
  EXPECT_EQ(*SSLStreamAdapter::GetEphemeralKeyExchangeCipherGroupName(
                SSL_GROUP_X25519),
            "X25519");
#endif
#ifdef SSL_GROUP_X25519_MLKEM768
  EXPECT_EQ(*SSLStreamAdapter::GetEphemeralKeyExchangeCipherGroupName(
                SSL_GROUP_X25519_MLKEM768),
            "X25519MLKEM768");
#endif

  EXPECT_FALSE(
      SSLStreamAdapter::GetEphemeralKeyExchangeCipherGroupName(0).has_value());
}

TEST(OpenSSLStreamAdapterTest, GetDefaultEphemeralKeyExchangeCipherGroups) {
  std::vector<uint16_t> expected = {
#ifdef SSL_GROUP_X25519
      SSL_GROUP_X25519,
#endif
#ifdef SSL_GROUP_SECP256R1
      SSL_GROUP_SECP256R1,
#endif
#ifdef SSL_GROUP_SECP384R1
      SSL_GROUP_SECP384R1,
#endif
  };
  EXPECT_EQ(SSLStreamAdapter::GetDefaultEphemeralKeyExchangeCipherGroups(
                /* field_trials= */ nullptr),
            expected);
}

TEST(OpenSSLStreamAdapterTest,
     GetDefaultEphemeralKeyExchangeCipherGroupsWithPQC) {
  std::vector<uint16_t> expected = {
#ifdef SSL_GROUP_X25519_MLKEM768
      SSL_GROUP_X25519_MLKEM768,
#endif
#ifdef SSL_GROUP_X25519
      SSL_GROUP_X25519,
#endif
#ifdef SSL_GROUP_SECP256R1
      SSL_GROUP_SECP256R1,
#endif
#ifdef SSL_GROUP_SECP384R1
      SSL_GROUP_SECP384R1,
#endif
  };
  FieldTrials field_trials =
      CreateTestFieldTrials("WebRTC-EnableDtlsPqc/Enabled/");
  EXPECT_EQ(SSLStreamAdapter::GetDefaultEphemeralKeyExchangeCipherGroups(
                &field_trials),
            expected);
}

}  // namespace
}  // namespace webrtc