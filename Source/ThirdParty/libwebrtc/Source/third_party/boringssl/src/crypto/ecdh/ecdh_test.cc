/* Copyright (c) 2016, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include <stdio.h>

#include <vector>

#include <gtest/gtest.h>

#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/ec.h>
#include <openssl/ec_key.h>
#include <openssl/ecdh.h>
#include <openssl/nid.h>

#include "../test/file_test.h"
#include "../test/test_util.h"


static bssl::UniquePtr<EC_GROUP> GetCurve(FileTest *t, const char *key) {
  std::string curve_name;
  if (!t->GetAttribute(&curve_name, key)) {
    return nullptr;
  }

  if (curve_name == "P-224") {
    return bssl::UniquePtr<EC_GROUP>(EC_GROUP_new_by_curve_name(NID_secp224r1));
  }
  if (curve_name == "P-256") {
    return bssl::UniquePtr<EC_GROUP>(EC_GROUP_new_by_curve_name(
        NID_X9_62_prime256v1));
  }
  if (curve_name == "P-384") {
    return bssl::UniquePtr<EC_GROUP>(EC_GROUP_new_by_curve_name(NID_secp384r1));
  }
  if (curve_name == "P-521") {
    return bssl::UniquePtr<EC_GROUP>(EC_GROUP_new_by_curve_name(NID_secp521r1));
  }

  t->PrintLine("Unknown curve '%s'", curve_name.c_str());
  return nullptr;
}

static bssl::UniquePtr<BIGNUM> GetBIGNUM(FileTest *t, const char *key) {
  std::vector<uint8_t> bytes;
  if (!t->GetBytes(&bytes, key)) {
    return nullptr;
  }

  return bssl::UniquePtr<BIGNUM>(BN_bin2bn(bytes.data(), bytes.size(), nullptr));
}

TEST(ECDHTest, TestVectors) {
  FileTestGTest("crypto/ecdh/ecdh_tests.txt", [](FileTest *t) {
    bssl::UniquePtr<EC_GROUP> group = GetCurve(t, "Curve");
    ASSERT_TRUE(group);
    bssl::UniquePtr<BIGNUM> priv_key = GetBIGNUM(t, "Private");
    ASSERT_TRUE(priv_key);
    bssl::UniquePtr<BIGNUM> x = GetBIGNUM(t, "X");
    ASSERT_TRUE(x);
    bssl::UniquePtr<BIGNUM> y = GetBIGNUM(t, "Y");
    ASSERT_TRUE(y);
    bssl::UniquePtr<BIGNUM> peer_x = GetBIGNUM(t, "PeerX");
    ASSERT_TRUE(peer_x);
    bssl::UniquePtr<BIGNUM> peer_y = GetBIGNUM(t, "PeerY");
    ASSERT_TRUE(peer_y);
    std::vector<uint8_t> z;
    ASSERT_TRUE(t->GetBytes(&z, "Z"));

    bssl::UniquePtr<EC_KEY> key(EC_KEY_new());
    ASSERT_TRUE(key);
    bssl::UniquePtr<EC_POINT> pub_key(EC_POINT_new(group.get()));
    ASSERT_TRUE(pub_key);
    bssl::UniquePtr<EC_POINT> peer_pub_key(EC_POINT_new(group.get()));
    ASSERT_TRUE(peer_pub_key);
    ASSERT_TRUE(EC_KEY_set_group(key.get(), group.get()));
    ASSERT_TRUE(EC_KEY_set_private_key(key.get(), priv_key.get()));
    ASSERT_TRUE(EC_POINT_set_affine_coordinates_GFp(group.get(), pub_key.get(),
                                                    x.get(), y.get(), nullptr));
    ASSERT_TRUE(EC_POINT_set_affine_coordinates_GFp(
        group.get(), peer_pub_key.get(), peer_x.get(), peer_y.get(), nullptr));
    ASSERT_TRUE(EC_KEY_set_public_key(key.get(), pub_key.get()));
    ASSERT_TRUE(EC_KEY_check_key(key.get()));

    std::vector<uint8_t> actual_z;
    // Make |actual_z| larger than expected to ensure |ECDH_compute_key| returns
    // the right amount of data.
    actual_z.resize(z.size() + 1);
    int ret = ECDH_compute_key(actual_z.data(), actual_z.size(),
                               peer_pub_key.get(), key.get(), nullptr);
    ASSERT_GE(ret, 0);
    EXPECT_EQ(Bytes(z), Bytes(actual_z.data(), static_cast<size_t>(ret)));

    // Test |ECDH_compute_key| truncates.
    actual_z.resize(z.size() - 1);
    ret = ECDH_compute_key(actual_z.data(), actual_z.size(), peer_pub_key.get(),
                           key.get(), nullptr);
    ASSERT_GE(ret, 0);
    EXPECT_EQ(Bytes(z.data(), z.size() - 1),
              Bytes(actual_z.data(), static_cast<size_t>(ret)));
  });
}
