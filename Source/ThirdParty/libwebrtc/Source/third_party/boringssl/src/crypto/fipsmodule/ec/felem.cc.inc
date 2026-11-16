// Copyright 2018 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/mem.h>

#include <assert.h>

#include "../../internal.h"
#include "../bn/internal.h"
#include "internal.h"


BSSL_NAMESPACE_BEGIN

const EC_FELEM *ec_felem_one(const EC_GROUP *group) {
  // We reuse generator.Z as a cache for 1 in the field.
  return &group->generator.raw.Z;
}

int ec_bignum_to_felem(const EC_GROUP *group, EC_FELEM *out, const BIGNUM *in) {
  uint8_t bytes[EC_MAX_BYTES];
  size_t len = BN_num_bytes(&group->field.N);
  assert(sizeof(bytes) >= len);
  if (BN_is_negative(in) || BN_cmp(in, &group->field.N) >= 0 ||
      !BN_bn2bin_padded(bytes, len, in)) {
    OPENSSL_PUT_ERROR(EC, EC_R_COORDINATES_OUT_OF_RANGE);
    return 0;
  }

  return ec_felem_from_bytes(group, out, bytes, len);
}

int ec_felem_to_bignum(const EC_GROUP *group, BIGNUM *out, const EC_FELEM *in) {
  uint8_t bytes[EC_MAX_BYTES];
  size_t len;
  ec_felem_to_bytes(group, bytes, &len, in);
  return BN_bin2bn(bytes, len, out) != nullptr;
}

void ec_felem_to_bytes(const EC_GROUP *group, uint8_t *out, size_t *out_len,
                       const EC_FELEM *in) {
  EC_FELEM tmp;
  ec_felem_from_montgomery(group, &tmp, in);
  size_t len = BN_num_bytes(&group->field.N);
  bn_words_to_big_endian(out, len, tmp.words, group->field.N.width);
  *out_len = len;
}

int ec_felem_from_bytes(const EC_GROUP *group, EC_FELEM *out, const uint8_t *in,
                        size_t len) {
  if (len != BN_num_bytes(&group->field.N)) {
    OPENSSL_PUT_ERROR(EC, EC_R_DECODE_ERROR);
    return 0;
  }

  bn_big_endian_to_words(out->words, group->field.N.width, in, len);
  if (!bn_less_than_words(out->words, group->field.N.d, group->field.N.width)) {
    OPENSSL_PUT_ERROR(EC, EC_R_DECODE_ERROR);
    return 0;
  }

  ec_felem_to_montgomery(group, out, out);
  return 1;
}

crypto_word_t ec_felem_from_bytes_or_placeholder(const EC_GROUP *group,
                                                 EC_FELEM *out,
                                                 const uint8_t *in,
                                                 size_t len) {
  const BIGNUM *field = &group->field.N;
  BSSL_CHECK(len == BN_num_bytes(field));
  bn_big_endian_to_words(out->words, field->width, in, len);
  BN_ULONG ok =
      BN_ULONG{0} - bn_less_than_words(out->words, field->d, field->width);
  // Replace `out->words` with an arbitrary valid field element if out of range.
  // This avoids breaking preconditions of later operations.
  for (int i = 0; i < field->width; i++) {
    out->words[i] &= ok;
  }
  ec_felem_to_montgomery(group, out, out);
  return ok;
}

void ec_felem_neg(const EC_GROUP *group, EC_FELEM *out, const EC_FELEM *a) {
  // -a is zero if a is zero and p-a otherwise.
  BN_ULONG mask = ec_felem_non_zero_mask(group, a);
  BN_ULONG borrow = bn_sub_words(out->words, group->field.N.d, a->words,
                                 group->field.N.width);
  assert(borrow == 0);
  (void)borrow;
  for (int i = 0; i < group->field.N.width; i++) {
    out->words[i] &= mask;
  }
}

void ec_felem_add(const EC_GROUP *group, EC_FELEM *out, const EC_FELEM *a,
                  const EC_FELEM *b) {
  EC_FELEM tmp;
  bn_mod_add_words(out->words, a->words, b->words, group->field.N.d, tmp.words,
                   group->field.N.width);
}

void ec_felem_sub(const EC_GROUP *group, EC_FELEM *out, const EC_FELEM *a,
                  const EC_FELEM *b) {
  EC_FELEM tmp;
  bn_mod_sub_words(out->words, a->words, b->words, group->field.N.d, tmp.words,
                   group->field.N.width);
}

BN_ULONG ec_felem_non_zero_mask(const EC_GROUP *group, const EC_FELEM *a) {
  BN_ULONG mask = 0;
  for (int i = 0; i < group->field.N.width; i++) {
    mask |= a->words[i];
  }
  return ~constant_time_is_zero_w(mask);
}

void ec_felem_select(const EC_GROUP *group, EC_FELEM *out, BN_ULONG mask,
                     const EC_FELEM *a, const EC_FELEM *b) {
  bn_select_words(out->words, mask, a->words, b->words, group->field.N.width);
}

int ec_felem_equal(const EC_GROUP *group, const EC_FELEM *a,
                   const EC_FELEM *b) {
  return CRYPTO_memcmp(a->words, b->words,
                       group->field.N.width * sizeof(BN_ULONG)) == 0;
}

void ec_felem_mul(const EC_GROUP *group, EC_FELEM *out, const EC_FELEM *a,
                  const EC_FELEM *b) {
  bn_mod_mul_montgomery_small(out->words, a->words, b->words,
                              group->field.N.width, &group->field);
}

void ec_felem_sqr(const EC_GROUP *group, EC_FELEM *out, const EC_FELEM *a) {
  bn_mod_mul_montgomery_small(out->words, a->words, a->words,
                              group->field.N.width, &group->field);
}

void ec_felem_to_montgomery(const EC_GROUP *group, EC_FELEM *out,
                            const EC_FELEM *a) {
  bn_to_montgomery_small(out->words, a->words, group->field.N.width,
                         &group->field);
}

void ec_felem_from_montgomery(const EC_GROUP *group, EC_FELEM *out,
                              const EC_FELEM *a) {
  bn_from_montgomery_small(out->words, group->field.N.width, a->words,
                           group->field.N.width, &group->field);
}

void ec_felem_reduce(const EC_GROUP *group, EC_FELEM *out,
                     const BN_ULONG *words, size_t num) {
  // Convert "from" Montgomery form so the value is reduced mod p.
  bn_from_montgomery_small(out->words, group->field.N.width, words, num,
                           &group->field);
  // Convert "to" Montgomery form to remove the R^-1 factor added.
  ec_felem_to_montgomery(group, out, out);
  // Convert to Montgomery form to match this implementation's representation.
  ec_felem_to_montgomery(group, out, out);
}

void ec_felem_exp(const EC_GROUP *group, EC_FELEM *out, const EC_FELEM *a,
                  const BN_ULONG *exp, size_t num_exp) {
  bn_mod_exp_mont_small(out->words, a->words, group->field.N.width, exp,
                        num_exp, &group->field);
}

// adjust_y negates `y` if its LSB (when converted out of Montgomery form) was
// not `y_bit`.
static void adjust_y(const EC_GROUP *group, EC_FELEM *y, crypto_word_t y_bit) {
  EC_FELEM y_from_mont, neg_y;
  // `y` is stored in the Montgomery domain. Convert back out to sample the LSB.
  ec_felem_from_montgomery(group, &y_from_mont, y);
  BN_ULONG lsb_mismatch = (y_bit ^ y_from_mont.words[0]) & 1;
  lsb_mismatch = 0u - lsb_mismatch;  // All ones or all zeros.
  ec_felem_neg(group, &neg_y, y);
  ec_felem_select(group, y, lsb_mismatch, &neg_y, y);
}

int ec_felem_sqrt(const EC_GROUP *group, EC_FELEM *out, const EC_FELEM *a,
                  crypto_word_t y_bit) {
  if (group->field_is_3_mod_4) {
    if (!ec_felem_sqrt_secret(group, out, a, y_bit)) {
      OPENSSL_PUT_ERROR(EC, EC_R_INVALID_COMPRESSED_POINT);
      return 0;
    }
    return 1;
  }

  // Curves that are not 3 mod 4 are messy to implement modular square root. For
  // now, fall back to BIGNUM. This case comes up for P-224 and custom curves.
  // P-224's prime is particularly inconvenient for compressed coordinates. See
  // https://cr.yp.to/papers/sqroot.pdf
  UniquePtr<BN_CTX> ctx(BN_CTX_new());
  UniquePtr<BIGNUM> bn(BN_new());
  if (ctx == nullptr || bn == nullptr ||
      !ec_felem_to_bignum(group, bn.get(), a)) {
    return 0;
  }

  UniquePtr<BIGNUM> sqrt(
      BN_mod_sqrt(nullptr, bn.get(), &group->field.N, ctx.get()));
  if (sqrt == nullptr || !ec_bignum_to_felem(group, out, sqrt.get())) {
    return 0;
  }

  adjust_y(group, out, y_bit);
  return 1;
}

crypto_word_t ec_felem_sqrt_secret(const EC_GROUP *group, EC_FELEM *out,
                                   const EC_FELEM *a, crypto_word_t y_bit) {
  BSSL_CHECK(group->field_is_3_mod_4);

  // If `a` is a square, then a^(p+1)/4 is a square root. By Euler's criterion,
  // a^(p-1)/2 = 1 for squares, so a^(p+1)/2 = a. p is 3 mod 4, so that exponent
  // is even, so we can halve it to find a square root.
  const size_t num_exp = group->field.N.width;
  BN_ULONG exp[EC_MAX_WORDS];
  bn_rshift_words(exp, group->field.N.d, 2, num_exp);  // exp = (p-3)/4
  bn_add_carry_words(exp, exp, /*carry=*/1, num_exp);  // exp = (p+1)/4
  ec_felem_exp(group, out, a, exp, num_exp);           // out = sqrt(a)

  // Pick the correct (possible) square root.
  adjust_y(group, out, y_bit);

  // Check `out^2 = a`. If `a` had a square root, `out` must be one and the
  // check will pass. If not, the check cannot pass.
  EC_FELEM check;
  ec_felem_sqr(group, &check, out);        // check = out^2
  ec_felem_sub(group, &check, &check, a);  // check = out^2 - a
  return ~ec_felem_non_zero_mask(group, &check);
}

BSSL_NAMESPACE_END
