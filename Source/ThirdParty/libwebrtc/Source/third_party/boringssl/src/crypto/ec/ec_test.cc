/* Copyright (c) 2014, Google Inc.
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
#include <string.h>

#include <vector>

#include <openssl/bn.h>
#include <openssl/bytestring.h>
#include <openssl/crypto.h>
#include <openssl/ec_key.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/nid.h>


namespace bssl {

// kECKeyWithoutPublic is an ECPrivateKey with the optional publicKey field
// omitted.
static const uint8_t kECKeyWithoutPublic[] = {
  0x30, 0x31, 0x02, 0x01, 0x01, 0x04, 0x20, 0xc6, 0xc1, 0xaa, 0xda, 0x15, 0xb0,
  0x76, 0x61, 0xf8, 0x14, 0x2c, 0x6c, 0xaf, 0x0f, 0xdb, 0x24, 0x1a, 0xff, 0x2e,
  0xfe, 0x46, 0xc0, 0x93, 0x8b, 0x74, 0xf2, 0xbc, 0xc5, 0x30, 0x52, 0xb0, 0x77,
  0xa0, 0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07,
};

// kECKeySpecifiedCurve is the above key with P-256's parameters explicitly
// spelled out rather than using a named curve.
static const uint8_t kECKeySpecifiedCurve[] = {
    0x30, 0x82, 0x01, 0x22, 0x02, 0x01, 0x01, 0x04, 0x20, 0xc6, 0xc1, 0xaa,
    0xda, 0x15, 0xb0, 0x76, 0x61, 0xf8, 0x14, 0x2c, 0x6c, 0xaf, 0x0f, 0xdb,
    0x24, 0x1a, 0xff, 0x2e, 0xfe, 0x46, 0xc0, 0x93, 0x8b, 0x74, 0xf2, 0xbc,
    0xc5, 0x30, 0x52, 0xb0, 0x77, 0xa0, 0x81, 0xfa, 0x30, 0x81, 0xf7, 0x02,
    0x01, 0x01, 0x30, 0x2c, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x01,
    0x01, 0x02, 0x21, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x30, 0x5b, 0x04, 0x20, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc,
    0x04, 0x20, 0x5a, 0xc6, 0x35, 0xd8, 0xaa, 0x3a, 0x93, 0xe7, 0xb3, 0xeb,
    0xbd, 0x55, 0x76, 0x98, 0x86, 0xbc, 0x65, 0x1d, 0x06, 0xb0, 0xcc, 0x53,
    0xb0, 0xf6, 0x3b, 0xce, 0x3c, 0x3e, 0x27, 0xd2, 0x60, 0x4b, 0x03, 0x15,
    0x00, 0xc4, 0x9d, 0x36, 0x08, 0x86, 0xe7, 0x04, 0x93, 0x6a, 0x66, 0x78,
    0xe1, 0x13, 0x9d, 0x26, 0xb7, 0x81, 0x9f, 0x7e, 0x90, 0x04, 0x41, 0x04,
    0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47, 0xf8, 0xbc, 0xe6, 0xe5,
    0x63, 0xa4, 0x40, 0xf2, 0x77, 0x03, 0x7d, 0x81, 0x2d, 0xeb, 0x33, 0xa0,
    0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96, 0x4f, 0xe3, 0x42, 0xe2,
    0xfe, 0x1a, 0x7f, 0x9b, 0x8e, 0xe7, 0xeb, 0x4a, 0x7c, 0x0f, 0x9e, 0x16,
    0x2b, 0xce, 0x33, 0x57, 0x6b, 0x31, 0x5e, 0xce, 0xcb, 0xb6, 0x40, 0x68,
    0x37, 0xbf, 0x51, 0xf5, 0x02, 0x21, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00,
    0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xbc,
    0xe6, 0xfa, 0xad, 0xa7, 0x17, 0x9e, 0x84, 0xf3, 0xb9, 0xca, 0xc2, 0xfc,
    0x63, 0x25, 0x51, 0x02, 0x01, 0x01,
};

// kECKeyMissingZeros is an ECPrivateKey containing a degenerate P-256 key where
// the private key is one. The private key is incorrectly encoded without zero
// padding.
static const uint8_t kECKeyMissingZeros[] = {
  0x30, 0x58, 0x02, 0x01, 0x01, 0x04, 0x01, 0x01, 0xa0, 0x0a, 0x06, 0x08, 0x2a,
  0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0xa1, 0x44, 0x03, 0x42, 0x00, 0x04,
  0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47, 0xf8, 0xbc, 0xe6, 0xe5, 0x63,
  0xa4, 0x40, 0xf2, 0x77, 0x03, 0x7d, 0x81, 0x2d, 0xeb, 0x33, 0xa0, 0xf4, 0xa1,
  0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96, 0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a, 0x7f,
  0x9b, 0x8e, 0xe7, 0xeb, 0x4a, 0x7c, 0x0f, 0x9e, 0x16, 0x2b, 0xce, 0x33, 0x57,
  0x6b, 0x31, 0x5e, 0xce, 0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5,
};

// kECKeyMissingZeros is an ECPrivateKey containing a degenerate P-256 key where
// the private key is one. The private key is encoded with the required zero
// padding.
static const uint8_t kECKeyWithZeros[] = {
  0x30, 0x77, 0x02, 0x01, 0x01, 0x04, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
  0xa0, 0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0xa1,
  0x44, 0x03, 0x42, 0x00, 0x04, 0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47,
  0xf8, 0xbc, 0xe6, 0xe5, 0x63, 0xa4, 0x40, 0xf2, 0x77, 0x03, 0x7d, 0x81, 0x2d,
  0xeb, 0x33, 0xa0, 0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96, 0x4f, 0xe3,
  0x42, 0xe2, 0xfe, 0x1a, 0x7f, 0x9b, 0x8e, 0xe7, 0xeb, 0x4a, 0x7c, 0x0f, 0x9e,
  0x16, 0x2b, 0xce, 0x33, 0x57, 0x6b, 0x31, 0x5e, 0xce, 0xcb, 0xb6, 0x40, 0x68,
  0x37, 0xbf, 0x51, 0xf5,
};

// DecodeECPrivateKey decodes |in| as an ECPrivateKey structure and returns the
// result or nullptr on error.
static bssl::UniquePtr<EC_KEY> DecodeECPrivateKey(const uint8_t *in,
                                                  size_t in_len) {
  CBS cbs;
  CBS_init(&cbs, in, in_len);
  bssl::UniquePtr<EC_KEY> ret(EC_KEY_parse_private_key(&cbs, NULL));
  if (!ret || CBS_len(&cbs) != 0) {
    return nullptr;
  }
  return ret;
}

// EncodeECPrivateKey encodes |key| as an ECPrivateKey structure into |*out|. It
// returns true on success or false on error.
static bool EncodeECPrivateKey(std::vector<uint8_t> *out, const EC_KEY *key) {
  ScopedCBB cbb;
  uint8_t *der;
  size_t der_len;
  if (!CBB_init(cbb.get(), 0) ||
      !EC_KEY_marshal_private_key(cbb.get(), key, EC_KEY_get_enc_flags(key)) ||
      !CBB_finish(cbb.get(), &der, &der_len)) {
    return false;
  }
  out->assign(der, der + der_len);
  OPENSSL_free(der);
  return true;
}

static bool Testd2i_ECPrivateKey() {
  bssl::UniquePtr<EC_KEY> key = DecodeECPrivateKey(kECKeyWithoutPublic,
                                        sizeof(kECKeyWithoutPublic));
  if (!key) {
    fprintf(stderr, "Failed to parse private key.\n");
    ERR_print_errors_fp(stderr);
    return false;
  }

  std::vector<uint8_t> out;
  if (!EncodeECPrivateKey(&out, key.get())) {
    fprintf(stderr, "Failed to serialize private key.\n");
    ERR_print_errors_fp(stderr);
    return false;
  }

  if (std::vector<uint8_t>(kECKeyWithoutPublic,
                           kECKeyWithoutPublic + sizeof(kECKeyWithoutPublic)) !=
      out) {
    fprintf(stderr, "Serialisation of key doesn't match original.\n");
    return false;
  }

  const EC_POINT *pub_key = EC_KEY_get0_public_key(key.get());
  if (pub_key == NULL) {
    fprintf(stderr, "Public key missing.\n");
    return false;
  }

  bssl::UniquePtr<BIGNUM> x(BN_new());
  bssl::UniquePtr<BIGNUM> y(BN_new());
  if (!x || !y) {
    return false;
  }
  if (!EC_POINT_get_affine_coordinates_GFp(EC_KEY_get0_group(key.get()),
                                           pub_key, x.get(), y.get(), NULL)) {
    fprintf(stderr, "Failed to get public key in affine coordinates.\n");
    return false;
  }
  bssl::UniquePtr<char> x_hex(BN_bn2hex(x.get()));
  bssl::UniquePtr<char> y_hex(BN_bn2hex(y.get()));
  if (!x_hex || !y_hex) {
    return false;
  }
  if (0 != strcmp(
          x_hex.get(),
          "c81561ecf2e54edefe6617db1c7a34a70744ddb261f269b83dacfcd2ade5a681") ||
      0 != strcmp(
          y_hex.get(),
          "e0e2afa3f9b6abe4c698ef6495f1be49a3196c5056acb3763fe4507eec596e88")) {
    fprintf(stderr, "Incorrect public key: %s %s\n", x_hex.get(), y_hex.get());
    return false;
  }

  return true;
}

static bool TestZeroPadding() {
  // Check that the correct encoding round-trips.
  bssl::UniquePtr<EC_KEY> key = DecodeECPrivateKey(kECKeyWithZeros,
                                        sizeof(kECKeyWithZeros));
  std::vector<uint8_t> out;
  if (!key || !EncodeECPrivateKey(&out, key.get())) {
    ERR_print_errors_fp(stderr);
    return false;
  }

  if (std::vector<uint8_t>(kECKeyWithZeros,
                           kECKeyWithZeros + sizeof(kECKeyWithZeros)) != out) {
    fprintf(stderr, "Serialisation of key was incorrect.\n");
    return false;
  }

  // Keys without leading zeros also parse, but they encode correctly.
  key = DecodeECPrivateKey(kECKeyMissingZeros, sizeof(kECKeyMissingZeros));
  if (!key || !EncodeECPrivateKey(&out, key.get())) {
    ERR_print_errors_fp(stderr);
    return false;
  }

  if (std::vector<uint8_t>(kECKeyWithZeros,
                           kECKeyWithZeros + sizeof(kECKeyWithZeros)) != out) {
    fprintf(stderr, "Serialisation of key was incorrect.\n");
    return false;
  }

  return true;
}

static bool TestSpecifiedCurve() {
  // Test keys with specified curves may be decoded.
  bssl::UniquePtr<EC_KEY> key =
      DecodeECPrivateKey(kECKeySpecifiedCurve, sizeof(kECKeySpecifiedCurve));
  if (!key) {
    ERR_print_errors_fp(stderr);
    return false;
  }

  // The group should have been interpreted as P-256.
  if (EC_GROUP_get_curve_name(EC_KEY_get0_group(key.get())) !=
      NID_X9_62_prime256v1) {
    fprintf(stderr, "Curve name incorrect.\n");
    return false;
  }

  // Encoding the key should still use named form.
  std::vector<uint8_t> out;
  if (!EncodeECPrivateKey(&out, key.get())) {
    ERR_print_errors_fp(stderr);
    return false;
  }
  if (std::vector<uint8_t>(kECKeyWithoutPublic,
                           kECKeyWithoutPublic + sizeof(kECKeyWithoutPublic)) !=
      out) {
    fprintf(stderr, "Serialisation of key was incorrect.\n");
    return false;
  }

  return true;
}

static bool TestSetAffine(const int nid) {
  bssl::UniquePtr<EC_KEY> key(EC_KEY_new_by_curve_name(nid));
  if (!key) {
    return false;
  }

  const EC_GROUP *const group = EC_KEY_get0_group(key.get());

  if (!EC_KEY_generate_key(key.get())) {
    fprintf(stderr, "EC_KEY_generate_key failed with nid %d\n", nid);
    ERR_print_errors_fp(stderr);
    return false;
  }

  if (!EC_POINT_is_on_curve(group, EC_KEY_get0_public_key(key.get()),
                            nullptr)) {
    fprintf(stderr, "generated point is not on curve with nid %d", nid);
    ERR_print_errors_fp(stderr);
    return false;
  }

  bssl::UniquePtr<BIGNUM> x(BN_new());
  bssl::UniquePtr<BIGNUM> y(BN_new());
  if (!EC_POINT_get_affine_coordinates_GFp(group,
                                           EC_KEY_get0_public_key(key.get()),
                                           x.get(), y.get(), nullptr)) {
    fprintf(stderr, "EC_POINT_get_affine_coordinates_GFp failed with nid %d\n",
            nid);
    ERR_print_errors_fp(stderr);
    return false;
  }

  auto point = bssl::UniquePtr<EC_POINT>(EC_POINT_new(group));
  if (!point) {
    return false;
  }

  if (!EC_POINT_set_affine_coordinates_GFp(group, point.get(), x.get(), y.get(),
                                           nullptr)) {
    fprintf(stderr, "EC_POINT_set_affine_coordinates_GFp failed with nid %d\n",
            nid);
    ERR_print_errors_fp(stderr);
    return false;
  }

  // Subtract one from |y| to make the point no longer on the curve.
  if (!BN_sub(y.get(), y.get(), BN_value_one())) {
    return false;
  }

  bssl::UniquePtr<EC_POINT> invalid_point(EC_POINT_new(group));
  if (!invalid_point) {
    return false;
  }

  if (EC_POINT_set_affine_coordinates_GFp(group, invalid_point.get(), x.get(),
                                          y.get(), nullptr)) {
    fprintf(stderr,
            "EC_POINT_set_affine_coordinates_GFp succeeded with invalid "
            "coordinates with nid %d\n",
            nid);
    ERR_print_errors_fp(stderr);
    return false;
  }

  return true;
}

static bool TestArbitraryCurve() {
  // Make a P-256 key and extract the affine coordinates.
  bssl::UniquePtr<EC_KEY> key(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  if (!key || !EC_KEY_generate_key(key.get())) {
    return false;
  }

  // Make an arbitrary curve which is identical to P-256.
  static const uint8_t kP[] = {
      0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  };
  static const uint8_t kA[] = {
      0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc,
  };
  static const uint8_t kB[] = {
      0x5a, 0xc6, 0x35, 0xd8, 0xaa, 0x3a, 0x93, 0xe7, 0xb3, 0xeb, 0xbd,
      0x55, 0x76, 0x98, 0x86, 0xbc, 0x65, 0x1d, 0x06, 0xb0, 0xcc, 0x53,
      0xb0, 0xf6, 0x3b, 0xce, 0x3c, 0x3e, 0x27, 0xd2, 0x60, 0x4b,
  };
  static const uint8_t kX[] = {
      0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47, 0xf8, 0xbc, 0xe6,
      0xe5, 0x63, 0xa4, 0x40, 0xf2, 0x77, 0x03, 0x7d, 0x81, 0x2d, 0xeb,
      0x33, 0xa0, 0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96,
  };
  static const uint8_t kY[] = {
      0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a, 0x7f, 0x9b, 0x8e, 0xe7, 0xeb,
      0x4a, 0x7c, 0x0f, 0x9e, 0x16, 0x2b, 0xce, 0x33, 0x57, 0x6b, 0x31,
      0x5e, 0xce, 0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5,
  };
  static const uint8_t kOrder[] = {
      0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xbc, 0xe6, 0xfa, 0xad, 0xa7, 0x17,
      0x9e, 0x84, 0xf3, 0xb9, 0xca, 0xc2, 0xfc, 0x63, 0x25, 0x51,
  };
  bssl::UniquePtr<BN_CTX> ctx(BN_CTX_new());
  bssl::UniquePtr<BIGNUM> p(BN_bin2bn(kP, sizeof(kP), nullptr));
  bssl::UniquePtr<BIGNUM> a(BN_bin2bn(kA, sizeof(kA), nullptr));
  bssl::UniquePtr<BIGNUM> b(BN_bin2bn(kB, sizeof(kB), nullptr));
  bssl::UniquePtr<BIGNUM> gx(BN_bin2bn(kX, sizeof(kX), nullptr));
  bssl::UniquePtr<BIGNUM> gy(BN_bin2bn(kY, sizeof(kY), nullptr));
  bssl::UniquePtr<BIGNUM> order(BN_bin2bn(kOrder, sizeof(kOrder), nullptr));
  bssl::UniquePtr<BIGNUM> cofactor(BN_new());
  if (!ctx || !p || !a || !b || !gx || !gy || !order || !cofactor ||
      !BN_set_word(cofactor.get(), 1)) {
    return false;
  }

  bssl::UniquePtr<EC_GROUP> group(
      EC_GROUP_new_curve_GFp(p.get(), a.get(), b.get(), ctx.get()));
  if (!group) {
    return false;
  }
  bssl::UniquePtr<EC_POINT> generator(EC_POINT_new(group.get()));
  if (!generator ||
      !EC_POINT_set_affine_coordinates_GFp(group.get(), generator.get(),
                                           gx.get(), gy.get(), ctx.get()) ||
      !EC_GROUP_set_generator(group.get(), generator.get(), order.get(),
                              cofactor.get())) {
    return false;
  }

  // |group| should not have a curve name.
  if (EC_GROUP_get_curve_name(group.get()) != NID_undef) {
    return false;
  }

  // Copy |key| to |key2| using |group|.
  bssl::UniquePtr<EC_KEY> key2(EC_KEY_new());
  bssl::UniquePtr<EC_POINT> point(EC_POINT_new(group.get()));
  bssl::UniquePtr<BIGNUM> x(BN_new()), y(BN_new());
  if (!key2 || !point || !x || !y ||
      !EC_KEY_set_group(key2.get(), group.get()) ||
      !EC_KEY_set_private_key(key2.get(), EC_KEY_get0_private_key(key.get())) ||
      !EC_POINT_get_affine_coordinates_GFp(EC_KEY_get0_group(key.get()),
                                           EC_KEY_get0_public_key(key.get()),
                                           x.get(), y.get(), nullptr) ||
      !EC_POINT_set_affine_coordinates_GFp(group.get(), point.get(), x.get(),
                                           y.get(), nullptr) ||
      !EC_KEY_set_public_key(key2.get(), point.get())) {
    fprintf(stderr, "Could not copy key.\n");
    return false;
  }

  // The key must be valid according to the new group too.
  if (!EC_KEY_check_key(key2.get())) {
    fprintf(stderr, "Copied key is not valid.\n");
    return false;
  }

  return true;
}

static bool TestAddingEqualPoints(int nid) {
  bssl::UniquePtr<EC_KEY> key(EC_KEY_new_by_curve_name(nid));
  if (!key) {
    return false;
  }

  const EC_GROUP *const group = EC_KEY_get0_group(key.get());

  if (!EC_KEY_generate_key(key.get())) {
    fprintf(stderr, "EC_KEY_generate_key failed with nid %d\n", nid);
    ERR_print_errors_fp(stderr);
    return false;
  }

  bssl::UniquePtr<EC_POINT> p1(EC_POINT_new(group));
  bssl::UniquePtr<EC_POINT> p2(EC_POINT_new(group));
  bssl::UniquePtr<EC_POINT> double_p1(EC_POINT_new(group));
  bssl::UniquePtr<EC_POINT> p1_plus_p2(EC_POINT_new(group));
  if (!p1 || !p2 || !double_p1 || !p1_plus_p2) {
    return false;
  }

  if (!EC_POINT_copy(p1.get(), EC_KEY_get0_public_key(key.get())) ||
      !EC_POINT_copy(p2.get(), EC_KEY_get0_public_key(key.get()))) {
    fprintf(stderr, "EC_POINT_COPY failed with nid %d\n", nid);
    ERR_print_errors_fp(stderr);
    return false;
  }

  bssl::UniquePtr<BN_CTX> ctx(BN_CTX_new());
  if (!ctx) {
    return false;
  }

  if (!EC_POINT_dbl(group, double_p1.get(), p1.get(), ctx.get()) ||
      !EC_POINT_add(group, p1_plus_p2.get(), p1.get(), p2.get(), ctx.get())) {
    fprintf(stderr, "Point operation failed with nid %d\n", nid);
    ERR_print_errors_fp(stderr);
    return false;
  }

  if (EC_POINT_cmp(group, double_p1.get(), p1_plus_p2.get(), ctx.get()) != 0) {
    fprintf(stderr, "A+A != 2A for nid %d", nid);
    return false;
  }

  return true;
}

static bool ForEachCurve(bool (*test_func)(int nid)) {
  const size_t num_curves = EC_get_builtin_curves(nullptr, 0);
  std::vector<EC_builtin_curve> curves(num_curves);
  EC_get_builtin_curves(curves.data(), num_curves);

  for (const auto& curve : curves) {
    if (!test_func(curve.nid)) {
      fprintf(stderr, "Test failed for %s\n", curve.comment);
      return false;
    }
  }

  return true;
}

static int Main() {
  CRYPTO_library_init();

  if (!Testd2i_ECPrivateKey() ||
      !TestZeroPadding() ||
      !TestSpecifiedCurve() ||
      !ForEachCurve(TestSetAffine) ||
      !ForEachCurve(TestAddingEqualPoints) ||
      !TestArbitraryCurve()) {
    fprintf(stderr, "failed\n");
    return 1;
  }

  printf("PASS\n");
  return 0;
}

}  // namespace bssl

int main() {
  return bssl::Main();
}
