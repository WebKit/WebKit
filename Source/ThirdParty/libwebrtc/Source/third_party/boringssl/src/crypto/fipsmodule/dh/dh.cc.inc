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

#include <openssl/dh.h>

#include <string.h>

#include <iterator>

#include <openssl/bn.h>
#include <openssl/digest.h>
#include <openssl/err.h>
#include <openssl/mem.h>

#include "../../internal.h"
#include "../../mem_internal.h"
#include "../bn/internal.h"
#include "../service_indicator/internal.h"
#include "internal.h"


using namespace bssl;

DH *DH_new() { return New<DHImpl>(); }

void DH_free(DH *dh) {
  if (dh != nullptr) {
    FromOpaque(dh)->DecRefInternal();
  }
}

unsigned DH_bits(const DH *dh) { return BN_num_bits(FromOpaque(dh)->p.get()); }

const BIGNUM *DH_get0_pub_key(const DH *dh) {
  return FromOpaque(dh)->pub_key.get();
}

const BIGNUM *DH_get0_priv_key(const DH *dh) {
  return FromOpaque(dh)->priv_key.get();
}

const BIGNUM *DH_get0_p(const DH *dh) { return FromOpaque(dh)->p.get(); }

const BIGNUM *DH_get0_q(const DH *dh) { return FromOpaque(dh)->q.get(); }

const BIGNUM *DH_get0_g(const DH *dh) { return FromOpaque(dh)->g.get(); }

void DH_get0_key(const DH *dh, const BIGNUM **out_pub_key,
                 const BIGNUM **out_priv_key) {
  auto *impl = FromOpaque(dh);
  if (out_pub_key != nullptr) {
    *out_pub_key = impl->pub_key.get();
  }
  if (out_priv_key != nullptr) {
    *out_priv_key = impl->priv_key.get();
  }
}

int DH_set0_key(DH *dh, BIGNUM *pub_key, BIGNUM *priv_key) {
  auto *impl = FromOpaque(dh);
  if (pub_key != nullptr) {
    impl->pub_key.reset(pub_key);
  }

  if (priv_key != nullptr) {
    impl->priv_key.reset(priv_key);
  }

  return 1;
}

void DH_get0_pqg(const DH *dh, const BIGNUM **out_p, const BIGNUM **out_q,
                 const BIGNUM **out_g) {
  auto *impl = FromOpaque(dh);
  if (out_p != nullptr) {
    *out_p = impl->p.get();
  }
  if (out_q != nullptr) {
    *out_q = impl->q.get();
  }
  if (out_g != nullptr) {
    *out_g = impl->g.get();
  }
}

int DH_set0_pqg(DH *dh, BIGNUM *p, BIGNUM *q, BIGNUM *g) {
  auto *impl = FromOpaque(dh);
  if ((impl->p == nullptr && p == nullptr) ||
      (impl->g == nullptr && g == nullptr)) {
    return 0;
  }

  if (p != nullptr) {
    impl->p.reset(p);
  }

  if (q != nullptr) {
    impl->q.reset(q);
  }

  if (g != nullptr) {
    impl->g.reset(g);
  }

  // Invalidate the cached Montgomery parameters.
  impl->method_mont_p = nullptr;
  return 1;
}

int DH_set_length(DH *dh, unsigned priv_length) {
  auto *impl = FromOpaque(dh);
  impl->priv_length = priv_length;
  return 1;
}

int DH_generate_key(DH *dh) {
  boringssl_ensure_ffdh_self_test();

  if (!dh_check_params_fast(dh)) {
    return 0;
  }

  auto *impl = FromOpaque(dh);
  UniquePtr<BN_CTX> ctx(BN_CTX_new());
  if (ctx == nullptr) {
    OPENSSL_PUT_ERROR(DH, ERR_R_BN_LIB);
    return 0;
  }

  if (!BN_MONT_CTX_set_locked(&impl->method_mont_p, &impl->method_mont_p_lock,
                              impl->p.get(), ctx.get())) {
    OPENSSL_PUT_ERROR(DH, ERR_R_BN_LIB);
    return 0;
  }

  // Only generate a private key if there's already one. Otherwise,
  // |DH_generate_key| recomputes the public key.
  const BIGNUM *priv_key = impl->priv_key.get();
  UniquePtr<BIGNUM> new_priv_key;
  if (priv_key == nullptr) {
    new_priv_key.reset(BN_new());
    if (new_priv_key == nullptr) {
      OPENSSL_PUT_ERROR(DH, ERR_R_BN_LIB);
      return 0;
    }
    if (impl->q) {
      // Section 5.6.1.1.4 of SP 800-56A Rev3 generates a private key uniformly
      // from [1, min(2^N-1, q-1)].
      //
      // Although SP 800-56A Rev3 now permits a private key length N,
      // |impl->priv_length| historically was ignored when q is available. We
      // continue to ignore it and interpret such a configuration as N = len(q).
      if (!BN_rand_range_ex(new_priv_key.get(), 1, impl->q.get())) {
        OPENSSL_PUT_ERROR(DH, ERR_R_BN_LIB);
        return 0;
      }
    } else {
      // If q is unspecified, we expect p to be a safe prime, with g generating
      // the (p-1)/2 subgroup. So, we use q = (p-1)/2. (If g generates a smaller
      // prime-order subgroup, q will still divide (p-1)/2.)
      //
      // We set N from |impl->priv_length|. Section 5.6.1.1.4 of SP 800-56A Rev3
      // says to reject N > len(q), or N > num_bits(p) - 1. However, this logic
      // originally aligned with PKCS#3, which allows num_bits(p). Instead, we
      // clamp |impl->priv_length| before invoking the algorithm.

      // Compute M = min(2^N, q).
      UniquePtr<BIGNUM> priv_key_limit(BN_new());
      if (priv_key_limit == nullptr) {
        OPENSSL_PUT_ERROR(DH, ERR_R_BN_LIB);
        return 0;
      }
      if (impl->priv_length == 0 ||
          impl->priv_length >= BN_num_bits(impl->p.get()) - 1) {
        // M = q = (p - 1) / 2.
        if (!BN_rshift1(priv_key_limit.get(), impl->p.get())) {
          OPENSSL_PUT_ERROR(DH, ERR_R_BN_LIB);
          return 0;
        }
      } else {
        // M = 2^N.
        if (!BN_set_bit(priv_key_limit.get(), impl->priv_length)) {
          OPENSSL_PUT_ERROR(DH, ERR_R_BN_LIB);
          return 0;
        }
      }

      // Choose a private key uniformly from [1, M-1].
      if (!BN_rand_range_ex(new_priv_key.get(), 1, priv_key_limit.get())) {
        OPENSSL_PUT_ERROR(DH, ERR_R_BN_LIB);
        return 0;
      }
    }
    priv_key = new_priv_key.get();
  }

  UniquePtr<BIGNUM> new_pub_key(BN_new());
  if (new_pub_key == nullptr ||
      !BN_mod_exp_mont_consttime(new_pub_key.get(), impl->g.get(), priv_key,
                                 impl->p.get(), ctx.get(),
                                 impl->method_mont_p.get())) {
    OPENSSL_PUT_ERROR(DH, ERR_R_BN_LIB);
    return 0;
  }

  impl->pub_key = std::move(new_pub_key);
  if (new_priv_key != nullptr) {
    impl->priv_key = std::move(new_priv_key);
  }
  return 1;
}

static int dh_compute_key(DH *dh, BIGNUM *out_shared_key,
                          const BIGNUM *peers_key, BN_CTX *ctx) {
  auto *impl = FromOpaque(dh);

  if (!dh_check_params_fast(dh)) {
    return 0;
  }

  if (impl->priv_key == nullptr) {
    OPENSSL_PUT_ERROR(DH, DH_R_NO_PRIVATE_VALUE);
    return 0;
  }

  int check_result;
  if (!DH_check_pub_key(dh, peers_key, &check_result) || check_result) {
    OPENSSL_PUT_ERROR(DH, DH_R_INVALID_PUBKEY);
    return 0;
  }

  BN_CTXScope scope(ctx);
  BIGNUM *p_minus_1 = BN_CTX_get(ctx);
  if (!p_minus_1 ||
      !BN_MONT_CTX_set_locked(&impl->method_mont_p, &impl->method_mont_p_lock,
                              impl->p.get(), ctx)) {
    return 0;
  }

  if (!BN_mod_exp_mont_consttime(out_shared_key, peers_key,
                                 impl->priv_key.get(), impl->p.get(), ctx,
                                 impl->method_mont_p.get()) ||
      !BN_copy(p_minus_1, impl->p.get()) || !BN_sub_word(p_minus_1, 1)) {
    OPENSSL_PUT_ERROR(DH, ERR_R_BN_LIB);
    return 0;
  }

  // This performs the check required by SP 800-56Ar3 section 5.7.1.1 step two.
  if (BN_cmp_word(out_shared_key, 1) <= 0 ||
      BN_cmp(out_shared_key, p_minus_1) == 0) {
    OPENSSL_PUT_ERROR(DH, DH_R_INVALID_PUBKEY);
    return 0;
  }

  return 1;
}

int bssl::dh_compute_key_padded_no_self_test(unsigned char *out,
                                             const BIGNUM *peers_key, DH *dh) {
  UniquePtr<BN_CTX> ctx(BN_CTX_new());
  if (ctx == nullptr) {
    return -1;
  }
  BN_CTXScope scope(ctx.get());
  int dh_size = DH_size(dh);
  BIGNUM *shared_key = BN_CTX_get(ctx.get());
  if (shared_key == nullptr ||
      !dh_compute_key(dh, shared_key, peers_key, ctx.get()) ||
      !BN_bn2bin_padded(out, dh_size, shared_key)) {
    return -1;
  }
  return dh_size;
}

int DH_compute_key_padded(unsigned char *out, const BIGNUM *peers_key, DH *dh) {
  boringssl_ensure_ffdh_self_test();

  return dh_compute_key_padded_no_self_test(out, peers_key, dh);
}

int DH_compute_key(unsigned char *out, const BIGNUM *peers_key, DH *dh) {
  boringssl_ensure_ffdh_self_test();

  UniquePtr<BN_CTX> ctx(BN_CTX_new());
  if (ctx == nullptr) {
    return -1;
  }
  BN_CTXScope scope(ctx.get());
  BIGNUM *shared_key = BN_CTX_get(ctx.get());
  if (shared_key == nullptr ||
      !dh_compute_key(dh, shared_key, peers_key, ctx.get())) {
    return -1;
  }
  // A |BIGNUM|'s byte count fits in |int|.
  return static_cast<int>(BN_bn2bin(shared_key, out));
}

int DH_compute_key_hashed(DH *dh, uint8_t *out, size_t *out_len,
                          size_t max_out_len, const BIGNUM *peers_key,
                          const EVP_MD *digest) {
  *out_len = SIZE_MAX;

  const size_t digest_len = EVP_MD_size(digest);
  if (digest_len > max_out_len) {
    return 0;
  }

  FIPS_service_indicator_lock_state();

  int ret = 0;
  const size_t dh_len = DH_size(dh);
  uint8_t *shared_bytes = reinterpret_cast<uint8_t *>(OPENSSL_malloc(dh_len));
  unsigned out_len_unsigned;
  if (!shared_bytes ||
      // SP 800-56A is ambiguous about whether the output should be padded prior
      // to revision three. But revision three, section C.1, awkwardly specifies
      // padding to the length of p.
      //
      // Also, padded output avoids side-channels, so is always strongly
      // advisable.
      DH_compute_key_padded(shared_bytes, peers_key, dh) != (int)dh_len ||
      !EVP_Digest(shared_bytes, dh_len, out, &out_len_unsigned, digest,
                  nullptr) ||
      out_len_unsigned != digest_len) {
    goto err;
  }

  *out_len = digest_len;
  ret = 1;

err:
  FIPS_service_indicator_unlock_state();
  OPENSSL_free(shared_bytes);
  return ret;
}

int DH_size(const DH *dh) { return BN_num_bytes(FromOpaque(dh)->p.get()); }

int DH_up_ref(DH *dh) {
  auto *impl = FromOpaque(dh);
  impl->UpRefInternal();
  return 1;
}

DH *DH_get_rfc7919_2048() {
  // This is the prime from https://tools.ietf.org/html/rfc7919#appendix-A.1,
  // which is specifically approved for FIPS in appendix D of SP 800-56Ar3.
  static const BN_ULONG kFFDHE2048Data[] = {
      TOBN(0xffffffff, 0xffffffff), TOBN(0x886b4238, 0x61285c97),
      TOBN(0xc6f34a26, 0xc1b2effa), TOBN(0xc58ef183, 0x7d1683b2),
      TOBN(0x3bb5fcbc, 0x2ec22005), TOBN(0xc3fe3b1b, 0x4c6fad73),
      TOBN(0x8e4f1232, 0xeef28183), TOBN(0x9172fe9c, 0xe98583ff),
      TOBN(0xc03404cd, 0x28342f61), TOBN(0x9e02fce1, 0xcdf7e2ec),
      TOBN(0x0b07a7c8, 0xee0a6d70), TOBN(0xae56ede7, 0x6372bb19),
      TOBN(0x1d4f42a3, 0xde394df4), TOBN(0xb96adab7, 0x60d7f468),
      TOBN(0xd108a94b, 0xb2c8e3fb), TOBN(0xbc0ab182, 0xb324fb61),
      TOBN(0x30acca4f, 0x483a797a), TOBN(0x1df158a1, 0x36ade735),
      TOBN(0xe2a689da, 0xf3efe872), TOBN(0x984f0c70, 0xe0e68b77),
      TOBN(0xb557135e, 0x7f57c935), TOBN(0x85636555, 0x3ded1af3),
      TOBN(0x2433f51f, 0x5f066ed0), TOBN(0xd3df1ed5, 0xd5fd6561),
      TOBN(0xf681b202, 0xaec4617a), TOBN(0x7d2fe363, 0x630c75d8),
      TOBN(0xcc939dce, 0x249b3ef9), TOBN(0xa9e13641, 0x146433fb),
      TOBN(0xd8b9c583, 0xce2d3695), TOBN(0xafdc5620, 0x273d3cf1),
      TOBN(0xadf85458, 0xa2bb4a9a), TOBN(0xffffffff, 0xffffffff),
  };

  UniquePtr<BIGNUM> ffdhe2048_p(BN_new());
  UniquePtr<BIGNUM> ffdhe2048_q(BN_new());
  UniquePtr<BIGNUM> ffdhe2048_g(BN_new());
  UniquePtr<DH> dh(DH_new());
  if (!ffdhe2048_p || !ffdhe2048_q || !ffdhe2048_g || !dh) {
    return nullptr;
  }

  bn_set_static_words(ffdhe2048_p.get(), kFFDHE2048Data,
                      std::size(kFFDHE2048Data));

  if (!BN_rshift1(ffdhe2048_q.get(), ffdhe2048_p.get()) ||
      !BN_set_word(ffdhe2048_g.get(), 2) ||
      !DH_set0_pqg(dh.get(), ffdhe2048_p.get(), ffdhe2048_q.get(),
                   ffdhe2048_g.get())) {
    return nullptr;
  }
  // |DH_set0_pqg| takes ownership on success.
  ffdhe2048_p.release();
  ffdhe2048_q.release();
  ffdhe2048_g.release();

  return dh.release();
}
