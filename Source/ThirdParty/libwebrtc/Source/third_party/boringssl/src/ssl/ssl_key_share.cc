/* Copyright (c) 2015, Google Inc.
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

#include <openssl/ssl.h>

#include <assert.h>
#include <string.h>

#include <utility>

#include <openssl/bn.h>
#include <openssl/bytestring.h>
#include <openssl/curve25519.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/nid.h>

#include "internal.h"
#include "../crypto/internal.h"


namespace bssl {

namespace {

class ECKeyShare : public SSLKeyShare {
 public:
  ECKeyShare(int nid, uint16_t group_id) : nid_(nid), group_id_(group_id) {}
  ~ECKeyShare() override {}

  uint16_t GroupID() const override { return group_id_; }

  bool Offer(CBB *out) override {
    assert(!private_key_);
    // Set up a shared |BN_CTX| for all operations.
    UniquePtr<BN_CTX> bn_ctx(BN_CTX_new());
    if (!bn_ctx) {
      return false;
    }
    BN_CTXScope scope(bn_ctx.get());

    // Generate a private key.
    UniquePtr<EC_GROUP> group(EC_GROUP_new_by_curve_name(nid_));
    private_key_.reset(BN_new());
    if (!group || !private_key_ ||
        !BN_rand_range_ex(private_key_.get(), 1,
                          EC_GROUP_get0_order(group.get()))) {
      return false;
    }

    // Compute the corresponding public key and serialize it.
    UniquePtr<EC_POINT> public_key(EC_POINT_new(group.get()));
    if (!public_key ||
        !EC_POINT_mul(group.get(), public_key.get(), private_key_.get(), NULL,
                      NULL, bn_ctx.get()) ||
        !EC_POINT_point2cbb(out, group.get(), public_key.get(),
                            POINT_CONVERSION_UNCOMPRESSED, bn_ctx.get())) {
      return false;
    }

    return true;
  }

  bool Finish(Array<uint8_t> *out_secret, uint8_t *out_alert,
              Span<const uint8_t> peer_key) override {
    assert(private_key_);
    *out_alert = SSL_AD_INTERNAL_ERROR;

    // Set up a shared |BN_CTX| for all operations.
    UniquePtr<BN_CTX> bn_ctx(BN_CTX_new());
    if (!bn_ctx) {
      return false;
    }
    BN_CTXScope scope(bn_ctx.get());

    UniquePtr<EC_GROUP> group(EC_GROUP_new_by_curve_name(nid_));
    if (!group) {
      return false;
    }

    UniquePtr<EC_POINT> peer_point(EC_POINT_new(group.get()));
    UniquePtr<EC_POINT> result(EC_POINT_new(group.get()));
    BIGNUM *x = BN_CTX_get(bn_ctx.get());
    if (!peer_point || !result || !x) {
      return false;
    }

    if (!EC_POINT_oct2point(group.get(), peer_point.get(), peer_key.data(),
                            peer_key.size(), bn_ctx.get())) {
      *out_alert = SSL_AD_DECODE_ERROR;
      return false;
    }

    // Compute the x-coordinate of |peer_key| * |private_key_|.
    if (!EC_POINT_mul(group.get(), result.get(), NULL, peer_point.get(),
                      private_key_.get(), bn_ctx.get()) ||
        !EC_POINT_get_affine_coordinates_GFp(group.get(), result.get(), x, NULL,
                                             bn_ctx.get())) {
      return false;
    }

    // Encode the x-coordinate left-padded with zeros.
    Array<uint8_t> secret;
    if (!secret.Init((EC_GROUP_get_degree(group.get()) + 7) / 8) ||
        !BN_bn2bin_padded(secret.data(), secret.size(), x)) {
      return false;
    }

    *out_secret = std::move(secret);
    return true;
  }

 private:
  UniquePtr<BIGNUM> private_key_;
  int nid_;
  uint16_t group_id_;
};

class X25519KeyShare : public SSLKeyShare {
 public:
  X25519KeyShare() {}
  ~X25519KeyShare() override {
    OPENSSL_cleanse(private_key_, sizeof(private_key_));
  }

  uint16_t GroupID() const override { return SSL_CURVE_X25519; }

  bool Offer(CBB *out) override {
    uint8_t public_key[32];
    X25519_keypair(public_key, private_key_);
    return !!CBB_add_bytes(out, public_key, sizeof(public_key));
  }

  bool Finish(Array<uint8_t> *out_secret, uint8_t *out_alert,
              Span<const uint8_t> peer_key) override {
    *out_alert = SSL_AD_INTERNAL_ERROR;

    Array<uint8_t> secret;
    if (!secret.Init(32)) {
      OPENSSL_PUT_ERROR(SSL, ERR_R_MALLOC_FAILURE);
      return false;
    }

    if (peer_key.size() != 32 ||
        !X25519(secret.data(), private_key_, peer_key.data())) {
      *out_alert = SSL_AD_DECODE_ERROR;
      OPENSSL_PUT_ERROR(SSL, SSL_R_BAD_ECPOINT);
      return false;
    }

    *out_secret = std::move(secret);
    return true;
  }

 private:
  uint8_t private_key_[32];
};

CONSTEXPR_ARRAY struct {
  int nid;
  uint16_t group_id;
  const char name[8], alias[11];
} kNamedGroups[] = {
    {NID_secp224r1, SSL_CURVE_SECP224R1, "P-224", "secp224r1"},
    {NID_X9_62_prime256v1, SSL_CURVE_SECP256R1, "P-256", "prime256v1"},
    {NID_secp384r1, SSL_CURVE_SECP384R1, "P-384", "secp384r1"},
    {NID_secp521r1, SSL_CURVE_SECP521R1, "P-521", "secp521r1"},
    {NID_X25519, SSL_CURVE_X25519, "X25519", "x25519"},
};

}  // namespace

UniquePtr<SSLKeyShare> SSLKeyShare::Create(uint16_t group_id) {
  switch (group_id) {
    case SSL_CURVE_SECP224R1:
      return UniquePtr<SSLKeyShare>(
          New<ECKeyShare>(NID_secp224r1, SSL_CURVE_SECP224R1));
    case SSL_CURVE_SECP256R1:
      return UniquePtr<SSLKeyShare>(
          New<ECKeyShare>(NID_X9_62_prime256v1, SSL_CURVE_SECP256R1));
    case SSL_CURVE_SECP384R1:
      return UniquePtr<SSLKeyShare>(
          New<ECKeyShare>(NID_secp384r1, SSL_CURVE_SECP384R1));
    case SSL_CURVE_SECP521R1:
      return UniquePtr<SSLKeyShare>(
          New<ECKeyShare>(NID_secp521r1, SSL_CURVE_SECP521R1));
    case SSL_CURVE_X25519:
      return UniquePtr<SSLKeyShare>(New<X25519KeyShare>());
    default:
      return nullptr;
  }
}

bool SSLKeyShare::Accept(CBB *out_public_key, Array<uint8_t> *out_secret,
                         uint8_t *out_alert, Span<const uint8_t> peer_key) {
  *out_alert = SSL_AD_INTERNAL_ERROR;
  return Offer(out_public_key) &&
         Finish(out_secret, out_alert, peer_key);
}

int ssl_nid_to_group_id(uint16_t *out_group_id, int nid) {
  for (const auto &group : kNamedGroups) {
    if (group.nid == nid) {
      *out_group_id = group.group_id;
      return 1;
    }
  }
  return 0;
}

int ssl_name_to_group_id(uint16_t *out_group_id, const char *name, size_t len) {
  for (const auto &group : kNamedGroups) {
    if (len == strlen(group.name) &&
        !strncmp(group.name, name, len)) {
      *out_group_id = group.group_id;
      return 1;
    }
    if (len == strlen(group.alias) &&
        !strncmp(group.alias, name, len)) {
      *out_group_id = group.group_id;
      return 1;
    }
  }
  return 0;
}

}  // namespace bssl

using namespace bssl;

const char* SSL_get_curve_name(uint16_t group_id) {
  for (const auto &group : kNamedGroups) {
    if (group.group_id == group_id) {
      return group.name;
    }
  }
  return nullptr;
}
