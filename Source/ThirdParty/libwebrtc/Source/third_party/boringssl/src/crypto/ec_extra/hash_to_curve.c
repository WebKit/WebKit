/* Copyright (c) 2020, Google Inc.
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

#include <openssl/ec.h>

#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/nid.h>
#include <openssl/type_check.h>

#include <assert.h>

#include "internal.h"
#include "../fipsmodule/bn/internal.h"
#include "../fipsmodule/ec/internal.h"
#include "../internal.h"


// This file implements hash-to-curve, as described in
// draft-irtf-cfrg-hash-to-curve-07.
//
// This hash-to-curve implementation is written generically with the
// expectation that we will eventually wish to support other curves. If it
// becomes a performance bottleneck, some possible optimizations by
// specializing it to the curve:
//
// - Rather than using a generic |felem_exp|, specialize the exponentation to
//   c2 with a faster addition chain.
//
// - |felem_mul| and |felem_sqr| are indirect calls to generic Montgomery
//   code. Given the few curves, we could specialize
//   |map_to_curve_simple_swu|. But doing this reasonably without duplicating
//   code in C is difficult. (C++ templates would be useful here.)
//
// - P-521's Z and c2 have small power-of-two absolute values. We could save
//   two multiplications in SSWU. (Other curves have reasonable values of Z
//   and inconvenient c2.) This is unlikely to be worthwhile without C++
//   templates to make specializing more convenient.

// expand_message_xmd implements the operation described in section 5.3.1 of
// draft-irtf-cfrg-hash-to-curve-07. It returns one on success and zero on
// allocation failure or if |out_len| was too large.
static int expand_message_xmd(const EVP_MD *md, uint8_t *out, size_t out_len,
                              const uint8_t *msg, size_t msg_len,
                              const uint8_t *dst, size_t dst_len) {
  int ret = 0;
  const size_t block_size = EVP_MD_block_size(md);
  const size_t md_size = EVP_MD_size(md);
  EVP_MD_CTX ctx;
  EVP_MD_CTX_init(&ctx);

  // Long DSTs are hashed down to size. See section 5.3.3.
  OPENSSL_STATIC_ASSERT(EVP_MAX_MD_SIZE < 256, "hashed DST still too large");
  uint8_t dst_buf[EVP_MAX_MD_SIZE];
  if (dst_len >= 256) {
    static const char kPrefix[] = "H2C-OVERSIZE-DST-";
    if (!EVP_DigestInit_ex(&ctx, md, NULL) ||
        !EVP_DigestUpdate(&ctx, kPrefix, sizeof(kPrefix) - 1) ||
        !EVP_DigestUpdate(&ctx, dst, dst_len) ||
        !EVP_DigestFinal_ex(&ctx, dst_buf, NULL)) {
      goto err;
    }
    dst = dst_buf;
    dst_len = md_size;
  }
  uint8_t dst_len_u8 = (uint8_t)dst_len;

  // Compute b_0.
  static const uint8_t kZeros[EVP_MAX_MD_BLOCK_SIZE] = {0};
  // If |out_len| exceeds 16 bits then |i| will wrap below causing an error to
  // be returned. This depends on the static assert above.
  uint8_t l_i_b_str_zero[3] = {out_len >> 8, out_len, 0};
  uint8_t b_0[EVP_MAX_MD_SIZE];
  if (!EVP_DigestInit_ex(&ctx, md, NULL) ||
      !EVP_DigestUpdate(&ctx, kZeros, block_size) ||
      !EVP_DigestUpdate(&ctx, msg, msg_len) ||
      !EVP_DigestUpdate(&ctx, l_i_b_str_zero, sizeof(l_i_b_str_zero)) ||
      !EVP_DigestUpdate(&ctx, dst, dst_len) ||
      !EVP_DigestUpdate(&ctx, &dst_len_u8, 1) ||
      !EVP_DigestFinal_ex(&ctx, b_0, NULL)) {
    goto err;
  }

  uint8_t b_i[EVP_MAX_MD_SIZE];
  uint8_t i = 1;
  while (out_len > 0) {
    if (i == 0) {
      // Input was too large.
      OPENSSL_PUT_ERROR(EC, ERR_R_INTERNAL_ERROR);
      goto err;
    }
    if (i > 1) {
      for (size_t j = 0; j < md_size; j++) {
        b_i[j] ^= b_0[j];
      }
    } else {
      OPENSSL_memcpy(b_i, b_0, md_size);
    }

    if (!EVP_DigestInit_ex(&ctx, md, NULL) ||
        !EVP_DigestUpdate(&ctx, b_i, md_size) ||
        !EVP_DigestUpdate(&ctx, &i, 1) ||
        !EVP_DigestUpdate(&ctx, dst, dst_len) ||
        !EVP_DigestUpdate(&ctx, &dst_len_u8, 1) ||
        !EVP_DigestFinal_ex(&ctx, b_i, NULL)) {
      goto err;
    }

    size_t todo = out_len >= md_size ? md_size : out_len;
    OPENSSL_memcpy(out, b_i, todo);
    out += todo;
    out_len -= todo;
    i++;
  }

  ret = 1;

err:
  EVP_MD_CTX_cleanup(&ctx);
  return ret;
}

// num_bytes_to_derive determines the number of bytes to derive when hashing to
// a number modulo |modulus|. See the hash_to_field operation defined in
// section 5.2 of draft-irtf-cfrg-hash-to-curve-07.
static int num_bytes_to_derive(size_t *out, const BIGNUM *modulus, unsigned k) {
  size_t bits = BN_num_bits(modulus);
  size_t L = (bits + k + 7) / 8;
  // We require 2^(8*L) < 2^(2*bits - 2) <= n^2 so to fit in bounds for
  // |felem_reduce| and |ec_scalar_reduce|. All defined hash-to-curve suites
  // define |k| to be well under this bound. (|k| is usually around half of
  // |p_bits|.)
  if (L * 8 >= 2 * bits - 2 ||
      L > 2 * EC_MAX_BYTES) {
    assert(0);
    OPENSSL_PUT_ERROR(EC, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  *out = L;
  return 1;
}

// big_endian_to_words decodes |in| as a big-endian integer and writes the
// result to |out|. |num_words| must be large enough to contain the output.
static void big_endian_to_words(BN_ULONG *out, size_t num_words,
                                const uint8_t *in, size_t len) {
  assert(len <= num_words * sizeof(BN_ULONG));
  // Ensure any excess bytes are zeroed.
  OPENSSL_memset(out, 0, num_words * sizeof(BN_ULONG));
  uint8_t *out_u8 = (uint8_t *)out;
  for (size_t i = 0; i < len; i++) {
    out_u8[len - 1 - i] = in[i];
  }
}

// hash_to_field implements the operation described in section 5.2
// of draft-irtf-cfrg-hash-to-curve-07, with count = 2. |k| is the security
// factor.
static int hash_to_field2(const EC_GROUP *group, const EVP_MD *md,
                          EC_FELEM *out1, EC_FELEM *out2, const uint8_t *dst,
                          size_t dst_len, unsigned k, const uint8_t *msg,
                          size_t msg_len) {
  size_t L;
  uint8_t buf[4 * EC_MAX_BYTES];
  if (!num_bytes_to_derive(&L, &group->field, k) ||
      !expand_message_xmd(md, buf, 2 * L, msg, msg_len, dst, dst_len)) {
    return 0;
  }
  BN_ULONG words[2 * EC_MAX_WORDS];
  size_t num_words = 2 * group->field.width;
  big_endian_to_words(words, num_words, buf, L);
  group->meth->felem_reduce(group, out1, words, num_words);
  big_endian_to_words(words, num_words, buf + L, L);
  group->meth->felem_reduce(group, out2, words, num_words);
  return 1;
}

// hash_to_scalar behaves like |hash_to_field2| but returns a value modulo the
// group order rather than a field element. |k| is the security factor.
static int hash_to_scalar(const EC_GROUP *group, const EVP_MD *md,
                          EC_SCALAR *out, const uint8_t *dst, size_t dst_len,
                          unsigned k, const uint8_t *msg, size_t msg_len) {
  size_t L;
  uint8_t buf[EC_MAX_BYTES * 2];
  if (!num_bytes_to_derive(&L, &group->order, k) ||
      !expand_message_xmd(md, buf, L, msg, msg_len, dst, dst_len)) {
    return 0;
  }

  BN_ULONG words[2 * EC_MAX_WORDS];
  size_t num_words = 2 * group->order.width;
  big_endian_to_words(words, num_words, buf, L);
  ec_scalar_reduce(group, out, words, num_words);
  return 1;
}

static inline void mul_A(const EC_GROUP *group, EC_FELEM *out,
                         const EC_FELEM *in) {
  assert(group->a_is_minus3);
  EC_FELEM tmp;
  ec_felem_add(group, &tmp, in, in);      // tmp = 2*in
  ec_felem_add(group, &tmp, &tmp, &tmp);  // tmp = 4*in
  ec_felem_sub(group, out, in, &tmp);     // out = -3*in
}

static inline void mul_minus_A(const EC_GROUP *group, EC_FELEM *out,
                               const EC_FELEM *in) {
  assert(group->a_is_minus3);
  EC_FELEM tmp;
  ec_felem_add(group, &tmp, in, in);   // tmp = 2*in
  ec_felem_add(group, out, &tmp, in);  // out = 3*in
}

// sgn0_le implements the operation described in section 4.1.2 of
// draft-irtf-cfrg-hash-to-curve-07.
static BN_ULONG sgn0_le(const EC_GROUP *group, const EC_FELEM *a) {
  uint8_t buf[EC_MAX_BYTES];
  size_t len;
  ec_felem_to_bytes(group, buf, &len, a);
  return buf[len - 1] & 1;
}

// map_to_curve_simple_swu implements the operation described in section 6.6.2
// of draft-irtf-cfrg-hash-to-curve-07, using the optimization in appendix
// D.2.1. It returns one on success and zero on error.
static int map_to_curve_simple_swu(const EC_GROUP *group, const EC_FELEM *Z,
                                   const BN_ULONG *c1, size_t num_c1,
                                   const EC_FELEM *c2, EC_RAW_POINT *out,
                                   const EC_FELEM *u) {
  void (*const felem_mul)(const EC_GROUP *, EC_FELEM *r, const EC_FELEM *a,
                          const EC_FELEM *b) = group->meth->felem_mul;
  void (*const felem_sqr)(const EC_GROUP *, EC_FELEM *r, const EC_FELEM *a) =
      group->meth->felem_sqr;

  // This function requires the prime be 3 mod 4, and that A = -3.
  if (group->field.width == 0 || (group->field.d[0] & 3) != 3 ||
      !group->a_is_minus3) {
    OPENSSL_PUT_ERROR(EC, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  EC_FELEM tv1, tv2, tv3, tv4, xd, x1n, x2n, tmp, gxd, gx1, y1, y2;
  felem_sqr(group, &tv1, u);                         // tv1 = u^2
  felem_mul(group, &tv3, Z, &tv1);                   // tv3 = Z * tv1
  felem_sqr(group, &tv2, &tv3);                      // tv2 = tv3^2
  ec_felem_add(group, &xd, &tv2, &tv3);              // xd = tv2 + tv3
  ec_felem_add(group, &x1n, &xd, &group->one);       // x1n = xd + 1
  felem_mul(group, &x1n, &x1n, &group->b);           // x1n = x1n * B
  mul_minus_A(group, &xd, &xd);                      // xd = -A * xd
  BN_ULONG e1 = ec_felem_non_zero_mask(group, &xd);  // e1 = xd == 0 [flipped]
  mul_A(group, &tmp, Z);
  ec_felem_select(group, &xd, e1, &xd, &tmp);  // xd = CMOV(xd, Z * A, e1)
  felem_sqr(group, &tv2, &xd);                 // tv2 = xd^2
  felem_mul(group, &gxd, &tv2, &xd);           // gxd = tv2 * xd = xd^3
  mul_A(group, &tv2, &tv2);                    // tv2 = A * tv2
  felem_sqr(group, &gx1, &x1n);                // gx1 = x1n^2
  ec_felem_add(group, &gx1, &gx1, &tv2);       // gx1 = gx1 + tv2
  felem_mul(group, &gx1, &gx1, &x1n);          // gx1 = gx1 * x1n
  felem_mul(group, &tv2, &group->b, &gxd);     // tv2 = B * gxd
  ec_felem_add(group, &gx1, &gx1, &tv2);       // gx1 = gx1 + tv2
  felem_sqr(group, &tv4, &gxd);                // tv4 = gxd^2
  felem_mul(group, &tv2, &gx1, &gxd);          // tv2 = gx1 * gxd
  felem_mul(group, &tv4, &tv4, &tv2);          // tv4 = tv4 * tv2
  group->meth->felem_exp(group, &y1, &tv4, c1, num_c1);  // y1 = tv4^c1
  felem_mul(group, &y1, &y1, &tv2);                      // y1 = y1 * tv2
  felem_mul(group, &x2n, &tv3, &x1n);                    // x2n = tv3 * x1n
  felem_mul(group, &y2, &y1, c2);                        // y2 = y1 * c2
  felem_mul(group, &y2, &y2, &tv1);                      // y2 = y2 * tv1
  felem_mul(group, &y2, &y2, u);                         // y2 = y2 * u
  felem_sqr(group, &tv2, &y1);                           // tv2 = y1^2
  felem_mul(group, &tv2, &tv2, &gxd);                    // tv2 = tv2 * gxd
  ec_felem_sub(group, &tv3, &tv2, &gx1);
  BN_ULONG e2 =
      ec_felem_non_zero_mask(group, &tv3);       // e2 = tv2 == gx1 [flipped]
  ec_felem_select(group, &x1n, e2, &x2n, &x1n);  // xn = CMOV(x2n, x1n, e2)
  ec_felem_select(group, &y1, e2, &y2, &y1);     // y = CMOV(y2, y1, e2)
  BN_ULONG sgn0_u = sgn0_le(group, u);
  BN_ULONG sgn0_y = sgn0_le(group, &y1);
  BN_ULONG e3 = sgn0_u ^ sgn0_y;
  e3 = ((BN_ULONG)0) - e3;  // e3 = sgn0(u) == sgn0(y) [flipped]
  ec_felem_neg(group, &y2, &y1);
  ec_felem_select(group, &y1, e3, &y2, &y1);  // y = CMOV(-y, y, e3)

  // Appendix D.1 describes how to convert (x1n, xd, y1, 1) to Jacobian
  // coordinates. Note yd = 1. Also note that gxd computed above is xd^3.
  felem_mul(group, &out->X, &x1n, &xd);     // X = xn * xd
  felem_mul(group, &out->Y, &y1, &gxd);     // Y = yn * gxd = yn * xd^3
  out->Z = xd;                              // Z = xd
  return 1;
}

static int hash_to_curve(const EC_GROUP *group, const EVP_MD *md,
                         const EC_FELEM *Z, const EC_FELEM *c2, unsigned k,
                         EC_RAW_POINT *out, const uint8_t *dst, size_t dst_len,
                         const uint8_t *msg, size_t msg_len) {
  EC_FELEM u0, u1;
  if (!hash_to_field2(group, md, &u0, &u1, dst, dst_len, k, msg, msg_len)) {
    return 0;
  }

  // Compute |c1| = (p - 3) / 4.
  BN_ULONG c1[EC_MAX_WORDS];
  size_t num_c1 = group->field.width;
  if (!bn_copy_words(c1, num_c1, &group->field)) {
    return 0;
  }
  bn_rshift_words(c1, c1, /*shift=*/2, /*num=*/num_c1);

  EC_RAW_POINT Q0, Q1;
  if (!map_to_curve_simple_swu(group, Z, c1, num_c1, c2, &Q0, &u0) ||
      !map_to_curve_simple_swu(group, Z, c1, num_c1, c2, &Q1, &u1)) {
    return 0;
  }

  group->meth->add(group, out, &Q0, &Q1);  // R = Q0 + Q1
  // All our curves have cofactor one, so |clear_cofactor| is a no-op.
  return 1;
}

static int felem_from_u8(const EC_GROUP *group, EC_FELEM *out, uint8_t a) {
  uint8_t bytes[EC_MAX_BYTES] = {0};
  size_t len = BN_num_bytes(&group->field);
  bytes[len - 1] = a;
  return ec_felem_from_bytes(group, out, bytes, len);
}

int ec_hash_to_curve_p384_xmd_sha512_sswu_draft07(
    const EC_GROUP *group, EC_RAW_POINT *out, const uint8_t *dst,
    size_t dst_len, const uint8_t *msg, size_t msg_len) {
  // See section 8.3 of draft-irtf-cfrg-hash-to-curve-07.
  if (EC_GROUP_get_curve_name(group) != NID_secp384r1) {
    OPENSSL_PUT_ERROR(EC, EC_R_GROUP_MISMATCH);
    return 0;
  }

  // kSqrt1728 was computed as follows in python3:
  //
  // p = 2**384 - 2**128 - 2**96 + 2**32 - 1
  // z3 = 12**3
  // c2 = pow(z3, (p+1)//4, p)
  // assert z3 == pow(c2, 2, p)
  // ", ".join("0x%02x" % b for b in c2.to_bytes(384//8, 'big')

  static const uint8_t kSqrt1728[] = {
      0x01, 0x98, 0x77, 0xcc, 0x10, 0x41, 0xb7, 0x55, 0x57, 0x43, 0xc0, 0xae,
      0x2e, 0x3a, 0x3e, 0x61, 0xfb, 0x2a, 0xaa, 0x2e, 0x0e, 0x87, 0xea, 0x55,
      0x7a, 0x56, 0x3d, 0x8b, 0x59, 0x8a, 0x09, 0x40, 0xd0, 0xa6, 0x97, 0xa9,
      0xe0, 0xb9, 0xe9, 0x2c, 0xfa, 0xa3, 0x14, 0xf5, 0x83, 0xc9, 0xd0, 0x66
  };

  // Z = -12, c2 = sqrt(1728)
  EC_FELEM Z, c2;
  if (!felem_from_u8(group, &Z, 12) ||
      !ec_felem_from_bytes(group, &c2, kSqrt1728, sizeof(kSqrt1728))) {
    return 0;
  }
  ec_felem_neg(group, &Z, &Z);

  return hash_to_curve(group, EVP_sha512(), &Z, &c2, /*k=*/192, out, dst,
                       dst_len, msg, msg_len);
}

int ec_hash_to_scalar_p384_xmd_sha512_draft07(
    const EC_GROUP *group, EC_SCALAR *out, const uint8_t *dst, size_t dst_len,
    const uint8_t *msg, size_t msg_len) {
  if (EC_GROUP_get_curve_name(group) != NID_secp384r1) {
    OPENSSL_PUT_ERROR(EC, EC_R_GROUP_MISMATCH);
    return 0;
  }

  return hash_to_scalar(group, EVP_sha512(), out, dst, dst_len, /*k=*/192, msg,
                        msg_len);
}
