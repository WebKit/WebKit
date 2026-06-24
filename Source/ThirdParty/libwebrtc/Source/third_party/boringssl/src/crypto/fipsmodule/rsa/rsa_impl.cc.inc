// Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#include <openssl/rsa.h>

#include <assert.h>
#include <limits.h>
#include <string.h>

#include <utility>

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/mem.h>

#include "../../bcm_support.h"
#include "../../internal.h"
#include "../../mem_internal.h"
#include "../bn/internal.h"
#include "../delocate.h"
#include "../service_indicator/internal.h"
#include "internal.h"


using namespace bssl;

static_assert(OPENSSL_RSA_MAX_MODULUS_BITS <=
                  BN_MONTGOMERY_MAX_WORDS * BN_BITS2,
              "Max RSA size too big for Montgomery arithmetic");

int bssl::rsa_check_public_key(const RSA *rsa) {
  auto *impl = FromOpaque(rsa);

  if (impl->n == nullptr) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_VALUE_MISSING);
    return 0;
  }

  unsigned n_bits = BN_num_bits(impl->n.get());
  if (n_bits > OPENSSL_RSA_MAX_MODULUS_BITS) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_MODULUS_TOO_LARGE);
    return 0;
  }

  if (n_bits < OPENSSL_RSA_MIN_MODULUS_BITS) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_KEY_SIZE_TOO_SMALL);
    return 0;
  }

  // RSA moduli must be positive and odd. In addition to being necessary for RSA
  // in general, we cannot setup Montgomery reduction with even moduli.
  if (!BN_is_odd(impl->n.get()) || BN_is_negative(impl->n.get())) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_BAD_RSA_PARAMETERS);
    return 0;
  }

  static const unsigned kMaxExponentBits = 33;
  if (impl->e != nullptr) {
    // Reject e = 1, negative e, and even e. e must be odd to be relatively
    // prime with phi(n).
    unsigned e_bits = BN_num_bits(impl->e.get());
    if (e_bits < 2 || BN_is_negative(impl->e.get()) ||
        !BN_is_odd(impl->e.get())) {
      OPENSSL_PUT_ERROR(RSA, RSA_R_BAD_E_VALUE);
      return 0;
    }
    if (impl->flags & RSA_FLAG_LARGE_PUBLIC_EXPONENT) {
      // The caller has requested disabling DoS protections. Still, e must be
      // less than n.
      if (BN_ucmp(impl->n.get(), impl->e.get()) <= 0) {
        OPENSSL_PUT_ERROR(RSA, RSA_R_BAD_E_VALUE);
        return 0;
      }
    } else {
      // Mitigate DoS attacks by limiting the exponent size. 33 bits was chosen
      // as the limit based on the recommendations in [1] and [2]. Windows
      // CryptoAPI doesn't support values larger than 32 bits [3], so it is
      // unlikely that exponents larger than 32 bits are being used for anything
      // Windows commonly does.
      //
      // [1] https://www.imperialviolet.org/2012/03/16/rsae.html
      // [2] https://www.imperialviolet.org/2012/03/17/rsados.html
      // [3] https://msdn.microsoft.com/en-us/library/aa387685(VS.85).aspx
      if (e_bits > kMaxExponentBits) {
        OPENSSL_PUT_ERROR(RSA, RSA_R_BAD_E_VALUE);
        return 0;
      }

      // The upper bound on |e_bits| and lower bound on |n_bits| imply e is
      // bounded by n.
      assert(BN_ucmp(impl->n.get(), impl->e.get()) > 0);
    }
  } else if (!(impl->flags & RSA_FLAG_NO_PUBLIC_EXPONENT)) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_VALUE_MISSING);
    return 0;
  }

  return 1;
}

static int ensure_fixed_copy(UniquePtr<BIGNUM> *out, const BIGNUM *in,
                             int width) {
  if (*out != nullptr) {
    return 1;
  }
  UniquePtr<BIGNUM> copy(BN_dup(in));
  if (copy == nullptr || !bn_resize_words(copy.get(), width)) {
    return 0;
  }
  bn_secret(copy.get());
  *out = std::move(copy);
  return 1;
}

// freeze_private_key finishes initializing |rsa|'s private key components.
// After this function has returned, |rsa| may not be changed. This is needed
// because |RSA| is a public struct and, additionally, OpenSSL 1.1.0 opaquified
// it wrong (see https://github.com/openssl/openssl/issues/5158).
static int freeze_private_key(RSAImpl *rsa, BN_CTX *ctx) {
  rsa->lock.LockRead();
  int frozen = rsa->private_key_frozen;
  rsa->lock.UnlockRead();
  if (frozen) {
    return 1;
  }

  const BIGNUM *n_fixed;
  MutexWriteLock lock(&rsa->lock);
  if (rsa->private_key_frozen) {
    return 1;
  }

  // Check the public components are within DoS bounds.
  if (!rsa_check_public_key(rsa)) {
    return 0;
  }

  // Pre-compute various intermediate values, as well as copies of private
  // exponents with correct widths. Note that other threads may concurrently
  // read from |rsa->n|, |rsa->e|, etc., so any fixes must be in separate
  // copies. We use |mont_n->N|, |mont_p->N|, and |mont_q->N| as copies of |n|,
  // |p|, and |q| with the correct minimal widths.

  if (rsa->mont_n == nullptr) {
    rsa->mont_n.reset(BN_MONT_CTX_new_for_modulus(rsa->n.get(), ctx));
    if (rsa->mont_n == nullptr) {
      return 0;
    }
  }
  n_fixed = &rsa->mont_n->N;

  // The only public upper-bound of |rsa->d| is the bit length of |rsa->n|. The
  // ASN.1 serialization of RSA private keys unfortunately leaks the byte length
  // of |rsa->d|, but normalize it so we only leak it once, rather than per
  // operation.
  if (rsa->d != nullptr &&
      !ensure_fixed_copy(&rsa->d_fixed, rsa->d.get(), n_fixed->width)) {
    return 0;
  }

  if (rsa->e != nullptr && rsa->p != nullptr && rsa->q != nullptr) {
    // TODO: p and q are also CONSTTIME_SECRET but not yet marked as such
    // because the Montgomery code does things like test whether or not values
    // are zero. So the secret marking probably needs to happen inside that
    // code.

    if (rsa->mont_p == nullptr) {
      rsa->mont_p.reset(BN_MONT_CTX_new_consttime(rsa->p.get(), ctx));
      if (rsa->mont_p == nullptr) {
        return 0;
      }
    }

    if (rsa->mont_q == nullptr) {
      rsa->mont_q.reset(BN_MONT_CTX_new_consttime(rsa->q.get(), ctx));
      if (rsa->mont_q == nullptr) {
        return 0;
      }
    }

    if (rsa->dmp1 != nullptr && rsa->dmq1 != nullptr && rsa->iqmp != nullptr) {
      // CRT components are only publicly bounded by their corresponding
      // moduli's bit lengths.
      const BIGNUM *p_fixed = &rsa->mont_p->N;
      const BIGNUM *q_fixed = &rsa->mont_q->N;
      if (!ensure_fixed_copy(&rsa->dmp1_fixed, rsa->dmp1.get(),
                             p_fixed->width) ||
          !ensure_fixed_copy(&rsa->dmq1_fixed, rsa->dmq1.get(),
                             q_fixed->width)) {
        return 0;
      }

      // Compute |iqmp_mont|, which is |iqmp| in Montgomery form and with the
      // correct bit width.
      if (rsa->iqmp_mont == nullptr) {
        UniquePtr<BIGNUM> iqmp_mont(BN_new());
        if (iqmp_mont == nullptr ||
            !BN_to_montgomery(iqmp_mont.get(), rsa->iqmp.get(),
                              rsa->mont_p.get(), ctx)) {
          return 0;
        }
        rsa->iqmp_mont = std::move(iqmp_mont);
        bn_secret(rsa->iqmp_mont.get());
      }
    }
  }

  rsa->private_key_frozen = 1;
  return 1;
}

void bssl::rsa_invalidate_key(RSA *rsa) {
  auto *impl = FromOpaque(rsa);
  impl->private_key_frozen = 0;
  impl->mont_n = nullptr;
  impl->mont_p = nullptr;
  impl->mont_q = nullptr;
  impl->d_fixed = nullptr;
  impl->dmp1_fixed = nullptr;
  impl->dmq1_fixed = nullptr;
  impl->iqmp_mont = nullptr;
}

int bssl::rsa_default_sign_raw(RSA *rsa, size_t *out_len, uint8_t *out,
                               size_t max_out, const uint8_t *in, size_t in_len,
                               int padding) {
  const unsigned rsa_size = RSA_size(rsa);
  uint8_t *buf = nullptr;
  int i, ret = 0;

  if (max_out < rsa_size) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_OUTPUT_BUFFER_TOO_SMALL);
    return 0;
  }

  buf = reinterpret_cast<uint8_t *>(OPENSSL_malloc(rsa_size));
  if (buf == nullptr) {
    goto err;
  }

  switch (padding) {
    case RSA_PKCS1_PADDING:
      i = RSA_padding_add_PKCS1_type_1(buf, rsa_size, in, in_len);
      break;
    case RSA_NO_PADDING:
      i = RSA_padding_add_none(buf, rsa_size, in, in_len);
      break;
    default:
      OPENSSL_PUT_ERROR(RSA, RSA_R_UNKNOWN_PADDING_TYPE);
      goto err;
  }

  if (i <= 0) {
    goto err;
  }

  if (!rsa_private_transform_no_self_test(rsa, out, buf, rsa_size)) {
    goto err;
  }

  CONSTTIME_DECLASSIFY(out, rsa_size);
  *out_len = rsa_size;
  ret = 1;

err:
  OPENSSL_free(buf);

  return ret;
}


static int rsa_mod_exp_crt(BIGNUM *r0, const BIGNUM *I, RSAImpl *rsa,
                           BN_CTX *ctx);

int bssl::rsa_verify_raw_no_self_test(RSA *rsa, size_t *out_len, uint8_t *out,
                                      size_t max_out, const uint8_t *in,
                                      size_t in_len, int padding) {
  auto *impl = FromOpaque(rsa);

  if (impl->n == nullptr || impl->e == nullptr) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_VALUE_MISSING);
    return 0;
  }

  if (!rsa_check_public_key(rsa)) {
    return 0;
  }

  const unsigned rsa_size = RSA_size(rsa);
  if (max_out < rsa_size) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_OUTPUT_BUFFER_TOO_SMALL);
    return 0;
  }

  if (in_len != rsa_size) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_DATA_LEN_NOT_EQUAL_TO_MOD_LEN);
    return 0;
  }

  UniquePtr<BN_CTX> ctx(BN_CTX_new());
  if (ctx == nullptr) {
    return 0;
  }

  int ret = 0;
  uint8_t *buf = nullptr;
  BN_CTXScope scope(ctx.get());
  BIGNUM *f = BN_CTX_get(ctx.get());
  BIGNUM *result = BN_CTX_get(ctx.get());
  if (f == nullptr || result == nullptr) {
    goto err;
  }

  if (padding == RSA_NO_PADDING) {
    buf = out;
  } else {
    // Allocate a temporary buffer to hold the padded plaintext.
    buf = reinterpret_cast<uint8_t *>(OPENSSL_malloc(rsa_size));
    if (buf == nullptr) {
      goto err;
    }
  }

  if (BN_bin2bn(in, in_len, f) == nullptr) {
    goto err;
  }

  if (BN_ucmp(f, impl->n.get()) >= 0) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_DATA_TOO_LARGE_FOR_MODULUS);
    goto err;
  }

  if (!BN_MONT_CTX_set_locked(&impl->mont_n, &impl->lock, impl->n.get(),
                              ctx.get()) ||
      !BN_mod_exp_mont(result, f, impl->e.get(), &impl->mont_n->N, ctx.get(),
                       impl->mont_n.get())) {
    goto err;
  }

  if (!BN_bn2bin_padded(buf, rsa_size, result)) {
    OPENSSL_PUT_ERROR(RSA, ERR_R_INTERNAL_ERROR);
    goto err;
  }

  switch (padding) {
    case RSA_PKCS1_PADDING:
      ret =
          RSA_padding_check_PKCS1_type_1(out, out_len, rsa_size, buf, rsa_size);
      break;
    case RSA_NO_PADDING:
      ret = 1;
      *out_len = rsa_size;
      break;
    default:
      OPENSSL_PUT_ERROR(RSA, RSA_R_UNKNOWN_PADDING_TYPE);
      goto err;
  }

  if (!ret) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_PADDING_CHECK_FAILED);
    goto err;
  }

err:
  if (buf != out) {
    OPENSSL_free(buf);
  }
  return ret;
}

int RSA_verify_raw(RSA *rsa, size_t *out_len, uint8_t *out, size_t max_out,
                   const uint8_t *in, size_t in_len, int padding) {
  boringssl_ensure_rsa_verify_self_test();
  return rsa_verify_raw_no_self_test(rsa, out_len, out, max_out, in, in_len,
                                     padding);
}

int bssl::rsa_default_private_transform(RSA *rsa, uint8_t *out,
                                        const uint8_t *in, size_t len) {
  auto *impl = FromOpaque(rsa);

  if (impl->n == nullptr || impl->d == nullptr) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_VALUE_MISSING);
    return 0;
  }

  UniquePtr<BN_CTX> ctx(BN_CTX_new());
  if (ctx == nullptr) {
    return 0;
  }
  BN_CTXScope scope(ctx.get());
  BIGNUM *f = BN_CTX_get(ctx.get());
  BIGNUM *result = BN_CTX_get(ctx.get());
  if (f == nullptr || result == nullptr) {
    return 0;
  }

  // The caller should have ensured this.
  assert(len == BN_num_bytes(impl->n.get()));
  if (BN_bin2bn(in, len, f) == nullptr) {
    return 0;
  }

  // The input to the RSA private transform may be secret, but padding is
  // expected to construct a value within range, so we can leak this comparison.
  if (constant_time_declassify_int(BN_ucmp(f, impl->n.get()) >= 0)) {
    // Usually the padding functions would catch this.
    OPENSSL_PUT_ERROR(RSA, RSA_R_DATA_TOO_LARGE_FOR_MODULUS);
    return 0;
  }

  if (!freeze_private_key(impl, ctx.get())) {
    OPENSSL_PUT_ERROR(RSA, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  if (impl->e == nullptr && (impl->flags & RSA_FLAG_NO_PUBLIC_EXPONENT) == 0) {
    // Unless the private key was specifically created with an API like
    // |RSA_new_private_key_no_e|, don't allow RSA keys to be missing the public
    // exponent, which disables some fault attack mitigations. (It should not be
    // possible to construct such an |RSA| object in the public API.)
    OPENSSL_PUT_ERROR(RSA, RSA_R_NO_PUBLIC_EXPONENT);
    return 0;
  }

  if (impl->p != nullptr && impl->q != nullptr && impl->e != nullptr &&
      impl->dmp1 != nullptr && impl->dmq1 != nullptr && impl->iqmp != nullptr &&
      // Require that we can reduce |f| by |impl->p| and |impl->q| in constant
      // time, which requires primes be the same size, rounded to the Montgomery
      // coefficient. (See |mod_montgomery|.) This is not required by RFC 8017,
      // but it is true for keys generated by us and all common implementations.
      bn_less_than_montgomery_R(impl->q.get(), impl->mont_p.get()) &&
      bn_less_than_montgomery_R(impl->p.get(), impl->mont_q.get())) {
    if (!rsa_mod_exp_crt(result, f, impl, ctx.get())) {
      return 0;
    }
  } else if (!BN_mod_exp_mont_consttime(result, f, impl->d_fixed.get(),
                                        impl->n.get(), ctx.get(),
                                        impl->mont_n.get())) {
    return 0;
  }

  // Verify the result to protect against fault attacks as described in the
  // 1997 paper "On the Importance of Checking Cryptographic Protocols for
  // Faults" by Dan Boneh, Richard A. DeMillo, and Richard J. Lipton. Some
  // implementations do this only when the CRT is used, but we do it in all
  // cases. Section 6 of the aforementioned paper describes an attack that
  // works when the CRT isn't used. That attack is much less likely to succeed
  // than the CRT attack, but there have likely been improvements since 1997.
  //
  // This check is cheap assuming |e| is small, which we require in
  // |rsa_check_public_key|.
  if (impl->e != nullptr) {
    BIGNUM *vrfy = BN_CTX_get(ctx.get());
    if (vrfy == nullptr ||
        !BN_mod_exp_mont(vrfy, result, impl->e.get(), impl->n.get(), ctx.get(),
                         impl->mont_n.get()) ||
        !constant_time_declassify_int(BN_equal_consttime(vrfy, f))) {
      OPENSSL_PUT_ERROR(RSA, ERR_R_INTERNAL_ERROR);
      return 0;
    }
  }

  // The computation should have left |result| as a maximally-wide number, so
  // that it and serializing does not leak information about the magnitude of
  // the result.
  //
  // See Falko Strenzke, "Manger's Attack revisited", ICICS 2010.
  assert(result->width == impl->mont_n->N.width);
  bn_assert_fits_in_bytes(result, len);
  if (!BN_bn2bin_padded(out, len, result)) {
    OPENSSL_PUT_ERROR(RSA, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  return 1;
}

// mod_montgomery sets |r| to |I| mod |p|. |I| must already be fully reduced
// modulo |p| times |q|. It returns one on success and zero on error.
static int mod_montgomery(BIGNUM *r, const BIGNUM *I, const BIGNUM *p,
                          const BN_MONT_CTX *mont_p, const BIGNUM *q,
                          BN_CTX *ctx) {
  // Reducing in constant-time with Montgomery reduction requires I <= p * R. We
  // have I < p * q, so this follows if q < R. The caller should have checked
  // this already.
  if (!bn_less_than_montgomery_R(q, mont_p)) {
    OPENSSL_PUT_ERROR(RSA, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  if (  // Reduce mod p with Montgomery reduction. This computes I * R^-1 mod p.
      !BN_from_montgomery(r, I, mont_p, ctx) ||
      // Multiply by R^2 and do another Montgomery reduction to compute
      // I * R^-1 * R^2 * R^-1 = I mod p.
      !BN_to_montgomery(r, r, mont_p, ctx)) {
    return 0;
  }

  // By precomputing R^3 mod p (normally |BN_MONT_CTX| only uses R^2 mod p) and
  // adjusting the API for |BN_mod_exp_mont_consttime|, we could instead compute
  // I * R mod p here and save a reduction per prime. But this would require
  // changing the RSAZ code and may not be worth it. Note that the RSAZ code
  // uses a different radix, so it uses R' = 2^1044. There we'd actually want
  // R^2 * R', and would further benefit from a precomputed R'^2. It currently
  // converts |mont_p->RR| to R'^2.
  return 1;
}

static int rsa_mod_exp_crt(BIGNUM *r0, const BIGNUM *I, RSAImpl *rsa,
                           BN_CTX *ctx) {
  assert(ctx != nullptr);

  assert(rsa->n != nullptr);
  assert(rsa->e != nullptr);
  assert(rsa->d != nullptr);
  assert(rsa->p != nullptr);
  assert(rsa->q != nullptr);
  assert(rsa->dmp1 != nullptr);
  assert(rsa->dmq1 != nullptr);
  assert(rsa->iqmp != nullptr);

  BN_CTXScope scope(ctx);
  BIGNUM *r1 = BN_CTX_get(ctx);
  BIGNUM *m1 = BN_CTX_get(ctx);
  if (r1 == nullptr || m1 == nullptr) {
    return 0;
  }

  // Use the minimal-width versions of |n|, |p|, and |q|. Either works, but if
  // someone gives us non-minimal values, these will be slightly more efficient
  // on the non-Montgomery operations.
  BIGNUM *n = &rsa->mont_n->N;
  BIGNUM *p = &rsa->mont_p->N;
  BIGNUM *q = &rsa->mont_q->N;

  // This is a pre-condition for |mod_montgomery|. It was already checked by the
  // caller.
  declassify_assert(BN_ucmp(I, n) < 0);

  if (  // |m1| is the result modulo |q|.
      !mod_montgomery(r1, I, q, rsa->mont_q.get(), p, ctx) ||
      !BN_mod_exp_mont_consttime(m1, r1, rsa->dmq1_fixed.get(), q, ctx,
                                 rsa->mont_q.get()) ||
      // |r0| is the result modulo |p|.
      !mod_montgomery(r1, I, p, rsa->mont_p.get(), q, ctx) ||
      !BN_mod_exp_mont_consttime(r0, r1, rsa->dmp1_fixed.get(), p, ctx,
                                 rsa->mont_p.get()) ||
      // Compute r0 = r0 - m1 mod p. |m1| is reduced mod |q|, not |p|, so we
      // just run |mod_montgomery| again for srsaicity. This could be more
      // efficient with more cases: if |p > q|, |m1| is already reduced. If
      // |p < q| but they have the same bit width, |bn_reduce_once| suffices.
      // However, compared to over 2048 Montgomery multiplications above, this
      // difference is not measurable.
      !mod_montgomery(r1, m1, p, rsa->mont_p.get(), q, ctx) ||
      !bn_mod_sub_consttime(r0, r0, r1, p, ctx) ||
      // r0 = r0 * iqmp mod p. We use Montgomery multiplication to compute this
      // in constant time. |iqmp_mont| is in Montgomery form and r0 is not, so
      // the result is taken out of Montgomery form.
      !BN_mod_mul_montgomery(r0, r0, rsa->iqmp_mont.get(), rsa->mont_p.get(),
                             ctx) ||
      // r0 = r0 * q + m1 gives the final result. Reducing modulo q gives m1, so
      // it is correct mod p. Reducing modulo p gives (r0-m1)*iqmp*q + m1 = r0,
      // so it is correct mod q. Finally, the result is bounded by [m1, n + m1),
      // and the result is at least |m1|, so this must be the unique answer in
      // [0, n).
      !bn_mul_consttime(r0, r0, q, ctx) ||  //
      !bn_uadd_consttime(r0, r0, m1)) {
    return 0;
  }

  // The result should be bounded by |n|, but fixed-width operations may
  // bound the width slightly higher, so fix it. This trips constant-time checks
  // because a naive data flow analysis does not realize the excess words are
  // publicly zero.
  declassify_assert(BN_cmp(r0, n) < 0);
  bn_assert_fits_in_bytes(r0, BN_num_bytes(n));
  if (!bn_resize_words(r0, n->width)) {
    return 0;
  }

  return 1;
}

static int ensure_bignum(UniquePtr<BIGNUM> *out) {
  if (*out == nullptr) {
    out->reset(BN_new());
  }
  return *out != nullptr;
}

// generate_prime sets |out| to a prime with length |bits| such that |out|-1 is
// relatively prime to |e|. If |p| is non-NULL, |out| will also not be close to
// |p|. |pow2_bits_100| must be 2^(bits-100).
//
// This function fails with probability around 2^-21.
static int generate_prime(BIGNUM *out, int bits, const BIGNUM *e,
                          const BIGNUM *p, const BIGNUM *pow2_bits_100,
                          BN_CTX *ctx, BN_GENCB *cb) {
  if (bits < 128 || (bits % BN_BITS2) != 0) {
    OPENSSL_PUT_ERROR(RSA, ERR_R_INTERNAL_ERROR);
    return 0;
  }
  assert(BN_is_pow2(pow2_bits_100));
  assert(BN_is_bit_set(pow2_bits_100, bits - 100));

  // See FIPS 186-5 appendix A.1.3, steps 4 and 5. Note |bits| here is nlen/2.

  // Use the limit from steps 4.7 and 5.8 for most values of |e|. When |e| is 3,
  // the 186-5 limit is too low, so we use a higher one. Note this case is not
  // reachable from |RSA_generate_key_fips|.
  //
  // |limit| determines the failure probability. We must find a prime that is
  // not 1 mod |e|. By the prime number theorem, we'll find one with probability
  // p = (e-1)/e * 2/(ln(2)*bits). Note the second term is doubled because we
  // discard even numbers.
  //
  // The failure probability is thus (1-p)^limit. To convert that to a power of
  // two, we take logs. -log_2((1-p)^limit) = -limit * ln(1-p) / ln(2).
  //
  // >>> def f(bits, e, limit):
  // ...   p = (e-1.0)/e * 2.0/(math.log(2)*bits)
  // ...   return -limit * math.log(1 - p) / math.log(2)
  // ...
  // >>> f(1024, 65537, 5*1024)
  // 20.842750558272634
  // >>> f(1536, 65537, 5*1536)
  // 20.83294549602474
  // >>> f(2048, 65537, 5*2048)
  // 20.828047576234948
  // >>> f(1024, 3, 8*1024)
  // 22.222147925962307
  // >>> f(1536, 3, 8*1536)
  // 22.21518251065506
  // >>> f(2048, 3, 8*2048)
  // 22.211701985875937
  if (bits >= INT_MAX / 32) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_MODULUS_TOO_LARGE);
    return 0;
  }
  int limit = BN_is_word(e, 3) ? bits * 8 : bits * 5;

  int tries = 0, rand_tries = 0;
  BN_CTXScope scope(ctx);
  BIGNUM *tmp = BN_CTX_get(ctx);
  if (tmp == nullptr) {
    return 0;
  }

  for (;;) {
    // Generate a random number of length |bits| where the bottom bit is set and
    // top two bits are set (steps 4.2–4.4 and 5.2–5.4):
    //
    // - Setting the top two bits is permitted by steps 4.2.1 and 5.2.1. Doing
    //   so implements steps 4.4 and 5.4 by making this case impossible because
    //   √2 < 1.5.
    //
    // - Setting the bottom bit implements steps 4.3 and 5.3.
    if (!BN_rand(out, bits, BN_RAND_TOP_TWO, BN_RAND_BOTTOM_ODD) ||
        !BN_GENCB_call(cb, BN_GENCB_GENERATED, rand_tries++)) {
      return 0;
    }

    if (p != nullptr) {
      // If |p| and |out| are too close, try again (step 5.5).
      if (!bn_abs_sub_consttime(tmp, out, p, ctx)) {
        return 0;
      }
      if (BN_cmp(tmp, pow2_bits_100) <= 0) {
        continue;
      }
    }

    // RSA key generation's bottleneck is discarding composites. If it fails
    // trial division, do not bother computing a GCD or performing Miller-Rabin.
    if (!bn_odd_number_is_obviously_composite(out)) {
      // Check gcd(out-1, e) is one (steps 4.5 and 5.6). Leaking the final
      // result of this comparison is safe because, if not relatively prime, the
      // value will be discarded.
      int relatively_prime;
      if (!bn_usub_consttime(tmp, out, BN_value_one()) ||
          !bn_is_relatively_prime(&relatively_prime, tmp, e, ctx)) {
        return 0;
      }
      if (constant_time_declassify_int(relatively_prime)) {
        // Test |out| for primality (steps 4.5.1 and 5.6.1).
        int is_probable_prime;
        if (!BN_primality_test(&is_probable_prime, out,
                               BN_prime_checks_for_generation, ctx, 0, cb)) {
          return 0;
        }
        if (is_probable_prime) {
          return 1;
        }
      }
    }

    // If we've tried too many times to find a prime, abort (steps 4.7 and 5.8).
    tries++;
    if (tries >= limit) {
      OPENSSL_PUT_ERROR(RSA, RSA_R_TOO_MANY_ITERATIONS);
      return 0;
    }
    if (!BN_GENCB_call(cb, 2, tries)) {
      return 0;
    }
  }
}

// rsa_generate_key_impl generates an RSA key using a generalized version of
// FIPS 186-5 appendix A.1.3. |RSA_generate_key_fips| performs additional checks
// for FIPS-compliant key generation.
//
// This function returns one on success and zero on failure. It has a failure
// probability of about 2^-20.
static int rsa_generate_key_impl(RSAImpl *rsa, int bits, const BIGNUM *e_value,
                                 BN_GENCB *cb) {
  if (bits > OPENSSL_RSA_MAX_MODULUS_BITS) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_MODULUS_TOO_LARGE);
    return 0;
  }

  // Always generate RSA keys which are a multiple of 128 bits. Round |bits|
  // down as needed.
  bits &= ~127;

  // Reject excessively small keys.
  if (bits < OPENSSL_RSA_MIN_MODULUS_BITS) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_KEY_SIZE_TOO_SMALL);
    return 0;
  }

  // Reject excessively large public exponents. Windows CryptoAPI and Go don't
  // support values larger than 32 bits, so match their limits for generating
  // keys. (|rsa_check_public_key| uses a slightly more conservative value, but
  // we don't need to support generating such keys.)
  // https://github.com/golang/go/issues/3161
  // https://msdn.microsoft.com/en-us/library/aa387685(VS.85).aspx
  if (BN_num_bits(e_value) > 32) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_BAD_E_VALUE);
    return 0;
  }

  // The smallest reasonable RSA exponent is 3, and it definitely must be odd.
  // Catching these here prevents endless loops or slow computation when trying
  // to generate keys later, and results in a better error code.
  if (
      // Would fail in |bn_lcm_consttime| as it only allows positive integers.
      BN_is_negative(e_value) ||
      // Would fail in |generate_prime| as only one |rsa->p|-1 is coprime with
      // an even |e_value| and that one is a little bit short. (The R in RSA
      // doesn't stand for Rabin.)
      !BN_is_odd(e_value) ||
      // Would loop endlessly because it'll always compute an |rsa->d| exponent
      // of 1, which is too small.
      BN_is_one(e_value)) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_BAD_E_VALUE);
    return 0;
  }

  UniquePtr<BN_CTX> ctx(BN_CTX_new());
  if (ctx == nullptr) {
    OPENSSL_PUT_ERROR(RSA, ERR_LIB_BN);
    return 0;
  }

  int prime_bits = bits / 2;
  BN_CTXScope scope(ctx.get());
  BIGNUM *totient = BN_CTX_get(ctx.get());
  BIGNUM *pm1 = BN_CTX_get(ctx.get());
  BIGNUM *qm1 = BN_CTX_get(ctx.get());
  BIGNUM *pow2_prime_bits_100 = BN_CTX_get(ctx.get());
  BIGNUM *pow2_prime_bits = BN_CTX_get(ctx.get());
  if (totient == nullptr || pm1 == nullptr || qm1 == nullptr ||
      pow2_prime_bits_100 == nullptr || pow2_prime_bits == nullptr ||
      !BN_set_bit(pow2_prime_bits_100, prime_bits - 100) ||
      !BN_set_bit(pow2_prime_bits, prime_bits)) {
    OPENSSL_PUT_ERROR(RSA, ERR_LIB_BN);
    return 0;
  }

  // We need the RSA components non-null.
  if (!ensure_bignum(&rsa->n) ||     //
      !ensure_bignum(&rsa->d) ||     //
      !ensure_bignum(&rsa->e) ||     //
      !ensure_bignum(&rsa->p) ||     //
      !ensure_bignum(&rsa->q) ||     //
      !ensure_bignum(&rsa->dmp1) ||  //
      !ensure_bignum(&rsa->dmq1) ||  //
      !ensure_bignum(&rsa->iqmp)) {
    OPENSSL_PUT_ERROR(RSA, ERR_LIB_BN);
    return 0;
  }

  if (!BN_copy(rsa->e.get(), e_value)) {
    OPENSSL_PUT_ERROR(RSA, ERR_LIB_BN);
    return 0;
  }

  do {
    // Generate p and q, each of size |prime_bits|, using the steps outlined in
    // appendix FIPS 186-5 appendix C.3.3.
    //
    // Each call to |generate_prime| fails with probability p = 2^-21. The
    // probability that either call fails is 1 - (1-p)^2, which is around 2^-20.
    if (!generate_prime(rsa->p.get(), prime_bits, rsa->e.get(), nullptr,
                        pow2_prime_bits_100, ctx.get(), cb) ||
        !BN_GENCB_call(cb, 3, 0) ||
        !generate_prime(rsa->q.get(), prime_bits, rsa->e.get(), rsa->p.get(),
                        pow2_prime_bits_100, ctx.get(), cb) ||
        !BN_GENCB_call(cb, 3, 1)) {
      OPENSSL_PUT_ERROR(RSA, ERR_LIB_BN);
      return 0;
    }

    if (BN_cmp(rsa->p.get(), rsa->q.get()) < 0) {
      std::swap(rsa->p, rsa->q);
    }

    // Calculate d = e^(-1) (mod lcm(p-1, q-1)), per FIPS 186-5. This differs
    // from typical RSA rsaementations which use (p-1)*(q-1).
    //
    // Note this means the size of d might reveal information about p-1 and
    // q-1. However, we do operations with Chinese Remainder Theorem, so we only
    // use d (mod p-1) and d (mod q-1) as exponents. Using a minimal totient
    // does not affect those two values.
    int no_inverse;
    if (!bn_usub_consttime(pm1, rsa->p.get(), BN_value_one()) ||
        !bn_usub_consttime(qm1, rsa->q.get(), BN_value_one()) ||
        !bn_lcm_consttime(totient, pm1, qm1, ctx.get()) ||
        !bn_mod_inverse_consttime(rsa->d.get(), &no_inverse, rsa->e.get(),
                                  totient, ctx.get())) {
      OPENSSL_PUT_ERROR(RSA, ERR_LIB_BN);
      return 0;
    }

    // Retry if |rsa->d| <= 2^|prime_bits|. See appendix A.3.1's guidance on
    // values for d. When we retry, p and q are discarded, so it is safe to leak
    // this comparison.
  } while (
      constant_time_declassify_int(BN_cmp(rsa->d.get(), pow2_prime_bits) <= 0));

  assert(BN_num_bits(pm1) == (unsigned)prime_bits);
  assert(BN_num_bits(qm1) == (unsigned)prime_bits);
  if (  // Calculate n.
      !bn_mul_consttime(rsa->n.get(), rsa->p.get(), rsa->q.get(), ctx.get()) ||
      // Calculate d mod (p-1).
      !bn_div_consttime(nullptr, rsa->dmp1.get(), rsa->d.get(), pm1, prime_bits,
                        ctx.get()) ||
      // Calculate d mod (q-1)
      !bn_div_consttime(nullptr, rsa->dmq1.get(), rsa->d.get(), qm1, prime_bits,
                        ctx.get())) {
    OPENSSL_PUT_ERROR(RSA, ERR_LIB_BN);
    return 0;
  }
  bn_set_minimal_width(rsa->n.get());

  // |rsa->n| is computed from the private key, but is public.
  bn_declassify(rsa->n.get());

  // Calculate q^-1 mod p.
  rsa->mont_p.reset(BN_MONT_CTX_new_consttime(rsa->p.get(), ctx.get()));
  if (rsa->mont_p == nullptr ||  //
      !bn_mod_inverse_secret_prime(rsa->iqmp.get(), rsa->q.get(), rsa->p.get(),
                                   ctx.get(), rsa->mont_p.get())) {
    OPENSSL_PUT_ERROR(RSA, ERR_LIB_BN);
    return 0;
  }

  // Sanity-check that |rsa->n| has the specified size. This is rsaied by
  // |generate_prime|'s bounds.
  if (BN_num_bits(rsa->n.get()) != (unsigned)bits) {
    OPENSSL_PUT_ERROR(RSA, ERR_R_INTERNAL_ERROR);
    return 0;
  }

  // The key generation process is complex and thus error-prone. It could be
  // disastrous to generate and then use a bad key so double-check that the key
  // makes sense. Also, while |rsa| is mutable, fill in the cached components.
  if (!RSA_check_key(rsa) || !freeze_private_key(rsa, ctx.get())) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_INTERNAL_ERROR);
    return 0;
  }

  return 1;
}

static int RSA_generate_key_ex_maybe_fips(RSAImpl *rsa, int bits,
                                          const BIGNUM *e_value, BN_GENCB *cb,
                                          int check_fips) {
  boringssl_ensure_rsa_sign_self_test();
  boringssl_ensure_rsa_verify_self_test();

  if (rsa == nullptr) {
    OPENSSL_PUT_ERROR(EC, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
  }

  UniquePtr<RSAImpl> tmp;

  // |rsa_generate_key_impl|'s 2^-20 failure probability is too high at scale,
  // so we run the FIPS algorithm four times, bringing it down to 2^-80. We
  // should just adjust the retry limit, but FIPS 186-5 prescribes that value
  // and thus results in unnecessary complexity.
  int failures = 0;
  do {
    ERR_clear_error();
    // Generate into scratch space, to avoid leaving partial work on failure.
    tmp.reset(FromOpaque(RSA_new()));
    if (tmp == nullptr) {
      return 0;
    }

    if (rsa_generate_key_impl(tmp.get(), bits, e_value, cb)) {
      break;
    }

    tmp = nullptr;
    failures++;

    // Only retry on |RSA_R_TOO_MANY_ITERATIONS|. This is so a caller-induced
    // failure in |BN_GENCB_call| is still fatal.
  } while (failures < 4 && ERR_equals(ERR_peek_error(), ERR_LIB_RSA,
                                      RSA_R_TOO_MANY_ITERATIONS));

  if (tmp == nullptr || (check_fips && !RSA_check_fips(tmp.get()))) {
    return 0;
  }

  rsa_invalidate_key(rsa);
  rsa->n = std::move(tmp->n);
  rsa->e = std::move(tmp->e);
  rsa->d = std::move(tmp->d);
  rsa->p = std::move(tmp->p);
  rsa->q = std::move(tmp->q);
  rsa->dmp1 = std::move(tmp->dmp1);
  rsa->dmq1 = std::move(tmp->dmq1);
  rsa->iqmp = std::move(tmp->iqmp);
  rsa->mont_n = std::move(tmp->mont_n);
  rsa->mont_p = std::move(tmp->mont_p);
  rsa->mont_q = std::move(tmp->mont_q);
  rsa->d_fixed = std::move(tmp->d_fixed);
  rsa->dmp1_fixed = std::move(tmp->dmp1_fixed);
  rsa->dmq1_fixed = std::move(tmp->dmq1_fixed);
  rsa->iqmp_mont = std::move(tmp->iqmp_mont);
  rsa->private_key_frozen = tmp->private_key_frozen;
  return 1;
}

int RSA_generate_key_ex(RSA *rsa, int bits, const BIGNUM *e_value,
                        BN_GENCB *cb) {
  return RSA_generate_key_ex_maybe_fips(FromOpaque(rsa), bits, e_value, cb,
                                        /*check_fips=*/0);
}

int RSA_generate_key_fips(RSA *rsa, int bits, BN_GENCB *cb) {
  // FIPS 186-4 allowed 2048-bit and 3072-bit RSA keys (1024-bit and 1536-bit
  // primes, respectively) with the prime generation method we use.
  // Subsequently, IG A.14 stated that larger modulus sizes can be used and ACVP
  // testing supports 4096 bits, and FIPS 186-5 allowed all key sizes at least
  // 2048.
  if (bits != 2048 && bits != 3072 && bits != 4096) {
    OPENSSL_PUT_ERROR(RSA, RSA_R_BAD_RSA_PARAMETERS);
    return 0;
  }

  BIGNUM *e = BN_new();
  int ret = e != nullptr && BN_set_word(e, RSA_F4) &&
            RSA_generate_key_ex_maybe_fips(FromOpaque(rsa), bits, e, cb,
                                           /*check_fips=*/1);
  BN_free(e);

  if (ret) {
    FIPS_service_indicator_update_state();
  }
  return ret;
}

BSSL_NAMESPACE_BEGIN

DEFINE_METHOD_FUNCTION(RSA_METHOD, RSA_default_method) {
  // All of the methods are NULL to make it easier for the compiler/linker to
  // drop unused functions. The wrapper functions will select the appropriate
  // |rsa_default_*| implementation.
  OPENSSL_memset(out, 0, sizeof(RSA_METHOD));
  out->common.is_static = 1;
}

BSSL_NAMESPACE_END
