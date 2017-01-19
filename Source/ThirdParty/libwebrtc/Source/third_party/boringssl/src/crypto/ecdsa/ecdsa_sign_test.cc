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

#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <openssl/ec.h>
#include <openssl/ec_key.h>
#include <openssl/ecdsa.h>
#include <openssl/nid.h>

#include "../test/file_test.h"


static bssl::UniquePtr<EC_GROUP> GetCurve(FileTest *t, const char *key) {
  std::string curve_name;
  if (!t->GetAttribute(&curve_name, key)) {
    return nullptr;
  }

  if (curve_name == "P-224") {
    return bssl::UniquePtr<EC_GROUP>(EC_GROUP_new_by_curve_name(NID_secp224r1));
  }
  if (curve_name == "P-256") {
    return bssl::UniquePtr<EC_GROUP>(
        EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
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

static bool TestECDSASign(FileTest *t, void *arg) {
  bssl::UniquePtr<EC_GROUP> group = GetCurve(t, "Curve");
  bssl::UniquePtr<BIGNUM> priv_key = GetBIGNUM(t, "Private");
  bssl::UniquePtr<BIGNUM> x = GetBIGNUM(t, "X");
  bssl::UniquePtr<BIGNUM> y = GetBIGNUM(t, "Y");
  bssl::UniquePtr<BIGNUM> k = GetBIGNUM(t, "K");
  bssl::UniquePtr<BIGNUM> r = GetBIGNUM(t, "R");
  bssl::UniquePtr<BIGNUM> s = GetBIGNUM(t, "S");
  std::vector<uint8_t> digest;
  if (!group || !priv_key || !x || !y || !k || !r || !s ||
      !t->GetBytes(&digest, "Digest")) {
    return false;
  }

  bssl::UniquePtr<EC_KEY> key(EC_KEY_new());
  bssl::UniquePtr<EC_POINT> pub_key(EC_POINT_new(group.get()));
  if (!key || !pub_key ||
      !EC_KEY_set_group(key.get(), group.get()) ||
      !EC_KEY_set_private_key(key.get(), priv_key.get()) ||
      !EC_POINT_set_affine_coordinates_GFp(group.get(), pub_key.get(), x.get(),
                                           y.get(), nullptr) ||
      !EC_KEY_set_public_key(key.get(), pub_key.get()) ||
      !EC_KEY_check_key(key.get())) {
    return false;
  }

  // |ECDSA_do_sign_ex| expects |k| to already be inverted.
  bssl::UniquePtr<BN_CTX> ctx(BN_CTX_new());
  if (!ctx ||
      !BN_mod_inverse(k.get(), k.get(), EC_GROUP_get0_order(group.get()),
                      ctx.get())) {
    return false;
  }

  bssl::UniquePtr<ECDSA_SIG> sig(ECDSA_do_sign_ex(digest.data(), digest.size(), k.get(),
                                 r.get(), key.get()));
  if (!sig) {
    return false;
  }

  if (BN_cmp(r.get(), sig->r) != 0 ||
      BN_cmp(s.get(), sig->s) != 0) {
    t->PrintLine("Signature mismatch.");
    return false;
  }

  return true;
}

int main(int argc, char *argv[]) {
  CRYPTO_library_init();

  if (argc != 2) {
    fprintf(stderr, "%s <test file.txt>\n", argv[0]);
    return 1;
  }

  return FileTestMain(TestECDSASign, nullptr, argv[1]);
}
